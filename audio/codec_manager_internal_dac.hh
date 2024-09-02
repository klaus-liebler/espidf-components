    #pragma once
#include <stdint.h>
#include <errorcodes.hh>
#include "codec_manager.hh"

#include "driver/dac_continuous.h"
namespace CodecManager{
	class InternalDacWithPotentiometer : public iCodecManager
    {
    private:
        uint32_t currentSampleRateHz{24000};
        dac_continuous_handle_t dac_handle{nullptr};

    public:
        ErrorCode SetPowerState(bool power) override
        {
            return ErrorCode::OK;
        }

        ErrorCode SetVolume(uint8_t volume) override
        {
            return ErrorCode::OK;
        }

        ErrorCode SetSampleRate(uint32_t sampleRateHz) override
        {
            if (sampleRateHz == currentSampleRateHz)
                return ErrorCode::OK;
            ESP_ERROR_CHECK(dac_continuous_disable(this->dac_handle));
            dac_continuous_del_channels(dac_handle);
            currentSampleRateHz = sampleRateHz;
            return Init();
        }
        ErrorCode Init() override
        {

            ESP_LOGI(TAG, "Initializing I2S_NUM_0 for internal DAC with sample rate %lu", this->currentSampleRateHz);
            dac_continuous_config_t cont_cfg = {};
            cont_cfg.chan_mask = DAC_CHANNEL_MASK_CH0;
            cont_cfg.desc_num = 4;
            cont_cfg.buf_size = 2048;
            cont_cfg.freq_hz = currentSampleRateHz;
            cont_cfg.offset = 0;
            cont_cfg.clk_src = DAC_DIGI_CLK_SRC_APLL;    // Using APLL as clock source to get a wider frequency range
            cont_cfg.chan_mode = DAC_CHANNEL_MODE_SIMUL; // Hat nur einen Effekt, wenn mehrere Kanäle aktiv sind
            /* Allocate continuous channels */
            ESP_ERROR_CHECK(dac_continuous_new_channels(&cont_cfg, &this->dac_handle));
            ESP_ERROR_CHECK(dac_continuous_enable(this->dac_handle));
            return ErrorCode::OK;
        }

        ErrorCode WriteAudioData(eChannels ch, eSampleBits bits, uint32_t sampleRateHz, size_t samples, void *buf) override
        {
            // Needs Mono, 8bit
            size_t dummy;
            uint8_t newBuf[samples];
            
            if(ch==eChannels::ONE && bits==eSampleBits::EIGHT){
                uint8_t *origBuf = (uint8_t *)buf;
                ESP_ERROR_CHECK(dac_continuous_write(dac_handle, origBuf, samples, &dummy, -1));
            }
            else if(ch==eChannels::ONE && bits==eSampleBits::SIXTEEN){
                int16_t *origBuf = (int16_t *)buf;
                int16_t max{INT16_MIN};
                uint8_t maxU8{0};
                int16_t min{INT16_MAX};
                uint8_t minU8{0};
                for (int i = 0; i < samples; i++)
                {

                    int val = (origBuf[i] + 0x8000) >> 8; // werte von 0 bis +255

                    if (origBuf[i] < min)
                    {
                        min = origBuf[i];
                        minU8 = val;
                    }
                    if (origBuf[i] > max)
                    {
                        max = origBuf[i];
                        maxU8 = val;
                    }
                    newBuf[i] = val;
                }

                //char *outBase64 = base64_encode(origBuf, 2*samples, nullptr);
                //char *outBase64 = base64_encode(newBuf, samples, nullptr);
                //printf("%s", outBase64);
                //free(outBase64);

                ESP_LOGD(TAG, "max=%d, min=%d, maxU8=%d, minU8=%d", max, min, maxU8, minU8);
                ESP_ERROR_CHECK(dac_continuous_write(dac_handle, newBuf, samples, &dummy, -1));
            }
            else if(ch==eChannels::TWO && bits==eSampleBits::SIXTEEN){
                int16_t *origBuf = (int16_t *)buf;
                int16_t max{INT16_MIN};
                uint8_t maxU8{0};
                int16_t min{INT16_MAX};
                uint8_t minU8{0};
                for (int i = 0; i < samples; i++)
                {
                    // Mische rechten und linken Kanal
                    int val = (origBuf[2 * i] + 0x8000 + origBuf[2 * i + 1] + 0x8000) >> 9; // werte von 0 bis +255, 8 bits nach rechts und dann noch wegen dem mix durch 2 teilen

                    if (origBuf[i] < min)
                    {
                        min = origBuf[i];
                        minU8 = val;
                    }
                    if (origBuf[i] > max)
                    {
                        max = origBuf[i];
                        maxU8 = val;
                    }
                    newBuf[i] = val;
                }
                ESP_LOGI(TAG, "max=%d, min=%d, maxU8=%d, minU8=%d", max, min, maxU8, minU8);
                ESP_ERROR_CHECK(dac_continuous_write(dac_handle, newBuf, samples, &dummy, -1));
            }
           
            return ErrorCode::OK;
        }
    };
}