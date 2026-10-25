// input: rapid frame offers and a deliberately slow panel; output: latest-frame and bounded-queue checks; pos: display worker regression.
#include "display-worker.h"
#define ARCADE_VIEW_W 960
#define ARCADE_VIEW_H 720
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <unistd.h>
static int report[2];
int display_update_stride(uint32_t seq,int x,int y,int w,int h,int mode,const uint8_t *pixels,size_t pitch){
    (void)seq;assert(x==56&&y==192&&w==960&&h==720&&pitch==960);assert(mode==1||mode==4);
    unsigned char v=pixels[0];assert(write(report[1],&v,1)==1);usleep(200000);return 0;
}
int main(void){
    assert(game_display_start(0,0,0,720,"TEST_WAVEFORM")<0);
    assert(!pipe(report));assert(!game_display_start(56,192,ARCADE_VIEW_W,ARCADE_VIEW_H,"TEST_WAVEFORM"));unsigned char *p=malloc(ARCADE_VIEW_W*ARCADE_VIEW_H);
    memset(p,1,ARCADE_VIEW_W*ARCADE_VIEW_H);assert(!game_display_offer(p,1));usleep(50000);
    for(int i=2;i<=25;i++){memset(p,i,ARCADE_VIEW_W*ARCADE_VIEW_H);assert(!game_display_offer(p,0));usleep(5000);}
    usleep(500000);assert(!game_display_offer(p,0));usleep(250000);game_display_stop();close(report[1]);unsigned char frames[64];int n=read(report[0],frames,sizeof(frames));
    assert(n>=2&&n<=4);assert(frames[0]==1&&frames[n-1]==25);int last_count=0;for(int i=0;i<n;i++)if(frames[i]==25)last_count++;assert(last_count==1);free(p);close(report[0]);
    printf("25 offered frames -> %d completed updates; newest frame preserved\n",n);return 0;
}
