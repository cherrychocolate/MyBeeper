#include "pcm_format.h"
#include <string.h>
bool idc_pcm_duplicate_mono(const int16_t *p,size_t n,int16_t *out,size_t cap){
 if(!p||!out||!n||n>SIZE_MAX/2||cap<n*2)return false;
 for(size_t i=0;i<n;i++){out[i*2]=p[i];out[i*2+1]=p[i];}return true;
}
bool idc_pcm_extract(const int16_t *p,size_t n,unsigned channel,int16_t *out,size_t cap){
 if(!p||!out||!n||channel>1||cap<n||n>SIZE_MAX/2)return false;
 for(size_t i=0;i<n;i++){out[i]=p[i*2+channel];}
 return true;
}
static void put16(uint8_t *p,uint16_t v){p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);}
static void put32(uint8_t *p,uint32_t v){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(v>>(i*8));}
bool idc_pcm_wav_header(uint32_t rate,unsigned channels,uint32_t bytes,uint8_t out[44]){
 if(!out||!rate||channels<1||channels>2||bytes>UINT32_MAX-36||
    bytes%(channels*2)||rate>UINT32_MAX/(channels*2))return false;
 memset(out,0,44);memcpy(out,"RIFF",4);put32(out+4,bytes+36);
 memcpy(out+8,"WAVEfmt ",8);put32(out+16,16);put16(out+20,1);
 put16(out+22,(uint16_t)channels);put32(out+24,rate);put32(out+28,rate*channels*2);
 put16(out+32,(uint16_t)(channels*2));put16(out+34,16);memcpy(out+36,"data",4);
 put32(out+40,bytes);return true;
}
