#pragma once

#include <stdio.h>
#include <esp_err.h>
#include <esp_log.h>
#include <driver/gpio.h>
#include <driver/spi_master.h>
#include <nrf24.hh>
#include "sdkconfig.h"

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>

namespace milight
{
    class iMilightCallback
    {
    public:
        virtual void ReceivedFromMilight(uint8_t cmd, uint8_t arg) = 0;
    };

    class Milight
    {
    private:
        iMilightCallback *callback;
        void task();
        Nrf24Receiver *recv;

    public:
        Milight(iMilightCallback *callback);
        esp_err_t SetupAndRun(spi_host_device_t hostDevice, int dmaChannel, gpio_num_t ce_pin, gpio_num_t csn_pin, gpio_num_t miso_pin, gpio_num_t mosi_pin, gpio_num_t sclk_pin);
    };
}