# 1 "C:\\Users\\alec\\AppData\\Local\\Temp\\tmpep5rn6f2"
#include <Arduino.h>
# 1 "C:/Users/alec/OneDrive/DIY/Projects/RGB-to-VGA-HDMI/zx-rgbi-to-vga-hdmi-ff-osd/ZX_RGBI_TO_VGA_HDMI/ZX_RGBI_TO_VGA_HDMI.ino"
#include <Arduino.h>

#include "pico.h"
#include "pico/time.h"
#include "hardware/vreg.h"

#include "serial_menu.h"

extern "C"
{
#include "g_config.h"
#include "rgb_capture.h"
#include "settings.h"
#include "v_buf.h"
#include "video_output.h"

#ifdef OSD_ENABLE
#include "osd.h"
#endif

#ifdef OSD_MENU_ENABLE
#include "osd_menu.h"
#endif

#ifdef OSD_FF_ENABLE
#include "ff_osd.h"
#endif
}

settings_t settings;

volatile bool start_core0 = false;

volatile bool stop_core1 = false;
volatile bool core1_inactive = false;

volatile bool restart_capture = false;
volatile bool capture_active = false;
void setup();
void loop();
void setup1();
#line 40 "C:/Users/alec/OneDrive/DIY/Projects/RGB-to-VGA-HDMI/zx-rgbi-to-vga-hdmi-ff-osd/ZX_RGBI_TO_VGA_HDMI/ZX_RGBI_TO_VGA_HDMI.ino"
void setup()
{
  vreg_set_voltage(VREG_VOLTAGE_1_25);
  sleep_ms(100);

  Serial.begin(9600);

  load_settings(&settings);
  set_buffering_mode(settings.buffering_mode);
  draw_welcome_screen(*(video_modes[settings.video_out_mode]));
  set_scanlines_mode();
  start_video_output(settings.video_out_type);

#ifdef OSD_ENABLE
  osd_init();
#endif

  start_core0 = true;

  Serial.println("  Starting...\n");
}

void loop()
{
#ifdef OSD_ENABLE
  osd_update();

  if (!osd_state.visible)
  {
#endif
    char c = get_menu_input(100);

    if (c != 0)
      handle_serial_menu();

#ifdef OSD_ENABLE
  }
#endif
}

void setup1()
{
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, LOW);

  while (!start_core0)
    sleep_ms(10);

#ifdef OSD_FF_ENABLE
  ff_osd_i2c_init();
#endif

  start_capture();
}

void __not_in_flash_func(loop1())
{
  uint32_t frame_count_tmp1 = frame_count;

#ifdef OSD_FF_ENABLE

  for (int i = 0; i < 100; i++)
  {
    ff_osd_i2c_process();
    sleep_ms(1);
  }
#else
  sleep_ms(100);
#endif

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