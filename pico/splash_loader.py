"""
Splash screen loader for RGB565 binary image
"""

def show_splash_screen(display):
    """Load and display RGB565 splash screen for 320x240 display"""
    try:
        # New splash dimensions for 320x240 LCD
        width, height = 320, 240

        with open("splashPico_RGB565.bin", "rb") as f:
            data = f.read()

        # Show image (full screen)
        display.blit_buffer(data, 0, 0, width, height)
        print("RGB565 splash screen loaded successfully (320x240)")

    except Exception as e:
        print(f"Error loading RGB565 splash: {e}")
        # Fallback to simple text
        display.clear(0x0000)  # Clear with black
        raise e  # Re-raise so main.py can handle fallback