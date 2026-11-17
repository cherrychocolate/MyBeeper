#include "audio_pcm.h"
#include <string.h>
#include "driver/gpio.h"
#define FAIL_ON(e) do{err=(e);if(err!=ESP_OK)goto fail;}while(0)
enum { AMP_EN=17, MCLK=16, BCLK=12, WS=13, TX_DATA=14, RX_DATA=15 };
void idc_audio_pcm_close(idc_audio_pcm *a){
 if(!a)return;
 a->ready=false;
 (void)gpio_set_level(AMP_EN,0);
 if(a->codec){
  (void)a->codec->mute(a->codec,true);
  (void)a->codec->enable(a->codec,false);
  audio_codec_delete_codec_if(a->codec);a->codec=NULL;
 }
 if(a->ctrl){audio_codec_delete_ctrl_if(a->ctrl);a->ctrl=NULL;}
 if(a->rx_enabled){(void)i2s_channel_disable(a->rx);a->rx_enabled=false;}
 if(a->tx_enabled){(void)i2s_channel_disable(a->tx);a->tx_enabled=false;}
 if(a->rx){(void)i2s_del_channel(a->rx);a->rx=NULL;}
 if(a->tx){(void)i2s_del_channel(a->tx);a->tx=NULL;}
 if(a->own_bus&&a->bus){(void)i2c_del_master_bus(a->bus);a->bus=NULL;a->own_bus=false;}
}
esp_err_t idc_audio_pcm_open(idc_audio_pcm *a,i2c_master_bus_handle_t borrowed){
 if(!a)return ESP_ERR_INVALID_ARG;
 /* Fresh or already-closed object required. Never open twice while active. */
 memset(a,0,sizeof(*a));esp_err_t err;
 FAIL_ON(gpio_set_level(AMP_EN,0));
 gpio_config_t amp={.pin_bit_mask=1ULL<<AMP_EN,.mode=GPIO_MODE_OUTPUT};
 FAIL_ON(gpio_config(&amp));
 a->bus=borrowed;
 if(!a->bus){
  i2c_master_bus_config_t bus={.i2c_port=I2C_NUM_0,.sda_io_num=7,.scl_io_num=6,
   .clk_source=I2C_CLK_SRC_DEFAULT,.glitch_ignore_cnt=7,
   .flags.enable_internal_pullup=false}; /* board external pullups required */
  FAIL_ON(i2c_new_master_bus(&bus,&a->bus));a->own_bus=true;
 }
 /* Fail early on absent codec; the driver has multiple register transactions. */
 FAIL_ON(i2c_master_probe(a->bus,ES8388_CODEC_DEFAULT_ADDR>>1,50));
 i2s_chan_config_t chan=I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0,I2S_ROLE_MASTER);
 chan.dma_desc_num=6;chan.dma_frame_num=160;chan.auto_clear=true;
 FAIL_ON(i2s_new_channel(&chan,&a->tx,&a->rx));
 i2s_std_config_t cfg={
  .clk_cfg=I2S_STD_CLK_DEFAULT_CONFIG(16000),
  .slot_cfg=I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT,I2S_SLOT_MODE_STEREO),
  .gpio_cfg={.mclk=MCLK,.bclk=BCLK,.ws=WS,.dout=TX_DATA,.din=RX_DATA}
 };
 cfg.clk_cfg.mclk_multiple=I2S_MCLK_MULTIPLE_256;
 FAIL_ON(i2s_channel_init_std_mode(a->tx,&cfg));
 FAIL_ON(i2s_channel_init_std_mode(a->rx,&cfg));
 int16_t silence[320]={0};size_t loaded=0;
 FAIL_ON(i2s_channel_preload_data(a->tx,silence,sizeof silence,&loaded));
 if(loaded!=sizeof silence){err=ESP_FAIL;goto fail;}
 FAIL_ON(i2s_channel_enable(a->tx));a->tx_enabled=true;
 FAIL_ON(i2s_channel_enable(a->rx));a->rx_enabled=true;
 /* This codec component accepts 8-bit address 0x20 and shifts to 7-bit 0x10.
    Do not shift ES8388_CODEC_DEFAULT_ADDR a second time. */
 audio_codec_i2c_cfg_t icfg={.port=0,.addr=ES8388_CODEC_DEFAULT_ADDR,.bus_handle=a->bus};
 a->ctrl=audio_codec_new_i2c_ctrl(&icfg);if(!a->ctrl){err=ESP_FAIL;goto fail;}
 es8388_codec_cfg_t ecfg={.ctrl_if=a->ctrl,.codec_mode=ESP_CODEC_DEV_WORK_MODE_BOTH,
                         .master_mode=false,.pa_pin=-1};
 a->codec=es8388_codec_new(&ecfg);if(!a->codec){err=ESP_FAIL;goto fail;}
 esp_codec_dev_sample_info_t fs={.bits_per_sample=16,.channel=2,
                               .sample_rate=16000,.mclk_multiple=256};
 FAIL_ON(a->codec->mute(a->codec,true));
 FAIL_ON(a->codec->set_fs(a->codec,&fs));
 FAIL_ON(a->codec->set_mic_gain(a->codec,0.0f)); /* no uncalibrated boost */
 FAIL_ON(a->codec->set_vol(a->codec,-40.0f));
 FAIL_ON(a->codec->enable(a->codec,true));
 FAIL_ON(a->codec->mute(a->codec,true));
 a->ready=true;return ESP_OK;
fail:
 idc_audio_pcm_close(a);return err;
}
static esp_err_t args(idc_audio_pcm *a,const void *p,size_t n,size_t *done,unsigned ms){
 if(done)*done=0;
 if(!a||!p||!done||!n||n>512||!ms||ms>1000)return ESP_ERR_INVALID_ARG;
 return a->ready?ESP_OK:ESP_ERR_INVALID_STATE;
}
esp_err_t idc_audio_pcm_read(idc_audio_pcm *a,int16_t *p,size_t n,size_t *done,unsigned ms){
 esp_err_t e=args(a,p,n,done,ms);if(e)return e;
 size_t got=0;e=i2s_channel_read(a->rx,p,n*4,&got,ms);*done=got/4;
 if(got%4)return ESP_FAIL;
 if(e)return e;
 return got==n*4?ESP_OK:ESP_ERR_TIMEOUT;
}
esp_err_t idc_audio_pcm_write(idc_audio_pcm *a,const int16_t *p,size_t n,size_t *done,unsigned ms){
 esp_err_t e=args(a,p,n,done,ms);if(e)return e;
 size_t got=0;e=i2s_channel_write(a->tx,p,n*4,&got,ms);*done=got/4;
 if(got%4)return ESP_FAIL;
 if(e)return e;
 return got==n*4?ESP_OK:ESP_ERR_TIMEOUT;
}
esp_err_t idc_audio_pcm_mute(idc_audio_pcm *a,bool mute){
 if(!a||!a->ready)return ESP_ERR_INVALID_STATE;
 return a->codec->mute(a->codec,mute);
}
