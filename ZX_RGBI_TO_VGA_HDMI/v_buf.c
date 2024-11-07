#include "g_config.h"
#include "v_buf.h"

uint8_t *v_bufs[3] = {g_v_buf, g_v_buf + V_BUF_SZ, g_v_buf + 2 * V_BUF_SZ};

bool show_v_buf[] = {false, false, false};

uint8_t v_buf_in_idx = 0;
uint8_t v_buf_out_idx = 0;

bool x3_buffering_mode = false;
bool first_frame = true;

void *__not_in_flash_func(get_v_buf_out)()
{
  if (!x3_buffering_mode | first_frame)
    return v_bufs[0];

  if (!show_v_buf[(v_buf_out_idx + 1) % 3])
  {
    show_v_buf[v_buf_out_idx] = true;
    v_buf_out_idx = (v_buf_out_idx + 1) % 3;
    return v_bufs[v_buf_out_idx];
  }

  if (!show_v_buf[(v_buf_out_idx + 2) % 3])
  {
    show_v_buf[v_buf_out_idx] = true;
    v_buf_out_idx = (v_buf_out_idx + 2) % 3;
    return v_bufs[v_buf_out_idx];
  }

  return v_bufs[v_buf_out_idx];
}

void *__not_in_flash_func(get_v_buf_in)()
{
  if (!x3_buffering_mode)
    return v_bufs[0];

  if (first_frame)
    first_frame = false;

  show_v_buf[v_buf_in_idx] = false;

  if (show_v_buf[(v_buf_in_idx + 1) % 3])
  {
    v_buf_in_idx = (v_buf_in_idx + 1) % 3;
    return v_bufs[v_buf_in_idx];
  }

  if (show_v_buf[(v_buf_in_idx + 2) % 3])
  {
    v_buf_in_idx = (v_buf_in_idx + 2) % 3;
    return v_bufs[v_buf_in_idx];
  }

  return NULL;
}

void set_v_buf_buffering_mode(bool buffering_mode)
{
  x3_buffering_mode = buffering_mode;
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