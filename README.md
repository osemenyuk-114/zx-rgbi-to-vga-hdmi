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

---

## Features

### Software

- **Video Output:**
  - VGA output with selectable resolutions: 640×480 @60Hz, 800×600 @60Hz, 1024×768 @60Hz, 1280×1024 @60Hz.
  - HDMI (DVI) resolutions: 640×480 @60Hz and 720×576 @50Hz.
  - Optional scanline effect on the VGA output at higher resolutions for a retro look.
  - "NO SIGNAL" message when no input is detected.
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

### Video Output Stability

- DMA IRQ priority set to highest (`PICO_HIGHEST_IRQ_PRIORITY`) in both VGA and DVI drivers.

### Project Structure

- Source reorganized into subfolders: `video/`, `osd/`, `i2c/`.
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
   Open `platformio.ini` and uncomment the desired environment in `default_envs` (one per line):

   | Environment | OSD Menu | FF OSD | Serial Menu | Source filter                           |
   |-------------|:--------:|:------:|:-----------:|-----------------------------------------|
   | `osd`       | ✓        | ✓      | ✓¹          | all files                               |
   | `osd-menu`  | ✓        |        | ✓¹          | excludes `ff_osd.c`, `i2c/`             |
   | `ff-osd`    |          | ✓      | ✓¹          | excludes `osd_menu.c`                   |
   | `no-osd`    |          |        | ✓           | excludes `osd/`, `i2c/`                 |

   ¹ Serial menu and USB stdio for `osd`, `osd-menu`, `ff-osd` are controlled via the `[env_serial]` section — uncomment `-D SERIAL_MENU_ENABLE` and `-D PICO_STDIO_USB` there to enable.
   For `no-osd`, serial menu is always enabled.

   ```ini
   [platformio]
   default_envs =
     osd
     ; ff-osd
     ; osd-menu
     ; no-osd
   ```

3. **Select the board variant**  
   In the `[env]` section `build_flags`, uncomment exactly one `-D BOARD_*` line:

   | Board           | VGA/DVI Auto-detect | FF OSD support | Notes                                     |
   |-----------------|:-------------------:|:--------------:|-------------------------------------------|
   | `BOARD_36LJU22` | ✓                   | ✓              |                                           |
   | `BOARD_38LJE24` | ✓                   | ✓              | DVI pins reversed, VGA R/B swapped        |
   | `BOARD_11XGA24` |                     |                | FF OSD automatically disabled in firmware |
   | `BOARD_25LEO25` | ✓                   | ✓              | Different OSD button pins                 |
   | `BOARD_09LJV23` | ✓                   | ✓              |                                           |

   ```ini
   build_flags =
     -D BOARD_36LJU22
     ; -D BOARD_38LJE24
     ; -D BOARD_11XGA24
     ; -D BOARD_25LEO25
     ; -D BOARD_09LJV23
   ```

   > **Note:** `BOARD_11XGA24` does not have I2C pins and does not support FF OSD. If you select the `osd` or `ff-osd` environment with this board, `OSD_FF_ENABLE` will be automatically undefined by the firmware.

4. **Build and upload**  
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
  i2c/                  I2C slave driver
```
