// input: Voyage touch / PagePress; output: held Doom keys and clean exit; pos: native game controls.
#include "input.h"
#include "../games/display-worker.h"
#include "controls.h"
#include "doomgeneric.h"
#include "../native/display.h"
#include <linux/input.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <poll.h>
#include <signal.h>

int game_running=1;
static int touch=-1,keys=-1,slot,type_b;
static void overlay_pause(int sig){
    (void)sig;if(touch>=0)ioctl(touch,EVIOCGRAB,0);if(keys>=0)ioctl(keys,EVIOCGRAB,0);
    game_display_pause(1);raise(SIGSTOP);game_display_pause(0);
    if(touch>=0)ioctl(touch,EVIOCGRAB,1);if(keys>=0)ioctl(keys,EVIOCGRAB,1);
}
static int packet_id=-1,packet_x,packet_y;
#define TOUCH_SLOTS 10
static struct {int x,y,down,button,id,seen;} contacts[TOUCH_SLOTS];
static unsigned char owners[256],physical[256];
static unsigned short queue[128];
static unsigned head,tail,deadline[256],pressed_at[256];
static unsigned char held[256];
static void push(int press,unsigned char key){
    unsigned next=(head+1)%128;
    if(next==tail)return;
    queue[head]=(press?256:0)|key;head=next;
}
static void key_down(unsigned char key){
    owners[key]++;deadline[key]=0;
    if(held[key])return;
    held[key]=1;pressed_at[key]=DG_GetTicksMs();push(1,key);
    fprintf(stderr,"DOOM_KEY %u down\n",key);
}
static void key_up(unsigned char key){
    if(owners[key])owners[key]--;
    if(owners[key]||!held[key])return;
    // Short taps must remain held across at least two game tics, including a release queued in the same evdev batch.
    unsigned now=DG_GetTicksMs();
    if(now-pressed_at[key]<100){deadline[key]=pressed_at[key]+100;return;}
    held[key]=0;deadline[key]=0;push(0,key);fprintf(stderr,"DOOM_KEY %u up\n",key);
}
static int open_named(const char *needle){
    for(int i=0;i<16;i++){
        char path[64],name[128]={0};snprintf(path,sizeof(path),"/dev/input/event%d",i);
        int fd=open(path,O_RDONLY|O_NONBLOCK);if(fd<0)continue;
        ioctl(fd,EVIOCGNAME(sizeof(name)),name);
        if(strstr(name,needle)&&!ioctl(fd,EVIOCGRAB,1)){signal(SIGUSR1,overlay_pause);return fd;}
        close(fd);
    }
    return -1;
}
int game_input_open(void){
    for(int i=0;i<TOUCH_SLOTS;i++){contacts[i].button=-1;contacts[i].id=-1;}
    touch=open_named("cyttsp4_mt");keys=open_named("fsr_keypad");
    unsigned char bits[(ABS_MAX+8)/8]={0};
    if(touch>=0)ioctl(touch,EVIOCGBIT(EV_ABS,sizeof(bits)),bits);
    type_b=!!(bits[ABS_MT_SLOT/8]&(1<<(ABS_MT_SLOT%8)));
    fprintf(stderr,"DOOM_INPUT touch=%d pagepress=%d protocol=%s\n",touch,keys,type_b?"TypeB":"TypeA");
    return touch<0?-1:0;
}
static int at(int x,int y){
    for(int i=0;i<GAME_BUTTON_COUNT;i++){
        const struct game_button *b=&game_buttons[i];
        if(x>=b->x&&x<b->x+b->w&&y>=b->y&&y<b->y+b->h)return i;
    }
    return -1;
}
static void apply_contacts(void){
    for(int i=0;i<TOUCH_SLOTS;i++){
        int next=contacts[i].down?at(contacts[i].x,contacts[i].y):-1,old=contacts[i].button;
        if(next==old)continue;
        if(old>=0&&game_buttons[old].key)key_up(game_buttons[old].key);
        contacts[i].button=next;
        if(next>=0){
            unsigned char key=game_buttons[next].key;
            if(!key){game_running=0;fprintf(stderr,"DOOM_EXIT_TOUCH\n");}
            else key_down(key);
        }
    }
}
static void type_a_contact(void){
    if(packet_id<0)return;
    int index=-1;
    for(int i=0;i<TOUCH_SLOTS;i++)if(contacts[i].id==packet_id){index=i;break;}
    if(index<0)for(int i=0;i<TOUCH_SLOTS;i++)if(!contacts[i].down&&!contacts[i].seen){index=i;break;}
    if(index>=0){contacts[index].id=packet_id;contacts[index].x=packet_x;contacts[index].y=packet_y;contacts[index].seen=1;}
    packet_id=-1;
}
static void touch_events(void){
    struct input_event e;
    while(read(touch,&e,sizeof(e))==sizeof(e)){
        if(getenv("DOOM_INPUT_TRACE"))fprintf(stderr,"DOOM_EV %u %u %d\n",e.type,e.code,e.value);
        if(e.type==EV_ABS){
            if(type_b){
                if(e.code==ABS_MT_SLOT){if(e.value>=0&&e.value<TOUCH_SLOTS)slot=e.value;continue;}
                if(e.code==ABS_MT_POSITION_X)contacts[slot].x=e.value;
                if(e.code==ABS_MT_POSITION_Y)contacts[slot].y=e.value;
                if(e.code==ABS_MT_TRACKING_ID)contacts[slot].down=e.value>=0;
            }else{
                if(e.code==ABS_MT_POSITION_X)packet_x=e.value;
                if(e.code==ABS_MT_POSITION_Y)packet_y=e.value;
                if(e.code==ABS_MT_TRACKING_ID)packet_id=e.value;
            }
        }
        if(e.type==EV_KEY&&e.code==BTN_TOUCH&&!e.value){
            packet_id=-1;for(int i=0;i<TOUCH_SLOTS;i++){contacts[i].down=0;contacts[i].seen=0;}
        }
        if(e.type==EV_SYN&&e.code==SYN_DROPPED){
            packet_id=-1;for(int i=0;i<TOUCH_SLOTS;i++){contacts[i].down=0;contacts[i].seen=0;}apply_contacts();
        }
        if(!type_b&&e.type==EV_SYN&&e.code==SYN_MT_REPORT)type_a_contact();
        if(e.type!=EV_SYN||e.code!=SYN_REPORT)continue;
        if(!type_b){
            type_a_contact();
            for(int i=0;i<TOUCH_SLOTS;i++){contacts[i].down=contacts[i].seen;contacts[i].seen=0;}
        }
        apply_contacts();
    }
}
int game_input_key(int *pressed,unsigned char *key){
    touch_events();struct input_event e;
    while(keys>=0&&read(keys,&e,sizeof(e))==sizeof(e)){
        unsigned char k=e.code==KEY_PAGEUP?KEY_USE:e.code==KEY_PAGEDOWN?KEY_FIRE:0;
        if(e.type==EV_SYN&&e.code==SYN_DROPPED){for(unsigned k=0;k<256;k++)if(physical[k]){physical[k]=0;key_up(k);}}
        if(e.type==EV_KEY&&k){if(e.value==1&&!physical[k]){physical[k]=1;key_down(k);}else if(e.value==0&&physical[k]){physical[k]=0;key_up(k);}}
    }
    unsigned now=DG_GetTicksMs();
    for(unsigned k=0;k<256;k++)if(deadline[k]&&!owners[k]&&(int)(now-deadline[k])>=0){deadline[k]=0;held[k]=0;push(0,k);fprintf(stderr,"DOOM_KEY %u up\n",k);}
    if(head==tail)return 0;
    unsigned value=queue[tail];tail=(tail+1)%128;*pressed=!!(value&256);*key=value&255;return 1;
}
void game_input_close(void){if(touch>=0)close(touch);if(keys>=0)close(keys);}
void game_input_wait(int timeout_ms){struct pollfd fds[2]={{touch,POLLIN,0},{keys,POLLIN,0}};poll(fds,2,timeout_ms);}
