# GPS Simulator for Pico

This simulator sends GPS coordinates for Stockholm, Sweden to your Pico via USB serial connection every second.

## Features

- Generates valid NMEA GPGGA sentences
- Stockholm coordinates: 59.3293°N, 18.0686°E
- Sends data every 1 second
- Includes proper NMEA checksums
- Auto-detects available serial ports

## Usage

### 1. Find your Pico's serial port

**macOS:**
```bash
ls /dev/tty.usb*
```

**Linux:**
```bash
ls /dev/ttyACM* /dev/ttyUSB*
```

**Windows:**
Check Device Manager for COM ports (e.g., COM3, COM4)

### 2. Run the simulator

```bash
# With auto-detection (shows available ports)
python3 simulator.py

# Or specify the port directly
python3 simulator.py /dev/tty.usbmodem14201

# Linux example
python3 simulator.py /dev/ttyACM0

# Windows example
python3 simulator.py COM3
```

### 3. Stop the simulator

Press `Ctrl+C` to stop sending data.

## NMEA Format

The simulator generates GPGGA sentences in this format:
```
$GPGGA,HHMMSS.SS,DDMM.MMMM,N,DDDMM.MMMM,E,1,08,1.0,50.0,M,0.0,M,,*XX
```

Where:
- `HHMMSS.SS` - UTC time
- `DDMM.MMMM,N` - Latitude in NMEA format (Stockholm: 5919.7580,N)
- `DDDMM.MMMM,E` - Longitude in NMEA format (Stockholm: 01804.1160,E)
- `1` - Fix quality (1 = GPS fix)
- `08` - Number of satellites (8)
- `1.0` - Horizontal dilution of precision
- `50.0,M` - Altitude in meters
- `*XX` - Checksum

## Configuration

Edit these variables in `simulator.py` to customize:

```python
STOCKHOLM_LAT = 59.3293  # Latitude in decimal degrees
STOCKHOLM_LON = 18.0686  # Longitude in decimal degrees
DEFAULT_PORT = '/dev/tty.usbmodem14201'  # Default serial port
BAUDRATE = 9600  # Must match Pico's baudrate
```

## Troubleshooting

**"Error opening serial port":**
- Check if Pico is connected via USB
- Verify the correct port name
- Make sure no other program is using the port
- On Linux, you may need permissions: `sudo chmod 666 /dev/ttyACM0`

**No data received on Pico:**
- Verify baudrate matches on both sides (default: 9600)
- Check that Pico's serial port is configured correctly
- Try a different USB cable or port

**"No serial ports found":**
- Make sure Pico is connected and powered on
- Install pyserial: `pip install pyserial`
