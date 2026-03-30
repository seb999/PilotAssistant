from pathlib import Path
from library import ST7789
from menu.traffic_menu import display_traffic_page


def validate_lcd_library():
    """Fail fast if an unexpected ST7789 module is imported."""
    expected = Path(__file__).resolve().parent / "library" / "ST7789.py"
    loaded = Path(ST7789.__file__).resolve()
    if loaded != expected:
        raise RuntimeError(f"Unexpected LCD driver module: {loaded} (expected {expected})")
    print(f"LCD driver: {loaded}")


def main():
    validate_lcd_library()

    lcd = ST7789.ST7789()
    lcd.Init()
    lcd.clear()
    lcd.bl_DutyCycle(50)

    try:
        display_traffic_page(lcd)
    finally:
        try:
            lcd.clear()
        except Exception:
            pass


if __name__ == "__main__":
    main()
