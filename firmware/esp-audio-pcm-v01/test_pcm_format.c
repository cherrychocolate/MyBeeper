#include "pcm_format.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint32_t le32(const uint8_t *p){return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
int main(void){
 int16_t mono[]={INT16_MIN,-1,0,1,INT16_MAX},stereo[10]={0},left[5],right[5];
 assert(idc_pcm_duplicate_mono(mono,5,stereo,10));
 assert(idc_pcm_extract(stereo,5,0,left,5)&&idc_pcm_extract(stereo,5,1,right,5));
 assert(!memcmp(mono,left,sizeof mono)&&!memcmp(mono,right,sizeof mono));
 int16_t distinct[]={-30000,30000,-100,123,0,-32768,32767,1};
 assert(idc_pcm_extract(distinct,4,0,left,5)&&idc_pcm_extract(distinct,4,1,right,5));
 assert(left[0]==-30000&&right[0]==30000&&left[2]==0&&right[2]==INT16_MIN);
 puts("PASS signed extremes, mono duplication and distinct L/R extraction");
 int16_t unchanged[10];for(unsigned i=0;i<10;i++)unchanged[i]=1234;
 assert(!idc_pcm_duplicate_mono(mono,5,unchanged,9));
 assert(!idc_pcm_duplicate_mono(mono,SIZE_MAX,unchanged,10));
 assert(!idc_pcm_extract(distinct,4,2,unchanged,10));
 for(unsigned i=0;i<10;i++)assert(unchanged[i]==1234);
 puts("PASS capacity/overflow/invalid-channel rejection before output mutation");
 uint8_t wav[44];assert(idc_pcm_wav_header(16000,2,640,wav));
 assert(!memcmp(wav,"RIFF",4)&&!memcmp(wav+8,"WAVEfmt ",8)&&!memcmp(wav+36,"data",4));
 assert(le32(wav+4)==676&&le32(wav+24)==16000&&le32(wav+28)==64000&&le32(wav+40)==640);
 /* 8h is only file-size arithmetic; no duration/energy/capture claim. */
 assert(idc_pcm_wav_header(16000,2,1843200000u,wav));
 assert(le32(wav+4)==1843200036u);
 memset(wav,0xa5,sizeof wav);
 assert(!idc_pcm_wav_header(16000,2,UINT32_MAX,wav));
 assert(!idc_pcm_wav_header(UINT32_MAX,2,640,wav));
 assert(!idc_pcm_wav_header(16000,2,1,wav));
 for(unsigned i=0;i<44;i++)assert(wav[i]==0xa5);
 puts("PASS RIFF sizes/rate/block alignment, 8h size arithmetic, overflow and misalignment rejection");
 return 0;
}
