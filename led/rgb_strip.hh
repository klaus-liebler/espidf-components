#pragma once
#include <common.hh>
#include <errorcodes.hh>
#include <esp_log.h>
#include <esp_heap_caps.h>
#include <driver/spi_master.h>
#include <driver/gpio.h>
#include <esp_timer.h>
#include "animatable_led.hh"
#include "auto_off.hh"
#include <array>
#define TAG "LED"

// Nachfolger von RGBLED::M (rgbled-Komponente). Steuert einen SPI-DMA-angesteuerten LED-Streifen
// mit LEDSIZE Pixeln an. Gegenueber dem Vorgaenger neu: ein AutoOffDeadline pro Pixel (vorher gar
// nicht vorhanden) plus dieselbe "gleiches Pattern -> Deadline nicht anfassen"-Idiom wie SingleLed
// (vorher fehlte auch das -- jeder AnimatePixel-Aufruf hat bedingungslos neu resettet).
namespace led
{
    enum class DeviceType
    {
        WS2812,
        PL9823,
        SK6805,
    };

    template <size_t LEDSIZE, DeviceType DEVICE>
    class RgbStrip : public IAnimatableLED
    {
    private:
        // 3.2MHz --> spi bit time = 312,5ns
        // 4 spi bits are used to transfer one WS2812 bit
        // data bit time = 1250ns
        // reset impulse = 50us = 40 data bit time (48 to be safe and have a "modulo 8==0" number to improve memory alignment)
        static constexpr size_t LED_DMA_BUFFER_SIZE = (LEDSIZE * 24 /*data bits per LED*/ + 48 /*reset pulse*/) * 4 /*spi bits per data bit*/ / 8 /*bits per byte*/;
        uint16_t *buffer{nullptr};
        uint32_t table[LEDSIZE];
        AnimationPattern *patterns[LEDSIZE]{};
        AutoOffDeadline autoOff[LEDSIZE];
        bool dirty{true};
        spi_device_handle_t spi_device_handle = NULL;

        static tms_t NowMs() { return esp_timer_get_time() / 1000; }

    public:
        RgbStrip() {}

        ErrorCode SetPixel(size_t index, CRGB color, bool refresh = false) override
        {
            if (index >= LEDSIZE)
                return ErrorCode::INDEX_OUT_OF_BOUNDS;
            patterns[index] = nullptr;
            if ((this->table[index]) != (color.raw32))
            {
                ESP_LOGD(TAG, "Set Index %u from %lu to %lu", index, table[index], color.raw32);
                this->table[index] = color.raw32;
                this->dirty = true;
            }
            if (refresh)
            {
                this->Refresh(1000, refresh);
            }
            return ErrorCode::OK;
        }

        ErrorCode AnimatePixel(size_t index, AnimationPattern *pattern, tms_t timeToAutoOff = 0) override
        {
            if (index >= LEDSIZE)
                return ErrorCode::INDEX_OUT_OF_BOUNDS; // TODO: RaceCondition: Set,Animate und Refresh müssen gegenseitig verriegelt werden...
            tms_t now = NowMs();
            if (pattern == patterns[index])
            {
                ESP_LOGD(TAG, "Pixel %u already animating with the same pattern", index);
                return ErrorCode::OK;
            }
            patterns[index] = pattern;
            autoOff[index].Arm(now, timeToAutoOff);
            if (pattern != nullptr)
            {
                pattern->Reset(now);
                table[index] = CRGB::TRANSPARENT;
            }
            return ErrorCode::OK;
        }

        ErrorCode Refresh(uint32_t timeout_ms = 1000, bool forceRefreshEvenIfNotNecessary = false) override
        {
            uint32_t i;
            int n = 0;
            tms_t now = NowMs();
            for (i = 0; i < LEDSIZE; i++)
            {
                if (patterns[i] != nullptr && autoOff[i].Expired(now))
                {
                    patterns[i] = nullptr;
                    ESP_LOGI(TAG, "Pixel %u switched off (auto-off)", i);
                    CRGB black = CRGB::Black;
                    if (this->table[i] != black.raw32)
                    {
                        this->table[i] = black.raw32;
                        this->dirty = true;
                    }
                    continue;
                }
                AnimationPattern *p = patterns[i];
                if (p == nullptr) continue;
                // avoid using SetPixel here, as this may have unintentional consequences
                CRGB color = p->Animate(now);
                if ((this->table[i]) == (color.raw32)) continue;
                this->table[i] = color.raw32;
                this->dirty = true;
            }

            if (!dirty && !forceRefreshEvenIfNotNecessary)
            {
                return ErrorCode::OK;
            }
            for (i = 0; i < LEDSIZE; i++)
            {
                uint32_t temp = table[i];
                if (DEVICE == DeviceType::WS2812)
                {
                    static const uint16_t LedBitPattern[16] = {
                        0x8888,
                        0x8C88,
                        0xC888,
                        0xCC88,
                        0x888C,
                        0x8C8C,
                        0xC88C,
                        0xCC8C,
                        0x88C8,
                        0x8CC8,
                        0xC8C8,
                        0xCCC8,
                        0x88CC,
                        0x8CCC,
                        0xC8CC,
                        0xCCCC};
                    // G
                    buffer[n++] = LedBitPattern[0x0f & (temp >> 12)];
                    buffer[n++] = LedBitPattern[0x0f & (temp) >> 8];
                    // R
                    buffer[n++] = LedBitPattern[0x0f & (temp >> 4)];
                    buffer[n++] = LedBitPattern[0x0f & (temp)];
                    // B
                    buffer[n++] = LedBitPattern[0x0f & (temp >> 20)];
                    buffer[n++] = LedBitPattern[0x0f & (temp) >> 16];
                }
                else if (DEVICE == DeviceType::PL9823)
                {
                    static const uint16_t LedBitPattern[16] = {
                        0x8888,
                        0x8E88,
                        0xE888,
                        0xEE88,
                        0x888E,
                        0x8E8E,
                        0xE88E,
                        0xEE8E,
                        0x88E8,
                        0x8EE8,
                        0xE8E8,
                        0xEEE8,
                        0x88EE,
                        0x8EEE,
                        0xE8EE,
                        0xEEEE};
                    // R
                    buffer[n++] = LedBitPattern[0x0f & (temp >> 4)];
                    buffer[n++] = LedBitPattern[0x0f & (temp)];
                    // G
                    buffer[n++] = LedBitPattern[0x0f & (temp >> 12)];
                    buffer[n++] = LedBitPattern[0x0f & (temp) >> 8];
                    // B
                    buffer[n++] = LedBitPattern[0x0f & (temp >> 20)];
                    buffer[n++] = LedBitPattern[0x0f & (temp) >> 16];
                }

                else if (DEVICE == DeviceType::SK6805)
                {
                    static const uint16_t LedBitPattern[16] = {
                        0x8888,
                        0x8E88,
                        0xE888,
                        0xEE88,
                        0x888E,
                        0x8E8E,
                        0xE88E,
                        0xEE8E,
                        0x88E8,
                        0x8EE8,
                        0xE8E8,
                        0xEEE8,
                        0x88EE,
                        0x8EEE,
                        0xE8EE,
                        0xEEEE};
                    // G
                    buffer[n++] = LedBitPattern[0x0f & (temp >> 12)];
                    buffer[n++] = LedBitPattern[0x0f & (temp) >> 8];
                    // R
                    buffer[n++] = LedBitPattern[0x0f & (temp >> 4)];
                    buffer[n++] = LedBitPattern[0x0f & (temp)];
                    // B
                    buffer[n++] = LedBitPattern[0x0f & (temp >> 20)];
                    buffer[n++] = LedBitPattern[0x0f & (temp) >> 16];
                }
            }

            spi_transaction_t t = {};
            t.length = LED_DMA_BUFFER_SIZE * 8; // length is in bits
            t.tx_buffer = buffer;

            ESP_LOGD(TAG, "Refreshing RGB-LED");
            ESP_ERROR_CHECK(spi_device_transmit(this->spi_device_handle, &t));
            dirty = false;
            return ErrorCode::OK;
        }

        ErrorCode Clear(uint32_t timeout_ms = 1000) override
        {
            CRGB black = CRGB::Black;
            memset(table, black.raw32, LEDSIZE * sizeof(uint32_t));
            memset(patterns, 0, LEDSIZE * sizeof(AnimationPattern *));
            dirty = true;
            return Refresh(timeout_ms);
        }

        size_t Size() const override { return LEDSIZE; }

        ErrorCode Begin(const spi_host_device_t spi_host, const gpio_num_t gpio, const spi_dma_chan_t dma_channel = SPI_DMA_CH_AUTO)
        {
            spi_bus_config_t bus_config = {};
            bus_config.miso_io_num = GPIO_NUM_NC;
            bus_config.mosi_io_num = gpio;
            bus_config.sclk_io_num = GPIO_NUM_NC;
            bus_config.quadwp_io_num = GPIO_NUM_NC;
            bus_config.quadhd_io_num = GPIO_NUM_NC;
            bus_config.max_transfer_sz = LED_DMA_BUFFER_SIZE;
            ESP_ERROR_CHECK(spi_bus_initialize(spi_host, &bus_config, dma_channel));
            spi_device_interface_config_t dev_config = {};
            if (DEVICE == DeviceType::WS2812)
                dev_config.clock_speed_hz = 3.2 * 1000 * 1000;
            else if (DEVICE == DeviceType::PL9823)
            {
                dev_config.clock_speed_hz = 2352941; // to reach a cycle time of 0.425us
            }
            else if (DEVICE == DeviceType::SK6805)
            {
                dev_config.clock_speed_hz = 3.2 * 1000 * 1000; // 3.2MHz -->800kHz bit timing and one "spi bit" is 312,5ns
            }
            dev_config.mode = 0,
            dev_config.spics_io_num = GPIO_NUM_NC;
            dev_config.queue_size = 1;
            dev_config.command_bits = 0;
            dev_config.address_bits = 0;
            ESP_ERROR_CHECK(spi_bus_add_device(spi_host, &dev_config, &this->spi_device_handle));

            this->buffer = static_cast<uint16_t *>(heap_caps_malloc(LED_DMA_BUFFER_SIZE, MALLOC_CAP_DMA)); // Critical to be DMA memory.
            if (this->buffer == NULL)
            {
                ESP_LOGE(TAG, "LED DMA Buffer can not be allocated");
                return ErrorCode::GENERIC_ERROR;
            }
            memset(this->buffer, 0, LED_DMA_BUFFER_SIZE);
            Clear(1000);
            return ErrorCode::OK;
        }
    };
}
#undef TAG
