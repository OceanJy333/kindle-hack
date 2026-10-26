// input: evdev capabilities and raw contacts; output: logical taps and optional page keys; pos: shared Kindle input adapter.
#include "display.h"
#include <linux/input.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#define BIT_SET(bits,code) ((bits)[(code)/8]&(1U<<((code)%8)))
static struct input_absinfo range_x,range_y;
static int touch_x,touch_y,owned_touch=-1;
static int xcode=ABS_MT_POSITION_X,ycode=ABS_MT_POSITION_Y;
static int open_input(int pagekeys){
    for(int i=0;i<32;i++){
        char path[64];snprintf(path,sizeof(path),"/dev/input/event%d",i);
        int fd=open(path,O_RDONLY|O_NONBLOCK|O_CLOEXEC);if(fd<0)continue;
        unsigned char absbits[(ABS_MAX+8)/8]={0},keys[(KEY_MAX+8)/8]={0};
        ioctl(fd,EVIOCGBIT(EV_KEY,sizeof(keys)),keys);
        if(pagekeys){if(!BIT_SET(keys,KEY_PAGEUP)&&!BIT_SET(keys,KEY_PAGEDOWN)){close(fd);continue;}}
        else{
            ioctl(fd,EVIOCGBIT(EV_ABS,sizeof(absbits)),absbits);
            if(BIT_SET(absbits,ABS_MT_POSITION_X)&&BIT_SET(absbits,ABS_MT_POSITION_Y)){
                xcode=ABS_MT_POSITION_X;ycode=ABS_MT_POSITION_Y;
            }else if(BIT_SET(absbits,ABS_X)&&BIT_SET(absbits,ABS_Y)&&BIT_SET(keys,BTN_TOUCH)){
                xcode=ABS_X;ycode=ABS_Y;
            }else {close(fd);continue;}
            if(ioctl(fd,EVIOCGABS(xcode),&range_x)||ioctl(fd,EVIOCGABS(ycode),&range_y)||range_x.maximum<=range_x.minimum||range_y.maximum<=range_y.minimum){close(fd);continue;}
            touch_x=range_x.value;touch_y=range_y.value;
        }
        if(!getenv("NOTIFY_INPUT")&&ioctl(fd,EVIOCGRAB,1)){close(fd);return -1;}
        fprintf(stderr,"INPUT %s %s",pagekeys?"pagekeys":"touch",path);
        if(!pagekeys)fprintf(stderr," x=%d..%d y=%d..%d",range_x.minimum,range_x.maximum,range_y.minimum,range_y.maximum);
        fprintf(stderr,"\n");return fd;
    }
    errno=ENODEV;return -1;
}
int input_open_touch(void){return open_input(0);}
int input_open_pagekeys(void){return open_input(1);}
int input_to_logical(int raw_x,int raw_y,int *x,int *y){
    return display_geometry_touch(display_geometry(),raw_x,raw_y,range_x.minimum,range_x.maximum,range_y.minimum,range_y.maximum,x,y);
}
static void overlay_pause(int sig){
    (void)sig;if(owned_touch>=0)ioctl(owned_touch,EVIOCGRAB,0);raise(SIGSTOP);
    if(owned_touch>=0)ioctl(owned_touch,EVIOCGRAB,1);
}
int touch_open(void){
    int fd=input_open_touch();if(fd>=0&&!getenv("NOTIFY_INPUT")){owned_touch=fd;signal(SIGUSR1,overlay_pause);}return fd;
}
int touch_read(int fd,int *x,int *y){
    static int down,release,seen,slot;
    struct input_event e;
    while(read(fd,&e,sizeof(e))==sizeof(e)){
        if(e.type==EV_ABS&&e.code==ABS_MT_SLOT){slot=e.value;continue;}
        if(e.type==EV_ABS&&slot==0){
            if(e.code==xcode)touch_x=e.value;if(e.code==ycode)touch_y=e.value;
            if(e.code==ABS_MT_TRACKING_ID){
                if(e.value>=0){
                    int xs[2]={xcode,touch_x},ys[2]={ycode,touch_y};
                    if(!ioctl(fd,EVIOCGMTSLOTS(sizeof(xs)),xs))touch_x=xs[1];
                    if(!ioctl(fd,EVIOCGMTSLOTS(sizeof(ys)),ys))touch_y=ys[1];
                    down=seen=1;
                }else release=down;
            }
        }
        if(e.type==EV_KEY&&e.code==BTN_TOUCH){if(e.value)down=seen=1;else release=down;}
        if(e.type==EV_SYN&&e.code==SYN_DROPPED){down=release=seen=0;}
        if(e.type==EV_SYN&&e.code==SYN_REPORT&&release&&seen){
            down=release=seen=0;
            if(input_to_logical(touch_x,touch_y,x,y)){
                fprintf(stderr,"TAP %d %d raw=%d,%d\n",*x,*y,touch_x,touch_y);return 1;
            }
        }
    }
    return 0;
}
