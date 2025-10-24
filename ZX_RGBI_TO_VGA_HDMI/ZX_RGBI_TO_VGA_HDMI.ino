#include <Arduino.h>

#include "pico.h"
#include "pico/time.h"

#include "serial_menu.h"

extern "C"
{
#include "g_config.h"
#include "rgb_capture.h"
#include "settings.h"
#include "v_buf.h"
#include "video_output.h"
}

settings_t settings;
video_mode_t video_mode;

volatile bool start_core0 = false;

volatile bool stop_core1 = false;
volatile bool core1_inactive = false;

volatile bool restart_capture = false;
volatile bool capture_active = false;

// Video output active state flag
video_out_type_t active_video_output = VIDEO_OUT_TYPE_DEF;

void setup()
{
  Serial.begin(9600);

  load_settings(&settings);
  set_buffering_mode(settings.buffering_mode);
  draw_welcome_screen(*(video_modes[settings.video_out_mode]));
  set_scanlines_mode();
  start_video_output(settings.video_out_type);

  start_core0 = true;

  Serial.println("  Starting...\n");
}

void loop()
{
  handle_serial_menu();
}

void setup1()
{
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, LOW);

  while (!start_core0)
    sleep_ms(10);

  start_capture();
}

void __not_in_flash_func(loop1())
{
  uint32_t frame_count_tmp1 = frame_count;

  sleep_ms(100);

  if (frame_count > 1)
  {
    digitalWrite(PIN_LED, (frame_count & 0x20) && capture_active);

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

  if (restart_capture)
  {
    stop_capture();
    start_capture();
    restart_capture = false;
  }

  if (stop_core1)
  {
    core1_inactive = true;

    uint32_t ints = save_and_disable_interrupts();

    while (core1_inactive)
      ;

    restore_interrupts_from_disabled(ints);
  }
}