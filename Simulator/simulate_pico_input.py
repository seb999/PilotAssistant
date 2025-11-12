#!/usr/bin/env python3
"""
Simulate Pico input for testing Raspberry Pi application
Creates a virtual serial port and sends button events
"""

import sys
import time
import os
import pty
import serial
import threading

def create_virtual_serial():
    """Create a virtual serial port pair"""
    master, slave = pty.openpty()
    slave_name = os.ttyname(slave)

    print(f"Virtual serial port created: {slave_name}")
    print(f"To use: sudo ln -s {slave_name} /dev/ttyACM0")
    print(f"Or modify PICO_DEVICE in main.c to: {slave_name}")

    return master, slave, slave_name

def send_button_events(master_fd):
    """Send simulated button events"""
    commands = [
        ("Starting simulation...", 1),
        ("BTN:down:PRESSED", 0.5),
        ("BTN:down:RELEASED", 0.5),
        ("BTN:down:PRESSED", 0.5),
        ("BTN:down:RELEASED", 0.5),
        ("BTN:up:PRESSED", 0.5),
        ("BTN:up:RELEASED", 0.5),
        ("BTN:right:PRESSED", 0.5),
        ("BTN:right:RELEASED", 1.5),
        ("BTN:down:PRESSED", 0.5),
        ("BTN:down:RELEASED", 0.5),
        ("BTN:press:PRESSED", 0.5),
        ("BTN:press:RELEASED", 1.5),
    ]

    for cmd, delay in commands:
        if cmd.startswith("BTN:"):
            message = f"{cmd}\n"
            os.write(master_fd, message.encode())
            print(f"Sent: {cmd}")
        else:
            print(cmd)
        time.sleep(delay)

def interactive_mode(master_fd):
    """Interactive mode - press keys to send button events"""
    print("\n=== Interactive Mode ===")
    print("Commands:")
    print("  w/up    - BTN:up:PRESSED")
    print("  s/down  - BTN:down:PRESSED")
    print("  a/left  - BTN:left:PRESSED")
    print("  d/right - BTN:right:PRESSED")
    print("  space   - BTN:press:PRESSED")
    print("  1       - BTN:key1:PRESSED")
    print("  4       - BTN:key4:PRESSED (exit)")
    print("  q       - Quit simulator")
    print("")

    import termios
    import tty

    # Save terminal settings
    old_settings = termios.tcgetattr(sys.stdin)

    try:
        tty.setcbreak(sys.stdin.fileno())

        while True:
            char = sys.stdin.read(1)

            button = None
            if char in ['w', '\x1b[A']:  # w or up arrow
                button = "up"
            elif char in ['s', '\x1b[B']:  # s or down arrow
                button = "down"
            elif char in ['a', '\x1b[D']:  # a or left arrow
                button = "left"
            elif char in ['d', '\x1b[C']:  # d or right arrow
                button = "right"
            elif char == ' ':
                button = "press"
            elif char == '1':
                button = "key1"
            elif char == '4':
                button = "key4"
            elif char == 'q':
                print("\nQuitting...")
                break

            if button:
                message = f"BTN:{button}:PRESSED\n"
                os.write(master_fd, message.encode())
                print(f"Sent: BTN:{button}:PRESSED")
                time.sleep(0.05)
                message = f"BTN:{button}:RELEASED\n"
                os.write(master_fd, message.encode())

    finally:
        # Restore terminal settings
        termios.tcsetattr(sys.stdin, termios.TCSADRAIN, old_settings)

def main():
    print("=== Pico Input Simulator ===\n")

    # Create virtual serial port
    master, slave, slave_name = create_virtual_serial()

    print("\nOptions:")
    print("1. Auto mode - sends predefined sequence")
    print("2. Interactive mode - control with keyboard")

    try:
        choice = input("\nSelect mode (1/2): ").strip()

        if choice == "1":
            print("\nStarting auto mode...")
            send_button_events(master)
            print("\nAuto sequence complete")
        elif choice == "2":
            interactive_mode(master)
        else:
            print("Invalid choice")

    except KeyboardInterrupt:
        print("\n\nInterrupted")
    finally:
        os.close(master)
        os.close(slave)
        print("Virtual serial port closed")

if __name__ == "__main__":
    main()
