// input: newest monochrome game image; output: one in-flight screen update; pos: shared game display queue.
#pragma once
int game_display_start(int x,int y,int w,int h,const char *wave_environment);
int game_display_offer(const unsigned char *pixels,int clean);
void game_display_stop(void);

void game_display_pause(int paused);
