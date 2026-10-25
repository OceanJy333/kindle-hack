// input: power key and local socket commands; output: frontlight, lock state and suspend; pos: independent Voyage power service.
#include "hardware.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <linux/input.h>
#include <poll.h>
#include <signal.h>
#define SOCKET "/tmp/ocean-power.sock"
#define CLIENTS 4
static volatile sig_atomic_t running=1;
static int locked=0,light=0,listener=-1,key=-1,ready=0;
static int wake_release=0,retry_ms=1000,last_suspend_error=0;
static long long sleep_at=0,last_sample=0,last_key=0;
static struct {int fd;char buffer[128];size_t used;} clients[CLIENTS];
static void quit(int sig){(void)sig;running=0;}
static void broadcast(void){
 char b[192];int n=snprintf(b,sizeof(b),"{\"locked\":%s,\"brightness\":%d,\"maxBrightness\":%d,\"battery\":%d}\n",locked?"true":"false",light,
 read_number("/sys/class/backlight/max77696-bl/max_brightness",4095),read_number(BATTERY "capacity",-1));
 for(int i=0;i<CLIENTS;i++)if(clients[i].fd>=0){
  if(send(clients[i].fd,b,n,MSG_NOSIGNAL)!=n){close(clients[i].fd);clients[i].fd=-1;clients[i].used=0;}
 }
}
static void unlock(void){locked=0;sleep_at=0;ready=0;retry_ms=1000;last_suspend_error=0;write_number(LIGHT,light);broadcast();}
static void lock_screen(void){
 if(locked)return;locked=1;ready=0;retry_ms=1000;last_suspend_error=0;write_number(LIGHT,0);sleep_at=milliseconds()+10000;broadcast();
}
static void command(const char *line){
 if(!strcmp(line,"sleep"))lock_screen();
 else if(!strcmp(line,"wake"))unlock();
 else if(!strcmp(line,"ready")&&locked){ready=1;sleep_at=milliseconds();}
 else if(!strcmp(line,"status"))broadcast();
 else if(!strncmp(line,"light ",6)){
  char *end;long value=strtol(line+6,&end,10);int max=read_number("/sys/class/backlight/max77696-bl/max_brightness",4095);
  if(!*end&&value>=0&&value<=max){if(locked||!write_number(LIGHT,value)){light=value;broadcast();}}
 }
}
static void try_suspend(void){
 if(!ready){fprintf(stderr,"Screen did not finish locking; suspend cancelled\n");unlock();return;}
 sleep_at=0;
 if(!last_suspend_error)sample("suspend");
 int rc=suspend_memory(),failure=errno;
 if(rc){
  if(failure!=last_suspend_error)fprintf(stderr,"Suspend deferred: %s\n",strerror(failure));
  last_suspend_error=failure;
  // A wakeup_count race cancels this attempt, not the user's locked screen.
  if(failure==EAGAIN||failure==EBUSY||failure==EINTR||failure==EINVAL){
   sleep_at=milliseconds()+retry_ms;
   retry_ms=retry_ms>15000?30000:retry_ms*2;
  }
  return;
 }
 sample("resume");
 // Consume the wake key before unlock so its release cannot immediately relock.
 struct input_event e;while(read(key,&e,sizeof(e))==sizeof(e)){
  if(e.type==EV_KEY&&e.code==KEY_POWER)wake_release=e.value!=0;
 }last_key=milliseconds();unlock();
}
static int open_service(void){
 struct sockaddr_un a={.sun_family=AF_UNIX};snprintf(a.sun_path,sizeof(a.sun_path),"%s",SOCKET);
 listener=socket(AF_UNIX,SOCK_STREAM|SOCK_CLOEXEC|SOCK_NONBLOCK,0);if(listener<0)return -1;
 // A live owner must exit first; never unlink another service's socket.
 int probe=socket(AF_UNIX,SOCK_STREAM|SOCK_CLOEXEC,0);
 if(probe>=0){int live=connect(probe,(struct sockaddr*)&a,sizeof(a));close(probe);if(live==0){errno=EADDRINUSE;return -1;}}
 unlink(SOCKET);if(bind(listener,(struct sockaddr*)&a,sizeof(a))||chmod(SOCKET,0600)||listen(listener,CLIENTS))return -1;
 for(int i=0;i<16;i++){
  char path[64],name[80]={0};snprintf(path,sizeof(path),"/dev/input/event%d",i);
  int fd=open(path,O_RDONLY|O_NONBLOCK|O_CLOEXEC);if(fd<0)continue;
  ioctl(fd,EVIOCGNAME(sizeof(name)),name);
  if(!strcmp(name,"max77696-onkey")){key=fd;return ioctl(key,EVIOCGRAB,1);}
  close(fd);
 }
 errno=ENODEV;return -1;
}
int main(void){
 setvbuf(stdout,NULL,_IOLBF,0);signal(SIGTERM,quit);signal(SIGINT,quit);signal(SIGPIPE,SIG_IGN);
 for(int i=0;i<CLIENTS;i++)clients[i].fd=-1;
 light=read_number(LIGHT,-1);if(light<0||open_service()){perror("power service");return 1;}
 sample("power-service-start");last_sample=milliseconds();
 while(running){
  long long now=milliseconds();int timeout=60000-(now-last_sample);if(timeout<0)timeout=0;
  if(sleep_at){int left=sleep_at-now;if(left<0)left=0;if(left<timeout)timeout=left;}
  struct pollfd p[CLIENTS+2]={{listener,POLLIN,0},{key,POLLIN,0}};
  for(int i=0;i<CLIENTS;i++){p[i+2].fd=clients[i].fd;p[i+2].events=POLLIN;}
  int result=poll(p,CLIENTS+2,timeout);if(result<0&&errno!=EINTR){perror("poll");break;}
  if(!running)break;
  if(p[0].revents&POLLIN){int fd=accept4(listener,NULL,NULL,SOCK_CLOEXEC|SOCK_NONBLOCK);if(fd>=0){int i;for(i=0;i<CLIENTS;i++)if(clients[i].fd<0){clients[i].fd=fd;clients[i].used=0;break;}if(i==CLIENTS)close(fd);else broadcast();}}
  if(p[1].revents&POLLIN){struct input_event e;while(read(key,&e,sizeof(e))==sizeof(e)){
   if(e.type==EV_KEY&&e.code==KEY_POWER&&wake_release){if(e.value==0)wake_release=0;continue;}
   if(e.type==EV_KEY&&e.code==KEY_POWER&&e.value==0&&milliseconds()-last_key>500){last_key=milliseconds();if(locked)unlock();else lock_screen();}
  }}
  for(int i=0;i<CLIENTS;i++)if(p[i+2].revents){
   char b[64];ssize_t n=read(clients[i].fd,b,sizeof(b));
   if(n<=0){if(n==0||errno!=EAGAIN){close(clients[i].fd);clients[i].fd=-1;clients[i].used=0;}continue;}
   for(ssize_t j=0;j<n;j++){
    if(b[j]=='\n'){clients[i].buffer[clients[i].used]=0;command(clients[i].buffer);clients[i].used=0;}
    else if(clients[i].used<sizeof(clients[i].buffer)-1)clients[i].buffer[clients[i].used++]=b[j];
    else {close(clients[i].fd);clients[i].fd=-1;clients[i].used=0;break;}
   }
  }
  now=milliseconds();
  if(locked&&sleep_at&&now>=sleep_at){
   try_suspend();
  }
  if(now-last_sample>=60000){broadcast();last_sample=now;}
 }
 write_number(LIGHT,light);if(key>=0)close(key);if(listener>=0)close(listener);unlink(SOCKET);
 for(int i=0;i<CLIENTS;i++)if(clients[i].fd>=0)close(clients[i].fd);
 return 0;
}
