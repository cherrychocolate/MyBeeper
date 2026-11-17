#ifndef IDC_TASK_NVS_H
#define IDC_TASK_NVS_H
#include "task_journal.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
typedef struct {
 idc_store store;
 nvs_handle_t handle;
 SemaphoreHandle_t mutex;
 char key[10];
 uint32_t task_id;
 bool opened,owns_gate;
} idc_nvs_service;
/* Zero-initialize before first open. One loaded service globally at a time;
   close/reopen switches task keys while retaining prior terminal records. */
int idc_nvs_open(idc_nvs_service *,uint32_t task_id);
/* Caller must stop all service users before close; never call from an ISR. */
void idc_nvs_close(idc_nvs_service *);
int idc_nvs_create(idc_nvs_service *,uint32_t,uint64_t,const idc_op *,uint8_t,uint32_t,uint32_t,bool);
int idc_nvs_approve(idc_nvs_service *,uint32_t);
int idc_nvs_begin(idc_nvs_service *,uint32_t,uint32_t,idc_token *,idc_op *);
int idc_nvs_result(idc_nvs_service *,idc_token,bool,uint32_t);
int idc_nvs_cancel(idc_nvs_service *);
int idc_nvs_snapshot(idc_nvs_service *,idc_task *,bool *);
#endif
