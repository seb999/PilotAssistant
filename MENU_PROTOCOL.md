# Menu Synchronization Protocol

## Overview
The Pico (master) sends menu state updates to the RPi (slave) via USB serial to keep both displays synchronized.

## Serial Configuration
- **Baud Rate**: 115200
- **Device**: `/dev/ttyACM0` (RPi side)
- **Format**: JSON messages, one per line

## Message Format

### Menu Update Message
Sent whenever the menu selection changes.

```json
{"type":"menu","selected":0,"total":4}
```

**Fields:**
- `type`: Always "menu" for menu updates
- `selected`: Index of currently selected item (0-based)
- `total`: Total number of menu items

### Menu Items
The menu items are in a fixed order known to both systems:

| Index | Label        | Description                |
|-------|--------------|----------------------------|
| 0     | GO FLY       | Enter flight mode          |
| 1     | BLUETOOTH    | Bluetooth configuration    |
| 2     | GYRO OFFSET  | Gyroscope calibration      |
| 3     | RADAR        | Traffic radar display      |

### Splash Screen Message
Sent when showing splash screen.

```json
{"type":"splash"}
```

### Action Selected Message
Sent when user presses button to select current menu item.

```json
{"type":"action","selected":0}
```

## Communication Flow

### Startup Sequence
1. **Pico**: Shows splash screen
2. **Pico**: Sends `{"type":"splash"}`
3. **RPi**: Shows splash screen
4. **Pico**: After delay, shows menu
5. **Pico**: Sends `{"type":"menu","selected":0,"total":4}`
6. **RPi**: Shows menu with item 0 selected

### Navigation
1. **User**: Moves joystick UP/DOWN
2. **Pico**: Updates local display
3. **Pico**: Sends `{"type":"menu","selected":N,"total":4}`
4. **RPi**: Updates display to highlight item N

### Selection
1. **User**: Presses joystick button
2. **Pico**: Executes action locally
3. **Pico**: Sends `{"type":"action","selected":N}`
4. **RPi**: Shows action-specific screen (optional)

## Example Session

```
Pico → RPi: {"type":"splash"}
[2 second delay]
Pico → RPi: {"type":"menu","selected":0,"total":4}
[User presses DOWN]
Pico → RPi: {"type":"menu","selected":1,"total":4}
[User presses DOWN]
Pico → RPi: {"type":"menu","selected":2,"total":4}
[User presses UP]
Pico → RPi: {"type":"menu","selected":1,"total":4}
[User presses button]
Pico → RPi: {"type":"action","selected":1}
```

## Error Handling

- **Parse errors**: Ignore malformed JSON, continue with current state
- **Unknown message types**: Ignore
- **Out of range indices**: Clamp to valid range [0, total-1]
- **Disconnect**: RPi shows "Waiting for Pico..." message
- **Reconnect**: Resume normal operation

## Implementation Notes

### Pico (Sender)
- Send messages via `printf()` (USB CDC)
- Send on every selection change
- Throttle updates if needed (avoid flooding)
- Messages are newline-terminated

### RPi (Receiver)
- Read from `/dev/ttyACM0` with timeout
- Parse JSON using simple string parsing (no library needed)
- Update LCD only on valid messages
- Buffer up to 256 bytes per message

## Future Extensions

### Heartbeat (Optional)
Periodic heartbeat to detect disconnection:
```json
{"type":"heartbeat","uptime":12345}
```

### Menu Labels (Optional)
Allow dynamic menu labels:
```json
{"type":"menu_labels","items":["GO FLY","BLUETOOTH","GYRO OFFSET","RADAR"]}
```

### State Sync (Optional)
Synchronize additional state (GPS, battery, etc.):
```json
{"type":"state","gps":true,"battery":85}
```
