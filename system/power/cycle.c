// input: bounded RTC wake interval; output: suspend/resume measurements; pos: standalone power hardware acceptance.
#include "hardware.h"
int main(int argc,char **argv){
 if(argc==1){sample("probe");return 0;}
 char *end;long seconds=strtol(argv[1],&end,10);
 if(*end||seconds<3||seconds>60){fprintf(stderr,"Wake interval must be 3..60 seconds\n");return 2;}
 int old_alarm=read_number(ALARM,0),old_light=read_number(LIGHT,-1);
 if(old_alarm>time(NULL)){fprintf(stderr,"An existing RTC wake alarm is active\n");return 3;}
 if(old_light<0||write_number(ALARM,0)||write_number(ALARM,time(NULL)+seconds)){perror("arm RTC");return 4;}
 sample("before");
 if(write_number(LIGHT,0)){perror("frontlight off");write_number(ALARM,0);return 5;}
 time_t start=time(NULL);int rc=suspend_memory();int failure=errno;
 write_number(LIGHT,old_light);write_number(ALARM,0);sample("after");
 printf("{\"suspend_rc\":%d,\"errno\":%d,\"elapsed_seconds\":%ld}\n",rc,rc?failure:0,(long)(time(NULL)-start));
 return rc?6:0;
}
