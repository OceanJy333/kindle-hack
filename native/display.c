// input: 8-bit gray pixels and evdev events; output: EPDC refresh and normalized taps; pos: Kindle driver.
#include "display.h"
#include <linux/fb.h>
#include <linux/input.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <time.h>
#include <signal.h>

// PW2 Linux 3.0.35 ABI; see FBInk eink/mxcfb-kindle.h (source recorded in README).
struct rect { uint32_t top, left, width, height; };
struct alt { uint32_t addr, width, height; struct rect region; };
struct update {
    struct rect region;
    uint32_t waveform, mode, marker, bw_waveform, gray_waveform;
    int32_t temp;
    uint32_t flags;
    struct alt alternate;
};
static int fb = -1;
static uint8_t *memory;
static struct fb_fix_screeninfo fix;
static struct fb_var_screeninfo var;
static struct input_absinfo range_x, range_y;
static int touch_x, touch_y;
static uint32_t submitted_marker;

int display_open(void) {
    fb = open("/dev/fb0", O_RDWR | O_CLOEXEC);
    if (fb < 0 || ioctl(fb, FBIOGET_FSCREENINFO, &fix) || ioctl(fb, FBIOGET_VSCREENINFO, &var)) return -1;
    if (var.bits_per_pixel != 8 || !var.xres || !var.yres) { errno = ENOTSUP; return -1; }
    memory = mmap(NULL, fix.smem_len, PROT_READ | PROT_WRITE, MAP_SHARED, fb, 0);
    if (memory == MAP_FAILED) return -1;
    fprintf(stderr, "DISPLAY %ux%u stride=%u bpp=%u offset=%u,%u\n", var.xres, var.yres, fix.line_length, var.bits_per_pixel, var.xoffset, var.yoffset);
    return 0;
}

int display_width(void) { return var.xres; }
int display_height(void) { return var.yres; }

int display_update(uint32_t seq, int x, int y, int w, int h, int full, const uint8_t *pixels) {
    return display_update_stride(seq, x, y, w, h, full, pixels, w > 0 ? (size_t)w : 0);
}

int display_update_stride(uint32_t seq, int x, int y, int w, int h, int full, const uint8_t *pixels, size_t pitch) {
    if (fb < 0 || !pixels || pitch < (size_t)w || x < 0 || y < 0 || w <= 0 || h <= 0 || (uint32_t)x + w > var.xres || (uint32_t)y + h > var.yres) {
        errno = EINVAL; return -1;
    }
    for (int row = 0; row < h; row++)
        memcpy(memory + (y + row + var.yoffset) * fix.line_length + x + var.xoffset, pixels + row * pitch, w);
    return display_refresh(seq, x, y, w, h, full);
}

int display_refresh(uint32_t seq, int x, int y, int w, int h, int full) {
    if (fb < 0 || x < 0 || y < 0 || w <= 0 || h <= 0 || (uint32_t)x + w > var.xres || (uint32_t)y + h > var.yres) {
        errno = EINVAL; return -1;
    }
    // Markers are device-unique: desk and reader sequences restart and overlap, and a reused marker
    // made the completion wait hang for the driver's 20 s timeout, freezing touch and the link.
    static uint32_t marker;
    (void)seq;
    struct update u = {0};
    u.region = (struct rect){ y, x, w, h };
    u.waveform = full == DISPLAY_FAST_MONO ? 4 : full == DISPLAY_MONO ? 1 : full ? 2 : 257;
    if(full == DISPLAY_FAST_MONO)u.flags=0x02; // Kernel A2 monochrome waveform. // GC16 on full refresh; driver chooses waveform for a partial region.
    // AUTO uses these histogram choices; zero is INIT and can cause repeated flashing.
    u.bw_waveform = 1; // DU for the existing black/white dithered game pixels.
    u.gray_waveform = 2; // Preserve GC16 grayscale fidelity on desktop/reader regions.
    u.mode = full == DISPLAY_CLEAN ? 1 : 0; // 2: reader GC16 regional refresh, without a full-screen flash.
    u.marker = ++marker ? marker : ++marker;
    u.temp = 0x1001; // PW2/KV TEMP_USE_AUTO, matching the stock temperature selection.
    // Submit only: the EPDC queues and merges updates itself, and the loop must stay responsive.
    if(ioctl(fb, _IOW('F', 0x2e, struct update), &u)<0)return -1;
    submitted_marker=u.marker;
    // Opt-in measurement only: wait for this driver's completion marker, not a TCP ACK.
    if(getenv("DISPLAY_SYNC")){
        struct timespec a,b;struct {uint32_t marker,collision;} done={u.marker,0};
        clock_gettime(CLOCK_MONOTONIC,&a);
        int result=ioctl(fb,_IOWR('F',0x2f,typeof(done)),&done);
        clock_gettime(CLOCK_MONOTONIC,&b);
        long long elapsed=(b.tv_sec-a.tv_sec)*1000000LL+(b.tv_nsec-a.tv_nsec)/1000;
        fprintf(stderr,"DISPLAY_COMPLETE marker=%u wait_us=%lld result=%d\n",u.marker,elapsed,result);
        if(result<0)return -1;
    }
    return 0;
}

int display_wait(void) {
    if(fb<0||!submitted_marker)return 0;
    struct {uint32_t marker,collision;} done={submitted_marker,0};
    // This driver returns remaining timeout ticks on success, not necessarily zero.
    return ioctl(fb,_IOWR('F',0x2f,typeof(done)),&done)<0?-1:0;
}

static int owned_touch=-1;
static void overlay_pause(int sig) {
    (void)sig;
    if(owned_touch>=0)ioctl(owned_touch,EVIOCGRAB,0);
    raise(SIGSTOP);
    if(owned_touch>=0)ioctl(owned_touch,EVIOCGRAB,1);
}
int touch_open(void) {
    char path[64], name[128];
    for (int i = 0; i < 16; i++) {
        snprintf(path, sizeof(path), "/dev/input/event%d", i);
        int fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        if (fd < 0) continue;
        memset(name, 0, sizeof(name));
        ioctl(fd, EVIOCGNAME(sizeof(name)), name);
        if (strstr(name, "cyttsp") && !ioctl(fd, EVIOCGABS(ABS_MT_POSITION_X), &range_x)
            && !ioctl(fd, EVIOCGABS(ABS_MT_POSITION_Y), &range_y)) {
            touch_x = range_x.value; touch_y = range_y.value;
            if(!getenv("NOTIFY_INPUT")){
                if (ioctl(fd, EVIOCGRAB, 1)) { close(fd); return -1; }
                owned_touch=fd;signal(SIGUSR1,overlay_pause);
            }
            fprintf(stderr, "TOUCH %s %s x=%d..%d y=%d..%d\n", path, name, range_x.minimum, range_x.maximum, range_y.minimum, range_y.maximum);
            return fd;
        }
        close(fd);
    }
    return -1;
}

int touch_read(int fd, int *x, int *y) {
    static int down, release, seen;
    struct input_event e;
    while (read(fd, &e, sizeof(e)) == sizeof(e)) {
        if (e.type == EV_ABS && e.code == ABS_MT_POSITION_X) touch_x = e.value;
        if (e.type == EV_ABS && e.code == ABS_MT_POSITION_Y) touch_y = e.value;
        if (e.type == EV_ABS && e.code == ABS_MT_TRACKING_ID) {
            if (e.value >= 0) {
                int xs[2] = { ABS_MT_POSITION_X, touch_x }, ys[2] = { ABS_MT_POSITION_Y, touch_y };
                if (!ioctl(fd, EVIOCGMTSLOTS(sizeof(xs)), xs)) touch_x = xs[1];
                if (!ioctl(fd, EVIOCGMTSLOTS(sizeof(ys)), ys)) touch_y = ys[1];
                down = 1; seen = 1;
            } else release = down;
        }
        if (e.type == EV_KEY && e.code == BTN_TOUCH) {
            if (e.value) { down = 1; seen = 1; } else release = down;
        }
        if (e.type == EV_SYN && e.code == SYN_REPORT && release && seen) {
            *x = (touch_x - range_x.minimum) * (int)(var.xres - 1) / (range_x.maximum - range_x.minimum);
            *y = (touch_y - range_y.minimum) * (int)(var.yres - 1) / (range_y.maximum - range_y.minimum);
            down = release = seen = 0;
            fprintf(stderr, "TAP %d %d raw=%d,%d\n", *x, *y, touch_x, touch_y);
            return 1;
        }
    }
    return 0;
}

int display_copy(uint8_t *pixels) {
    if(fb<0 || !pixels)return -1;
    for(unsigned y=0;y<var.yres;y++)memcpy(pixels+y*var.xres,memory+(y+var.yoffset)*fix.line_length+var.xoffset,var.xres);
    return 0;
}

void display_close(void) {
    if (memory && memory != MAP_FAILED) munmap(memory, fix.smem_len);
    if (fb >= 0) close(fb);
}
