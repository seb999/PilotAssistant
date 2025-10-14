#!/usr/bin/env python3
"""
GPS Simulator for Pico
Sends Stockholm GPS coordinates to Pico via USB serial port every second
"""

import serial
import time
import sys
from datetime import datetime

# Stockholm coordinates
STOCKHOLM_LAT = 59.3293  # N
STOCKHOLM_LON = 18.0686  # E

# Serial port configuration
# Common USB serial ports:
# macOS: /dev/tty.usbmodem* or /dev/tty.usbserial*
# Linux: /dev/ttyACM0 or /dev/ttyUSB0
# Windows: COM3, COM4, etc.
DEFAULT_PORT = '/dev/tty.usbmodem14201'  # Change this to match your Pico's port
BAUDRATE = 9600
TIMEOUT = 1


def convert_to_nmea_format(decimal_degrees, is_latitude):
    """
    Convert decimal degrees to NMEA format (DDMM.MMMM or DDDMM.MMMM)

    Args:
        decimal_degrees: Float value in decimal degrees
        is_latitude: True for latitude, False for longitude

    Returns:
        Tuple of (nmea_string, direction)
    """
    # Determine direction
    if is_latitude:
        direction = 'N' if decimal_degrees >= 0 else 'S'
    else:
        direction = 'E' if decimal_degrees >= 0 else 'W'

    # Work with absolute value
    abs_degrees = abs(decimal_degrees)

    # Extract degrees and minutes
    degrees = int(abs_degrees)
    minutes = (abs_degrees - degrees) * 60

    # Format: DDMM.MMMM for latitude, DDDMM.MMMM for longitude
    if is_latitude:
        nmea_string = f"{degrees:02d}{minutes:07.4f}"
    else:
        nmea_string = f"{degrees:03d}{minutes:07.4f}"

    return nmea_string, direction


def calculate_checksum(sentence):
    """
    Calculate NMEA checksum (XOR of all characters between $ and *)

    Args:
        sentence: NMEA sentence without $ and checksum

    Returns:
        Two-digit hex checksum string
    """
    checksum = 0
    for char in sentence:
        checksum ^= ord(char)
    return f"{checksum:02X}"


def generate_gpgga_sentence(latitude, longitude, satellites=8):
    """
    Generate a GPGGA NMEA sentence

    Args:
        latitude: Latitude in decimal degrees
        longitude: Longitude in decimal degrees
        satellites: Number of satellites (default: 8)

    Returns:
        Complete GPGGA NMEA sentence with checksum
    """
    # Get current UTC time
    now = datetime.utcnow()
    utc_time = now.strftime("%H%M%S.00")

    # Convert coordinates to NMEA format
    lat_nmea, lat_dir = convert_to_nmea_format(latitude, True)
    lon_nmea, lon_dir = convert_to_nmea_format(longitude, False)

    # Build GPGGA sentence (without $ and checksum)
    # Format: $GPGGA,time,lat,NS,lon,EW,quality,sats,hdop,alt,M,geoid,M,,*checksum
    sentence_parts = [
        "GPGGA",
        utc_time,           # UTC time
        lat_nmea,           # Latitude
        lat_dir,            # N/S
        lon_nmea,           # Longitude
        lon_dir,            # E/W
        "1",                # Fix quality (1 = GPS fix)
        f"{satellites:02d}", # Number of satellites
        "1.0",              # HDOP (Horizontal Dilution of Precision)
        "50.0",             # Altitude above sea level (meters)
        "M",                # Altitude unit
        "0.0",              # Geoid separation
        "M",                # Geoid separation unit
        "",                 # Age of differential GPS data
        ""                  # Differential reference station ID
    ]

    sentence = ",".join(sentence_parts)
    checksum = calculate_checksum(sentence)

    return f"${sentence}*{checksum}"


def list_serial_ports():
    """List available serial ports"""
    import serial.tools.list_ports
    ports = serial.tools.list_ports.comports()
    return [(port.device, port.description) for port in ports]


def main():
    """Main simulator loop"""
    # Check if port was provided as command line argument
    if len(sys.argv) > 1:
        port = sys.argv[1]
    else:
        # List available ports
        print("=== Available Serial Ports ===")
        ports = list_serial_ports()
        if not ports:
            print("No serial ports found!")
            print("\nPlease connect your Pico and try again.")
            return

        for i, (device, description) in enumerate(ports, 1):
            print(f"{i}. {device} - {description}")

        print("\nUsage: python3 simulator.py [port]")
        print(f"Example: python3 simulator.py {ports[0][0]}")
        print(f"\nAttempting to use default port: {DEFAULT_PORT}")
        port = DEFAULT_PORT

    print(f"\n=== GPS Simulator for Pico ===")
    print(f"Location: Stockholm, Sweden")
    print(f"Coordinates: {STOCKHOLM_LAT}°N, {STOCKHOLM_LON}°E")
    print(f"Port: {port}")
    print(f"Baudrate: {BAUDRATE}")
    print(f"\nPress Ctrl+C to stop\n")

    # Open serial port
    try:
        ser = serial.Serial(port, BAUDRATE, timeout=TIMEOUT)
        print(f"✓ Serial port opened successfully")
        time.sleep(2)  # Wait for connection to stabilize
    except serial.SerialException as e:
        print(f"✗ Error opening serial port: {e}")
        print("\nTroubleshooting:")
        print("1. Check if the Pico is connected via USB")
        print("2. Verify the correct port name:")
        print("   - macOS: ls /dev/tty.usb*")
        print("   - Linux: ls /dev/ttyACM* or ls /dev/ttyUSB*")
        print("   - Windows: Check Device Manager for COM ports")
        print("3. Make sure no other program is using the port")
        return

    # Main loop - send GPS data every second
    sentence_count = 0
    try:
        while True:
            # Generate GPGGA sentence
            gpgga = generate_gpgga_sentence(STOCKHOLM_LAT, STOCKHOLM_LON)

            # Send to Pico
            ser.write(f"{gpgga}\r\n".encode('ascii'))

            sentence_count += 1
            timestamp = time.strftime("%H:%M:%S")
            print(f"[{timestamp}] Sent #{sentence_count}: {gpgga}")

            # Wait 1 second before next update
            time.sleep(1)

    except KeyboardInterrupt:
        print(f"\n\n=== Simulator Stopped ===")
        print(f"Total sentences sent: {sentence_count}")
    except Exception as e:
        print(f"\n✗ Error during transmission: {e}")
    finally:
        ser.close()
        print("Serial port closed.")


if __name__ == "__main__":
    main()
