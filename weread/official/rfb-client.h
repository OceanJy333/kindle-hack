// input: local native Qt VNC endpoint; output: pixel updates and pointer transport; pos: Kindle display adapter API.
#pragma once
int rfb_open(const char *host, int port);
int rfb_update(int fd, int full);
int rfb_receive(int fd, unsigned *sequence, int full);
int rfb_pointer(int fd, int x, int y, int down);
int rfb_key(int fd, unsigned key, int down);
int rfb_clean(unsigned *sequence);

int rfb_clean_region(unsigned *sequence,int x,int y,int w,int h);
