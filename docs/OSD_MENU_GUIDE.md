# OSD Menu Guide

## Hardware Connections

### Button Wiring

Connect three push buttons to the Raspberry Pi Pico (pins 31-34):

| Button | GPIO   | Pico Pin | Connection                            |
|--------|--------|----------|---------------------------------------|
| UP     | GPIO26 | 31       | Connect button between GPIO26 and GND |
| DOWN   | GPIO27 | 32       | Connect button between GPIO27 and GND |
| GND    | GND    | 33       | Common ground for all buttons         |
| SEL    | GPIO28 | 34       | Connect button between GPIO28 and GND |

**Notes:**

- All buttons use internal pull-up resistors (active LOW)
- Press each button to connect its GPIO pin to GND
- No external resistors required

## Button Layout

- **UP** - Navigate up / Increase value
- **DOWN** - Navigate down / Decrease value
- **SEL** - Select item / Toggle mode / Confirm

## Basic Navigation

### Opening the Menu

- **Default behavior:** Press **UP**, **DOWN**, or **SEL** to open the OSD menu
- **When FF OSD is enabled in FLASHFLOPPY protocol mode:** Hold **SEL** for about 1 second to open the menu
- When opened by a long hold, input is blocked until all buttons are released once

### Closing the Menu

- Select **EXIT** from main menu, or
- Menu auto-closes after 10 seconds of inactivity

### Video Output Type Toggle

- **Hold SEL for 5+ seconds** - toggles between VGA and DVI output
  - Does not open menu
  - Automatically switches output and resets to default resolution
  - Short press of SEL has no action when menu is closed

## Menu Structure

### MAIN MENU

```text
OUTPUT SETTINGS      >
CAPTURE SETTINGS     >
IMAGE ADJUST         >
FF OSD CONFIG        >
ABOUT                >
SAVE
EXIT
```

**Note:**

- **FF OSD CONFIG** is available on firmware builds with FlashFloppy OSD support enabled

### OUTPUT SETTINGS

```text
MODE         [resolution]    - Video output resolution
SCANLINES    ON/OFF          - Scanline filter (VGA only, certain modes)
BUFFERING    X1/X3           - Frame buffering mode
< BACK TO MAIN
```

**MODE Setting:**

- Press SEL to enter tuning mode (`>` indicator, bright cyan highlight)
- Use UP/DOWN to cycle through available resolutions
- Press SEL again to apply and exit tuning mode
- Only restarts video if resolution actually changed

**Available Modes:**

- **DVI:** 640x480@60Hz, 720x576@50Hz
- **VGA:** 640x480@60Hz, 800x600@60Hz, 1024x768@60Hz, 1280x1024@60Hz (DIV3/DIV4)

### CAPTURE SETTINGS

```text
FREQ      [value]            - Capture frequency (Hz)
MODE      SELF-SYNC/EXTERNAL - Sync source
DIVIDER   [value]            - External clock divider (EXT mode only)
SYNC      COMPOSITE/SEPARATE - Video sync mode
MASK      >                  - Pin inversion mask submenu
< BACK TO MAIN
```

**Adjustable Parameters (FREQ, DIVIDER):**

- Press SEL to enter tuning mode (`>` indicator, bright cyan highlight)
- Use UP/DOWN to adjust value
- Press SEL again to exit tuning mode

**FREQ Acceleration:**

- First second: 100 Hz steps
- After 1s: 1 kHz steps
- After 2s: 10 kHz steps
- After 5s: 100 kHz steps

**Toggle Parameters (MODE, SYNC):**

- Press SEL to toggle between values

### PIN INVERSION MASK

```text
F   (FREQ)      ON/OFF
SSI (HSYNC)     ON/OFF
KSI (VSYNC)     ON/OFF
I   (BRIGHT)    ON/OFF
R   (RED)       ON/OFF
G   (GREEN)     ON/OFF
B   (BLUE)      ON/OFF
< BACK TO CAPTURE
```

- Press SEL on any signal to toggle inversion ON/OFF
- Changes apply immediately and restart capture

### IMAGE ADJUST

```text
H_POS    [value]             - Horizontal position
V_POS    [value]             - Vertical position
DELAY    [value]             - Capture delay
RESET TO DEFAULTS
< BACK TO MAIN
```

**Adjustable Parameters (H_POS, V_POS, DELAY):**

- Press SEL to enter tuning mode (`>` indicator, bright cyan highlight)
- Use UP/DOWN to adjust value
- Press SEL again to exit tuning mode

**RESET TO DEFAULTS:**

- Press SEL to reset all image adjustment values
- Menu closes automatically after reset

### ABOUT

```text
VERSION   [version number]

https://github.com/
osemenyuk-114/
zx-rgbi-to-vga-hdmi

< BACK TO MAIN
```

Displays:

- Current firmware version
- Project GitHub URL

Press SEL on BACK to return to the main menu.

### FF OSD CONFIG

```text
ENABLE    ON/OFF
PROTOCOL  FLASHFLOPPY/LCD HD44780
ROWS      [value]
COLUMNS   [value]
H_POS     LEFT..RIGHT
V_POS     TOP/BOTTOM
< BACK TO MAIN
```

This menu configures the optional I2C on-screen display interface for Gotek drives running FlashFloppy.

- **ENABLE** toggles FF OSD runtime state.
- **PROTOCOL** switches between native **FLASHFLOPPY** and **LCD HD44780** compatibility modes.
- **ROWS** and **COLUMNS** are editable only in LCD mode.
- **H_POS** and **V_POS** control FF OSD placement.

To avoid duplicating protocol behavior, addresses, host-side configuration, and troubleshooting, see:

- [FF OSD Guide](FF_OSD_GUIDE.md)

## Visual Indicators

### Selection Highlighting

- **White background** - Currently selected item (normal)
- **Bright cyan background** - Selected item in tuning mode
- **`>` indicator** - Tuning mode active (left side of line)
- **Cyan text** - Dimmed/unavailable items

### Menu Colors

- **Bright cyan text** - Title text
- **Bright white text** - Tuning mode highlight
- **White text** - Border and normal items
- **Cyan text** - Dimmed/unavailable items
- **Black background** - Default background

## Saving Settings

From the main menu:

1. Select **SAVE**
2. Press SEL
3. Settings are saved to flash memory
4. Menu closes automatically

To exit without saving, select **EXIT** instead.

## Tips

- Menu has 10-second auto-timeout - any button press resets the timer
- Dimmed items indicate unavailable settings (e.g., SCANLINES on DVI, DIVIDER when MODE is SELF-SYNC)
- Tuning mode allows real-time adjustment while viewing the image
- Video mode changes only restart output if the resolution actually changed
- Long SEL press (5s) for quick VGA/DVI toggle without opening menu
- FF OSD protocol-specific button behavior is documented in [FF OSD Guide](FF_OSD_GUIDE.md)
