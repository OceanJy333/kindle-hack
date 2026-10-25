// input: native zImage, initramfs and live boot identity; output: checked kexec segments; pos: RAM boot loader.
// SPDX-License-Identifier: GPL-2.0-only
#include "protocol.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>

struct segment {void *buffer;size_t bytes;unsigned long address;size_t allocated;};
struct load {unsigned long entry,count;struct segment *segments;unsigned long flags;};
_Static_assert(sizeof(void *)==4&&sizeof(struct segment)==16,"ARM32 loader required");
static unsigned int tags[1024];static size_t tag_bytes;
static void die(const char *reason){perror(reason);exit(1);}
static size_t page(size_t n){return (n+4095)&~4095UL;}
static void tag(unsigned type,const void *payload,size_t size){
 size_t bytes=(size+3)&~3UL;
 if(tag_bytes+bytes+8>sizeof(tags)){errno=E2BIG;die("boot tags");}
 unsigned int *head=(void *)((unsigned char *)tags+tag_bytes);
 head[0]=type?(bytes+8)/4:0;head[1]=type;
 if(size)memcpy(head+2,payload,size);tag_bytes+=bytes+8;
}
static void *read_image(const char *path,size_t maximum,size_t *size){
 struct stat state;int fd=open(path,O_RDONLY|O_CLOEXEC);if(fd<0)die("open image");
 if(fstat(fd,&state)||state.st_size<=0||(unsigned long)state.st_size>maximum){errno=EFBIG;die("image size");}
 *size=state.st_size;void *data=malloc(*size);if(!data)die("image memory");
 size_t done=0;while(done<*size){ssize_t n=read(fd,(char *)data+done,*size-done);if(n<=0)die("read image");done+=n;}
 close(fd);return data;
}
static void build_tags(struct ocean_boot_info *info,const char *command,size_t initrd){
 unsigned core[]={1,4096,0},memory[]={512U*1024*1024,0x80000000},ramdisk[]={0x88000000,initrd};
 unsigned serial[]={info->serial_low,info->serial_high};unsigned char mac[32],boot[32];
 if(strlen(command)>1023){errno=E2BIG;die("command line");}
 memcpy(mac,info->mac,12);memcpy(mac+12,info->secret,20);
 memcpy(boot,info->boot,16);memcpy(boot+16,info->postmode,16);
 tag(0x54410001,core,sizeof(core));tag(0x54410002,memory,sizeof(memory));
 tag(0x54420005,ramdisk,sizeof(ramdisk));tag(0x54410006,serial,sizeof(serial));
 tag(0x54410007,&info->revision,4);tag(0x5441000a,info->serial,16);tag(0x5441000b,info->board,16);
 tag(0x5441000c,&info->post,4);tag(0x5441000d,mac,sizeof(mac));tag(0x5441000f,boot,sizeof(boot));
 tag(0x54410010,&info->ddr,4);tag(0x54410011,info->btmac,12);
 tag(0x54410009,command,strlen(command)+1);tag(0,NULL,0);
}
int main(int argc,char **argv){
 if(argc<2){fputs("Use probe | unload | load ZIMAGE INITRAMFS COMMANDLINE | execute\n",stderr);return 2;}
 int fd=open("/dev/kexec",O_RDWR|O_CLOEXEC);if(fd<0)die("kernel loader");
 struct ocean_boot_info info={0};
 if(ioctl(fd,OCEAN_KEXEC_INFO,&info)||info.version!=OCEAN_KEXEC_ABI){errno=EPROTO;die("boot identity ABI");}
 if(!strcmp(argv[1],"probe")){
  if(!info.board[0]||!info.serial[0]){errno=ENODATA;die("boot identity");}
  printf("{\"bootInfoAvailable\":true,\"abi\":%u,\"machineType\":%u}\n",info.version,info.machine);
 }else if(!strcmp(argv[1],"unload")){
  struct load empty={0};if(ioctl(fd,OCEAN_KEXEC_LOAD,&empty))die("unload image");puts("Image unloaded");
 }else if(!strcmp(argv[1],"execute")){
  sync();if(ioctl(fd,OCEAN_KEXEC_EXEC))die("execute image");
 }else if(!strcmp(argv[1],"load")&&argc==5){
  size_t kernel_bytes,initrd_bytes;
  void *kernel=read_image(argv[2],16*1024*1024,&kernel_bytes);
  void *initrd=read_image(argv[3],32*1024*1024,&initrd_bytes);
  if(kernel_bytes<0x30||memcmp((char *)kernel+0x24,"\x18\x28\x6f\x01",4)){errno=ENOEXEC;die("zImage header");}
  if(initrd_bytes<110||memcmp(initrd,"070701",6)){errno=ENOEXEC;die("initramfs newc");}
  if(!info.board[0]||!info.serial[0]){errno=ENODATA;die("boot identity");}
  build_tags(&info,argv[4],initrd_bytes);
  struct segment segments[]={{kernel,kernel_bytes,0x80008000,page(kernel_bytes)},
   {tags,tag_bytes,0x80001000,page(tag_bytes)},{initrd,initrd_bytes,0x88000000,page(initrd_bytes)}};
  struct load request={0x80008000,3,segments,OCEAN_KEXEC_ARM};
  if(ioctl(fd,OCEAN_KEXEC_LOAD,&request))die("load image");
  printf("{\"imageLoaded\":true,\"kernelBytes\":%zu,\"initramfsBytes\":%zu,\"tagBytes\":%zu}\n",kernel_bytes,initrd_bytes,tag_bytes);
  free(kernel);free(initrd);
 }else {fputs("Invalid loader command\n",stderr);return 2;}
 close(fd);return 0;
}
