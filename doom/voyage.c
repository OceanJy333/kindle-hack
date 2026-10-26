// input: native Doom RGB frames and evdev keys; output: bounded black/white EPDC frames; pos: Voyage platform adapter.
#include "doomgeneric.h"
#include "doomstat.h"
#include "i_system.h"
#include "input.h"
#include "../native/display.h"
#include "../games/display-worker.h"
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>

static unsigned char *screen,*previous,*region;
static unsigned char source_gray[DOOMGENERIC_RESX*DOOMGENERIC_RESY];
static int *source_x,*source_y;
static unsigned frames,skipped,marker,interval=83,last_frame,last_stats,start_ms,last_clean;
static int width,height,gx,gy,gw,gh,have_frame,input_changed;
static void stop(int signal){(void)signal;game_running=0;}
uint32_t DG_GetTicksMs(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec*1000U+t.tv_nsec/1000000U;}
void DG_SleepMs(uint32_t ms){struct timespec t={ms/1000,(ms%1000)*1000000};nanosleep(&t,NULL);}
void DG_SetWindowTitle(const char *title){fprintf(stderr,"DOOM_TITLE %s\n",title);}
int DG_GetKey(int *pressed,unsigned char *key){int result=game_input_key(pressed,key);if(result)input_changed=1;return result;}
void DG_Init(void){
    if(display_open()){perror("DOOM framebuffer");exit(1);}
    width=display_width();height=display_height();
    screen=malloc(width*height);previous=malloc(width*height);region=malloc(width*height);
    if(!screen||!previous||!region)exit(1);
    FILE *overlay=fopen("overlay.gray","rb");
    if(!overlay||fread(screen,1,width*height,overlay)!=(size_t)(width*height)){perror("DOOM overlay");exit(1);}
    fclose(overlay);memset(previous,0,width*height);
    gx=56;gy=192;gw=width-112;gh=gw*3/4;
    source_x=malloc(gw*sizeof(int));source_y=malloc(gh*sizeof(int));
    if(!source_x||!source_y)exit(1);
    for(int x=0;x<gw;x++)source_x[x]=x*DOOMGENERIC_RESX/gw;
    for(int y=0;y<gh;y++)source_y[y]=y*DOOMGENERIC_RESY/gh;
    const char *fps=getenv("DOOM_FPS");if(fps){int f=atoi(fps);if(f>=1&&f<=15)interval=1000/f;}
    if(game_input_open())exit(1);
    signal(SIGTERM,stop);signal(SIGINT,stop);signal(SIGPIPE,SIG_IGN);
    if(display_update(++marker,0,0,width,height,DISPLAY_CLEAN,screen)||game_display_start(gx,gy,gw,gh,"DOOM_WAVEFORM"))exit(1);
    start_ms=last_stats=DG_GetTicksMs();
    fprintf(stderr,"DOOM_PLATFORM ARMv7 %dx%d viewport=%dx%d target=%uHz silent=1\n",width,height,gw,gh,1000/interval);
}
void DG_DrawFrame(void){
    unsigned now=DG_GetTicksMs();
    if(have_frame&&now-last_frame<interval&&(!input_changed||now-last_frame<33)){skipped++;return;}
    last_frame=now;input_changed=0;
    static const unsigned char bayer[4][4]={{0,8,2,10},{12,4,14,6},{3,11,1,9},{15,7,13,5}};
    // Convert each original pixel once; scaling lookups avoid millions of software divisions on Cortex-A9.
    int source_changed=!have_frame;
    for(unsigned i=0;i<DOOMGENERIC_RESX*DOOMGENERIC_RESY;i++){
        uint32_t rgb=DG_ScreenBuffer[i];
        unsigned char gray=(((rgb>>16)&255)*77+((rgb>>8)&255)*150+(rgb&255)*29)>>8;
        if(gray!=source_gray[i])source_changed=1;source_gray[i]=gray;
    }
    if(source_changed){
    for(int y=0;y<gh;y++){
        const unsigned char *in=source_gray+source_y[y]*DOOMGENERIC_RESX;
        unsigned char *out=screen+(gy+y)*width+gx;
        for(int x=0;x<gw;x++)out[x]=in[source_x[x]]>8+bayer[y&3][x&3]*16?255:0;
    }
    // Only changed images cross the process boundary; an idle scene does not trigger periodic cleans.
    int changed=!have_frame;
    for(int y=0;y<gh&&!changed;y++)
        if(memcmp(screen+(gy+y)*width+gx,previous+y*gw,gw))changed=1;
    if(changed){
        for(int y=0;y<gh;y++)memcpy(region+y*gw,screen+(gy+y)*width+gx,gw);
        int clean=!have_frame||now-last_clean>=8000;
        if(game_display_offer(region,clean)){perror("DOOM display worker");game_running=0;}
        memcpy(previous,region,gw*gh);frames++;have_frame=1;if(clean)last_clean=now;
    }
    }
    if(now-last_stats>=2000){
        player_t *p=&players[consoleplayer];
        fprintf(stderr,"DOOM_STATS ms=%u frames=%u skipped=%u tic=%d state=%d demo=%d menu=%d x=%d y=%d angle=%u health=%d ammo=%d\n",
          now-start_ms,frames,skipped,gametic,gamestate,demoplayback,menuactive,p->mo?p->mo->x>>16:0,p->mo?p->mo->y>>16:0,p->mo?p->mo->angle:0,p->health,p->ammo[am_clip]);
        last_stats=now;
    }
}
static int runtime_probe(void){
    struct timespec now;int clock_ok=clock_gettime(CLOCK_MONOTONIC,&now)==0;
    int fd[2],pipe_ok=pipe(fd)==0,fork_ok=0;
    if(pipe_ok){
        pid_t child=fork();
        if(child==0){close(fd[0]);int ok=write(fd[1],"P",1)==1;close(fd[1]);_exit(ok?0:1);}
        close(fd[1]);
        if(child>0){
            char byte=0;int status=0;
            int read_ok=read(fd[0],&byte,1)==1&&byte=='P';
            fork_ok=waitpid(child,&status,0)==child&&WIFEXITED(status)&&WEXITSTATUS(status)==0&&read_ok;
        }
        close(fd[0]);
    }
    int ok=clock_ok&&pipe_ok&&fork_ok;
    printf("{\"program\":\"doom-voyage\",\"runtime\":%s,\"clock\":%s,\"pipe\":%s,\"fork\":%s}\n",
      ok?"true":"false",clock_ok?"true":"false",pipe_ok?"true":"false",fork_ok?"true":"false");
    return ok?0:1;
}
int main(int argc,char **argv){
    if(argc==2&&!strcmp(argv[1],"--probe"))return runtime_probe();
    doomgeneric_Create(argc,argv);
    while(game_running)doomgeneric_Tick();
    fprintf(stderr,"DOOM_FINAL elapsed=%u frames=%u skipped=%u\n",DG_GetTicksMs()-start_ms,frames,skipped);
    game_display_stop();game_input_close();display_close();free(screen);free(previous);free(region);free(source_x);free(source_y);
    I_Quit();return 0;
}
