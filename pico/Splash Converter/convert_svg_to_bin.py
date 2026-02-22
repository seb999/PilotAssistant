#!/usr/bin/env python3
"""
Convert SVG icon to 32x32 PNG then to RGB565 binary format with transparency.
"""

from PIL import Image
import struct
import sys
import os

def rgb888_to_rgb565(r, g, b):
    """Convert RGB888 to RGB565 format"""
    r5 = (r >> 3) & 0x1F
    g6 = (g >> 2) & 0x3F
    b5 = (b >> 3) & 0x1F
    return (r5 << 11) | (g6 << 5) | b5

def svg_to_png(svg_path, png_path, width=32, height=32):
    """Convert SVG to PNG with specified size"""
    import cairosvg
    cairosvg.svg2png(
        url=svg_path,
        write_to=png_path,
        output_width=width,
        output_height=height
    )
    print(f"✓ Converted SVG to PNG: {png_path}")
    return True

def convert_png_to_rgb565(input_png, output_bin, output_c, var_name, width=32, height=32):
    """Convert PNG to RGB565 binary with transparency"""
    img = Image.open(input_png)
    if img.mode != 'RGBA':
        img = img.convert('RGBA')
    if img.size != (width, height):
        img = img.resize((width, height), Image.Resampling.LANCZOS)

    bin_data = bytearray()
    pixels = img.load()

    for y in range(height):
        for x in range(width):
            r, g, b, a = pixels[x, y]
            rgb565 = rgb888_to_rgb565(r, g, b)
            bin_data.extend(struct.pack('<H', rgb565))

    with open(output_bin, 'wb') as f:
        f.write(bin_data)
    print(f"✓ Binary: {output_bin} ({len(bin_data)} bytes)")

    # Generate C header
    with open(output_c, 'w') as f:
        f.write(f"// Auto-generated {width}x{height} RGB565 icon\n")
        f.write(f"#ifndef {var_name.upper()}_H\n")
        f.write(f"#define {var_name.upper()}_H\n\n")
        f.write(f"#include <stdint.h>\n\n")
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
        f.write(f"#define {var_name.upper()}_WIDTH {width}\n")
        f.write(f"#define {var_name.upper()}_HEIGHT {height}\n\n")
        f.write(f"#endif\n")
    
    print(f"✓ Header: {output_c}")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python3 convert_svg_to_bin.py <input.svg> [output_name] [width] [height]")
        sys.exit(1)

    input_svg = sys.argv[1]
    output_name = sys.argv[2] if len(sys.argv) > 2 else os.path.splitext(os.path.basename(input_svg))[0]
    width = int(sys.argv[3]) if len(sys.argv) > 3 else 32
    height = int(sys.argv[4]) if len(sys.argv) > 4 else 32

    temp_png = f"{output_name}_temp.png"
    output_bin = f"{output_name}.bin"
    output_c = f"{output_name}.h"

    print(f"\n🔄 Converting {input_svg} ({width}x{height})...")
    svg_to_png(input_svg, temp_png, width, height)
    convert_png_to_rgb565(temp_png, output_bin, output_c, output_name, width, height)
    
    if os.path.exists(temp_png):
        os.remove(temp_png)
    
    print(f"✅ Done!\n")
