
# zx-rgbi-to-vga-hdmi

For detailed hardware and original software information, please refer to the source: [ZX_RGBI2VGA-HDMI](https://github.com/AlexEkb4ever/ZX_RGBI2VGA-HDMI/).

## Changes and New Features

### Software

- **Video Output:**
  - Added new resolutions on VGA output (800x600, 1024x768, 1280x1024).
  - Introduced scanline effect at higher resolutions.
- **Configuration via Serial Terminal:**
  - Text-based menus.
  - Frequency presets for self-synchronizing capture mode (supports ZX Spectrum 48K/128K timings).
  - Real-time settings adjustments for capture delay, image position, scanline effects, and buffering modes.
- **PIO Clock Divider Optimization:** Enhanced precision in self-synchronizing capture mode.
- **Test/Welcome Screen:** Styled like the ZX Spectrum 128K.

### Hardware

- **Analog to Digital Conversion:** Converts Analog RGB to digital RGBI.
  - Based on the [RGBtoHDMI](https://github.com/hoglet67/RGBtoHDMI) project.

### Removed Features

- Z80 CLK external clock source.
- Separate horizontal and vertical sync inputs. (Now only composite sync is supported.)
