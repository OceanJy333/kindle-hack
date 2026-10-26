// input: mocked Kindle framebuffer/ioctls; output: model-specific completion and partial-update assertions; pos: Linux hardware-boundary regression.
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/utsname.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdarg.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int mock_open(const char *path,int flags,...);
static int mock_ioctl(int fd,unsigned long request,...);
static void *mock_mmap(void *addr,size_t length,int prot,int flags,int fd,off_t offset);
static int mock_munmap(void *addr,size_t length);
static int mock_close(int fd);
static int mock_uname(struct utsname *name);
#define open mock_open
#define ioctl mock_ioctl
#define mmap mock_mmap
#define munmap mock_munmap
#define close mock_close
#define uname mock_uname
#include "display.c"
#undef open
#undef ioctl
#undef mmap
#undef munmap
#undef close
#undef uname
static struct fb_var_screeninfo fake_var;
static struct fb_fix_screeninfo fake_fix;
static unsigned char pixels[4*1024*1024];
static struct update last_update;
static unsigned sends,waits,last_wait_size;
static int fail_wait;
static const char *release="3.0.35-ocean-voyage";
static int mock_open(const char *path,int flags,...){(void)flags;assert(!strcmp(path,"/dev/fb0"));return 7;}
static int mock_close(int fd){assert(fd==7);return 0;}
static int mock_uname(struct utsname *name){memset(name,0,sizeof(*name));strcpy(name->release,release);return 0;}
static void *mock_mmap(void *addr,size_t length,int prot,int flags,int fd,off_t offset){(void)addr;(void)prot;(void)flags;assert(fd==7&&offset==0&&length<=sizeof(pixels));return pixels;}
static int mock_munmap(void *addr,size_t length){assert(addr==pixels&&length<=sizeof(pixels));return 0;}
static int mock_ioctl(int fd,unsigned long request,...){
    assert(fd==7);va_list args;va_start(args,request);void *value=va_arg(args,void*);va_end(args);
    if(request==FBIOGET_FSCREENINFO){memcpy(value,&fake_fix,sizeof(fake_fix));return 0;}
    if(request==FBIOGET_VSCREENINFO){memcpy(value,&fake_var,sizeof(fake_var));return 0;}
    if(request==_IOW('F',0x2e,struct update)){last_update=*(struct update*)value;sends++;return 0;}
    assert(request==_IOW('F',0x2f,uint32_t)||request==_IOWR('F',0x2f,struct completion));
    last_wait_size=(request>>16)&0x3fff;waits++;assert(*(uint32_t*)value==last_update.marker);
    if(last_wait_size==8)assert(((struct completion*)value)->collision==0);
    if(fail_wait){errno=EIO;return -1;}return 17;
}
static void panel(const char *profile,int width,int height,int bpp,unsigned expected_wait){
    memset(&fake_var,0,sizeof(fake_var));memset(&fake_fix,0,sizeof(fake_fix));
    fake_var.xres=width;fake_var.yres=height;fake_var.bits_per_pixel=bpp;fake_var.xoffset=3;fake_var.yoffset=2;
    fake_var.red=(struct fb_bitfield){11,5,0};fake_var.green=(struct fb_bitfield){5,6,0};fake_var.blue=(struct fb_bitfield){0,5,0};
    strcpy(fake_fix.id,"mxc_epdc_fb");fake_fix.line_length=(width+3)*(bpp/8)+13;fake_fix.smem_len=fake_fix.line_length*(height+2);
    if(profile)setenv("OCEAN_DEVICE_PROFILE",profile,1);else unsetenv("OCEAN_DEVICE_PROFILE");
    memset(pixels,255,sizeof(pixels));assert(!display_open());assert(display_width()==1072&&display_height()==1448);
    unsigned char *frame=malloc(DISPLAY_WIDTH*DISPLAY_HEIGHT);assert(frame);memset(frame,128,DISPLAY_WIDTH*DISPLAY_HEIGHT);
    setenv("DISPLAY_SYNC","1",1);unsigned before=sends;
    assert(!display_update(999,0,0,DISPLAY_WIDTH,DISPLAY_HEIGHT,DISPLAY_CLEAN,frame));assert(sends==before+1);
    assert(last_update.waveform==2&&last_update.mode==1&&last_update.bw_waveform==1&&last_update.gray_waveform==2&&last_update.temp==0x1001);
    assert(last_update.region.left==0&&last_update.region.top==0&&last_update.region.width==(unsigned)width&&last_update.region.height==(unsigned)height);
    assert(last_wait_size==expected_wait);uint32_t marker=last_update.marker;
    unsigned char partial[7*5];memset(partial,0,sizeof(partial));
    assert(!display_update_stride(999,56,192,3,5,DISPLAY_FAST_MONO,partial,7));
    struct display_rect rect;assert(!display_geometry_rect(display_geometry(),56,192,3,5,&rect));
    assert(last_update.region.left==(unsigned)rect.x&&last_update.region.top==(unsigned)rect.y&&last_update.region.width==(unsigned)rect.w&&last_update.region.height==(unsigned)rect.h);
    assert(last_update.marker!=marker&&last_update.waveform==4&&last_update.flags==2&&last_update.mode==0);
    assert(!display_update(1,0,0,1,1,DISPLAY_GRAY,frame));assert(last_update.waveform==2&&last_update.mode==0);
    assert(!display_update(1,0,0,1,1,DISPLAY_MONO,frame));assert(last_update.waveform==1&&last_update.mode==0);
    assert(!display_update(1,0,0,1,1,DISPLAY_AUTO,frame));assert(last_update.waveform==257&&last_update.mode==0);
    unsetenv("DISPLAY_SYNC");before=waits;assert(!display_update(1,0,0,1,1,0,frame));assert(waits==before);
    assert(!display_wait());assert(waits==before+1);fail_wait=1;assert(display_wait()<0);fail_wait=0;
    // Another process drew after display_open. Snapshot must capture that current physical image.
    memset(pixels,0,fake_fix.smem_len);assert(!display_copy(frame));for(int i=0;i<DISPLAY_WIDTH*DISPLAY_HEIGHT;i++)assert(frame[i]==0);
    before=sends;assert(display_update_stride(1,0,0,4,1,0,frame,3)<0);assert(sends==before);
    free(frame);display_close();printf("PASS profile=%s %dx%d %dbpp completion=%u bytes\n",profile?profile:"auto",width,height,bpp,last_wait_size);
}
int main(void){
    _Static_assert(_IOW('F',0x2e,struct update)==0x4048462e,"send request");
    _Static_assert(_IOW('F',0x2f,uint32_t)==0x4004462f,"PW1 wait request");
    _Static_assert(_IOWR('F',0x2f,struct completion)==0xc008462f,"Carta wait request");
    panel("pw1",758,1024,8,4);panel("pw2",758,1024,16,8);panel("pw3",1072,1448,16,8);panel("voyage",1072,1448,8,8);
    panel("pw2",600,800,8,8);release="2.6.31-rt11-lab126";panel(NULL,758,1024,8,4);
    release="3.0.35-lab126";panel(NULL,1072,1448,8,8);setenv("OCEAN_DEVICE_PROFILE","unknown",1);assert(display_open()<0);
    unsetenv("OCEAN_DEVICE_PROFILE");release="6.0-unknown";assert(display_open()<0);return 0;
}
