#pragma once
#include <cmath>
#include <limits>
#include <cstring>
#include <vector>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_check.h"
#include "onewire_bus.h"
#include "onewire_cmd.h"
#include "onewire_crc.h"
#include "ds18b20.h"

#define TAG "ds18b20"
namespace OneWire
{

    constexpr uint8_t DS18B20_CMD_CONVERT_TEMP{0x44};
    constexpr uint8_t DS18B20_CMD_WRITE_SCRATCHPAD{0x4E};
    constexpr uint8_t DS18B20_CMD_READ_SCRATCHPAD{0xBE};
    /**
     * @brief Structure of DS18B20's scratchpad
     */
    typedef struct
    {
        uint8_t temp_lsb;      /*!< lsb of temperature */
        uint8_t temp_msb;      /*!< msb of temperature */
        uint8_t th_user1;      /*!< th register or user byte 1 */
        uint8_t tl_user2;      /*!< tl register or user byte 2 */
        uint8_t configuration; /*!< resolution configuration register */
        uint8_t _reserved1;
        uint8_t _reserved2;
        uint8_t _reserved3;
        uint8_t crc_value; /*!< crc value of scratchpad data */
    } __attribute__((packed)) ds18b20_scratchpad_t;

    
    
    class Ds18B20
    {
    private:
        onewire_device_address_t addr;
        uint8_t th_user1;
        uint8_t tl_user2;
        ds18b20_resolution_t resolution;
        float lastTemperature{std::numeric_limits<float>::quiet_NaN()};

        esp_err_t SendCommand(onewire_bus_handle_t bus, uint8_t cmd)
        {
            // send command
            uint8_t tx_buffer[10] = {0};
            tx_buffer[0] = ONEWIRE_CMD_MATCH_ROM;
            memcpy(&tx_buffer[1], &addr, sizeof(addr));
            tx_buffer[sizeof(addr) + 1] = cmd;
            return onewire_bus_write_bytes(bus, tx_buffer, sizeof(tx_buffer));
        }

        Ds18B20(onewire_device_address_t addr, ds18b20_resolution_t resolution) : addr(addr), th_user1(0), tl_user2(0), resolution(resolution) {}

    public:
        static Ds18B20 *BuildFromOnewireDevice(onewire_device_t *device)
        {
            if (!device)
            {
                return nullptr;
            }
            // check ROM ID, the family code of DS18B20 is 0x28
            if ((device->address & 0xFF) != 0x28)
            {
                ESP_LOGD(TAG, "%016llX is not a DS18B20 device", device->address);
                return nullptr;
            }
            return new Ds18B20(device->address, DS18B20_RESOLUTION_12B); // DS18B20 default resolution is 12 bits
        }

        esp_err_t SetResolution(onewire_bus_handle_t bus, ds18b20_resolution_t resolution)
        {

            ESP_RETURN_ON_ERROR(onewire_bus_reset(bus), TAG, "reset bus error");
            ESP_RETURN_ON_ERROR(SendCommand(bus, DS18B20_CMD_WRITE_SCRATCHPAD), TAG, "send DS18B20_CMD_WRITE_SCRATCHPAD failed");

            // write new resolution to scratchpad
            const uint8_t resolution_data[] = {0x1F, 0x3F, 0x5F, 0x7F};
            uint8_t tx_buffer[3] = {0};
            tx_buffer[0] = th_user1;
            tx_buffer[1] = tl_user2;
            tx_buffer[2] = resolution_data[resolution];
            ESP_RETURN_ON_ERROR(onewire_bus_write_bytes(bus, tx_buffer, sizeof(tx_buffer)), TAG, "send new resolution failed");

            resolution = resolution;
            return ESP_OK;
        }

        esp_err_t TriggerTemperatureConversion(onewire_bus_handle_t bus)
        {
            ESP_RETURN_ON_ERROR(onewire_bus_reset(bus), TAG, "reset bus error");
            // send command: DS18B20_CMD_CONVERT_TEMP
            ESP_RETURN_ON_ERROR(SendCommand(bus, DS18B20_CMD_CONVERT_TEMP), TAG, "send DS18B20_CMD_CONVERT_TEMP failed");
            return ESP_OK;
        }

       
        float GetTemperature(){
            return this->lastTemperature;
        }
       
        esp_err_t UpdateTemperature(onewire_bus_handle_t bus)
        {
            // reset bus and check if the ds18b20 is present
            ESP_RETURN_ON_ERROR(onewire_bus_reset(bus), TAG, "reset bus error");

            // send command: DS18B20_CMD_READ_SCRATCHPAD
            ESP_RETURN_ON_ERROR(SendCommand(bus, DS18B20_CMD_READ_SCRATCHPAD), TAG, "send DS18B20_CMD_READ_SCRATCHPAD failed");

            // read scratchpad data
            ds18b20_scratchpad_t scratchpad;
            ESP_RETURN_ON_ERROR(onewire_bus_read_bytes(bus, (uint8_t *)&scratchpad, sizeof(scratchpad)),
                                TAG, "error while reading scratchpad data");
            // check crc
            ESP_RETURN_ON_FALSE(onewire_crc8(0, (uint8_t *)&scratchpad, 8) == scratchpad.crc_value, ESP_ERR_INVALID_CRC, TAG, "scratchpad crc error");

            const uint8_t lsb_mask[4] = {0x07, 0x03, 0x01, 0x00}; // mask bits not used in low resolution
            uint8_t lsb_masked = scratchpad.temp_lsb & (~lsb_mask[scratchpad.configuration >> 5]);
            this->lastTemperature = (((int16_t)scratchpad.temp_msb << 8) | lsb_masked) / 16.0f;

            return ESP_OK;
        }
    };

    template <gpio_num_t gpio>
    class OneWireBus
    {
        private:
        onewire_bus_handle_t bus;
        std::vector<Ds18B20 *> ds18b20_vect;
        time_t nextReadoutMs{INT64_MAX};

        public:
        void Loop(time_t nowMs)
        {
            if(nowMs<nextReadoutMs) return;
            for (Ds18B20* sensor:this->ds18b20_vect)
            {
               sensor->UpdateTemperature(bus);
            }
            TriggerTemperatureConversionForAll();
            nextReadoutMs=nowMs+800;
        }

        float GetMostRecentTemp(size_t index){
            if(index>=ds18b20_vect.size()) return std::numeric_limits<float>::quiet_NaN();
            return ds18b20_vect.at(index)->GetTemperature();
        }

        float GetMaxTemp(){
            
            float max{-273.15};
            for (size_t i = 0; i < ds18b20_vect.size(); i++)
            {
                float t=ds18b20_vect.at(i)->GetTemperature();
                if(t>max){ max=t;}
            }
            return max;
        }

        float GetMinTemp(){
            
            float min{+1000};
            for (size_t i = 0; i < ds18b20_vect.size(); i++)
            {
                float t=ds18b20_vect.at(i)->GetTemperature();
                if(t<min){ min=t;}
            }
            return min;
        }


        esp_err_t TriggerTemperatureConversionForAll(){
            ESP_RETURN_ON_ERROR(onewire_bus_reset(bus), TAG, "reset bus error");
            uint8_t tx_buffer[2] = {0};
            tx_buffer[0] = ONEWIRE_CMD_SKIP_ROM;
            tx_buffer[1] = DS18B20_CMD_CONVERT_TEMP;
            return onewire_bus_write_bytes(bus, tx_buffer, sizeof(tx_buffer));
        }

        void Init()
        {
            onewire_bus_config_t bus_config = {
                .bus_gpio_num = gpio,
            };
            onewire_bus_rmt_config_t rmt_config = {
                .max_rx_bytes = 10, // 1byte ROM command + 8byte ROM number + 1byte device command
            };
            ESP_ERROR_CHECK(onewire_new_bus_rmt(&bus_config, &rmt_config, &bus));
            ESP_LOGI(TAG, "1-Wire bus installed on GPIO%d", gpio);
            onewire_device_iter_handle_t iter = nullptr;
            onewire_device_t next_onewire_device;
            esp_err_t search_result = ESP_OK;
            assert(&iter);
            // create 1-wire device iterator, which is used for device search
            ESP_ERROR_CHECK(onewire_new_device_iter(bus, &iter));
            ESP_LOGI(TAG, "OneWire Device iterator created, start searching...");
            while (true)
            {
                search_result = onewire_device_iter_get_next(iter, &next_onewire_device);
                if (search_result != ESP_OK)
                {
                    break;
                }
                // found a new device, let's check if we can upgrade it to a DS18B20
                Ds18B20 *device = Ds18B20::BuildFromOnewireDevice(&next_onewire_device);
                if (!device)
                {
                    ESP_LOGI(TAG, "Found an unknown device, address: %016llX", next_onewire_device.address);
                    continue;
                }
                ESP_LOGI(TAG, "Found a DS18B20[%d], address: %016llX", ds18b20_vect.size(), next_onewire_device.address);
                ds18b20_vect.push_back(device);
            }
            ESP_ERROR_CHECK(onewire_del_device_iter(iter));
            TriggerTemperatureConversionForAll();
            nextReadoutMs=(esp_timer_get_time()/1000)+800;
            ESP_LOGI(TAG, "Searching done, %d DS18B20 device(s) found", ds18b20_vect.size());
        }
    };


}
#undef TAG