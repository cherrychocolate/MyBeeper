from pathlib import Path
import re
p=Path(__file__).parent
s=(p/'vendor/EPD_3in6e.cpp').read_text()
lic=s[:s.index('#include')]
# Preserve the exact manufacturer initialization register/data ordering.
a=s[s.index('    //20211212'):s.index('/******************************************************************************\nfunction :  Clear screen')]
a=a[:a.rfind('}')]
pairs=re.findall(r'EPD_3IN6E_Send(Command|Data)\((0[xX][0-9A-Fa-f]+)\)',a)
init='\n'.join('    TRY(tx(d,%d,%s));'%(k=='Data',v) for k,v in pairs)
(p/'epd_e6.c').write_text(lic+'''\n/* AI ID Card portable adaptation. No vendor sketch is executed. */
#include "epd_e6.h"
#define TRY(x) do { int err=(x); if(err) { d->ready=false;d->known=false;return err; } } while(0)
static int tx(epd_e6 *d,int data,uint8_t b){return d->io.write(d->io.ctx,data,&b,1)?E6_IO:0;}
static int wait_idle(epd_e6 *d){
 unsigned elapsed=0;
 for(;;){int b=d->io.busy(d->io.ctx);if(b<0)return E6_IO;if(b){d->io.delay(d->io.ctx,100);return 0;}
 if(elapsed>=d->timeout_ms)return E6_TIMEOUT;
 d->io.delay(d->io.ctx,10);elapsed+=10;}
}
int e6_init(epd_e6 *d){
 if(!d||!d->io.write||!d->io.busy||!d->io.delay||!d->io.reset||d->timeout_ms<10||d->timeout_ms>120000)return E6_ARG;
 d->ready=false; d->known=false;
 d->io.reset(d->io.ctx,1);d->io.delay(d->io.ctx,200);
 d->io.reset(d->io.ctx,0);d->io.delay(d->io.ctx,20);
 d->io.reset(d->io.ctx,1);d->io.delay(d->io.ctx,200);
 TRY(wait_idle(d));d->io.delay(d->io.ctx,30);
'''+init+'''
 TRY(wait_idle(d));d->ready=true;return 0;
}
int e6_display(epd_e6 *d,const uint8_t *frame,size_t len){
 if(!d||!frame||len!=E6_FRAME_BYTES)return E6_ARG;
 if(!d->ready)return E6_STATE;
 /* Reject unsupported palette values BEFORE the display transaction starts. */
 for(size_t n=0;n<len;n++){unsigned a=frame[n]>>4,b=frame[n]&15;
 if(a==4||a>6||b==4||b>6)return E6_ARG;}
 d->known=false;
 TRY(tx(d,0,0x10));
 for(size_t i=0;i<len;i+=1024){size_t n=len-i;if(n>1024)n=1024;
 if(d->io.write(d->io.ctx,1,frame+i,n)){d->ready=false;return E6_IO;}}
 TRY(tx(d,0,0x04));TRY(wait_idle(d));d->io.delay(d->io.ctx,200);
 TRY(tx(d,0,0x06));TRY(tx(d,1,0x6f));TRY(tx(d,1,0x1f));TRY(tx(d,1,0x16));TRY(tx(d,1,0x29));
 d->io.delay(d->io.ctx,200);
 TRY(tx(d,0,0x12));TRY(tx(d,1,0));TRY(wait_idle(d));
 TRY(tx(d,0,0x02));TRY(tx(d,1,0));TRY(wait_idle(d));
 d->known=true;return 0;
}
int e6_sleep(epd_e6 *d){
 if(!d||!d->ready)return E6_STATE;
 TRY(tx(d,0,0x07));TRY(tx(d,1,0xa5));d->ready=false;
 /* No acknowledgement for deep sleep: image knowledge stays as last confirmed. */
 return 0;
}
''')
print('Vendor init commands copied in exact order:',len(pairs))
