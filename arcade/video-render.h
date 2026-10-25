// input: original core pixels; output: exact ordered-dither viewport; pos: arcade CPU rendering boundary.
#pragma once
#include "libretro.h"
#define ARCADE_VIEW_W 960
#define ARCADE_VIEW_H 720
int arcade_convert(const void *data,unsigned width,unsigned height,size_t pitch,enum retro_pixel_format format,unsigned char *out);
void arcade_convert_close(void);
struct arcade_rect { int x,y,w,h; };
int arcade_dirty(const unsigned char *pixels,const unsigned char *previous,struct arcade_rect *rect);
