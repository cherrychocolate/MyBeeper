#include "e6_hal.h"
#include <string.h>
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

enum { CLK=18, MOSI=19, CS=39, DC=5, RST=0, BUSY=1, POWER=35 };
static const uint64_t outputs=(1ULL<<CS)|(1ULL<<DC)|(1ULL<<RST);
static void remember(idc_e6_hal *h, esp_err_t e) {
    if(h->fault==ESP_OK && e!=ESP_OK) h->fault=e;
}
static void pause_ms(void *ctx,unsigned ms) {
    (void)ctx;
    /* Round UP: reset pulse and vendor waits must never be shortened. */
    const TickType_t ticks=(ms+portTICK_PERIOD_MS-1)/portTICK_PERIOD_MS;
    vTaskDelay(ticks ? ticks : 1);
}
static void reset_line(void *ctx,int level) {
    idc_e6_hal *h=ctx;
    remember(h,gpio_set_level(RST,level));
}
static int busy_line(void *ctx) {
    idc_e6_hal *h=ctx;
    return h->fault==ESP_OK ? gpio_get_level(BUSY) : -1;
}
static int write_bytes(void *ctx,int data,const uint8_t *p,size_t n) {
    idc_e6_hal *h=ctx;
    if(h->fault!=ESP_OK || !h->device || !p || n==0 || n>1024) return -1;
    remember(h,gpio_set_level(DC,data ? 1 : 0));
    if(h->fault!=ESP_OK) return -1;
    /* Hardware CS surrounds each synchronous write. DC is task-owned.
       IDF may bounce-copy non-DMA/unaligned input; no async buffer lifetime. */
    spi_transaction_t t={ .length=n*8, .tx_buffer=p };
    remember(h,spi_device_transmit(h->device,&t));
    return h->fault==ESP_OK ? 0 : -1;
}
void idc_e6_close(idc_e6_hal *h) {
    if(!h) return;
    h->panel.ready=false;
    if(h->device) {
        remember(h,spi_bus_remove_device(h->device));
        h->device=NULL;
    }
    if(h->bus_open) {
        remember(h,spi_bus_free(SPI3_HOST));
        h->bus_open=false;
    }
    const gpio_config_t quiet={.pin_bit_mask=outputs|(1ULL<<CLK)|(1ULL<<MOSI),
        .mode=GPIO_MODE_INPUT,.pull_up_en=GPIO_PULLUP_DISABLE,
        .pull_down_en=GPIO_PULLDOWN_DISABLE,.intr_type=GPIO_INTR_DISABLE};
    remember(h,gpio_config(&quiet));
    remember(h,gpio_set_level(POWER,0));
    /* known is last completed controller sequence, not observed image. */
}
esp_err_t idc_e6_open(idc_e6_hal *h) {
    if(!h) return ESP_ERR_INVALID_ARG;
    memset(h,0,sizeof(*h));
    /* Preload LOW before enabling output; external EN pulldown still required. */
    remember(h,gpio_set_level(POWER,0));
    const gpio_config_t power={.pin_bit_mask=1ULL<<POWER,.mode=GPIO_MODE_OUTPUT};
    remember(h,gpio_config(&power));
    remember(h,gpio_set_level(CS,1));
    remember(h,gpio_set_level(DC,0));
    remember(h,gpio_set_level(RST,0));
    const gpio_config_t out={.pin_bit_mask=outputs,.mode=GPIO_MODE_OUTPUT};
    remember(h,gpio_config(&out));
    const gpio_config_t input={.pin_bit_mask=1ULL<<BUSY,.mode=GPIO_MODE_INPUT,
        .pull_up_en=GPIO_PULLUP_DISABLE,.pull_down_en=GPIO_PULLDOWN_DISABLE};
    remember(h,gpio_config(&input));
    if(h->fault!=ESP_OK) goto fail;
    const spi_bus_config_t bus={.mosi_io_num=MOSI,.miso_io_num=-1,
        .sclk_io_num=CLK,.quadwp_io_num=-1,.quadhd_io_num=-1,
        .data4_io_num=-1,.data5_io_num=-1,.data6_io_num=-1,.data7_io_num=-1,
        .max_transfer_sz=1024};
    remember(h,spi_bus_initialize(SPI3_HOST,&bus,SPI_DMA_CH_AUTO));
    if(h->fault!=ESP_OK) goto fail;
    h->bus_open=true;
    const spi_device_interface_config_t dev={.clock_speed_hz=1000000,
        .mode=0,.spics_io_num=CS,.queue_size=1};
    remember(h,spi_bus_add_device(SPI3_HOST,&dev,&h->device));
    if(h->fault!=ESP_OK) goto fail;
    remember(h,gpio_set_level(POWER,1));
    if(h->fault!=ESP_OK) goto fail;
    pause_ms(h,100); /* engineering settle allowance; validate on hardware */
    h->panel=(epd_e6){.io={h,write_bytes,busy_line,pause_ms,reset_line},
        .timeout_ms=45000};
    return ESP_OK;
fail:
    idc_e6_close(h);
    return h->fault;
}
