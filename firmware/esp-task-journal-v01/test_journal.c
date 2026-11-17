#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE 1
#include "task_journal.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
typedef struct {uint8_t bytes[64];bool exists;int failure;unsigned writes;} flash;
static int rd(void *v,uint8_t *p,size_t n){flash *f=v;if(!f->exists)return 1;assert(n==64);memcpy(p,f->bytes,n);return 0;}
static int wr(void *v,const uint8_t *p,size_t n){
 flash *f=v;f->writes++;if(f->failure==1)return -1;
 assert(n==64);memcpy(f->bytes,p,n);f->exists=true;return f->failure==2?-1:0;
}
static idc_store open_store(flash *f){idc_store s;assert(idc_store_open(&s,(idc_journal_io){f,rd,wr})==0);return s;}
static void create(idc_store *s,bool approval){
 idc_op ops[]={IDC_IR_SEND,IDC_NFC_EXCHANGE};
 assert(idc_store_create(s,7,0x1122334455667788ULL,ops,2,(1u<<IDC_IR_SEND)|(1u<<IDC_NFC_EXCHANGE),9,approval)==0);
}
static idc_token begin(idc_store *s){idc_token k={0};idc_op op;assert(idc_store_begin(s,UINT32_MAX,9,&k,&op)==0);assert(op==IDC_IR_SEND);return k;}
static int disk_read(void *v,uint8_t *p,size_t n){
 FILE *f=fopen(v,"rb");if(!f)return 1;
 size_t got=fread(p,1,n,f);int extra=fgetc(f);fclose(f);return got==n&&extra==EOF?0:-1;
}
static int disk_save(void *v,const uint8_t *p,size_t n){
 const char *path=v;char tmp[512];int l=snprintf(tmp,sizeof tmp,"%s.tmp",path);
 if(l<0||(size_t)l>=sizeof tmp)return -1;
 FILE *f=fopen(tmp,"wb");if(!f)return -1;
 bool ok=fwrite(p,1,n,f)==n;
 if(ok)ok=fflush(f)==0;
 if(ok)ok=fsync(fileno(f))==0;
 if(fclose(f))ok=false;
 if(!ok)return -1;
 return rename(tmp,path)==0?0:-1;
}
int main(void){
 flash f={0};idc_store s=open_store(&f);create(&s,true);
 s=open_store(&f);assert(s.task.state==IDC_WAIT_APPROVAL);
 idc_token k={99,99,99};idc_op op=IDC_PHONE_CALL;
 unsigned n=f.writes;assert(idc_store_begin(&s,UINT32_MAX,9,&k,&op)==IDC_DENIED&&f.writes==n);
 assert(idc_store_approve(&s,8)==IDC_STALE&&f.writes==n);
 assert(idc_store_approve(&s,9)==0);s=open_store(&f);assert(s.task.approved);
 assert(idc_store_begin(&s,0,9,&k,&op)==IDC_DENIED);
 puts("PASS durable approval and live-capability checks");
 for(int failure=1;failure<=2;failure++){
  f=(flash){0};s=open_store(&f);create(&s,false);f.failure=failure;
  k=(idc_token){99,99,99};op=IDC_PHONE_CALL;
  assert(idc_store_begin(&s,UINT32_MAX,9,&k,&op)==IDC_JOURNAL_IO);
  assert(k.task_id==99&&k.generation==99&&k.step==99&&op==IDC_PHONE_CALL);
  assert(idc_store_begin(&s,UINT32_MAX,9,&k,&op)==IDC_JOURNAL_IO);
  f.failure=0;s=open_store(&f);
  if(failure==2)assert(s.task.state==IDC_UNKNOWN&&idc_store_begin(&s,UINT32_MAX,9,&k,&op)==IDC_RECONCILE);
  else assert(s.task.state==IDC_READY);
 }
 puts("PASS failed/ambiguous intent commit exposes no dispatch token; restart never replays uncertain intent");
 f=(flash){0};s=open_store(&f);create(&s,false);k=begin(&s);
 assert(idc_store_begin(&s,UINT32_MAX,9,&k,&op)==IDC_BUSY);
 assert(idc_store_cancel(&s)==IDC_RECONCILE);s=open_store(&f);
 assert(s.task.state==IDC_UNKNOWN&&s.task.cancel_requested);
 assert(idc_store_result(&s,k,true,10)==0);s=open_store(&f);
 assert(s.task.state==IDC_CANCELLED&&s.task.cursor==1);
 assert(idc_store_begin(&s,UINT32_MAX,9,&k,&op)==IDC_TERMINAL);
 puts("PASS durable sticky cancellation with late receipt suppresses next effect");
 for(int failure=1;failure<=2;failure++){
  f=(flash){0};s=open_store(&f);create(&s,false);k=begin(&s);
  n=f.writes;assert(idc_store_result(&s,k,true,0)==IDC_INVALID&&f.writes==n);
  f.failure=failure;assert(idc_store_result(&s,k,true,20)==IDC_JOURNAL_IO);
  f.failure=0;s=open_store(&f);
  if(failure==1){assert(s.task.state==IDC_UNKNOWN);assert(idc_store_result(&s,k,true,20)==0);}
  assert(s.task.cursor==1&&s.task.state==IDC_READY);
  assert(idc_store_result(&s,k,true,20)==IDC_STALE);
 }
 puts("PASS receipt commit failures reconcile without losing or repeating completed step");
 uint8_t wire[64];assert(idc_journal_encode(&s.task,wire)==0);
 for(unsigned i=0;i<64;i++){
  flash bad={.exists=true};memcpy(bad.bytes,wire,64);bad.bytes[i]^=1;
  idc_store corrupt;assert(idc_store_open(&corrupt,(idc_journal_io){&bad,rd,wr})==IDC_JOURNAL_CORRUPT);
  assert(corrupt.faulted&&!corrupt.present);
 }
 idc_task invalid=s.task;invalid.required_caps=0;assert(idc_journal_encode(&invalid,wire)==IDC_JOURNAL_CORRUPT);
 puts("PASS all 64 single-byte corruption positions and inconsistent task rejected");
 /* Real process exit plus filesystem restore, not a simulated hardware brownout. */
 char dir[]="/tmp/id-card-journal-XXXXXX";assert(mkdtemp(dir));char path[512];
 assert(snprintf(path,sizeof path,"%s/task.bin",dir)>0);
 pid_t child=fork();assert(child>=0);
 if(child==0){
  idc_store a;assert(idc_store_open(&a,(idc_journal_io){path,disk_read,disk_save})==0);
  idc_op one=IDC_IR_SEND;assert(idc_store_create(&a,42,123,&one,1,1u<<IDC_IR_SEND,1,false)==0);
  idc_token token;idc_op action;assert(idc_store_begin(&a,UINT32_MAX,1,&token,&action)==0);
  _exit(0); /* persisted RUNNING; effect deliberately not executed */
 }
 int status;assert(waitpid(child,&status,0)==child&&WIFEXITED(status)&&WEXITSTATUS(status)==0);
 idc_store a;assert(idc_store_open(&a,(idc_journal_io){path,disk_read,disk_save})==0);
 assert(a.task.state==IDC_UNKNOWN);k=(idc_token){42,1,0};
 assert(idc_store_begin(&a,UINT32_MAX,1,&k,&op)==IDC_RECONCILE);
 assert(idc_store_result(&a,k,true,55)==0);
 assert(idc_store_open(&a,(idc_journal_io){path,disk_read,disk_save})==0&&a.task.state==IDC_DONE);
 idc_op one=IDC_IR_SEND;assert(idc_store_create(&a,42,123,&one,1,UINT32_MAX,1,false)==IDC_BUSY);
 assert(unlink(path)==0&&rmdir(dir)==0);
 puts("PASS real child-process exit/file restore to UNKNOWN, durable DONE and retained ID record");
 return 0;
}
