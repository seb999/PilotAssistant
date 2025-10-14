#!/usr/bin/env python3
"""
High-Performance GPS Simulator for Pico using USB Bulk Transfer
Sends Stockholm GPS coordinates to Pico via USB bulk endpoints
Much faster and more efficient than serial communication
"""

import usb.core
import usb.util
import time
import sys
import struct
from datetime import datetime

# Stockholm coordinates
STOCKHOLM_LAT = 59.3293  # N
STOCKHOLM_LON = 18.0686  # E

# Raspberry Pi Pico USB IDs
PICO_VENDOR_ID = 0x2E8A
PICO_PRODUCT_ID = 0x0005  # Default MicroPython product ID

# USB endpoints (will be auto-detected)
EP_OUT = None  # Bulk OUT endpoint (host to device)
EP_IN = None   # Bulk IN endpoint (device to host)


class GPSData:
    """GPS data packet structure for efficient binary transfer"""

    # Binary format: latitude(f), longitude(f), satellites(B), fix_quality(B), timestamp(d)
    FORMAT = '<ffBBd'  # little-endian, 2 floats, 2 bytes, 1 double
    SIZE = struct.calcsize(FORMAT)

    @staticmethod
    def pack(latitude, longitude, satellites=8, fix_quality=1):
        """Pack GPS data into binary format"""
        timestamp = time.time()
        return struct.pack(
            GPSData.FORMAT,
            latitude,
            longitude,
            satellites,
            fix_quality,
            timestamp
        )

    @staticmethod
    def unpack(data):
        """Unpack binary GPS data"""
        return struct.unpack(GPSData.FORMAT, data)


def generate_nmea_sentence(latitude, longitude, satellites=8):
    """
    Generate a GPGGA NMEA sentence for compatibility
    (Can be used if Pico prefers NMEA format)
    """
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


def find_pico_device():
    """Find and configure Pico USB device"""
    global EP_OUT, EP_IN

    print("=== Searching for Raspberry Pi Pico ===")

    # Find the device
    dev = usb.core.find(idVendor=PICO_VENDOR_ID)

    if dev is None:
        print("✗ Pico not found!")
        print(f"  Make sure Pico is connected via USB")
        print(f"  Looking for: Vendor ID 0x{PICO_VENDOR_ID:04X}")

        # List all USB devices for debugging
        print("\nAvailable USB devices:")
        for device in usb.core.find(find_all=True):
            print(f"  - 0x{device.idVendor:04X}:0x{device.idProduct:04X} - {device.manufacturer if device.manufacturer else 'Unknown'}")
        return None

    print(f"✓ Found Pico: 0x{dev.idVendor:04X}:0x{dev.idProduct:04X}")
    print(f"  Manufacturer: {dev.manufacturer}")
    print(f"  Product: {dev.product}")
    print(f"  Serial: {dev.serial_number}")

    # Detach kernel driver if necessary (Linux)
    try:
        if dev.is_kernel_driver_active(0):
            print("  Detaching kernel driver...")
            dev.detach_kernel_driver(0)
    except (NotImplementedError, usb.core.USBError):
        pass  # Not needed on macOS/Windows

    # Set configuration
    try:
        dev.set_configuration()
        print("  Configuration set")
    except usb.core.USBError as e:
        print(f"  Warning: Could not set configuration: {e}")

    # Get active configuration
    cfg = dev.get_active_configuration()
    intf = cfg[(0, 0)]

    print(f"\n=== USB Endpoints ===")

    # Find bulk endpoints
    ep_out = usb.util.find_descriptor(
        intf,
        custom_match=lambda e: \
            usb.util.endpoint_direction(e.bEndpointAddress) == usb.util.ENDPOINT_OUT and \
            usb.util.endpoint_type(e.bmAttributes) == usb.util.ENDPOINT_TYPE_BULK
    )

    ep_in = usb.util.find_descriptor(
        intf,
        custom_match=lambda e: \
            usb.util.endpoint_direction(e.bEndpointAddress) == usb.util.ENDPOINT_IN and \
            usb.util.endpoint_type(e.bmAttributes) == usb.util.ENDPOINT_TYPE_BULK
    )

    if ep_out is None:
        print("✗ Bulk OUT endpoint not found!")
        print("  Available endpoints:")
        for endpoint in intf:
            direction = "IN" if usb.util.endpoint_direction(endpoint.bEndpointAddress) == usb.util.ENDPOINT_IN else "OUT"
            ep_type = ["Control", "Isochronous", "Bulk", "Interrupt"][usb.util.endpoint_type(endpoint.bmAttributes)]
            print(f"    - 0x{endpoint.bEndpointAddress:02X} ({direction}, {ep_type})")
        return None

    EP_OUT = ep_out
    EP_IN = ep_in

    print(f"✓ Bulk OUT endpoint: 0x{ep_out.bEndpointAddress:02X} (max packet: {ep_out.wMaxPacketSize} bytes)")
    if ep_in:
        print(f"✓ Bulk IN endpoint: 0x{ep_in.bEndpointAddress:02X} (max packet: {ep_in.wMaxPacketSize} bytes)")

    return dev


def send_binary_mode(dev, use_binary=True):
    """Send GPS data in binary format"""
    print(f"\n=== Starting GPS Transmission (Binary Mode) ===")
    print(f"Location: Stockholm, Sweden")
    print(f"Coordinates: {STOCKHOLM_LAT}°N, {STOCKHOLM_LON}°E")
    print(f"Update rate: 1 Hz (1 update/second)")
    print(f"Packet size: {GPSData.SIZE} bytes")
    print(f"\nPress Ctrl+C to stop\n")

    packet_count = 0
    start_time = time.time()

    try:
        while True:
            # Pack GPS data into binary format
            data = GPSData.pack(STOCKHOLM_LAT, STOCKHOLM_LON, satellites=8, fix_quality=1)

            # Send via USB bulk transfer
            try:
                bytes_sent = dev.write(EP_OUT, data, timeout=1000)
                packet_count += 1

                elapsed = time.time() - start_time
                rate = packet_count / elapsed if elapsed > 0 else 0

                timestamp = time.strftime("%H:%M:%S")
                print(f"[{timestamp}] Packet #{packet_count}: {bytes_sent} bytes sent | Rate: {rate:.2f} pkt/s")

            except usb.core.USBError as e:
                print(f"✗ USB error: {e}")
                if "timeout" not in str(e).lower():
                    raise

            # Wait 1 second before next update
            time.sleep(1)

    except KeyboardInterrupt:
        elapsed = time.time() - start_time
        rate = packet_count / elapsed if elapsed > 0 else 0
        print(f"\n\n=== Transmission Statistics ===")
        print(f"Total packets sent: {packet_count}")
        print(f"Total time: {elapsed:.1f} seconds")
        print(f"Average rate: {rate:.2f} packets/second")
        print(f"Total data: {packet_count * GPSData.SIZE} bytes ({packet_count * GPSData.SIZE / 1024:.2f} KB)")


def send_nmea_mode(dev):
    """Send GPS data in NMEA format (text-based, for compatibility)"""
    print(f"\n=== Starting GPS Transmission (NMEA Mode) ===")
    print(f"Location: Stockholm, Sweden")
    print(f"Coordinates: {STOCKHOLM_LAT}°N, {STOCKHOLM_LON}°E")
    print(f"Update rate: 1 Hz (1 update/second)")
    print(f"\nPress Ctrl+C to stop\n")

    sentence_count = 0
    start_time = time.time()

    try:
        while True:
            # Generate NMEA sentence
            nmea = generate_nmea_sentence(STOCKHOLM_LAT, STOCKHOLM_LON)
            data = nmea.encode('ascii')

            # Send via USB bulk transfer
            try:
                bytes_sent = dev.write(EP_OUT, data, timeout=1000)
                sentence_count += 1

                elapsed = time.time() - start_time
                rate = sentence_count / elapsed if elapsed > 0 else 0

                timestamp = time.strftime("%H:%M:%S")
                print(f"[{timestamp}] Sentence #{sentence_count}: {bytes_sent} bytes | Rate: {rate:.2f} msg/s")
                print(f"  {nmea.strip()}")

            except usb.core.USBError as e:
                print(f"✗ USB error: {e}")
                if "timeout" not in str(e).lower():
                    raise

            # Wait 1 second before next update
            time.sleep(1)

    except KeyboardInterrupt:
        elapsed = time.time() - start_time
        rate = sentence_count / elapsed if elapsed > 0 else 0
        print(f"\n\n=== Transmission Statistics ===")
        print(f"Total sentences sent: {sentence_count}")
        print(f"Total time: {elapsed:.1f} seconds")
        print(f"Average rate: {rate:.2f} sentences/second")


def main():
    """Main simulator loop"""
    print("=== High-Performance GPS Simulator for Pico ===")
    print("Using USB Bulk Transfer for maximum performance\n")

    # Find and configure Pico
    dev = find_pico_device()
    if dev is None:
        print("\n⚠ Installation requirements:")
        print("  pip install pyusb")
        print("\n⚠ On macOS/Linux, you may need libusb:")
        print("  macOS: brew install libusb")
        print("  Linux: sudo apt-get install libusb-1.0-0")
        return

    # Choose mode
    print("\n=== Select Transfer Mode ===")
    print("1. Binary mode (efficient, requires custom Pico code)")
    print("2. NMEA mode (compatible with standard GPS parsers)")

    mode = input("\nEnter mode (1 or 2, default=1): ").strip() or "1"

    try:
        if mode == "1":
            send_binary_mode(dev)
        else:
            send_nmea_mode(dev)
    except Exception as e:
        print(f"\n✗ Error: {e}")
    finally:
        # Clean up
        usb.util.dispose_resources(dev)
        print("\nUSB resources released.")


if __name__ == "__main__":
    main()
