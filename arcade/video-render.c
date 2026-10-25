// input: original RGB565/1555/8888 frame; output: 960x720 black/white pixels; pos: bounded, pixel-exact arcade scaler.
#include "video-render.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
static unsigned char lut16[65536],*source_gray;
static int *destination_x,*destination_y;
static unsigned char *last_output;
static uint32_t patterns[4][256][4];
static int patterns_ready;
static unsigned map_w,map_h;
static int map_format=-1;
static const unsigned char bayer[4][4]={{0,8,2,10},{12,4,14,6},{3,11,1,9},{15,7,13,5}};
int arcade_convert(const void *data,unsigned w,unsigned h,size_t pitch,enum retro_pixel_format format,unsigned char *out){
    int reset=out!=last_output;
    if(w!=map_w||h!=map_h||map_format!=(int)format){
        unsigned char *next=realloc(source_gray,(size_t)w*h);if(!next)return -1;source_gray=next;
        int *starts=realloc(destination_x,(w+1)*sizeof(int));if(!starts)return -1;destination_x=starts;
        for(unsigned x=0;x<=w;x++)destination_x[x]=(x*ARCADE_VIEW_W+w-1)/w;
        int *rows=realloc(destination_y,(h+1)*sizeof(int));if(!rows)return -1;destination_y=rows;
        for(unsigned y=0;y<=h;y++)destination_y[y]=(y*ARCADE_VIEW_H+h-1)/h;
        reset=1;
        if(format!=RETRO_PIXEL_FORMAT_XRGB8888)for(unsigned p=0;p<65536;p++){
            unsigned r=((p>>(format==RETRO_PIXEL_FORMAT_RGB565?11:10))&31)*255/31;
            unsigned g=((p>>5)&(format==RETRO_PIXEL_FORMAT_RGB565?63:31))*255/(format==RETRO_PIXEL_FORMAT_RGB565?63:31);
            unsigned b=(p&31)*255/31;lut16[p]=(r*77+g*150+b*29)>>8;
        }
        map_w=w;map_h=h;map_format=format;
    }
    if(!patterns_ready){
        for(unsigned y=0;y<4;y++)for(unsigned gray=0;gray<256;gray++)for(unsigned start=0;start<4;start++){
            uint32_t word=0;for(unsigned i=0;i<4;i++)if(gray>8+bayer[y][(start+i)&3]*16)word|=255U<<(i*8);
            patterns[y][gray][start]=word;
        }
        patterns_ready=1;
    }
    // Update only source pixels whose luminance changed. Unchanged scaled pixels stay byte-identical.
    for(unsigned y=0;y<h;y++){
        const void *row=(const unsigned char*)data+y*pitch;
        unsigned char *prior=source_gray+y*w;
        for(unsigned x=0;x<w;x++){
            unsigned g;
            if(format==RETRO_PIXEL_FORMAT_XRGB8888){
                unsigned p=((const uint32_t*)row)[x];g=(((p>>16)&255)*77+((p>>8)&255)*150+(p&255)*29)>>8;
            }else g=lut16[((const uint16_t*)row)[x]];
            if(!reset&&prior[x]==g)continue;
            prior[x]=g;
            int start=destination_x[x],count=destination_x[x+1]-start;
            for(int dy=destination_y[y];dy<destination_y[y+1];dy++){
                uint32_t word=patterns[dy&3][g][start&3];unsigned char *span=out+dy*ARCADE_VIEW_W+start;
                if(count==3){span[0]=word;span[1]=word>>8;span[2]=word>>16;}
                else if(count==4)memcpy(span,&word,4);
                else for(int i=0;i<count;i++)span[i]=word>>((i&3)*8);
            }
        }
    }
    last_output=out;
    return 0;
}
void arcade_convert_close(void){free(source_gray);free(destination_x);free(destination_y);source_gray=NULL;destination_x=NULL;destination_y=NULL;last_output=NULL;map_w=map_h=0;map_format=-1;}

int arcade_dirty(const unsigned char *pixels,const unsigned char *previous,struct arcade_rect *rect){
    int left=ARCADE_VIEW_W,top=ARCADE_VIEW_H,right=-1,bottom=-1;
    for(int y=0;y<ARCADE_VIEW_H;y++){
        const unsigned char *a=pixels+y*ARCADE_VIEW_W,*b=previous+y*ARCADE_VIEW_W;
        if(!memcmp(a,b,ARCADE_VIEW_W))continue;
        // Only the row endpoints affect its bounding box; the changed middle needs no per-pixel branches.
        int first=0,last=ARCADE_VIEW_W-1;
        while(a[first]==b[first])first++;
        while(a[last]==b[last])last--;
        if(first<left)left=first;
        if(last>right)right=last;
        if(y<top)top=y;
        bottom=y;
    }
    if(right<left)return 0;
    *rect=(struct arcade_rect){left,top,right-left+1,bottom-top+1};return 1;
}
