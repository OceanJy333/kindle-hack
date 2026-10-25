// input: touch coordinates and hold time; output: real Type A/B evdev contacts; pos: simulator acceptance helper only.
#include <linux/input.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/ioctl.h>
static int fd,type_b;
static void event(int type,int code,int value){struct input_event e={.type=type,.code=code,.value=value};if(write(fd,&e,sizeof(e))!=sizeof(e)){perror("evdev");exit(1);}}
static void contact(int slot,int x,int y){
    if(type_b)event(EV_ABS,ABS_MT_SLOT,slot);
    event(EV_ABS,ABS_MT_TRACKING_ID,1000+slot);
    // Seed both axes: legacy evdev omits unchanged slot coordinates and lacks the MTSLOTS snapshot ioctl.
    event(EV_ABS,ABS_MT_POSITION_X,x+1);event(EV_ABS,ABS_MT_POSITION_X,x);
    event(EV_ABS,ABS_MT_POSITION_Y,y+1);event(EV_ABS,ABS_MT_POSITION_Y,y);
    if(!type_b)event(EV_SYN,SYN_MT_REPORT,0);
}
int main(int argc,char **argv){
    if(argc!=4&&argc!=6)return 2;
    fd=open("/dev/input/event1",O_RDWR);if(fd<0)return 1;
    unsigned char bits[(ABS_MAX+8)/8]={0};ioctl(fd,EVIOCGBIT(EV_ABS,sizeof(bits)),bits);type_b=!!(bits[ABS_MT_SLOT/8]&(1<<(ABS_MT_SLOT%8)));
    struct input_absinfo info;ioctl(fd,EVIOCGABS(ABS_MT_SLOT),&info);fprintf(stderr,"TEST protocol=%d slots=%d..%d\n",type_b,info.minimum,info.maximum);
    contact(0,atoi(argv[1]),atoi(argv[2]));if(argc==6)contact(1,atoi(argv[3]),atoi(argv[4]));
    event(EV_KEY,BTN_TOUCH,1);event(EV_SYN,SYN_REPORT,0);usleep(atoi(argv[argc-1])*1000);
    if(type_b){
        if(argc==6){event(EV_ABS,ABS_MT_SLOT,1);event(EV_ABS,ABS_MT_TRACKING_ID,-1);}
        event(EV_ABS,ABS_MT_SLOT,0);event(EV_ABS,ABS_MT_TRACKING_ID,-1);
    }else event(EV_SYN,SYN_MT_REPORT,0);
    event(EV_KEY,BTN_TOUCH,0);event(EV_SYN,SYN_REPORT,0);close(fd);return 0;
}
