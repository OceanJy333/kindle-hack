// input: logical grayscale canvas; output: model-selected EPDC updates; pos: Kindle framebuffer boundary.
#include "display.h"
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/utsname.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <time.h>

// Linux UAPI layout, checked against Amazon's headers consolidated in
// https://github.com/NiLuJe/FBInk/blob/master/eink/mxcfb-kindle.h . No FBInk code is linked.
// PW1 (FW >= 5.1) and PW2/3/Voyage share SEND; PW1's completion ioctl is different.
struct rect { uint32_t top,left,width,height; };
struct alt { uint32_t addr,width,height;struct rect region; };
struct update { struct rect region;uint32_t waveform,mode,marker,bw_waveform,gray_waveform;int32_t temp;uint32_t flags;struct alt alternate; };
struct completion { uint32_t marker,collision; };
_Static_assert(sizeof(struct update)==72,"Kindle EPDC send ABI");
_Static_assert(sizeof(struct completion)==8,"Carta completion ABI");
static int fb=-1,pearl;
static uint8_t *memory,*canvas;
static struct fb_fix_screeninfo fix;
static struct fb_var_screeninfo var;
static struct display_geometry geometry;
static struct display_format format;
static uint32_t submitted_marker;

const struct display_geometry *display_geometry(void){return &geometry;}
int display_width(void){return DISPLAY_WIDTH;}
int display_height(void){return DISPLAY_HEIGHT;}
static int select_profile(void){
    struct utsname kernel={0};uname(&kernel);
    const char *profile=getenv("OCEAN_DEVICE_PROFILE");
    if(profile&&*profile){
        if(!strcmp(profile,"pw1"))pearl=1;
        else if(!strcmp(profile,"pw2")||!strcmp(profile,"pw3")||!strcmp(profile,"voyage"))pearl=0;
        else {errno=ENOTSUP;return -1;}
    }else{
        // These kernels distinguish the mx50 Pearl and mx6sl Carta driver generations.
        if(strncmp(fix.id,"mxc_epdc_fb",11)){errno=ENOTSUP;return -1;}
        if(!strncmp(kernel.release,"2.6.31",6)){profile="pw1";pearl=1;}
        else if(!strncmp(kernel.release,"3.0.35",6)){profile="pw2/pw3/voyage";pearl=0;}
        else {errno=ENOTSUP;return -1;}
    }
    fprintf(stderr,"DISPLAY_PROFILE %s kernel=%s fb=%.16s completion=%s\n",profile,kernel.release,fix.id,pearl?"pearl":"carta");return 0;
}
int display_open(void){
    fb=open("/dev/fb0",O_RDWR|O_CLOEXEC);
    if(fb<0)return -1;
    if(ioctl(fb,FBIOGET_FSCREENINFO,&fix)||ioctl(fb,FBIOGET_VSCREENINFO,&var))goto fail;
    if((var.bits_per_pixel!=8&&var.bits_per_pixel!=16)||display_geometry_init(&geometry,var.xres,var.yres)){errno=ENOTSUP;goto fail;}
    {const char *darkness=getenv("OCEAN_TEXT_DARKNESS");display_geometry_set_darkness(darkness?atoi(darkness):60);}
    format=(struct display_format){var.bits_per_pixel,var.red.offset,var.red.length,var.green.offset,var.green.length,var.blue.offset,var.blue.length,var.grayscale==2};
    if(var.bits_per_pixel==16&&(!var.green.length||var.red.offset+var.red.length>16||var.green.offset+var.green.length>16||var.blue.offset+var.blue.length>16)){errno=ENOTSUP;goto fail;}
    if((size_t)(var.xres+var.xoffset)*(var.bits_per_pixel/8)>fix.line_length||(size_t)(var.yres+var.yoffset)*fix.line_length>fix.smem_len){errno=EINVAL;goto fail;}
    if(select_profile())goto fail;
    memory=mmap(NULL,fix.smem_len,PROT_READ|PROT_WRITE,MAP_SHARED,fb,0);
    if(memory==MAP_FAILED){memory=NULL;goto fail;}
    canvas=malloc(DISPLAY_WIDTH*DISPLAY_HEIGHT);if(!canvas)goto fail;
    // Start with the visible screen, so a notification can restore what was under it.
    display_geometry_capture(&geometry,&format,memory,fix.line_length,var.xoffset,var.yoffset,canvas);
    fprintf(stderr,"DISPLAY %ux%u stride=%u bpp=%u offset=%u,%u canvas=%dx%d viewport=%d,%d,%d,%d\n",var.xres,var.yres,fix.line_length,var.bits_per_pixel,var.xoffset,var.yoffset,DISPLAY_WIDTH,DISPLAY_HEIGHT,geometry.x,geometry.y,geometry.w,geometry.h);
    return 0;
fail:{int error=errno;display_close();errno=error;return -1;}
}
int display_update(uint32_t seq,int x,int y,int w,int h,int full,const uint8_t *pixels){
    return display_update_stride(seq,x,y,w,h,full,pixels,w>0?(size_t)w:0);
}
int display_update_stride(uint32_t seq,int x,int y,int w,int h,int full,const uint8_t *pixels,size_t pitch){
    struct display_rect r;
    if(fb<0||!pixels||w<=0||pitch<(size_t)w||display_geometry_rect(&geometry,x,y,w,h,&r)){errno=EINVAL;return -1;}
    display_canvas_write(canvas,x,y,w,h,pixels,pitch);
    if(x==0&&y==0&&w==DISPLAY_WIDTH&&h==DISPLAY_HEIGHT)display_geometry_clear_margins(&geometry,&format,memory,fix.line_length,var.xoffset,var.yoffset);
    display_geometry_render(&geometry,&format,memory,fix.line_length,var.xoffset,var.yoffset,canvas,r);
    return display_refresh(seq,x,y,w,h,full);
}
static int refresh_region(uint32_t seq,struct display_rect r,int full);
int display_panel_size(int *w,int *h){
    struct fb_var_screeninfo v;struct display_geometry g;
    int probe=open("/dev/fb0",O_RDONLY|O_CLOEXEC);
    if(probe<0)return -1;
    int bad=ioctl(probe,FBIOGET_VSCREENINFO,&v)||display_geometry_init(&g,v.xres,v.yres);
    close(probe);if(bad)return -1;
    *w=g.w;*h=g.h;return 0;
}
int display_panel_width(void){return geometry.w;}
int display_panel_height(void){return geometry.h;}
// Pixels are in viewport coordinates and reach the panel 1:1, so a UI rendered at panel size is not resampled.
int display_update_panel(uint32_t seq,int x,int y,int w,int h,int full,const uint8_t *pixels,size_t pitch){
    if(fb<0||!pixels||w<=0||h<=0||pitch<(size_t)w||x<0||y<0||x>geometry.w-w||y>geometry.h-h){errno=EINVAL;return -1;}
    if(x==0&&y==0&&w==geometry.w&&h==geometry.h)display_geometry_clear_margins(&geometry,&format,memory,fix.line_length,var.xoffset,var.yoffset);
    display_geometry_blit(&geometry,&format,memory,fix.line_length,var.xoffset,var.yoffset,x,y,w,h,pixels,pitch);
    return refresh_region(seq,(struct display_rect){geometry.x+x,geometry.y+y,w,h},full);
}
int display_wait(void){
    if(fb<0||!submitted_marker)return 0;
    if(pearl){uint32_t marker=submitted_marker;return ioctl(fb,_IOW('F',0x2f,uint32_t),&marker)<0?-1:0;}
    struct completion done={submitted_marker,0};
    // Success may be remaining timeout ticks rather than zero.
    return ioctl(fb,_IOWR('F',0x2f,struct completion),&done)<0?-1:0;
}
static int refresh_region(uint32_t seq,struct display_rect r,int full){
    if(!r.w||!r.h)return 0;
    static uint32_t marker;(void)seq;
    struct update u={0};u.region=(struct rect){r.y,r.x,r.w,r.h};
    u.waveform=full==DISPLAY_FAST_MONO?4:full==DISPLAY_MONO?1:full?2:257;
    u.flags=full==DISPLAY_FAST_MONO?0x02:0;u.bw_waveform=1;u.gray_waveform=2;
    u.mode=full==DISPLAY_CLEAN?1:0;u.marker=++marker?marker:++marker;u.temp=0x1001;
    if(ioctl(fb,_IOW('F',0x2e,struct update),&u)<0)return -1;
    submitted_marker=u.marker;
    if(getenv("DISPLAY_SYNC")){
        struct timespec a,b;clock_gettime(CLOCK_MONOTONIC,&a);int result=display_wait();clock_gettime(CLOCK_MONOTONIC,&b);
        long long elapsed=(b.tv_sec-a.tv_sec)*1000000LL+(b.tv_nsec-a.tv_nsec)/1000;
        fprintf(stderr,"DISPLAY_COMPLETE marker=%u wait_us=%lld result=%d\n",u.marker,elapsed,result);return result;
    }
    return 0;
}
int display_refresh(uint32_t seq,int x,int y,int w,int h,int full){
    struct display_rect r;
    if(fb<0||display_geometry_rect(&geometry,x,y,w,h,&r)){errno=EINVAL;return -1;}
    if(x==0&&y==0&&w==DISPLAY_WIDTH&&h==DISPLAY_HEIGHT)r=(struct display_rect){0,0,geometry.width,geometry.height};
    return refresh_region(seq,r,full);
}
int display_copy(uint8_t *pixels){
    if(fb<0||!pixels)return -1;
    display_geometry_capture(&geometry,&format,memory,fix.line_length,var.xoffset,var.yoffset,pixels);return 0;
}
void display_close(void){
    free(canvas);canvas=NULL;if(memory)munmap(memory,fix.smem_len);memory=NULL;
    if(fb>=0)close(fb);fb=-1;submitted_marker=0;
}
