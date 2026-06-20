#include "g_config.h"
#include "v_buf.h"
#include "hardware/sync.h"

extern settings_t settings;

uint8_t *v_bufs[3] = {
    g_v_buf,
    g_v_buf + V_BUF_SZ,
    g_v_buf + (2 * V_BUF_SZ),
};

// Triple buffering state
// buf_is_free[i]: true = buffer available for capture, false = buffer ready for display
volatile bool buf_is_free[] = {false, true, true};

volatile uint8_t v_buf_in_idx = 0;  // Buffer index for capture (written in ISR)
volatile uint8_t v_buf_out_idx = 0; // Buffer index for display

bool buffering_mode = false;
bool first_frame = true;

// Optimized index increment for triple buffer (replaces expensive modulo)
static inline uint8_t next_buf_idx(uint8_t idx)
{
  return (idx == 2) ? 0 : idx + 1;
}

void *__not_in_flash_func(get_v_buf_out)()
{
  if (!buffering_mode || first_frame)
    return v_bufs[0];

  // Try next buffer
  uint8_t next = next_buf_idx(v_buf_out_idx);

  __dmb();

  if (!buf_is_free[next]) // Buffer has fresh data
  {
    buf_is_free[v_buf_out_idx] = true; // Mark current as free for capture

    __dmb();

    v_buf_out_idx = next;
    return v_bufs[next];
  }

  // Try buffer after next
  next = next_buf_idx(next);
  if (!buf_is_free[next]) // Buffer has fresh data
  {
    buf_is_free[v_buf_out_idx] = true; // Mark current as free for capture

    __dmb();

    v_buf_out_idx = next;
    return v_bufs[next];
  }

  // No new buffer available, keep current
  return v_bufs[v_buf_out_idx];
}

void *__not_in_flash_func(get_v_buf_in)()
{
  if (!buffering_mode)
    return v_bufs[0];

  first_frame = false;

  __dmb();

  buf_is_free[v_buf_in_idx] = false; // Mark current buffer as ready for display

  __dmb();

  // Find next free buffer for capture
  uint8_t next = next_buf_idx(v_buf_in_idx);
  if (buf_is_free[next]) // Buffer is free
  {
    v_buf_in_idx = next;
    return v_bufs[next];
  }

  // Try buffer after next
  next = next_buf_idx(next);
  if (buf_is_free[next]) // Buffer is free
  {
    v_buf_in_idx = next;
    return v_bufs[next];
  }

  // No free buffer available (display hasn't consumed any buffers yet)
  // This is normal during heavy load - capture will skip this frame
  return NULL;
}

void set_buffering_mode(bool buf_mode)
{
  buffering_mode = buf_mode;
}

void clear_video_buffers()
{
  // Clear all three video buffers
  memset(g_v_buf, 0, 3 * V_BUF_SZ);

  // Reset buffer indices
  v_buf_in_idx = 0;
  v_buf_out_idx = 0;

  // Initialize buffer states
  // Buffer 0: ready for display (capture will write here first)
  // Buffer 1, 2: free for capture
  buf_is_free[0] = false;
  buf_is_free[1] = true;
  buf_is_free[2] = true;

  first_frame = true;
}