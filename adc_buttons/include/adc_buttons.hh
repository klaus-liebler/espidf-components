#pragma once
#include <stdio.h>
#include <cstring>
#include <array>
#include <algorithm>
#include <sdkconfig.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_adc/adc_continuous.h>
#include <esp_adc/adc_cali.h>
#include <esp_adc/adc_cali_scheme.h>
#include <esp_log.h>
#include <driver/gpio.h>
#define TAG "ADCBUT"

namespace AdcButtons
{
    constexpr auto SAMPLE_FREQUENCY{SOC_ADC_SAMPLE_FREQ_THRES_LOW};
    constexpr size_t NECESSARY_CONFIMATIONS{2};

    enum class State{
        IDLE,
        RIGHT,
        LEFT,
        BUTTON,
        UNDEFINED,
    };


    constexpr uint16_t MIN[]{  0, 180, 410,  760, UINT16_MAX-1};
    constexpr uint16_t MAX[]{100, 240, 510, 940, UINT16_MAX};

    
    adc_continuous_handle_t handle{nullptr};
    adc_cali_handle_t adc1_cali_handle{nullptr};
    State currentState{State::IDLE};
    State nextState{State::IDLE};
    size_t stateConfirmations{0};


    State detectCurrentState(int voltage_milliVolts)
    {
        ESP_LOGD(TAG, "Got %i, current %i, next %i",voltage_milliVolts, (int)currentState, (int)nextState);
        if (voltage_milliVolts >= MIN[(int)nextState] && voltage_milliVolts < MAX[(int)nextState])
        { 
            ESP_LOGD(TAG, "Confirm %i",(int)nextState);
            stateConfirmations++;
            if(stateConfirmations>NECESSARY_CONFIMATIONS){
                currentState=nextState;
            }
        } else{
            stateConfirmations=0;
            nextState=State::UNDEFINED;
            for(int i=0;i<4;i++){
                if(voltage_milliVolts >= MIN[i] && voltage_milliVolts < MAX[i]){
                    nextState=(State)i;
                    break;
                }
            }
            ESP_LOGD(TAG, "Found %i",(int)nextState);
        }
        return currentState;
    }

    State UpdateBlocking(){
        static uint8_t result[SOC_ADC_DIGI_RESULT_BYTES] = {0};
        uint32_t out_length;
        adc_continuous_read(handle, result, SOC_ADC_DIGI_RESULT_BYTES, &out_length, 0);
        adc_digi_output_data_t *p = (adc_digi_output_data_t *)result;
        uint16_t rawVal = p->type2.data;
        int calibratedVoltage_mV{0};
        adc_cali_raw_to_voltage(adc1_cali_handle, rawVal, &calibratedVoltage_mV);
        //printf(">volt:%i\n", calibratedVoltage_mV);   
        return detectCurrentState(calibratedVoltage_mV);
    }


    void InitAdcButtons(gpio_num_t gpio)
    {
        adc_channel_t adc_channel{ADC_CHANNEL_0};
        adc_unit_t adc_unit = ADC_UNIT_1;

        ESP_ERROR_CHECK(adc_continuous_io_to_channel(gpio, &adc_unit, &adc_channel));

        adc_continuous_handle_cfg_t adc_config = {};
        adc_config.max_store_buf_size = SOC_ADC_DIGI_RESULT_BYTES;
        adc_config.conv_frame_size = SOC_ADC_DIGI_RESULT_BYTES;
        ESP_ERROR_CHECK(adc_continuous_new_handle(&adc_config, &handle));

        
        adc_cali_curve_fitting_config_t cali_config = {};
        cali_config.unit_id = ADC_UNIT_1;
        cali_config.atten = ADC_ATTEN_DB_11;
        cali_config.bitwidth = ADC_BITWIDTH_12;
        ESP_ERROR_CHECK(adc_cali_create_scheme_curve_fitting(&cali_config, &adc1_cali_handle));

        adc_continuous_config_t dig_cfg = {};
        dig_cfg.sample_freq_hz = SAMPLE_FREQUENCY;
        dig_cfg.conv_mode = ADC_CONV_SINGLE_UNIT_1;
        dig_cfg.format = ADC_DIGI_OUTPUT_FORMAT_TYPE2;
        adc_digi_pattern_config_t adc_pattern[SOC_ADC_PATT_LEN_MAX];
        adc_pattern[0].atten = ADC_ATTEN_DB_11;
        adc_pattern[0].channel = adc_channel;
        adc_pattern[0].unit = ADC_UNIT_1;
        adc_pattern[0].bit_width = ADC_BITWIDTH_12;
        dig_cfg.pattern_num = 1;
        dig_cfg.adc_pattern = adc_pattern;
        ESP_ERROR_CHECK(adc_continuous_config(handle, &dig_cfg));

        ESP_ERROR_CHECK(adc_continuous_start(handle));

        ESP_LOGI(TAG, "Encoder successfully initialized");
    }
}
#undef TAG