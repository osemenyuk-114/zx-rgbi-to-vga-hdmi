#include "hardware/gpio.h"

#include "g_config.h"
#include "video_output.h"
#include "dvi.h"
#include "v_buf.h"
#include "vga.h"

#ifdef OSD_ENABLE
#include "osd.h"
#endif

extern settings_t settings;
video_out_type_t active_video_output = VIDEO_OUT_TYPE_DEF;

video_mode_t video_mode;
int16_t h_visible_area;
int16_t h_margin;
int16_t v_visible_area;
int16_t v_margin;

video_out_type_t detect_video_output_type()
{
  // VGA DAC per color channel:
  //
  //   D0 --- 820 Ohm ----+
  //                      |
  //   D1 --- 390 Ohm ----+--- 75 Ohm --- GND (monitor termination)
  //
  // Drive D0 (820 Ohm) LOW, read D1 (390 Ohm) with internal pull-up (~50KOhm).
  //   VGA (no monitor):   D0-D1 path = 390 + 820 = 1210 Ohm    -> reads LOW
  //   VGA (with monitor): D1-GND = 390 + (820||75) = ~459 Ohm  -> reads LOW
  //   HDMI/DVI:           >1MOhm isolation, pull-up wins       -> reads HIGH

  const uint pins_out[] = {VGA_PIN_D0, VGA_PIN_D0 + 2, VGA_PIN_D0 + 4};
  const uint pins_in[] = {VGA_PIN_D0 + 1, VGA_PIN_D0 + 3, VGA_PIN_D0 + 5};

  const int num_pairs = 3;

  for (int i = 0; i < num_pairs; i++)
  {
    gpio_init(pins_out[i]);
    gpio_set_dir(pins_out[i], GPIO_OUT);
    gpio_put(pins_out[i], 0);

    gpio_init(pins_in[i]);
    gpio_set_dir(pins_in[i], GPIO_IN);
    gpio_pull_up(pins_in[i]);
  }

  sleep_ms(2);

  int high_count = 0;
  for (int i = 0; i < num_pairs; i++)
  {
    if (gpio_get(pins_in[i]))
      high_count++;
  }

  // clean up: disable pulls, set pins back to inputs
  for (int i = 0; i < num_pairs; i++)
  {
    gpio_disable_pulls(pins_in[i]);
    gpio_disable_pulls(pins_out[i]);
    gpio_set_dir(pins_out[i], GPIO_IN);
    gpio_deinit(pins_out[i]);
    gpio_deinit(pins_in[i]);
  }

  // majority vote: if at least 2 of 3 pairs read HIGH -> HDMI/DVI
  return (high_count >= 2) ? DVI : VGA;
}

void set_video_mode_params(video_mode_t v_mode)
{
  video_mode = v_mode;

  h_visible_area = (uint16_t)(video_mode.h_visible_area / (video_mode.div * 4)) * 2;
  h_margin = (h_visible_area - (uint8_t)(settings.frequency / 1000000) * (ACTIVE_VIDEO_TIME / 2)) / 2;

  if (h_margin < 0)
    h_margin = 0;

  h_visible_area -= h_margin * 2;

  v_visible_area = V_BUF_H * video_mode.div;
  v_margin = ((int16_t)((video_mode.v_visible_area - v_visible_area) / (video_mode.div * 2) + 0.5)) * video_mode.div;

  if (v_margin < 0)
    v_margin = 0;
}

void start_video_output(video_out_type_t output_type)
{
  active_video_output = output_type;

  set_video_mode_params(*(video_modes[settings.video_out_mode]));

#ifdef OSD_ENABLE
  osd_set_position();
#endif

  switch (output_type)
  {
  case DVI:
    start_dvi();
    break;

  case VGA:
    start_vga();
    break;

  default:
    break;
  }
}

void stop_video_output()
{
  switch (active_video_output)
  {
  case DVI:
    stop_dvi();
    break;

  case VGA:
    stop_vga();
    break;

  default:
    break;
  }
}

void set_scanlines_mode()
{
  if (settings.video_out_type == VGA)
    set_vga_scanlines_mode(settings.scanlines_mode);
}

void draw_welcome_screen(video_mode_t video_mode)
{
  int16_t h_visible_area = (uint16_t)(video_mode.h_visible_area / (video_mode.div * 4)) * 4;
  int16_t h_margin = h_visible_area - (uint8_t)(settings.frequency / 1000000) * ACTIVE_VIDEO_TIME;

  if (h_margin < 0)
    h_margin = 0;

  h_visible_area -= h_margin;

  int16_t v_margin = ((int16_t)((video_mode.v_visible_area - V_BUF_H * video_mode.div) / (video_mode.div * 2) + 0.5)) * video_mode.div * 2;

  if (v_margin < 0)
    v_margin = 0;

  uint16_t v_visible_area = video_mode.v_visible_area - v_margin;

  uint8_t *v_buf = (uint8_t *)get_v_buf_out();

  for (int y = 0; y < V_BUF_H; y++)
  {
    uint8_t j = (3 * y * video_mode.div) / v_visible_area;

    for (int x = 0; x < V_BUF_W; x++)
    {
      uint8_t i = ((9 * x) / h_visible_area) + (9 * j);
      uint8_t R = (((i / 3) % 3) != 0) ? ((((i / 3) % 3) == 2) ? 0b00000011 : 0b00000001) : 0b00000000;
      uint8_t G = (((i / 9) % 3) != 0) ? ((((i / 9) % 3) == 2) ? 0b00001100 : 0b00000100) : 0b00000000;
      uint8_t B = ((i % 3) != 0) ? (((i % 3) == 2) ? 0b00110000 : 0b00010000) : 0b00000000;

      *v_buf++ = R | G | B;
    }
  }
}

void draw_welcome_screen_h(video_mode_t video_mode)
{
  int16_t v_margin = ((int16_t)((video_mode.v_visible_area - V_BUF_H * video_mode.div) / (video_mode.div * 2) + 0.5)) * video_mode.div * 2;

  if (v_margin < 0)
    v_margin = 0;

  uint16_t v_visible_area = video_mode.v_visible_area - v_margin;

  uint8_t *v_buf = (uint8_t *)get_v_buf_out();

  for (int y = 0; y < V_BUF_H; y++)
  {
    uint8_t j = (9 * y * video_mode.div) / v_visible_area;

    for (int x = 0; x < V_BUF_W; x++)
    {
      uint8_t i = ((3 * x * video_mode.div) / video_mode.h_visible_area) + (3 * j);
      uint8_t R = (((i / 3) % 3) != 0) ? ((((i / 3) % 3) == 2) ? 0b00000011 : 0b00000001) : 0b00000000;
      uint8_t G = (((i / 9) % 3) != 0) ? ((((i / 9) % 3) == 2) ? 0b00001100 : 0b00000100) : 0b00000000;
      uint8_t B = ((i % 3) != 0) ? (((i % 3) == 2) ? 0b00110000 : 0b00010000) : 0b00000000;

      *v_buf++ = R | G | B;
    }
  }
}

const char nosignal[14][115] = {
    "xx      xx      xxxxxx                  xxxxxx      xxxxxx      xxxxxx      xx      xx        xx        xx",
    "xx      xx     xxxxxxxx                xxxxxxxx     xxxxxx     xxxxxxxx     xx      xx       xxxx       xx",
    "xxx     xx    xxx    xxx              xxx    xxx      xx      xxx    xxx    xxx     xx      xxxxxx      xx",
    "xxx     xx    xx      xx              xx      xx      xx      xx      xx    xxx     xx     xxx  xxx     xx",
    "xxxx    xx    xx      xx              xx              xx      xx            xxxx    xx    xxx    xxx    xx",
    "xxxxx   xx    xx      xx              xxx             xx      xx            xxxxx   xx    xx      xx    xx",
    "xx xxx  xx    xx      xx               xxxxxxx        xx      xx            xx xxx  xx    xx      xx    xx",
    "xx  xxx xx    xx      xx                xxxxxxx       xx      xx    xxxx    xx  xxx xx    xx      xx    xx",
    "xx   xxxxx    xx      xx                     xxx      xx      xx    xxxx    xx   xxxxx    xxxxxxxxxx    xx",
    "xx    xxxx    xx      xx                      xx      xx      xx      xx    xx    xxxx    xxxxxxxxxx    xx",
    "xx     xxx    xx      xx              xx      xx      xx      xx      xx    xx     xxx    xx      xx    xx",
    "xx     xxx    xxx    xxx              xxx    xxx      xx      xxx    xxx    xx     xxx    xx      xx    xx",
    "xx      xx     xxxxxxxx                xxxxxxxx     xxxxxx     xxxxxxxx     xx      xx    xx      xx    xxxxxxxxxx",
    "xx      xx      xxxxxx                  xxxxxx      xxxxxx      xxxxxx      xx      xx    xx      xx    xxxxxxxxxx",
};

void draw_no_signal(video_mode_t video_mode)
{
  int16_t h_visible_area = (uint16_t)(video_mode.h_visible_area / (video_mode.div * 4)) * 4;
  int16_t h_margin = h_visible_area - (uint8_t)(settings.frequency / 1000000) * ACTIVE_VIDEO_TIME;

  if (h_margin < 0)
    h_margin = 0;

  int16_t v_margin = ((int16_t)((video_mode.v_visible_area - V_BUF_H * video_mode.div) / (video_mode.div * 2) + 0.5)) * video_mode.div * 2;

  if (v_margin < 0)
    v_margin = 0;

  uint16_t y = (video_mode.v_visible_area - v_margin) / (video_mode.div * 2);
  uint16_t x = (h_visible_area - h_margin - 114) / 2;

  uint8_t *v_buf = (uint8_t *)get_v_buf_out();

  memset(v_buf, 0, V_BUF_H * V_BUF_W);

  for (int row = 0; row < 14; ++row)
    for (int col = 0; col < 114; ++col)
      v_buf[(y + row) * V_BUF_W + x + col] = (nosignal[row][col] == 'x') ? 0x15 : 0x00;
}