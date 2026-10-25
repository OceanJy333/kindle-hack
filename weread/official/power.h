// input: powerd events and real user input; output: native app pause/resume; pos: event-driven power adapter.
#pragma once
#include <signal.h>
#include <time.h>
#include <stdlib.h>
#include <errno.h>
#ifndef POWER_EVENT_SOURCE
#define POWER_EVENT_SOURCE "com.lab126.powerd"
#endif
static pid_t power_listener;
static int renderer_pid(void) {
    int pid = 0;
    FILE *file = fopen("/tmp/weread-login.pid", "r");
    if (file) { if (fscanf(file, "%d", &pid) != 1) pid = 0; fclose(file); }
    return pid > 1 ? pid : 0;
}
static int power_open(void) {
    int pipes[2];
    if (pipe(pipes)) return -1;
    power_listener = fork();
    if (!power_listener) {
        dup2(pipes[1], STDOUT_FILENO);
        for (int fd = 3; fd < 64; ++fd) close(fd);
        execlp("lipc-wait-event", "lipc-wait-event", "-m", POWER_EVENT_SOURCE,
               "goingToScreenSaver,outOfScreenSaver", (char *)NULL);
        _exit(127);
    }
    close(pipes[1]);
    if (power_listener < 0) { close(pipes[0]); return -1; }
    fcntl(pipes[0], F_SETFL, O_NONBLOCK);
    return pipes[0];
}
static void power_activity(void) {
    static time_t last;
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    if (last && now.tv_sec - last < 15) return;
    last = now.tv_sec;
    if (!fork()) {
        for (int fd = 3; fd < 64; ++fd) close(fd);
        execlp("lipc-set-prop", "lipc-set-prop", "com.lab126.powerd",
               "touchScreenSaverTimeout", "1", (char *)NULL);
        _exit(127);
    }
}
static int power_read(int fd, int *asleep) {
    static char line[256];
    static unsigned used;
    char bytes[256]; int wake = 0; ssize_t count;
    while ((count = read(fd, bytes, sizeof(bytes))) > 0) {
        for (ssize_t i = 0; i < count; ++i) {
            if (bytes[i] != '\n') {
                if (used < sizeof(line) - 1) line[used++] = bytes[i];
                continue;
            }
            line[used] = 0; used = 0;
            if (strstr(line, "goingToScreenSaver")) {
                *asleep = 1;
                fprintf(stderr, "POWER_SLEEP\n");
            } else if (strstr(line, "outOfScreenSaver")) {
                *asleep = 0; wake = 1;
                fprintf(stderr, "POWER_WAKE\n");
            }
        }
    }
    return wake;
}
static void power_close(void) {
    if (power_listener > 0) kill(power_listener, SIGTERM);
}
