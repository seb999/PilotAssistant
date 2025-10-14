#!/usr/bin/env python3
"""
Auto-running GPS Simulator for Pico using USB Bulk Transfer
Automatically sends Stockholm GPS coordinates every second
No user interaction required - just run it!
"""

import usb.core
import usb.util
import time
import struct
from datetime import datetime

# Stockholm coordinates
STOCKHOLM_LAT = 59.3293  # N
STOCKHOLM_LON = 18.0686  # E

# Raspberry Pi Pico USB IDs
PICO_VENDOR_ID = 0x2E8A
PICO_PRODUCT_ID = 0x0005

# Transfer mode: 'binary' or 'nmea'
TRANSFER_MODE = 'binary'  # Change to 'nmea' if you prefer


def pack_gps_binary(latitude, longitude, satellites=8, fix_quality=1):
    """Pack GPS data into binary format (18 bytes)"""
    timestamp = time.time()
    return struct.pack('<ffBBd', latitude, longitude, satellites, fix_quality, timestamp)


def generate_nmea(latitude, longitude, satellites=8):
    """Generate GPGGA NMEA sentence"""
    now = datetime.utcnow()
    utc_time = now.strftime("%H%M%S.00")

    # Convert to NMEA format
    lat_dir = 'N' if latitude >= 0 else 'S'
    lon_dir = 'E' if longitude >= 0 else 'W'

    abs_lat = abs(latitude)
    abs_lon = abs(longitude)

    lat_deg = int(abs_lat)
    lat_min = (abs_lat - lat_deg) * 60
    lat_nmea = f"{lat_deg:02d}{lat_min:07.4f}"

    lon_deg = int(abs_lon)
    lon_min = (abs_lon - lon_deg) * 60
    lon_nmea = f"{lon_deg:03d}{lon_min:07.4f}"

    sentence_parts = [
        "GPGGA", utc_time, lat_nmea, lat_dir, lon_nmea, lon_dir,
        "1", f"{satellites:02d}", "1.0", "50.0", "M", "0.0", "M", "", ""
    ]

    sentence = ",".join(sentence_parts)
    checksum = 0
    for char in sentence:
        checksum ^= ord(char)

    return f"${sentence}*{checksum:02X}\r\n"


print("=== Auto GPS Simulator for Pico (USB Bulk) ===")
print(f"Location: Stockholm, Sweden ({STOCKHOLM_LAT}°N, {STOCKHOLM_LON}°E)")
print(f"Mode: {TRANSFER_MODE.upper()}")
print("Searching for Pico...\n")

# Find Pico
dev = usb.core.find(idVendor=PICO_VENDOR_ID)

if dev is None:
    print("✗ Pico not found!")
    print(f"  Looking for Vendor ID: 0x{PICO_VENDOR_ID:04X}")
    print("\nAvailable USB devices:")
    for device in usb.core.find(find_all=True):
        try:
            print(f"  - 0x{device.idVendor:04X}:0x{device.idProduct:04X}")
        except:
            pass
    exit(1)

print(f"✓ Found Pico: {dev.manufacturer} {dev.product}")
print(f"  Serial: {dev.serial_number}")

# Configure device
try:
    if dev.is_kernel_driver_active(0):
        dev.detach_kernel_driver(0)
except:
    pass

try:
    dev.set_configuration()
except usb.core.USBError as e:
    print(f"  Note: {e}")

# Find bulk OUT endpoint
cfg = dev.get_active_configuration()
intf = cfg[(0, 0)]

ep_out = usb.util.find_descriptor(
    intf,
    custom_match=lambda e: \
        usb.util.endpoint_direction(e.bEndpointAddress) == usb.util.ENDPOINT_OUT and \
        usb.util.endpoint_type(e.bmAttributes) == usb.util.ENDPOINT_TYPE_BULK
)

if ep_out is None:
    print("✗ Bulk OUT endpoint not found!")
    exit(1)

print(f"✓ Endpoint: 0x{ep_out.bEndpointAddress:02X} (max: {ep_out.wMaxPacketSize} bytes)")
print("\nStarting transmission... (Press Ctrl+C to stop)\n")

# Main loop
count = 0
start_time = time.time()

try:
    while True:
        # Prepare data
        if TRANSFER_MODE == 'binary':
            data = pack_gps_binary(STOCKHOLM_LAT, STOCKHOLM_LON)
        else:
            data = generate_nmea(STOCKHOLM_LAT, STOCKHOLM_LON).encode('ascii')

        # Send to Pico
        try:
            bytes_sent = dev.write(ep_out, data, timeout=1000)
            count += 1

            elapsed = time.time() - start_time
            rate = count / elapsed if elapsed > 0 else 0

            timestamp = time.strftime("%H:%M:%S")
            print(f"[{timestamp}] #{count:4d} | {bytes_sent:3d} bytes | {rate:5.2f} pkt/s", end='\r')

        except usb.core.USBError as e:
            if "timeout" not in str(e).lower():
                print(f"\n✗ USB error: {e}")
                break

        # 1 Hz update rate
        time.sleep(1)

except KeyboardInterrupt:
    elapsed = time.time() - start_time
    rate = count / elapsed if elapsed > 0 else 0
    print(f"\n\n=== Statistics ===")
    print(f"Packets sent: {count}")
    print(f"Duration: {elapsed:.1f}s")
    print(f"Average rate: {rate:.2f} pkt/s")
finally:
    usb.util.dispose_resources(dev)
    print("Done.")
