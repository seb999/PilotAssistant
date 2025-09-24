"""
Splash screen loader for RGB565 binary image
"""

def show_splash_screen(display):
    """Load and display RGB565 splash screen"""
    try:
        width, height = 240, 230

        with open("splash_rgb565.bin", "rb") as f:
            data = f.read()

        # Show image (top-left corner at 0,0)
        display.blit_buffer(data, 0, 0, width, height)
        print("RGB565 splash screen loaded successfully")

    except Exception as e:
        print(f"Error loading RGB565 splash: {e}")
        # Fallback to simple text
        display.clear(0x0000)  # Clear with black
        raise e  # Re-raise so main.py can handle fallback