// input: synthetic panels/gray frames; output: scale, stride, dirty-region and touch assertions; pos: portable framebuffer regression.
#include "display-geometry.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void panel(int width,int height,unsigned bpp){
    struct display_geometry g;assert(!display_geometry_init(&g,width,height));
    struct display_format format={.bpp=bpp,.red_offset=11,.red_length=5,.green_offset=5,.green_length=6,.blue_length=5};
    int xoffset=3,yoffset=2,bytes=bpp/8;size_t stride=(width+xoffset)*bytes+17,size=stride*(height+yoffset+1);
    unsigned char *a=malloc(size),*b=malloc(size),*canvas=malloc(DISPLAY_WIDTH*DISPLAY_HEIGHT),*patched=calloc(DISPLAY_WIDTH*DISPLAY_HEIGHT,1),*source=malloc((DISPLAY_WIDTH+19)*DISPLAY_HEIGHT);
    assert(a&&b&&canvas&&patched&&source);memset(a,0xa5,size);memset(b,0xa5,size);
    display_geometry_clear_margins(&g,&format,a,stride,xoffset,yoffset);display_geometry_clear_margins(&g,&format,b,stride,xoffset,yoffset);
    for(int y=0;y<DISPLAY_HEIGHT;y++)for(int x=0;x<DISPLAY_WIDTH;x++)canvas[y*DISPLAY_WIDTH+x]=(unsigned char)(x*7+y*13);
    struct display_rect full;assert(!display_geometry_rect(&g,0,0,DISPLAY_WIDTH,DISPLAY_HEIGHT,&full));
    assert(full.x==g.x&&full.y==g.y&&full.w==g.w&&full.h==g.h);
    display_geometry_render(&g,&format,a,stride,xoffset,yoffset,canvas,full);
    // Every row has padding, every dirty block starts at a different logical sample.
    for(int y=0;y<DISPLAY_HEIGHT;y+=71)for(int x=0;x<DISPLAY_WIDTH;x+=43){
        int w=DISPLAY_WIDTH-x<43?DISPLAY_WIDTH-x:43,h=DISPLAY_HEIGHT-y<71?DISPLAY_HEIGHT-y:71;
        for(int row=0;row<h;row++)memcpy(source+row*(DISPLAY_WIDTH+19),canvas+(y+row)*DISPLAY_WIDTH+x,w);
        display_canvas_write(patched,x,y,w,h,source,DISPLAY_WIDTH+19);
        struct display_rect dirty;assert(!display_geometry_rect(&g,x,y,w,h,&dirty));
        display_geometry_render(&g,&format,b,stride,xoffset,yoffset,patched,dirty);
    }
    assert(!memcmp(canvas,patched,DISPLAY_WIDTH*DISPLAY_HEIGHT));
    assert(!memcmp(a,b,size));
    for(int py=0;py<height;py++)for(int px=0;px<width;px++){
        unsigned char *pixel=a+(py+yoffset)*stride+(px+xoffset)*bytes;
        if(px<g.x||px>=g.x+g.w||py<g.y||py>=g.y+g.h){assert(display_geometry_gray(&format,pixel)==255);continue;}
        int actual=display_geometry_gray(&format,pixel),expected;
        if(g.w<DISPLAY_WIDTH&&g.h<DISPLAY_HEIGHT){
            // Reference box average, weighting each logical pixel by its overlap with this physical pixel.
            double sum=0;int dx=px-g.x,dy=py-g.y;
            double x0=(double)dx*DISPLAY_WIDTH/g.w,x1=(double)(dx+1)*DISPLAY_WIDTH/g.w,y0=(double)dy*DISPLAY_HEIGHT/g.h,y1=(double)(dy+1)*DISPLAY_HEIGHT/g.h;
            for(int y=(int)y0;y<y1&&y<DISPLAY_HEIGHT;y++)for(int x=(int)x0;x<x1&&x<DISPLAY_WIDTH;x++){
                double wx=(x+1<x1?x+1:x1)-(x>x0?x:x0),wy=(y+1<y1?y+1:y1)-(y>y0?y:y0);sum+=canvas[y*DISPLAY_WIDTH+x]*wx*wy;
            }
            expected=(int)(sum/((x1-x0)*(y1-y0))+.5);assert(abs(actual-expected)<=5);
        }else{
            int sx=(px-g.x)*(DISPLAY_WIDTH-1)/(g.w-1),sy=(py-g.y)*(DISPLAY_HEIGHT-1)/(g.h-1);
            expected=canvas[sy*DISPLAY_WIDTH+sx];assert(abs(actual-expected)<=4);
        }
    }
    // Raw axes deliberately have nonzero minima. Viewport corners must hit the same logical corners.
    for(int corner=0;corner<4;corner++){
        int px=g.x+(corner&1?g.w-1:0),py=g.y+(corner&2?g.h-1:0),x=-1,y=-1;
        assert(display_geometry_touch(&g,px-13,py+37,-13,width-14,37,height+36,&x,&y));
        assert(x==(corner&1?DISPLAY_WIDTH-1:0));assert(y==(corner&2?DISPLAY_HEIGHT-1:0));
    }
    if(g.x){int x,y;assert(!display_geometry_touch(&g,0,height/2,0,width-1,0,height-1,&x,&y));}
    if(g.y){int x,y;assert(!display_geometry_touch(&g,width/2,0,0,width-1,0,height-1,&x,&y));}
    assert(display_geometry_rect(&g,-1,0,1,1,&full)<0);assert(display_geometry_rect(&g,0,0,DISPLAY_WIDTH+1,1,&full)<0);
    for(int y=0;y<yoffset;y++)for(size_t x=0;x<stride;x++)assert(a[y*stride+x]==0xa5);
    for(int y=0;y<height;y++)for(size_t x=(width+xoffset)*bytes;x<stride;x++)assert(a[(y+yoffset)*stride+x]==0xa5);
    int shrunk=g.w<DISPLAY_WIDTH&&g.h<DISPLAY_HEIGHT;
    if(shrunk){
        // Averaging blends neighbours of the nearest-neighbour capture, so check that flat gray survives the round trip.
        for(int py=g.y;py<g.y+g.h;py++)for(int px=g.x;px<g.x+g.w;px++){
            size_t index=(py+yoffset)*stride+(px+xoffset)*bytes;
            if(bytes==1)a[index]=117;else{uint16_t value=(uint16_t)((14<<11)|(29<<5)|14);memcpy(a+index,&value,2);}
        }
    }
    display_geometry_capture(&g,&format,a,stride,xoffset,yoffset,patched);
    display_geometry_render(&g,&format,b,stride,xoffset,yoffset,patched,full);
    // RGB565 capture rounds through eight-bit gray: tolerate one channel quantization step.
    for(int py=g.y;py<g.y+g.h;py++)for(int px=g.x;px<g.x+g.w;px++){
        size_t index=(py+yoffset)*stride+(px+xoffset)*bytes;
        assert(abs(display_geometry_gray(&format,a+index)-display_geometry_gray(&format,b+index))<=4);
    }
    free(a);free(b);free(canvas);free(patched);free(source);printf("PASS %dx%d %ubpp viewport=%d,%d %dx%d\n",width,height,bpp,g.x,g.y,g.w,g.h);
}
int main(void){
    panel(600,800,8);panel(600,800,16);panel(758,1024,8);panel(758,1024,16);panel(1072,1448,8);panel(1072,1448,16);
    return 0;
}
