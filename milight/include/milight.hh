#pragma once

#include <stdio.h>
#include <esp_err.h>
#include <esp_log.h>
#include <driver/gpio.h>
#include <driver/spi_master.h>
#include <nrf24.hh>
#include "sdkconfig.h"

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>

//see https://arduino-projects4u.com/milight-new-protocol/

#define TAG "MILIGHT"

namespace milight
{
    enum class eTransformationError:uint8_t{
        OK = 0x00,
        FIRST_SYNC_BYTE_ERROR = 0x01,
        SECOND_BYTE_ERROR,
        UNEXPECTED_PAYLOAD_LENGTH,
        
        CRC_ERROR,
    };

    constexpr uint8_t V2_OFFSETS[8][4]{
        {0x45, 0x1F, 0x14, 0x5C},
        {0x2B, 0xC9, 0xE3, 0x11},
        {0xEE, 0xDE, 0x0B, 0xAA},
        {0xAF, 0x03, 0x1D, 0xF3},
        {0x1A, 0xE2, 0xF0, 0xD1},
        {0x04, 0xD8, 0x71, 0x42},
        {0xAF, 0x04, 0xDD, 0x07},
        {0xE1, 0x93, 0xB8, 0xE4}};
    
    constexpr uint8_t V2_OFFSET_JUMP_START = 0x54;
    
    //Mapping of nibbles to their reverse order
    constexpr const uint8_t rev[16]{0, 8, 4, 0xC, 2, 0xA, 6, 0xE, 1, 9, 5, 0xD, 3, 0xB, 7, 0xF};

    constexpr uint8_t NRF24_ADDRESS_LENGTH{5};
    //the following two constants are protocol specific

    constexpr uint8_t NRF24_ADDRESS_FILTER_7BYTE_PAYLOAD[NRF24_ADDRESS_LENGTH]        {0xD1, 0x28, 0x5E, 0x55, 0x55};
    constexpr uint8_t NRF24_ADDRESS_FILTER_9BYTE_PAYLOAD[NRF24_ADDRESS_LENGTH]        {0x90, 0x4e, 0x6c, 0x55, 0x55};


    constexpr size_t GROSS_PAYLOAD_SIZE_7BYTE_PAYLOAD{7+5};//without the first byte, which is the status byte of nrf24. The first byte of the payload is the length of the payload, so we need to add 1 to it.
    constexpr size_t GROSS_PAYLOAD_SIZE_9BYTE_PAYLOAD{9+5};//without the first byte, which is the status byte of nrf24. The first byte of the payload is the length of the payload, so we need to add 1 to it.
    
    constexpr uint8_t LAST_SYNC_BYTE_9BYTE_PAYLOAD{0x18}; 
    constexpr uint8_t LAST_SYNC_BYTE_7BYTE_PAYLOAD{0xA4}; 

    class iMilightCallback
    {
    public:
        virtual void ReceivedFromMilight(uint8_t cmd, uint8_t arg) = 0;
    };

    class Milight
    {
    protected:
        Nrf24Receiver *recv;
        iMilightCallback *callback;

        uint8_t calcOffset(uint8_t byte, uint8_t key, uint8_t jumpStart)
        {
            return V2_OFFSETS[byte - 1][key % 4] + ((jumpStart > 0 && key >= jumpStart && key <= jumpStart + 0x80) ? 0x80 : 0);
        }

        uint8_t xorKey(uint8_t key)
        {
            // Generate most significant nibble
            const uint8_t shift = (key & 0x0F) < 0x04 ? 0 : 1;
            const uint8_t x = (((key & 0xF0) >> 4) + shift + 6) % 8;
            const uint8_t msn = (((4 + x) ^ 1) & 0x0F) << 4;
            // Generate least significant nibble
            const uint8_t lsn = ((((key & 0x0F) + 4) ^ 2) & 0x0F);
            return (msn | lsn);
        }
        
        uint8_t decodeByte(uint8_t byte, uint8_t s1, uint8_t xorKey, uint8_t s2)
        {
            uint8_t value = byte - s2;
            value = value ^ xorKey;
            value = value - s1;
            return value;
        }

        uint16_t calc_crc(uint8_t *data, size_t data_length)
        {
            static const uint16_t CRC_POLY = 0x8408;
            uint16_t state = 0;
            for (size_t i = 0; i < data_length; i++)
            {
                uint8_t byte = data[i];
                for (int j = 0; j < 8; j++)
                {
                    if ((byte ^ state) & 0x01)
                    {
                        state = (state >> 1) ^ CRC_POLY;
                    }
                    else
                    {
                        state = state >> 1;
                    }
                    byte = byte >> 1;
                }
            }
            return state;
        }

        uint8_t getByte(uint8_t *src, size_t i)
        { // i=0 meint:length
            uint8_t highNibble = rev[src[i + 2] >> 4];
            uint8_t lowNibble = rev[src[i + 1] & 0x0F];
            return (highNibble << 4) | (lowNibble & 0x0F);
        }

        virtual eTransformationError transformPacket(uint8_t *tmp)=0;


        uint32_t GetPacketId(uint8_t *packet)
        {
            return (((packet[1] & 0xF0) << 24) | (packet[2] << 16) | (packet[3] << 8) | (packet[7]));
        }
        
    public:
        Milight(Nrf24Receiver *recv, iMilightCallback *callback): recv(recv), callback(callback) {}
        virtual esp_err_t SetupAndRun()=0;
    };

    class Milight9BytePayload : public Milight
    {
        


    public:
        Milight9BytePayload(Nrf24Receiver *recv, iMilightCallback *callback) : Milight(recv, callback) {}

        esp_err_t SetupAndRun() override
        {
            recv->Config(10, GROSS_PAYLOAD_SIZE_9BYTE_PAYLOAD, NRF24_ADDRESS_FILTER_9BYTE_PAYLOAD, NRF24_ADDRESS_LENGTH, 0, Rf24Datarate::RF24_1MBPS, Rf24PowerAmp::RF24_PA_HIGH);
            return xTaskCreate([](void *p){((Milight9BytePayload*)p)->task(); }, "milightReceiverTask", 4096*4, this, 12, nullptr) == pdPASS ? ESP_OK : ESP_FAIL;
        }

        void task(){
            
            uint8_t buf[20] __attribute__((aligned(4)))={0};//20 is sufficient for all payloads
            uint32_t previousID = 0;
    
    
            ESP_LOGI(TAG, "milightReceiverTask started");
            TickType_t xLastWakeTime;
            const TickType_t xFrequency = pdMS_TO_TICKS(40);
    
            while (true)
            {
                vTaskDelayUntil( &xLastWakeTime, xFrequency );
                if(!recv->IsIrqAsserted())
                    continue;
                while(recv->IsDataReady()){
                    recv->GetRxData(buf);
                    
                    uint8_t *packet = buf + 1; 
                    // to ignore the very first byte, which is the status byte of nrf24 and which should have a value of 0x42 (not checked...). 
                    // "packet" now points to the package lentgh
                    
                    auto err = transformPacket(packet);
                    ESP_LOG_BUFFER_HEXDUMP("TRANS", packet, 16, ESP_LOG_DEBUG);
                    if (err != eTransformationError::OK){
                        ESP_LOGE(TAG, "eTransformationError %d", (int)err);
                        continue;
                    }
                    
                    uint32_t id = GetPacketId(packet);
                    if (id == previousID)
                        continue;
                    previousID = id;
                    uint8_t* decodeArea = buf + 2; // packet points to the payload (jump over length)
                    
                    
                    uint8_t key = xorKey(decodeArea[0]);
                    for (size_t i = 1; i <= 7; i++)
                    {
                        decodeArea[i] = decodeByte(decodeArea[i], 0, key, calcOffset(i, decodeArea[0], V2_OFFSET_JUMP_START));
                    }
                    
                    ESP_LOG_BUFFER_HEXDUMP("DECOD", decodeArea, 16, ESP_LOG_DEBUG);
                    uint8_t cmd = packet[5];
                    uint8_t arg = packet[6];
                    callback->ReceivedFromMilight(cmd, arg);
                }
            }
            ESP_LOGI(TAG, "Manager::Task stopped");
            vTaskDelete(nullptr);
        }
 
        
        eTransformationError transformPacket(uint8_t *tmp) override
        {
            
            if (tmp[0] != LAST_SYNC_BYTE_9BYTE_PAYLOAD)
                return eTransformationError::FIRST_SYNC_BYTE_ERROR;
            
            int len = tmp[0] = getByte(tmp, 0);   // get payload length
            if(len!=9){
                ESP_LOGE(TAG, "Unexpected payload length %d (expected: 9)", len);
                return eTransformationError::UNEXPECTED_PAYLOAD_LENGTH;
            }  
            for (int i = 1; i < 1 + len + 2; i++) // length item + playload length + crc
            {
                tmp[i] = getByte(tmp, i);
            }
            uint16_t crcShouldbe = calc_crc(tmp, len + 1);
            uint16_t crcIs = (tmp[len + 2] << 8) | tmp[len + 1];

            if (crcShouldbe != crcIs)
            {
                ESP_LOGW(TAG, "CRC ERROR crcShouldbe=0x%04X, crcIs=0x%04X", crcShouldbe, crcIs);
                return eTransformationError::CRC_ERROR;
            }
            return eTransformationError::OK;
        }
    };
}
#undef TAG