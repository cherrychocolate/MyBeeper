#ifndef IDC_POWER_POLICY_H
#define IDC_POWER_POLICY_H
#include <stdbool.h>
#include <stdint.h>
typedef enum { IDC_SUSPEND, IDC_USB100, IDC_USB500, IDC_ILIM } idc_power_mode;
typedef struct {
 bool vbus, usb_suspended, configured, cc_valid, normal_sink, fault;
 unsigned configured_ma, cc_detect, cc_age_ms;
} idc_power_input;
/* cc_detect is TUSB320 register 0x08 bits5:4: 0 default,1 medium,
 * 2 charged audio accessory (NOT 3A),3 high. No BC1.2/PD inference.
 * This pure policy does not read registers or drive physical pins. */
static inline idc_power_mode idc_power_select(idc_power_input x) {
 if (!x.vbus || x.usb_suspended || x.fault) return IDC_SUSPEND;
 if (x.cc_valid && x.cc_age_ms <= 20 && x.normal_sink &&
     (x.cc_detect == 1 || x.cc_detect == 3)) return IDC_ILIM;
 if (x.configured && x.configured_ma >= 500) return IDC_USB500;
 return IDC_USB100;
}
/* bit0 EN1,bit1 EN2; TI datasheet's table prints EN2 first. */
static inline unsigned idc_power_en(idc_power_mode m) {
 switch(m) { case IDC_USB100:return 0; case IDC_USB500:return 1;
 case IDC_ILIM:return 2; default:return 3; }
}
static inline unsigned idc_power_bound_ma(idc_power_mode m) {
 switch(m) { case IDC_USB100:return 100;case IDC_USB500:return 500;
 case IDC_ILIM:return 966;default:return 0; }
 /* suspend=0 means no functional input allocation, NOT zero leakage. */
}
#endif
