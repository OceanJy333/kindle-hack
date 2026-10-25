// input: authenticated TCP gray rectangles; output: framebuffer updates, touch and ACK; pos: native device app.
#include "display.h"
#include "board-link.h"
#include "device-settings.h"
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <poll.h>
#include <unistd.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <zlib.h>
#include <fcntl.h>

#define PIXELS (1072 * 1448)
#define MAX_PACKET (32 + PIXELS * 2)
static uint8_t packet[MAX_PACKET], pixels[PIXELS];
static uint8_t unpacked[PIXELS*2];
static uint32_t le32(const uint8_t *p) { return p[0] | p[1]<<8 | p[2]<<16 | (uint32_t)p[3]<<24; }
static long long now_us(void) {
    struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t);
    return (long long)t.tv_sec * 1000000 + t.tv_nsec / 1000;
}
static int send_text(int fd, const char *line) {
    size_t left = strlen(line);
    while (left) { ssize_t n = send(fd, line, left, MSG_NOSIGNAL); if (n <= 0) return -1; line += n; left -= n; }
    return 0;
}
static int draw_packet(int fd, size_t length) {
    uint32_t seq = le32(packet + 4), x = le32(packet + 8), y = le32(packet + 12);
    uint32_t w = le32(packet + 16), h = le32(packet + 20), n = le32(packet + 24), flags = le32(packet + 28);
    if (le32(packet) != 0x31424741 || !w || !h || w > (unsigned)display_width() || h > (unsigned)display_height() || x > (unsigned)display_width()-w || y > (unsigned)display_height()-h || length != n+32) return -1;
    size_t out = 0;
    long long begin = now_us();
    const uint8_t *rle=packet+32;unsigned long rle_len=n;
    if(flags&2) {rle_len=sizeof(unpacked);if(uncompress(unpacked,&rle_len,packet+32,n)!=Z_OK)return -1;rle=unpacked;}
    if(rle_len%2)return -1;
    for (size_t i = 0; i < rle_len; i += 2) {
        unsigned count = rle[i];
        if (!count || out + count > w*h) return -1;
        memset(pixels + out, rle[i+1], count); out += count;
    }
    if (out != w*h || display_update(seq, x, y, w, h, flags&1, pixels)) { perror("display_update"); return -1; }
    char ack[160];
    snprintf(ack, sizeof(ack), "ACK %u %lld %u\n", seq, now_us()-begin, n);
    fprintf(stderr, "%s", ack);
    return send_text(fd, ack);
}

static size_t used, need = 32;
int board_open(const char *host, int port, const char *token) {
    (void)token;
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr = {.sin_family=AF_INET,.sin_port=htons(port)};
    fcntl(fd,F_SETFL,O_NONBLOCK);
    if(inet_pton(AF_INET,host,&addr.sin_addr)!=1){close(fd);return -1;}
    if(connect(fd,(void*)&addr,sizeof(addr)) && errno!=EINPROGRESS){close(fd);return -1;}
    int one=1;setsockopt(fd,IPPROTO_TCP,TCP_NODELAY,&one,sizeof(one));
    used=0;need=32;return fd;
}
// The optional tab lets the desktop paint the requested mode as its first frame (no stale-mode flash).
int board_auth(int fd,const char *token,const char *tab) {
    int error=0;socklen_t length=sizeof(error);
    if(getsockopt(fd,SOL_SOCKET,SO_ERROR,&error,&length) || error)return -1;
    char hello[256];snprintf(hello,sizeof(hello),tab?"HELLO %s %s\n":"HELLO %s\n",token,tab);
    return send_text(fd,hello);
}
int board_tap(int fd,int x,int y) {
    char line[64];snprintf(line,sizeof(line),"TAP %d %d\n",x,y);return send_text(fd,line);
}
int board_key(int fd,int direction) {
    return send_text(fd,direction>0?"KEY 1\n":"KEY -1\n");
}
int board_receive(int fd) {
    ssize_t count=recv(fd,packet+used,need-used,0);
    if(count<=0)return -1;
    used+=count;
    if(used==32 && need==32) {
        if(le32(packet)!=0x31424741)return -1;
        unsigned bytes=le32(packet+24);
        if(!bytes && le32(packet+28)==64){used=0;need=32;return 5;}
        if(!bytes && le32(packet+28)==16){
            unsigned level=le32(packet+8),automatic=le32(packet+12);used=0;need=32;
            if(level>24||automatic>1)return -1;
            device_light((int)level,(int)automatic);device_status(fd);return 0;
        }
        if(!bytes && le32(packet+28)==32){
            used=0;need=32;display_refresh(0,0,0,display_width(),display_height(),1);return 0;
        }
        if(!bytes && le32(packet+28)==4){used=0;need=32;return 2;}
        if(!bytes && le32(packet+28)==8){
            unsigned game=le32(packet+8);used=0;need=32;
            return game==1||game==2?(int)game+2:-1;
        }
        if(!bytes || bytes>PIXELS*2)return -1;
        need=32+bytes;
    }
    if(used==need){if(draw_packet(fd,used))return -1;used=0;need=32;return 1;}
    return 0;
}
