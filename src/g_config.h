#pragma once

#include <Arduino.h>

#include "pico.h"
#include "pico/time.h"

#define FW_VER "v1.8.0"

#if PICO_RP2350
#define FW_VERSION FW_VER " (PICO 2)"
#else
#define FW_VERSION FW_VER
#endif

#define GIT_REPO_URL_1 "https://github.com/"
#define GIT_REPO_URL_2 "osemenyuk-114/"
#define GIT_REPO_URL_3 "zx-rgbi-to-vga-hdmi"

#if defined(BOARD_36LJU22) || defined(BOARD_RP2040_ZERO)

#define VIDEO_OUTPUT_AUTO_DETECT

#ifdef BOARD_36LJU22

#define HW_VERSION "36LJU22"
#define I2C_PIN_SDA 16
#define I2C_PIN_SCL 17

#else

#define HW_VERSION "RP2040 Zero"
#define WS2812_LED_ENABLE
#define I2C_PIN_SDA 20
#define I2C_PIN_SCL 21

#endif

#define DVI_PIN_D0 8
#define DVI_PIN_CLK0 (DVI_PIN_D0 + 6)
#define VGA_PIN_D0 DVI_PIN_D0
#define CAP_PIN_D0 0
#define OSD_BTN_UP 26
#define OSD_BTN_DOWN 27
#define OSD_BTN_SEL 28

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
#define PS2_PIN_DATA 1
#define PS2_PIN_CLK 0
#define KBD_PIN_DATA 2
#define KBD_PIN_CLK 3
#define KBD_PIN_STB 4

#elif defined(BOARD_11XGA24_1) || defined(BOARD_11XGA24_2)

#define HW_VERSION "11XGA24"
#define DVI_PINS_REVERSED // DVI pins are in reverse order (D0 is the last pin, D5 is the first)
#define VGA_PINS_SWAPPED  // VGA R and B pins are swapped

#ifdef BOARD_11XGA24_1

#define DVI_PIN_D0 2
#define DVI_PIN_CLK0 0
#define VGA_PIN_D0 8

#else

#define DVI_PIN_D0 10
#define DVI_PIN_CLK0 8
#define VGA_PIN_D0 0

#endif

#define CAP_PIN_D0 16
#define OSD_BTN_UP 26
#define OSD_BTN_DOWN 27
#define OSD_BTN_SEL 28

#elif defined(BOARD_LEOV3) || defined(BOARD_LEOV3_2040BT)

#define VIDEO_OUTPUT_AUTO_DETECT
#define SPI_KB_ENABLE

#ifdef BOARD_LEOV3
#define HW_VERSION "LEO v3.0"
#define WS2812_LED_ENABLE
#define OSD_BTN_UP 19
#define OSD_BTN_DOWN 18
#define OSD_BTN_SEL 17
#define I2C_PIN_SDA 20
#define I2C_PIN_SCL 21

#else

#define HW_VERSION "LEO v3.0.2040BT"
#define OSD_BTN_UP 20
#define OSD_BTN_DOWN 21
#define OSD_BTN_SEL 22
#define I2C_PIN_SDA 16
#define I2C_PIN_SCL 17

#endif

#define DVI_PIN_D0 8
#define DVI_PIN_CLK0 (DVI_PIN_D0 + 6)
#define VGA_PIN_D0 DVI_PIN_D0
#define CAP_PIN_D0 0
#define PS2_PIN_DATA 29
#define PS2_PIN_CLK 28
#define KBD_PIN_DATA 7
#define KBD_PIN_CLK 26
#define KBD_PIN_STB 27

#elif defined(BOARD_09LJV23)

#define HW_VERSION "09LJV23"
#define VIDEO_OUTPUT_AUTO_DETECT
#define DVI_PIN_D0 7
#define DVI_PIN_CLK0 (DVI_PIN_D0 + 6)
#define VGA_PIN_D0 DVI_PIN_D0
#define CAP_PIN_D0 0
#define OSD_BTN_UP 26
#define OSD_BTN_DOWN 27
#define OSD_BTN_SEL 28
#define I2C_PIN_SDA 16
#define I2C_PIN_SCL 17

#else

#error "Unsupported board variant. Please define the board variant in CMakeLists.txt."

#endif

// capture pins bit positions
#define CAP_B 0
#define CAP_G 1
#define CAP_R 2
#define CAP_I 3
#define CAP_HS 4
#define CAP_VS 5
#define CAP_F 6

// capture GPIO pin numbers
#define CAP_HS_PIN (CAP_PIN_D0 + CAP_HS)
#define CAP_VS_PIN (CAP_PIN_D0 + CAP_VS)
#define CAP_F_PIN (CAP_PIN_D0 + CAP_F)

// PIO and SM for DVI
#define PIO_DVI pio0
#define DREQ_PIO_DVI DREQ_PIO0_TX0
#define SM_DVI 0
#define SM_DVI_CONV (SM_DVI + 1)

// PIO and SM for VGA
#define PIO_VGA pio0
#define DREQ_PIO_VGA DREQ_PIO0_TX0
#define SM_VGA 0

// capture PIO and SM
#define PIO_CAP pio1
#define DREQ_PIO_CAP DREQ_PIO1_RX0
#define SM_CAP 0

// I2C instance
#define I2C_INST i2c0

#define PIO_PS2 pio1
// Use IRQ index 1 (PIO1_IRQ_1) to avoid conflicts with capture SM
#define PIO_PS2_IRQ PIO1_IRQ_1
#define SM_PS2 1

#define PIO_CH446Q pio1
#define SM_CH446Q 2

#define PIO_SPI pio1
#define SM_SPI 2

// PIO and SM for WS2812B RGB LED (on PIO0 to avoid PIO1 memory pressure)
#define PIO_WS2812 pio0
#define SM_WS2812 2

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

// Keyboard subsystem meta-flag: enabled if any keyboard backend is enabled
#if defined(PS2_KBD_ENABLE) || defined(USB_KBD_ENABLE)
#define KBD_ENABLE
#endif