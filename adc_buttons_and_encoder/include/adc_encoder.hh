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
#define TAG "ENCA"

namespace EncoderADC
{
    constexpr size_t READ_LEN{128};
    constexpr auto SAMPLE_FREQUENCY{1000};
    constexpr size_t NECESSARY_CONFIMATIONS{2};

    namespace EncoderState
    {
        constexpr uint16_t REST{0};
        constexpr uint16_t REST_IN{100};

        constexpr uint16_t RIGHT_IN_L{200};
        constexpr uint16_t RIGHT{290};
        constexpr uint16_t RIGHT_IN_H{350};

        constexpr uint16_t LEFT_IN_L{500};
        constexpr uint16_t LEFT{564};
        constexpr uint16_t LEFT_IN_H{600};

        constexpr uint16_t HALFWAY_IN_L{700};
        constexpr uint16_t HALFWAY{765};
        constexpr uint16_t HALFWAY_IN_H{850};

        constexpr uint16_t SWITCH_IN{930};
        constexpr uint16_t SWITCH{1015};

        constexpr uint16_t TRANSITION{UINT16_MAX - 1};
    };

    int32_t steps{0};
    uint16_t previousState{EncoderState::REST};
    
    adc_continuous_handle_t handle{nullptr};
    adc_cali_handle_t adc1_cali_handle{nullptr};

    // Erkenne Zustände, wenn N Messungen nacheinander den Zustand bestätigen


    uint16_t assumedNewState{EncoderState::REST};
    uint16_t assumedMin{0};
    uint16_t assumedMax{EncoderState::RIGHT_IN_L};
    size_t assumedNewStateConfirmations{0};
    int counter{0};
    bool button{false};

    uint16_t detectCurrentState(uint16_t val)
    {
        if (val < EncoderState::REST_IN)
        {
            assumedMin = EncoderState::REST;
            assumedMax = EncoderState::RIGHT_IN_L;
            assumedNewState = EncoderState::REST;
            assumedNewStateConfirmations = NECESSARY_CONFIMATIONS;
            return EncoderState::REST;
        }
        if (val > EncoderState::SWITCH_IN)
        {
            assumedMin = EncoderState::HALFWAY_IN_H;
            assumedMax = UINT16_MAX;
            assumedNewState = EncoderState::SWITCH;
            assumedNewStateConfirmations = NECESSARY_CONFIMATIONS;
            return EncoderState::SWITCH;
        }
        if (val >= assumedMin && val <= assumedMax)
        {
            if (assumedNewStateConfirmations < NECESSARY_CONFIMATIONS)
            {
                assumedNewStateConfirmations++;
                return EncoderState::TRANSITION;
            }
            else
            {
                return assumedNewState;
            }
        }
        if (val > EncoderState::RIGHT_IN_L && val < EncoderState::RIGHT_IN_H)
        {
            assumedNewState = EncoderState::RIGHT;
            assumedMin = EncoderState::RIGHT_IN_L;
            assumedMax = EncoderState::RIGHT_IN_H;
        }
        else if (val > EncoderState::LEFT_IN_L && val < EncoderState::LEFT_IN_H)
        {
            assumedNewState = EncoderState::LEFT;
            assumedMin = EncoderState::LEFT_IN_L;
            assumedMax = EncoderState::LEFT_IN_H;
        }
        else if (val > EncoderState::HALFWAY_IN_L && val < EncoderState::HALFWAY_IN_H)
        {
            assumedNewState = EncoderState::HALFWAY;
            assumedMin = EncoderState::HALFWAY_IN_L;
            assumedMax = EncoderState::HALFWAY_IN_H;
        }
        assumedNewStateConfirmations = 1;
        return EncoderState::TRANSITION;
    }

    int Update(adc_digi_output_data_t *p)
    {
        if (p->type2.channel >= SOC_ADC_CHANNEL_NUM(ADC_UNIT_1))
        {
            return 0;
        }
        uint16_t rawVal = p->type2.data;
        int calibratedVoltage_mV{0};
        adc_cali_raw_to_voltage(adc1_cali_handle, rawVal, &calibratedVoltage_mV);   
        uint16_t newState = detectCurrentState(calibratedVoltage_mV);
        int change = 0;
        if (newState == EncoderState::TRANSITION || newState == previousState)
            return 0;
        if (newState == EncoderState::SWITCH)
        {
            steps = 0; // das Einschalten des Switches wird hier direkt registriert und alles andere abgebrochen (ansonsten müsste das in jedem case berücksichtigt werden)
            button = true;
            previousState = EncoderState::SWITCH;
            return 0;
        }
        switch (previousState)
        {
        case EncoderState::SWITCH:
            button = false; // das Ausschalten des Switches wird hier registriert
            break;
        case EncoderState::REST:
            if (newState == EncoderState::LEFT)
            {
                steps = -1;
            }
            else if (newState == EncoderState::RIGHT)
            {
                steps = +1;
            }
            break;
        case EncoderState::LEFT:
            if (newState == EncoderState::REST)
            {
                if (steps > 1)
                {
                    ESP_LOGD(TAG, "Rotated RIGHT with steps=%li", steps);
                    change = 1;
                }
                steps = 0;
            }
            else if (newState == EncoderState::HALFWAY)
            {
                if (steps == -1)
                {
                    steps = -2;
                }
                else
                {
                    steps = 0;
                }
            }
            break;
        case EncoderState::RIGHT:
            if (newState == EncoderState::REST)
            {
                if (steps < -1)
                {
                    ESP_LOGD(TAG, "Rotated LEFT with steps=%li", steps);
                    change = -1;
                }
                steps = 0;
            }
            else if (newState == EncoderState::HALFWAY)
            {
                if (steps == +1)
                {
                    steps = +2;
                }
                else
                {
                    steps = 0;
                }
            }
            break;
        case EncoderState::HALFWAY:
            // Wenn er links rum angedreht wurde und er jetzt durch rechts durchgeht, dann ist das ein schritt weiter nach links
            // Wenn er rechts rum angedreht wurde und er jetzt durch links durchgeht, dann ist das ein schritt weiter nach rechts

            if (newState == EncoderState::RIGHT)
            {
                if (steps == -2)
                {
                    steps = -3;
                }
                else
                {
                    steps = 0;
                }
            }
            else if (newState == EncoderState::LEFT)
            {
                if (steps == +2)
                {
                    steps = +3;
                }
                else
                {
                    steps = 0;
                }
            }
            else if (newState == EncoderState::REST)
            {
                if (steps > 1)
                {
                    ESP_LOGD(TAG, "Rotated RIGHT with steps=%li", steps);
                    change = 1;
                }
                else
                {
                    ESP_LOGD(TAG, "Rotated LEFT with steps=%li", steps);
                    change = -1;
                }
                steps = 0;
            }
            break;
        }
        ESP_LOGD(TAG, "Changed state from %hu to %hu, steps= %li", previousState, newState, steps);
        previousState = newState;
        return change;
    }

    bool adc_continuous_callback(adc_continuous_handle_t handle, const adc_continuous_evt_data_t *edata, void *user_data)
    {

        for(int i=0;i<READ_LEN;i++){
            adc_digi_output_data_t *p = (adc_digi_output_data_t *)&edata->conv_frame_buffer[i*SOC_ADC_DIGI_RESULT_BYTES];
            int change = Update(p);
            counter+=change;
        }
        
        return false;
    }

    uint8_t result[SOC_ADC_DIGI_RESULT_BYTES * READ_LEN] = {0};
    void UpdateBlocking(){
        uint32_t out_length;
        ESP_ERROR_CHECK(adc_continuous_read(handle, result, SOC_ADC_DIGI_RESULT_BYTES * READ_LEN, &out_length, portMAX_DELAY));
        for (int i = 0; i < READ_LEN; i++)
        {
            adc_digi_output_data_t *p = (adc_digi_output_data_t *)&result[i*SOC_ADC_DIGI_RESULT_BYTES];
            int change = Update(p);
            counter+=change;
            if(change){
                 ESP_LOGD(TAG, "Counter %i", counter);
            }
        }
    }

    void DoTeleplot(){
        uint32_t out_length;
        ESP_ERROR_CHECK(adc_continuous_read(handle, result, SOC_ADC_DIGI_RESULT_BYTES * READ_LEN, &out_length, portMAX_DELAY));
        for (int i = 0; i < READ_LEN; i++)
        {
            adc_digi_output_data_t *p = (adc_digi_output_data_t *)&result[i*SOC_ADC_DIGI_RESULT_BYTES];
            printf(">adc2:%i\n", p->type2.data);
        }
    }

    int previousCounter{0};

    int GetChangesSinceLastCallOfThisFunction()
    {
        int tmp = counter;
        int ret = tmp - previousCounter;
        previousCounter = tmp;
        return ret;
    }

    bool GetButtonPressed()
    {
        return button;
    }

    void InitEncoder(gpio_num_t gpio, bool runInInterruptMode)
    {
        adc_channel_t adc_channel{ADC_CHANNEL_0};
        adc_unit_t adc_unit = ADC_UNIT_1;

        ESP_ERROR_CHECK(adc_continuous_io_to_channel(gpio, &adc_unit, &adc_channel));

        adc_continuous_handle_cfg_t adc_config = {};
        adc_config.max_store_buf_size = 4 * SOC_ADC_DIGI_RESULT_BYTES * READ_LEN;
        adc_config.conv_frame_size = SOC_ADC_DIGI_RESULT_BYTES * READ_LEN;
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

        adc_continuous_evt_cbs_t callback_cfg={};
        callback_cfg.on_conv_done=adc_continuous_callback;
        callback_cfg.on_pool_ovf=nullptr;
        if(runInInterruptMode){
            ESP_ERROR_CHECK(adc_continuous_register_event_callbacks(handle, &callback_cfg, nullptr));
        }

        ESP_ERROR_CHECK(adc_continuous_start(handle));

        ESP_LOGI(TAG, "Encoder successfully initialized");
    }
}
#undef TAG