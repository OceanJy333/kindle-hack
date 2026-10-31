// input: logical rectangles and physical geometry; output: scaled pixels/coordinates; pos: portable display math.
#pragma once
#include <stddef.h>
#include <stdint.h>
#define DISPLAY_WIDTH 1072
#define DISPLAY_HEIGHT 1448
struct display_rect { int x,y,w,h; };
struct display_geometry { int width,height,x,y,w,h; };
struct display_format { unsigned bpp,red_offset,red_length,green_offset,green_length,blue_offset,blue_length,inverted; };
int display_geometry_init(struct display_geometry *g,int width,int height);
int display_geometry_rect(const struct display_geometry *g,int x,int y,int w,int h,struct display_rect *r);
int display_geometry_touch(const struct display_geometry *g,int raw_x,int raw_y,int min_x,int max_x,int min_y,int max_y,int *x,int *y);
void display_canvas_write(uint8_t *canvas,int x,int y,int w,int h,const uint8_t *pixels,size_t pitch);
void display_geometry_capture(const struct display_geometry *g,const struct display_format *f,const uint8_t *fb,size_t stride,int xoffset,int yoffset,uint8_t *canvas);
void display_geometry_clear_margins(const struct display_geometry *g,const struct display_format *f,uint8_t *fb,size_t stride,int xoffset,int yoffset);
void display_geometry_render(const struct display_geometry *g,const struct display_format *f,uint8_t *fb,size_t stride,int xoffset,int yoffset,const uint8_t *canvas,struct display_rect r);
void display_geometry_set_darkness(int percent);
void display_geometry_blit(const struct display_geometry *g,const struct display_format *f,uint8_t *fb,size_t stride,int xoffset,int yoffset,int x,int y,int w,int h,const uint8_t *pixels,size_t pitch);
uint8_t display_geometry_gray(const struct display_format *f,const uint8_t *pixel);
