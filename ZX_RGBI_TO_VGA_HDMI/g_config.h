#ifndef G_CONFIG_H
#define G_CONFIG_H

#include <Arduino.h>

#include "pico/platform.h"
#include "pico/stdlib.h"

#include "stdio.h"
#include "inttypes.h"
#include "stdbool.h"

#define FW_VERSION "v1.2.1"

enum cap_sync_mode_t
{
  SYNC_MODE_MIN,
  SELF = SYNC_MODE_MIN,
  EXT,
  SYNC_MODE_MAX = EXT,
};

enum video_out_mode_t
{
  VIDEO_MODE_MIN,
  DVI = VIDEO_MODE_MIN,
  VGA640x480,
  VGA800x600,
  VGA1024x768,
  VGA1280x1024_d3,
  VGA1280x1024_d4,
  VIDEO_MODE_MAX = VGA1280x1024_d4,
};

typedef struct settings_t
{
  enum video_out_mode_t video_out_mode;
  bool scanlines_mode : 1;
  bool x3_buffering_mode : 1;
  bool video_sync_mode : 1;
  enum cap_sync_mode_t cap_sync_mode;
  uint32_t frequency;
  uint8_t ext_clk_divider;
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

extern video_mode_t vga_640x480;
extern video_mode_t vga_800x600;
extern video_mode_t vga_1024x768;
extern video_mode_t vga_1280x1024_d3;
extern video_mode_t vga_1280x1024_d4;

extern video_mode_t *vga_modes[];

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

// video buffer
#define V_BUF_W 448
#define V_BUF_H 306
#define V_BUF_SZ (V_BUF_H * V_BUF_W / 2)

// settings MIN values
#define VIDEO_OUT_MODE_MIN VIDEO_MODE_MIN
#define CAP_SYNC_MODE_MIN SYNC_MODE_MIN
#define FREQUENCY_MIN 6000000
#define EXT_CLK_DIVIDER_MIN 1
#define DELAY_MIN 0
#define shX_MIN 0
#define shY_MIN 0

// settings MAX values
#define VIDEO_OUT_MODE_MAX VIDEO_MODE_MAX
#define CAP_SYNC_MODE_MAX SYNC_MODE_MAX
#define FREQUENCY_MAX 8000000
#define EXT_CLK_DIVIDER_MAX 5
#define DELAY_MAX 31
#define shX_MAX 200
#define shY_MAX 200
#define PIN_INVERSION_MASK 0x7f

#endif
