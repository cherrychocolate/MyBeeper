#include "sdkconfig.h"
#include "audio_pcm.h"
#include "esp_log.h"
void app_main(void){
#if CONFIG_IDC_AUDIO_CAPTURE_PROBE
 idc_audio_pcm audio;esp_err_t e=idc_audio_pcm_open(&audio,NULL);
 if(e){ESP_LOGE("pcm","initialization failed: %s",esp_err_to_name(e));return;}
 int16_t samples[320];size_t frames=0;
 for(unsigned i=0;i<10;i++){
  e=idc_audio_pcm_read(&audio,samples,160,&frames,100);
  ESP_LOGI("pcm","capture result=%s frames=%u",esp_err_to_name(e),(unsigned)frames);
  if(e)break;
 }
 idc_audio_pcm_close(&audio);
 /* No sample contents, phone input, SD writes or speaker loopback here. */
#else
 ESP_LOGI("pcm","hardware probe disabled; no codec/I2S GPIO touched");
#endif
}
