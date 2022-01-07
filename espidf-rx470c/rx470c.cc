#include <stdio.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "rx470c.hh"

#define TAG "RX470"

extern "C"
{
    void rx470cReceiverTask(void *);
}

void rx470cReceiverTask(void *arg)
{
    RX470C *rx470c = (RX470C *)arg;

    ESP_LOGI(TAG, "rx470cReceiverTask started");
    RingbufHandle_t rb = NULL;
    rmt_get_ringbuf_handle(rx470c->rmtRxChannel, &rb);
    assert(rb != NULL);
    rmt_rx_start(rx470c->rmtRxChannel, true);
    size_t length = 0;
    while (true){
        rmt_item32_t *items = (rmt_item32_t *)xRingbufferReceive(rb, &length, portMAX_DELAY);
         
        if (!items)
            continue;
        length /= sizeof(rmt_item32_t);
        //ESP_LOGI(TAG, "rx470cReceiverTask received something with len=%d", length);
        uint32_t value = 0;
        if (rx470c->decoder->decode(items, length, &value))
        {
            //ESP_LOGI(TAG, "rx470cReceiverTask received %d, previousValue=%d, repCnt=%d", value, rx470c->previousValue, rx470c->repetitions);
            if (value == rx470c->previousValue)
            {
                int64_t nowUs=esp_timer_get_time();
                rx470c->repetitions++;
                
                if (rx470c->repetitions >= 1 && rx470c->lastReceivedUs + 2000000 < nowUs)
                {
                    ESP_LOGI(TAG, "rx470cReceiverTask received %d multiple times. StackHighWatermark in u32 is %d\n", value, uxTaskGetStackHighWaterMark(NULL));
                    if(rx470c->callback){
                        rx470c->callback->ReceivedFromRx470c(value);
                    }
                    rx470c->lastReceivedUs = nowUs;
                }
            }
            else
            {
                rx470c->repetitions = 0;
                rx470c->previousValue = value;
                rx470c->lastReceivedUs = 0;
            }
        }
        vRingbufferReturnItem(rb, (void *)items);
    }
}

RX470C::RX470C(const rmt_channel_t rmtRxChannel, RF_Decoder* decoder, Rx470cCallback* callback):rmtRxChannel(rmtRxChannel), decoder(decoder), callback(callback){}

esp_err_t RX470C::Init(gpio_num_t pin, uint8_t mem_block_num)
{
    rmt_config_t rmt_rx;
    rmt_rx.channel = rmtRxChannel;                                        // RMT channel
    rmt_rx.gpio_num = pin;                                                  // GPIO pin
    rmt_rx.clk_div = this->decoder->GetClkDiv();                            // Clock divider
    rmt_rx.mem_block_num = mem_block_num;                                   // number of mem blocks used
    rmt_rx.rmt_mode = RMT_MODE_RX;                                          // Receive mode
    rmt_rx.rx_config.filter_en = true;                                      // Enable filter
    rmt_rx.rx_config.filter_ticks_thresh = this->decoder->GetFilterTicks(); // Filter all shorter than 100 ticks
    rmt_rx.rx_config.idle_threshold = this->decoder->GetIdleThreshold();    // Timeout after 1300 ticks which is well above the maximum zero level of the parsed PT2262 signal
    esp_err_t err = rmt_config(&rmt_rx);
    if(err!=ESP_OK) return err;                                               // Init channel
    return rmt_driver_install(rmtRxChannel, 4000, 0);                            // Install driver with ring buffer size 1000
}

esp_err_t RX470C::Start()
{
    return xTaskCreate(rx470cReceiverTask, "rx470cReceiverTask", 4096 * 4, this, 12, NULL)==pdPASS?ESP_OK:ESP_FAIL;
}