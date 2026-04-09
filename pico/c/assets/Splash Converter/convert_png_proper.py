#!/usr/bin/env python3
from PIL import Image
import struct
import sys

def rgb888_to_rgb565(r, g, b):
    r5 = (r >> 3) & 0x1F
    g6 = (g >> 2) & 0x3F
    b5 = (b >> 3) & 0x1F
    return (r5 << 11) | (g6 << 5) | b5

def convert_png_icon(png_path, output_name):
    img = Image.open(png_path).convert('RGBA')
    width, height = img.size
    pixels = img.load()
    
    bin_data = bytearray()
    
    # For each pixel
    for y in range(height):
        for x in range(width):
            r, g, b, a = pixels[x, y]
            
            # If pixel is mostly transparent (alpha < 128), use white (will be skipped)
            # Otherwise convert to RGB565
            if a < 128:
                rgb565 = 0xFFFF  # White = transparent
            else:
                rgb565 = rgb888_to_rgb565(r, g, b)
            
            bin_data.extend(struct.pack('<H', rgb565))
    
    # Write binary file
    with open(f"img/{output_name}.bin", 'wb') as f:
        f.write(bin_data)
    
    # Write header file
    with open(f"img/{output_name}.h", 'w') as f:
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
        f.write(f"#define {output_name.upper()}_HEIGHT {height}\n")
        f.write(f"#endif\n")
    
    print(f"✅ Converted {png_path} -> img/{output_name}.bin/h")

if __name__ == "__main__":
    convert_png_icon("img/wifi24.png", "wifi_icon")
    convert_png_icon("img/GPS24.png", "gps_icon")
    convert_png_icon("img/bluetooth24.png", "bluetooth_icon")
    print("\n✅ All icons converted!")
