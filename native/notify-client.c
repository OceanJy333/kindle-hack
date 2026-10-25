// input: authenticated handoff frames and power events; output: modal native result view; pos: cross-mode notification owner.
#include "display.h"
#include "board-link.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <poll.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <linux/input.h>
#include "../weread/official/power.h"
static volatile sig_atomic_t alive=1;
static void stop(int s){(void)s;alive=0;}
static unsigned char *saved;
static int paused[3],touch=-1,modal;
static const char *files[]={"/tmp/agent-board.pid","/tmp/weread-login.pid","/tmp/game.pid"};
static const char *names[]={"/tmp/agent-board","weread-login","-voyage"};
static int process(int i){
 FILE *f=fopen(files[i],"r");int pid=0;if(!f)return 0;
 if(fscanf(f,"%d",&pid)!=1)pid=0;fclose(f);if(pid<2)return 0;
 char path[80],cmd[512]={0};snprintf(path,sizeof(path),"/proc/%d/cmdline",pid);
 f=fopen(path,"r");if(!f)return 0;size_t n=fread(cmd,1,sizeof(cmd)-1,f);fclose(f);
 for(size_t k=0;k<n;k++)if(!cmd[k])cmd[k]=' ';
 if(!strstr(cmd,names[i]))return 0;
 snprintf(path,sizeof(path),"/proc/%d/status",pid);f=fopen(path,"r");if(!f)return 0;
 char line[256];int stopped=0;while(fgets(line,sizeof(line),f))if(!strncmp(line,"State:",6)&&strchr(line,'T'))stopped=1;fclose(f);
 return stopped?0:pid;
}
static void show(void){
 if(modal)return;
 for(int i=0;i<3;i++){paused[i]=process(i);if(paused[i])kill(paused[i],i==1?SIGSTOP:SIGUSR1);}
 // Wait briefly for a pending EPDC write to leave the kernel before taking the canvas.
 usleep(150000);
 saved=malloc((size_t)display_width()*display_height());if(saved)display_copy(saved);
 ioctl(touch,EVIOCGRAB,1);modal=1;fprintf(stderr,"NOTICE_SHOW\n");
}
static void hide(int sleeping){
 if(!modal)return;
 if(saved&&!sleeping)display_update(0,0,0,display_width(),display_height(),DISPLAY_CLEAN,saved);
 free(saved);saved=NULL;ioctl(touch,EVIOCGRAB,0);
 for(int i=0;i<3;i++){if(paused[i]&&(!sleeping||i==0))kill(paused[i],SIGCONT);paused[i]=0;}
 modal=0;fprintf(stderr,"NOTICE_HIDE\n");
}
int main(int argc,char **argv){
 if(argc!=3)return 2;
 signal(SIGTERM,stop);signal(SIGINT,stop);signal(SIGPIPE,SIG_IGN);signal(SIGCHLD,SIG_IGN);
 if(display_open())return 1;setenv("NOTIFY_INPUT","1",1);touch=touch_open();if(touch<0)return 1;
 int power=power_open(),asleep=0,fd=-1,connecting=0,retry=0;
 while(alive){
  if(fd<0&&!asleep&&retry<=0){fd=board_open(argv[1],18767,argv[2]);connecting=fd>=0;retry=5000;}
  struct pollfd p[3]={{fd,connecting?POLLOUT:POLLIN,0},{modal?touch:-1,POLLIN,0},{power,POLLIN,0}};
  int timeout=(fd<0&&!asleep)||connecting?5000:-1;
  int r=poll(p,3,timeout);if(r<0&&errno==EINTR)continue;if(r<0)break;
  if(!r){retry=0;if(connecting){close(fd);fd=-1;connecting=0;}continue;}
  if(p[2].revents&POLLIN){power_read(power,&asleep);hide(asleep);if(fd>=0)close(fd);fd=-1;connecting=0;retry=0;continue;}
  if(connecting&&p[0].revents){connecting=0;if(!(p[0].revents&POLLOUT)||board_auth(fd,argv[2],NULL)){close(fd);fd=-1;retry=5000;}continue;}
  if(p[0].revents&(POLLHUP|POLLERR)){hide(0);close(fd);fd=-1;continue;}
  if(p[0].revents&POLLIN){
   if(!modal){unsigned char head[32];int n=recv(fd,head,sizeof(head),MSG_PEEK);if(n==0){close(fd);fd=-1;continue;}if(n<32){usleep(1000);continue;}if(head[28]!=64)show();}
   int result=board_receive(fd);
   if(result==5)hide(0);
   else if(result<0){hide(0);close(fd);fd=-1;}
  }
  if(modal&&(p[1].revents&POLLIN)){int x,y;while(touch_read(touch,&x,&y)){power_activity();if(fd>=0)board_tap(fd,x,y);}}
 }
 hide(0);power_close();if(fd>=0)close(fd);if(power>=0)close(power);close(touch);display_close();return 0;
}
