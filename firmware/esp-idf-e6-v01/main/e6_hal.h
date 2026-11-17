#ifndef IDC_E6_HAL_H
#define IDC_E6_HAL_H
#include "epd_e6.h"
#include "esp_err.h"
#include "driver/spi_master.h"
/* Dedicated SPI3 and one owning task. Do not copy this object while active. */
typedef struct {
    spi_device_handle_t device;
    bool bus_open;
    esp_err_t fault;
    epd_e6 panel;
} idc_e6_hal;
esp_err_t idc_e6_open(idc_e6_hal *h);
/* Returns GPIOs to high-Z before rail-off; not proof of zero backfeed. */
void idc_e6_close(idc_e6_hal *h);
#endif
