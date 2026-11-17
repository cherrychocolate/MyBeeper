#include <stdlib.h>
#include <string.h>
#include "sdkconfig.h"
#include "esp_log.h"
#include "e6_hal.h"

void app_main(void) {
#if CONFIG_IDC_E6_RUN_ONCE
    /* One serialized worker (app_main). No display UI or periodic refresh. */
    idc_e6_hal h;
    uint8_t *frame=malloc(E6_FRAME_BYTES);
    if(!frame) { ESP_LOGE("e6","No frame buffer; no IO started"); return; }
    memset(frame,0x11,E6_FRAME_BYTES); /* native six-color palette: white */
    esp_err_t io=idc_e6_open(&h);
    int result=E6_IO;
    if(io==ESP_OK) {
        result=e6_init(&h.panel);
        if(!result) result=e6_display(&h.panel,frame,E6_FRAME_BYTES);
        if(!result) result=e6_sleep(&h.panel);
        ESP_LOGI("e6","sequence=%d known=%d (not optical verification)",
                 result,h.panel.known);
        idc_e6_close(&h);
    }
    ESP_LOGI("e6","IO=%s core=%d sticky=%s",esp_err_to_name(io),result,
             esp_err_to_name(h.fault));
    free(frame);
#else
    ESP_LOGI("e6","Physical bring-up disabled; no display GPIO/rail touched");
#endif
}
