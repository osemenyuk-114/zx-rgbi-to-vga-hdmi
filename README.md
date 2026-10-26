# rgb-to-vga-hdmi

A converter for RGB video signals to modern VGA and HDMI displays.

For detailed hardware and original software information, see the upstream projects:

[ZX_RGBI2VGA-HDMI](https://github.com/AlexEkb4ever/ZX_RGBI2VGA-HDMI/)
[RGBI_TO_VGA](https://github.com/tchv71/RGBI_TO_VGA)

---

## Documentation

- [OSD Menu Guide](docs/OSD_MENU_GUIDE.md) - local button controls, menu tree, and tuning workflow.
- [VGA Timings](docs/VGA_TIMINGS.md) - supported VGA/DVI timing tables.

---

## Features

### Software

- **Video Output:**
  - VGA output with selectable resolutions: 640×480 @60Hz, 800×600 @60Hz, 1280×1024 @60Hz.
  - HDMI (DVI) resolutions: 640×480 @60Hz and 720×576 @50Hz.
  - Optional scanline effect on the VGA output at higher resolutions for a retro look.
  - "NO SIGNAL" message when no input is detected.
- **On-Screen Display (OSD) Menu:**
  - Full-featured graphical menu system overlaid on video output.
  - Three-button control (UP, DOWN, SEL) with live tuning and save-to-flash support.
  - Quick VGA/DVI toggle via long SEL press (5 seconds).
  - Auto-timeout after 10 seconds of inactivity.
  - See [OSD Menu Guide](docs/OSD_MENU_GUIDE.md) for detailed usage instructions.
- **Configuration via Serial Terminal:**
  - Alternative text-based menu system for headless configuration.
  - Frequency presets for self-synchronizing capture mode (ZX Spectrum 48K/128K pixel clocks).
  - Real-time adjustment of all parameters (changes applied immediately).
  - Settings can be saved to flash memory without restart.
- **Capture Frequency Presets:** OSD and serial menus support preset snap for ZX Spectrum 48K (7.0 MHz) and 128K/+2/+2A/+3 (7.0938 MHz) pixel clocks.
- **Test/Welcome Screen**

### Hardware

- **Analog to Digital Conversion:** Converts analog RGB to digital RGBI.
  - Based on the project:
    [RGBtoHDMI](https://github.com/hoglet67/RGBtoHDMI)

---

## Removed Features

- Z80 CLK external clock source. Self-sync capture mode is now preferred.

---

## Recent Improvements

### Video Output Stability

- DMA IRQ priority set to highest (`PICO_HIGHEST_IRQ_PRIORITY`) in both VGA and DVI drivers.

### Project Structure

- Source reorganized into subfolders: `video/`, `osd/`.

### Performance Improvements

- **Video Output Optimization**: Streamlined DMA handling for both VGA and DVI/HDMI output modes, resulting in more efficient memory usage and cleaner code structure.
- **Buffer Management**: Simplified buffer switching mechanisms for improved video processing performance.

### Code Quality

- **Settings Integrity**: CRC-32 validation on saved settings — corrupted or uninitialized flash data is detected on boot and automatically replaced with safe defaults.
- **Memory Safety**: All video buffer allocations are checked — `watchdog_reboot()` on allocation failure prevents undefined behavior.
- **Dual-Core Synchronization**: Memory barriers (`__dmb()`) on all cross-core flag variables (`stop_core1`, `core1_inactive`, `buf_is_free[]`) ensure correct operation on both RP2040 (Cortex-M0+) and RP2350 (Cortex-M33 with caches).
- **Clean Video Mode Switching**: ISR state variables (`y`, `scr_buffer`, `active_buf_idx`) are reset on `stop_dvi()`/`stop_vga()`, eliminating first-frame glitches after mode changes.
- **Memory Optimization**: Reduced unnecessary memory allocations and pointer complexity in video output modules.
- **Architecture Refinements**: Better separation of concerns between video input capture and output generation systems.
- **Maintainability**: Cleaner code structure while preserving critical hardware-specific requirements for reliable video processing.
