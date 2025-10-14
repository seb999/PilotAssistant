# artificial_horizon.py
# Optimized artificial horizon for Pico2 + ST7789 using existing display driver
# Dual-core: one core for drawing, one for simulation / IMU input
# Press KEY2 button to exit

import _thread, time, math

# -------------------------
# CONFIG
# -------------------------
WIDTH = 320
HEIGHT = 240
CENTER_X = WIDTH // 2
CENTER_Y = HEIGHT // 2
UPDATE_INTERVAL = 0.033  # ~30 FPS (33ms per frame)

# -------------------------
# SINE/COSINE TABLES
# -------------------------
TABLE_SIZE = 360
SIN_TABLE = [math.sin(math.radians(a)) for a in range(TABLE_SIZE)]
COS_TABLE = [math.cos(math.radians(a)) for a in range(TABLE_SIZE)]

def fast_sin(deg):
    return SIN_TABLE[int(deg) % TABLE_SIZE]

def fast_cos(deg):
    return COS_TABLE[int(deg) % TABLE_SIZE]

# -------------------------
# STATE SHARING (thread-safe)
# -------------------------
pitch = 0.0
roll = 0.0
lock = _thread.allocate_lock()
running = True  # Global flag to stop simulation

# -------------------------
# DRAWING
# -------------------------
def draw_horizon(display, pitch, roll):
    """Draw artificial horizon with banking (roll) visualization."""
    # Pre-calculate colors
    sky_color = display.color565(0, 80, 200)
    ground_color = display.color565(139, 69, 19)
    white = display.color565(255, 255, 255)

    # Horizon vertical offset from pitch (pixels per degree)
    horizon_offset = int(pitch * 2.0)

    # Calculate tilted horizon line based on roll angle
    # For each column x, calculate the y position of the horizon
    roll_rad = roll * 3.14159 / 180.0
    tan_roll = math.tan(roll_rad)

    # Draw vertical slices to create tilted horizon (faster than pixel by pixel)
    slice_width = 4  # Draw in 4-pixel wide columns for speed
    for x in range(0, WIDTH, slice_width):
        # Calculate horizon y position at this x coordinate
        dx = x - CENTER_X
        dy = int(dx * tan_roll)
        horizon_y = CENTER_Y + horizon_offset + dy

        # Clamp horizon to screen
        horizon_y_clamped = max(0, min(HEIGHT, horizon_y))

        # Draw sky slice above horizon
        if horizon_y_clamped > 0:
            display.draw_rect(x, 0, slice_width, horizon_y_clamped, sky_color)

        # Draw ground slice below horizon
        if horizon_y_clamped < HEIGHT:
            ground_height = HEIGHT - horizon_y_clamped
            display.draw_rect(x, horizon_y_clamped, slice_width, ground_height, ground_color)

    # Roll tick marks - rotate with bank angle
    for deg in [-30, 0, 30]:
        x1 = int(CENTER_X + 80 * fast_sin(deg + roll))
        y1 = int(CENTER_Y - 80 * fast_cos(deg + roll))
        x2 = int(CENTER_X + 70 * fast_sin(deg + roll))
        y2 = int(CENTER_Y - 70 * fast_cos(deg + roll))
        display.draw_line(x1, y1, x2, y2, white)

    # Aircraft symbol - drawn last to stay on top
    display.draw_rect(CENTER_X - 20, CENTER_Y - 2, 40, 4, white)  # Horizontal bar
    display.draw_rect(CENTER_X - 2, CENTER_Y - 10, 4, 20, white)   # Vertical bar

# -------------------------
# THREAD 1: DRAW LOOP
# -------------------------
def draw_thread(display):
    global pitch, roll, running
    while running:
        with lock:
            p = pitch
            r = roll
        draw_horizon(display, p, r)
        time.sleep(UPDATE_INTERVAL)

# -------------------------
# THREAD 0: SIMULATION (or IMU input)
# -------------------------
def simulation_loop(display, input_handler=None):
    """Simulates motion with pitch and bank; also handles joystick left to exit."""
    global pitch, roll, running
    t = 0.0
    exit_requested = False  # Debounce flag

    while running:
        t += UPDATE_INTERVAL

        # Simulate realistic flight maneuvers
        # Pitch: gentle oscillation ±15 degrees
        new_pitch = 15 * math.sin(t / 2.5)

        # Bank/Roll: more dramatic oscillation ±45 degrees with different frequency
        # This makes the roll tick marks rotate around the display
        new_roll = 45 * math.sin(t / 2.0)

        # Add a slower combined maneuver (like a turn)
        new_roll += 15 * math.sin(t / 5.0)

        with lock:
            pitch = new_pitch
            roll = new_roll

        # Check KEY2 button to exit (with debouncing)
        if input_handler and not exit_requested:
            changes = input_handler.read_inputs()
            for name, state in changes:
                if name == "key2" and state == 0:  # pressed
                    print("KEY2 pressed - exiting horizon")
                    exit_requested = True
                    running = False
                    break  # Stop processing other inputs

        time.sleep(UPDATE_INTERVAL)

# -------------------------
# ENTRY FUNCTION
# -------------------------
def display_artificial_horizon(display, input_handler=None):
    """Main function to start artificial horizon display."""
    global running
    running = True
    _thread.start_new_thread(draw_thread, (display,))
    simulation_loop(display, input_handler)  # run in main thread

    # Wait for draw thread to finish its current frame
    time.sleep(0.15)

    # Clear the display before exiting
    display.clear(0x0000)  # Black

    # Additional delay to ensure button release is processed
    time.sleep(0.1)

    print("Artificial horizon cleanup complete")
