#pragma once

#include <stdio.h>
#include <esp_log.h>
#include <sdkconfig.h>
#include <esp_err.h>
#include <driver/pulse_cnt.h>
#include <driver/gpio.h>

class cRotaryEncoder{
public:
	pcnt_unit_handle_t pcnt_unit;
    gpio_num_t phase_a_gpio_num;
    gpio_num_t phase_b_gpio_num;
    gpio_num_t sw_gpio_num;
	int16_t minCount;
	int16_t maxCount;
	
	esp_err_t Init();

    esp_err_t Start();

    esp_err_t Stop();

    esp_err_t GetValue(int16_t &value, bool &isPressed, bool resetValueToZero=true);

	cRotaryEncoder(gpio_num_t phase_a_gpio_num, gpio_num_t phase_b_gpio_num, gpio_num_t sw_gpio_num, int16_t minCount=INT16_MIN, int16_t maxCount=INT16_MAX);
};





