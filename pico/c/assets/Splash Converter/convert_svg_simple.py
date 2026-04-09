#!/usr/bin/env python3
"""Simple SVG to RGB565 converter using svglib and reportlab"""

from PIL import Image
from svglib.svglib import svg2rlg
from reportlab.graphics import renderPM
import struct
import sys
import os

def rgb888_to_rgb565(r, g, b):
    r5 = (r >> 3) & 0x1F
    g6 = (g >> 2) & 0x3F
    b5 = (b >> 3) & 0x1F
    return (r5 << 11) | (g6 << 5) | b5

def svg_to_png_simple(svg_path, png_path, width=32, height=32):
    drawing = svg2rlg(svg_path)
    renderPM.drawToFile(drawing, png_path, fmt="PNG")
    # Resize
    img = Image.open(png_path)
    img = img.resize((width, height), Image.Resampling.LANCZOS)
    img.save(png_path)
    return True

def convert_to_bin(input_svg, output_name, width=32, height=32):
    temp_png = f"{output_name}_temp.png"
    output_bin = f"{output_name}.bin"
    output_c = f"{output_name}.h"
    
    print(f"Converting {input_svg}...")
    svg_to_png_simple(input_svg, temp_png, width, height)
    
    img = Image.open(temp_png)
    if img.mode != 'RGBA':
        img = img.convert('RGBA')
    
    bin_data = bytearray()
    pixels = img.load()
    
    for y in range(height):
        for x in range(width):
            r, g, b, a = pixels[x, y]
            rgb565 = rgb888_to_rgb565(r, g, b)
            bin_data.extend(struct.pack('<H', rgb565))
    
    with open(output_bin, 'wb') as f:
        f.write(bin_data)
    
    # Generate C header
    with open(output_c, 'w') as f:
        f.write(f"// {width}x{height} RGB565 icon\n")
        f.write(f"#ifndef {output_name.upper()}_H\n")
        f.write(f"#define {output_name.upper()}_H\n\n")
        f.write(f"#include <stdint.h>\n\n")
        f.write(f"const uint16_t {output_name}_data[{width * height}] = {{\n")
        
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
        f.write(f"#define {output_name.upper()}_WIDTH {width}\n")
        f.write(f"#define {output_name.upper()}_HEIGHT {height}\n#endif\n")
    
    os.remove(temp_png)
    print(f"✓ {output_bin}, {output_c}")

if __name__ == "__main__":
    input_svg = sys.argv[1]
    output_name = sys.argv[2] if len(sys.argv) > 2 else os.path.splitext(os.path.basename(input_svg))[0]
    width = int(sys.argv[3]) if len(sys.argv) > 3 else 32
    height = int(sys.argv[4]) if len(sys.argv) > 4 else 32
    convert_to_bin(input_svg, output_name, width, height)
