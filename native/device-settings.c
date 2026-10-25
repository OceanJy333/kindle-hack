// input: power_supply and powerd; output: device telemetry and bounded frontlight changes; pos: Voyage settings adapter.
#include "device-settings.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
static int property(const char *name){
    char command[128];snprintf(command,sizeof(command),"lipc-get-prop com.lab126.powerd %s 2>/dev/null",name);
    FILE *f=popen(command,"r");int value=-1;if(f){if(fscanf(f,"%d",&value)!=1)value=-1;pclose(f);}return value;
}
int device_status(int fd){
    int percent=-1;char status[32]="Unknown",line[128];
    FILE *f=fopen("/sys/class/power_supply/max77696-battery/capacity","r");
    if(f){if(fscanf(f,"%d",&percent)!=1)percent=-1;fclose(f);}
    f=fopen("/sys/class/power_supply/max77696-battery/status","r");if(f){if(!fgets(status,sizeof(status),f))strcpy(status,"Unknown");fclose(f);}
    int charging=!strncmp(status,"Charging",8)?1:!strncmp(status,"Full",4)?2:0;
    int n=snprintf(line,sizeof(line),"DEVICE %d %d %d %d\n",percent,property("flIntensity"),property("flAuto"),charging);
    return send(fd,line,n,MSG_NOSIGNAL)==n?0:-1;
}
int device_light(int level,int automatic){
    if(level<0||level>24||automatic<0||automatic>1)return -1;
    char command[192];
    // The service owns brightness; disable ambient control before a manual level.
    if(automatic)snprintf(command,sizeof(command),"lipc-set-prop com.lab126.powerd flAuto 1");
    else snprintf(command,sizeof(command),"lipc-set-prop com.lab126.powerd flAuto 0 && lipc-set-prop com.lab126.powerd flIntensity %d",level);
    return system(command);
}
