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

  for (int y = 0; y < V_BUF_H; y++)
    for (int x = 0; x < V_BUF_W; x++)
    {
      uint8_t i = 15 - ((16 * x * video_mode.div) / video_mode.h_visible_area);
      uint8_t c = (!(i & 1) << 3) | ((i >> 2) & 2) | (i & 4) | ((i >> 1) & 1);

      if (x & 1)
        *v_buf++ |= (c << 4) & 0xf0;
      else
        *v_buf = c & 0x0f;
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

    uint8_t i = (16 * y * video_mode.div) / v_area;
    uint8_t c = ((i & 1) << 3) | ((i >> 2) & 2) | (i & 4) | ((i >> 1) & 1);

    for (int x = 0; x < V_BUF_W / 2; x++)
    {
      c |= c << 4;
      *v_buf++ = c;
    }
  }
}