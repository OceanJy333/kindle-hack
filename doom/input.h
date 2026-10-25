// input: evdev and monotonic time; output: Doom key events; pos: game input interface.
#pragma once
int game_input_open(void);
void game_input_close(void);
int game_input_key(int *pressed,unsigned char *key);
void game_input_wait(int timeout_ms);
extern int game_running;
