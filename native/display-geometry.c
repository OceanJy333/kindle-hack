// input: fixed gray canvas and panel geometry; output: centered pixels and inverse touch mapping; pos: hardware-independent scaler.
#include "display-geometry.h"
#include <string.h>
static int ceil_scale(int value,int dest,int source){return (int)(((int64_t)value*dest+source-1)/source);}
static int sample(int value,int dest,int source){return dest>1?value*(source-1)/(dest-1):0;}
// Smaller panels average the covered logical pixels; nearest-neighbour sampling drops thin glyph strokes.
static int shrinks(const struct display_geometry *g){return g->w<DISPLAY_WIDTH&&g->h<DISPLAY_HEIGHT;}
static int overlap(int d,int dest,int s,int source){
    int64_t lo=(int64_t)d*source,hi=lo+source,from=(int64_t)s*dest,to=from+dest;
    if(from>lo)lo=from;if(to<hi)hi=to;return hi>lo?(int)(hi-lo):0;
}
static uint8_t area_gray(const uint8_t *canvas,int dx,int dy,int dw,int dh){
    int x0=(int)((int64_t)dx*DISPLAY_WIDTH/dw),x1=(int)(((int64_t)(dx+1)*DISPLAY_WIDTH+dw-1)/dw);
    int y0=(int)((int64_t)dy*DISPLAY_HEIGHT/dh),y1=(int)(((int64_t)(dy+1)*DISPLAY_HEIGHT+dh-1)/dh);
    if(x1>DISPLAY_WIDTH)x1=DISPLAY_WIDTH;if(y1>DISPLAY_HEIGHT)y1=DISPLAY_HEIGHT;
    uint64_t sum=0;
    for(int y=y0;y<y1;y++){
        int wy=overlap(dy,dh,y,DISPLAY_HEIGHT);uint64_t row=0;
        for(int x=x0;x<x1;x++)row+=(uint64_t)canvas[y*DISPLAY_WIDTH+x]*overlap(dx,dw,x,DISPLAY_WIDTH);
        sum+=row*wy;
    }
    return (uint8_t)((sum+((uint64_t)DISPLAY_WIDTH*DISPLAY_HEIGHT)/2)/((uint64_t)DISPLAY_WIDTH*DISPLAY_HEIGHT));
}
int display_geometry_init(struct display_geometry *g,int width,int height){
    if(width<2||height<2)return -1;
    *g=(struct display_geometry){.width=width,.height=height,.w=width,.h=height};
    if((int64_t)width*DISPLAY_HEIGHT>(int64_t)height*DISPLAY_WIDTH)g->w=height*DISPLAY_WIDTH/DISPLAY_HEIGHT;
    else g->h=width*DISPLAY_HEIGHT/DISPLAY_WIDTH;
    g->x=(width-g->w)/2;g->y=(height-g->h)/2;return 0;
}
int display_geometry_rect(const struct display_geometry *g,int x,int y,int w,int h,struct display_rect *r){
    if(x<0||y<0||w<=0||h<=0||x>DISPLAY_WIDTH-w||y>DISPLAY_HEIGHT-h)return -1;
    int left=ceil_scale(x,g->w-1,DISPLAY_WIDTH-1),top=ceil_scale(y,g->h-1,DISPLAY_HEIGHT-1);
    int right=ceil_scale(x+w,g->w-1,DISPLAY_WIDTH-1),bottom=ceil_scale(y+h,g->h-1,DISPLAY_HEIGHT-1);
    if(shrinks(g)){
        left=(int)((int64_t)x*g->w/DISPLAY_WIDTH);top=(int)((int64_t)y*g->h/DISPLAY_HEIGHT);
        right=ceil_scale(x+w,g->w,DISPLAY_WIDTH);bottom=ceil_scale(y+h,g->h,DISPLAY_HEIGHT);
    }
    if(right>g->w)right=g->w;if(bottom>g->h)bottom=g->h;
    *r=(struct display_rect){g->x+left,g->y+top,right-left,bottom-top};return 0;
}
int display_geometry_touch(const struct display_geometry *g,int raw_x,int raw_y,int min_x,int max_x,int min_y,int max_y,int *x,int *y){
    if(max_x<=min_x||max_y<=min_y)return 0;
    int px=(int)((int64_t)(raw_x-min_x)*(g->width-1)/(max_x-min_x));
    int py=(int)((int64_t)(raw_y-min_y)*(g->height-1)/(max_y-min_y));
    if(px<g->x||py<g->y||px>=g->x+g->w||py>=g->y+g->h)return 0;
    *x=sample(px-g->x,g->w,DISPLAY_WIDTH);*y=sample(py-g->y,g->h,DISPLAY_HEIGHT);return 1;
}
static uint16_t channel(uint8_t gray,unsigned offset,unsigned length){
    return length?(uint16_t)(((gray*((1U<<length)-1)+127)/255)<<offset):0;
}
void display_canvas_write(uint8_t *canvas,int x,int y,int w,int h,const uint8_t *pixels,size_t pitch){
    for(int row=0;row<h;row++)memcpy(canvas+(y+row)*DISPLAY_WIDTH+x,pixels+row*pitch,w);
}
void display_geometry_clear_margins(const struct display_geometry *g,const struct display_format *f,uint8_t *fb,size_t stride,int xoffset,int yoffset){
    if(g->w==g->width&&g->h==g->height)return;
    uint8_t gray=f->inverted?0:255;
    uint16_t white=channel(gray,f->red_offset,f->red_length)|channel(gray,f->green_offset,f->green_length)|channel(gray,f->blue_offset,f->blue_length);
    for(int y=0;y<g->height;y++)for(int x=0;x<g->width;x++){
        if(x>=g->x&&x<g->x+g->w&&y>=g->y&&y<g->y+g->h)continue;
        uint8_t *out=fb+(y+yoffset)*stride+(x+xoffset)*(f->bpp/8);
        if(f->bpp==8)*out=gray;else memcpy(out,&white,2);
    }
}
void display_geometry_capture(const struct display_geometry *g,const struct display_format *f,const uint8_t *fb,size_t stride,int xoffset,int yoffset,uint8_t *canvas){
    if(g->w==DISPLAY_WIDTH&&g->h==DISPLAY_HEIGHT&&f->bpp==8&&!f->inverted){
        for(int y=0;y<DISPLAY_HEIGHT;y++)memcpy(canvas+y*DISPLAY_WIDTH,fb+(y+g->y+yoffset)*stride+g->x+xoffset,DISPLAY_WIDTH);
        return;
    }
    for(int y=0;y<DISPLAY_HEIGHT;y++)for(int x=0;x<DISPLAY_WIDTH;x++){
        int px=g->x+ceil_scale(x,g->w-1,DISPLAY_WIDTH-1),py=g->y+ceil_scale(y,g->h-1,DISPLAY_HEIGHT-1);
        canvas[y*DISPLAY_WIDTH+x]=display_geometry_gray(f,fb+(py+yoffset)*stride+(px+xoffset)*(f->bpp/8));
    }
}
void display_geometry_render(const struct display_geometry *g,const struct display_format *f,uint8_t *fb,size_t stride,int xoffset,int yoffset,const uint8_t *canvas,struct display_rect r){
    if(g->w==DISPLAY_WIDTH&&g->h==DISPLAY_HEIGHT&&f->bpp==8&&!f->inverted){
        for(int y=r.y;y<r.y+r.h;y++)memcpy(fb+(y+yoffset)*stride+r.x+xoffset,canvas+(y-g->y)*DISPLAY_WIDTH+r.x-g->x,r.w);
        return;
    }
    for(int py=r.y;py<r.y+r.h;py++){
        uint8_t *out=fb+(py+yoffset)*stride+(r.x+xoffset)*(f->bpp/8);
        int sy=sample(py-g->y,g->h,DISPLAY_HEIGHT);
        for(int px=r.x;px<r.x+r.w;px++){
            uint8_t gray=shrinks(g)?area_gray(canvas,px-g->x,py-g->y,g->w,g->h):canvas[sy*DISPLAY_WIDTH+sample(px-g->x,g->w,DISPLAY_WIDTH)];
            if(f->inverted)gray=255-gray;
            if(f->bpp==8)*out++=gray;
            else {uint16_t value=channel(gray,f->red_offset,f->red_length)|channel(gray,f->green_offset,f->green_length)|channel(gray,f->blue_offset,f->blue_length);memcpy(out,&value,2);out+=2;}
        }
    }
}
uint8_t display_geometry_gray(const struct display_format *f,const uint8_t *pixel){
    unsigned gray=*pixel;
    if(f->bpp==16){uint16_t value;memcpy(&value,pixel,2);unsigned mask=(1U<<f->green_length)-1;gray=((value>>f->green_offset)&mask)*255/mask;}
    return f->inverted?255-gray:gray;
}
