#!/usr/bin/env python3
"""
Convert PNG icon to RGB565 binary format with transparency support.
Transparent pixels are marked with a special value (0x0000 with alpha=0).
"""

from PIL import Image
import struct
import sys

def rgb888_to_rgb565(r, g, b):
    """Convert RGB888 to RGB565 format"""
    r5 = (r >> 3) & 0x1F
    g6 = (g >> 2) & 0x3F
    b5 = (b >> 3) & 0x1F
    return (r5 << 11) | (g6 << 5) | b5

def convert_png_to_rgb565_with_alpha(input_png, output_bin, output_c, var_name, width=32, height=32):
    """
    Convert PNG to RGB565 binary with alpha channel info.
    Creates both .bin file and C header.
    """
    # Open and resize image if needed
    img = Image.open(input_png)

    # Ensure RGBA mode
    if img.mode != 'RGBA':
        img = img.convert('RGBA')

    # Resize if needed
    if img.size != (width, height):
        img = img.resize((width, height), Image.Resampling.LANCZOS)
        print(f"Resized image to {width}x{height}")

    # Create binary data
    bin_data = bytearray()
    alpha_data = bytearray()

    pixels = img.load()

    for y in range(height):
        for x in range(width):
            r, g, b, a = pixels[x, y]

            # Convert to RGB565
            rgb565 = rgb888_to_rgb565(r, g, b)

            # Store RGB565 in little-endian format
            bin_data.extend(struct.pack('<H', rgb565))

            # Store alpha (0 = transparent, 255 = opaque)
            alpha_data.append(a)

    # Write binary file
    with open(output_bin, 'wb') as f:
        f.write(bin_data)

    print(f"Binary data written to: {output_bin}")
    print(f"Size: {len(bin_data)} bytes ({width}x{height} pixels)")

    # Generate C header file
    with open(output_c, 'w') as f:
        f.write(f"// Auto-generated from {input_png}\n")
        f.write(f"// {width}x{height} RGB565 icon with transparency\n\n")
        f.write(f"#ifndef {var_name.upper()}_H\n")
        f.write(f"#define {var_name.upper()}_H\n\n")
        f.write(f"#include <stdint.h>\n\n")

        # RGB565 color data
        f.write(f"const uint16_t {var_name}_data[{width * height}] = {{\n")
        for i in range(0, len(bin_data), 2):
            if i % 32 == 0:
                f.write("    ")

            rgb565 = struct.unpack('<H', bin_data[i:i+2])[0]
            f.write(f"0x{rgb565:04X}")

            if i < len(bin_data) - 2:
                f.write(", ")

            if (i + 2) % 32 == 0:
                f.write("\n")

        if len(bin_data) % 32 != 0:
            f.write("\n")
        f.write("};\n\n")

        # Alpha channel data (packed as bytes)
        f.write(f"const uint8_t {var_name}_alpha[{width * height}] = {{\n")
        for i, alpha in enumerate(alpha_data):
            if i % 16 == 0:
                f.write("    ")

            f.write(f"0x{alpha:02X}")

            if i < len(alpha_data) - 1:
                f.write(", ")

            if (i + 1) % 16 == 0:
                f.write("\n")

        if len(alpha_data) % 16 != 0:
            f.write("\n")
        f.write("};\n\n")

        f.write(f"#define {var_name.upper()}_WIDTH {width}\n")
        f.write(f"#define {var_name.upper()}_HEIGHT {height}\n\n")
        f.write(f"#endif // {var_name.upper()}_H\n")

    print(f"C header written to: {output_c}")

    # Print statistics
    transparent_count = sum(1 for a in alpha_data if a < 128)
    opaque_count = len(alpha_data) - transparent_count
    print(f"\nStatistics:")
    print(f"  Transparent pixels: {transparent_count}")
    print(f"  Opaque pixels: {opaque_count}")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python3 convert_icon_to_bin.py <input.png> [output_name] [width] [height]")
        print("Example: python3 convert_icon_to_bin.py wifiGreen.png wifi_green_icon 32 32")
        sys.exit(1)

    input_png = sys.argv[1]
    output_name = sys.argv[2] if len(sys.argv) > 2 else "icon"
    width = int(sys.argv[3]) if len(sys.argv) > 3 else 32
    height = int(sys.argv[4]) if len(sys.argv) > 4 else 32

    output_bin = f"{output_name}.bin"
    output_c = f"{output_name}.h"

    convert_png_to_rgb565_with_alpha(input_png, output_bin, output_c, output_name, width, height)
