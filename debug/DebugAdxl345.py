import smbus2
import time

# ADXL345 constants
ADDRESS = 0x53
POWER_CTL = 0x2D
DATA_FORMAT = 0x31
DATAX0 = 0x32

# Initialize I2C (bus 1 for Raspberry Pi)
bus = smbus2.SMBus(1)

# Wake up the ADXL345 (set Measure bit to 1)
bus.write_byte_data(ADDRESS, POWER_CTL, 0x08)

# Optional: Set data format (e.g. ±2g, full resolution)
bus.write_byte_data(ADDRESS, DATA_FORMAT, 0x08)

def read_axes():
    # Read 6 bytes from DATAX0 to DATAZ1
    data = bus.read_i2c_block_data(ADDRESS, DATAX0, 6)

    # Convert the raw data to signed 16-bit integers
    x = int.from_bytes(data[0:2], byteorder='little', signed=True)
    y = int.from_bytes(data[2:4], byteorder='little', signed=True)
    z = int.from_bytes(data[4:6], byteorder='little', signed=True)

    # Scale to 'g' values (assuming ±2g range and 10-bit resolution = 256 LSB/g)
    x_g = x / 256.0
    y_g = y / 256.0
    z_g = z / 256.0

    return (x_g, y_g, z_g)

# Read and print acceleration data
try:
    while True:
        x, y, z = read_axes()
        print(f"X: {x:.2f} g, Y: {y:.2f} g, Z: {z:.2f} g")
        time.sleep(0.1)

except KeyboardInterrupt:
    print("Stopped.")
