#ifndef VGA_H
#define VGA_H

// sync pulse patterns (positive polarity)
#define NO_SYNC 0b00000000
#define V_SYNC 0b10000000
#define H_SYNC 0b01000000
#define VH_SYNC 0b11000000

void set_vga_scanlines_mode(bool sl_mode);
void start_vga(video_mode_t v_mode);
void stop_vga();
#endif