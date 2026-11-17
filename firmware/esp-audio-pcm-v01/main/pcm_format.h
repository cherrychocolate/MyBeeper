#ifndef IDC_PCM_FORMAT_H
#define IDC_PCM_FORMAT_H
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
/* Buffers must be disjoint. Signed samples; no codec decoding is performed. */
bool idc_pcm_duplicate_mono(const int16_t *,size_t frames,int16_t *,size_t out_samples);
bool idc_pcm_extract(const int16_t *,size_t frames,unsigned channel,int16_t *,size_t out_samples);
/* PCM16 RIFF/WAVE header only, not a filesystem writer or recovery service. */
bool idc_pcm_wav_header(uint32_t sample_rate,unsigned channels,uint32_t data_bytes,uint8_t out[44]);
#endif
