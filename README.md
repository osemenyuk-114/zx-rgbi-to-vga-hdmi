# rgb-to-vga-hdmi

A converter for RGB video signals to modern VGA and HDMI displays.

For detailed hardware and original software information, see the upstream project:  
🔗 [ZX_RGBI2VGA-HDMI](https://github.com/AlexEkb4ever/ZX_RGBI2VGA-HDMI/)

## Features

### Software

- **Video Output:**
  - VGA output with selectable resolutions: 640×480 @60Hz, 800×600 @60Hz, 1280×1024 @60Hz.
  - HDMI (DVI) resolutions: 640×480 @60Hz and 720×576 @50Hz.
  - Optional scanline effect on the VGA output at higher resolutions for a retro look.
  - "NO SIGNAL" message when no input is detected.
- **Configuration via Serial Terminal:**
  - Text-based menu system.
  - Frequency presets for self-synchronizing capture mode (supports ZX Spectrum 48K/128K pixel clocks).
  - Real-time adjustment of all parameters (changes applied immediately).
  - Settings can be saved to flash memory without restart.
- Test/Welcome Screen

### Hardware

- **Analog to Digital Conversion:** Converts Analog RGB to digital RGBI.
  - Based on the project:  
🔗 [RGBtoHDMI](https://github.com/hoglet67/RGBtoHDMI)

---

## Recent Improvements

### Performance Improvements

- **Video Output Optimization**: Streamlined DMA handling for both VGA and DVI/HDMI output modes, resulting in more efficient memory usage and cleaner code structure.
- **Buffer Management**: Simplified buffer switching mechanisms for improved video processing performance.

### Development Experience

- **PlatformIO Integration**: Full PlatformIO support with Arduino framework for easier development and dependency management. The project can also be built using the Arduino IDE for those who prefer a simpler setup (requires: Optimization **-O3**, USB Stack - **Pico SDK**).
- **Enhanced Build System**: Improved VS Code integration with custom build scripts for streamlined development workflow.
- **Better Task Management**: Added comprehensive build, upload, and monitoring tasks with proper error handling.

### Code Quality

- **Memory Optimization**: Reduced unnecessary memory allocations and pointer complexity in video output modules.
- **Architecture Refinements**: Better separation of concerns between video input capture and output generation systems.
- **Maintainability**: Cleaner code structure while preserving critical hardware-specific requirements for reliable video processing.
