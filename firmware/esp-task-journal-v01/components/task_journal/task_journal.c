#include "task_journal.h"
#include <string.h>
static void put32(uint8_t *p,uint32_t n){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(n>>(i*8));}
static uint32_t get32(const uint8_t *p){uint32_t n=0;for(unsigned i=0;i<4;i++)n|=(uint32_t)p[i]<<(i*8);return n;}
static uint32_t crc32(const uint8_t *p,size_t n){
 uint32_t c=UINT32_MAX;
 for(size_t i=0;i<n;i++){c^=p[i];for(unsigned j=0;j<8;j++)c=(c>>1)^((c&1u)?0xedb88320u:0u);}
 return ~c;
}
static bool valid(const idc_task *t){
 if(!t||t->schema_version!=1||!t->task_id||!t->generation||!t->authorization_version||
    !t->count||t->count>IDC_MAX_STEPS||t->cursor>t->count||
    t->state<IDC_WAIT_APPROVAL||t->state>IDC_CANCELLED)return false;
 uint32_t caps=0;
 for(unsigned i=0;i<t->count;i++){
  if(t->steps[i]<0||t->steps[i]>=IDC_OP_COUNT)return false;
  caps|=1u<<t->steps[i];
 }
 if(caps!=t->required_caps||(caps&t->granted_caps)!=caps)return false;
 if(t->state==IDC_DONE){if(t->cursor!=t->count||!t->receipt_id)return false;}
 else if(t->cursor>=t->count)return false;
 if(t->state==IDC_WAIT_APPROVAL){
  if(!t->approval_required||t->approved||t->cursor)return false;
 }else if(t->state!=IDC_CANCELLED&&!t->approved)return false;
 if(!t->approval_required&&!t->approved)return false;
 if(t->cancel_requested && t->state!=IDC_UNKNOWN && t->state!=IDC_CANCELLED &&
    t->state!=IDC_DONE && t->state!=IDC_FAILED)return false;
 return true;
}
int idc_journal_encode(const idc_task *t,uint8_t out[IDC_JOURNAL_BYTES]){
 if(!out||!valid(t))return IDC_JOURNAL_CORRUPT;
 memset(out,0,IDC_JOURNAL_BYTES);memcpy(out,"IDC1",4);out[4]=1;out[6]=64;
 put32(out+8,t->task_id);put32(out+12,t->generation);
 put32(out+16,(uint32_t)t->payload_fingerprint);put32(out+20,(uint32_t)(t->payload_fingerprint>>32));
 put32(out+24,t->required_caps);put32(out+28,t->granted_caps);
 put32(out+32,t->authorization_version);put32(out+36,t->receipt_id);
 out[40]=t->count;out[41]=t->cursor;out[42]=(uint8_t)t->state;
 out[43]=(uint8_t)((t->approval_required?1:0)|(t->approved?2:0)|(t->cancel_requested?4:0));
 for(unsigned i=0;i<t->count;i++)out[44+i]=(uint8_t)t->steps[i];
 put32(out+60,crc32(out,60));return 0;
}
int idc_journal_decode(const uint8_t in[IDC_JOURNAL_BYTES],idc_task *out){
 if(!in||!out||memcmp(in,"IDC1",4)||in[4]!=1||in[5]||in[6]!=64||in[7]||
    (in[43]&~7u)||get32(in+60)!=crc32(in,60))return IDC_JOURNAL_CORRUPT;
 for(unsigned i=52;i<60;i++)if(in[i])return IDC_JOURNAL_CORRUPT;
 if(in[40]>IDC_MAX_STEPS)return IDC_JOURNAL_CORRUPT;
 for(unsigned i=44+in[40];i<52;i++)if(in[i])return IDC_JOURNAL_CORRUPT;
 idc_task t={0};t.schema_version=1;t.task_id=get32(in+8);t.generation=get32(in+12);
 t.payload_fingerprint=(uint64_t)get32(in+16)|((uint64_t)get32(in+20)<<32);
 t.required_caps=get32(in+24);t.granted_caps=get32(in+28);t.authorization_version=get32(in+32);
 t.receipt_id=get32(in+36);t.count=in[40];t.cursor=in[41];t.state=(idc_state)in[42];
 t.approval_required=(in[43]&1u)!=0;t.approved=(in[43]&2u)!=0;t.cancel_requested=(in[43]&4u)!=0;
 for(unsigned i=0;i<t.count;i++)t.steps[i]=(idc_op)in[44+i];
 if(!valid(&t))return IDC_JOURNAL_CORRUPT;
 *out=t;return 0;
}
static int persist(idc_store *s,const idc_task *t){
 uint8_t wire[IDC_JOURNAL_BYTES];int rc=idc_journal_encode(t,wire);
 if(rc){s->faulted=true;return rc;}
 if(s->io.save(s->io.ctx,wire,sizeof wire)){s->faulted=true;return IDC_JOURNAL_IO;}
 s->task=*t;s->present=true;return 0;
}
int idc_store_open(idc_store *s,idc_journal_io io){
 if(!s||!io.read||!io.save)return IDC_INVALID;
 memset(s,0,sizeof(*s));s->io=io;uint8_t wire[IDC_JOURNAL_BYTES];
 int rc=io.read(io.ctx,wire,sizeof wire);
 if(rc==1)return 0;
 s->faulted=true;
 if(rc)return IDC_JOURNAL_IO;
 idc_task t;rc=idc_journal_decode(wire,&t);if(rc)return rc;
 /* A durable RUNNING intent may have executed before power loss. Never replay. */
 bool running=t.state==IDC_RUNNING;idc_on_reboot(&t);
 if(running){rc=persist(s,&t);if(rc)return rc;}
 else{s->task=t;s->present=true;}
 s->faulted=false;return 0;
}
static int usable(idc_store *s){return !s?IDC_INVALID:(s->faulted?IDC_JOURNAL_IO:(!s->present?IDC_INVALID:0));}
int idc_store_create(idc_store *s,uint32_t id,uint64_t fp,const idc_op *ops,uint8_t n,
                     uint32_t caps,uint32_t av,bool approval){
 if(!s)return IDC_INVALID;
 if(s->faulted)return IDC_JOURNAL_IO;
 /* Retain even terminal records; archive/rotation protocol is not implemented. */
 if(s->present)return IDC_BUSY;
 idc_task t;int rc=idc_create(&t,id,fp,ops,n,caps,av,approval);
 return rc?rc:persist(s,&t);
}
int idc_store_approve(idc_store *s,uint32_t av){
 int rc=usable(s);if(rc)return rc;idc_task t=s->task;rc=idc_approve(&t,av);
 return rc?rc:persist(s,&t);
}
int idc_store_begin(idc_store *s,uint32_t caps,uint32_t av,idc_token *token,idc_op *op){
 int rc=usable(s);if(rc)return rc;if(!token||!op)return IDC_INVALID;
 idc_task t=s->task;idc_token k;idc_op action;rc=idc_begin(&t,caps,av,&k,&action);
 if(rc)return rc;
 rc=persist(s,&t);
 if(!rc){*token=k;*op=action;}return rc;
}
int idc_store_result(idc_store *s,idc_token k,bool ok,uint32_t receipt){
 int rc=usable(s);if(rc)return rc;idc_task t=s->task;rc=idc_result(&t,k,ok,receipt);
 return rc?rc:persist(s,&t);
}
int idc_store_cancel(idc_store *s){
 int rc=usable(s);if(rc)return rc;idc_task t=s->task;rc=idc_cancel(&t);
 /* RECONCILE also mutates state: cancellation intent must survive reboot. */
 if(rc!=IDC_OK&&rc!=IDC_RECONCILE)return rc;
 int saved=persist(s,&t);return saved?saved:rc;
}
