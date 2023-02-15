#include <stdint.h>
#include <vector>
#include <bitset>
#include <algorithm>
#include <inttypes.h>
#include <common.hh>
#include <string.h>
#include <esp_log.h>
#include <esp_timer.h>
#include "sdkconfig.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <freertos/queue.h>
#include <driver/gpio.h>
#include "modbuscommons.hh"
#define TAG "MODBUS"


namespace modbus
{

    class ModbusRequest
    {
        public:
        virtual ModbusError BuildRequest(uint8_t *buf, size_t &bufLen) = 0;
        
        virtual ModbusMessageParsingResult ProcessResponse(uint8_t *buf, size_t &bufLen) = 0;
        virtual ModbusError FindRecentChanges(uint8_t *buf, size_t &bufLen) = 0;
        uint8_t GetSlaveId(){return slaveId;}
        uint16_t GetStart(){return start;}
        protected:
        const uint8_t slaveId;
        const uint16_t start;
        ModbusRequest(uint8_t slaveId, uint16_t start):slaveId(slaveId), start(start){}
        ModbusError BuildBasicRequest(uint8_t *buf, size_t &bufLen, uint8_t slaveId, FunctionCodes functionCode, uint16_t start, uint16_t count){
            if(bufLen < 8) return ModbusError::BUFFER_TOO_SMALL;
            buf[0] = slaveId;
            buf[1] = (uint8_t)functionCode;
            WriteUInt16BigEndian(start, buf, 2);
            WriteUInt16BigEndian(count, buf, 4);
            WriteCRC(buf, 6);
            bufLen=8;
            return ModbusError::OK;
        }

        ModbusMessageParsingResult DoBasicChecks(uint8_t *buf, size_t &bufLen, uint8_t slaveId, FunctionCodes functionCode, uint8_t expectedByteCount){
            if(buf[0]!=slaveId){
                ESP_LOGE(TAG, "ModbusMessageParsingResult::WRONG_SLAVE_ID_IN_RESPONSE is=%d should=%d", buf[0], slaveId);
                return ModbusMessageParsingResult::WRONG_SLAVE_ID_IN_RESPONSE;
            }
            if((buf[1] & 0x7F) != (uint8_t)functionCode){
                ESP_LOGE(TAG, "ModbusMessageParsingResult::WRONG_FUNCTION_CODE_IN_RESPONSE is=%d should=%d", buf[1], (uint8_t)functionCode);
                return ModbusMessageParsingResult::WRONG_FUNCTION_CODE_IN_RESPONSE;
            }
            if(buf[1] & 0x80){
                ESP_LOGE(TAG, "ModbusMessageParsingResult::SLAVE_REPORTED_PROCESSING_ERROR");
                return ModbusMessageParsingResult::SLAVE_REPORTED_PROCESSING_ERROR;
            }
            if(buf[2] != expectedByteCount){
                ESP_LOGE(TAG, "ModbusMessageParsingResult::WRONG_PAYLOAD_SIZE is=%d should=%d", buf[2], expectedByteCount);
                return ModbusMessageParsingResult::WRONG_PAYLOAD_SIZE;
            }
            if(bufLen!=5+buf[2]){
                ESP_LOGE(TAG, "ModbusMessageParsingResult::WRONG_BUFFER_SIZE is=%d should=%d", bufLen, 5+buf[2]);
                return ModbusMessageParsingResult::WRONG_BUFFER_SIZE;
            }
            if(!validCRCInLastTwoBytes(buf, bufLen)){
                ESP_LOGE(TAG, "ModbusMessageParsingResult::CRC_ERROR");
                return ModbusMessageParsingResult::CRC_ERROR;
            }
            return ModbusMessageParsingResult::OK;
        }
    };

    


    class ReadCoilsOrder : public ModbusRequest
    {
    private:
        const uint16_t count;
        std::vector<uint8_t> data;
        bool changed=false;

    public:
        ReadCoilsOrder(const uint8_t slaveId,  const uint16_t start, const uint16_t count):ModbusRequest(slaveId, start), count(count)
        {
            this->data.resize((count+7)/8);
        }

        ModbusError BuildRequest(uint8_t *buf, size_t& bufLen) override
        {
            return BuildBasicRequest(buf, bufLen, slaveId, FunctionCodes::READ_COILS, start,count);
        }

        ModbusError FindRecentChanges(uint8_t *buf, size_t &bufLen) override{
            if(!changed){
                return ModbusError::NO_CHANGE;
            }
            for(int i=0;i<(count+7)/8;i++ ){  
                ESP_LOGI(TAG, "Changed byte %d to 0x%02x", i, data[i]);   
            }
            changed=false;
            return ModbusError::OK;
        }
        
        ModbusMessageParsingResult ProcessResponse(uint8_t *buf, size_t& bufLen) override{
            ModbusMessageParsingResult err;
            if((err=DoBasicChecks(buf, bufLen, slaveId, FunctionCodes::READ_INPUT_REGISTERS, (count+7)/8))!=ModbusMessageParsingResult::OK){
                return err;
            }
            for(int i=0;i<buf[2];i++){
                uint8_t val = buf[3+i];
                data[i]= val;
            }
            return ModbusMessageParsingResult::OK;
        }
    };

    

    class ReadInputRegistersRequest : public ModbusRequest
    {
    private:

        const uint16_t count;
        std::vector<uint16_t> data;
        bool changed;
    public:
        

        ReadInputRegistersRequest(const uint8_t slaveId,  const uint16_t start, const uint16_t count):ModbusRequest(slaveId, start), count(count)
        {
            this->data.resize(count);
        }

        ModbusError BuildRequest(uint8_t *buf, size_t& bufLen) override
        {
            return BuildBasicRequest(buf, bufLen, slaveId, FunctionCodes::READ_INPUT_REGISTERS, start, count);
        }

        ModbusError FindRecentChanges(uint8_t *buf, size_t& bufLen) override{

            changed=false;
            return ModbusError::OK;
        }
        
        ModbusMessageParsingResult ProcessResponse(uint8_t *buf, size_t& bufLen) override{
            
            ModbusMessageParsingResult err;
            if((err = DoBasicChecks(buf, bufLen, slaveId, FunctionCodes::READ_INPUT_REGISTERS, 2*count))!=ModbusMessageParsingResult::OK){
                return err;
            }
            for(int i=0;i<buf[2]/2;i++){
                int16_t val = ParseInt16BigEndian(buf, 2+2*i);
                if(data[i]!=val){
                    changed=true;
                    data[i]=val;
                }
            }
            return ModbusMessageParsingResult::OK;
        }
    };

    constexpr int MAX_RETRIES{3};
    constexpr size_t BUF_SIZE{64};
    constexpr time_t TIMEOUT_FOR_RESPONSE_AFTER_REQUEST_US{100000};
    constexpr time_t MIN_TIME_BETWEEN_TWO_REQUESTS_US{50000};


    class ModbusRTUMaster
    {
    private:
        uart_port_t port;
        QueueHandle_t uart_queue;
        SemaphoreHandle_t dataMutex;
        std::vector<modbus::ModbusRequest*> *requests;
        enum class EventTaskState{
            INIT,
            WAIT_FOR_RESPONSE,
            WAIT_FOR_PAUSE,
        };
        
        size_t lastRequestIndex=0;
        int64_t nextTimeoutUs=0;
        int retriesAvailable=MAX_RETRIES; //smart trick to trigger sending the very first request
        EventTaskState state = EventTaskState::INIT;

        void sendRequestAndUpdateStateAndTimeoutAndRetries(int64_t nowUs){
            uint8_t dtmp[BUF_SIZE];
            nextTimeoutUs=nowUs+TIMEOUT_FOR_RESPONSE_AFTER_REQUEST_US;
            retriesAvailable--;
            state=EventTaskState::WAIT_FOR_RESPONSE;
            ModbusRequest* r = requests->at(lastRequestIndex);
            size_t size=sizeof(dtmp);
            r->BuildRequest(dtmp, size);
            uart_write_bytes(port, dtmp, size);
            
        }

        void uart_event_task(){
            
            uart_event_t event;
            
            while (true)
            {
                // Waiting for UART event.
                int64_t nowUs = esp_timer_get_time();
                int64_t timeoutUs = std::max(1000LL, nextTimeoutUs-nowUs);
                TickType_t ticks = pdMS_TO_TICKS(timeoutUs/1000)+2;
                if (!xQueueReceive(uart_queue, (void *)&event, ticks))
                {
                    int64_t newNowUs = esp_timer_get_time();
                    if(newNowUs<nextTimeoutUs){
                        ESP_LOGW(TAG, "Timeout was not long enough previousNow=%lu, nextTimeoutUs=%lu, timeoutUs=%lu, ticks=%lu, newNow=%lu", (uint32_t)nowUs, (uint32_t)nextTimeoutUs, (uint32_t)timeoutUs, (uint32_t)ticks, (uint32_t)newNowUs);
                        continue;
                    }
                    nowUs=newNowUs;
                    switch (state)
                    {
                    case EventTaskState::INIT:
                    {
                        ESP_LOGI(TAG, "Hooray, I, the best ModbusMaster all over the world, have been successfully initialized. I am happy to send the very first request now.");
                        sendRequestAndUpdateStateAndTimeoutAndRetries(nowUs);
                        break;
                    }
                    case EventTaskState::WAIT_FOR_PAUSE:
                    { //die Pause zwischen zwei Request-Response-Spielchen ist zu Ende -- gehe zum nächsten Request
                        lastRequestIndex=(lastRequestIndex+1)%requests->size();
                        retriesAvailable=MAX_RETRIES;
                        sendRequestAndUpdateStateAndTimeoutAndRetries(nowUs);
                        ESP_LOGI(TAG, "Sending next request");
                        break;
                    }
                    case EventTaskState::WAIT_FOR_RESPONSE:{
                        if(retriesAvailable>0){
                            ESP_LOGW(TAG, "Slave did not respond, but there are still %d retries left", retriesAvailable);
                            sendRequestAndUpdateStateAndTimeoutAndRetries(nowUs);
                        }
                        else{
                            ESP_LOGE(TAG, "Slave did not respond. There are no retries left");
                            lastRequestIndex=(lastRequestIndex+1)%requests->size();
                            retriesAvailable=MAX_RETRIES;
                            sendRequestAndUpdateStateAndTimeoutAndRetries(nowUs);
                        }
                        break;
                    }
                    }
                }
                else if (event.type == UART_DATA && event.timeout_flag)
                {

                    uint8_t dtmp[BUF_SIZE];
                    uart_read_bytes(port, dtmp, event.size, portMAX_DELAY);
                    if(state!=EventTaskState::WAIT_FOR_RESPONSE){
                        ESP_LOGE(TAG, "We detected an incoming message while we are not waiting for such message! Message is:");
                        ESP_LOG_BUFFER_HEX_LEVEL(TAG, dtmp, event.size, ESP_LOG_ERROR);
                    }
                    xSemaphoreTake(dataMutex, portMAX_DELAY);
                    ModbusMessageParsingResult err = this->requests->at(lastRequestIndex)->ProcessResponse(dtmp, event.size);
                    xSemaphoreGive(dataMutex);
                    switch (err)
                    {
                    case ModbusMessageParsingResult::OK:
                        state=EventTaskState::WAIT_FOR_PAUSE;
                        this->nextTimeoutUs=nowUs+MIN_TIME_BETWEEN_TWO_REQUESTS_US;
                        ESP_LOGI(TAG, "Received a full answer of size %d. Consumed it successfully!. Set timeout", event.size);
                        break;
                    default:
                        ESP_LOGE(TAG, "Received a full answer of size %d. Error %d while processing. Message is", event.size, (int)err);
                        ESP_LOG_BUFFER_HEX_LEVEL(TAG, dtmp, event.size, ESP_LOG_ERROR);
                        //am state nichts ändern - eine korrekte Antwort ist ja (noch!) nicht gekommen
                        break;
                    }
                    
                }
                else{
                    switch (event.type)
                    {
                    // Event of UART receving data
                    case UART_DATA:
                        ESP_LOGI(TAG, "[UART DATA]: %d", event.size);
                        uart_flush_input(port);
                        xQueueReset(uart_queue);
                        break;
                    // Event of HW FIFO overflow detected
                    case UART_FIFO_OVF:
                        ESP_LOGI(TAG, "hw fifo overflow");
                        // If fifo overflow happened, you should consider adding flow control for your application.
                        // The ISR has already reset the rx FIFO,
                        // As an example, we directly flush the rx buffer here in order to read more data.
                        uart_flush_input(port);
                        xQueueReset(uart_queue);
                        break;
                    // Event of UART ring buffer full
                    case UART_BUFFER_FULL:
                        ESP_LOGI(TAG, "ring buffer full");
                        // If buffer full happened, you should consider encreasing your buffer size
                        // As an example, we directly flush the rx buffer here in order to read more data.
                        uart_flush_input(port);
                        xQueueReset(uart_queue);
                        break;
                    // Event of UART RX break detected
                    case UART_BREAK:
                        //UART_BREAK is an expected event
                        ESP_LOGD(TAG, "uart rx break");
                        break;
                    // Event of UART parity check error
                    case UART_PARITY_ERR:
                        ESP_LOGI(TAG, "uart parity error");
                        break;
                    // Event of UART frame error
                    case UART_FRAME_ERR:
                        ESP_LOGI(TAG, "uart frame error");
                        break;

                    // Others
                    default:
                        ESP_LOGE(TAG, "unknown uart event type: %d", event.type);
                        break;
                    }
                }
            }
        }

    public:
        void RequestInputRegister(uint8_t slaveId, uint16_t startIndex, uint16_t count)
        {
            uint8_t buffer[8];
            buffer[0] = slaveId;
            buffer[1] = (uint8_t)FunctionCodes::READ_INPUT_REGISTERS;
            WriteUInt16BigEndian(startIndex, buffer, 2);
            WriteUInt16BigEndian(count, buffer, 4);
            uint16_t crc = calcCRC(buffer, 6);
            WriteUInt16(crc, buffer, 6);
            ESP_LOGI(TAG, "crc=0x%04x", crc);
            ESP_LOG_BUFFER_HEX_LEVEL(TAG, buffer, 8, ESP_LOG_INFO);
            uart_write_bytes(port, buffer, 8);
        }


        static void uart_event_task_static(void *pvParameters)
        {
            ModbusRTUMaster *myself = (ModbusRTUMaster *)pvParameters;
            myself->uart_event_task();
        }

        void Init(uart_port_t port, gpio_num_t tx_io_num, gpio_num_t rx_io_num, gpio_num_t driver_enable, int baud_rate, uart_parity_t parity, std::vector<modbus::ModbusRequest*> *requests)
        {
            this->port = port;
            this->requests=requests;
            // Install UART driver, and get the queue.
            ESP_ERROR_CHECK(uart_driver_install(port, 256, 256, 20, &uart_queue, ESP_INTR_FLAG_IRAM));

            uart_config_t xUartConfig = {};
            xUartConfig.baud_rate = baud_rate;
            xUartConfig.data_bits = UART_DATA_8_BITS;
            xUartConfig.parity = parity;
            xUartConfig.stop_bits = UART_STOP_BITS_1;
            xUartConfig.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
            xUartConfig.rx_flow_ctrl_thresh = 2;
            xUartConfig.source_clk = UART_SCLK_APB;

            ESP_ERROR_CHECK(uart_param_config(port, &xUartConfig));
            ESP_ERROR_CHECK(uart_set_pin(port, tx_io_num, rx_io_num, driver_enable, GPIO_NUM_NC));
            ESP_ERROR_CHECK(uart_set_mode(port, UART_MODE_RS485_HALF_DUPLEX));
            // Set timeout for TOUT interrupt (T3.5 modbus time)
            ESP_ERROR_CHECK(uart_set_rx_timeout(port, 3));
            // Set always timeout flag to trigger timeout interrupt even after rx fifo full
            uart_set_always_rx_timeout(port, true);
            // Create a task to handle UART events
            TaskHandle_t xMbTaskHandle;
            dataMutex = xSemaphoreCreateMutex();
            BaseType_t xStatus = xTaskCreate(ModbusRTUMaster::uart_event_task_static, "uart_queue_task", 4096, this, 10, &xMbTaskHandle);
            if (xStatus != pdPASS)
            {
                vTaskDelete(xMbTaskHandle);
                // Force exit from function with failure
                ESP_LOGE(TAG, "stack serial task creation error. xTaskCreate() returned (0x%x).", xStatus);
            }
        }
    };
}
#undef TAG