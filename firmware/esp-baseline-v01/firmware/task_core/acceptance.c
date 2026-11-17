#include "task_core.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
 idc_task t;idc_token k,k2;idc_op op;
 idc_op sequence[]={IDC_RECORD_START,IDC_RECORD_STOP,IDC_AGENT_TASK};
 uint32_t caps=(1u<<IDC_RECORD_START)|(1u<<IDC_RECORD_STOP)|(1u<<IDC_AGENT_TASK);
 assert(idc_create(&t,1,45,sequence,3,0,1,true)==IDC_DENIED);
 assert(idc_create(&t,1,45,sequence,0,caps,1,true)==IDC_INVALID);
 assert(idc_create(&t,1,45,sequence,9,caps,1,true)==IDC_INVALID);
 idc_op bad[]={IDC_OP_COUNT};assert(idc_create(&t,1,45,bad,1,~0u,1,false)==IDC_INVALID);
 assert(idc_create(&t,1,45,sequence,3,caps,1,true)==IDC_OK);
 assert(idc_begin(&t,caps,1,&k,&op)==IDC_DENIED);
 assert(idc_approve(&t,2)==IDC_STALE);
 assert(idc_approve(&t,1)==IDC_OK);
 assert(idc_begin(&t,caps,2,&k,&op)==IDC_DENIED);
 assert(idc_begin(&t,0,1,&k,&op)==IDC_DENIED);
 assert(idc_begin(&t,caps,1,&k,&op)==IDC_OK && op==IDC_RECORD_START);
 assert(idc_begin(&t,caps,1,&k2,&op)==IDC_BUSY);
 assert(idc_result(&t,k,true,0)==IDC_INVALID);
 idc_task restored=t;idc_on_reboot(&restored);
 assert(restored.state==IDC_UNKNOWN);
 assert(idc_begin(&restored,caps,1,&k2,&op)==IDC_RECONCILE);
 assert(idc_result(&restored,k,true,101)==IDC_OK);
 assert(idc_result(&restored,k,true,101)==IDC_STALE);
 assert(idc_begin(&restored,caps,1,&k2,&op)==IDC_OK && op==IDC_RECORD_STOP);
 assert(idc_cancel(&restored)==IDC_RECONCILE && restored.state==IDC_UNKNOWN);
 assert(idc_result(&restored,k2,true,102)==IDC_OK);
 /* Cancellation remains sticky across asynchronous receipt reconciliation. */
 assert(restored.state==IDC_CANCELLED && restored.cancel_requested);
 assert(idc_begin(&restored,caps,1,&k2,&op)==IDC_TERMINAL);
 /* Full successful sequence, each receipt advances exactly one step. */
 assert(idc_create(&t,2,46,sequence,3,caps,3,false)==IDC_OK);
 for (int i=0;i<3;i++) {
  assert(idc_begin(&t,caps,3,&k,&op)==IDC_OK && op==sequence[i]);
  idc_token stale=k;stale.generation++;
  assert(idc_result(&t,stale,true,200+i)==IDC_STALE);
  assert(idc_result(&t,k,true,200+i)==IDC_OK);
 }
 assert(t.state==IDC_DONE && t.cursor==3);
 assert(idc_begin(&t,caps,3,&k,&op)==IDC_TERMINAL);
 printf("PASS: permission fence, explicit approval, stale/revoked auth, bounded steps, duplicate dispatch/receipt, missing receipt, reboot unknown recovery, cancel reconciliation, terminal states. task_record_bytes=%zu\n",sizeof(idc_task));
 return 0;
}
