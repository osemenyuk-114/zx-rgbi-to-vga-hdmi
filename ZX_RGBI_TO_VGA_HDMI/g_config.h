#pragma once

#include <Arduino.h>

#include "pico.h"
#include "pico/time.h"

// FW_VERSION can be overridden at build time via -DFW_VERSION="..."
#ifndef FW_VERSION
#define FW_VERSION "v1.7.1"
#endif

#define GIT_REPO_URL_1 "https://github.com/"
#define GIT_REPO_URL_2 "osemenyuk-114/"
#define GIT_REPO_URL_3 "zx-rgbi-to-vga-hdmi"

// For PlatformIO builds, OSD features are controlled via build_flags in platformio.ini.
// For Arduino IDE builds, enable/disable by commenting/uncommenting below.
#ifndef PLATFORMIO
// Serial menu is enabled by default
#define SERIAL_MENU_ENABLE
// OSD features — enable or disable as needed:
#define OSD_MENU_ENABLE
#define OSD_FF_ENABLE

// Board variant — uncomment exactly one:
#define BOARD_36LJU22
// #define BOARD_38LJE24
// #define BOARD_11XGA24
// #define BOARD_25LEO25
// #define BOARD_09LJV23
#endif

// board pin configurations
#ifdef BOARD_36LJU22
#define HW_VERSION "36LJU22"
#define VIDEO_OUTPUT_AUTO_DETECT
#define DVI_PIN_D0 8
#define DVI_PIN_CLK0 (DVI_PIN_D0 + 6)
#define VGA_PIN_D0 DVI_PIN_D0
#define CAP_PIN_D0 0
#define OSD_BTN_UP 26
#define OSD_BTN_DOWN 27
#define OSD_BTN_SEL 28
#define I2C_PIN_SDA 20
#define I2C_PIN_SCL 21
#define I2C_INST i2c0
#elif defined(BOARD_38LJE24)
#define HW_VERSION "38LJE24"
#define VIDEO_OUTPUT_AUTO_DETECT
#define DVI_PINS_REVERSED // DVI pins are in reverse order (D0 is the last pin, D5 is the first)
#define DVI_PIN_D0 14
#define DVI_PIN_CLK0 12
#define VGA_PINS_SWAPPED // VGA R and B pins are swapped
#define VGA_PIN_D0 DVI_PIN_CLK0
#define CAP_PIN_D0 5
#define OSD_BTN_UP 26
#define OSD_BTN_DOWN 27
#define OSD_BTN_SEL 28
#define I2C_PIN_SDA 20
#define I2C_PIN_SCL 21
#define I2C_INST i2c0
#define PS2_KBD_ENABLE
#define PS2_PIN_DATA 1
#define PS2_PIN_CLK 0
#define CH446Q_PIN_DATA 2
#define CH446Q_PIN_CLK 3
#define CH446Q_PIN_STB 4
#elif defined(BOARD_11XGA24)
#define HW_VERSION "11XGA24"
#define DVI_PINS_REVERSED // DVI pins are in reverse order (D0 is the last pin, D5 is the first)
#define DVI_PIN_D0 2
#define DVI_PIN_CLK0 0
#define VGA_PINS_SWAPPED // VGA R and B pins are swapped
#define VGA_PIN_D0 8
#define CAP_PIN_D0 16
#define OSD_BTN_UP 26
#define OSD_BTN_DOWN 27
#define OSD_BTN_SEL 28
#if defined(OSD_FF_ENABLE)
#undef OSD_FF_ENABLE
#endif
#elif defined(BOARD_25LEO25)
#define HW_VERSION "25LEO25"
#define VIDEO_OUTPUT_AUTO_DETECT
#define DVI_PIN_D0 8
#define DVI_PIN_CLK0 (DVI_PIN_D0 + 6)
#define VGA_PIN_D0 DVI_PIN_D0
#define CAP_PIN_D0 0
#define OSD_BTN_UP 19
#define OSD_BTN_DOWN 18
#define OSD_BTN_SEL 17
#define I2C_PIN_SDA 20
#define I2C_PIN_SCL 21
#define I2C_INST i2c0
#define PS2_KBD_ENABLE
#define PS2_PIN_DATA 29
#define PS2_PIN_CLK 28
#define CH446Q_PIN_DATA 7
#define CH446Q_PIN_CLK 26
#define CH446Q_PIN_STB 27
#else /* 09LJV23 */
#define HW_VERSION "09LJV23"
#define VIDEO_OUTPUT_AUTO_DETECT
#define DVI_PIN_D0 7
#define DVI_PIN_CLK0 (DVI_PIN_D0 + 6)
#define VGA_PIN_D0 DVI_PIN_D0
#define CAP_PIN_D0 0
#define OSD_BTN_UP 26
#define OSD_BTN_DOWN 27
#define OSD_BTN_SEL 28
#define I2C_PIN_SDA 20
#define I2C_PIN_SCL 21
#define I2C_INST i2c0
#endif

// capture pins
#define B_PIN CAP_PIN_D0
#define G_PIN (CAP_PIN_D0 + 1)
#define R_PIN (CAP_PIN_D0 + 2)
#define I_PIN (CAP_PIN_D0 + 3)
#define HS_PIN (CAP_PIN_D0 + 4)
#define VS_PIN (CAP_PIN_D0 + 5)
#define F_PIN (CAP_PIN_D0 + 6)

// PIO and SM for DVI
#define PIO_DVI pio0
#define DREQ_PIO_DVI DREQ_PIO0_TX0
#define SM_DVI 0

// PIO and SM for VGA
#define PIO_VGA pio0
#define DREQ_PIO_VGA DREQ_PIO0_TX0
#define SM_VGA 0

// capture PIO and SM
#define PIO_CAP pio1
#define DREQ_PIO_CAP DREQ_PIO1_RX0
#define SM_CAP 0

typedef enum video_out_type_t
{
  OUTPUT_TYPE_MIN,
  DVI = OUTPUT_TYPE_MIN,
  VGA,
  OUTPUT_TYPE_MAX = VGA,
} video_out_type_t;

typedef enum video_out_mode_t
{
  VIDEO_MODE_MIN,
  MODE_640x480_60Hz = VIDEO_MODE_MIN,
  MODE_720x576_50Hz,
  VIDEO_MODE_DVI_MAX = MODE_720x576_50Hz,
  MODE_800x600_60Hz,
  MODE_1024x768_60Hz_d3,
  MODE_1024x768_60Hz_d4,
  MODE_1280x1024_60Hz_d3,
  MODE_1280x1024_60Hz_d4,
  VIDEO_MODE_MAX = MODE_1280x1024_60Hz_d4,
} video_out_mode_t;

typedef enum cap_sync_mode_t
{
  SYNC_MODE_MIN,
  SELF = SYNC_MODE_MIN,
  EXT,
  SYNC_MODE_MAX = EXT,
} cap_sync_mode_t;

#ifdef OSD_FF_ENABLE
typedef struct ff_osd_config_t
{
  bool enabled;
  bool i2c_protocol; // false = LCD_HD44780, true = FlashFloppy
  uint16_t cols;
  uint16_t rows;
  uint8_t h_position; // 1=left, 2=left-center, 3=center, 4=center-right, 5=right
  bool v_position;    // false = at the top, true = at the bottom of the screen
} ff_osd_config_t;

#define FF_OSD_COLUMNS_MIN 16
#define FF_OSD_COLUMNS_MAX 40
#define FF_OSD_ROWS_MIN 2
#define FF_OSD_ROWS_MAX 4

#endif

typedef struct settings_t
{
  video_out_type_t video_out_type;
  video_out_mode_t video_out_mode;
  bool scanlines_mode;
  bool buffering_mode;
  bool video_sync_mode;
  cap_sync_mode_t cap_sync_mode;
  uint32_t frequency;
  int8_t ext_clk_divider;
  int8_t delay;
  int16_t shX;
  int16_t shY;
  uint8_t pin_inversion_mask;
#ifdef OSD_FF_ENABLE
  ff_osd_config_t ff_osd_config;
#endif
  uint32_t crc;
} settings_t;

typedef struct video_mode_t
{
  uint32_t sys_freq;
  float pixel_freq;
  uint16_t h_visible_area;
  uint16_t v_visible_area;
  uint16_t whole_line;
  uint16_t whole_frame;
  uint8_t h_front_porch;
  uint8_t h_sync_pulse;
  uint8_t h_back_porch;
  uint8_t v_front_porch;
  uint8_t v_sync_pulse;
  uint8_t v_back_porch;
  uint8_t sync_polarity;
  uint8_t div;
} video_mode_t;

extern video_mode_t mode_640x480_60Hz;
extern video_mode_t mode_720x576_50Hz;
extern video_mode_t mode_800x600_60Hz;
extern video_mode_t mode_1024x768_60Hz_d3;
extern video_mode_t mode_1024x768_60Hz_d4;
extern video_mode_t mode_1280x1024_60Hz_d3;
extern video_mode_t mode_1280x1024_60Hz_d4;

extern video_mode_t *video_modes[];

extern uint8_t g_v_buf[];

// settings MIN values
#define VIDEO_OUT_TYPE_MIN OUTPUT_TYPE_MIN
#define VIDEO_OUT_MODE_MIN VIDEO_MODE_MIN
#define CAP_SYNC_MODE_MIN SYNC_MODE_MIN
#define FREQUENCY_MIN 6000000
#define EXT_CLK_DIVIDER_MIN 1
#define DELAY_MIN 0
#define shX_MIN 0
#define shY_MIN 0

// settings MAX values
#define VIDEO_OUT_TYPE_MAX OUTPUT_TYPE_MAX
#define VIDEO_OUT_MODE_MAX VIDEO_MODE_MAX
#define CAP_SYNC_MODE_MAX SYNC_MODE_MAX
#define FREQUENCY_MAX 8000000
#define EXT_CLK_DIVIDER_MAX 5
#define DELAY_MAX 31
#define shX_MAX 200
#define shY_MAX 200
#define PIN_INVERSION_MASK 0x7f

// settings DEFAULT values
#define VIDEO_OUT_TYPE_DEF VGA
#define VIDEO_OUT_MODE_DEF MODE_640x480_60Hz
#define CAP_SYNC_MODE_DEF SELF
#define FREQUENCY_DEF 7000000
#define EXT_CLK_DIVIDER_DEF 2
#define DELAY_DEF 15
#define shX_DEF 68
#define shY_DEF 34
#define PIN_INVERSION_MASK_DEF 0x00

// video timing
// 64 us - duration of a single scanline, 12 us - combined duration of the front porch, horizontal sync pulse, and back porch
#define ACTIVE_VIDEO_TIME (64 - 12)

// video buffer
// width of the video buffer is calculated as max captured line length in pixels
#define V_BUF_W (ACTIVE_VIDEO_TIME * (FREQUENCY_MAX / 1000000))
#define V_BUF_H 304
#define V_BUF_SZ (V_BUF_H * V_BUF_W / 2)

// enable scanlines on 640x480 and 800x600 resolutions
// not enabled due to reduced image brightness and uneven line thickness caused by monitor scaler
// #define SCANLINES_ENABLE_LOW_RES

// select scanline thickness for the 1024x768 and 1280x1024 DIV4 video modes
// thin - show scanline once every four lines
// thick - show scanline twice in four lines
#define SCANLINES_USE_THIN

#if defined(OSD_MENU_ENABLE) || defined(OSD_FF_ENABLE)
#define OSD_ENABLE
#endif