#include <Arduino.h>
#include <typeinfo>

extern "C"
{
#include "g_config.h"
#include "dvi.h"
#include "rgb_capture.h"
#include "v_buf.h"
#include "vga.h"

#include "hardware/flash.h"
}

settings_t settings;
video_mode_t video_mode;

const int *saved_settings = (const int *)(XIP_BASE + (PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE));
bool start_core0 = false;

void save_settings(settings_t *settings)
{
  Serial.println("  Saving settings...");

  check_settings(settings);

  rp2040.idleOtherCore();
  uint32_t ints = save_and_disable_interrupts();

  flash_range_erase((PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE), FLASH_SECTOR_SIZE);
  flash_range_program((PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE), (uint8_t *)settings, FLASH_PAGE_SIZE);

  // restore_interrupts(ints);
  // rp2040.resumeOtherCore();
}

void print_byte_hex(uint8_t byte)
{
  if (byte < 16)
    Serial.print("0");

  Serial.print(byte, HEX);
}

String binary_to_string(uint8_t value, bool mask_1)
{
  uint8_t binary = value;
  String str = "";

  for (int i = 0; i < 8; i++)
  {
    str += binary & 0b10000000 ? (mask_1 ? "X" : "1") : "0";
    binary <<= 1;
  }

  return str;
}

void print_main_menu()
{
  Serial.println("");
  Serial.print("      * ZX RGB(I) to VGA/HDMI ");
  Serial.print(FW_VERSION);
  Serial.println(" *");
  Serial.println("");
  Serial.println("  v   set video output mode");
  Serial.println("  s   set scanlines mode");
  Serial.println("  b   set buffering mode");
  Serial.println("  c   set capture synchronization source");
  Serial.println("  f   set capture frequency");
  Serial.println("  d   set external clock divider");
  Serial.println("  y   set video sync mode");
  Serial.println("  t   set capture delay and image position");
  Serial.println("  m   set pin inversion mask");
  Serial.println("");
  Serial.println("  p   show configuration");
  Serial.println("  h   show help (this menu)");
  Serial.println("  q   exit configuration mode");
  Serial.println("  w   save configuration and restart");
  Serial.println("");
}

void print_video_out_menu()
{
  Serial.println("");
  Serial.println("      * Video output mode *");
  Serial.println("");
  Serial.println("  1   HDMI   640x480 (div 2)");
  Serial.println("  2   VGA    640x480 (div 2)");
  Serial.println("  3   VGA    800x600 (div 2)");
  Serial.println("  4   VGA   1024x768 (div 3)");
  Serial.println("  5   VGA  1280x1024 (div 3)");
  Serial.println("  6   VGA  1280x1024 (div 4)");
  Serial.println("");
  Serial.println("  p   show configuration");
  Serial.println("  h   show help (this menu)");
  Serial.println("  q   exit to main menu");
  Serial.println("");
}

void print_scanlines_mode_menu()
{
  Serial.println("");
  Serial.println("      * Scanlines mode *");
  Serial.println("");
  Serial.println("  s   change scanlines mode");
  Serial.println("");
  Serial.println("  p   show configuration");
  Serial.println("  h   show help (this menu)");
  Serial.println("  q   exit to main menu");
  Serial.println("");
}

void print_buffering_mode_menu()
{
  Serial.println("");
  Serial.println("      * Buffering mode *");
  Serial.println("");
  Serial.println("  b   change buffering mode");
  Serial.println("");
  Serial.println("  p   show configuration");
  Serial.println("  h   show help (this menu)");
  Serial.println("  q   exit to main menu");
  Serial.println("");
}

void print_cap_sync_mode_menu()
{
  Serial.println("");
  Serial.println("      * Capture synchronization source *");
  Serial.println("");
  Serial.println("  1   self-synchronizing");
  Serial.println("  2   external clock");
  Serial.println("");
  Serial.println("  p   show configuration");
  Serial.println("  h   show help (this menu)");
  Serial.println("  q   exit to main menu");
  Serial.println("");
}

void print_capture_frequency_menu()
{
  Serial.println("");
  Serial.println("      * Capture frequency *");
  Serial.println("");
  Serial.println("  1   7000000 Hz (ZX Spectrum  48K)");
  Serial.println("  2   7093790 Hz (ZX Spectrum 128K)");
  Serial.println("  3   custom");
  Serial.println("");
  Serial.println("  p   show configuration");
  Serial.println("  h   show help (this menu)");
  Serial.println("  q   exit to main menu");
  Serial.println("");
}

void print_ext_clk_divider_menu()
{
  Serial.println("");
  Serial.println("      * External clock divider *");
  Serial.println("");
  Serial.println("  a   increment divider (+1)");
  Serial.println("  z   decrement divider (-1)");
  Serial.println("");
  Serial.println("  p   show configuration");
  Serial.println("  h   show help (this menu)");
  Serial.println("  q   exit to main menu");
  Serial.println("");
}

void print_video_sync_mode_menu()
{
  Serial.println("");
  Serial.println("      * Video synchronization mode *");
  Serial.println("");
  Serial.println("  y   change synchronization mode");
  Serial.println("");
  Serial.println("  p   show configuration");
  Serial.println("  h   show help (this menu)");
  Serial.println("  q   exit to main menu");
  Serial.println("");
}

void print_image_tuning_menu()
{
  Serial.println("");
  Serial.println("      * Capture delay and image position *");
  Serial.println("");
  Serial.println("  a   increment delay (+1)");
  Serial.println("  z   decrement delay (-1)");
  Serial.println("");
  Serial.println("  i   shift image UP");
  Serial.println("  k   shift image DOWN");
  Serial.println("  j   shift image LEFT");
  Serial.println("  l   shift image RIGHT");
  Serial.println("");
  Serial.println("  p   show configuration");
  Serial.println("  h   show help (this menu)");
  Serial.println("  q   exit to main menu");
  Serial.println("");
}

void print_pin_inversion_mask_menu()
{
  Serial.println("");
  Serial.println("      * Pin inversion mask *");
  Serial.println("");
  Serial.println("  m   set pin inversion mask");
  Serial.println("");
  Serial.println("  p   show configuration");
  Serial.println("  h   show help (this menu)");
  Serial.println("  q   exit to main menu");
  Serial.println("");
}

void print_test_menu()
{
  Serial.println("");
  Serial.println("      * Test *");
  Serial.println("");
  Serial.println("  1   draw welcome image (vertical stripes)");
  Serial.println("  2   draw welcome image (horizontal stripes)");
  Serial.println("  i   show captured frame count");
  Serial.println("");
  Serial.println("  p   show configuration");
  Serial.println("  h   show help (this menu)");
  Serial.println("  q   exit to main menu");
  Serial.println("");
}

void print_video_out_mode()
{
  Serial.print("  Video output mode ........... ");
  switch (settings.video_out_mode)
  {
  case DVI:
    Serial.println("HDMI 640x480");
    break;

  case VGA640x480:
    Serial.println("VGA 640x480");
    break;

  case VGA800x600:
    Serial.println("VGA 800x600");
    break;

  case VGA1024x768:
    Serial.println("VGA 1024x768");
    break;

  case VGA1280x1024_d3:
    Serial.println("VGA 1280x1024 (div 3)");
    break;

  case VGA1280x1024_d4:
    Serial.println("VGA 1280x1024 (div 4)");
    break;

  default:
    break;
  }
}

void print_scanlines_mode()
{
  Serial.print("  Scanlines ................... ");

  if (settings.scanlines_mode)
    Serial.println("enabled");
  else
    Serial.println("disabled");
}

void print_buffering_mode()
{
  Serial.print("  Buffering mode .............. ");

  if (settings.x3_buffering_mode)
    Serial.println("x3");
  else
    Serial.println("x1");
}

void print_cap_sync_mode()
{
  Serial.print("  Capture sync source ......... ");
  switch (settings.cap_sync_mode)
  {
  case SELF:
    Serial.println("self-synchronizing");
    break;

  case EXT:
    Serial.println("external clock");
    break;

  default:
    break;
  }
}

void print_capture_frequency()
{
  Serial.print("  Capture frequency ........... ");
  Serial.print(settings.frequency, DEC);
  Serial.println(" Hz");
}

void print_ext_clk_divider()
{
  Serial.print("  External clock divider ...... ");
  Serial.println(settings.ext_clk_divider, DEC);
}

void print_capture_delay()
{
  Serial.print("  Capture delay ............... ");
  Serial.println(settings.delay, DEC);
}

void print_x_offset()
{
  Serial.print("  X offset .................... ");
  Serial.println(settings.shX, DEC);
}

void print_y_offset()
{
  Serial.print("  Y offset .................... ");
  Serial.println(settings.shY, DEC);
}

void print_dividers()
{
  uint16_t div_int;
  uint8_t div_frac;

  video_mode_t video_mode = *(vga_modes[settings.video_out_mode]);

  Serial.println("");

  Serial.print("  System clock frequency ...... ");
  Serial.print(clock_get_hz(clk_sys), 1);
  Serial.println(" Hz");

  Serial.println("  Capture divider");

  Serial.print("    calculated (SDK) .......... ");

  pio_calculate_clkdiv_from_float((float)clock_get_hz(clk_sys) / (settings.frequency * 12.0), &div_int, &div_frac);

  Serial.print((div_int + (float)div_frac / 256), 8);

  Serial.print(" ( ");
  Serial.print("0x");
  print_byte_hex((uint8_t)(div_int >> 8));
  print_byte_hex((uint8_t)(div_int & 0xff));
  print_byte_hex(div_frac);
  Serial.println(" )");

  Serial.print("    optimized ................. ");

  calculate_clkdiv(settings.frequency, &div_int, &div_frac);

  Serial.print((div_int + (float)div_frac / 256), 8);

  Serial.print(" ( ");
  Serial.print("0x");
  print_byte_hex((uint8_t)(div_int >> 8));
  print_byte_hex((uint8_t)(div_int & 0xff));
  print_byte_hex(div_frac);
  Serial.println(" )");

  Serial.print("  Video output clock divider .. ");

  pio_calculate_clkdiv_from_float(((float)clock_get_hz(clk_sys) * video_mode.div) / video_mode.pixel_freq, &div_int, &div_frac);

  Serial.print((div_int + (float)div_frac / 256), 8);

  Serial.print(" ( ");
  Serial.print("0x");
  print_byte_hex((uint8_t)(div_int >> 8));
  print_byte_hex((uint8_t)(div_int & 0xff));
  print_byte_hex(div_frac);
  Serial.println(" )");

  Serial.println("");
}

void print_video_sync_mode()
{
  Serial.print("  Video synchronization mode .. ");
  if (settings.video_sync_mode)
    Serial.println("separate");
  else
    Serial.println("composite");
}

void print_pin_inversion_mask()
{
  Serial.print("  Pin inversion mask .......... ");
  Serial.println(binary_to_string(settings.pin_inversion_mask, false));
}

void print_settings()
{
  Serial.println("");
  print_video_out_mode();
  print_scanlines_mode();
  print_buffering_mode();
  print_cap_sync_mode();
  print_capture_frequency();
  print_ext_clk_divider();
  print_video_sync_mode();
  print_capture_delay();
  print_x_offset();
  print_y_offset();
  print_pin_inversion_mask();
  print_dividers();
  Serial.println("");
}

void set_scanlines_mode()
{
  if (settings.video_out_mode != DVI)
    set_vga_scanlines_mode(settings.scanlines_mode);
}

void setup()
{
  Serial.begin(9600);

  // loading saved settings
  memcpy(&settings, saved_settings, sizeof(settings_t));
  // correct if there is garbage in the cells
  check_settings(&settings);

  set_v_buf_buffering_mode(settings.x3_buffering_mode);

  draw_welcome_screen(*(vga_modes[settings.video_out_mode]));

  set_scanlines_mode();

  if (settings.video_out_mode == DVI)
  {
    start_dvi(*(vga_modes[settings.video_out_mode]));
  }
  else
  {
    start_vga(*(vga_modes[settings.video_out_mode]));
  }

  start_core0 = true;

  Serial.println("  Starting...");
  Serial.println("");
}

void loop()
{
  char inbyte = 0;

  while (1)
  {
    sleep_ms(500);

    if (Serial.available())
    {
      inbyte = 'h';
      break;
    }
  }

  Serial.println(" Entering the configuration mode");
  Serial.println("");

  while (1)
  {
    sleep_ms(10);

    if (inbyte != 'h' && Serial.available())
      inbyte = Serial.read();

    switch (inbyte)
    {
    case 'p':
      print_settings();
      inbyte = 0;
      break;

    case 'v':
    {
      inbyte = 'h';

      while (1)
      {
        sleep_ms(10);

        if (inbyte != 'h' && Serial.available())
          inbyte = Serial.read();

        switch (inbyte)
        {
        case 'p':
          print_video_out_mode();
          break;

        case 'h':
          print_video_out_menu();
          break;

        case '1':
          settings.video_out_mode = DVI;
          print_video_out_mode();
          break;

        case '2':
          settings.video_out_mode = VGA640x480;
          print_video_out_mode();
          break;

        case '3':
          settings.video_out_mode = VGA800x600;
          print_video_out_mode();
          break;

        case '4':
          settings.video_out_mode = VGA1024x768;
          print_video_out_mode();
          break;

        case '5':
          settings.video_out_mode = VGA1280x1024_d3;
          print_video_out_mode();
          break;
        case '6':
          settings.video_out_mode = VGA1280x1024_d4;
          print_video_out_mode();
          break;

        default:
          break;
        }

        if (inbyte == 'q')
        {
          inbyte = 'h';
          break;
        }

        inbyte = 0;
      }

      break;
    }

    case 's':
    {
      inbyte = 'h';

      while (1)
      {
        sleep_ms(10);

        if (inbyte != 'h' && Serial.available())
          inbyte = Serial.read();

        switch (inbyte)
        {
        case 'p':
          print_scanlines_mode();
          break;

        case 'h':
          print_scanlines_mode_menu();
          break;

        case 's':
          settings.scanlines_mode = !settings.scanlines_mode;
          print_scanlines_mode();
          set_scanlines_mode();
          break;

        default:
          break;
        }

        if (inbyte == 'q')
        {
          inbyte = 'h';
          break;
        }

        inbyte = 0;
      }

      break;
    }

    case 'b':
    {
      inbyte = 'h';

      while (1)
      {
        sleep_ms(10);

        if (inbyte != 'h' && Serial.available())
          inbyte = Serial.read();

        switch (inbyte)
        {
        case 'p':
          print_buffering_mode();
          break;

        case 'h':
          print_buffering_mode_menu();
          break;

        case 'b':
          settings.x3_buffering_mode = !settings.x3_buffering_mode;
          print_buffering_mode();
          set_v_buf_buffering_mode(settings.x3_buffering_mode);
          break;

        default:
          break;
        }

        if (inbyte == 'q')
        {
          inbyte = 'h';
          break;
        }

        inbyte = 0;
      }

      break;
    }

    case 'c':
    {
      inbyte = 'h';

      while (1)
      {
        sleep_ms(10);

        if (inbyte != 'h' && Serial.available())
          inbyte = Serial.read();

        switch (inbyte)
        {
        case 'p':
          print_cap_sync_mode();
          break;

        case 'h':
          print_cap_sync_mode_menu();
          break;

        case '1':
          settings.cap_sync_mode = SELF;
          print_cap_sync_mode();
          break;

        case '2':
          settings.cap_sync_mode = EXT;
          print_cap_sync_mode();
          break;

        default:
          break;
        }

        if (inbyte == 'q')
        {
          inbyte = 'h';
          break;
        }

        inbyte = 0;
      }

      break;
    }

    case 'f':
    {
      inbyte = 'h';

      while (1)
      {
        sleep_ms(10);

        if (inbyte != 'h' && Serial.available())
          inbyte = Serial.read();

        switch (inbyte)
        {
        case 'p':
          print_capture_frequency();
          break;

        case 'h':
          print_capture_frequency_menu();
          break;

        case '1':
          settings.frequency = 7000000;
          print_capture_frequency();
          break;

        case '2':
          settings.frequency = 7093790;
          print_capture_frequency();
          break;

        case '3':
        {
          String str_frequency = "";
          uint32_t frequency = 0;

          Serial.print("  Enter frequency: ");

          while (1)
          {
            sleep_ms(10);
            inbyte = 0;

            if (Serial.available())
              inbyte = Serial.read();

            if (inbyte >= '0' && inbyte <= '9')
            {
              Serial.print(inbyte);
              str_frequency += inbyte;
            }

            if (inbyte == '\r')
            {
              Serial.println("");
              frequency = str_frequency.toInt();

              if (frequency >= FREQUENCY_MIN && frequency <= FREQUENCY_MAX)
              {
                settings.frequency = frequency;
                print_capture_frequency();
                break;
              }
              else
              {
                str_frequency = "";
                Serial.print("  Allowed frequency range ..... ");
                Serial.print(FREQUENCY_MIN, DEC);
                Serial.print(" - ");
                Serial.print(FREQUENCY_MAX, DEC);
                Serial.println(" Hz");
                Serial.print("  Enter frequency: ");
              }
            }
          }

          break;
        }

        default:
          break;
        }

        if (inbyte == 'q')
        {
          inbyte = 'h';
          break;
        }

        inbyte = 0;
      }

      break;
    }

    case 'd':
    {
      inbyte = 'h';

      while (1)
      {
        sleep_ms(10);

        if (inbyte != 'h' && Serial.available())
          inbyte = Serial.read();

        switch (inbyte)
        {

        case 'p':
          print_ext_clk_divider();
          break;

        case 'h':
          print_ext_clk_divider_menu();
          break;

        case 'a':
          settings.ext_clk_divider = settings.ext_clk_divider < EXT_CLK_DIVIDER_MAX ? (settings.ext_clk_divider + 1) : EXT_CLK_DIVIDER_MAX;
          print_ext_clk_divider();
          break;

        case 'z':
          settings.ext_clk_divider = settings.ext_clk_divider > EXT_CLK_DIVIDER_MIN ? (settings.ext_clk_divider - 1) : EXT_CLK_DIVIDER_MIN;
          print_ext_clk_divider();
          break;

        default:
          break;
        }

        if (inbyte == 'q')
        {
          inbyte = 'h';
          break;
        }

        inbyte = 0;
      }

      break;
    }

    case 'y':
    {
      inbyte = 'h';

      while (1)
      {
        sleep_ms(10);

        if (inbyte != 'h' && Serial.available())
          inbyte = Serial.read();

        switch (inbyte)
        {
        case 'p':
          print_video_sync_mode();
          break;

        case 'h':
          print_video_sync_mode_menu();
          break;

        case 'y':
          settings.video_sync_mode = !settings.video_sync_mode;
          print_video_sync_mode();
          set_video_sync_mode(settings.video_sync_mode);
          break;

        default:
          break;
        }

        if (inbyte == 'q')
        {
          inbyte = 'h';
          break;
        }

        inbyte = 0;
      }

      break;
    }

    case 't':
    {
      inbyte = 'h';

      while (1)
      {
        sleep_ms(10);

        if (inbyte != 'h' && Serial.available())
          inbyte = Serial.read();

        switch (inbyte)
        {

        case 'p':
          print_capture_delay();
          print_x_offset();
          print_y_offset();
          break;

        case 'h':
          print_image_tuning_menu();
          break;

        case 'a':
          settings.delay = set_capture_delay(settings.delay + 1);
          print_capture_delay();
          break;

        case 'z':
          settings.delay = set_capture_delay(settings.delay - 1);
          print_capture_delay();
          break;

        case 'i':
          settings.shY = set_capture_shY(settings.shY + 1);
          print_y_offset();
          break;

        case 'k':
          settings.shY = set_capture_shY(settings.shY - 1);
          print_y_offset();
          break;

        case 'j':
          settings.shX = set_capture_shX(settings.shX - 1);
          print_x_offset();
          break;

        case 'l':
          settings.shX = set_capture_shX(settings.shX + 1);
          print_x_offset();
          break;

        default:
          break;
        }

        if (inbyte == 'q')
        {
          inbyte = 'h';
          break;
        }

        inbyte = 0;
      }

      break;
    }

    case 'm':
    {
      inbyte = 'h';

      while (1)
      {
        sleep_ms(10);

        if (inbyte != 'h' && Serial.available())
          inbyte = Serial.read();

        switch (inbyte)
        {
        case 'p':
          print_pin_inversion_mask();
          break;

        case 'h':
          print_pin_inversion_mask_menu();
          break;

        case 'm':
        {
          String str_pin_inversion_mask = "";

          Serial.print("  Enter pin inversion mask: ");

          while (1)
          {
            sleep_ms(10);
            inbyte = 0;

            if (Serial.available())
              inbyte = Serial.read();

            if (inbyte >= '0' && inbyte <= '1')
            {
              Serial.print(inbyte);
              str_pin_inversion_mask += inbyte;
            }

            if (inbyte == '\r')
            {
              Serial.println("");

              uint8_t pin_inversion_mask = 0;

              for (int i = 0; i < str_pin_inversion_mask.length(); i++)
              {
                pin_inversion_mask <<= 1;
                pin_inversion_mask |= str_pin_inversion_mask[i] == '1' ? 1 : 0;
              }

              if (!(pin_inversion_mask & ~PIN_INVERSION_MASK))
              {
                settings.pin_inversion_mask = pin_inversion_mask;
                print_pin_inversion_mask();
                break;
              }
              else
              {
                str_pin_inversion_mask = "";
                Serial.print("  Allowed inversion mask ...... ");
                Serial.println(binary_to_string(PIN_INVERSION_MASK, true));
                Serial.print("  Enter pin inversion mask: ");
              }
            }
          }

          break;
        }

        default:
          break;
        }

        if (inbyte == 'q')
        {
          inbyte = 'h';
          break;
        }

        inbyte = 0;
      }

      break;
    }

    case 'T':
    {
      inbyte = 'h';

      while (1)
      {
        sleep_ms(10);

        if (inbyte != 'h' && Serial.available())
          inbyte = Serial.read();

        switch (inbyte)
        {
        case 'p':
          print_settings();
          break;

        case 'h':
          print_test_menu();
          break;

        case 'i':
          Serial.print("  Current frame count ......... ");
          Serial.println(frame_count - 1, DEC);
          break;

        case '1':
        case '2':
        {
          uint32_t frame_count_temp = frame_count;

          sleep_ms(100);

          if (frame_count - frame_count_temp == 0) // draw welcome screen only if capture is not active
          {
            Serial.println("  Drawing the welcome screen...");

            if (inbyte == '1')
              draw_welcome_screen(*(vga_modes[settings.video_out_mode]));
            else
              draw_welcome_screen_h(*(vga_modes[settings.video_out_mode]));
          }

          break;
        }

        default:
          break;
        }

        if (inbyte == 'q')
        {
          inbyte = 'h';
          break;
        }

        inbyte = 0;
      }

      break;
    }

    case 'h':
      print_main_menu();
      inbyte = 0;
      break;

    case 'w':
      save_settings(&settings);
      rp2040.restart();
      break;

    default:
      break;
    }

    if (inbyte == 'q')
    {
      inbyte = 0;

      Serial.println(" Leaving the configuration mode");
      Serial.println("");
      break;
    }
  }
}

void setup1()
{
  while (!start_core0)
    sleep_ms(10);

  start_capture(&settings);
}

void loop1()
{
  sleep_ms(1000);
}
