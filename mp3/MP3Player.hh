#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <cstring>
#include <algorithm>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include <driver/gpio.h>
#include <driver/dac.h>
#include <driver/i2s.h>
#define MINIMP3_ONLY_MP3
#define MINIMP3_NO_SIMD
#define MINIMP3_NONSTANDARD_BUT_LOGICAL
#define MINIMP3_IMPLEMENTATION
#include "minimp3.h"
#include <esp_log.h>

#define TAG "MP3"

namespace MP3
{
    constexpr size_t SAMPLES_PER_FRAME = 1152;
    constexpr size_t FRAME_MAX_SIZE_BYTES = 1024;
    constexpr size_t HEADER_LENGTH_BYTES = 4;
    constexpr uint8_t SYNCWORDH = 0xff;
    constexpr uint8_t SYNCWORDL = 0xf0;
}

class MP3Player
{
private:
    mp3dec_t decoder;
    i2s_port_t i2s_num;
    int frameStart{0};
    int16_t *outBuffer;
    uint8_t *inBuffer{nullptr};
    const uint8_t *file{nullptr};
    size_t fileLen{0};
    int64_t timeCalc{0};
    bool useInternalDAC{false};

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
    esp_err_t Play(const uint8_t *file, size_t fileLen)
    {
        i2s_zero_dma_buffer(i2s_num);
        this->file = file;
        this->fileLen = fileLen;
        //frameStart=0;
        frameStart = FindSyncWordOnAnyPosition(0);

        ESP_LOGI(TAG, "Successfully initialized a new sound play task. File=%p; FileLen=%zu; FrameStart=%d;", file, fileLen, frameStart);
        return ESP_OK;
    }

    esp_err_t Stop()
    {
        i2s_zero_dma_buffer(i2s_num);
        i2s_zero_dma_buffer(i2s_num);
        this->file = nullptr;
        this->fileLen = 0;
        this->frameStart=0;
        return ESP_OK;
    }

    esp_err_t Loop()
    {
        if (!file)
        {
            vTaskDelay(1);
            return ESP_OK;
        }

        if (frameStart >= fileLen)
        {
            ESP_LOGI(TAG, "Reached End of File. time ms %lld", timeCalc / 1000);
            file = nullptr;
            fileLen = 0;
            i2s_zero_dma_buffer(i2s_num);
            return ESP_OK;
        }
        int bytesLeft = fileLen - frameStart;
        mp3dec_frame_info_t info;
        int64_t now = esp_timer_get_time();
        int samples = mp3dec_decode_frame(&decoder, file + frameStart, bytesLeft, this->outBuffer, &info);
        timeCalc += esp_timer_get_time() - now;
        this->frameStart += info.frame_bytes;
        ESP_LOGD(TAG, "frameStart=%d; frame_bytes=%d; samples=%d", frameStart, info.frame_bytes, samples); //frameStart=82672; frame_bytes=418; samples=1152
        if (samples == 0)
        {
            return ESP_OK;
        }

        uint16_t* outBufferU16 = (uint16_t*)outBuffer;
        for (size_t i = 0; i < samples*2; i += 2)
        {
            int l = outBuffer[i];
            int r = outBuffer[i+1];
            l/=2;
            r/=2;
            if(useInternalDAC){
                l+=0x8000;
                r+=0x8000;
                outBufferU16[i]   = l;
                outBufferU16[i+1] = r;
            }else{
                outBuffer[i]   = l;
                outBuffer[i+1] = r;
            }
            //printf("%f ", l/32768.0);
        }
        //printf("\n");
        vTaskDelay(1);

        size_t i2s_bytes_write;
        i2s_write(i2s_num, outBuffer, samples * 4, &i2s_bytes_write, portMAX_DELAY);
        return ESP_OK;
    }

    MP3Player()
    {
        this->outBuffer = new int16_t[2 * MP3::SAMPLES_PER_FRAME];
    }

    esp_err_t InitExternalI2SDAC(i2s_port_t i2s_num, gpio_num_t bck, gpio_num_t ws, gpio_num_t data)
    {
        this->i2s_num=i2s_num;
        mp3dec_init(&decoder);
        i2s_config_t i2s_config;
        i2s_config.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
        i2s_config.sample_rate = 44100;
        i2s_config.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
        i2s_config.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT; //2-channels
        i2s_config.communication_format = I2S_COMM_FORMAT_STAND_MSB;
        i2s_config.intr_alloc_flags = 0; //Default interrupt priority
        i2s_config.dma_buf_count = 8;
        i2s_config.dma_buf_len = 1024; //MP3::SAMPLES_PER_FRAME/2; //Max 1024
        i2s_config.use_apll = false;
        i2s_config.fixed_mclk = -1;
        i2s_config.tx_desc_auto_clear = true; //Auto clear tx descriptor on underflow

        ESP_ERROR_CHECK(i2s_driver_install(i2s_num, &i2s_config, 0, NULL));
        i2s_pin_config_t pin_config;
        pin_config.bck_io_num = (int)bck;
        pin_config.ws_io_num = (int)ws;
        pin_config.data_out_num = (int)data;
        pin_config.data_in_num = -1; //Not used
        ESP_ERROR_CHECK(i2s_set_pin(i2s_num, &pin_config));
        return ESP_OK;
    }

    esp_err_t InitInternalDAC(i2s_dac_mode_t dac_mode)
    {
        this->useInternalDAC=true;
        this->i2s_num = I2S_NUM_0;
        mp3dec_init(&decoder);
        i2s_config_t i2s_config;
        i2s_config.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_DAC_BUILT_IN); // Only TX
        i2s_config.sample_rate = 44100;
        i2s_config.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
        i2s_config.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT; //2-channels
        i2s_config.communication_format = I2S_COMM_FORMAT_STAND_I2S;
        i2s_config.intr_alloc_flags = ESP_INTR_FLAG_LEVEL3; 
        i2s_config.dma_buf_count = 16;
        i2s_config.dma_buf_len = 64; //Max 1024
        i2s_config.use_apll = false;
        i2s_config.fixed_mclk = 0;
        i2s_config.tx_desc_auto_clear = true; //Auto clear tx descriptor on underflow

        ESP_ERROR_CHECK(i2s_driver_install(i2s_num, &i2s_config, 0, NULL));

        ESP_ERROR_CHECK(i2s_set_pin(i2s_num, NULL));
        ESP_ERROR_CHECK(i2s_set_dac_mode(dac_mode));
        dac_output_disable(DAC_CHANNEL_2);
        gpio_reset_pin(GPIO_NUM_26);
        return ESP_OK;
    }
};
#undef TAG