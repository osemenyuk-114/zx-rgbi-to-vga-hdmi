# zx-rgbi-to-vga-hdmi

A converter for ZX Spectrum RGBI video signals to modern VGA and HDMI displays.

For detailed hardware and original software information, see the upstream project:
[ZX_RGBI2VGA-HDMI](https://github.com/AlexEkb4ever/ZX_RGBI2VGA-HDMI/)

## Alternative Firmware Build: Native Pico SDK

If you prefer working directly with the **Raspberry Pi Pico SDK** and **CMake**, check out the companion project:
[zx-rgbi-to-vga-hdmi-PICOSDK](https://github.com/osemenyuk-114/zx-rgbi-to-vga-hdmi-PICOSDK)

That companion firmware version:

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
  - OSD menu control via keyboard (F9, arrows, Enter, Esc).
  - Gotek/FlashFloppy control via keyboard (F10 toggle, arrows, Enter).
  - NMI signal via F11: CH446Q mode — closes switch Y5:X10; EPM3256 mode — sends NMI bit in SPI frame (EPM3256 emulates button press).
  - RESET signal via F12: CH446Q mode — closes switch Y6:X11; EPM3256 mode — sends RESET bit in SPI frame.
  - USB mouse support with Kempston-compatible output (SPI/EPM3256 builds); F6 toggles button mapping.
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
    [RGBtoHDMI](https://github.com/hoglet67/RGBtoHDMI)

---

## Removed Features

- Z80 CLK external clock source. Self-sync capture mode is now preferred.

---

## Recent Improvements

### Keyboard Support

- **PS/2 Keyboard**: PIO-based driver with IRQ-driven scancode decoding.
- **USB Keyboard**: TinyUSB Host boot keyboard driver with O(1) HID→universal key mapping.
- **ZX Spectrum Emulation**: Universal→ZX 8×5 matrix mapping via CH446Q analog switch.
- **OSD Control**: F9 toggles menu, arrows/Enter/Esc navigate. Controlled repeat (400ms delay, 80ms rate).
- **Gotek Control**: F10 toggles keyboard→Gotek mode (arrows→LEFT/RIGHT, Enter→SELECT). Cyan text indicator.
- **NMI / RESET**: F11/F12 are level-based. CH446Q mode: directly drives switches Y5:X10 (NMI) and Y6:X11 (RESET). EPM3256 mode: NMI/RESET bits sent in every SPI frame; EPM3256 emulates button presses. EPM3256 V0: not supported.
- **USB Mouse**: Kempston-compatible X/Y accumulation and buttons (SPI builds). Default: right→D0, left→D1 (original schematic). F6 toggles mapping.
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
- **Memory Safety**: All video buffer allocations are checked — `watchdog_reboot()` on allocation failure prevents undefined behavior.
- **Dual-Core Synchronization**: Memory barriers (`__dmb()`) on all cross-core flag variables (`stop_core1`, `core1_inactive`, `buf_is_free[]`) ensure correct operation on both RP2040 (Cortex-M0+) and RP2350 (Cortex-M33 with caches).
- **Clean Video Mode Switching**: ISR state variables (`y`, `scr_buffer`, `active_buf_idx`) are reset on `stop_dvi()`/`stop_vga()`, eliminating first-frame glitches after mode changes.
- **FF OSD Integration**: Added dedicated FlashFloppy/Gotek I2C OSD support, including protocol switching and separate documentation for setup and usage.
- **FF OSD Runtime Control**: FF OSD can be enabled/disabled and the protocol switched at runtime; both operations trigger a full I2C re-initialization on the next Core 1 loop cycle.
- **Memory Optimization**: Reduced unnecessary memory allocations and pointer complexity in video output modules.
- **Architecture Refinements**: Better separation of concerns between video input capture and output generation systems.
- **Maintainability**: Cleaner code structure while preserving critical hardware-specific requirements for reliable video processing.

---

## PlatformIO Setup

1. **Install PlatformIO**  
   Install the [PlatformIO IDE extension](https://platformio.org/install/ide?install=vscode) for VS Code, or use the PlatformIO CLI.

2. **Select the board environment**  
   Open `platformio.ini` and uncomment the desired board in `default_envs`. Each board environment automatically includes the correct feature flags, pin mappings, and source filters:

   | Environment     | OSD Menu | FF OSD | PS/2 Kbd | USB Kbd | Serial¹ | VGA/DVI Auto | Notes                                  |
   |-----------------|:--------:|:------:|:--------:|:-------:|:-------:|:------------:|----------------------------------------|
   | `36LJU22`       | ✓        | ✓      |          |         | ✓       | ✓            |                                        |
   | `RP2040_ZERO`   | ✓        | ✓      |          |         | ✓       | ✓            | WS2812 LED                             |
   | `38LJE24`       | ✓        | ✓      | ✓        | ✓       |         | ✓            | DVI pins reversed, VGA R/B swapped     |
   | `11XGA24_1`     | ✓        |        |          |         | ✓       |              | No I2C, no FF OSD                      |
   | `11XGA24_2`     | ✓        |        |          |         | ✓       |              | No I2C, no FF OSD (alt pin config)     |
   | `LEO_V2`        | ✓        | ✓      |          |         | ✓       | ✓            | WS2812 LED                             |
   | `LEO_V3`        | ✓        | ✓      | ✓        | ✓       |         | ✓            | SPI keyboard (EPM3256), WS2812 LED     |
   | `LEO_V3_2040BT` | ✓        | ✓      | ✓        | ✓       |         | ✓            | SPI keyboard (EPM3256)                 |
   | `09LJV23`       | ✓        | ✓      |          |         | ✓       | ✓            |                                        |

   ¹ Serial menu (`SERIAL_MENU_ENABLE` + `PICO_STDIO_USB`) is controlled via the `[env_serial]` section. Boards with keyboard support use USB Host mode by default; enable serial by editing the `[env_usb_kbd]` section.

   ```ini
   [platformio]
   default_envs =
     36LJU22
     ; RP2040_ZERO
     ; 38LJE24
     ; 11XGA24_1
     ; 11XGA24_2
     ; LEO_V2
     ; LEO_V3
     ; LEO_V3_2040BT
     ; 09LJV23
   ```

3. **USB mode**  
   Boards with keyboard support (`38LJE24`, `LEO_V3`, `LEO_V3_2040BT`) default to **Host mode** (USB keyboard). To switch to **Device mode** (Serial menu), edit the `[env_usb_kbd]` section:

   ```ini
   [env_usb_kbd]
   build_flags =
     ; Host mode (keyboard) — comment out these two lines to disable:
     -D USB_KBD_ENABLE
     -D NO_USB
   ```

   For boards without keyboard support (`36LJU22`, `RP2040_ZERO`, `LEO_V2`, `11XGA24_*`, `09LJV23`), serial is enabled via the `[env_serial]` section.

4. **Build and upload**  
   Use **PlatformIO: Build** and **PlatformIO: Upload** from the VS Code toolbar, or run:

   ```cmd
   pio run --target upload
   ```

### PIO Compilation (`.pio` -> `.pio.h`)

Header files `*.pio.h` are compiled and updated automatically during build.
