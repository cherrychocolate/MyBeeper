#include "epd_e6.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef struct {int idle,fail_command,cmd;size_t frame;unsigned elapsed,writes;uint8_t commands[128];unsigned n;} mock;
static int wr(void *v,int data,const uint8_t *p,size_t n){mock*m=v;m->writes++;
 if(!data){m->cmd=p[0];assert(m->n<128);m->commands[m->n++]=p[0];if(m->cmd==m->fail_command)return -1;}
 else if(m->cmd==0x10)m->frame+=n;
 return 0;}
static int busy(void*v){return ((mock*)v)->idle;}
static void delay(void*v,unsigned ms){((mock*)v)->elapsed+=ms;}
static void reset(void*v,int x){(void)v;(void)x;}
static epd_e6 device(mock*m){epd_e6 d={.io={m,wr,busy,delay,reset},.timeout_ms=40};return d;}
int main(void){
 static uint8_t f[E6_FRAME_BYTES];memset(f,0x11,sizeof f);
 mock m={.idle=1,.fail_command=-1};epd_e6 d=device(&m);
 assert(e6_display(&d,f,sizeof f)==E6_STATE);
 assert(e6_init(&d)==0&&d.ready&&!d.known);
 unsigned before=m.n;assert(e6_display(&d,f,sizeof f)==0&&d.known);
 assert(m.frame==120000&&m.n==before+5);
 uint8_t want[]={0x10,0x04,0x06,0x12,0x02};assert(!memcmp(m.commands+before,want,5));
 unsigned writes=m.writes;f[10]=0x41;assert(e6_display(&d,f,sizeof f)==E6_ARG&&d.known&&m.writes==writes);f[10]=0x11;
 assert(e6_display(&d,f,sizeof f-1)==E6_ARG&&m.writes==writes);
 m.fail_command=0x02;assert(e6_display(&d,f,sizeof f)==E6_IO&&!d.known&&!d.ready);
 m=(mock){.idle=0,.fail_command=-1};d=device(&m);assert(e6_init(&d)==E6_TIMEOUT&&!d.ready&&!d.known);assert(m.elapsed==460);
 m=(mock){.idle=-1,.fail_command=-1};d=device(&m);assert(e6_init(&d)==E6_IO);
 m=(mock){.idle=1,.fail_command=-1};d=device(&m);assert(!e6_init(&d));m.idle=0;assert(e6_display(&d,f,sizeof f)==E6_TIMEOUT&&!d.ready&&!d.known);
 m=(mock){.idle=1,.fail_command=-1};d=device(&m);assert(!e6_init(&d));assert(!e6_display(&d,f,sizeof f));assert(!e6_sleep(&d)&&!d.ready&&d.known);assert(e6_display(&d,f,sizeof f)==E6_STATE);
 puts("Host checks passed: frame length/palette/order, BUSY timeout/read error, power-off failure, stale-image state, sleep/reinit. No target or panel tested.");return 0;
}
