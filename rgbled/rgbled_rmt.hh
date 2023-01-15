#pragma once
#include <stddef.h>
#include <string.h>
#include <common.hh>
#include <esp_log.h>
#include <driver/rmt.h>
#include <driver/gpio.h>
#include <esp_timer.h>
#include "crgb.hh"
#include <array>
#define TAG "RGBLED"
namespace RGBLED
{
    enum class Timing
    {
        WS2812=0,
        PL9823=1,
        SK6805=2,
        MAX
    };

    constexpr uint32_t counter_clk_hz = 40000000;
    // ns -> ticks
    constexpr float ratio = (float)counter_clk_hz / 1e9;
    
    constexpr uint32_t T(int ns){
        return (uint32_t)(ratio * ns);
    }

    constexpr uint32_t TIMING[(size_t)Timing::MAX][4]={
    //  T0H      T0L      T1H      T1L
        {T(350), T(1000), T(1000), T(350)},
        {T(350), T(1000), T(1000), T(350)},
        //{T(300), T(1000), T(640), T(660)},
        {T(300), T(900), T(600), T(600)},
    };
    
    
    template <size_t LEDSIZE, Timing DEVICE>
    class M
    {
    private:
        rmt_channel_t rmt_channel;

        
        
        static void IRAM_ATTR ws2812_rmt_adapter(const void *src, rmt_item32_t *dest, size_t src_size, size_t wanted_num, size_t *translated_size, size_t *item_num)
        {
            if (src == NULL || dest == NULL)
            {
                *translated_size = 0;
                *item_num = 0;
                return;
            }

            const rmt_item32_t bit0 = {{{TIMING[(size_t)DEVICE][0], 1, TIMING[(size_t)DEVICE][1], 0}}}; // Logical 0
            const rmt_item32_t bit1 = {{{TIMING[(size_t)DEVICE][2], 1, TIMING[(size_t)DEVICE][3], 0}}}; // Logical 1
            size_t size = 0;
            size_t num = 0;
            uint8_t *psrc = (uint8_t *)src;
            rmt_item32_t *pdest = dest;
            while (size < src_size && num < wanted_num)
            {
                for (int i = 0; i < 8; i++)
                {
                    // MSB first
                    if (*psrc & (1 << (7 - i)))
                    {
                        pdest->val = bit1.val;
                    }
                    else
                    {
                        pdest->val = bit0.val;
                    }
                    num++;
                    pdest++;
                }
                size++;
                psrc++;
            }
            *translated_size = size;
            *item_num = num;
        }

       
        uint8_t table[3*LEDSIZE];

        bool dirty{true};

    public:
        M() {}

        esp_err_t SetPixel(size_t index, CRGB color, bool refresh = false)
        {
            if (index >= LEDSIZE)
                return ESP_FAIL;
            //patterns[index] = nullptr;
            uint32_t start = index * 3; //GRB
            if (this->table[start+0] != color.g || this->table[start+1] != color.r || this->table[start+2] != color.b)
            {
                ESP_LOGD(TAG, "Set Index %d to %d", index, color.raw32);
                this->table[start+0] = color.g;
                this->table[start+1] = color.r;
                this->table[start+2] = color.b;
                this->dirty = true;
            }
            if (refresh)
            {
                this->Refresh(1000, refresh);
            }
            return ESP_OK;
        }



        esp_err_t Refresh(uint32_t timeout_ms = 1000, bool forceRefreshEvenIfNotNecessary = false)
        {
            if (!dirty && !forceRefreshEvenIfNotNecessary)
            {
                return ESP_OK;
            }
            ESP_ERROR_CHECK(rmt_write_sample(rmt_channel, table, 3*LEDSIZE, true));
            dirty = false;
            return rmt_wait_tx_done(rmt_channel, pdMS_TO_TICKS(timeout_ms));
            
        }

        esp_err_t Clear(uint32_t timeout_ms = 1000)
        {
            CRGB black = CRGB::Black;
            memset(table, 0, 3*LEDSIZE);
            //memset(patterns, 0, LEDSIZE * sizeof(AnimationPattern *));
            dirty = true;
            return Refresh(timeout_ms);
        }

        esp_err_t Init(const rmt_channel_t rmt_channel, const gpio_num_t gpio)
        {
            this->rmt_channel=rmt_channel;
            rmt_config_t config = RMT_DEFAULT_CONFIG_TX(gpio, rmt_channel);
            // set counter clock to 40MHz
            config.clk_div = 2;

            ESP_ERROR_CHECK(rmt_config(&config));
            ESP_ERROR_CHECK(rmt_driver_install(config.channel, 0, 0));
            uint32_t hz = 0;
            ESP_ERROR_CHECK(rmt_get_counter_clock(rmt_channel, &hz));
            if(counter_clk_hz!=hz){
                ESP_LOGE(TAG, "Clock is not as expected");
                return ESP_FAIL;
            }
            rmt_translator_init(rmt_channel, ws2812_rmt_adapter);
            Clear(1000);
            return ESP_OK;
        }
    };

}

#undef TAG