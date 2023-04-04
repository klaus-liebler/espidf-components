#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <cstring>
#include <algorithm>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <esp_system.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <driver/gpio.h>
#include <driver/i2s_std.h>

#define MINIMP3_ONLY_MP3
#define MINIMP3_NO_SIMD
#define MINIMP3_NONSTANDARD_BUT_LOGICAL
#define MINIMP3_IMPLEMENTATION
#include "minimp3.h"

// https://voicemaker.in/
// Neural TTS, German, Katja, 24000Hz, VoiceSpeed+20%

#define TAG "AUDIO"

namespace AudioPlayer
{
    namespace MP3{
        constexpr size_t SAMPLES_PER_FRAME = 1152;
        constexpr size_t FRAME_MAX_SIZE_BYTES = 1024;
        constexpr size_t HEADER_LENGTH_BYTES = 4;
        constexpr uint8_t SYNCWORDH = 0xff;
        constexpr uint8_t SYNCWORDL = 0xf0;
    }

    enum class AudioType
    {
        SILENCE,
        MP3,
        PCM,
        VOLTAGE, //for future use
        SYNTHESIZER, // for future use
    };

    struct AudioOrder{
        AudioType type;
        const uint8_t *file;
        size_t fileLen;
        uint32_t sampleRate;//0 means "auto detect"
        bool cancelPrevious; //false means: play current AudioOrder to its end
    };

    constexpr AudioOrder SILENCE_ORDER{AudioType::SILENCE, nullptr, 0, 0, true};

    class Player
    {
    private:
        QueueHandle_t orderQueue{nullptr};
        mp3dec_t *decoder;
        i2s_chan_handle_t tx_handle{nullptr};
        int32_t frameStart{0};
        int16_t *outBuffer{nullptr};

        AudioOrder currentOrder;

        uint8_t gainF2P6{0};
        int32_t currentSampleRateHz{0};
        bool usesInternalDac{false};

        static int FindSyncWordOnAnyPosition(const uint8_t *file, size_t fileLen, int offset){
            for (int i = offset; i < fileLen - 1; i++)
            {
                if ((file[i + 0] & MP3::SYNCWORDH) == MP3::SYNCWORDH && (file[i + 1] & MP3::SYNCWORDL) == MP3::SYNCWORDL)
                    return i;
            }
            return -1;
        }

        void SetGain(float f)
        {
            if (f > 4.0)
                f = 4.0;
            if (f < 0.0)
                f = 0.0;
            gainF2P6 = (uint8_t)(f * (1 << 6));
        }

        void AmplifyAndClampAndNormalizeForDAC(int16_t &s){
            int32_t v = (s * gainF2P6) >> 6;
            if (v > INT16_MAX)
                s = INT16_MAX;
            else if (v < INT16_MIN)
                s = INT16_MIN;
            else
                s = v;
            if (usesInternalDac)
            {
                s += 0x8000;
            }
        }

        esp_err_t LoopPCM(){
            size_t i2s_bytes_writen;
            ESP_ERROR_CHECK(i2s_channel_write(tx_handle, currentOrder.file, currentOrder.fileLen, &i2s_bytes_writen, portMAX_DELAY));
            if(i2s_bytes_writen!=currentOrder.fileLen){
                ESP_LOGE(TAG, "Not all bytes are written!!!");
            }
            currentOrder=SILENCE_ORDER;
            return ESP_OK;
        }
        
        esp_err_t LoopMP3(){
            if (frameStart >= currentOrder.fileLen)
            {
                ESP_LOGI(TAG, "Reached End of MP3 File.");
                currentOrder=SILENCE_ORDER;
                return ESP_OK;
            }
            int bytesLeft = currentOrder.fileLen - frameStart;
            mp3dec_frame_info_t info = {};
            int samples = mp3dec_decode_frame(decoder, currentOrder.file + frameStart, bytesLeft, this->outBuffer, &info);
            this->frameStart += info.frame_bytes;
            if (samples == 0){return ESP_OK;}

            if (this->currentSampleRateHz != info.hz)
            {
                ESP_LOGI(TAG, "Change sample rate to %d Hz", info.hz);
                i2s_std_clk_config_t clk_cfg=I2S_STD_CLK_DEFAULT_CONFIG((uint32_t)info.hz);
                ESP_ERROR_CHECK(i2s_channel_disable(tx_handle));
                ESP_ERROR_CHECK(i2s_channel_reconfig_std_clock(tx_handle, &clk_cfg));
                ESP_ERROR_CHECK(i2s_channel_enable(tx_handle));
                this->currentSampleRateHz = info.hz;
            }
            
            if (info.channels == 1)
            {
                for (int i = samples - 1; i >= 0; i--)
                {
                    int16_t l = outBuffer[i];
                    int16_t r = outBuffer[i];
                    AmplifyAndClampAndNormalizeForDAC(l);
                    AmplifyAndClampAndNormalizeForDAC(r);
                    outBuffer[2 * i] = l;
                    outBuffer[2 * i + 1] = r;
                }
            }
            else if (info.channels == 2 && usesInternalDac)
            {
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
            ESP_LOGD(TAG, "Writing %d bytes to i2s", i2s_bytes_to_write);
            ESP_ERROR_CHECK(i2s_channel_write(tx_handle, outBuffer, i2s_bytes_to_write, &i2s_bytes_writen, portMAX_DELAY));

            if (i2s_bytes_to_write != i2s_bytes_writen)
            {
                ESP_LOGE(TAG, "i2s_bytes_to_write!=i2s_bytes_writen");
            }
            return ESP_OK;
        }

        esp_err_t InitMP3(){
            frameStart = FindSyncWordOnAnyPosition(currentOrder.file, currentOrder.fileLen, 0);
            if (frameStart < 0)
            {
                ESP_LOGE(TAG, "No synch word found in file!");
                return ESP_FAIL;
            }
            mp3dec_init(decoder);
            ESP_LOGI(TAG, "Successfully initialized a new MP3 sound play task. File=%p; FileLen=%zu; FrameStart=%ld;", currentOrder.file, currentOrder.fileLen, frameStart);
            return ESP_OK;
        }

        esp_err_t InitPCM(){
            frameStart=0;
            i2s_std_clk_config_t clk_cfg=I2S_STD_CLK_DEFAULT_CONFIG((uint32_t)currentOrder.sampleRate);
            ESP_ERROR_CHECK(i2s_channel_disable(tx_handle));
            ESP_ERROR_CHECK(i2s_channel_reconfig_std_clock(tx_handle, &clk_cfg));
            ESP_ERROR_CHECK(i2s_channel_enable(tx_handle));
            this->currentSampleRateHz = currentOrder.sampleRate;
            ESP_LOGI(TAG, "Successfully initialized a new PCM sound play task. File=%p; FileLen=%zu; SampleRate %lu", currentOrder.file, currentOrder.fileLen, currentOrder.sampleRate);
            return ESP_OK;
        }

        esp_err_t InitSilence(){
            return ESP_OK;
        }


    public:
        bool IsEmittingSamples()
        {
            return currentOrder.type!=AudioType::SILENCE;
        }

        esp_err_t PlayMP3(const uint8_t *file, size_t fileLen, bool stopPrevious)
        {
            if (tx_handle==nullptr)
            {
                ESP_LOGE(TAG, "AudioPlayer not initialized");
                return ESP_FAIL;
            }
            AudioOrder ao{AudioType::MP3, file, fileLen, 0, stopPrevious};
            xQueueOverwrite(orderQueue, &ao);
            return ESP_OK;
        }

        esp_err_t PlayPCM(const uint8_t *file, size_t fileLen, uint32_t sampleRate,  bool stopPrevious)
        {
            if (tx_handle==nullptr)
            {
                ESP_LOGE(TAG, "AudioPlayer not initialized");
                return ESP_FAIL;
            }
            AudioOrder ao{AudioType::PCM, file, fileLen, sampleRate, stopPrevious};
            xQueueOverwrite(orderQueue, &ao);
            return ESP_OK;
        }

        esp_err_t Stop()
        {
            xQueueOverwrite(orderQueue, &SILENCE_ORDER);
            return ESP_OK;
        }

 
        esp_err_t Loop(){
            AudioOrder temp;
            if(currentOrder.type==AudioType::SILENCE || (xQueuePeek(orderQueue, &temp, 0) && temp.cancelPrevious)){
                if(xQueueReceive(orderQueue, &currentOrder, 0)){
                    switch (currentOrder.type){
                        case AudioType::MP3:
                            if(InitMP3()!=ESP_OK){
                                currentOrder=SILENCE_ORDER;
                                InitSilence();
                            }
                            break;
                        case AudioType::PCM:
                            if(InitPCM()!=ESP_OK){
                                currentOrder=SILENCE_ORDER;
                                InitSilence();
                            }
                            break;
                        default:
                            InitSilence();
                            break;
                    }
                }
            }

            switch (currentOrder.type)
            {
            case AudioType::MP3:
                return LoopMP3();
                break;
            case AudioType::PCM:
                return LoopPCM();
                break;
            default:
                vTaskDelay(pdMS_TO_TICKS(50));
                return ESP_OK;
            }
        }

        Player()
        {
            this->outBuffer = new int16_t[2 * MP3::SAMPLES_PER_FRAME];
            this->decoder = new mp3dec_t();
            this->orderQueue = xQueueCreate(1,sizeof(AudioOrder));
            mp3dec_init(decoder);
            SetGain(1.0);
        }

        esp_err_t InitExternalI2SDAC(gpio_num_t bck, gpio_num_t ws, gpio_num_t data)
        {
            ESP_LOGI(TAG, "Initializing AudioPlayer for external I2S DAC");
            i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
            chan_cfg.auto_clear = true; // Auto clear the legacy data in the DMA buffer
            ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &tx_handle, nullptr));
            i2s_std_config_t std_cfg = {
                .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(44100),
                .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
                .gpio_cfg = {
                    .mclk = GPIO_NUM_NC,
                    .bclk = bck,
                    .ws = ws,
                    .dout = data,
                    .din = GPIO_NUM_NC,
                    .invert_flags = {
                        .mclk_inv = false,
                        .bclk_inv = false,
                        .ws_inv = false,
                    },
                },
            };
            ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_handle, &std_cfg));
            ESP_ERROR_CHECK(i2s_channel_enable(tx_handle));
            return ESP_OK;
        }

#ifdef USE_OLD_I2S_API
        esp_err_t InitInternalDACMonoRightPin25()
        {
            ESP_LOGI(TAG, "Initializing I2S_NUM_0 for internal DAC");
            this->output = Output::UNINITIALIZED;
            i2s_port_t i2s_num = I2S_NUM_0; // only I2S_NUM_0 has access to internal DAC!!!
            i2s_config_t i2s_config = {};
            i2s_config.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_DAC_BUILT_IN); // see https://github.com/earlephilhower/ESP8266Audio/blob/master/src/AudioOutputI2S.cpp
            i2s_config.sample_rate = 44100;
            i2s_config.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
            i2s_config.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;

            i2s_config.communication_format = (i2s_comm_format_t)I2S_COMM_FORMAT_STAND_MSB;
            i2s_config.dma_desc_num = 8;
            i2s_config.dma_frame_num = 64; // MP3::SAMPLES_PER_FRAME/2; //Max 1024

            i2s_config.intr_alloc_flags = 0;
            i2s_config.use_apll = false;
            i2s_config.fixed_mclk = 0;
            i2s_config.tx_desc_auto_clear = false; // Auto clear tx descriptor on underflow

            ESP_ERROR_CHECK(i2s_driver_install(i2s_num, &i2s_config, 0, nullptr));
            ESP_ERROR_CHECK(i2s_set_pin(i2s_num, nullptr));
            ESP_ERROR_CHECK(i2s_set_dac_mode(I2S_DAC_CHANNEL_RIGHT_EN));
            ESP_ERROR_CHECK(i2s_set_sample_rates(i2s_num, 24000));
            this->output = Output::INTERNAL_DAC;
            return ESP_OK;
        }
#endif
    };

}

#undef TAG