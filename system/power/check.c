// input: injected wake races; output: actual daemon lock-transition checks; pos: ARM power regression.
#include "hardware.h"
#define CHECK(test) do{if(!(test)){fprintf(stderr,"POWER_CHECK_FAILED line %d: %s\n",__LINE__,#test);return 1;}}while(0)
static long long test_now=1000;
static int test_error=EBUSY,test_light=100,test_attempts=0;
static long long fake_milliseconds(void){return test_now;}
static int fake_write_number(const char *path,long value){(void)path;test_light=value;return 0;}
static int fake_read_number(const char *path,int fallback){(void)path;return fallback;}
static void fake_sample(const char *phase){(void)phase;}
static int fake_suspend(void){test_attempts++;errno=test_error;return test_error?-1:0;}
#define milliseconds fake_milliseconds
#define write_number fake_write_number
#define read_number fake_read_number
#define sample fake_sample
#define suspend_memory fake_suspend
#define main daemon_main
#include "powerd.c"
#undef main
int main(void){
 for(int i=0;i<CLIENTS;i++)clients[i].fd=-1;
 light=100;
 command("sleep");command("ready");try_suspend();
 CHECK(locked&&ready&&test_light==0&&sleep_at==test_now+1000);
 for(int i=0;i<8;i++){
  long long previous=sleep_at;test_now=previous;try_suspend();
  CHECK(locked&&test_light==0&&sleep_at>test_now&&sleep_at-test_now<=30000);
 }
 test_error=0;test_now=sleep_at;try_suspend();
 CHECK(!locked&&!sleep_at&&test_light==100);
 test_error=EAGAIN;command("sleep");command("ready");try_suspend();
 CHECK(locked&&sleep_at==test_now+1000);
 command("wake");CHECK(!locked&&!sleep_at&&test_light==100);
 test_error=EIO;command("sleep");command("ready");try_suspend();
 CHECK(locked&&!sleep_at&&test_light==0);
 command("wake");CHECK(!locked&&test_light==100);
 command("sleep");try_suspend();CHECK(!locked&&!sleep_at);
 printf("POWER_CHECK_PASSED: wake race, capped retry, successful resume, explicit wake, permanent error, display timeout (%d suspend calls)\n",test_attempts);
 return 0;
}
