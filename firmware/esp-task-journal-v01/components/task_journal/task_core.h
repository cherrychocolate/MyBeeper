#ifndef IDC_TASK_CORE_H
#define IDC_TASK_CORE_H
#include <stdbool.h>
#include <stdint.h>
#define IDC_MAX_STEPS 8
/* A bounded device executor; identity/signature verification belongs to transport. */
typedef enum { IDC_NEW, IDC_WAIT_APPROVAL, IDC_READY, IDC_RUNNING,
 IDC_UNKNOWN, IDC_DONE, IDC_FAILED, IDC_CANCELLED } idc_state;
typedef enum { IDC_DISPLAY, IDC_RECORD_START, IDC_RECORD_STOP, IDC_IR_SEND,
 IDC_NFC_EXCHANGE, IDC_PHONE_MEDIA, IDC_PHONE_CALL, IDC_AGENT_TASK, IDC_OP_COUNT } idc_op;
typedef enum { IDC_OK, IDC_INVALID, IDC_DENIED, IDC_BUSY, IDC_RECONCILE,
 IDC_TERMINAL, IDC_STALE } idc_rc;
typedef struct { uint32_t task_id; uint32_t generation; uint32_t step; } idc_token;
typedef struct {
 uint32_t schema_version, task_id, generation;
 uint64_t payload_fingerprint; /* Identity aid, not a cryptographic signature. */
 uint32_t required_caps, granted_caps, authorization_version;
 uint8_t count, cursor;
 idc_state state;
 bool approval_required, approved, cancel_requested;
 idc_op steps[IDC_MAX_STEPS];
 uint32_t receipt_id;
} idc_task;
idc_rc idc_create(idc_task *t, uint32_t id, uint64_t fingerprint,
 const idc_op *steps, uint8_t count, uint32_t granted_caps,
 uint32_t auth_version, bool explicit_approval);
idc_rc idc_approve(idc_task *t, uint32_t auth_version);
idc_rc idc_begin(idc_task *t, uint32_t live_caps, uint32_t auth_version,
 idc_token *token, idc_op *op);
idc_rc idc_result(idc_task *t, idc_token token, bool success, uint32_t receipt);
idc_rc idc_cancel(idc_task *t);
void idc_on_reboot(idc_task *t);
#endif
