# zx-rgbi-to-vga-hdmi

A converter for ZX Spectrum RGBI video signals to modern VGA and HDMI displays.

For detailed hardware and original software information, see the upstream project: [ZX_RGBI2VGA-HDMI](https://github.com/AlexEkb4ever/ZX_RGBI2VGA-HDMI/).

---

## Features

### Software

- **Video Output:**
  - VGA output with selectable resolutions: 640x480, 800x600, 1024x768, 1280x1024.
  - HDMI (DVI) output support (fixed resolution 640x480).
  - Optional scanline effect at higher resolutions for a retro look.
  - "NO SIGNAL" message when no input is detected.
- **Configuration via Serial Terminal:**
  - Text-based menu system.
  - Frequency presets for self-synchronizing capture mode (supports ZX Spectrum 48K/128K pixel clocks).
  - Real-time adjustment of all parameters (changes applied immediately).
  - Settings can be saved to flash memory without restart.
- **Test/Welcome Screen:** Styled after the ZX Spectrum 128K.

### Hardware

- **Analog to Digital Conversion:** Converts analog RGB to digital RGBI.
  - Based on the [RGBtoHDMI](https://github.com/hoglet67/RGBtoHDMI) project.

---

## Removed Features

- Z80 CLK external clock source. Self-sync capture mode is now preferred.
