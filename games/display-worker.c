// input: latest game frame in shared memory; output: one completed EPDC update at a time; pos: prevents a stale-frame backlog.
#include "display-worker.h"
#include "../native/display.h"
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
static int view_x,view_y,view_w,view_h;
static size_t size,shared_size;
static const char *wave_env;
static struct shared_frame {volatile int lock;unsigned version;int clean,stop;unsigned char pixels[];} *shared;
static int notify_fd=-1;
static pid_t child=-1;
static void lock_frame(void){while(__sync_lock_test_and_set(&shared->lock,1))usleep(100);}
static void unlock_frame(void){__sync_lock_release(&shared->lock);}
struct dirty_rect {int x,y,w,h;};
static int dirty(const unsigned char *a,const unsigned char *b,struct dirty_rect *r){
    int left=view_w,top=view_h,right=-1,bottom=-1;
    for(int y=0;y<view_h;y++){
        const unsigned char *p=a+y*view_w,*q=b+y*view_w;
        if(!memcmp(p,q,view_w))continue;
        int first=0,last=view_w-1;while(p[first]==q[first])first++;while(p[last]==q[last])last--;
        if(first<left)left=first;if(last>right)right=last;if(y<top)top=y;bottom=y;
    }
    if(right<left)return 0;*r=(struct dirty_rect){left,top,right-left+1,bottom-top+1};return 1;
}
static void worker(int fd){
    unsigned char *current=malloc(size),*previous=calloc(1,size);unsigned seen=0,marker=0;int have=0;
    if(!current||!previous)_exit(2);
    // Only this child touches the framebuffer during gameplay. The parent continues input/emulation.
    const char *wave=getenv(wave_env);
    // "old": the driver-chosen waveform without waiting for the panel, as before the A2 fix; it ghosts on purpose.
    int old=wave&&!strcmp(wave,"old");
    if(!old)setenv("DISPLAY_SYNC","1",1);
    int motion=old?DISPLAY_AUTO:wave&&!strcmp(wave,"du")?DISPLAY_MONO:DISPLAY_FAST_MONO;
    fprintf(stderr,"GAME_PANEL_MODE %s\n",old?"OLD":motion==DISPLAY_FAST_MONO?"A2":"DU");
    for(;;){
        struct pollfd p={fd,POLLIN,0};if(poll(&p,1,-1)<0)continue;
        if(p.revents&(POLLHUP|POLLERR))break;
        char signals[128];while(read(fd,signals,sizeof(signals))>0){}
        lock_frame();int stop=shared->stop,clean=shared->clean;unsigned version=shared->version;
        if(version!=seen){memcpy(current,shared->pixels,size);shared->clean=0;}
        unlock_frame();if(stop)break;if(version==seen)continue;seen=version;
        struct dirty_rect r={0,0,view_w,view_h};
        if(have&&!clean&&!dirty(current,previous,&r))continue;
        int mode=clean||!have?DISPLAY_CLEAN:motion;
        if(display_update_stride(++marker,view_x+r.x,view_y+r.y,r.w,r.h,mode,current+r.y*view_w+r.x,view_w)){
            perror("GAME_PANEL");break;
        }
        memcpy(previous,current,size);have=1;
        fprintf(stderr,"GAME_PANEL frame=%u latest=%u clean=%d\n",marker,version,mode==DISPLAY_CLEAN);
    }
    free(current);free(previous);close(fd);_exit(0);
}
int game_display_start(int x,int y,int w,int h,const char *environment){
    if(x<0||y<0||w<=0||h<=0||w>4096||h>4096)return -1;
    view_x=x;view_y=y;view_w=w;view_h=h;wave_env=environment;
    size=(size_t)w*h;shared_size=sizeof(*shared)+size;
    shared=mmap(NULL,shared_size,PROT_READ|PROT_WRITE,MAP_SHARED|MAP_ANONYMOUS,-1,0);
    if(shared==MAP_FAILED){shared=NULL;return -1;}
    int pipefd[2];if(pipe(pipefd)){munmap(shared,shared_size);shared=NULL;return -1;}
    for(int i=0;i<2;i++)fcntl(pipefd[i],F_SETFL,O_NONBLOCK);
    child=fork();
    if(child==0){close(pipefd[1]);worker(pipefd[0]);}
    close(pipefd[0]);notify_fd=pipefd[1];
    if(child<0){close(notify_fd);notify_fd=-1;munmap(shared,shared_size);shared=NULL;return -1;}
    return 0;
}
int game_display_offer(const unsigned char *pixels,int clean){
    if(child<=0)return -1;
    if(waitpid(child,NULL,WNOHANG)==child){child=-1;return -1;}
    lock_frame();memcpy(shared->pixels,pixels,size);shared->version++;shared->clean|=clean;unlock_frame();
    // A full pipe already contains a wakeup; shared memory always contains the newest frame.
    (void)write(notify_fd,"F",1);return 0;
}
void game_display_stop(void){
    if(child>0){lock_frame();shared->stop=1;unlock_frame();(void)write(notify_fd,"S",1);waitpid(child,NULL,0);}
    if(notify_fd>=0)close(notify_fd);if(shared)munmap(shared,shared_size);
}

void game_display_pause(int paused){
    if(child<=0)return;
    if(!paused)shared->clean=1;
    kill(child,paused?SIGSTOP:SIGCONT);
}
