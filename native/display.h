// input: framebuffer and touch devices; output: pixel updates/taps; pos: Kindle hardware boundary.
#pragma once
#include <stdint.h>
#include <stddef.h>
#include "display-geometry.h"
enum display_mode { DISPLAY_AUTO=0, DISPLAY_CLEAN=1, DISPLAY_GRAY=2, DISPLAY_MONO=3, DISPLAY_FAST_MONO=4 };
int display_open(void);
int display_width(void);
int display_height(void);
int display_refresh(uint32_t seq, int x, int y, int w, int h, int full);
int display_update(uint32_t seq, int x, int y, int w, int h, int full, const uint8_t *pixels);
int display_update_stride(uint32_t seq, int x, int y, int w, int h, int full, const uint8_t *pixels, size_t pitch);
int touch_open(void);
int input_open_touch(void);
int input_open_pagekeys(void);
int input_to_logical(int raw_x,int raw_y,int *x,int *y);
const struct display_geometry *display_geometry(void);
int touch_read(int fd, int *x, int *y);
int display_copy(uint8_t *pixels);
int display_wait(void);
void display_close(void);
