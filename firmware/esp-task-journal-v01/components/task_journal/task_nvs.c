#include "task_nvs.h"
#include <string.h>
#include <stdio.h>
#include <inttypes.h>
#include "nvs_flash.h"
static portMUX_TYPE gate_lock=portMUX_INITIALIZER_UNLOCKED;
static bool gate_owned=false;
static int read_blob(void *ctx,uint8_t *p,size_t cap){
 idc_nvs_service *s=ctx;size_t n=cap;
 esp_err_t e=nvs_get_blob(s->handle,s->key,p,&n);
 if(e==ESP_ERR_NVS_NOT_FOUND)return 1;
 return e==ESP_OK&&n==cap?0:-1;
}
static int save_blob(void *ctx,const uint8_t *p,size_t n){
 idc_nvs_service *s=ctx;
 esp_err_t e=nvs_set_blob(s->handle,s->key,p,n);
 if(e!=ESP_OK)return -1;
 return nvs_commit(s->handle)==ESP_OK?0:-1;
}
void idc_nvs_close(idc_nvs_service *s){
 if(!s)return;
 if(s->opened)nvs_close(s->handle);
 s->opened=false;
 if(s->mutex){vSemaphoreDelete(s->mutex);s->mutex=NULL;}
 if(s->owns_gate){
  portENTER_CRITICAL(&gate_lock);gate_owned=false;portEXIT_CRITICAL(&gate_lock);
  s->owns_gate=false;
 }
}
int idc_nvs_open(idc_nvs_service *s,uint32_t id){
 if(!s||!id||s->owns_gate)return IDC_INVALID;
 memset(s,0,sizeof(*s));
 portENTER_CRITICAL(&gate_lock);
 bool busy=gate_owned;if(!busy)gate_owned=true;
 portEXIT_CRITICAL(&gate_lock);
 if(busy)return IDC_BUSY;
 s->owns_gate=true;s->task_id=id;
 (void)snprintf(s->key,sizeof s->key,"t%08" PRIx32,id);
 /* Fail closed. Never erase persistent tasks as an automatic repair. */
 if(nvs_flash_init_partition("tasklog")!=ESP_OK){idc_nvs_close(s);return IDC_JOURNAL_IO;}
 s->mutex=xSemaphoreCreateMutex();if(!s->mutex){idc_nvs_close(s);return IDC_JOURNAL_IO;}
 if(nvs_open_from_partition("tasklog","idc_v1",NVS_READWRITE,&s->handle)!=ESP_OK){
  idc_nvs_close(s);return IDC_JOURNAL_IO;
 }
 s->opened=true;
 int rc=idc_store_open(&s->store,(idc_journal_io){s,read_blob,save_blob});
 if(!rc&&s->store.present&&s->store.task.task_id!=id)rc=IDC_JOURNAL_CORRUPT;
 if(rc)idc_nvs_close(s);
 return rc;
}
static int lock(idc_nvs_service *s){
 if(!s||!s->opened||!s->mutex)return IDC_JOURNAL_IO;
 return xSemaphoreTake(s->mutex,portMAX_DELAY)==pdTRUE?0:IDC_JOURNAL_IO;
}
int idc_nvs_create(idc_nvs_service *s,uint32_t id,uint64_t fp,const idc_op *ops,uint8_t n,
                   uint32_t caps,uint32_t av,bool approval){
 int rc=lock(s);if(rc)return rc;
 rc=id==s->task_id?idc_store_create(&s->store,id,fp,ops,n,caps,av,approval):IDC_INVALID;
 xSemaphoreGive(s->mutex);return rc;
}
int idc_nvs_approve(idc_nvs_service *s,uint32_t av){
 int rc=lock(s);if(rc)return rc;rc=idc_store_approve(&s->store,av);
 xSemaphoreGive(s->mutex);return rc;
}
int idc_nvs_begin(idc_nvs_service *s,uint32_t caps,uint32_t av,idc_token *k,idc_op *op){
 int rc=lock(s);if(rc)return rc;rc=idc_store_begin(&s->store,caps,av,k,op);
 xSemaphoreGive(s->mutex);return rc;
}
int idc_nvs_result(idc_nvs_service *s,idc_token k,bool ok,uint32_t receipt){
 int rc=lock(s);if(rc)return rc;rc=idc_store_result(&s->store,k,ok,receipt);
 xSemaphoreGive(s->mutex);return rc;
}
int idc_nvs_cancel(idc_nvs_service *s){
 int rc=lock(s);if(rc)return rc;rc=idc_store_cancel(&s->store);
 xSemaphoreGive(s->mutex);return rc;
}
int idc_nvs_snapshot(idc_nvs_service *s,idc_task *t,bool *present){
 if(!t||!present)return IDC_INVALID;
 int rc=lock(s);if(rc)return rc;
 if(s->store.faulted)rc=IDC_JOURNAL_IO;
 else{*present=s->store.present;if(*present)*t=s->store.task;}
 xSemaphoreGive(s->mutex);return rc;
}
