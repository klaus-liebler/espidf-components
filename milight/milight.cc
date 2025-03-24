#include <stdio.h>
#include "sdkconfig.h"
#include <ctime>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include <esp_timer.h>
#include <milight.hh>

#define TAG "MILIGHT"
namespace milight
{
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
    
    class MilightDecoder
    {
    public:
        const uint8_t rev[16]{0, 8, 4, 0xC, 2, 0xA, 6, 0xE, 1, 9, 5, 0xD, 3, 0xB, 7, 0xF};
        uint8_t V2_OFFSET(uint8_t byte, uint8_t key, uint8_t jumpStart)
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

        int transformPacket(uint8_t *tmp)
        {
            static const uint8_t LAST_SYNC_BYTE = 0x18; // for 9-Byte payload
            if (tmp[0] != LAST_SYNC_BYTE)
                return -1;
            if ((tmp[1] & 0xF0) != 0xA0)
                return -2;
            if ((tmp[1] & 0xF0) != 0xA0)
                return -3;
            int len = tmp[0] = getByte(tmp, 0);   // get payload length
            for (int i = 1; i < 1 + len + 2; i++) // length item + playload length + crc
            {
                tmp[i] = getByte(tmp, i);
            }
            uint16_t crcShouldbe = calc_crc(tmp, len + 1);
            uint16_t crcIs = (tmp[len + 2] << 8) | tmp[len + 1];

            if (crcShouldbe != crcIs)
            {
                // ESP_LOGE(TAG, "CRC ERROR crcShouldbe=0x%04X, crcIs=0x%04X", crcShouldbe, crcIs);
                return -4;
            }
            return 0;
        }

        uint32_t GetPacketId(uint8_t *packet)
        {
            return (((packet[1] & 0xF0) << 24) | (packet[2] << 16) | (packet[3] << 8) | (packet[7]));
        }
    };

    

    void Milight::task()
    {
        MilightDecoder dec{};
        uint8_t buf[20] __attribute__((aligned(4)));
        uint32_t previousID = 0;
        uint8_t previousCmd{0};
        uint8_t previousArg{0};

        ESP_LOGI(TAG, "milightReceiverTask started");
        TickType_t xLastWakeTime;
        const TickType_t xFrequency = pdMS_TO_TICKS(40);
        time_t lastForwarded_us{0};
        while (true)
        {
            vTaskDelayUntil( &xLastWakeTime, xFrequency );
            if(!recv->IsIrqAsserted())
                continue;
            if (!recv->IsDataReady())
                continue;
            recv->GetRxData(buf);
            uint8_t *packet = buf + 1; // to ignore the very first byte, which is the satus byte (clocked out first). packet points to the packetLen
            // ESP_LOG_BUFFER_HEXDUMP("RAW", packet, PAYLOAD_SIZE, ESP_LOG_INFO);
            int err = dec.transformPacket(packet);
            if (err < 0)
                continue;
            // ESP_LOG_BUFFER_HEXDUMP("TRANS", packet, 9, ESP_LOG_INFO);
            uint32_t id = dec.GetPacketId(packet);
            packet = buf + 2; // packet points to the payload
            uint8_t key = dec.xorKey(packet[0]);
            for (size_t i = 1; i <= 7; i++)
            {
                packet[i] = dec.decodeByte(packet[i], 0, key, dec.V2_OFFSET(i, packet[0], V2_OFFSET_JUMP_START));
            }
            if (id == previousID)
                continue;
            previousID = id;
            // ESP_LOG_BUFFER_HEXDUMP("DECRYP", packet, 9, ESP_LOG_INFO);
            uint8_t cmd = packet[4];
            uint8_t arg = packet[5];
            if(arg==previousArg && cmd==previousCmd && lastForwarded_us+500'000<esp_timer_get_time())
                continue;
            previousArg=arg;
            previousCmd=cmd;
            ESP_LOGI(TAG, "CMD %0d ARG/SEQ %3d", cmd, arg);
            lastForwarded_us=esp_timer_get_time();
            callback->ReceivedFromMilight(cmd, arg);
        }
        ESP_LOGI(TAG, "Manager::Task stopped");
        vTaskDelete(nullptr);
    }

    Milight::Milight(Nrf24Receiver *recv, iMilightCallback *callback) :recv(recv), callback(callback) {}
    
    esp_err_t Milight::SetupAndRun()
    {
        const uint8_t addressLength{5};
        const uint8_t address[addressLength]{0x90, 0x4e, 0x6c, 0x55, 0x55}; // for 9-Byte payload
       
        recv->Config(10, 14, address, addressLength, 0, Rf24Datarate::RF24_1MBPS, Rf24PowerAmp::RF24_PA_HIGH);
        return xTaskCreate([](void *p){((Milight*)p)->task(); }, "milightReceiverTask", 4096*4, this, 12, nullptr) == pdPASS ? ESP_OK : ESP_FAIL;

    }
}