#ifndef IDC_EPD_E6_H
#define IDC_EPD_E6_H
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#define E6_WIDTH 400u
#define E6_HEIGHT 600u
#define E6_FRAME_BYTES 120000u
#define E6_IO (-1)
#define E6_TIMEOUT (-2)
#define E6_ARG (-3)
#define E6_STATE (-4)
typedef struct {
 void *ctx;
 int (*write)(void *,int data,const uint8_t *,size_t);
 int (*busy)(void *); /* 0 busy,1 idle,negative IO error */
 void (*delay)(void *,unsigned ms); /* must yield task, not block scheduler */
 void (*reset)(void *,int level);
} epd_e6_io;
typedef struct {epd_e6_io io;unsigned timeout_ms;bool ready,known;} epd_e6;
/* Serialized worker only. timeout is engineering policy, not vendor refresh spec.
 * known means controller sequence completed, NOT optical verification. */
int e6_init(epd_e6 *);
int e6_display(epd_e6 *,const uint8_t *,size_t);
int e6_sleep(epd_e6 *);
#endif
