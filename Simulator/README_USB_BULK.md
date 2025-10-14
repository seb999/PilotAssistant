# High-Performance GPS Simulator - USB Bulk Transfer

This simulator uses USB bulk transfers for maximum performance when sending GPS data to your Pico. Much faster and more efficient than serial communication!

## Features

- **USB Bulk Transfer**: Direct USB communication for best performance
- **Binary Mode**: Compact 18-byte packets for maximum efficiency
- **NMEA Mode**: Compatible with standard GPS parsers
- **High throughput**: Can send hundreds of updates per second (limited to 1 Hz for GPS simulation)
- **Stockholm coordinates**: 59.3293°N, 18.0686°E

## Installation

### 1. Install Python dependencies

```bash
pip install pyusb
```

### 2. Install libusb (if not already installed)

**macOS:**
```bash
brew install libusb
```

**Linux:**
```bash
sudo apt-get install libusb-1.0-0
```

**Windows:**
- Download libusb from https://libusb.info/
- Or install via vcpkg: `vcpkg install libusb`

## Usage

### Step 1: Prepare your Pico

Copy `pico_usb_receiver.py` to your Pico, then import it in your main code:

```python
from pico_usb_receiver import USBGPSReceiver

gps = USBGPSReceiver()

# Binary mode (most efficient)
while True:
    if gps.read_binary():
        data = gps.get_data()
        print(f"Position: {data['latitude']:.6f}, {data['longitude']:.6f}")
        print(f"Satellites: {data['satellites']}")
    time.sleep(0.1)
```

### Step 2: Run the simulator

```bash
# Interactive mode (choose binary or NMEA)
python3 simulator_usb_bulk.py

# The script will:
# 1. Auto-detect your Pico
# 2. Ask you to choose binary or NMEA mode
# 3. Start sending GPS data at 1 Hz
```

## Transfer Modes

### Binary Mode (Recommended)
- **Size**: 18 bytes per packet
- **Format**: latitude(float), longitude(float), satellites(byte), fix_quality(byte), timestamp(double)
- **Performance**: Minimal overhead, maximum efficiency
- **Use case**: When you control both simulator and Pico code

### NMEA Mode
- **Size**: ~70 bytes per sentence
- **Format**: Standard GPGGA NMEA sentences
- **Performance**: Slower but compatible
- **Use case**: When you need compatibility with existing GPS parsers

## Performance Comparison

| Method | Packet Size | Overhead | Throughput |
|--------|-------------|----------|------------|
| **USB Bulk (Binary)** | 18 bytes | Minimal | ~480 Mbps (USB 2.0 Full Speed) |
| **USB Bulk (NMEA)** | ~70 bytes | Low | ~480 Mbps (USB 2.0 Full Speed) |
| Serial (NMEA) | ~70 bytes | High | 9600 bps (0.009 Mbps) |

**USB Bulk is ~50,000x faster than serial!**

## Binary Packet Structure

```
Offset | Type   | Size | Field
-------|--------|------|-------------
0      | float  | 4    | Latitude (decimal degrees)
4      | float  | 4    | Longitude (decimal degrees)
8      | byte   | 1    | Number of satellites
9      | byte   | 1    | Fix quality (0=no fix, 1=GPS fix, 2=DGPS fix)
10     | double | 8    | Timestamp (Unix time)
-------|--------|------|-------------
Total:          18 bytes
```

## Device Detection

The simulator automatically detects your Pico using:
- **Vendor ID**: 0x2E8A (Raspberry Pi)
- **Product ID**: 0x0005 (MicroPython default)

If your Pico uses different IDs, edit these constants in `simulator_usb_bulk.py`:
```python
PICO_VENDOR_ID = 0x2E8A
PICO_PRODUCT_ID = 0x0005
```

## Troubleshooting

### "Pico not found"
- Verify Pico is connected: `system_profiler SPUSBDataType | grep -i pico` (macOS)
- Check USB cable (must be data cable, not charge-only)
- Try a different USB port

### "Access denied" or "Permission denied" (Linux)
```bash
# Option 1: Run with sudo (not recommended)
sudo python3 simulator_usb_bulk.py

# Option 2: Add udev rule (recommended)
sudo nano /etc/udev/rules.d/99-pico.rules
# Add this line:
SUBSYSTEM=="usb", ATTR{idVendor}=="2e8a", MODE="0666"
# Then reload:
sudo udevadm control --reload-rules
sudo udevadm trigger
```

### "No backend available" (libusb not found)
- macOS: `brew install libusb`
- Linux: `sudo apt-get install libusb-1.0-0`
- Windows: Download from https://libusb.info/

### Pico not receiving data
1. Make sure you copied `pico_usb_receiver.py` to your Pico
2. Verify your Pico code is calling `gps.read_binary()` or `gps.read_nmea()`
3. Check that USB CDC is enabled in your Pico's `boot.py`:
   ```python
   import usb_cdc
   usb_cdc.enable(console=True, data=True)
   ```

## Configuration

Edit `simulator_usb_bulk.py` to customize:

```python
# Change location
STOCKHOLM_LAT = 59.3293  # Your latitude
STOCKHOLM_LON = 18.0686  # Your longitude

# Change update rate (in send_binary_mode/send_nmea_mode functions)
time.sleep(1)  # Change to 0.1 for 10 Hz, 0.01 for 100 Hz, etc.
```

## Advanced: High-Speed Mode

For testing high-speed data transfer (not realistic for GPS, but useful for performance testing):

```python
# In simulator_usb_bulk.py, change:
time.sleep(1)  # to
time.sleep(0.01)  # 100 Hz update rate
```

The Pico can easily handle 100+ updates per second with binary mode!

## Comparison with Serial Simulator

Both simulators are available in this directory:
- `simulator.py` - Serial communication (simple, compatible)
- `simulator_usb_bulk.py` - USB bulk transfer (fast, efficient)

Use serial for:
- Simple testing
- Compatibility with existing code
- No special setup required

Use USB bulk for:
- Maximum performance
- High update rates (>10 Hz)
- Minimum latency
- Production systems
