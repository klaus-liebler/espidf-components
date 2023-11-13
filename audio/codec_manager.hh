#pragma once
#include <stdint.h>
#include <errorcodes.hh>

#include "driver/dac_continuous.h"
#define TAG "CODEC"

static const char base64_table[65] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static const char base64_url_table[65] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

static char *base64_gen_encode(const unsigned char *src, size_t len,
                               size_t *out_len, const char *table, int add_pad)
{
    char *out, *pos;
    const unsigned char *end, *in;
    size_t olen;
    int line_len;

    if (len >= SIZE_MAX / 4)
        return NULL;
    olen = len * 4 / 3 + 4; /* 3-byte blocks to 4-byte */
    if (add_pad)
        olen += olen / 72; /* line feeds */
    olen++;                /* nul termination */
    if (olen < len)
        return NULL; /* integer overflow */
    out = (char*)malloc(olen);
    if (out == NULL)
        return NULL;

    end = src + len;
    in = src;
    pos = out;
    line_len = 0;
    while (end - in >= 3)
    {
        *pos++ = table[(in[0] >> 2) & 0x3f];
        *pos++ = table[(((in[0] & 0x03) << 4) | (in[1] >> 4)) & 0x3f];
        *pos++ = table[(((in[1] & 0x0f) << 2) | (in[2] >> 6)) & 0x3f];
        *pos++ = table[in[2] & 0x3f];
        in += 3;
        line_len += 4;
        if (add_pad && line_len >= 72)
        {
            *pos++ = '\n';
            line_len = 0;
        }
    }

    if (end - in)
    {
        *pos++ = table[(in[0] >> 2) & 0x3f];
        if (end - in == 1)
        {
            *pos++ = table[((in[0] & 0x03) << 4) & 0x3f];
            if (add_pad)
                *pos++ = '=';
        }
        else
        {
            *pos++ = table[(((in[0] & 0x03) << 4) |
                            (in[1] >> 4)) &
                           0x3f];
            *pos++ = table[((in[1] & 0x0f) << 2) & 0x3f];
        }
        if (add_pad)
            *pos++ = '=';
        line_len += 4;
    }

    if (add_pad && line_len)
        *pos++ = '\n';

    *pos = '\0';
    if (out_len)
        *out_len = pos - out;
    return out;
}

char *base64_encode(const void *src, size_t len, size_t *out_len)
{
    return base64_gen_encode((const unsigned char *)src, len, out_len, base64_table, 1);
}

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
        virtual ErrorCode SetPowerState(bool power) = 0;
        virtual ErrorCode SetVolume(uint8_t volume) = 0;
        virtual ErrorCode SetSampleRate(uint32_t sampleRateHz) = 0;
        /**
         * Alle notwendigen Infomationen werden im Konstruktor übergeben. Das Init initialisiert dann sowohl die ESP32-internen Ressourcen als auch den Codec (also den externen IC)
         */
        virtual ErrorCode Init() = 0;
        virtual ErrorCode WriteAudioData(AudioFormat format, size_t samples, void *buf) = 0;
    };



    class I2sWithHardwareVolume : public CodecManagerInterface
    {
    public:
        ErrorCode SetSampleRate(uint32_t sampleRateHz) override
        {
            ESP_LOGI(TAG, "Change sample rate to %ld Hz", sampleRateHz);
            i2s_std_clk_config_t clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG((uint32_t)sampleRateHz);
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
                .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(44100),
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