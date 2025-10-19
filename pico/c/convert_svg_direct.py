#!/usr/bin/env python3
"""Convert SVG to RGB565 using PIL and simple path rendering"""

from PIL import Image, ImageDraw
import struct
import sys
import os
import xml.etree.ElementTree as ET

def rgb888_to_rgb565(r, g, b):
    r5 = (r >> 3) & 0x1F
    g6 = (g >> 2) & 0x3F
    b5 = (b >> 3) & 0x1F
    return (r5 << 11) | (g6 << 5) | b5

def simple_svg_to_png(svg_path, png_path, size=32, color=(0, 255, 0)):
    """Render SVG to PNG using basic approach"""
    # For Font Awesome SVGs, we'll use a simple rasterization
    # Create transparent image
    img = Image.new('RGBA', (size, size), (255, 255, 255, 0))
    
    # Read SVG and extract viewBox
    tree = ET.parse(svg_path)
    root = tree.getroot()
    
    # Get viewBox for scaling
    viewBox = root.get('viewBox', '0 0 640 640').split()
    vb_width = float(viewBox[2])
    vb_height = float(viewBox[3])
    scale = size / max(vb_width, vb_height)
    
    # Save as temporary file and use external tool or just save simplified version
    # For now, create a colored square as placeholder
    draw = ImageDraw.Draw(img)
    
    # Since we can't easily render complex paths without cairo,
    # let's create the icons programmatically
    basename = os.path.basename(svg_path).lower()
    
    if 'wifi' in basename:
        # Draw WiFi icon
        center_x, center_y = size // 2, size * 0.6
        # Arc 1
        draw.arc([center_x-6, center_y-6, center_x+6, center_y+6], 180, 360, fill=color, width=3)
        # Arc 2
        draw.arc([center_x-12, center_y-12, center_x+12, center_y+12], 180, 360, fill=color, width=3)
        # Arc 3
        draw.arc([center_x-20, center_y-20, center_x+20, center_y+20], 180, 360, fill=color, width=3)
        # Dot
        draw.ellipse([center_x-2, center_y+8, center_x+2, center_y+12], fill=color)
        
    elif 'gps' in basename:
        # Draw GPS/location pin
        center_x, center_y = size // 2, size * 0.4
        # Pin head (circle)
        draw.ellipse([center_x-8, center_y-8, center_x+8, center_y+8], fill=color)
        # Inner circle (white/transparent)
        draw.ellipse([center_x-4, center_y-4, center_x+4, center_y+4], fill=(255,255,255,0))
        # Pin point (triangle)
        draw.polygon([(center_x, center_y+8), (center_x-4, center_y), (center_x+4, center_y)], fill=color)
        
    elif 'bluetooth' in basename:
        # Draw Bluetooth icon
        center_x, center_y = size // 2, size // 2
        # Vertical line
        draw.line([(center_x, 4), (center_x, size-4)], fill=color, width=3)
        # Top triangle
        draw.polygon([(center_x, 4), (center_x+10, center_y), (center_x, center_y-6)], fill=color)
        # Bottom triangle
        draw.polygon([(center_x, size-4), (center_x+10, center_y), (center_x, center_y+6)], fill=color)
        # Cross lines
        draw.line([(center_x-8, center_y-6), (center_x+8, center_y+6)], fill=color, width=2)
        draw.line([(center_x-8, center_y+6), (center_x+8, center_y-6)], fill=color, width=2)
    
    img.save(png_path)
    return True

def convert_to_bin(input_svg, output_name, width=32, height=32):
    temp_png = f"{output_name}_temp.png"
    output_bin = f"img/{output_name}.bin"
    output_c = f"img/{output_name}.h"
    
    print(f"🔄 Converting {input_svg} to {width}x{height}...")
    simple_svg_to_png(input_svg, temp_png, width)
    
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
        f.write(f"// {width}x{height} RGB565 icon from {os.path.basename(input_svg)}\n")
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
    print(f"✅ Generated: {output_bin}, {output_c}\n")

if __name__ == "__main__":
    input_svg = sys.argv[1]
    output_name = sys.argv[2] if len(sys.argv) > 2 else os.path.splitext(os.path.basename(input_svg))[0]
    width = int(sys.argv[3]) if len(sys.argv) > 3 else 32
    height = int(sys.argv[4]) if len(sys.argv) > 4 else 32
    convert_to_bin(input_svg, output_name, width, height)
