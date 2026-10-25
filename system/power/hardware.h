// input: Voyage sysfs; output: checked hardware access; pos: standalone power hardware boundary.
#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#define BATTERY "/sys/class/power_supply/max77696-battery/"
#define LIGHT "/sys/class/backlight/max77696-bl/brightness"
#define ALARM "/sys/class/rtc/rtc0/wakealarm"
static int read_number(const char *path,int fallback){
 FILE *f=fopen(path,"r");int n;if(!f)return fallback;
 int ok=fscanf(f,"%d",&n);fclose(f);return ok==1?n:fallback;
}
static int write_text(const char *path,const char *text){
 int fd=open(path,O_WRONLY|O_CLOEXEC);if(fd<0)return -1;
 size_t n=strlen(text);ssize_t written=write(fd,text,n);int saved=(written>=0&&written!=(ssize_t)n)?EIO:errno;close(fd);errno=saved;
 return written==(ssize_t)n?0:-1;
}
static int write_number(const char *path,long n){char b[40];snprintf(b,sizeof(b),"%ld\n",n);return write_text(path,b);}
static long long milliseconds(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (long long)t.tv_sec*1000+t.tv_nsec/1000000;}
static void sample(const char *phase){
 int mv=read_number(BATTERY "voltage_now",0),ma=read_number(BATTERY "current_now",0);
 printf("{\"phase\":\"%s\",\"epoch\":%ld,\"capacity\":%d,\"mV\":%d,\"mA\":%d,\"mW\":%.3f,\"tempF\":%d,\"brightness\":%d}\n",
 phase,(long)time(NULL),read_number(BATTERY "capacity",-1),mv,ma,-mv*(double)ma/1000,
 read_number(BATTERY "temp",0),read_number(LIGHT,-1));fflush(stdout);
}
static int suspend_memory(void){
 // wakeup_count rejects a suspend if an interrupt arrived after the snapshot.
 FILE *f=fopen("/sys/power/wakeup_count","r");char count[40];
 if(!f)return -1;char *ok=fgets(count,sizeof(count),f);fclose(f);
 if(!ok||write_text("/sys/power/wakeup_count",count))return -1;
 return write_text("/sys/power/state","mem\n");
}
