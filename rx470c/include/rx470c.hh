#pragma once

#include <stdio.h>
#include <esp_err.h>
#include <esp_log.h>
#include <driver/gpio.h>

#include <driver/rmt_rx.h>
//#include <driver/periph_ctrl.h>
#include <soc/rmt_reg.h>
#include "sdkconfig.h"

class Rx470cCallback{
    public:
    virtual void ReceivedFromRx470c(uint32_t val)=0;
};

class RF_Decoder
{
public:
    virtual bool decode(rmt_rx_done_event_data_t *data, uint32_t *val) = 0;
    virtual uint32_t GetResolution() = 0;
    virtual uint8_t GetFilterTicks() = 0;
    virtual uint16_t GetIdleThreshold() = 0;
};

class PT2262Decoder : public RF_Decoder
{
    const uint32_t alpha = 70;
public:
    uint32_t GetResolution() override
    {
        return 1000000;
    }

    uint8_t GetFilterTicks() override
    {
        return 200;
    }

    uint16_t GetIdleThreshold() override
    {
        return 1300;
    }

    inline bool rf_valid(rmt_symbol_word_t *i)
    {
        return (i->level0 && !i->level1);
    }

    inline bool in_range(int duration_ticks, uint32_t min_us, uint32_t max_us)
    {
        return (duration_ticks < max_us) && (duration_ticks > min_us);
    }

    inline bool bit_zero_if(rmt_symbol_word_t *item, uint32_t alpha)
    {
        return (in_range(item->duration0, 240, 400) && in_range(item->duration1, 11 * alpha, 15 * alpha));
    }

    inline bool bit_one_if(rmt_symbol_word_t *item, uint32_t alpha)
    {
        return (in_range(item->duration0, 700, 1000) && in_range(item->duration1, 2 * alpha, 6 * alpha));
    }

    inline bool bit_sync_if(rmt_symbol_word_t *item, uint32_t alpha)
    {
        return in_range(item->duration0, 2 * alpha, 6 * alpha);
    }

    bool decode(rmt_rx_done_event_data_t *data, uint32_t *val) override
    {
        if (data->num_symbols != 25)
            return false;
        uint32_t buf = 0;
        // for (size_t i = 0; i < 4; i++) {
        //     alpha+= items[i].duration0+items[i].duration1;
        // }
        // alpha/=64;
        //Serial.printf("AlphaTime=%d\n", alpha);
        uint32_t min0{UINT16_MAX};
        uint32_t max0{0};
        uint32_t min1{UINT16_MAX};
        uint32_t max1{0};
        for (size_t i = 0; i < 24; i++)
        {
            if (!rf_valid(&data->received_symbols[i]))
            {
                ESP_LOGD("DEC", "bit %d wrong polarity\n", i);
                return false;
            }
            if (bit_one_if(&data->received_symbols[i], alpha))
            {
                // one
                buf |= (1 << (data->num_symbols - 2 - i)); // lsb <-> msb
                //ESP_LOGE("DEC", "Bit %d = 1\n", i);
                if(data->received_symbols[i].duration0>max1) max1=data->received_symbols[i].duration0;
                if(data->received_symbols[i].duration0<min1) min1=data->received_symbols[i].duration0;
            }
            else if (bit_zero_if(&data->received_symbols[i], alpha))
            {
                //ESP_LOGE("DEC", "Bit %d = 0\n", i);
                if(data->received_symbols[i].duration0>max0) max0=data->received_symbols[i].duration0;
                if(data->received_symbols[i].duration0<min0) min0=data->received_symbols[i].duration0;
                continue;
            }
            else
            {
                //ESP_LOGD("DEC", "Bit %lu has dur0=%lu and dur1=%lu. alpha was %lu -->INVALID!!!", i, items[i].duration0, items[i].duration1, alpha);
                return false; // pulse did not correspond to 0 or 1
            }
        }
        //Check sync impulse
        if (!bit_sync_if(&data->received_symbols[24], alpha))
        {
            //ESP_LOGD("DEC", "Sync Bit has dur0=%lu. alpha was %d -->INVALID!!!", items[24].duration0, alpha);
            return false;
        }
        *val = buf;
        ESP_LOGD("DEC", "Received %lu, min0=%lu, max0=%lu, min1=%lu, max1=%lu", buf, min0, max0, min1, max1);
        return true;
    }
};

class RX470C
{

public:
    rmt_channel_handle_t rmtRxChannel;
    RF_Decoder *decoder;
    Rx470cCallback* callback;
    uint32_t previousValue{0};
    uint32_t repetitions{0};
    int64_t lastReceivedUs{0};
    RX470C(RF_Decoder *decoder, Rx470cCallback* callback);
    esp_err_t Init(const gpio_num_t pin, const uint8_t mem_block_num);
    esp_err_t Start();

private:
    static void Task(void* args);
    static bool RxDoneCallback(rmt_channel_handle_t channel, const rmt_rx_done_event_data_t *edata, void *user_data);
    void Loop();
};