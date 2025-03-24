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
    Nrf24Receiver *recv;
        iMilightCallback *callback;
        
        void task();
    public:
        Milight(Nrf24Receiver *recv, iMilightCallback *callback);
        esp_err_t SetupAndRun();
    };
}