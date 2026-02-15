# HUD Menu System Setup

## System Architecture

```
┌─────────────────┐         USB Serial          ┌──────────────────┐
│  Pico2 (Master) │  ────────────────────────>  │  RPi (HUD Slave) │
│                 │      (Menu State)            │                  │
│  - Joystick     │                              │  - LCD Display   │
│  - 320x240 LCD  │                              │  - 240x240 LCD   │
│  - Menu Control │                              │  - HUD Projection│
└─────────────────┘                              └──────────────────┘
```

## How It Works

1. **Pico** is the master controller:
   - Handles joystick input
   - Shows menu on its own 320x240 LCD
   - Sends menu state to RPi via USB serial (115200 baud)

2. **RPi** is the HUD display slave:
   - Receives menu updates from Pico
   - Displays synchronized menu on 240x240 LCD
   - LCD is projected onto semi-transparent plexiglass for HUD effect
   - Uses **white text on black background** for optimal HUD visibility

## HUD Display Features

### Color Scheme (Optimized for Projection)
- **Background**: Black (transparent when projected)
- **Normal Text**: White (bright, easy to see)
- **Selected Item**: Cyan (highlighted, stands out)
- **Indicators**: Cyan arrows and underlines

### Text Scaling
- **Selected Item**: 3x scale (large, highly visible)
- **Other Items**: 2x scale (readable but not distracting)
- **Large font ensures readability at distance**

### Layout
```
┌────────────────────────────┐
│         MENU              │  ← Title (cyan)
├───────────────────────────┤
│                           │
│  > GO FLY      (3x)       │  ← Selected (cyan, large)
│    ═══════════════        │  ← Underline
│                           │
│    BLUETOOTH   (2x)       │  ← Normal (white)
│                           │
│    GYRO OFFSET (2x)       │
│                           │
│    RADAR       (2x)       │
│                           │
├───────────────────────────┤
│      HUD DISPLAY          │  ← Footer
└────────────────────────────┘
```

## Build Instructions

### On Raspberry Pi

```bash
cd rpi/c/build
cmake ..
make pilot_assistant_hud
```

### On Pico2

```bash
cd pico/c/build
cmake -DPICO_BOARD=pico2 ..
make menu_system
```

## Running the System

### Step 1: Start RPi HUD Display

```bash
cd rpi/c/build
./pilot_assistant_hud
```

**Output:**
```
=== Pilot Assistant HUD Display ===
Pico Device: /dev/ttyACM0
Baud Rate: 115200
Press Ctrl+C to exit

Initializing LCD (HUD mode)...
✓ LCD initialized
Connecting to Pico...
✓ Connected to Pico

Waiting for menu data from Pico...
```

**What you'll see on LCD:**
- Splash screen: "PILOT ASSISTANT - HUD MODE - Waiting for Pico..."

### Step 2: Start/Flash Pico

1. Flash `menu_system.uf2` to Pico
2. Pico will:
   - Show splash screen
   - Send `{"type":"splash"}` to RPi
   - After 2 seconds, show menu
   - Send `{"type":"menu","selected":0,"total":4}` to RPi

### Step 3: Use the System

**On Pico:**
- Move joystick UP/DOWN to navigate menu
- Press joystick button to select

**On RPi HUD:**
- Display automatically updates to match Pico
- Selected item is highlighted in cyan with large text
- Arrow indicator (">") shows current selection

## Serial Protocol Messages

### Splash Screen
```json
{"type":"splash"}
```

### Menu Update
```json
{"type":"menu","selected":0,"total":4}
```
- `selected`: Index of current selection (0-3)
- `total`: Number of menu items (4)

### Action Selected
```json
{"type":"action","selected":1}
```
- Sent when user presses button
- HUD briefly flashes selection

## Menu Items

| Index | Label        | Function           |
|-------|--------------|--------------------|
| 0     | GO FLY       | Enter flight mode  |
| 1     | BLUETOOTH    | BT configuration   |
| 2     | GYRO OFFSET  | Gyro calibration   |
| 3     | RADAR        | Traffic display    |

## Troubleshooting

### RPi says "Pico not connected"
- Check USB cable connection
- Verify Pico is powered on
- Check `/dev/ttyACM0` exists: `ls -la /dev/ttyACM*`
- May need to unplug/replug Pico USB

### HUD display is dim or hard to see
- Adjust LCD backlight brightness in code
- Check plexiglass angle and position
- Ensure room lighting isn't too bright
- Verify black background (check cable orientation if display looks inverted)

### Menu doesn't update
- Check serial connection
- Monitor serial output: `./pilot_assistant_hud` shows received messages
- Verify Pico is sending data: see printf output from Pico via USB serial monitor

### Display is mirrored/rotated wrong
- LCD rotation is configured in `st7789_rpi.c` initialization
- Python version uses 270° rotation
- Adjust based on your HUD mount orientation

## HUD Physical Setup

### Recommended Configuration

```
         │ Plexiglass (semi-transparent)
         │ /
         │/
    ┌────────┐
    │  LCD   │  ← Mounted facing plexiglass
    │ (RPi)  │     at ~45° angle
    └────────┘
         ↓
    Projection reflects onto plexiglass
         ↓
    Visible to pilot while looking forward
```

### Tips
1. **Angle**: Mount LCD at 30-45° angle to plexiglass
2. **Distance**: 10-20cm from plexiglass for sharp projection
3. **Brightness**: Adjust for ambient light conditions
4. **Background**: Pure black ensures only text is visible (HUD effect)
5. **Contrast**: High contrast (white/cyan on black) works best

## Testing Without Pico

You can test the RPi HUD display by sending test messages:

```bash
# In one terminal
./pilot_assistant_hud

# In another terminal
echo '{"type":"splash"}' > /dev/ttyACM0
sleep 2
echo '{"type":"menu","selected":0,"total":4}' > /dev/ttyACM0
sleep 1
echo '{"type":"menu","selected":1,"total":4}' > /dev/ttyACM0
sleep 1
echo '{"type":"menu","selected":2,"total":4}' > /dev/ttyACM0
```

## Next Steps

Once the HUD menu system is working, you can add:
- Real-time telemetry display (GPS, altitude, speed)
- Radar/traffic overlay
- Warning indicators
- Status icons (GPS lock, Bluetooth status)

All synchronized between Pico and RPi HUD!
