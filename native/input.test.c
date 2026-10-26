// input: synthetic evdev capability maps and taps; output: device discovery/protocol/coordinate assertions; pos: shared input regression.
#include <linux/input.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdarg.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
static int mock_open(const char *path,int flags,...);
static int mock_ioctl(int fd,unsigned long request,...);
static int mock_close(int fd);
static ssize_t mock_read(int fd,void *buffer,size_t count);
#define open mock_open
#define ioctl mock_ioctl
#define close mock_close
#define read mock_read
#include "input.c"
#undef open
#undef ioctl
#undef close
#undef read
static struct display_geometry test_geometry;
const struct display_geometry *display_geometry(void){return &test_geometry;}
static int protocol,have_pagekeys=1,grabs;
static struct input_event events[32];
static unsigned event_count,event_index;
static void set_bit(void *bits,unsigned code){((unsigned char*)bits)[code/8]|=1U<<(code%8);}
static int mock_open(const char *path,int flags,...){
    (void)flags;int index=-1;assert(sscanf(path,"/dev/input/event%d",&index)==1);
    if(index<0||index>2){errno=ENOENT;return -1;}return index+20;
}
static int mock_close(int fd){assert(fd>=20&&fd<=22);return 0;}
static int mock_ioctl(int fd,unsigned long request,...){
    va_list args;va_start(args,request);
    if(request==EVIOCGRAB){assert(va_arg(args,int)==1);va_end(args);grabs++;return 0;}
    void *value=va_arg(args,void*);va_end(args);
    if(request==EVIOCGBIT(EV_KEY,(KEY_MAX+8)/8)){
        if(fd==20)set_bit(value,KEY_POWER);
        if(fd==21&&have_pagekeys){set_bit(value,KEY_PAGEUP);set_bit(value,KEY_PAGEDOWN);}
        if(fd==22)set_bit(value,BTN_TOUCH);return 0;
    }
    if(request==EVIOCGBIT(EV_ABS,(ABS_MAX+8)/8)){
        if(fd==22){set_bit(value,protocol?ABS_MT_POSITION_X:ABS_X);set_bit(value,protocol?ABS_MT_POSITION_Y:ABS_Y);if(protocol==2)set_bit(value,ABS_MT_SLOT);}return 0;
    }
    if(request==EVIOCGABS(protocol?ABS_MT_POSITION_X:ABS_X)){*(struct input_absinfo*)value=(struct input_absinfo){.minimum=11,.maximum=test_geometry.width+10};return 0;}
    if(request==EVIOCGABS(protocol?ABS_MT_POSITION_Y:ABS_Y)){*(struct input_absinfo*)value=(struct input_absinfo){.minimum=23,.maximum=test_geometry.height+22};return 0;}
    if(request==EVIOCGMTSLOTS(sizeof(int)*2)){errno=EINVAL;return -1;}
    assert(!"unexpected ioctl (device names must not be consulted)");return -1;
}
static ssize_t mock_read(int fd,void *buffer,size_t count){
    assert(fd==22&&count==sizeof(struct input_event));if(event_index==event_count){errno=EAGAIN;return -1;}
    memcpy(buffer,&events[event_index++],count);return count;
}
static void event(unsigned short type,unsigned short code,int value){assert(event_count<32);events[event_count++]=(struct input_event){.type=type,.code=code,.value=value};}
static void tap(int px,int py){
    event_count=event_index=0;
    if(protocol==2)event(EV_ABS,ABS_MT_SLOT,0);
    if(protocol)event(EV_ABS,ABS_MT_TRACKING_ID,37);else event(EV_KEY,BTN_TOUCH,1);
    event(EV_ABS,protocol?ABS_MT_POSITION_X:ABS_X,px+11);event(EV_ABS,protocol?ABS_MT_POSITION_Y:ABS_Y,py+23);
    if(protocol==1)event(EV_SYN,SYN_MT_REPORT,0);event(EV_SYN,SYN_REPORT,0);
    if(protocol)event(EV_ABS,ABS_MT_TRACKING_ID,-1);else event(EV_KEY,BTN_TOUCH,0);
    event(EV_SYN,SYN_REPORT,0);
}
static void panel(int width,int height){
    assert(!display_geometry_init(&test_geometry,width,height));
    for(protocol=0;protocol<3;protocol++){
        assert(input_open_touch()==22);assert(input_open_pagekeys()==21);
        for(int corner=0;corner<4;corner++){
            tap(test_geometry.x+(corner&1?test_geometry.w-1:0),test_geometry.y+(corner&2?test_geometry.h-1:0));
            int x=-1,y=-1;assert(touch_read(22,&x,&y)==1);assert(x==(corner&1?1071:0)&&y==(corner&2?1447:0));
        }
        printf("PASS touch %dx%d protocol=%s\n",width,height,protocol==0?"single":protocol==1?"TypeA":"TypeB");
    }
}
int main(void){
    panel(600,800);panel(758,1024);panel(1072,1448);assert(grabs==18);
    have_pagekeys=0;assert(input_open_pagekeys()<0&&errno==ENODEV);
    setenv("NOTIFY_INPUT","1",1);assert(input_open_touch()==22&&grabs==18);return 0;
}
