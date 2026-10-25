// input: native framebuffer and optional waveform; output: PGM pixels or refresh timings; pos: physical display acceptance.
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
struct rect {uint32_t top,left,width,height;};
struct alternate {uint32_t address,width,height;struct rect region;};
struct update {
 struct rect region;uint32_t waveform,mode,marker,bw,gray;int32_t temperature;
 uint32_t flags;struct alternate alternate;
};
static void fail(const char *message){perror(message);exit(1);}
static long long microseconds(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec*1000000LL+t.tv_nsec/1000;}
int main(int argc,char **argv){
 int fd=open("/dev/fb0",O_RDWR|O_CLOEXEC);if(fd<0)fail("framebuffer");
 struct fb_var_screeninfo v;struct fb_fix_screeninfo f;
 if(ioctl(fd,FBIOGET_VSCREENINFO,&v)||ioctl(fd,FBIOGET_FSCREENINFO,&f))fail("screen information");
 fprintf(stderr,"{\"width\":%u,\"height\":%u,\"stride\":%u,\"bpp\":%u,\"offsetX\":%u,\"offsetY\":%u}\n",
  v.xres,v.yres,f.line_length,v.bits_per_pixel,v.xoffset,v.yoffset);
 if(argc==2&&!strcmp(argv[1],"capture")){
  if(v.bits_per_pixel!=8||!v.xres||!v.yres||(uint64_t)(v.yres+v.yoffset)*f.line_length>f.smem_len)return 2;
  unsigned char *pixels=mmap(NULL,f.smem_len,PROT_READ,MAP_SHARED,fd,0);if(pixels==MAP_FAILED)fail("map pixels");
  printf("P5\n%u %u\n255\n",v.xres,v.yres);
  for(unsigned y=0;y<v.yres;y++)if(fwrite(pixels+(y+v.yoffset)*f.line_length+v.xoffset,1,v.xres,stdout)!=v.xres)fail("write pixels");
  munmap(pixels,f.smem_len);
 }else if(argc==3&&!strcmp(argv[1],"refresh")){
  char *end;long waveform=strtol(argv[2],&end,10);if(*end||(waveform!=1&&waveform!=2&&waveform!=4))return 2;
  for(unsigned i=0;i<8;i++){
   struct update u={.region={0,0,v.xres,v.yres},.waveform=waveform,.marker=0x5a000000+i,
    .bw=1,.gray=2,.temperature=0x1001,.flags=waveform==4?2:0};
   struct {uint32_t marker,collision;} done={u.marker,0};long long start=microseconds();
   if(ioctl(fd,_IOW('F',0x2e,struct update),&u)<0)fail("submit refresh");
   int result=ioctl(fd,_IOWR('F',0x2f,typeof(done)),&done);if(result<0)fail("wait refresh");
   printf("{\"waveform\":%ld,\"microseconds\":%lld,\"waitResult\":%d}\n",waveform,microseconds()-start,result);fflush(stdout);
  }
 }else if(argc!=1)return 2;
 close(fd);return 0;
}
