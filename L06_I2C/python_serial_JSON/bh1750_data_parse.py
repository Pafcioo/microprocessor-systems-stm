import serial
import json
import matplotlib.pyplot as plt

ser = serial.Serial('/dev/ttyACM0', 115200)

def send_and_read(ser, command, timeout=20):
    """Send a command string and return the response line."""
    ser.reset_input_buffer()
    ser.write(command.encode() + b'\r\n')

    old_timeout = ser.timeout
    ser.timeout = timeout
    try:
        return ser.readline().decode('utf-8').strip()
    finally:
        ser.timeout = old_timeout

ser.write(b'freq=20;')
response = send_and_read(ser, "JSON_log;")
if not response:
    raise TimeoutError("No data given in time")

points = json.loads(response)

pwm = [point["pwm_percent"] for point in points]
lux = [point["illuminance_lux"] for point in points]

print(f"Odebrano {len(points)} punktów")
print(f"Pierwszy punkt: {pwm[0]}% PWM → {lux[0]} lx")

plt.plot(pwm,lux)
plt.xlabel("Wypełnienie PWM[%]")
plt.ylabel("Luminancja[lx]")
plt.title("Charakterystyka statyczna układu LED-sensor")
plt.grid(True)
plt.show()