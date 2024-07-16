#include <stdio.h>
#include <sdkconfig.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <esp_timer.h>
#include <esp_log.h>
#include "rx470c.hh"

#define TAG "RX470"

void RX470C::Task(void *arg)
{
    RX470C *rx470c = (RX470C *)arg;
    rx470c->Loop();
}

bool RX470C::RxDoneCallback(rmt_channel_handle_t channel, const rmt_rx_done_event_data_t *edata, void *user_data){
    BaseType_t high_task_wakeup = pdFALSE;
    QueueHandle_t receive_queue = static_cast<QueueHandle_t>(user_data);
    // send the received RMT symbols to the parser task
    xQueueSendFromISR(receive_queue, edata, &high_task_wakeup);
    return high_task_wakeup == pdTRUE;
}
void RX470C::Loop(){

    ESP_LOGI(TAG, "rx470cReceiverTask started");

    QueueHandle_t receive_queue = xQueueCreate(1, sizeof(rmt_rx_done_event_data_t));
    assert(receive_queue);
    rmt_rx_event_callbacks_t cbs = {};
    cbs.on_recv_done = RX470C::RxDoneCallback;

    ESP_ERROR_CHECK(rmt_rx_register_event_callbacks(this->rmtRxChannel, &cbs, receive_queue));

    // the following timing requirement is based on NEC protocol
    rmt_receive_config_t receive_config = {};
    receive_config.signal_range_min_ns = this->decoder->GetFilterTicks();   
    receive_config.signal_range_max_ns = this->decoder->GetIdleThreshold();
    ESP_ERROR_CHECK(rmt_enable(this->rmtRxChannel));

    rmt_symbol_word_t raw_symbols[64]; // 64 symbols should be sufficient for a standard NEC frame
    rmt_rx_done_event_data_t rx_data;
    // ready to receive
    ESP_ERROR_CHECK(rmt_receive(this->rmtRxChannel, raw_symbols, sizeof(raw_symbols), &receive_config));
    
    while (true){
        xQueueReceive(receive_queue, &rx_data, portMAX_DELAY);
        ESP_LOGI(TAG, "rx470cReceiverTask received something with len=%d", rx_data.num_symbols);
        uint32_t value = 0;
        if (this->decoder->decode(&rx_data, &value))
        {
            ESP_LOGI(TAG, "rx470cReceiverTask received %lu, previousValue=%lu, repCnt=%lu", value, this->previousValue, this->repetitions);
            if (value == this->previousValue)
            {
                int64_t nowUs=esp_timer_get_time();
                this->repetitions++;
                
                if (this->repetitions >= 1 && this->lastReceivedUs + 2000000 < nowUs)
                {
                    ESP_LOGI(TAG, "rx470cReceiverTask received %lu multiple times. StackHighWatermark in u32 is %i\n", value, uxTaskGetStackHighWaterMark(NULL));
                    if(this->callback){
                        this->callback->ReceivedFromRx470c(value);
                    }
                    this->lastReceivedUs = nowUs;
                }
            }
            else
            {
                this->repetitions = 0;
                this->previousValue = value;
                this->lastReceivedUs = 0;
            }
        }
        ESP_ERROR_CHECK(rmt_receive(this->rmtRxChannel, raw_symbols, sizeof(raw_symbols), &receive_config));
    }
}

RX470C::RX470C(RF_Decoder* decoder, Rx470cCallback* callback):decoder(decoder), callback(callback){}

esp_err_t RX470C::Init(gpio_num_t pin, uint8_t mem_block_num)
{
    rmt_rx_channel_config_t rx_chan_config = {};

    rx_chan_config.clk_src = RMT_CLK_SRC_DEFAULT;       // select source clock
    rx_chan_config.resolution_hz = this->decoder->GetResolution(); // 1MHz tick resolution, i.e. 1 tick = 1us
    rx_chan_config.mem_block_symbols = mem_block_num;          // memory block size, 64 * 4 = 256Bytes
    rx_chan_config.gpio_num = pin;                    // GPIO number
    rx_chan_config.flags.invert_in = false;         // don't invert input signal
    rx_chan_config.flags.with_dma = false;          // don't need DMA backend
    return rmt_new_rx_channel(&rx_chan_config, &this->rmtRxChannel);
}

esp_err_t RX470C::Start()
{
    return xTaskCreate(RX470C::Task, "RX470C::Task", 4096 * 4, this, 12, nullptr)==pdPASS?ESP_OK:ESP_FAIL;
}