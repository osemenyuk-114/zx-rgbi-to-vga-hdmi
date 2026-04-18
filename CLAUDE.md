# CLAUDE.md — Project Context for AI Assistants

## Project Overview

**zx-rgbi-to-vga-hdmi** — Firmware for a Raspberry Pi Pico (RP2040) that converts ZX Spectrum RGBI video signals to VGA or HDMI (DVI) output.

- Upstream hardware reference: [ZX_RGBI2VGA-HDMI](https://github.com/AlexEkb4ever/ZX_RGBI2VGA-HDMI/)
- Native Pico SDK variant: [zx-rgbi-to-vga-hdmi-PICOSDK](https://github.com/osemenyuk-114/zx-rgbi-to-vga-hdmi-PICOSDK)
- Firmware version: `v1.7.1` (see `ZX_RGBI_TO_VGA_HDMI/g_config.h`)

---

## Repository Structure

```
platformio.ini              # PlatformIO build config (environments, board, flags)
ZX_RGBI_TO_VGA_HDMI/        # Main firmware source
  ZX_RGBI_TO_VGA_HDMI.ino   # Arduino entry point (setup/loop)
  g_config.h                # Global config: board variants, feature flags, pin maps
  settings.h/.c             # Persistent settings (flash, CRC-32 validated)
  rgb_capture.h/.c          # PIO-based RGBI input capture
  video_output.h/.c         # VGA/DVI output coordination
  vga.h/.c                  # VGA signal generation
  dvi.h/.c                  # DVI/HDMI signal generation
  v_buf.h/.c                # Video buffer management
  osd.h/.c                  # On-screen display base layer
  osd_menu.h/.c             # OSD graphical menu system
  ff_osd.h/.c               # FlashFloppy/Gotek I2C OSD support
  serial_menu.h/.cpp        # Serial terminal menu (headless config)
  programs.pio              # PIO assembly programs (capture + output)
  programs.pio.h            # Pre-compiled PIO headers
  pio_programs.h            # PIO program helpers
  font.h                    # OSD font data
  i2c_slave.h/.c            # I2C slave driver (MIT licensed, bundled)
  i2c_fifo.h                # I2C FIFO helpers (bundled; superseded by hardware/i2c.h _raw variants)
docs/
  FF_OSD_GUIDE.md           # FlashFloppy I2C wiring and protocol
  OSD_MENU_GUIDE.md         # OSD button controls and menu tree
  VGA_TIMINGS.md            # Supported VGA/DVI timing tables
hardware/                   # (files committed; subdirectory contents local-only, not committed)
```

---

## Build System

### PlatformIO (preferred)

```ini
# platformio.ini — key values
platform  = raspberrypi (via maxgerhardt/platform-raspberrypi)
board     = pico
framework = arduino  (earlephilhower core, pin 5.5.1)
src_dir   = ZX_RGBI_TO_VGA_HDMI
optimize  = -O3
```

**Environments** (set `default_envs` in `[platformio]`):

| Env        | `OSD_MENU_ENABLE` | `OSD_FF_ENABLE` |
|------------|:-----------------:|:---------------:|
| `osd`      | ✓                 | ✓               |
| `osd_menu` | ✓                 |                 |
| `ff_osd`   |                   | ✓               |
| `no_osd`   |                   |                 |

**Board variants** — uncomment exactly one `-D BOARD_*` flag:
- `BOARD_36LJU22` (default)
- `BOARD_11XGA24`
- `BOARD_LEO_REV3`
- `BOARD_09LJV23`

Build and upload:
```cmd
pio run --target upload
```

### Arduino IDE

- Board: **Raspberry Pi Pico** (earlephilhower package)
- Optimize: `-O3`, USB Stack: `Pico SDK`
- Edit `ZX_RGBI_TO_VGA_HDMI/g_config.h` to set OSD features and board variant inside the `#ifndef PLATFORMIO` block.

---

## Key Concepts

### Feature Flags (compile-time)

| Flag                  | Source                     | Effect                          |
|-----------------------|----------------------------|---------------------------------|
| `OSD_MENU_ENABLE`     | platformio.ini / g_config.h | Enable graphical OSD menu       |
| `OSD_FF_ENABLE`       | platformio.ini / g_config.h | Enable FlashFloppy I2C OSD      |
| `SERIAL_MENU_ENABLE`  | g_config.h (always on)     | Enable serial terminal menu     |
| `VIDEO_OUTPUT_AUTO_DETECT` | g_config.h (board-dependent) | Auto-detect VGA vs DVI at boot |
| `BOARD_*`             | platformio.ini / g_config.h | Selects pin mapping             |

### Settings

- Stored in flash, validated with CRC-32 on boot.
- Corrupted/uninitialized flash is silently replaced with safe defaults.
- API: `load_settings()`, `save_settings()`, `reset_settings_to_defaults()`, `check_settings()`.

### Video Pipeline

1. **Capture** (`rgb_capture.c`) — PIO state machine samples RGBI pins at pixel clock.
2. **Buffer** (`v_buf.c`) — Double-buffered scanline storage.
3. **Output** (`video_output.c`) — Selects VGA or DVI path; DMA-driven scanline output.

### OSD System

- `osd.c` — base layer: draw pixels, characters, copy scanlines into output buffer.
- `osd_menu.c` — full menu tree rendered via OSD; three-button control (UP/DOWN/SEL).
- `ff_osd.c` — I2C slave emulating an HD44780 LCD for FlashFloppy host.

### FF OSD I2C Initialization

`ff_osd_i2c_init()` supports full re-initialization (deinit + reinit):
- Called at boot if FF OSD is enabled, and again whenever `ff_osd_needs_i2c_init` is set.
- `ff_osd_needs_i2c_init = true` is the correct signal for any runtime change requiring re-init:
  enabling FF OSD from a menu, or toggling LCD ↔ FlashFloppy I2C protocol.
- Do **not** call `ff_osd_set_address()` directly for protocol changes — it skips
  `lcd_display_update()` and the full hardware re-init.
- Guard variable `ff_osd_i2c_initialized` is a function-local static inside `ff_osd_i2c_init()`.

---

## Development Notes

- `programs.pio` is the authoritative PIO source. `programs.pio.h` is generated from it.
- Core 0 runs setup/loop (OSD, serial menu, settings). Core 1 runs video capture.
- `stop_core1` / `core1_inactive` volatile flags coordinate safe shutdown of Core 1.
- `BOARD_11XGA24` does **not** support `OSD_FF_ENABLE` (automatically undefined in g_config.h).
- `BOARD_LEO_REV3` adds PS/2 keyboard (`PS2_KBD_ENABLE`) and CH446Q analog switch support.
- The removed Z80 CLK external clock source feature should not be re-introduced; self-sync capture is preferred.
- `ff_osd.c` uses `i2c_read_byte_raw` / `i2c_write_byte_raw` from `hardware/i2c.h` (Pico SDK).
  The bundled `i2c_fifo.h` (`i2c_read_byte` / `i2c_write_byte`) is equivalent but no longer used.

---

## Testing

There is no automated test suite. Validation is hardware-in-the-loop:
1. Flash firmware to Pico.
2. Connect ZX Spectrum RGBI output to board input.
3. Verify VGA/DVI output on a monitor.
4. Exercise OSD menu and serial menu manually.

---

## Recent Changes

### v1.7.1 — FF OSD I2C Re-initialization Refactor

**ff_osd.c:**
- `ff_osd_i2c_init()` now uses function-local `static bool ff_osd_i2c_initialized` guard.
- Supports full re-initialization: on re-call, runs `i2c_slave_deinit()` then complete reinit.
- Uses `i2c_read_byte_raw` / `i2c_write_byte_raw` from `hardware/i2c.h` (Pico SDK).
- The bundled `i2c_fifo.h` variants are no longer used anywhere in the codebase.

**osd_menu.c / serial_menu.cpp:**
- Protocol toggle operations now set `ff_osd_needs_i2c_init = true` instead of calling `ff_osd_set_address()`.
- This flag triggers proper I2C re-initialization in Core 1 `loop1()`.
- Ensures `lcd_display_update()` is called and full hardware re-init occurs.

**Rule:** Always use `ff_osd_needs_i2c_init = true` to trigger re-init. Never call `ff_osd_set_address()` for protocol/enable changes.
