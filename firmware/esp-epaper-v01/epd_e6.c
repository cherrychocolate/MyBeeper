/*****************************************************************************
* | File        :   EPD_3in6e.c
* | Author      :   Waveshare team
* | Function    :   3.6inch e-Paper (E) Driver
* | Info        :
*----------------
* | This version:   V1.0
* | Date        :   2025-12-04
* | Info        :
* -----------------------------------------------------------------------------
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documnetation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to  whom the Software is
# furished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS OR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.
#
******************************************************************************/

/* AI ID Card portable adaptation. No vendor sketch is executed. */
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
    TRY(tx(d,0,0xAA));
    TRY(tx(d,1,0x49));
    TRY(tx(d,1,0x55));
    TRY(tx(d,1,0x20));
    TRY(tx(d,1,0x08));
    TRY(tx(d,1,0x09));
    TRY(tx(d,1,0x18));
    TRY(tx(d,0,0x01));
    TRY(tx(d,1,0x3F));
    TRY(tx(d,0,0x00));
    TRY(tx(d,1,0x5F));
    TRY(tx(d,1,0x69));
    TRY(tx(d,0,0x05));
    TRY(tx(d,1,0x40));
    TRY(tx(d,1,0x1F));
    TRY(tx(d,1,0x1F));
    TRY(tx(d,1,0x2C));
    TRY(tx(d,0,0x08));
    TRY(tx(d,1,0x6F));
    TRY(tx(d,1,0x1F));
    TRY(tx(d,1,0x1F));
    TRY(tx(d,1,0x22));
    TRY(tx(d,0,0x06));
    TRY(tx(d,1,0x6F));
    TRY(tx(d,1,0x1F));
    TRY(tx(d,1,0x17));
    TRY(tx(d,1,0x17));
    TRY(tx(d,0,0x03));
    TRY(tx(d,1,0x00));
    TRY(tx(d,1,0x54));
    TRY(tx(d,1,0x00));
    TRY(tx(d,1,0x44));
    TRY(tx(d,0,0x60));
    TRY(tx(d,1,0x02));
    TRY(tx(d,1,0x00));
    TRY(tx(d,0,0x30));
    TRY(tx(d,1,0x08));
    TRY(tx(d,0,0x50));
    TRY(tx(d,1,0x3F));
    TRY(tx(d,0,0x61));
    TRY(tx(d,1,0x01));
    TRY(tx(d,1,0x90));
    TRY(tx(d,1,0x02));
    TRY(tx(d,1,0x58));
    TRY(tx(d,0,0xE3));
    TRY(tx(d,1,0x2F));
    TRY(tx(d,0,0x84));
    TRY(tx(d,1,0x01));
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
