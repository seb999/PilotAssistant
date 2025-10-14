import time

def display_go_fly_menu(lcd, input_handler):
    print("Displaying Go Fly menu...")

    # Start dual-core artificial horizon
    if lcd is not None:
        from menu import artificial_horizon
        # Call the main display function - it will run until user exits with joystick left
        artificial_horizon.display_artificial_horizon(lcd, input_handler)

    print("Exited artificial horizon, returning to main menu")
