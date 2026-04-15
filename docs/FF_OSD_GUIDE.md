# FF OSD Guide

## Overview

This firmware can act as an external on-screen display for a Gotek floppy emulator running FlashFloppy.

Two I2C display modes are supported:

- **FlashFloppy protocol** - Native FF OSD mode with up to 40 columns, up to 4 rows, and support for double-height rows
- **LCD HD44780 compatibility** - Emulates a PCF8574-style HD44780 I2C backpack at the standard `0x27` address

FF OSD uses the same three local buttons as the normal setup OSD:

In native **FlashFloppy** mode, short taps are forwarded:

- **UP** -> FlashFloppy **LEFT**
- **DOWN** -> FlashFloppy **RIGHT**
- **SEL** -> FlashFloppy **SELECT**

Long **SEL** hold is reserved for opening the local setup OSD menu.

## Hardware Connections

### Pico I2C Pins

The FF OSD interface uses `I2C0` on these Pico pins:

| Signal | GPIO   | Pico Pin | Notes                    |
|--------|--------|----------|--------------------------|
| SDA    | GPIO20 | 26       | I2C data                 |
| SCL    | GPIO21 | 27       | I2C clock                |
| GND    | GND    | 28       | Common ground with Gotek |

### Wiring Notes

- Connect Pico **GPIO20** to the Gotek I2C **SDA** line
- Connect Pico **GPIO21** to the Gotek I2C **SCL** line
- Connect Pico **GND** to Gotek **GND**
- The bus runs at **100 kHz**
- SDA and SCL must be pulled up to **3.3V** with about **4.7k to 10k** resistors
- Do not use 5V pull-ups on the Pico I2C pins

## Protocol Modes

### FlashFloppy Protocol

Use this mode when you want the Gotek to drive the on-screen text directly.

- Pico I2C slave address: `0x10`
- Text width: up to **40 columns**
- Text height: up to **4 rows**
- Double-height rows are supported when the host sends them
- Rows and columns are controlled by the host display data rather than the local LCD geometry settings

### LCD HD44780 Compatibility

Use this mode if the host expects a common I2C LCD backpack instead of the native FF OSD protocol.

- Pico I2C slave address: `0x27`
- Column range: **16 to 40**
- Row options: **2** or **4**
- Maximum local text area: **80 characters total**

Examples:

- `20 x 4` = 80 characters
- `40 x 2` = 80 characters
- `40 x 4` is not allowed

## OSD Menu Configuration

For generic local menu navigation, tuning-mode behavior, and save/exit flow, see [OSD Menu Guide](OSD_MENU_GUIDE.md). This section only covers FF-specific menu items.

Open the normal converter OSD and go to:

```text
MAIN MENU
  FF OSD CONFIG >
```

The submenu layout is:

```text
ENABLE    ON/OFF
PROTOCOL  FLASHFLOPPY/LCD HD44780
ROWS      [value]
COLUMNS   [value]
H_POS     LEFT..RIGHT
V_POS     TOP/BOTTOM
< BACK TO MAIN
```

### Menu Items

#### **ENABLE**

- Turns FF OSD on or off at runtime without reflashing
- When set to **OFF**, the I2C interface stops processing and no overlay is drawn
- When switched from OFF to ON, the I2C peripheral is initialized automatically if it has not been initialized yet (i.e. when the device was started with FF OSD disabled)

#### **PROTOCOL**

- Toggles between native FlashFloppy mode and LCD compatibility mode
- Triggers a full I2C re-initialization on the next Core 1 loop cycle, applying the new slave address and resetting the display configuration

#### **ROWS**

- Used only in LCD compatibility mode
- Toggles between 2 and 4 rows
- Dimmed in FlashFloppy mode

#### **COLUMNS**

- Used only in LCD compatibility mode
- Adjustable from 16 to 40 columns in tuning mode
- Dimmed in FlashFloppy mode

#### **H_POS**

- Horizontal placement presets:
  - LEFT
  - LEFT-CENTER
  - CENTER
  - CENTER-RIGHT
  - RIGHT

#### **V_POS**

- Selects whether the FF OSD appears at the top or bottom of the video output

## FlashFloppy Configuration

### Native FF OSD Mode

For the native FlashFloppy protocol, configure the Gotek with an FF.CFG similar to:

```ini
display-type = auto
osd-display-order = 3,0
osd-columns = 40
display-off-secs = 60
```

Notes:

- `display-type = oled-128x64` can also be used if that matches the host setup better
- `osd-display-order` and `display-order` can be configured independently
- A single FF OSD can show up to 40 columns and up to 4 rows
- With dual displays, FF OSD column count follows the primary display configuration used by the host

### LCD Compatibility Mode

For HD44780-compatible mode, use a configuration similar to:

```ini
display-type = lcd-20x04
display-order = 3,0,2,1
display-off-secs = 60
```

In this mode, the Pico behaves like a PCF8574 LCD backpack, and the visible layout is controlled by the local **ROWS** and **COLUMNS** settings in the converter.

## Runtime Behavior

- The FF OSD renderer is separate from the normal converter setup menu
- If the converter setup menu is open, that menu takes over the screen until it closes
- In FLASHFLOPPY protocol mode, opening the local converter menu uses a long **SEL** hold (about 1 second)
- In FLASHFLOPPY protocol mode, short UP/DOWN taps are forwarded to the host as LEFT/RIGHT navigation
- When the local menu is opened by a long hold, menu key handling is blocked until keys are released once to avoid carry-over scrolling/actions
- On local menu close, forwarding to FF OSD is also blocked until buttons are released once, preventing delayed release from triggering host actions
- When the host display is off, the FF OSD overlay is hidden
- In native FlashFloppy mode, host text can update asynchronously over I2C
- Native FF rendering uses a CP437-style glyph set (including box-drawing symbols and extended character support)
- Horizontal placement uses five fixed presets rather than pixel-by-pixel movement
- Vertical placement is only **TOP** or **BOTTOM**
- When FF OSD is **disabled**, I2C processing is suspended and no overlay is drawn; button state is cleared so the host does not receive stale inputs
- When FF OSD is **enabled** after being disabled at startup, the I2C peripheral is initialized on the next Core 1 loop cycle
- When the **protocol is changed** at runtime, the I2C peripheral is fully re-initialized with the new slave address and display configuration on the next Core 1 loop cycle

## Defaults

Factory defaults for FF OSD settings are:

- Enabled: **No**
- Protocol: **FlashFloppy**
- Columns: **40**
- Rows: **3**
- Horizontal position: **CENTER**
- Vertical position: **TOP**

## Troubleshooting

### No FF OSD visible

- Check that Pico and Gotek share a common ground
- Check SDA/SCL wiring and pull-up resistors
- Confirm the selected protocol matches the host configuration
- Confirm the host is trying to drive an external display
- Check whether the host has turned the display off with its timeout setting

### Wrong address or no I2C response

- Use `0x10` for native FlashFloppy protocol
- Use `0x27` for LCD HD44780 compatibility mode
- After changing protocol in the menu, the Pico re-initializes I2C with the new address on the next Core 1 loop cycle

### Rows or columns cannot be changed

- That is expected in **FlashFloppy** mode
- Switch to **LCD HD44780** mode to edit local rows and columns

### Text width looks wrong

- In FlashFloppy mode, width comes from the host display data
- In LCD mode, width comes from the local **COLUMNS** setting

### Russian filenames are garbled

- This firmware can display them, but the Gotek side may need a FlashFloppy build with Russian filename support
