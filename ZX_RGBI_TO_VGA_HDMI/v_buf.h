#pragma once

void *get_v_buf_out();
void *get_v_buf_in();
void set_v_buf_buffering_mode(bool);
void clear_video_buffers();

void draw_welcome_screen(video_mode_t);
void draw_welcome_screen_h(video_mode_t);
void draw_no_signal(video_mode_t);
