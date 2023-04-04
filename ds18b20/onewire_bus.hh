#pragma once

#include "esp_err.h"
#include <esp_log.h>
#include <esp_check.h>
#include <string.h>
#include "onewire_bus_rmt.h"

#define ONEWIRE_ROM_ID(id) (id)[0], (id)[1], (id)[2], (id)[3], (id)[4], (id)[5], (id)[6], (id)[7]
#define ONEWIRE_ROM_ID_STR "%02X%02X%02X%02X%02X%02X%02X%02X"

#define TAG "1Wi"

namespace onewire
{
    constexpr uint8_t ONEWIRE_CMD_SEARCH_ROM{0xF0};
    constexpr uint8_t ONEWIRE_CMD_READ_ROM{0x33};
    constexpr uint8_t ONEWIRE_CMD_MATCH_ROM{0x55};
    constexpr uint8_t ONEWIRE_CMD_SKIP_ROM{0xCC};
    constexpr uint8_t ONEWIRE_CMD_ALARM_SEARCH_ROM{0xEC};
    constexpr uint8_t DS18B20_CMD_CONVERT_TEMP{0x44};
    constexpr uint8_t DS18B20_CMD_WRITE_SCRATCHPAD{0x4E};
    constexpr uint8_t DS18B20_CMD_READ_SCRATCHPAD{0xBE};
    constexpr uint8_t dscrc_table[] = {
        0, 94, 188, 226, 97, 63, 221, 131, 194, 156, 126, 32, 163, 253, 31, 65,
        157, 195, 33, 127, 252, 162, 64, 30, 95, 1, 227, 189, 62, 96, 130, 220,
        35, 125, 159, 193, 66, 28, 254, 160, 225, 191, 93, 3, 128, 222, 60, 98,
        190, 224, 2, 92, 223, 129, 99, 61, 124, 34, 192, 158, 29, 67, 161, 255,
        70, 24, 250, 164, 39, 121, 155, 197, 132, 218, 56, 102, 229, 187, 89, 7,
        219, 133, 103, 57, 186, 228, 6, 88, 25, 71, 165, 251, 120, 38, 196, 154,
        101, 59, 217, 135, 4, 90, 184, 230, 167, 249, 27, 69, 198, 152, 122, 36,
        248, 166, 68, 26, 153, 199, 37, 123, 58, 100, 134, 216, 91, 5, 231, 185,
        140, 210, 48, 110, 237, 179, 81, 15, 78, 16, 242, 172, 47, 113, 147, 205,
        17, 79, 173, 243, 112, 46, 204, 146, 211, 141, 111, 49, 178, 236, 14, 80,
        175, 241, 19, 77, 206, 144, 114, 44, 109, 51, 209, 143, 12, 82, 176, 238,
        50, 108, 142, 208, 83, 13, 239, 177, 240, 174, 76, 18, 145, 207, 45, 115,
        202, 148, 118, 40, 171, 245, 23, 73, 8, 86, 180, 234, 105, 55, 213, 139,
        87, 9, 235, 181, 54, 104, 138, 212, 149, 203, 41, 119, 244, 170, 72, 22,
        233, 183, 85, 11, 136, 214, 52, 106, 43, 117, 151, 201, 74, 20, 246, 168,
        116, 42, 200, 150, 21, 75, 169, 247, 182, 232, 10, 84, 215, 137, 107, 53};

    struct ds18b20_scratchpad_t {
        uint8_t temp_lsb; /*!< lsb of temperature */
        uint8_t temp_msb; /*!< msb of temperature */
        uint8_t th_user1; /*!< th register or user byte 1 */
        uint8_t tl_user2; /*!< tl register or user byte 2 */
        uint8_t configuration; /*!< configuration register */
        uint8_t _reserved1;
        uint8_t _reserved2;
        uint8_t _reserved3;
        uint8_t crc_value; /*!< crc value of scratchpad data */
    } ;

    /**
     * @brief Enumeration of DS18B20's resolution config
     *
     */
    enum ds18b20_resolution_t{
        DS18B20_RESOLUTION_12B = 0x7F, /*!< 750ms convert time */
        DS18B20_RESOLUTION_11B = 0x5F, /*!< 375ms convert time */
        DS18B20_RESOLUTION_10B = 0x3F, /*!< 187.5ms convert time */
        DS18B20_RESOLUTION_9B = 0x1F, /*!< 93.75ms convert time */
    } ;


    class M
    {
    private:
        bool last_device_flag;
        uint16_t last_discrepancy;
        uint8_t rom_number[8];
        onewire_bus_handle_t handle;

        uint8_t onewire_check_crc8(uint8_t *input, size_t input_size)
        {
            uint8_t crc8 = 0;
            for (size_t i = 0; i < input_size; i++)
            {
                crc8 = dscrc_table[crc8 ^ input[i]];
            }
            return crc8;
        }

    public:
        esp_err_t Init(gpio_num_t gpio, size_t maxRxBytes=10)
        {
            onewire_rmt_config_t config = {};
            config.gpio_pin = gpio;
            config.max_rx_bytes = maxRxBytes;

            ESP_ERROR_CHECK(onewire_new_bus_rmt(&config, &this->handle));
            ESP_LOGI(TAG, "1-wire bus via rmt installed");
            ResetSearch();
            return ESP_OK;
        }

        esp_err_t GetRomNumber(uint8_t *rom_number_out)
        {
            ESP_RETURN_ON_FALSE(rom_number_out, ESP_ERR_INVALID_ARG, TAG, "invalid rom_number pointer");
            memcpy(rom_number_out, this->rom_number, sizeof(this->rom_number));
            return ESP_OK;
        }

        void ResetSearch()
        {
            this->last_device_flag = 0;
            this->last_discrepancy = 0;
            memset(rom_number, 0, 8);
        }

        esp_err_t SearchRom()
        {
            uint8_t last_zero = 0;
            if (!this->last_device_flag)
            {
                if (onewire_bus_reset(this->handle) != ESP_OK)
                { // no device present
                    return ESP_ERR_NOT_FOUND;
                }

                // send rom search command and start search algorithm
                uint8_t data[1]={ONEWIRE_CMD_SEARCH_ROM};
                ESP_RETURN_ON_ERROR(onewire_bus_write_bytes(this->handle, data, 1), TAG, "error while sending search rom command");

                for (uint16_t rom_bit_index = 0; rom_bit_index < 64; rom_bit_index++)
                {
                    uint8_t rom_byte_index = rom_bit_index / 8;
                    uint8_t rom_bit_mask = 1 << (rom_bit_index % 8); // calculate byte index and bit mask in advance for convenience

                    uint8_t rom_bit, rom_bit_complement;
                    ESP_RETURN_ON_ERROR(onewire_bus_read_bit(this->handle, &rom_bit), TAG, "error while reading rom bit"); // write 1 bit to read from the bus
                    ESP_RETURN_ON_ERROR(onewire_bus_read_bit(this->handle, &rom_bit_complement),
                                        TAG, "error while reading rom bit"); // read a bit and its complement

                    uint8_t search_direction;
                    if (rom_bit && rom_bit_complement)
                    { // No devices participating in search.
                        ESP_LOGE(TAG, "no devices participating in search");
                        return ESP_ERR_NOT_FOUND;
                    }
                    else
                    {
                        if (rom_bit != rom_bit_complement)
                        {                               // There are only 0s or 1s in the bit of the participating ROM numbers.
                            search_direction = rom_bit; // just go ahead
                        }
                        else
                        { // There are both 0s and 1s in the current bit position of the participating ROM numbers. This is a discrepancy.
                            if (rom_bit_index < this->last_discrepancy)
                            {                                                                                          // current id bit is before the last discrepancy bit
                                search_direction = (this->rom_number[rom_byte_index] & rom_bit_mask) ? 0x01 : 0x00; // follow previous way
                            }
                            else
                            {
                                search_direction = (rom_bit_index == this->last_discrepancy) ? 0x01 : 0x00; // search for 0 bit first
                            }

                            if (search_direction == 0)
                            { // record zero's position in last zero
                                last_zero = rom_bit_index;
                            }
                        }

                        if (search_direction == 1)
                        { // set corrsponding rom bit by serach direction
                            this->rom_number[rom_byte_index] |= rom_bit_mask;
                        }
                        else
                        {
                            this->rom_number[rom_byte_index] &= ~rom_bit_mask;
                        }

                        ESP_RETURN_ON_ERROR(onewire_bus_write_bit(this->handle, search_direction),
                                            TAG, "error while writing direction bit"); // set search direction
                    }
                }
            }
            else
            {
                ESP_LOGD(TAG, "1-wire rom search finished");
                return ESP_FAIL;
            }

            // if the search was successful
            this->last_discrepancy = last_zero;
            if (this->last_discrepancy == 0)
            { // last zero loops back to the first bit
                this->last_device_flag = true;
            }

            if (onewire_check_crc8(this->rom_number, 7) != this->rom_number[7])
            { // check crc
                ESP_LOGE(TAG, "bad crc checksum of device with id " ONEWIRE_ROM_ID_STR, ONEWIRE_ROM_ID(this->rom_number));
                return ESP_ERR_INVALID_CRC;
            }

            return ESP_OK;
        }

        esp_err_t TriggerTemperatureConversion(const uint8_t *rom_number)
        {
            ESP_RETURN_ON_ERROR(onewire_bus_reset(handle), TAG, "error while resetting bus"); // reset bus and check if the device is present

            uint8_t tx_buffer[10];
            uint8_t tx_buffer_size;

            if (rom_number) { // specify rom id
                tx_buffer[0] = ONEWIRE_CMD_MATCH_ROM;
                tx_buffer[9] = DS18B20_CMD_CONVERT_TEMP;
                memcpy(&tx_buffer[1], rom_number, 8);
                tx_buffer_size = 10;
            } else { // skip rom id
                tx_buffer[0] = ONEWIRE_CMD_SKIP_ROM;
                tx_buffer[1] = DS18B20_CMD_CONVERT_TEMP;
                tx_buffer_size = 2;
            }

            ESP_RETURN_ON_ERROR(onewire_bus_write_bytes(handle, tx_buffer, tx_buffer_size),
                                TAG, "error while triggering temperature convert");

            return ESP_OK;
        }

        esp_err_t GetTemperature(const uint8_t *rom_number, float *temperature)
        {
            ESP_RETURN_ON_FALSE(temperature, ESP_ERR_INVALID_ARG, TAG, "invalid temperature pointer");

            ESP_RETURN_ON_ERROR(onewire_bus_reset(handle), TAG, "error while resetting bus"); // reset bus and check if the device is present

            ds18b20_scratchpad_t scratchpad;

            uint8_t tx_buffer[10];
            uint8_t tx_buffer_size;

            if (rom_number) { // specify rom id
                tx_buffer[0] = ONEWIRE_CMD_MATCH_ROM;
                tx_buffer[9] = DS18B20_CMD_READ_SCRATCHPAD;
                memcpy(&tx_buffer[1], rom_number, 8);
                tx_buffer_size = 10;
            } else {
                tx_buffer[0] = ONEWIRE_CMD_SKIP_ROM;
                tx_buffer[1] = DS18B20_CMD_READ_SCRATCHPAD;
                tx_buffer_size = 2;
            }

            ESP_RETURN_ON_ERROR(onewire_bus_write_bytes(handle, tx_buffer, tx_buffer_size),
                                TAG, "error while sending read scratchpad command");
            ESP_RETURN_ON_ERROR(onewire_bus_read_bytes(handle, (uint8_t *)&scratchpad, sizeof(scratchpad)),
                                TAG, "error while reading scratchpad command");

            ESP_RETURN_ON_FALSE(onewire_check_crc8((uint8_t *)&scratchpad, 8) == scratchpad.crc_value, ESP_ERR_INVALID_CRC,
                                TAG, "crc error");

            static const uint8_t lsb_mask[4] = { 0x07, 0x03, 0x01, 0x00 };
            uint8_t lsb_masked = scratchpad.temp_lsb & (~lsb_mask[scratchpad.configuration >> 5]); // mask bits not used in low resolution
            *temperature = (((int16_t)scratchpad.temp_msb << 8) | lsb_masked)  / 16.0f;

            return ESP_OK;
        }

        esp_err_t SetResolution(const uint8_t *rom_number, ds18b20_resolution_t resolution)
        {
            uint8_t tx_buffer[10];
            uint8_t tx_buffer_size;

            if (rom_number) { // specify rom id
                tx_buffer[0] = ONEWIRE_CMD_MATCH_ROM;
                tx_buffer[9] = DS18B20_CMD_WRITE_SCRATCHPAD;
                memcpy(&tx_buffer[1], rom_number, 8);
                tx_buffer_size = 10;
            } else {
                tx_buffer[0] = ONEWIRE_CMD_SKIP_ROM;
                tx_buffer[1] = DS18B20_CMD_WRITE_SCRATCHPAD;
                tx_buffer_size = 2;
            }

            ESP_RETURN_ON_ERROR(onewire_bus_write_bytes(handle, tx_buffer, tx_buffer_size),
                                TAG, "error while sending read scratchpad command");

            tx_buffer[0] = 0;
            tx_buffer[1] = 0;
            tx_buffer[2] = resolution;
            ESP_RETURN_ON_ERROR(onewire_bus_write_bytes(handle, tx_buffer, 3),
                                TAG, "error while sending write scratchpad command");

            return ESP_OK;
        }

    };
};
#undef TAG