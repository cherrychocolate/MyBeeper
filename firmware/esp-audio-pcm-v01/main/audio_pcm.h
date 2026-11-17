#ifndef IDC_AUDIO_PCM_H
#define IDC_AUDIO_PCM_H
#include <stddef.h>
#include <stdint.h>
#include "driver/i2s_std.h"
#include "driver/i2c_master.h"
#include "esp_codec_dev_defaults.h"
/* One owning audio worker; no concurrent close/reconfiguration or ISR calls. */
typedef struct {
 i2c_master_bus_handle_t bus;
 i2s_chan_handle_t tx,rx;
 const audio_codec_ctrl_if_t *ctrl;
 const audio_codec_if_t *codec;
 bool own_bus,tx_enabled,rx_enabled,ready;
} idc_audio_pcm;
/* Optional borrowed I2C0 bus: owner keeps it alive through close.
   Fixed 16kHz, signed 16-bit, two interleaved channels, Philips I2S. */
esp_err_t idc_audio_pcm_open(idc_audio_pcm *,i2c_master_bus_handle_t borrowed);
void idc_audio_pcm_close(idc_audio_pcm *);
/* Up to 512 frames per call; timeout_ms 1..1000. Reports partial frame counts
   even on timeout. Never silently treats a short transfer as complete. */
esp_err_t idc_audio_pcm_read(idc_audio_pcm *,int16_t *,size_t frames,size_t *done,unsigned timeout_ms);
esp_err_t idc_audio_pcm_write(idc_audio_pcm *,const int16_t *,size_t frames,size_t *done,unsigned timeout_ms);
/* DAC remains muted until caller explicitly requests unmute. NS4150 GPIO17
   remains LOW; actual amplifier/headphone routing is a separate service. */
esp_err_t idc_audio_pcm_mute(idc_audio_pcm *,bool);
#endif
