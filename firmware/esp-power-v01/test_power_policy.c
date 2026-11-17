#include "power_policy.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
 unsigned n=0;
 const struct { idc_power_input in; idc_power_mode want; } cases[]={
  {{.vbus=true},IDC_USB100},
  {{.vbus=true,.configured=true,.configured_ma=100},IDC_USB100},
  {{.vbus=true,.configured=true,.configured_ma=500},IDC_USB500},
  {{.vbus=true,.normal_sink=true,.cc_valid=true,.cc_detect=1,.cc_age_ms=20},IDC_ILIM},
  {{.vbus=true,.normal_sink=true,.cc_valid=true,.cc_detect=3,.cc_age_ms=0},IDC_ILIM},
  {{.vbus=true,.normal_sink=true,.cc_valid=true,.cc_detect=2},IDC_USB100},
  {{.vbus=true,.normal_sink=true,.cc_valid=true,.cc_detect=3,.cc_age_ms=21},IDC_USB100},
  {{.vbus=true,.usb_suspended=true,.normal_sink=true,.cc_valid=true,.cc_detect=3},IDC_SUSPEND},
  {{.vbus=true,.fault=true,.configured=true,.configured_ma=500},IDC_SUSPEND},
  {{.vbus=false,.configured=true,.configured_ma=500},IDC_SUSPEND},
  {{.vbus=true,.normal_sink=false,.cc_valid=true,.cc_detect=3},IDC_USB100}
 };
 for(unsigned i=0;i<sizeof cases/sizeof cases[0];i++)
  assert(idc_power_select(cases[i].in)==cases[i].want);
 for(unsigned bits=0;bits<128;bits++)
 for(unsigned current=0;current<4;current++)
 for(unsigned age=0;age<3;age++) {
  const unsigned ages[]={0,20,21};
  idc_power_input x={.vbus=(bits&1)!=0,.usb_suspended=(bits&2)!=0,
   .configured=(bits&4)!=0,.cc_valid=(bits&8)!=0,.normal_sink=(bits&16)!=0,
   .fault=(bits&32)!=0,.configured_ma=(bits&64)?500:100,
   .cc_detect=current,.cc_age_ms=ages[age]};
  idc_power_mode m=idc_power_select(x);
  if(!x.vbus||x.usb_suspended||x.fault) assert(m==IDC_SUSPEND);
  else if(m==IDC_ILIM) assert(x.cc_valid&&x.normal_sink&&x.cc_age_ms<=20&&
                             (current==1||current==3));
  else if(m==IDC_USB500) assert(x.configured&&x.configured_ma>=500);
  else assert(m==IDC_USB100);
  assert(idc_power_en(m)<4);n++;
 }
 assert(idc_power_en(IDC_USB100)==0&&idc_power_en(IDC_USB500)==1);
 assert(idc_power_en(IDC_ILIM)==2&&idc_power_en(IDC_SUSPEND)==3);
 printf("11 named boundary cases + %u policy combinations passed; host only, no target/hardware validation\n",n);
 return 0;
}
