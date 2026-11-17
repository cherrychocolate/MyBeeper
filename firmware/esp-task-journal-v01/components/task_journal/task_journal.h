#ifndef IDC_TASK_JOURNAL_H
#define IDC_TASK_JOURNAL_H
#include <stddef.h>
#include "task_core.h"
#define IDC_JOURNAL_BYTES 64u
#define IDC_JOURNAL_IO (-1)
#define IDC_JOURNAL_CORRUPT (-2)
typedef struct {
 void *ctx;
 /* read: 0 exact blob, 1 missing, negative failure. */
 int (*read)(void *,uint8_t *,size_t);
 /* save: 0 only after durable commit, nonzero ambiguous failure. */
 int (*save)(void *,const uint8_t *,size_t);
} idc_journal_io;
/* Single task slot, serialized by its owner. No global task-ID dedup claim. */
typedef struct { idc_task task; idc_journal_io io; bool present,faulted; } idc_store;
int idc_journal_encode(const idc_task *,uint8_t out[IDC_JOURNAL_BYTES]);
int idc_journal_decode(const uint8_t in[IDC_JOURNAL_BYTES],idc_task *);
int idc_store_open(idc_store *,idc_journal_io);
int idc_store_create(idc_store *,uint32_t,uint64_t,const idc_op *,uint8_t,
                     uint32_t,uint32_t,bool);
int idc_store_approve(idc_store *,uint32_t);
int idc_store_begin(idc_store *,uint32_t,uint32_t,idc_token *,idc_op *);
int idc_store_result(idc_store *,idc_token,bool,uint32_t);
int idc_store_cancel(idc_store *);
#endif
