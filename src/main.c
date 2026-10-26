#include "pico/multicore.h"

#include "pico/stdlib.h"
#include "hardware/vreg.h"

#include "g_config.h"
#include "led.h"
#include "rgb_capture.h"
#include "settings.h"
#include "v_buf.h"
#include "video_output.h"

#ifdef OSD_ENABLE
#include "osd.h"
#endif

#ifdef SERIAL_MENU_ENABLE
#include "pico/stdio_usb.h"
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
  stdio_init_all();
#endif

  load_settings(&settings);

#ifdef VIDEO_OUTPUT_AUTO_DETECT
  settings.video_out_type = detect_video_output_type();
  check_settings(&settings);
#endif

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

}

void __attribute__((weak)) setup1()
{
  while (!start_core0)
    sleep_ms(10);

  start_capture();
}

void __attribute__((weak)) __not_in_flash_func(loop1())
{
  uint32_t frame_count_tmp1 = frame_count;

  sleep_ms(100);

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

void main1()
{
  if (setup1)
    setup1();

  while (1)
    if (loop1)
      loop1();
}

int main()
{
  multicore_launch_core1(main1);

  setup();

  while (1)
    loop();
}