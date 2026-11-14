import json
import serial
import matplotlib.pyplot as plt
import numpy as np
import csv

ser = serial.Serial('COM3', 115200)

def send_and_read(ser, command, timeout=660):
    """Send a command string and return the response line."""
    ser.reset_input_buffer()
    ser.write(command.encode() + b'\r\n')

    old_timeout = ser.timeout
    ser.timeout = timeout
    try:
        return ser.readline().decode('utf-8').strip()
    finally:
        ser.timeout = old_timeout

def save_csv(filepath, temp_data, press_data):
    with open(filepath,"w",newline="") as file:
        writer = csv.writer(file)
        elapsed = np.arange(len(temp_data)) * period
        writer.writerow(["time_s", "temperature_C", "pressure_hPa"])
        for i in range(len(temp_data)):
            writer.writerow([elapsed[i],press_data[i],temp_data[i]])

frequency = 8
period = 1/frequency
data_collection_time_minutes = 0.2
samples = data_collection_time_minutes*60*frequency
time = np.arange(0,data_collection_time_minutes*60,period)/60
ser.write(f"freq={frequency};".encode("ascii"))
response = send_and_read(ser, f"logger={samples};")
if not response:
    raise TimeoutError("No data given in time")

temperature, pressure = json.loads(response)

fig, (ax_temp, ax_press) = plt.subplots(2, 1, figsize=(12, 7), layout="constrained")

ax_temp.plot(time, temperature)
ax_temp.set_xlabel("Time[min]")
ax_temp.set_ylabel("Temperature[C]")
ax_temp.set_title("Temperature in time")
ax_temp.grid()

ax_press.plot(time, pressure)
ax_press.set_xlabel("Time[min]")
ax_press.set_ylabel("Pressure[hPa]")
ax_press.set_title("Pressure in time")
ax_press.grid()
ax_press.ticklabel_format(axis="y", style="plain", useOffset=False)


plt.show()

save_csv('data.csv',temperature,pressure)
