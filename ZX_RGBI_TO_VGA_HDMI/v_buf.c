#include "g_config.h"
#include "v_buf.h"

uint8_t *v_bufs = g_v_buf;

void *__not_in_flash_func(get_v_buf_out)()
{
  return v_bufs;
}

void *__not_in_flash_func(get_v_buf_in)()
{
  return v_bufs;
}

void draw_welcome_screen(video_mode_t video_mode)
{
  uint8_t *v_buf = (uint8_t *)get_v_buf_out();
  int16_t v_margin = (int16_t)((video_mode.v_visible_area - V_BUF_H * video_mode.div) / video_mode.div) * video_mode.div;

  if (v_margin < 0)
    v_margin = 0;

  uint v_area = video_mode.v_visible_area - v_margin;

  for (int y = 0; y < V_BUF_H; y++)
  {
    uint8_t j = (3 * y * video_mode.div) / v_area;

    for (int x = 0; x < V_BUF_W; x++)
    {
      uint8_t i = ((9 * x * video_mode.div) / video_mode.h_visible_area) + (9 * j);
      uint8_t R = (((i / 3) % 3) != 0) ? ((((i / 3) % 3) == 2) ? 0b00001100 : 0b00000100) : 0b00000000;
      uint8_t G = (((i / 9) % 3) != 0) ? ((((i / 9) % 3) == 2) ? 0b00010010 : 0b00000010) : 0b00000000;
      uint8_t B = ((i % 3) != 0) ? (((i % 3) == 2) ? 0b00100001 : 0b00000001) : 0b00000000;

      *v_buf++ = R | G | B;
    }
  }
}

void draw_welcome_screen_h(video_mode_t video_mode)
{
  uint8_t *v_buf = (uint8_t *)get_v_buf_out();
  int16_t v_margin = (int16_t)((video_mode.v_visible_area - V_BUF_H * video_mode.div) / video_mode.div) * video_mode.div;

  if (v_margin < 0)
    v_margin = 0;

  uint v_area = video_mode.v_visible_area - v_margin;

  for (int y = 0; y < V_BUF_H; y++)
  {
    uint8_t j = (9 * y * video_mode.div) / v_area;

    for (int x = 0; x < V_BUF_W; x++)
    {
      uint8_t i = ((3 * x * video_mode.div) / video_mode.h_visible_area) + (3 * j);
      uint8_t R = (((i / 3) % 3) != 0) ? ((((i / 3) % 3) == 2) ? 0b00001100 : 0b00000100) : 0b00000000;
      uint8_t G = (((i / 9) % 3) != 0) ? ((((i / 9) % 3) == 2) ? 0b00010010 : 0b00000010) : 0b00000000;
      uint8_t B = ((i % 3) != 0) ? (((i % 3) == 2) ? 0b00100001 : 0b00000001) : 0b00000000;

      *v_buf++ = R | G | B;
    }
  }
}