// input: power command; output: one JSON response; pos: local service diagnostic client.
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
static int response(int fd){
 struct pollfd p={fd,POLLIN,0};char c;
 while(poll(&p,1,15000)>0&&read(fd,&c,1)==1){if(write(1,&c,1)!=1)return 1;if(c=='\n')return 0;}
 return 1;
}
int main(int argc,char **argv){
 int fd=socket(AF_UNIX,SOCK_STREAM,0);struct sockaddr_un a={.sun_family=AF_UNIX};
 snprintf(a.sun_path,sizeof(a.sun_path),"/tmp/ocean-power.sock");
 if(fd<0||connect(fd,(struct sockaddr*)&a,sizeof(a))){perror("power socket");return 1;}
 int rc=response(fd);
 if(!rc&&argc==2){
  char line[128];int n=snprintf(line,sizeof(line),"%s\n",argv[1]);
  if(n<0||n>=(int)sizeof(line)||write(fd,line,n)!=n)rc=1;else rc=response(fd);
 }
 close(fd);return rc;
}
