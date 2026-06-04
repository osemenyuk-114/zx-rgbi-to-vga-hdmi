# CLAUDE.md — Project Context for AI Assistants

## Project Overview

**zx-rgbi-to-vga-hdmi** — Firmware for a Raspberry Pi Pico (RP2040) that converts ZX Spectrum RGBI video signals to VGA or HDMI (DVI) output.

- Upstream hardware reference: [ZX_RGBI2VGA-HDMI](https://github.com/AlexEkb4ever/ZX_RGBI2VGA-HDMI/)
- Native Pico SDK variant: [zx-rgbi-to-vga-hdmi-PICOSDK](https://github.com/osemenyuk-114/zx-rgbi-to-vga-hdmi-PICOSDK)
- Firmware version: `v1.7.2-dev` (see `ZX_RGBI_TO_VGA_HDMI/g_config.h`)

### New in v1.7.2-dev
- PS/2 keyboard support with PIO driver
- ZX Spectrum keyboard emulation via CH446Q analog switch
- Human-editable keyboard mapping table
- Conditional compilation for keyboard features

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
  kbd.h/.c                  # Keyboard dispatcher: merges PS/2+USB state, routes to CH446Q
  kbd_osd.h/.c              # Keyboard↔OSD bridge (Core 1 intercept + Core 0 inject)
  kbd_codes.h               # Universal keyboard codes and 128-bit state management
  kbd_ps2.h/.c              # PIO-based PS/2 keyboard driver (IRQ-driven)
  kbd_usb.h/.c              # USB HID boot keyboard via TinyUSB Host
  kbd_zx.h/.c               # Universal→ZX Spectrum 8×5 matrix mapping
  kbd_ch446q.h/.c           # CH446Q analog switch driver (3-wire serial)
  tusb_config_host.h        # TinyUSB dual-mode config (host when USB_KBD_ENABLE)
  tinyusb_host*.c           # TinyUSB host source wrappers (usbh, hcd, hid)
docs/
  FF_OSD_GUIDE.md           # FlashFloppy I2C wiring and protocol
  OSD_MENU_GUIDE.md         # OSD button controls and menu tree
  VGA_TIMINGS.md            # Supported VGA/DVI timing tables
  PS2_KEYBOARD_README.md    # PS/2 keyboard implementation guide
  PS2_KEYBOARD_MAPPING.md   # Human-readable key mapping table
  PS2_KEYBOARD_BUILD.md     # Build configuration examples
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
| `KBD_ENABLE`          | g_config.h (auto-derived)  | Enable keyboard subsystem (auto from PS2/USB) |
| `PS2_KBD_ENABLE`      | g_config.h (board-dependent) | Enable PS/2 keyboard support   |
| `USB_KBD_ENABLE`      | g_config.h (board-dependent) | Enable USB keyboard support (future) |
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
- `BOARD_38LJE24` and `BOARD_25LEO25` support PS/2 keyboard (`PS2_KBD_ENABLE`) and CH446Q analog switch.
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

### v1.7.3-dev — USB Keyboard, Gotek Keyboard Control, Code Cleanup (June 2026)

**USB HID Keyboard:**
- `kbd_usb.c/h` — TinyUSB Host boot keyboard driver
- `tuh_init(0)` for host mode (NOT `tusb_init()` which is device-mode from libpico.a)
- `hid_to_universal[]` lookup table for O(1) HID→universal mapping
- PS/2 + USB states OR-merged in `kbd_on_event()`
- Single keyboard support (CFG_TUH_DEVICE_MAX=1)

**Gotek Keyboard Control (ScrLk):**
- `ff_osd_kbd_active` flag — independent of `ff_osd_display.on` (which is overwritten by I2C)
- ScrLk toggles `ff_osd_kbd_active` via `osd_gotek_request`
- When active: arrows→LEFT/RIGHT, Enter→SELECT sent to Gotek
- Keyboard buttons read directly from `osd_virtual_held` (bypasses double osd_buttons_update)
- Second ScrLk press deactivates and releases keyboard

**kbd_osd.c — Keyboard↔OSD Bridge:**
- Core 1 (`kbd_osd_intercept`): hotkeys + virtual button generation
- Core 0 (`kbd_osd_apply_virtual`): injects SEL into osd_buttons, preserves BACK for osd_menu
- ESC key: exits tuning → submenu → main menu → closes menu
- Keyboard intercept condition: `menu_active || ff_osd_kbd_active` (NOT ff_osd_display.on)
- Controlled repeat for arrows: 400ms delay, 80ms rate

**Video Output Stability (DMA IRQ Priority):**
- `irq_set_priority(DMA_IRQ_0, PICO_HIGHEST_IRQ_PRIORITY)` in both `vga.c` and `dvi.c`
- Prevents USB Host ISR (USBCTRL_IRQ) from blocking video DMA handler on Core 0
- `kbd_usb_task()` throttled to 500µs interval (was every loop iteration)

**Gotek Keyboard Visual Indicator:**
- FF OSD text color changes to Cyan (0x3) when `ff_osd_kbd_active` (keyboard controls Gotek)
- Returns to white (7) when keyboard control is deactivated

**Code Cleanup:**
- Removed unused `kbd_output_apply_fn` typedef + function pointer indirection
- Removed unused `kbd_get_state()`, `clear_kb_state()`
- Removed duplicate includes, simplified release logic
- OSD timing reduced 20%: debounce 200ms, repeat delay 400ms, rate 80ms

### v1.7.2-dev — PS/2 Keyboard Support (April 2026)

**New Modules (C + Pico SDK):**
- `kbd.c/h` — Keyboard dispatcher: merges PS/2+USB, routes to CH446Q via delta-apply.
- `kbd_codes.h` — Universal keyboard codes and 128-bit state management.
- `kbd_ps2.c/h` — PIO-based PS/2 keyboard driver (IRQ-driven scancode decoder).
- `kbd_zx.c/h` — Universal→ZX Spectrum 8×5 matrix mapping with LUT.
- `kbd_ch446q.c/h` — CH446Q analog switch driver (3-wire serial interface).

**kbd.c Architecture:**
```
PS/2 IRQ / USB callback  →  kbd_on_event()  →  OR-merge  →  kbd_osd_intercept  →  CH446Q delta-apply
```
- `KBD_ENABLE` meta-flag auto-derived from `PS2_KBD_ENABLE || USB_KBD_ENABLE`.
- `kbd_osd_active` suppresses ZX output when OSD is driving.

**Design Principles:**
- Pure C implementation (no C++/Arduino libraries).
- PIO + IRQ for zero-overhead PS/2 scancode reception.
- Modular architecture with universal keyboard state layer.
- Based on RGB_TO_VGA_HDMI_25 reference project.

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
