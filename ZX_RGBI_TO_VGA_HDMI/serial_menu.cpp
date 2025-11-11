#include <Arduino.h>

#include "hardware/clocks.h"
#include "hardware/pio.h"
#include "hardware/watchdog.h"

#include "serial_menu.h"

extern "C"
{
#include "g_config.h"
#include "v_buf.h"
#include "settings.h"
#include "rgb_capture.h"
#include "video_output.h"
}

// External variables that need to be accessed
extern settings_t settings;
extern video_out_type_t active_video_output;
extern volatile bool restart_capture;

void print_byte_hex(uint8_t byte)
{
    if (byte < 16)
        Serial.print("0");

    Serial.print(byte, HEX);
}

void binary_to_string(uint8_t value, bool mask_1, char *str)
{
    uint8_t binary = value;

    for (int i = 0; i < 8; i++)
    {
        str[i] = binary & 0b10000000 ? (mask_1 ? 'X' : '1') : '0';
        binary <<= 1;
    }

    str[8] = '\0';
}

uint32_t string_to_int(const char *value)
{
    char *end;
    long val = strtol(value, &end, 10);
    return (value == end) ? 0 : (uint32_t)val;
}

// Input helper function to consolidate repetitive input reading logic
char get_menu_input(int timeout_ms)
{
    sleep_ms(timeout_ms);
    return Serial.available() ? (char)(Serial.read()) : 0;
}

void print_main_menu()
{
    Serial.print("\n      * ZX RGB(I) to VGA/HDMI ");
    Serial.print(FW_VERSION);
    Serial.println(" *\n");

    Serial.println("  o   set video output type (DVI/VGA)");
    Serial.println("  v   set video resolution");

    if (settings.video_out_type == VGA)
        Serial.println("  s   set scanlines mode");

    Serial.println("  b   set buffering mode");
    Serial.println("  c   set capture synchronization source");
    Serial.println("  f   set capture frequency");
    Serial.println("  d   set external clock divider");
    Serial.println("  y   set video sync mode");
    Serial.println("  t   set capture delay and image position");
    Serial.println("  m   set pin inversion mask\n");

    Serial.println("  p   show configuration");
    Serial.println("  h   show help (this menu)");
    Serial.println("  q   exit configuration mode");
    Serial.println("  w   save configuration");
    Serial.println("  r   restart\n");
}

void print_video_out_menu()
{
    Serial.println("\n      * Video resolution *\n");

    Serial.println("  1    640x480 @60Hz (div 2)");

    switch (settings.video_out_type)
    {
    case DVI:
        Serial.println("  2    720x576 @50Hz (div 2)");
        break;

    case VGA:
        Serial.println("  2    800x600 @60Hz (div 2)");
        Serial.println("  3   1024x768 @60Hz (div 3)");
        Serial.println("  4  1280x1024 @60Hz (div 3)");
        Serial.println("  5  1280x1024 @60Hz (div 4)");
        break;

    default:
        break;
    }

    Serial.println();
    Serial.println("  p   show configuration");
    Serial.println("  h   show help (this menu)");
    Serial.println("  q   exit to main menu\n");
}

void print_video_out_type_menu()
{
    Serial.println("\n      * Video output type *\n");

    Serial.println("  1   DVI");
    Serial.println("  2   VGA\n");

    Serial.println("  p   show configuration");
    Serial.println("  h   show help (this menu)");
    Serial.println("  q   exit to main menu\n");
}

void print_scanlines_mode_menu()
{
    Serial.println("\n      * Scanlines mode *\n");

    Serial.println("  s   change scanlines mode\n");

    Serial.println("  p   show configuration");
    Serial.println("  h   show help (this menu)");
    Serial.println("  q   exit to main menu\n");
}

void print_buffering_mode_menu()
{
    Serial.println("\n      * Buffering mode *\n");

    Serial.println("  b   change buffering mode\n");

    Serial.println("  p   show configuration");
    Serial.println("  h   show help (this menu)");
    Serial.println("  q   exit to main menu\n");
}

void print_cap_sync_mode_menu()
{
    Serial.println("\n      * Capture synchronization source *\n");

    Serial.println("  1   self-synchronizing");
    Serial.println("  2   external clock\n");

    Serial.println("  p   show configuration");
    Serial.println("  h   show help (this menu)");
    Serial.println("  q   exit to main menu\n");
}

void print_capture_frequency_menu()
{
    Serial.println("\n      * Capture frequency *\n");

    Serial.println("  1   7000000 Hz (ZX Spectrum  48K)");
    Serial.println("  2   7093800 Hz (ZX Spectrum 128K)");
    Serial.println("  3   custom\n");

    Serial.println("  p   show configuration");
    Serial.println("  h   show help (this menu)");
    Serial.println("  q   exit to main menu\n");
}

void print_ext_clk_divider_menu()
{
    Serial.println("\n      * External clock divider *\n");

    Serial.println("  a   increment divider (+1)");
    Serial.println("  z   decrement divider (-1)\n");

    Serial.println("  p   show configuration");
    Serial.println("  h   show help (this menu)");
    Serial.println("  q   exit to main menu\n");
}

void print_video_sync_mode_menu()
{
    Serial.println("\n      * Video synchronization mode *\n");

    Serial.println("  y   change synchronization mode\n");

    Serial.println("  p   show configuration");
    Serial.println("  h   show help (this menu)");
    Serial.println("  q   exit to main menu\n");
}

void print_image_tuning_menu()
{
    Serial.println("\n      * Capture delay and image position *\n");

    Serial.println("  a   increment delay (+1)");
    Serial.println("  z   decrement delay (-1)\n");

    Serial.println("  i   shift image UP");
    Serial.println("  k   shift image DOWN");
    Serial.println("  j   shift image LEFT");
    Serial.println("  l   shift image RIGHT\n");

    Serial.println("  p   show configuration");
    Serial.println("  h   show help (this menu)");
    Serial.println("  q   exit to main menu\n");
}

void print_pin_inversion_mask_menu()
{
    Serial.println("\n      * Pin inversion mask *\n");

    Serial.println("  m   set pin inversion mask\n");

    Serial.println("  p   show configuration");
    Serial.println("  h   show help (this menu)");
    Serial.println("  q   exit to main menu\n");
}

void print_test_menu()
{
    Serial.println("\n      * Tests *\n");

    Serial.println("  1   draw welcome image (vertical stripes)");
    Serial.println("  2   draw welcome image (horizontal stripes)");
    Serial.println("  3   draw \"NO SIGNAL\" screen");
    Serial.println("  i   show captured frame count\n");

    Serial.println("  p   show configuration");
    Serial.println("  h   show help (this menu)");
    Serial.println("  q   exit to main menu\n");
}

void print_video_out_type()
{
    Serial.print("  Video output type ........... ");

    switch (settings.video_out_type)
    {
    case DVI:
        Serial.println("DVI");
        break;

    case VGA:
        Serial.println("VGA");
        break;

    default:
        break;
    }
}

void print_video_out_mode()
{
    Serial.print("  Video resolution ............ ");

    switch (settings.video_out_mode)
    {
    case MODE_640x480_60Hz:
        Serial.println("640x480 @60Hz");
        break;

    case MODE_720x576_50Hz:
        Serial.println("720x576 @50Hz");
        break;

    case MODE_800x600_60Hz:
        Serial.println("800x600 @60Hz");
        break;

    case MODE_1024x768_60Hz:
        Serial.println("1024x768 @60Hz");
        break;

    case MODE_1280x1024_60Hz_d3:
        Serial.println("1280x1024 @60Hz (div 3)");
        break;

    case MODE_1280x1024_60Hz_d4:
        Serial.println("1280x1024 @60Hz (div 4)");
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

    if (settings.buffering_mode)
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

    video_mode_t video_mode = *(video_modes[settings.video_out_mode]);

    Serial.print("\n  System clock frequency ...... ");
    Serial.print(clock_get_hz(clk_sys));
    Serial.println(" Hz");

    if (settings.cap_sync_mode == SELF)
    {
        Serial.print("  Capture divider ............. ");

        pio_calculate_clkdiv_from_float((float)clock_get_hz(clk_sys) / (settings.frequency * 12.0), &div_int, &div_frac);

        Serial.print((div_int + (float)div_frac / 256), 8);

        Serial.print(" ( ");
        Serial.print("0x");
        print_byte_hex((uint8_t)(div_int >> 8));
        print_byte_hex((uint8_t)(div_int & 0xff));
        print_byte_hex(div_frac);
        Serial.println(" )");
    }

    if (settings.video_out_type == VGA)
    {
        Serial.print("  Video output clock divider .. ");

        pio_calculate_clkdiv_from_float(((float)clock_get_hz(clk_sys) * video_mode.div) / video_mode.pixel_freq, &div_int, &div_frac);

        Serial.print((div_int + (float)div_frac / 256), 8);

        Serial.print(" ( ");
        Serial.print("0x");
        print_byte_hex((uint8_t)(div_int >> 8));
        print_byte_hex((uint8_t)(div_int & 0xff));
        print_byte_hex(div_frac);
        Serial.println(" )");
    }

    Serial.println();
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
    char binary_str[9];
    binary_to_string(settings.pin_inversion_mask, false, binary_str);
    Serial.print("  Pin inversion mask .......... ");
    Serial.println(binary_str);
}

void print_settings()
{
    Serial.println("");
    print_video_out_type();
    print_video_out_mode();

    if (settings.video_out_type == VGA)
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

void handle_serial_menu()
{
    char inchar = 'h';

    Serial.println(" Entering the configuration mode\n");

    while (1)
    {
        if (inchar != 'h')
            inchar = get_menu_input(10); // 10ms timeout

        switch (inchar)
        {
        case 'p':
            print_settings();
            inchar = 0;
            break;

        case 'o':
        {
            inchar = 'h';

            while (1)
            {
                if (inchar != 'h')
                    inchar = get_menu_input(10);

                uint8_t video_out_type = settings.video_out_type;

                switch (inchar)
                {
                case 'p':
                    print_video_out_type();
                    break;

                case 'h':
                    print_video_out_type_menu();
                    break;

                case '1':
                    settings.video_out_type = DVI;
                    settings.video_out_mode = VIDEO_OUT_MODE_DEF;
                    print_video_out_type();
                    break;

                case '2':
                    settings.video_out_type = VGA;
                    settings.video_out_mode = VIDEO_OUT_MODE_DEF;
                    print_video_out_type();
                    break;

                default:
                    break;
                }

                if (video_out_type != settings.video_out_type && active_video_output != settings.video_out_type)
                {
                    stop_video_output();
                    start_video_output(settings.video_out_type);
                    // capture PIO clock divider needs to be adjusted for new system clock frequency set in start_video_output()
                    set_capture_frequency(settings.frequency);
                }

                if (inchar == 'q')
                {
                    inchar = 'h';
                    break;
                }

                inchar = 0;
            }

            break;
        }

        case 'v':
        {
            inchar = 'h';

            while (1)
            {
                if (inchar != 'h')
                    inchar = get_menu_input(10);

                uint8_t video_out_mode = settings.video_out_mode;

                switch (inchar)
                {
                case 'p':
                    print_video_out_mode();
                    break;

                case 'h':
                    print_video_out_menu();
                    break;

                case '1':
                    settings.video_out_mode = MODE_640x480_60Hz;
                    print_video_out_mode();
                    break;

                case '2':
                    if (settings.video_out_type == DVI)
                        settings.video_out_mode = MODE_720x576_50Hz;
                    else
                        settings.video_out_mode = MODE_800x600_60Hz;

                    print_video_out_mode();
                    break;

                case '3':
                    if (settings.video_out_type == VGA)
                    {
                        settings.video_out_mode = MODE_1024x768_60Hz;
                        print_video_out_mode();
                    }

                    break;

                case '4':
                    if (settings.video_out_type == VGA)
                    {
                        settings.video_out_mode = MODE_1280x1024_60Hz_d3;
                        print_video_out_mode();
                    }

                    break;

                case '5':
                    if (settings.video_out_type == VGA)
                    {
                        settings.video_out_mode = MODE_1280x1024_60Hz_d4;
                        print_video_out_mode();
                    }

                    break;

                default:
                    break;
                }

                if (video_out_mode != settings.video_out_mode && active_video_output == settings.video_out_type)
                {
                    stop_video_output();
                    start_video_output(active_video_output);
                    // capture PIO clock divider needs to be adjusted for new system clock frequency set in start_video_output()
                    set_capture_frequency(settings.frequency);
                }

                if (inchar == 'q')
                {
                    inchar = 'h';
                    break;
                }

                inchar = 0;
            }

            break;
        }

        case 's':
        {
            if (settings.video_out_type != VGA)
            {
                inchar = 0;
                break;
            }

            inchar = 'h';

            while (1)
            {
                if (inchar != 'h')
                    inchar = get_menu_input(10);

                switch (inchar)
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

                if (inchar == 'q')
                {
                    inchar = 'h';
                    break;
                }

                inchar = 0;
            }

            break;
        }

        case 'b':
        {
            inchar = 'h';

            while (1)
            {
                if (inchar != 'h')
                    inchar = get_menu_input(10);

                switch (inchar)
                {
                case 'p':
                    print_buffering_mode();
                    break;

                case 'h':
                    print_buffering_mode_menu();
                    break;

                case 'b':
                    settings.buffering_mode = !settings.buffering_mode;
                    print_buffering_mode();
                    set_buffering_mode(settings.buffering_mode);
                    break;

                default:
                    break;
                }

                if (inchar == 'q')
                {
                    inchar = 'h';
                    break;
                }

                inchar = 0;
            }

            break;
        }

        case 'c':
        {
            inchar = 'h';

            while (1)
            {
                if (inchar != 'h')
                    inchar = get_menu_input(10);

                uint8_t cap_sync_mode = settings.cap_sync_mode;

                switch (inchar)
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

                if (cap_sync_mode != settings.cap_sync_mode)
                    restart_capture = true;

                if (inchar == 'q')
                {
                    inchar = 'h';
                    break;
                }

                inchar = 0;
            }

            break;
        }

        case 'f':
        {
            inchar = 'h';

            while (1)
            {
                if (inchar != 'h')
                    inchar = get_menu_input(10);

                uint32_t frequency = settings.frequency;

                switch (inchar)
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
                    settings.frequency = 7093800;
                    print_capture_frequency();
                    break;

                case '3':
                {
                    char frequency_str[8] = "";
                    int str_len = 0;
                    uint32_t frequency_int = 0;

                    Serial.print("  Enter frequency: ");

                    while (1)
                    {
                        inchar = get_menu_input(10);

                        if (inchar >= '0' && inchar <= '9' && str_len < 7)
                        {
                            Serial.print(inchar);
                            frequency_str[str_len++] = inchar;
                            frequency_str[str_len] = '\0';
                        }
                        else if (inchar == 8 || inchar == 127) // Backspace
                        {
                            if (str_len > 0)
                            {
                                str_len--;
                                frequency_str[str_len] = '\0';
                                Serial.print("\b \b");
                            }
                        }
                        else if (inchar == '\r' || inchar == '\n')
                        {
                            Serial.println();

                            if (str_len > 0)
                            {
                                frequency_int = string_to_int(frequency_str);

                                if (frequency_int >= FREQUENCY_MIN && frequency_int <= FREQUENCY_MAX)
                                {
                                    settings.frequency = frequency_int;
                                    print_capture_frequency();
                                    break;
                                }
                            }

                            // Reset for retry
                            str_len = 0;
                            frequency_str[0] = '\0';
                            Serial.print("  Allowed frequency range ..... ");
                            Serial.print(FREQUENCY_MIN, DEC);
                            Serial.print(" - ");
                            Serial.print(FREQUENCY_MAX, DEC);
                            Serial.println(" Hz");
                            Serial.print("  Enter frequency: ");
                        }
                    }

                    break;
                }

                default:
                    break;
                }

                if (frequency != settings.frequency)
                {
                    set_capture_frequency(settings.frequency);
                    // restart video output with new capture frequency value which is used to calculate horizontal margins for some video output modes
                    stop_video_output();
                    start_video_output(active_video_output);
                }

                if (inchar == 'q')
                {
                    inchar = 'h';
                    break;
                }

                inchar = 0;
            }

            break;
        }

        case 'd':
        {
            inchar = 'h';

            while (1)
            {
                if (inchar != 'h')
                    inchar = get_menu_input(10);

                switch (inchar)
                {

                case 'p':
                    print_ext_clk_divider();
                    break;

                case 'h':
                    print_ext_clk_divider_menu();
                    break;

                case 'a':
                    settings.ext_clk_divider = set_ext_clk_divider(settings.ext_clk_divider + 1);
                    print_ext_clk_divider();
                    break;

                case 'z':
                    settings.ext_clk_divider = set_ext_clk_divider(settings.ext_clk_divider - 1);
                    print_ext_clk_divider();
                    break;

                default:
                    break;
                }

                if (inchar == 'q')
                {
                    inchar = 'h';
                    break;
                }

                inchar = 0;
            }

            break;
        }

        case 'y':
        {
            inchar = 'h';

            while (1)
            {
                if (inchar != 'h')
                    inchar = get_menu_input(10);

                switch (inchar)
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

                if (inchar == 'q')
                {
                    inchar = 'h';
                    break;
                }

                inchar = 0;
            }

            break;
        }

        case 't':
        {
            inchar = 'h';

            while (1)
            {
                if (inchar != 'h')
                    inchar = get_menu_input(10);

                switch (inchar)
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
                    settings.shX = set_capture_shX(settings.shX + 1);
                    print_x_offset();
                    break;

                case 'l':
                    settings.shX = set_capture_shX(settings.shX - 1);
                    print_x_offset();
                    break;

                default:
                    break;
                }

                if (inchar == 'q')
                {
                    inchar = 'h';
                    break;
                }

                inchar = 0;
            }

            break;
        }

        case 'm':
        {
            inchar = 'h';

            while (1)
            {
                if (inchar != 'h')
                    inchar = get_menu_input(10);

                uint8_t pin_inversion_mask = settings.pin_inversion_mask;

                switch (inchar)
                {
                case 'p':
                    print_pin_inversion_mask();
                    break;

                case 'h':
                    print_pin_inversion_mask_menu();
                    break;

                case 'm':
                {
                    char pin_inversion_mask_str[9] = "";
                    int str_len = 0;

                    Serial.print("  Enter pin inversion mask: ");

                    while (1)
                    {
                        inchar = get_menu_input(10);

                        if ((inchar == '0' || inchar == '1') && str_len < 8)
                        {
                            Serial.print(inchar);
                            pin_inversion_mask_str[str_len++] = inchar;
                            pin_inversion_mask_str[str_len] = '\0';
                        }
                        else if (inchar == 8 || inchar == 127) // Backspace
                        {
                            if (str_len > 0)
                            {
                                str_len--;
                                pin_inversion_mask_str[str_len] = '\0';
                                Serial.print("\b \b");
                            }
                        }
                        else if (inchar == '\r' || inchar == '\n')
                        {
                            Serial.println();

                            if (str_len > 0)
                            {
                                uint8_t pin_inversion_mask_int = 0;

                                for (int i = 0; i < str_len; i++)
                                {
                                    pin_inversion_mask_int <<= 1;
                                    pin_inversion_mask_int |= pin_inversion_mask_str[i] == '1' ? 1 : 0;
                                }

                                if (!(pin_inversion_mask_int & ~PIN_INVERSION_MASK))
                                {
                                    settings.pin_inversion_mask = pin_inversion_mask_int;
                                    print_pin_inversion_mask();
                                    break;
                                }
                            }

                            // Reset for retry
                            str_len = 0;
                            pin_inversion_mask_str[0] = '\0';
                            char allowed_mask[9];
                            binary_to_string(PIN_INVERSION_MASK, true, allowed_mask);
                            Serial.print("  Allowed inversion mask ...... ");
                            Serial.println(allowed_mask);
                            Serial.print("  Enter pin inversion mask: ");
                        }
                    }

                    break;
                }

                default:
                    break;
                }

                if (pin_inversion_mask != settings.pin_inversion_mask)
                    set_pin_inversion_mask(settings.pin_inversion_mask);

                if (inchar == 'q')
                {
                    inchar = 'h';
                    break;
                }

                inchar = 0;
            }

            break;
        }

        case 'T':
        {
            inchar = 'h';

            while (1)
            {
                if (inchar != 'h')
                    inchar = get_menu_input(10);

                switch (inchar)
                {
                case 'p':
                    print_settings();
                    break;

                case 'h':
                    print_test_menu();
                    break;

                case 'i':
                    Serial.print("  Current frame count ......... ");
                    Serial.println(frame_count, DEC);
                    break;

                case '1':
                case '2':
                case '3':
                {
                    uint32_t frame_count_tmp = frame_count;

                    sleep_ms(100);

                    if (frame_count == frame_count_tmp) // draw the screen only if capture is not active
                    {
                        Serial.println("  Drawing the screen...");

                        if (inchar == '1')
                            draw_welcome_screen(*(video_modes[settings.video_out_mode]));
                        else if (inchar == '2')
                            draw_welcome_screen_h(*(video_modes[settings.video_out_mode]));
                        else
                            draw_no_signal(*(video_modes[settings.video_out_mode]));
                    }

                    break;
                }

                default:
                    break;
                }

                if (inchar == 'q')
                {
                    inchar = 'h';
                    break;
                }

                inchar = 0;
            }

            break;
        }

        case 'h':
            print_main_menu();
            inchar = 0;
            break;

        case 'w':
            Serial.println("  Saving settings...");
            save_settings(&settings);
            inchar = 0;
            break;

        case 'r':
            Serial.println("  Restarting........");
            sleep_ms(100);
            watchdog_reboot(0, 0, 0);
            break;

        default:
            break;
        }

        if (inchar == 'q')
        {
            inchar = 0;
            Serial.println(" Leaving the configuration mode\n");
            break;
        }
    }
}