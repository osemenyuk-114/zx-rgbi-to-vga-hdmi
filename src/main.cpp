#include <Arduino.h>

#include "pico.h"
#include "pico/time.h"
#include "hardware/vreg.h"

#ifdef USB_KBD_ENABLE
#include "tusb.h"
#endif

extern "C"
{
#include "g_config.h"
#include "led.h"
#include "rgb_capture.h"
#include "settings.h"
#include "v_buf.h"
#include "video_output.h"

#ifdef OSD_ENABLE
#include "osd.h"
#endif

#ifdef OSD_FF_ENABLE
#include "ff_osd.h"
#endif

#ifdef KBD_ENABLE
#include "kbd.h"
#endif

#ifdef USB_KBD_ENABLE
#include "usb_kbd.h"
#endif
}

#ifdef SERIAL_MENU_ENABLE
#include "serial_menu.h"
#endif

settings_t settings;

volatile bool start_core0 = false;

volatile bool stop_core1 = false;
volatile bool core1_inactive = false;

volatile bool restart_capture = false;
volatile bool capture_active = false;

void setup()
{
  vreg_set_voltage(VREG_VOLTAGE_1_25);
  sleep_ms(100);

#ifdef SERIAL_MENU_ENABLE
  Serial.begin(115200);
#endif

#ifdef USB_KBD_ENABLE
  // Initialize TinyUSB Host directly (not tusb_init() which comes from
  // pre-compiled libpico.a and is device-mode only).
  tuh_init(0);
#endif

  load_settings(&settings);
#ifdef VIDEO_OUTPUT_AUTO_DETECT
  settings.video_out_type = detect_video_output_type();
  check_settings(&settings);
#endif
  set_buffering_mode(settings.buffering_mode);

  // Initialize LED before video output so WS2812 PIO program claims offset 0 on PIO0
  led_init();

  draw_welcome_screen(*(video_modes[settings.video_out_mode]));
  set_scanlines_mode();
  start_video_output(settings.video_out_type);

#ifdef OSD_ENABLE
  osd_init();
#endif

  start_core0 = true;
}

void loop()
{
#ifdef OSD_ENABLE
  osd_update();
#endif

#ifdef SERIAL_MENU_ENABLE
#ifdef OSD_ENABLE
  if (!osd_state.visible)
#endif
    handle_serial_menu();
#endif

#ifdef USB_KBD_ENABLE
  {
    static uint32_t last_usb_us = 0;
    uint32_t now_us = time_us_32();

    if (now_us - last_usb_us >= 500)
    {
      usb_kbd_task();
      last_usb_us = now_us;
    }
  }
#endif
}

void setup1()
{
  while (!start_core0)
    sleep_ms(10);

#ifdef OSD_FF_ENABLE
  if (settings.ff_osd_config.enabled)
    ff_osd_i2c_init();
#endif

#ifdef KBD_ENABLE
  kbd_init();
#endif

  start_capture();
}

void __not_in_flash_func(loop1())
{
  uint32_t frame_count_tmp1 = frame_count;

#ifdef OSD_FF_ENABLE
  if (ff_osd_needs_i2c_init)
  {
    ff_osd_i2c_init();
    ff_osd_needs_i2c_init = false;
  }

  //  Call osd_process frequently to keep up with I2C data
  if (settings.ff_osd_config.enabled)
  {
    for (int i = 0; i < 100; i++)
    {
      ff_osd_i2c_process();
      sleep_ms(1);
    }
  }
  else
    sleep_ms(100);
#else
  sleep_ms(100);
#endif

#ifdef KBD_ENABLE
  // Timeout: turn off blue LED if no keyboard activity for this cycle
  {
    static uint32_t prev_kbd_cnt = 0;
    uint32_t cnt = kbd_activity_cnt;
    if (cnt == prev_kbd_cnt)
      led_put(LED_B, 0);
    prev_kbd_cnt = cnt;
  }
#endif

  if (frame_count > 1)
  {
    led_put(LED_G, ((frame_count & 0x20) && capture_active) ? 16 : 0);

    if (frame_count == frame_count_tmp1)
    {
      if (capture_active)
      {
        capture_active = false;
        draw_no_signal(*(video_modes[settings.video_out_mode]));
      }
    }
    else if (!capture_active)
      capture_active = true;
  }

  if (stop_core1)
  {
    core1_inactive = true;

    __dmb();

    uint32_t ints = save_and_disable_interrupts();

    while (core1_inactive)
      __dmb();

    restore_interrupts_from_disabled(ints);
  }

  if (restart_capture)
  {
    stop_capture();
    start_capture();
    restart_capture = false;
  }
}