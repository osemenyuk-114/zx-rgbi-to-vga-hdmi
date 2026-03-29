#include <stddef.h>

#include "hardware/flash.h"
#include "hardware/sync.h"
#include "hardware/watchdog.h"

#include "g_config.h"
#include "settings.h"

extern volatile bool stop_core1;
extern volatile bool core1_inactive;

extern settings_t settings;

void check_settings(settings_t *settings)
{
  if (settings->video_out_type > VIDEO_OUT_TYPE_MAX ||
      settings->video_out_type < VIDEO_OUT_TYPE_MIN)
    settings->video_out_type = VIDEO_OUT_TYPE_DEF;

  if (settings->video_out_mode > VIDEO_OUT_MODE_MAX ||
      (settings->video_out_type == DVI && settings->video_out_mode > VIDEO_MODE_DVI_MAX) || // DVI mode supports resolutions up to VIDEO_MODE_DVI_MAX
      settings->video_out_mode < VIDEO_OUT_MODE_MIN)
    settings->video_out_mode = VIDEO_OUT_MODE_DEF;

  if (settings->cap_sync_mode > CAP_SYNC_MODE_MAX ||
      settings->cap_sync_mode < CAP_SYNC_MODE_MIN)
    settings->cap_sync_mode = CAP_SYNC_MODE_DEF;

  if (settings->frequency > FREQUENCY_MAX ||
      settings->frequency < FREQUENCY_MIN)
    settings->frequency = FREQUENCY_DEF;

  if (settings->ext_clk_divider > EXT_CLK_DIVIDER_MAX ||
      settings->ext_clk_divider < EXT_CLK_DIVIDER_MIN)
    settings->ext_clk_divider = EXT_CLK_DIVIDER_DEF;

  if (settings->delay > DELAY_MAX ||
      settings->delay < DELAY_MIN)
    settings->delay = DELAY_DEF;

  if (settings->shX > shX_MAX ||
      settings->shX < shX_MIN)
    settings->shX = shX_DEF;

  if (settings->shY > shY_MAX ||
      settings->shY < shY_MIN)
    settings->shY = shY_DEF;

  if (settings->pin_inversion_mask & ~PIN_INVERSION_MASK)
  {
    settings->pin_inversion_mask = PIN_INVERSION_MASK_DEF;
    settings->scanlines_mode = false;
    settings->buffering_mode = false;
    settings->video_sync_mode = false;
  }

#ifdef OSD_FF_ENABLE
  if (settings->ff_osd_config.h_position < 1 || settings->ff_osd_config.h_position > 5)
    settings->ff_osd_config.h_position = 3;
#endif

  settings->crc = calculate_settings_crc(settings);
}

void reset_settings_to_defaults(settings_t *settings)
{
  settings->video_out_type = VIDEO_OUT_TYPE_DEF;
  settings->video_out_mode = VIDEO_OUT_MODE_DEF;
  settings->cap_sync_mode = CAP_SYNC_MODE_DEF;
  settings->frequency = FREQUENCY_DEF;
  settings->ext_clk_divider = EXT_CLK_DIVIDER_DEF;
  settings->delay = DELAY_DEF;
  settings->shX = shX_DEF;
  settings->shY = shY_DEF;
  settings->pin_inversion_mask = PIN_INVERSION_MASK_DEF;
  settings->scanlines_mode = false;
  settings->buffering_mode = false;
  settings->video_sync_mode = false;
#ifdef OSD_FF_ENABLE
  settings->ff_osd_config = (ff_osd_config_t){
      .i2c_protocol = true,
      .cols = 40,
      .rows = 3,
      .h_position = 3,
      .v_position = false,
  };
#endif

  settings->crc = calculate_settings_crc(settings);
}

uint32_t calculate_settings_crc(const settings_t *settings)
{
  const uint8_t *data = (const uint8_t *)settings;
  // CRC covers all fields except the crc field itself (last 4 bytes)
  size_t len = offsetof(settings_t, crc);
  uint32_t crc = 0xFFFFFFFF;

  for (size_t i = 0; i < len; i++)
  {
    crc ^= data[i];
    for (int bit = 0; bit < 8; bit++)
      crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
  }

  return ~crc;
}

void load_settings(settings_t *settings)
{
  const int *saved_settings = (const int *)(XIP_BASE + (PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE));

  memcpy(settings, saved_settings, sizeof(settings_t));

  if (settings->crc != calculate_settings_crc(settings))
  {
    reset_settings_to_defaults(settings);
    return;
  }

  check_settings(settings);
}

void save_settings(settings_t *settings)
{
  check_settings(settings);

  stop_core1 = true;

  while (!core1_inactive)
    ;

  uint32_t ints = save_and_disable_interrupts();

  flash_range_erase((PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE), FLASH_SECTOR_SIZE);
  flash_range_program((PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE), (uint8_t *)settings, FLASH_PAGE_SIZE);

  restore_interrupts_from_disabled(ints);

  stop_core1 = false;
  core1_inactive = false;
}