import json
import serial
import threading
import time
from collections import deque
import numpy as np
import matplotlib.animation as mpla
import matplotlib.pyplot as plt
import csv


# class SerialReader from https://www.pyserial.com/docs/performance/threading site
class SerialReader:
    """Background serial reader with callback."""

    def __init__(self, port, baudrate=9600, callback=None):
        self.ser = serial.Serial(port, baudrate, timeout=0.1)
        self.callback = callback
        self.running = False
        self.lock = threading.Lock()

    def start(self):
        self.running = True
        # Thread for only reading data from COM port and start it
        self.thread = threading.Thread(target=self._read_loop, daemon=True) # Function that is used inside a thread that is _read_loop, where we have a buffer for data
        self.thread.start()

    def _read_loop(self):
        buffer = bytearray()
        while self.running:
            try:
                if self.ser.in_waiting:
                    data = self.ser.read(self.ser.in_waiting)
                    buffer.extend(data)
                    # In this buffer every time we have a newline character we pass a whole line of data that we recevied to function callback, that is on_data(implemented later)
                    while b'\n' in buffer: 
                        line, buffer = buffer.split(b'\n', 1)
                        if self.callback:
                            self.callback(line.decode(errors='replace').strip())
                else:
                    time.sleep(0.001)
            except serial.SerialException:
                break

    def write(self, data):
        """Thread-safe write."""
        with self.lock:
            self.ser.write(data)

    def stop(self):
        self.running = False
        if hasattr(self, 'thread'):
            self.thread.join(timeout=2)
        self.ser.close()

frequency = 8
period = 1/frequency
data_collection_time_minutes = 5
samples = 5*60*frequency
# Double ended queues for temperature and pressure data
temp_data = deque(maxlen=samples)
press_data = deque(maxlen=samples)
# VERY IMPORTANT
# To ensure that there is no sitution where we access this data while it is still appended we create a lock for data
data_lock = threading.Lock()
# Plot creation
fig, (ax_temp, ax_press) = plt.subplots(2, 1, sharex=True, layout="constrained")
# First we have to create plots without any data
temp_line, = ax_temp.plot([], [])
press_line, = ax_press.plot([], [])
# Customizing plots
ax_temp.set_ylabel("Temperature [C]")
ax_temp.set_title("Live temperature")
ax_temp.grid()
ax_press.set_xlabel("Time [min]")
ax_press.set_ylabel("Pressure [hPa]")
ax_press.set_title("Live pressure")
ax_press.ticklabel_format(axis="y", style="plain", useOffset=False)
ax_press.grid()

# on_data function is a callback for our serial reader, so every time _read_loop has a line of data ready from COM it invokes this fucntion that parses this data 
# using json
def on_data(line):
    try:
        reading = json.loads(line)
        if not isinstance(reading, list) or len(reading) != 2:
            raise ValueError("expected [temperature, pressure]")
        temperature, pressure = (float(value) for value in reading)
    except (json.JSONDecodeError, TypeError, ValueError) as error:
        print(f"Skipping invalid sensor message {line!r}: {error}")
        return

    # after a line of data is parsed we take a lock so we can safely append our parsed data to our double ended queues and by that prevent other with data_lock to execute their lines of code
    with data_lock:
        temp_data.append(temperature)
        press_data.append(pressure)

def update(_frame):
    # if there is a situation where on_data has a data_lock we wait
    # after on_data appended data we create a list with our temp and pressure data that later is used to create an animated plot
    with data_lock:
        temperatures = list(temp_data)
        pressures = list(press_data)

    if not temperatures:
        return temp_line, press_line

    elapsed = np.arange(len(temperatures)) * period /60 # a time vector based on how many samples we received since start and period of measurements
    temp_line.set_data(elapsed, temperatures)
    press_line.set_data(elapsed, pressures)
    # adjusting axes to scale automatically based on plotted data
    ax_temp.relim()
    ax_temp.autoscale_view()
    ax_press.relim()
    ax_press.autoscale_view()
    return temp_line, press_line

def save_csv(filepath):
    with data_lock:
        temperatures = list(temp_data)
        pressures = list(press_data)
    with open(filepath,"w",newline="") as file:
        writer = csv.writer(file)
        elapsed = np.arange(len(temperatures)) * period
        writer.writerow(["time_s", "temperature_C", "pressure_hPa"])
        for i in range(len(temperatures)):
            writer.writerow([elapsed[i],temperatures[i],pressures[i]])
        

reader = SerialReader('COM3', 115200, callback=on_data) # serial port with callback funciton on_data, so every time a whole line of data is ready this function is invoked
reader.start()

def on_plot_close(_event):
    reader.write(b"log_live:stop;")
    save_csv("data.csv")


fig.canvas.mpl_connect("close_event", on_plot_close)

animation = mpla.FuncAnimation(fig, update, interval=period, blit=False, cache_frame_data=False)
try:
    # commands to start logging data
    reader.write(f"freq={frequency};".encode("ascii"))
    time.sleep(1)
    reader.write(f"log_live={samples};".encode("ascii"))
    plt.show()
finally:
    reader.stop()