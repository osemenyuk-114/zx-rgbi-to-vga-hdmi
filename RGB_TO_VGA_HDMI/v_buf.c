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

void clear_video_buffers()
{
  // clear video buffer
  memset(g_v_buf, 0, V_BUF_SZ);
}