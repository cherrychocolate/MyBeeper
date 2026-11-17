#include "task_core.h"
#include <string.h>
static bool terminal(idc_state s) {
 return s==IDC_DONE || s==IDC_FAILED || s==IDC_CANCELLED;
}
idc_rc idc_create(idc_task *t,uint32_t id,uint64_t fingerprint,
 const idc_op *steps,uint8_t count,uint32_t granted,uint32_t av,bool approval) {
 if (!t || !steps || !id || !av || count==0 || count>IDC_MAX_STEPS) return IDC_INVALID;
 uint32_t need=0;
 for (uint8_t i=0;i<count;i++) {
  if (steps[i]<0 || steps[i]>=IDC_OP_COUNT) return IDC_INVALID;
  need |= 1u << steps[i];
 }
 if ((need & granted)!=need) return IDC_DENIED;
 memset(t,0,sizeof(*t));t->schema_version=1;t->task_id=id;
 t->payload_fingerprint=fingerprint;t->generation=1;
 t->required_caps=need;t->granted_caps=granted;t->authorization_version=av;
 t->count=count;t->approval_required=approval;t->approved=!approval;
 memcpy(t->steps,steps,count*sizeof(*steps));
 t->state=approval ? IDC_WAIT_APPROVAL : IDC_READY;
 return IDC_OK;
}
idc_rc idc_approve(idc_task *t,uint32_t av) {
 if (!t || t->state!=IDC_WAIT_APPROVAL) return IDC_INVALID;
 if (av!=t->authorization_version) return IDC_STALE;
 t->approved=true;t->state=IDC_READY;return IDC_OK;
}
idc_rc idc_begin(idc_task *t,uint32_t live,uint32_t av,idc_token *token,idc_op *op) {
 if (!t || !token || !op) return IDC_INVALID;
 if (terminal(t->state)) return IDC_TERMINAL;
 if (t->state==IDC_RUNNING) return IDC_BUSY;
 if (t->state==IDC_UNKNOWN) return IDC_RECONCILE;
 if (t->state!=IDC_READY || !t->approved) return IDC_DENIED;
 if (av!=t->authorization_version || (t->required_caps & live)!=t->required_caps)
  return IDC_DENIED;
 if (t->cursor>=t->count) return IDC_INVALID;
 *token=(idc_token){t->task_id,t->generation,t->cursor};
 *op=t->steps[t->cursor];t->state=IDC_RUNNING;return IDC_OK;
}
idc_rc idc_result(idc_task *t,idc_token k,bool success,uint32_t receipt) {
 if (!t) return IDC_INVALID;
 if (k.task_id!=t->task_id || k.generation!=t->generation || k.step!=t->cursor)
  return IDC_STALE;
 if (t->state!=IDC_RUNNING && t->state!=IDC_UNKNOWN) return IDC_STALE;
 /* Success must come from an authenticated adapter receipt, not an LLM claim. */
 if (success && receipt==0) return IDC_INVALID;
 t->receipt_id=receipt;
 if (!success) {t->state=IDC_FAILED;return IDC_OK;}
 ++t->cursor;
 t->state=t->cursor==t->count ? IDC_DONE : (t->cancel_requested ? IDC_CANCELLED : IDC_READY);return IDC_OK;
}
idc_rc idc_cancel(idc_task *t) {
 if (!t) return IDC_INVALID;
 if (terminal(t->state)) return IDC_TERMINAL;
 /* Cancel suppresses future steps; an already-dispatched effect may have happened. */
 if (t->state==IDC_RUNNING || t->state==IDC_UNKNOWN) {
  t->cancel_requested=true;t->state=IDC_UNKNOWN;return IDC_RECONCILE;
 }
 t->state=IDC_CANCELLED;return IDC_OK;
}
void idc_on_reboot(idc_task *t) {
 /* Caller restores and validates the durable journal before invoking this hook. */
 if (t && t->state==IDC_RUNNING) t->state=IDC_UNKNOWN;
}
