// input: full 16-bit color space and padded RGB frames; output: byte-exact comparison with the shipped scaler; pos: renderer regression.
#include "video-render.h"
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned char reference(const void *data,unsigned w,unsigned h,size_t pitch,int f,int x,int y){
    const void *row=(const unsigned char*)data+(y*h/ARCADE_VIEW_H)*pitch;unsigned r,g,b;
    if(f==RETRO_PIXEL_FORMAT_XRGB8888){unsigned p=((const uint32_t*)row)[x*w/ARCADE_VIEW_W];r=(p>>16)&255;g=(p>>8)&255;b=p&255;}
    else{unsigned p=((const uint16_t*)row)[x*w/ARCADE_VIEW_W];r=(p>>(f==RETRO_PIXEL_FORMAT_RGB565?11:10))&31;g=(p>>5)&(f==RETRO_PIXEL_FORMAT_RGB565?63:31);b=p&31;r=r*255/31;g=g*255/(f==RETRO_PIXEL_FORMAT_RGB565?63:31);b=b*255/31;}
    const unsigned char bayer[4][4]={{0,8,2,10},{12,4,14,6},{3,11,1,9},{15,7,13,5}};
    return ((r*77+g*150+b*29)>>8)>8+bayer[y&3][x&3]*16?255:0;
}
int main(void){
    unsigned char *out=malloc(ARCADE_VIEW_W*ARCADE_VIEW_H);unsigned cases=0;
    for(int format=0;format<3;format++)for(unsigned shape=0;shape<5;shape++){
        unsigned w=shape==0?256:shape==1?304:shape==2?320:shape==3?1:1024,h=shape==0?256:224,bytes=format==1?4:2;size_t pitch=w*bytes+12;
        unsigned char *data=calloc(pitch,h);
        for(unsigned y=0;y<h;y++)for(unsigned x=0;x<w;x++){
            if(bytes==2)((uint16_t*)(data+y*pitch))[x]=(uint16_t)(y*w+x);
            else((uint32_t*)(data+y*pitch))[x]=(y*w+x)*2654435761U;
        }
        assert(arcade_convert(data,w,h,pitch,format,out)==0);
        for(int y=0;y<ARCADE_VIEW_H;y++)for(int x=0;x<ARCADE_VIEW_W;x++)assert(out[y*ARCADE_VIEW_W+x]==reference(data,w,h,pitch,format,x,y));
        for(int pass=0;pass<3;pass++){
            if(pass==1)data[0]^=0x7f;
            assert(arcade_convert(data,w,h,pitch,format,out)==0);
            for(int y=0;y<ARCADE_VIEW_H;y++)for(int x=0;x<ARCADE_VIEW_W;x++)assert(out[y*ARCADE_VIEW_W+x]==reference(data,w,h,pitch,format,x,y));
        }
        free(data);cases++;
    }
    unsigned char *previous=calloc(ARCADE_VIEW_W,ARCADE_VIEW_H);
    for(int pattern=0;pattern<5;pattern++){
        memset(out,0,ARCADE_VIEW_W*ARCADE_VIEW_H);
        if(pattern==1)out[31*ARCADE_VIEW_W+63]=255;
        if(pattern==2){out[0]=255;out[ARCADE_VIEW_W*ARCADE_VIEW_H-1]=255;}
        if(pattern==3)memset(out,255,ARCADE_VIEW_W*ARCADE_VIEW_H);
        if(pattern==4)for(int i=123;i<ARCADE_VIEW_W*ARCADE_VIEW_H;i+=997)out[i]=255;
        int left=ARCADE_VIEW_W,top=ARCADE_VIEW_H,right=-1,bottom=-1;
        for(int y=0;y<ARCADE_VIEW_H;y++)for(int x=0;x<ARCADE_VIEW_W;x++)if(out[y*ARCADE_VIEW_W+x]!=previous[y*ARCADE_VIEW_W+x]){
            if(x<left)left=x;if(x>right)right=x;if(y<top)top=y;if(y>bottom)bottom=y;
        }
        struct arcade_rect r;int changed=arcade_dirty(out,previous,&r);
        assert(changed==(right>=left));
        if(changed)assert(r.x==left&&r.y==top&&r.w==right-left+1&&r.h==bottom-top+1);
    }
    free(previous);arcade_convert_close();free(out);printf("%u pixel-exact cases and 5 dirty-region cases passed\n",cases);return 0;
}
