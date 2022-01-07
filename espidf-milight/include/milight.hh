#pragma once

#include <stdio.h>
#include <esp_err.h>
#include <esp_log.h>
#include <driver/gpio.h>
#include <driver/spi_master.h>
#include <nrf24.hh>
#include "sdkconfig.h"

class MilightCallback{
    public:
    virtual void ReceivedFromMilight(uint8_t cmd, uint8_t arg)=0;
};

class Milight
{

public:
    MilightCallback* callback;
    Nrf24Receiver *recv;
    Milight(MilightCallback* callback);
    esp_err_t Init(spi_host_device_t hostDevice, int dmaChannel, gpio_num_t ce_pin, gpio_num_t csn_pin, gpio_num_t miso_pin, gpio_num_t mosi_pin, gpio_num_t sclk_pin);
    esp_err_t Start();
};