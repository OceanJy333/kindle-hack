// input: on-device Qt frames, touch and power; output: native e-ink desktop; pos: local-only compositor.
#include "display.h"
#include "../weread/official/rfb-client.h"
#include "../weread/official/pagepress.h"
#include <sys/stat.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <fcntl.h>
#include "../weread/official/power.h"
#include "lock-screen.h"
static volatile sig_atomic_t alive=1;
static void stop(int s){(void)s;alive=0;}
static long long millis(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec*1000LL+t.tv_nsec/1000000;}
static void renderer(int active){int p=renderer_pid();if(p)kill(p,active?SIGCONT:SIGSTOP);}
int main(int argc,char **argv){
 if(argc==2&&!strcmp(argv[1],"--lock-screen")){unsigned s=0;if(display_open())return 1;lock_screen(&s);display_close();return 0;}
 signal(SIGPIPE,SIG_IGN);signal(SIGCHLD,SIG_IGN);signal(SIGINT,stop);signal(SIGTERM,stop);
 if(display_open())return 1;
 int touch=touch_open(),keys=pagepress_open(),power=power_open();if(touch<0)return 1;
 mkfifo("/tmp/agent-desk-refresh",0600);int clean=open("/tmp/agent-desk-refresh",O_RDWR|O_NONBLOCK);
 int asleep=0,fd=-1,full=1,yieldGame=0,px=0,py=0,region=-1,rect[4]={0},cleanFull=0;
 unsigned seq=0;unsigned char packet[8],*sleepFrame=NULL;
 long long reconnect=0,up=0,cleanDue=0;renderer(1);
 while(alive){
  if(!asleep&&fd<0&&millis()>=reconnect){fd=rfb_open("127.0.0.1",5900);full=1;if(fd>=0&&rfb_update(fd,1)){close(fd);fd=-1;}reconnect=millis()+1000;}
  int timeout=-1;if(!asleep&&fd<0)timeout=1000;
  if(up)timeout=(int)(up-millis());if(cleanDue&&(timeout<0||cleanDue-millis()<timeout))timeout=(int)(cleanDue-millis());if(timeout<0&&(up||cleanDue))timeout=0;
  struct pollfd p[4]={{asleep?-1:fd,POLLIN,0},{touch,POLLIN,0},{keys,POLLIN,0},{power,POLLIN,0}};
  struct pollfd f[5]={p[0],p[1],p[2],p[3],{clean,POLLIN,0}};
  if(poll(f,5,timeout)<0){if(errno==EINTR)continue;break;}
  if(f[3].revents&POLLIN){
   int before=asleep;power_read(power,&asleep);if(fd>=0)close(fd);fd=-1;up=0;cleanDue=0;reconnect=0;
   renderer(!asleep);
   if(asleep&&!before){if(!sleepFrame)sleepFrame=malloc((size_t)display_width()*display_height());if(sleepFrame)display_copy(sleepFrame);lock_screen(&seq);}
   else if(!asleep&&before&&sleepFrame){display_update(++seq,0,0,display_width(),display_height(),DISPLAY_CLEAN,sleepFrame);free(sleepFrame);sleepFrame=NULL;}
   continue;
  }
  if(f[4].revents&POLLIN){char bytes[128];ssize_t n;while((n=read(clean,bytes,sizeof(bytes)))>0)for(int i=0;i<n;i++){
   if(region>=0){packet[region++]=bytes[i];if(region==8){for(int j=0;j<4;j++)rect[j]=packet[j*2]*256+packet[j*2+1];cleanDue=millis()+120;region=-1;}}
   else if(bytes[i]=='R')region=0;
   else if(bytes[i]=='C'){cleanFull=1;cleanDue=millis()+120;}
   else if(bytes[i]=='X'){yieldGame=1;alive=0;}
  }}
  if(!alive)break;
  if(f[1].revents&POLLIN){int x,y;while(touch_read(touch,&x,&y)){if(asleep||fd<0)continue;power_activity();if(up)rfb_pointer(fd,px,py,0);px=x;py=y;rfb_pointer(fd,x,y,1);up=millis()+120;}}
  if(f[2].revents&POLLIN){int direction;while((direction=pagepress_read(keys))){if(asleep||fd<0)continue;power_activity();unsigned key=direction>0?0xff56:0xff55;rfb_key(fd,key,1);rfb_key(fd,key,0);}}
  if(fd>=0&&f[0].fd==fd&&(f[0].revents&(POLLIN|POLLERR|POLLHUP))){int result=(f[0].revents&(POLLERR|POLLHUP))?-1:rfb_receive(fd,&seq,full);if(result<0){close(fd);fd=-1;}else if(result){full=0;if(rfb_update(fd,0)){close(fd);fd=-1;}}}
  if(cleanDue&&millis()>=cleanDue){if(!asleep){if(cleanFull)rfb_clean(&seq);else if(rect[2]>0&&rect[3]>0&&rect[0]+rect[2]<=display_width()&&rect[1]+rect[3]<=display_height())rfb_clean_region(&seq,rect[0],rect[1],rect[2],rect[3]);}cleanDue=0;cleanFull=0;}
  if(up&&millis()>=up){if(fd>=0)rfb_pointer(fd,px,py,0);up=0;}
 }
 free(sleepFrame);if(fd>=0)close(fd);power_close();if(!yieldGame)renderer(0);
 if(power>=0)close(power);close(clean);close(touch);if(keys>=0)close(keys);display_close();return 0;
}
