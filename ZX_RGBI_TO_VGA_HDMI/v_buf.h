#ifndef VBUF_H
#define VBUF_H

void *get_v_buf_out();
void *get_v_buf_in();

void draw_welcome_screen(video_mode_t video_mode);
void draw_welcome_screen_h(video_mode_t video_mode);
void draw_no_signal(video_mode_t video_mode);

#endif