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
  - Frequency presets for self-synchronizing capture mode (supports ZX Spectrum 48K/128K pixel clocks).
  - Real-time adjustment of all parameters (changes applied immediately).
  - Settings can be saved to flash memory without restart.
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

### Performance Improvements

- **Video Output Optimization**: Streamlined DMA handling for both VGA and DVI/HDMI output modes, resulting in more efficient memory usage and cleaner code structure.
- **Buffer Management**: Simplified buffer switching mechanisms for improved video processing performance.

### Development Experience

- **PlatformIO Integration**: Full PlatformIO support with Arduino framework for easier development and dependency management.
- **Arduino IDE builds**: The project can also be built using the Arduino IDE (see [Arduino IDE Setup](#arduino-ide-setup) below).

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
   Open `platformio.ini` and set `default_envs` in the `[platformio]` section to one of:

   | Environment | `OSD_MENU_ENABLE` | `OSD_FF_ENABLE` |
   |-------------|:-----------------:|:---------------:|
   | `osd`       | ✓                 | ✓               |
   | `osd_menu`  | ✓                 |                 |
   | `ff_osd`    |                   | ✓               |
   | `no_osd`    |                   |                 |

   ```ini
   [platformio]
   default_envs = osd
   ```

3. **Select the board variant**  
   In the `[env]` section, uncomment exactly one `BOARD_*` flag:

   ```ini
   build_flags =
     -O3
     -D PICO_STDIO_USB
     -D BOARD_36LJU22 ; BOARD_11XGA24 ; BOARD_LEO_REV3 ; BOARD_09LJV23 ;
   ```

4. **Build and upload**  
   Use **PlatformIO: Build** and **PlatformIO: Upload** from the VS Code toolbar, or run:

   ```cmd
   pio run --target upload
   ```

---

## Arduino IDE Setup

1. **Install the board package**  
   Open **Tools → Board → Boards Manager**, search for **Raspberry Pi Pico/RP2040/RP2350** by Earle F. Philhower and install it.

   > **Arduino IDE 1.x:** You must first add the package URL manually.  
   > Open **File → Preferences** and add the following to **Additional Board Manager URLs**:  
   > `https://github.com/earlephilhower/arduino-pico/releases/download/global/package_rp2040_index.json`

2. **Select the board**  
   Go to **Tools → Board → Raspberry Pi RP2040 Boards** and select **Raspberry Pi Pico**.

3. **Configure build settings**  
   In the **Tools** menu set:
   - **Optimize** → `-O3`
   - **USB Stack** → `Pico SDK`

4. **Configure OSD features and board variant**  
   For Arduino IDE builds, these are controlled by the `#ifndef PLATFORMIO` block in `ZX_RGBI_TO_VGA_HDMI/g_config.h`.  
   Comment or uncomment the following lines before building:

   ```c
   // OSD features — enable or disable as needed:
   #define OSD_MENU_ENABLE
   #define OSD_FF_ENABLE

   // Board variant — uncomment exactly one:
   #define BOARD_36LJU22
   // #define BOARD_11XGA24
   // #define BOARD_LEO_REV3
   // #define BOARD_09LJV23
   ```

5. **Build and upload**  
   Open `ZX_RGBI_TO_VGA_HDMI/ZX_RGBI_TO_VGA_HDMI.ino` and use **Sketch → Upload**.
