#pragma once

#include <stdio.h>
#include <esp_log.h>
#include <sdkconfig.h>
#include <esp_err.h>
#include <driver/pulse_cnt.h>
#include <driver/gpio.h>

class cRotaryEncoder{
public:
    enum class StepMode : uint8_t {
        Step1 = 1,
        Step2 = 2,
        Step4 = 4,
    };

	pcnt_unit_handle_t pcnt_unit;
    gpio_num_t phase_a_gpio_num;
    gpio_num_t phase_b_gpio_num;
    gpio_num_t sw_gpio_num;
	int32_t lowLimit;
	int32_t highLimit;
	
    esp_err_t Init(StepMode stepMode=StepMode::Step1);

    esp_err_t Start();

    esp_err_t Stop();

    esp_err_t GetValue(int16_t &value, bool &isPressed, bool resetValueToZero=true);

	cRotaryEncoder(gpio_num_t phase_a_gpio_num, gpio_num_t phase_b_gpio_num, gpio_num_t sw_gpio_num, int16_t lowLimit=INT16_MIN, int16_t highLimit=INT16_MAX);
};





