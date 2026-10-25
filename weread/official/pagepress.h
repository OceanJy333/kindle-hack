// input: Voyage fsr_keypad evdev; output: one page action per press; pos: hardware key adapter.
#pragma once
#include <linux/input.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
static int pagepress_open(void) {
    for (int i = 0; i < 16; ++i) {
        char path[64], name[128] = {0};
        snprintf(path, sizeof(path), "/dev/input/event%d", i);
        int fd = open(path, O_RDONLY | O_NONBLOCK);
        if (fd < 0) continue;
        ioctl(fd, EVIOCGNAME(sizeof(name)), name);
        if (!strcmp(name, "fsr_keypad") && !ioctl(fd, EVIOCGRAB, 1)) {
            fprintf(stderr, "PAGEPRESS %s codes=%d,%d\n", path, KEY_PAGEUP, KEY_PAGEDOWN);
            return fd;
        }
        close(fd);
    }
    return -1;
}
static int pagepress_read(int fd) {
    static unsigned held;
    static int dropped;
    struct input_event e;
    while (read(fd, &e, sizeof(e)) == sizeof(e)) {
        if (e.type == EV_SYN && e.code == SYN_DROPPED) { dropped = 1; held = 3; }
        if (dropped) {
            if (e.type == EV_SYN && e.code == SYN_REPORT) dropped = 0;
            continue;
        }
        if (e.type != EV_KEY) continue;
        unsigned bit = e.code == KEY_PAGEUP ? 1 : e.code == KEY_PAGEDOWN ? 2 : 0;
        if (!bit) continue;
        if (!e.value) held &= ~bit;
        else if (e.value == 1 && !(held & bit)) {
            held |= bit;
            return bit == 1 ? -1 : 1;
        }
    }
    return 0;
}
