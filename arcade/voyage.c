// input: native libretro core, game ZIP and evdev; output: silent Neo Geo frames on EPDC; pos: Voyage arcade frontend.
#include "doomgeneric.h"
#include "../doom/input.h"
#include "../native/display.h"
#include "video-render.h"
#include "../games/display-worker.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <signal.h>
#include <unistd.h>

#define VIEW_W 960
#define VIEW_H 720
static unsigned char screen[1072*1448],previous[VIEW_W*VIEW_H],gray[VIEW_W*VIEW_H];
static unsigned short keys;
static enum retro_pixel_format format=RETRO_PIXEL_FORMAT_0RGB1555;
static uint32_t start,frames,runs,shown;
static double next_frame,frame_step=5.0;
static uint32_t last_clean,last_submit;
static uint64_t submit_wall_us;
static uint64_t core_us,video_us,io_us;
static unsigned bench_frames,converted,unchanged;
static int input_changed,frame_requested;
static const char *rom_dir="/mnt/us/games/roms";
uint32_t DG_GetTicksMs(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec*1000U+t.tv_nsec/1000000U;}
static uint64_t cpu_us(void){struct timespec t;clock_gettime(CLOCK_PROCESS_CPUTIME_ID,&t);return (uint64_t)t.tv_sec*1000000+t.tv_nsec/1000;}
static void stop(int sig){(void)sig;game_running=0;}
static void log_message(enum retro_log_level level,const char *fmt,...){(void)level;va_list args;va_start(args,fmt);vfprintf(stderr,fmt,args);va_end(args);}
static bool environment(unsigned cmd,void *data){
    switch(cmd){
    case RETRO_ENVIRONMENT_GET_LOG_INTERFACE:((struct retro_log_callback*)data)->log=log_message;return true;
    case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
    case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY:*(const char**)data=rom_dir;return true;
    case RETRO_ENVIRONMENT_GET_CAN_DUPE:*(bool*)data=true;return true;
    case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE:*(bool*)data=false;return true;
    case RETRO_ENVIRONMENT_GET_AUDIO_VIDEO_ENABLE:
        // Render at the configured display cadence; experiments may request a fractional rate.
        int urgent=input_changed && DG_GetTicksMs()-last_submit>=33;
        frame_requested=!shown||urgent||runs>=next_frame;
        if(frame_requested){if(urgent)next_frame=runs+frame_step;else next_frame+=frame_step;if(next_frame<=runs)next_frame=runs+frame_step;}
        *(int*)data=frame_requested?1:0;return true;
    case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:
        format=*(enum retro_pixel_format*)data;return format==RETRO_PIXEL_FORMAT_RGB565||format==RETRO_PIXEL_FORMAT_0RGB1555||format==RETRO_PIXEL_FORMAT_XRGB8888;
    case RETRO_ENVIRONMENT_SET_VARIABLES:
    case RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS:
    case RETRO_ENVIRONMENT_SET_CONTROLLER_INFO:
    case RETRO_ENVIRONMENT_SET_GEOMETRY:return true;
    case RETRO_ENVIRONMENT_GET_VARIABLE:{
        struct retro_variable *v=data;
        if(strstr(v->key,"frameskip")){v->value="disabled";return true;}
        return false;
    }
    default:return false;
    }
}
static void input_poll(void){
    unsigned short old=keys;
    int down;unsigned char key;
    while(game_input_key(&down,&key))if(key&&key<=16){unsigned mask=1U<<(key-1);if(down)keys|=mask;else keys&=~mask;}
    if(bench_frames){
        // A fixed input replay makes CPU/frame comparisons reproducible; the real core still simulates every tick.
        keys=0;
        if(runs>=240&&runs<270)keys|=1U<<RETRO_DEVICE_ID_JOYPAD_SELECT;
        if(runs>=300&&runs<340)keys|=1U<<RETRO_DEVICE_ID_JOYPAD_START;
        if(runs>=600){keys|=1U<<RETRO_DEVICE_ID_JOYPAD_RIGHT;keys|=1U<<RETRO_DEVICE_ID_JOYPAD_B;if(runs%180<40)keys|=1U<<RETRO_DEVICE_ID_JOYPAD_A;}
    }
    if(keys!=old)input_changed=1;
}
static int16_t input_state(unsigned port,unsigned device,unsigned index,unsigned id){return port==0&&device==RETRO_DEVICE_JOYPAD&&index==0&&id<16?!!(keys&(1U<<id)):0;}
// Audio is consumed without opening any sound device; the core patch skips sample mixing.
static void audio_sample(int16_t left,int16_t right){(void)left;(void)right;}
static size_t audio_batch(const int16_t *samples,size_t count){(void)samples;return count;}
static void video(const void *data,unsigned w,unsigned h,size_t pitch){
    if(!data||!w||!h)return;
    uint64_t begin=cpu_us();
    uint32_t now=DG_GetTicksMs();
    converted++;
    input_changed=0;
    if(arcade_convert(data,w,h,pitch,format,gray)){game_running=0;return;}
    struct arcade_rect region={0,0,VIEW_W,VIEW_H};
    int changed=!shown||arcade_dirty(gray,previous,&region);
    video_us+=cpu_us()-begin;
    if(!changed){unchanged++;return;}
    int clean=!shown || now-last_clean>=8000;
    if(clean)region=(struct arcade_rect){0,0,VIEW_W,VIEW_H};
    begin=cpu_us();
    int left=region.x,top=region.y,pw=region.w,ph=region.h;
    uint32_t submitted=DG_GetTicksMs();
    frames++;
    if(game_display_offer(gray,clean)){perror("ARCADE display");game_running=0;}
    for(int y=0;y<ph;y++)memcpy(previous+(top+y)*VIEW_W+left,gray+(top+y)*VIEW_W+left,pw);
    submit_wall_us+=(uint64_t)(DG_GetTicksMs()-submitted)*1000;
    last_submit=now;if(clean){last_clean=now;fprintf(stderr,"ARCADE_CLEAN ms=%u\n",now-start);}
    shown=1;
    io_us+=cpu_us()-begin;
}
static int show_file(const char *path,int x,int y,int w,int h,int full){
    FILE *f=fopen(path,"rb");if(!f)return -1;
    size_t n=fread(screen,1,w*h,f);fclose(f);
    return n==(size_t)(w*h)?display_update(++frames,x,y,w,h,full,screen):-1;
}
int main(int argc,char **argv){
    retro_set_environment(environment);retro_set_video_refresh(video);retro_set_audio_sample(audio_sample);retro_set_audio_sample_batch(audio_batch);
    retro_set_input_poll(input_poll);retro_set_input_state(input_state);retro_init();
    struct retro_system_info info;retro_get_system_info(&info);
    fprintf(stderr,"ARCADE_CORE %s %s ARMv7 silent=1\n",info.library_name,info.library_version);
    if(argc==2&&!strcmp(argv[1],"--probe")){retro_deinit();return 0;}
    const char *bench=getenv("ARCADE_BENCH_FRAMES");if(bench){unsigned n=strtoul(bench,NULL,10);if(n>=600&&n<=3600)bench_frames=n;}
    if(display_open()||display_width()!=1072||display_height()!=1448||game_input_open())return 1;
    signal(SIGTERM,stop);signal(SIGINT,stop);signal(SIGPIPE,SIG_IGN);
    if(show_file("overlay.gray",0,0,1072,1448,1))return 1;
    if(game_display_start(56,192,VIEW_W,VIEW_H,"ARCADE_WAVEFORM"))return 1;
    struct retro_game_info game={.path=argc>1?argv[1]:"/mnt/us/games/roms/mslug.zip"};
    bool loaded=access(game.path,R_OK)==0&&retro_load_game(&game);
    if(!loaded){
        fprintf(stderr,"ARCADE_MISSING game=%s gameplay=0\n",game.path);
        if(show_file("missing.gray",56,192,VIEW_W,VIEW_H,0))return 1;
        // Assets are absent: wait on input, without drawing or running an emulation loop.
        while(game_running){game_input_wait(-1);input_poll();}
    }else{
        struct retro_system_av_info av;retro_get_system_av_info(&av);
        double target=18.0;
        const char *hz=getenv("ARCADE_TARGET_HZ");
        if(hz){double requested=strtod(hz,NULL);if(requested>=6&&requested<=60)target=requested;}
        frame_step=av.timing.fps/target;
        fprintf(stderr,"ARCADE_DISPLAY target=%.1fHz clean_interval=8000ms\n",target);
        fprintf(stderr,"ARCADE_GAME_LOADED fps=%.3f size=%ux%u\n",av.timing.fps,av.geometry.base_width,av.geometry.base_height);
        uint64_t tick=(uint64_t)(1000000000.0/av.timing.fps),next=0;
        start=DG_GetTicksMs();
        while(game_running){
            struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);uint64_t now=(uint64_t)t.tv_sec*1000000000+t.tv_nsec;
            if(next>now){struct timespec delay={0,(long)(next-now)};nanosleep(&delay,NULL);}else if(now-next>tick*3)next=now;
            uint64_t cpu=cpu_us(),display=video_us+io_us;
            retro_run();core_us+=cpu_us()-cpu-(video_us+io_us-display);runs++;next+=tick;
            if(runs%120==0)fprintf(stderr,"ARCADE_STATS ms=%u runs=%u frames=%u keys=%u core_us=%llu video_us=%llu io_us=%llu converted=%u unchanged=%u submit_wall_us=%llu\n",DG_GetTicksMs()-start,runs,frames,keys,(unsigned long long)core_us,(unsigned long long)video_us,(unsigned long long)io_us,converted,unchanged,(unsigned long long)submit_wall_us);
            if(bench_frames&&runs>=bench_frames)game_running=0;
        }
        retro_unload_game();
    }
    game_display_stop();retro_deinit();game_input_close();display_close();arcade_convert_close();return 0;
}
