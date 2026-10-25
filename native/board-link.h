// input: desktop endpoint and taps; output: decoded board frames; pos: authenticated transport API.
#pragma once
int board_open(const char *host,int port,const char *token);
int board_auth(int fd,const char *token,const char *tab);
int board_key(int fd,int direction);
int board_tap(int fd,int x,int y);
int board_receive(int fd);
