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
      uint8_t R = (((i / 3) % 3) != 0) ? ((((i / 3) % 3) == 2) ? 0b00000011 : 0b00000001) : 0b00000000;
      uint8_t G = (((i / 9) % 3) != 0) ? ((((i / 9) % 3) == 2) ? 0b00001100 : 0b00000100) : 0b00000000;
      uint8_t B = ((i % 3) != 0) ? (((i % 3) == 2) ? 0b00110000 : 0b00010000) : 0b00000000;

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
      uint8_t R = (((i / 3) % 3) != 0) ? ((((i / 3) % 3) == 2) ? 0b00000011 : 0b00000001) : 0b00000000;
      uint8_t G = (((i / 9) % 3) != 0) ? ((((i / 9) % 3) == 2) ? 0b00001100 : 0b00000100) : 0b00000000;
      uint8_t B = ((i % 3) != 0) ? (((i % 3) == 2) ? 0b00110000 : 0b00010000) : 0b00000000;

      *v_buf++ = R | G | B;
    }
  }
}

const char nosignal[14][114] = {
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
    "xx      xx      xxxxxx                  xxxxxx      xxxxxx      xxxxxx      xx      xx    xx      xx    xxxxxxxxxx"};

void draw_no_signal(video_mode_t video_mode)
{
  uint8_t *v_buf = (uint8_t *)get_v_buf_out();
  int16_t v_margin = (int16_t)((video_mode.v_visible_area - V_BUF_H * video_mode.div) / video_mode.div) * video_mode.div;

  if (v_margin < 0)
    v_margin = 0;

  uint y = (video_mode.v_visible_area - v_margin) / (video_mode.div * 2);
  uint x = (V_BUF_W - 114) / 4;

  memset(v_buf, 0, V_BUF_H * V_BUF_W);

  for (int row = 0; row < 14; ++row)
    for (int col = 0; col < 114; ++col)
      v_buf[(y + row) * V_BUF_W + x + col] = (nosignal[row][col] == 'x') ? 0x15 : 0x00;
}
