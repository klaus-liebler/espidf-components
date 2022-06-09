#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <cstring>
#include <algorithm>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_system.h>
#include <esp_log.h>
#include <driver/gpio.h>
#include <driver/dac.h>
#include <driver/i2s.h>
#define MINIMP3_ONLY_MP3
#define MINIMP3_NO_SIMD
#define MINIMP3_NONSTANDARD_BUT_LOGICAL
#define MINIMP3_IMPLEMENTATION
#include "minimp3.h"

#define TAG "MP3"


namespace MP3
{
    constexpr size_t SAMPLES_PER_FRAME = 1152;
    constexpr size_t FRAME_MAX_SIZE_BYTES = 1024;
    constexpr size_t HEADER_LENGTH_BYTES = 4;
    constexpr uint8_t SYNCWORDH = 0xff;
    constexpr uint8_t SYNCWORDL = 0xf0;

    enum class PlayerMode{
        UNINITIALIZED,
        INTERNAL_DAC,
        EXTERNAL_I2S_DAC,
    };

    class Player
{
private:
    mp3dec_t *decoder;
    i2s_port_t i2s_num;
    int frameStart{0};
    int16_t *outBuffer;
    uint8_t *inBuffer{nullptr};
    const uint8_t *file{nullptr};
    size_t fileLen{0};
    int64_t timeCalc{0};
    int64_t timeWrite{0};
    PlayerMode mode{PlayerMode::UNINITIALIZED};
    uint8_t  gainF2P6{0};
    int currentSampleRateHz{0};

    int FindSyncWordOnAnyPosition(int offset)
    {
        for (int i = offset; i < fileLen - 1; i++)
        {
            if ((file[i + 0] & MP3::SYNCWORDH) == MP3::SYNCWORDH && (file[i + 1] & MP3::SYNCWORDL) == MP3::SYNCWORDL)
                return i;
        }
        return -1;
    }

public:
    bool IsEmittingSamples(){
        return file != nullptr;
    }
    
    esp_err_t Play(const uint8_t *file, size_t fileLen)
    {
        if(mode==PlayerMode::UNINITIALIZED){
            ESP_LOGE(TAG, "MP3 player not initialized");
            return ESP_FAIL;
        }
        i2s_zero_dma_buffer(i2s_num);
        this->file = file;
        this->fileLen = fileLen;
        // frameStart=0;
        frameStart = FindSyncWordOnAnyPosition(0);
        if(frameStart<0){
            ESP_LOGE(TAG, "No synch word found in file!");
            this->file = nullptr;
            this->fileLen = 0;
            return ESP_FAIL;
        }
        mp3dec_init(decoder);
        ESP_LOGI(TAG, "Successfully initialized a new sound play task. File=%p; FileLen=%zu; FrameStart=%d;", file, fileLen, frameStart);
        return ESP_OK;
    }

    esp_err_t Stop()
    {
        if(mode==PlayerMode::UNINITIALIZED){
            ESP_LOGE(TAG, "MP3 player not initialized");
            return ESP_FAIL;
        }
        i2s_zero_dma_buffer(i2s_num);
        this->file = nullptr;
        this->fileLen = 0;
        this->frameStart = 0;
        return ESP_OK;
    }

    void SetGain(float f)
    {
        if (f > 4.0)
            f = 4.0;
        if (f < 0.0)
            f = 0.0;
        gainF2P6 = (uint8_t)(f * (1 << 6));
    }

    void AmplifyAndClampAndNormalizeForDAC(int16_t& s)
    {
        int32_t v = (s * gainF2P6) >> 6;
        if(v>INT16_MAX)
            s=INT16_MAX;
        else if(v<INT16_MIN)
            s=INT16_MIN;
        else
            s=v;
        if(mode==PlayerMode::INTERNAL_DAC){
            s+=0x8000;
        }
    }

    esp_err_t Loop()
    {
        if(mode==PlayerMode::UNINITIALIZED){
            ESP_LOGE(TAG, "MP3 player not initialized");
            return ESP_FAIL;
        }
        if (!file)
        {
            vTaskDelay(pdMS_TO_TICKS(50));
            return ESP_OK;
        }

        if (frameStart >= fileLen)
        {
            ESP_LOGI(TAG, "Reached End of File. decode time %lld ms, writeTime %lld ms", timeCalc / 1000, timeWrite/1000);
            timeCalc=0;
            timeWrite=0;
            file = nullptr;
            fileLen = 0;
            i2s_zero_dma_buffer(i2s_num);
            return ESP_OK;
        }
        int bytesLeft = fileLen - frameStart;
        mp3dec_frame_info_t info={};
        int64_t now = esp_timer_get_time();
        
        int samples = mp3dec_decode_frame(decoder, file + frameStart, bytesLeft, this->outBuffer, &info);
        timeCalc += esp_timer_get_time() - now;
        
        this->frameStart += info.frame_bytes;
            
        if(this->currentSampleRateHz!=info.hz){
            i2s_set_sample_rates(i2s_num, info.hz);
            ESP_LOGI(TAG, "Change sample rate to %d Hz", info.hz);
            this->currentSampleRateHz=info.hz;
        }
        
        if (samples == 0)
        {
            return ESP_OK;
        }
        if(info.channels==1){
            for (int i = samples-1; i >=0; i--)
            {
                int16_t l = outBuffer[i];
                int16_t r = outBuffer[i];
                AmplifyAndClampAndNormalizeForDAC(l);
                AmplifyAndClampAndNormalizeForDAC(r);
                outBuffer[2*i] = l;
                outBuffer[2*i + 1] = r;
            }
        }else if(info.channels==2){
            for (size_t i = 0; i < samples * 2; i += 2)
            {
                int16_t l = outBuffer[i];
                int16_t r = outBuffer[i + 1];
                AmplifyAndClampAndNormalizeForDAC(l);
                AmplifyAndClampAndNormalizeForDAC(r);
                outBuffer[i] = l;
                outBuffer[i + 1] = r;
            }
        }

        size_t i2s_bytes_writen;
        int i2s_bytes_to_write{samples * 4};
        now = esp_timer_get_time();
        i2s_write(i2s_num, outBuffer, i2s_bytes_to_write, &i2s_bytes_writen, portMAX_DELAY);
        timeWrite += esp_timer_get_time() - now;
        if(i2s_bytes_to_write!=i2s_bytes_writen){
            ESP_LOGE(TAG, "i2s_bytes_to_write!=i2s_bytes_writen");
        }
        return ESP_OK;
    }

    Player()
    {
        this->outBuffer = new int16_t[2 * MP3::SAMPLES_PER_FRAME];
        decoder = new mp3dec_t();
        SetGain(1.0);
    }

    esp_err_t InitExternalI2SDAC(i2s_port_t i2s_num, gpio_num_t bck, gpio_num_t ws, gpio_num_t data)
    {
        ESP_LOGI(TAG, "Initializing I2S_NUM_0 for internal DAC");
        this->i2s_num = i2s_num;
        this->mode = PlayerMode::UNINITIALIZED;
        mp3dec_init(decoder);
        i2s_config_t i2s_config;
        i2s_config.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
        i2s_config.sample_rate = 44100;
        i2s_config.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
        i2s_config.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT; // 2-channels
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 2, 0)
        i2s_config.communication_format = (i2s_comm_format_t) I2S_COMM_FORMAT_STAND_MSB;
        i2s_config.dma_desc_num = 8;
        i2s_config.dma_frame_num = 64; // MP3::SAMPLES_PER_FRAME/2; //Max 1024
#else
        i2s_config.communication_format = (i2s_comm_format_t) I2S_COMM_FORMAT_I2S_MSB;
        i2s_config.dma_buf_count = 4,
        i2s_config.dma_buf_len = 128,
#endif
        i2s_config.intr_alloc_flags = 0; // Default interrupt priority
        i2s_config.use_apll = false;
        i2s_config.fixed_mclk = 0;
        i2s_config.tx_desc_auto_clear = false; // Auto clear tx descriptor on underflow

        ESP_ERROR_CHECK(i2s_driver_install(i2s_num, &i2s_config, 0, NULL));
        i2s_pin_config_t pin_config;
        pin_config.bck_io_num = (int)bck;
        pin_config.ws_io_num = (int)ws;
        pin_config.data_out_num = (int)data;
        pin_config.data_in_num = -1; // Not used
        ESP_ERROR_CHECK(i2s_set_pin(i2s_num, &pin_config));
        this->mode = PlayerMode::EXTERNAL_I2S_DAC;
        return ESP_OK;
    }

    esp_err_t InitInternalDACMonoRightPin25()
    {
        ESP_LOGI(TAG, "Initializing I2S_NUM_0 for internal DAC");
        this->mode = PlayerMode::UNINITIALIZED;
        this->i2s_num = I2S_NUM_0; // only I2S_NUM_0 has access to internal DAC!!!
        mp3dec_init(decoder);
        i2s_config_t i2s_config={};
        i2s_config.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_DAC_BUILT_IN); // see https://github.com/earlephilhower/ESP8266Audio/blob/master/src/AudioOutputI2S.cpp
        i2s_config.sample_rate = 44100;
        i2s_config.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
        i2s_config.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 2, 0)
        i2s_config.communication_format = (i2s_comm_format_t) I2S_COMM_FORMAT_STAND_MSB;
        i2s_config.dma_desc_num = 8;
        i2s_config.dma_frame_num = 64; // MP3::SAMPLES_PER_FRAME/2; //Max 1024
#else
        i2s_config.communication_format = (i2s_comm_format_t) I2S_COMM_FORMAT_I2S_MSB;
        i2s_config.dma_buf_count = 4,
        i2s_config.dma_buf_len = 128,
#endif
        i2s_config.intr_alloc_flags = 0;
        i2s_config.use_apll = false;
        i2s_config.fixed_mclk = 0;
        i2s_config.tx_desc_auto_clear = false; // Auto clear tx descriptor on underflow

        ESP_ERROR_CHECK(i2s_driver_install(i2s_num, &i2s_config, 0, NULL));
        ESP_LOGI(TAG, "i2s_driver_install");
        ESP_ERROR_CHECK(i2s_set_pin(i2s_num, NULL));
        ESP_LOGI(TAG, "i2s_set_pin");
        ESP_ERROR_CHECK(i2s_set_dac_mode(I2S_DAC_CHANNEL_BOTH_EN));
        ESP_LOGI(TAG, "i2s_set_dac_mode");
        ESP_ERROR_CHECK(i2s_set_sample_rates(i2s_num, 44100));
        ESP_LOGI(TAG, "i2s_set_sample_rates");
        dac_output_disable(DAC_CHANNEL_2);
        gpio_reset_pin(GPIO_NUM_26);
        this->mode = PlayerMode::INTERNAL_DAC;
        return ESP_OK;
    }
};

}

#undef TAG