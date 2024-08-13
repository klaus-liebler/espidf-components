#pragma once
#include <stdint.h>
#include <errorcodes.hh>

#include "driver/dac_continuous.h"
#define TAG "CODEC"

namespace CodecManager
{
    enum class AudioFormat
    {
        Mono_8bit,
        Mono_16bit,
        Stereo_16bit,
    };

    class CodecManagerInterface
    {
    protected:
        uint8_t gainF2P6{1 << 6};
        void SetGain(float gain_0to4)
        {
            if (gain_0to4 < 0.0)
            {
                gain_0to4 = 0.0;
            };
            if (gain_0to4 > 3.99)
            {
                gain_0to4 = 3.99;
            }
            gainF2P6 = (gain_0to4 * (1 << 6));
        }

        int16_t GainAndClip(int16_t audio)
        {
            int32_t v = (audio * gainF2P6) >> 6;
            return clip(v, (int32_t)INT16_MIN, (int32_t)INT16_MAX);
        }

    public:
        virtual ErrorCode WriteAudioData(AudioFormat format, size_t samples, void *buf) = 0;
        virtual ErrorCode SetSampleRate(uint32_t sampleRateHz) = 0;
        
        virtual ErrorCode SetPowerState(bool power) = 0;
        virtual ErrorCode SetVolume(uint8_t volume) = 0;
        virtual ErrorCode Init() = 0;
    };

    class I2sWithHardwareVolume : public CodecManagerInterface
    {
    public:
        ErrorCode SetSampleRate(uint32_t sampleRateHz) override
        {
            if(this->currentSampleRateHz==sampleRateHz){
                return ErrorCode::OK;
            }
            ESP_LOGD(TAG, "Change sample rate to %ld Hz", sampleRateHz);
            
            i2s_std_clk_config_t clk_cfg ={};//this produces warning! I2S_STD_CLK_DEFAULT_CONFIG((uint32_t)sampleRateHz);
            clk_cfg.sample_rate_hz = sampleRateHz;
            clk_cfg.clk_src = I2S_CLK_SRC_DEFAULT;
            clk_cfg.mclk_multiple = I2S_MCLK_MULTIPLE_256;
            ESP_ERROR_CHECK(i2s_channel_disable(tx_handle));
            ESP_ERROR_CHECK(i2s_channel_reconfig_std_clock(tx_handle, &clk_cfg));
            ESP_ERROR_CHECK(i2s_channel_enable(tx_handle));
            this->currentSampleRateHz = sampleRateHz;
            return ErrorCode::OK;
        }

        ErrorCode WriteAudioData(AudioFormat format, size_t samples, void *buf) override
        {
            RETURN_ERRORCODE_ON_ERROR(i2s_channel_write(tx_handle, buf, samples * 4, nullptr, -1), ErrorCode::GENERIC_ERROR);
            return ErrorCode::OK;
        }

    private:
        i2s_chan_handle_t tx_handle{nullptr};
        uint32_t currentSampleRateHz{44100};

    protected:
        
        ErrorCode InitI2sEsp32(gpio_num_t mclk, gpio_num_t bck, gpio_num_t ws, gpio_num_t data)
        {
            ESP_LOGI(TAG, "Initializing AudioPlayer for external I2S DAC");
            i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
            chan_cfg.auto_clear = true; // Auto clear the legacy data in the DMA buffer
            ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &tx_handle, nullptr));
            i2s_std_config_t std_cfg = {
                .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(currentSampleRateHz),
                .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
                .gpio_cfg = {
                    .mclk = mclk,
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
            return ErrorCode::OK;
        }
    };
}
#undef TAG