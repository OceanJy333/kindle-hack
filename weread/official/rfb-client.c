// input: original Qt VNC frames; output: 8-bit gray EPDC updates; pos: native display transport, RFC 6143.
#include "rfb-client.h"
#include "../../native/display.h"
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static int receive(int fd, void *buffer, size_t count) {
    uint8_t *data = buffer;
    while (count) {
        ssize_t size = recv(fd, data, count, 0);
        if (size <= 0) return -1;
        data += size; count -= size;
    }
    return 0;
}

static int transmit(int fd, const void *buffer, size_t count) {
    const uint8_t *data = buffer;
    while (count) {
        ssize_t size = send(fd, data, count, MSG_NOSIGNAL);
        if (size <= 0) return -1;
        data += size; count -= size;
    }
    return 0;
}

static unsigned word(const uint8_t *data) { return data[0] * 256U + data[1]; }
static unsigned dword(const uint8_t *data) {
    return (unsigned)data[0] << 24 | (unsigned)data[1] << 16 | (unsigned)data[2] << 8 | data[3];
}

int rfb_open(const char *host, int port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    struct timeval timeout = {.tv_sec=5};
    setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout));
    setsockopt(fd,SOL_SOCKET,SO_SNDTIMEO,&timeout,sizeof(timeout));
    struct sockaddr_in addr = { .sin_family = AF_INET, .sin_port = htons(port) };
    uint8_t banner[12], init[24], security[4];
    if (inet_pton(AF_INET, host, &addr.sin_addr) != 1 || connect(fd, (void *)&addr, sizeof(addr))) goto fail;
    if (receive(fd, banner, sizeof(banner)) || memcmp(banner, "RFB 003.003\n", 12)) goto fail;
    if (transmit(fd, banner, 12) || receive(fd, security, 4) || dword(security) != 1) goto fail;
    uint8_t shared = 1;
    if (transmit(fd, &shared, 1) || receive(fd, init, sizeof(init))) goto fail;
    if (word(init) != (unsigned)display_width() || word(init + 2) != (unsigned)display_height() || dword(init + 20) > 1024) goto fail;
    char name[1025]; unsigned length = dword(init + 20);
    if (receive(fd, name, length)) goto fail;
    name[length] = 0;
    fprintf(stderr, "NATIVE_QT_SCREEN %s %dx%d\n", name, display_width(), display_height());
    uint8_t format[20] = {0, 0, 0, 0, 16, 16, 0, 1, 0, 31, 0, 63, 0, 31, 11, 5, 0, 0, 0, 0};
    uint8_t encodings[8] = {2, 0, 0, 1, 0, 0, 0, 0};
    if (transmit(fd, format, sizeof(format)) || transmit(fd, encodings, sizeof(encodings))) goto fail;
    return fd;
fail:
    close(fd); return -1;
}

int rfb_update(int fd, int full) {
    int w = display_width(), h = display_height();
    uint8_t request[10] = {3, !full, 0, 0, 0, 0, w >> 8, w, h >> 8, h};
    return transmit(fd, request, sizeof(request));
}

int rfb_pointer(int fd, int x, int y, int down) {
    uint8_t request[6] = {5, down, x >> 8, x, y >> 8, y};
    return transmit(fd, request, sizeof(request));
}

int rfb_key(int fd, unsigned key, int down) {
    uint8_t request[8] = {4, down, 0, 0, key >> 24, key >> 16, key >> 8, key};
    return transmit(fd, request, sizeof(request));
}

static uint8_t *canvas, *scratch, *row;
static int canvas_valid;
int rfb_clean(unsigned *sequence) {
    return canvas_valid ? display_update(++*sequence,0,0,display_width(),display_height(),1,canvas) : 0;
}
int rfb_receive(int fd, unsigned *sequence, int full) {
    uint8_t kind, header[12];
    if (receive(fd, &kind, 1)) return -1;
    if (kind == 2) return 0; // All notifications stay silent.
    if (kind == 3) {
        if (receive(fd, header, 7)) return -1;
        unsigned remaining = dword(header + 3);
        if (remaining > 1024 * 1024) return -1;
        while (remaining) {
            unsigned size = remaining > sizeof(header) ? sizeof(header) : remaining;
            if (receive(fd, header, size)) return -1;
            remaining -= size;
        }
        return 0;
    }
    if (kind != 0 || receive(fd, header, 3)) return -1;
    unsigned count = word(header + 1);
    if (count > 1024) return -1;
    static uint8_t gray565[65536];
    static unsigned page_updates;
    int width = display_width(), height = display_height();
    if (!canvas) {
        canvas = calloc(width, height); scratch = malloc(width * height); row = malloc(width * 2);
        if (!canvas || !scratch || !row) return -1;
        for (unsigned rgb = 0; rgb < 65536; ++rgb) {
            unsigned r = ((rgb >> 11) & 31) * 255 / 31;
            unsigned g = ((rgb >> 5) & 63) * 255 / 63;
            unsigned b = (rgb & 31) * 255 / 31;
            gray565[rgb] = (r * 77 + g * 150 + b * 29) >> 8;
        }
    }
    int left = width, top = height, right = -1, bottom = -1;
    unsigned gray_changes=0;
    for (unsigned i = 0; i < count; i++) {
        if (receive(fd, header, 12)) return -1;
        unsigned x = word(header), y = word(header + 2), w = word(header + 4), h = word(header + 6);
        if (dword(header + 8) || !w || !h || x + w > (unsigned)display_width() || y + h > (unsigned)display_height()) return -1;
        for (unsigned line = 0; line < h; line++) {
            if (receive(fd, row, w * 2)) return -1;
            for (unsigned column = 0; column < w; column++) {
                uint8_t gray = gray565[row[column * 2] | row[column * 2 + 1] << 8];
                int px = x + column, py = y + line;
                if (full || canvas[py * width + px] != gray) {
                    uint8_t old=canvas[py * width + px];
                    if((old>32 && old<224)||(gray>32 && gray<224))gray_changes++;
                    canvas[py * width + px] = gray;
                    if (px < left) left = px;
                    if (px > right) right = px;
                    if (py < top) top = py;
                    if (py > bottom) bottom = py;
                }
            }
        }
    }
    if (right >= left && bottom >= top) {
        int w = right - left + 1, h = bottom - top + 1;
        // Count substantial changed pages, not transport rectangles or unchanged frames.
        if (w * h > width * height / 4 && ++page_updates % 6 == 0) full = 1;
        // Large images need a clean when entering/leaving, rather than leaving their shadows for six pages.
        if(gray_changes>(unsigned)(width*height/12))full=1;
        if (full) { left = top = 0; w = width; h = height; }
        for (int y = 0; y < h; ++y)
            memcpy(scratch + y * w, canvas + (top + y) * width + left, w);
        if (display_update(++*sequence, left, top, w, h, full ? 1 : 2, scratch)) return -1;
        canvas_valid = 1;
        fprintf(stderr, "OFFICIAL_FRAME %u %d,%d %dx%d full=%d\n", *sequence, left, top, w, h, full);
    }
    return 1; // A zero-rectangle update still consumes the outstanding request.
}

int rfb_clean_region(unsigned *sequence,int x,int y,int w,int h) {
    if(!canvas_valid || x<0 || y<0 || w<=0 || h<=0 || x+w>display_width() || y+h>display_height())return -1;
    return display_update_stride(++*sequence,x,y,w,h,2,canvas+y*display_width()+x,display_width());
}
