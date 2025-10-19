#pragma once

#include <Arduino.h>

#include "pico.h"
#include "pico/time.h"

#ifndef FW_VERSION
#define FW_VERSION "v1.5.0"
#endif

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
  MODE_1024x768_60Hz,
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

typedef struct settings_t
{
  video_out_type_t video_out_type;
  video_out_mode_t video_out_mode;
  bool scanlines_mode : 1;
  bool buffering_mode : 1;
  bool video_sync_mode : 1;
  cap_sync_mode_t cap_sync_mode;
  uint32_t frequency;
  int8_t ext_clk_divider;
  int8_t delay;
  int16_t shX;
  int16_t shY;
  uint8_t pin_inversion_mask;
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
extern video_mode_t mode_1024x768_60Hz;
extern video_mode_t mode_1280x1024_60Hz_d3;
extern video_mode_t mode_1280x1024_60Hz_d4;

extern video_mode_t *video_modes[];

extern uint8_t g_v_buf[];
extern uint32_t frame_count;

#define BOARD_CODE_36LJU22
// #define BOARD_CODE_09LJV23

// board pin configurations
#ifdef BOARD_CODE_36LJU22
// 36LJU22
// first VGA pin
#define VGA_PIN_D0 8
// DVI pins and settings
#define DVI_PIN_D0 VGA_PIN_D0
#define DVI_PIN_CLK0 (DVI_PIN_D0 + 6)

#elif defined(BOARD_CODE_09LJV23)
// 09LJV23
// first VGA pin
#define VGA_PIN_D0 7
// DVI pins and settings
#define DVI_PIN_D0 VGA_PIN_D0
#define DVI_PIN_CLK0 (DVI_PIN_D0 + 6)

#else
// defaults
// first VGA pin
#define VGA_PIN_D0 7
// DVI pins and settings
#define DVI_PIN_D0 VGA_PIN_D0
#define DVI_PIN_CLK0 (DVI_PIN_D0 + 6)

#endif

// capture pins
#ifndef CAP_PIN_D0
#define CAP_PIN_D0 0
#endif

#ifndef HS_PIN
#define HS_PIN (CAP_PIN_D0 + 4)
#endif

#ifndef VS_PIN
#define VS_PIN (CAP_PIN_D0 + 5)
#endif

#ifndef F_PIN
#define F_PIN (CAP_PIN_D0 + 6)
#endif

// DVI pins and settings
#ifndef DVI_PIN_invert_diffpairs
#define DVI_PIN_invert_diffpairs 0
#endif

#ifndef DVI_PIN_RGB_notBGR
#define DVI_PIN_RGB_notBGR 0
#endif

// PIO and SM for VGA
#define PIO_VGA_NUM 0
#if PIO_VGA_NUM == 0
#define PIO_VGA pio0
#define DREQ_PIO_VGA DREQ_PIO0_TX0
#else
#define PIO_VGA pio1
#define DREQ_PIO_VGA DREQ_PIO1_TX0
#endif

#define SM_VGA 0

// PIO and SM for DVI
#define PIO_DVI_NUM 0
#if PIO_DVI_NUM == 0
#define PIO_DVI pio0
#define DREQ_PIO_DVI DREQ_PIO0_TX0
#else
#define PIO_DVI pio1
#define DREQ_PIO_DVI DREQ_PIO0_TX0
#endif

#define SM_DVI 0

// capture PIO and SM
#define PIO_CAP_NUM 1
#if PIO_CAP_NUM == 0
#define PIO_CAP pio0
#define DREQ_PIO_CAP DREQ_PIO0_RX0
#else
#define PIO_CAP pio1
#define DREQ_PIO_CAP DREQ_PIO1_RX0
#endif

#define SM_CAP 0

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

// video buffer
#define V_BUF_W ((64 - 6) * (FREQUENCY_MAX / 1000000)) // width of the video buffer calculate as max captured line length in pixels (64 µs - whole scanline time, 6 µs - front porch + horizontal sync pulse durations)
#define V_BUF_H 304
#define V_BUF_SZ (V_BUF_H * V_BUF_W / 2)

// video timing
#define ACTIVE_VIDEO_TIME (64 - 12) // active video time in µs (64 µs - whole scanline time, 12 µs - front porch + horizontal sync pulse durations + back porch durations)

// enable scanlines on 640x480 and 800x600 resolutions
// not enabled due to reduced image brightness and uneven line thickness caused by monitor scaler
// #define LOW_RES_SCANLINE

// select scanline thickness for the 1280x1024 video mode
// narrow - show scanline once every four lines
// wide   - show scanline twice in four lines
#define NARROW_SCANLINE
