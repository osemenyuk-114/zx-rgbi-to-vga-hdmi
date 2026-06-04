# zx-rgbi-to-vga-hdmi

A converter for ZX Spectrum RGBI video signals to modern VGA and HDMI displays.

For detailed hardware and original software information, see the upstream project:  
🔗 [ZX_RGBI2VGA-HDMI](https://github.com/AlexEkb4ever/ZX_RGBI2VGA-HDMI/)

## Alternative Firmware Build: Native Pico SDK

If you prefer working directly with the **Raspberry Pi Pico SDK** and **CMake**, check out the companion project:  
🔗 [zx-rgbi-to-vga-hdmi-PICOSDK](https://github.com/osemenyuk-114/zx-rgbi-to-vga-hdmi-PICOSDK)

This version of the firmware:

- Uses the **native Pico SDK** instead of the Arduino framework  
- Enables more direct control and customization of **PIO programs**  
- Ideal for developers experimenting with low-level video signal processing or custom capture logic

---

## Documentation

- [OSD Menu Guide](docs/OSD_MENU_GUIDE.md) - local button controls, menu tree, and tuning workflow.
- [FF OSD Guide](docs/FF_OSD_GUIDE.md) - Gotek/FlashFloppy I2C wiring, protocol modes, and host configuration.
- [VGA Timings](docs/VGA_TIMINGS.md) - supported VGA/DVI timing tables.
- [Keyboard Guide](docs/KEYBOARD_GUIDE.md) - PS/2 and USB keyboard support, OSD and Gotek control, ZX Spectrum key mapping.

---

## Features

### Software

- **Video Output:**
  - VGA output with selectable resolutions: 640×480 @60Hz, 800×600 @60Hz, 1024×768 @60Hz, 1280×1024 @60Hz.
  - HDMI (DVI) resolutions: 640×480 @60Hz and 720×576 @50Hz.
  - Optional scanline effect on the VGA output at higher resolutions for a retro look.
  - "NO SIGNAL" message when no input is detected.
- **Keyboard Input:**
  - PS/2 keyboard support (PIO-based, IRQ-driven).
  - USB keyboard support (TinyUSB Host, boot protocol).
  - Full ZX Spectrum keyboard emulation via CH446Q analog switch matrix.
  - OSD menu control via keyboard (PrtScr, arrows, Enter, Esc).
  - Gotek/FlashFloppy control via keyboard (ScrLk toggle, arrows, Enter).
  - Visual indicator: FF OSD text turns Cyan when keyboard controls Gotek.
- **On-Screen Display (OSD) Menu:**
  - Full-featured graphical menu system overlaid on video output.
  - Three-button control (UP, DOWN, SEL) with live tuning and save-to-flash support.
  - Quick VGA/DVI toggle via long SEL press (5 seconds).
  - Auto-timeout after 10 seconds of inactivity.
  - See [OSD Menu Guide](docs/OSD_MENU_GUIDE.md) for detailed usage instructions.
- **FlashFloppy / Gotek OSD Support:**
  - Can act as an external I2C OSD for a Gotek running FlashFloppy.
  - Supports both native FF protocol and HD44780-compatible LCD emulation.
  - Runtime enable/disable and protocol switching are available from OSD and serial menus.
  - See [FF OSD Guide](docs/FF_OSD_GUIDE.md) for wiring and configuration details.
- **Configuration via Serial Terminal:**
  - Alternative text-based menu system for headless configuration.
  - Frequency presets for self-synchronizing capture mode (ZX Spectrum 48K/128K pixel clocks).
  - Real-time adjustment of all parameters (changes applied immediately).
  - Settings can be saved to flash memory without restart.
- **Capture Frequency Presets:** OSD and serial menus support preset snap for ZX Spectrum 48K (7.0 MHz) and 128K/+2/+2A/+3 (7.0938 MHz) pixel clocks.
- **Test/Welcome Screen:** Styled after the ZX Spectrum 128K.

### Hardware

- **Analog to Digital Conversion:** Converts analog RGB to digital RGBI.
  - Based on the project:  
🔗 [RGBtoHDMI](https://github.com/hoglet67/RGBtoHDMI)

---

## Removed Features

- Z80 CLK external clock source. Self-sync capture mode is now preferred.

---

## Recent Improvements

### Keyboard Support

- **PS/2 Keyboard**: PIO-based driver with IRQ-driven scancode decoding.
- **USB Keyboard**: TinyUSB Host boot keyboard driver with O(1) HID→universal key mapping.
- **ZX Spectrum Emulation**: Universal→ZX 8×5 matrix mapping via CH446Q analog switch.
- **OSD Control**: PrtScr opens menu, arrows/Enter/Esc navigate. Controlled repeat (400ms delay, 80ms rate).
- **Gotek Control**: ScrLk toggles keyboard→Gotek mode (arrows→LEFT/RIGHT, Enter→SELECT). Cyan text indicator.
- See [Keyboard Guide](docs/KEYBOARD_GUIDE.md) for full details.

### Video Output Stability

- DMA IRQ priority set to highest (`PICO_HIGHEST_IRQ_PRIORITY`) in both VGA and DVI drivers.
- Prevents USB Host ISR from blocking video output on Core 0.
- USB keyboard task throttled to 500µs interval.

### Project Structure

- Source reorganized into subfolders: `video/`, `osd/`, `kbd/`, `usb/`, `i2c/`.
- PlatformIO-only build (Arduino IDE support removed).
- `build_src_filter` per environment for selective compilation.

### Performance Improvements

- **Video Output Optimization**: Streamlined DMA handling for both VGA and DVI/HDMI output modes, resulting in more efficient memory usage and cleaner code structure.
- **Buffer Management**: Simplified buffer switching mechanisms for improved video processing performance.

### Development Experience

- **PlatformIO Integration**: Full PlatformIO support with Arduino framework for easier development and dependency management.

### Code Quality

- **Settings Integrity**: CRC-32 validation on saved settings — corrupted or uninitialized flash data is detected on boot and automatically replaced with safe defaults.
- **FF OSD Integration**: Added dedicated FlashFloppy/Gotek I2C OSD support, including protocol switching and separate documentation for setup and usage.
- **FF OSD Runtime Control**: FF OSD can be enabled/disabled and the protocol switched at runtime; both operations trigger a full I2C re-initialization on the next Core 1 loop cycle.
- **Memory Optimization**: Reduced unnecessary memory allocations and pointer complexity in video output modules.
- **Architecture Refinements**: Better separation of concerns between video input capture and output generation systems.
- **Maintainability**: Cleaner code structure while preserving critical hardware-specific requirements for reliable video processing.

---

## PlatformIO Setup

1. **Install PlatformIO**  
   Install the [PlatformIO IDE extension](https://platformio.org/install/ide?install=vscode) for VS Code, or use the PlatformIO CLI.

2. **Select the build environment**  
   Open `platformio.ini` and set `default_envs` in the `[platformio]` section to one or more of:

   | Environment | OSD Menu | FF OSD | Keyboard | Source filter                           |
   |-------------|:--------:|:------:|:--------:|-----------------------------------------|
   | `osd`       | ✓        | ✓      | ✓        | all files                               |
   | `osd-menu`  | ✓        |        | ✓        | excludes `ff_osd.c`, `i2c/`             |
   | `ff-osd`    |          | ✓      | ✓        | excludes `osd_menu.c`                   |
   | `no-osd`    |          |        |          | excludes `osd/`, `kbd/`, `usb/`, `i2c/` |

   ```ini
   [platformio]
   default_envs = osd
   ```

3. **Select the board variant**  
   In the `[env]` section, uncomment exactly one `BOARD_*` flag:

   ```ini
   build_flags =
     -D BOARD_36LJU22 ; BOARD_09LJV23 ; BOARD_25LEO25 ; BOARD_11XGA24 ; BOARD_38LJE24 ;
   ```

4. **USB mode**  
   By default, USB is configured for **Host mode** (USB keyboard). To use **Device mode** (Serial menu), change the flags:

   ```ini
   ; Host mode (keyboard):
   -D USB_KBD_ENABLE
   -D NO_USB

   ; Device mode (serial):
   ; -D USB_KBD_ENABLE
   ; -D NO_USB
   -D PICO_STDIO_USB
   ```

5. **Build and upload**  
   Use **PlatformIO: Build** and **PlatformIO: Upload** from the VS Code toolbar, or run:

   ```cmd
   pio run --target upload
   ```

### Source Structure

```text
src/
  main.cpp              Entry point (setup/loop, Core 0 + Core 1)
  g_config.h/c          Global config: board variants, feature flags, pin maps
  settings.h/c          Persistent settings (flash, CRC-32 validated)
  serial_menu.h/cpp     Serial terminal menu
  video/                Video subsystem
    rgb_capture.c/h       PIO-based RGBI input capture
    vga.c/h               VGA signal generation
    dvi.c/h               DVI/HDMI signal generation
    video_output.c/h      VGA/DVI output coordination
    v_buf.c/h             Video buffer management
    programs.pio          PIO assembly programs
  osd/                  On-screen display
    osd.c/h               OSD base layer
    osd_menu.c/h          OSD graphical menu system
    ff_osd.c/h            FlashFloppy/Gotek I2C OSD
    font.h                OSD font data
  kbd/                  Keyboard subsystem
    kbd.c/h               Keyboard dispatcher (PS/2 + USB merge)
    kbd_codes.c/h         Universal keyboard codes
    kbd_osd.c/h           Keyboard↔OSD bridge
    kbd_ps2.c/h           PS/2 keyboard driver (PIO + IRQ)
    kbd_usb.c/h           USB HID keyboard driver (TinyUSB Host)
    kbd_zx.c/h            ZX Spectrum 8×5 matrix mapping
    kbd_ch446q.c/h        CH446Q analog switch driver
  usb/                  TinyUSB host configuration
  i2c/                  I2C slave driver
```
