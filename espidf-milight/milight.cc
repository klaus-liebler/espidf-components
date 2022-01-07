#include <stdio.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "milight.hh"

#define TAG "MILIGHT"
class MilightDecoder
{
public:
    const uint8_t rev[16]{0, 8, 4, 0xC, 2, 0xA, 6, 0xE, 1, 9, 5, 0xD, 3, 0xB, 7, 0xF};
    uint8_t V2_OFFSET(uint8_t byte, uint8_t key, uint8_t jumpStart)
    {
        static const uint8_t V2_OFFSETS[][4]{
            {0x45, 0x1F, 0x14, 0x5C},
            {0x2B, 0xC9, 0xE3, 0x11},
            {0xEE, 0xDE, 0x0B, 0xAA},
            {0xAF, 0x03, 0x1D, 0xF3},
            {0x1A, 0xE2, 0xF0, 0xD1},
            {0x04, 0xD8, 0x71, 0x42},
            {0xAF, 0x04, 0xDD, 0x07},
            {0xE1, 0x93, 0xB8, 0xE4}};
        return V2_OFFSETS[byte - 1][key % 4] + ((jumpStart > 0 && key >= jumpStart && key <= jumpStart + 0x80) ? 0x80 : 0);
    }

    const uint8_t V2_OFFSET_JUMP_START = 0x54;

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
    { //i=0 meint:length
        uint8_t highNibble = rev[src[i + 2] >> 4];
        uint8_t lowNibble = rev[src[i + 1] & 0x0F];
        return (highNibble << 4) | (lowNibble & 0x0F);
    }

    int transformPacket(uint8_t *tmp)
    {
        static const uint8_t LAST_SYNC_BYTE = 0x18; //for 9-Byte payload
        if (tmp[0] != LAST_SYNC_BYTE)
            return -1;
        if ((tmp[1] & 0xF0) != 0xA0)
            return -2;
        if ((tmp[1] & 0xF0) != 0xA0)
            return -3;
        int len = tmp[0] = getByte(tmp, 0);   //get payload length
        for (int i = 1; i < 1 + len + 2; i++) //length item + playload length + crc
        {
            tmp[i] = getByte(tmp, i);
        }
        uint16_t crcShouldbe = calc_crc(tmp, len + 1);
        uint16_t crcIs = (tmp[len + 2] << 8) | tmp[len + 1];

        if (crcShouldbe != crcIs)
        {
            //ESP_LOGE(TAG, "CRC ERROR crcShouldbe=0x%04X, crcIs=0x%04X", crcShouldbe, crcIs);
            return -4;
        }
        return 0;
    }

    uint32_t GetPacketId(uint8_t *packet)
    {
        return (((packet[1] & 0xF0) << 24) | (packet[2] << 16) | (packet[3] << 8) | (packet[7]));
    }
};
extern "C" void milightReceiverTask(void *arg)
{
    Milight *milight = (Milight *)arg;
    MilightDecoder dec{};
    uint8_t buf[20] __attribute__((aligned(4)));
    uint32_t previousID = 0;

    ESP_LOGI(TAG, "milightReceiverTask started");

    while (true)
    {
        vTaskDelay(50 / portTICK_RATE_MS);
        if (!milight->recv->IsDataReady())
            continue;
        milight->recv->GetRxData(buf);
        uint8_t *packet = buf + 1; //to ignore the very first byte, which is the satus byte (clocked out first). packet points to the packetLen
        //ESP_LOG_BUFFER_HEXDUMP("RAW", packet, PAYLOAD_SIZE, ESP_LOG_INFO);
        int err = dec.transformPacket(packet);
        if (err < 0)
            continue;
        //ESP_LOG_BUFFER_HEXDUMP("TRANS", packet, 9, ESP_LOG_INFO);
        uint32_t id = dec.GetPacketId(packet);
        packet = buf + 2; //packet points to the payload
        uint8_t key = dec.xorKey(packet[0]);
        for (size_t i = 1; i <= 7; i++)
        {
            packet[i] = dec.decodeByte(packet[i], 0, key, dec.V2_OFFSET(i, packet[0], dec.V2_OFFSET_JUMP_START));
        }
        if (id == previousID)
            continue;
        previousID = id;
        //ESP_LOG_BUFFER_HEXDUMP("DECRYP", packet, 9, ESP_LOG_INFO);
        uint8_t cmd = packet[4];
        uint8_t arg = packet[5];
        ESP_LOGI(TAG, "CMD %0d ARG/SEQ %3d", cmd, arg);
        milight->callback->ReceivedFromMilight(cmd, arg);
    }
}

Milight::Milight(MilightCallback *callback) : callback(callback) {}

esp_err_t Milight::Init(spi_host_device_t hostDevice, int dmaChannel, gpio_num_t ce_pin, gpio_num_t csn_pin, gpio_num_t miso_pin, gpio_num_t mosi_pin, gpio_num_t sclk_pin)
{
    this->recv = new Nrf24Receiver();
    recv->Setup(hostDevice, dmaChannel, ce_pin, csn_pin, miso_pin, mosi_pin, sclk_pin);
    uint8_t address[6]{0x90, 0x4e, 0x6c, 0x55, 0x55}; //for 9-Byte payload
    recv->Config(10, 14, address, 5, 0, Rf24Datarate::RF24_1MBPS, Rf24PowerAmp::RF24_PA_HIGH);
    return ESP_OK;
}

esp_err_t Milight::Start()
{
    return xTaskCreate(milightReceiverTask, "milightReceiverTask", 4096 * 4, this, 12, NULL) == pdPASS ? ESP_OK : ESP_FAIL;
}