#include "task_nvs.h"
#include "esp_log.h"
void app_main(void){
 idc_nvs_service service={0};int rc=idc_nvs_open(&service,1);
 if(rc){ESP_LOGE("journal","restore refused: %d; no task dispatched",rc);return;}
 idc_task task;bool present=false;rc=idc_nvs_snapshot(&service,&task,&present);
 if(rc)ESP_LOGE("journal","snapshot failed: %d",rc);
 else if(present)ESP_LOGI("journal","restored task state=%d; no automatic replay",task.state);
 else ESP_LOGI("journal","empty slot; waiting for authorized task transport");
 idc_nvs_close(&service);
}
