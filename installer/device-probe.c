// input: known Kindle profile, fb and evdev; output: non-drawing JSON/shell probe; pos: release binary.
#include <linux/fb.h>
#include <linux/input.h>
#include <sys/ioctl.h>
#include <sys/utsname.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <poll.h>
#include <time.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define BITS (8 * sizeof(unsigned long))
#define HAS(a,b) ((a)[(b)/BITS] & (1UL << ((b)%BITS)))

static int runtime_check(void) {
    struct timespec before, after, nap = {0, 1000000};
    if (clock_gettime(CLOCK_MONOTONIC, &before) || nanosleep(&nap, NULL) ||
        clock_gettime(CLOCK_MONOTONIC, &after)) return 0;
    if (after.tv_sec < before.tv_sec || (after.tv_sec == before.tv_sec &&
        after.tv_nsec <= before.tv_nsec)) return 0;
    unsigned char *mem = mmap(NULL, 4096, PROT_READ | PROT_WRITE,
                             MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    if (mem == MAP_FAILED) return 0;
    mem[0] = 42;
    int ok = mem[0] == 42;
    munmap(mem, 4096);
    FILE *file = tmpfile();
    if (!file) return 0;
    ok = ok && fwrite("O", 1, 1, file) == 1 && fflush(file) == 0;
    rewind(file);
    ok = ok && fgetc(file) == 'O';
    fclose(file);
    int pipes[2];
    if (pipe(pipes)) return 0;
    pid_t child = fork();
    if (!child) {
        close(pipes[0]);
        _exit(write(pipes[1], "O", 1) == 1 ? 0 : 1);
    }
    close(pipes[1]);
    if (child < 0) { close(pipes[0]); return 0; }
    struct pollfd p = {pipes[0], POLLIN, 0};
    char sample = 0;
    ok = ok && poll(&p, 1, 2000) > 0 && read(pipes[0], &sample, 1) == 1 && sample == 'O';
    close(pipes[0]);
    int status = 0;
    ok = waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 0 && ok;
    return ok;
}

static int touch_device(void) {
    char path[64];
    for (int i = 0; i < 32; ++i) {
        snprintf(path, sizeof(path), "/dev/input/event%d", i);
        int fd = open(path, O_RDONLY | O_NONBLOCK);
        if (fd < 0) continue;
        unsigned long abs[(ABS_MAX + BITS) / BITS] = {0};
        unsigned long keys[(KEY_MAX + BITS) / BITS] = {0};
        int ok = ioctl(fd, EVIOCGBIT(EV_ABS, sizeof(abs)), abs) >= 0;
        int contact = ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keys)), keys) >= 0 &&
            (HAS(keys, BTN_TOUCH) || HAS(keys, BTN_TOOL_FINGER));
        ok = ok && ((HAS(abs, ABS_MT_POSITION_X) && HAS(abs, ABS_MT_POSITION_Y)) ||
                    (HAS(abs, ABS_X) && HAS(abs, ABS_Y) && contact));
        close(fd);
        if (ok) return 1;
    }
    return 0;
}

int main(int argc, char **argv) {
    int shell = argc == 2 && !strcmp(argv[1], "--shell");
    if (argc != 2 || (!shell && strcmp(argv[1], "--json"))) return 2;
    const char *profile = getenv("OCEAN_DEVICE_PROFILE");
    unsigned width = 0, height = 0;
    if (profile && (!strcmp(profile, "pw1") || !strcmp(profile, "pw2"))) {
        width = 758; height = 1024;
    } else if (profile && (!strcmp(profile, "pw3") || !strcmp(profile, "voyage"))) {
        width = 1072; height = 1448;
    }
    struct utsname name;
    int system = !uname(&name);
    int cpu = system && !strcmp(name.machine, "armv7l") && runtime_check();
    const char *branch = profile && !strcmp(profile, "pw1") ? "2.6.31" : "3.0.35";
    size_t branch_length = strlen(branch);
    int kernel = system && !strncmp(name.release, branch, branch_length) &&
        (!name.release[branch_length] || name.release[branch_length] == '-');
    struct fb_var_screeninfo v = {0};
    struct fb_fix_screeninfo f = {0};
    int fd = open("/dev/fb0", O_RDWR);
    int fb = fd >= 0 && !ioctl(fd, FBIOGET_VSCREENINFO, &v) &&
        !ioctl(fd, FBIOGET_FSCREENINFO, &f);
    if (fd >= 0) close(fd);
    // Read-only ioctls identify the EPDC framebuffer. No refresh, mmap or input grab occurs.
    int display = fb && width && kernel && v.xres == width && v.yres == height &&
        (v.bits_per_pixel == 8 || v.bits_per_pixel == 16) && !strncmp(f.id, "mxc_epdc_fb", 11) &&
        f.line_length >= v.xres * (v.bits_per_pixel / 8) &&
        f.smem_len >= (v.yres + v.yoffset) * f.line_length;
    int input = touch_device();
    int ready = cpu && display && input;
    if (shell) {
        printf("OCEAN_STATIC_RUNTIME=%d\nOCEAN_KERNEL_READY=%d\nOCEAN_DISPLAY_READY=%d\nOCEAN_INPUT_READY=%d\n"
               "OCEAN_HARDWARE_READY=%d\nOCEAN_FB_WIDTH=%u\nOCEAN_FB_HEIGHT=%u\nOCEAN_FB_BPP=%u\n",
               cpu, kernel, display, input, ready, v.xres, v.yres, v.bits_per_pixel);
    } else {
        printf("{\"staticRuntime\":%s,\"kernelReady\":%s,\"displayReady\":%s,\"inputReady\":%s,"
               "\"hardwareReady\":%s,\"width\":%u,\"height\":%u,\"bpp\":%u,"
               "\"screenUpdated\":false}\n", cpu ? "true" : "false", kernel ? "true" : "false", display ? "true" : "false",
               input ? "true" : "false", ready ? "true" : "false", v.xres, v.yres,
               v.bits_per_pixel);
    }
    return ready ? 0 : 1;
}
