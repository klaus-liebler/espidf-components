#include <inttypes.h>
#include <i2c.hh>
#include "aht_sensor.hh"
#include <esp_log.h>
#include <string.h>
#include <common.hh>
#define TAG "AHT"

namespace AHT
{
    constexpr uint8_t eSensorCalibrateCmd[]{0xE1, 0x08, 0x00};
    constexpr uint8_t eSensorNormalCmd[]{0xA8, 0x00, 0x00};
    constexpr uint8_t eSensorMeasureCmd[]{0xAC, 0x33, 0x00};
    constexpr uint8_t eSensorResetCmd{0xBA};
    constexpr bool GetRHumidityCmd{true};
    constexpr bool GetTempCmd{false};

    M::M(i2c_master_bus_handle_t bus_handle, AHT::ADDRESS slaveaddr) : I2CSensor(bus_handle, (uint8_t)slaveaddr)
    {
    }
    ErrorCode M::Initialize(int64_t &waitTillFirstTrigger)
    {
        if (Probe((uint8_t)this->address_7bit) != ErrorCode::OK)
        {
            ESP_LOGW(TAG, "AHTxy not found");
        }
        if (WriteRegs8(this->address_7bit, eSensorCalibrateCmd, sizeof(eSensorCalibrateCmd)) != ErrorCode::OK)
        {
            ESP_LOGE(TAG, "Could not write calibration commands.");
            return ErrorCode::GENERIC_ERROR;
        }
        vTaskDelay(pdMS_TO_TICKS(500));
        if ((readStatus() & 0x68) != 0x08)
        {
            ESP_LOGE(TAG, "Incorrect status after calibration");
            return ErrorCode::GENERIC_ERROR;
        }
        ESP_LOGI(TAG, "AHTxy successfully initialized");
        return ErrorCode::OK;
    }
    ErrorCode M::Readout(int64_t &waitTillNextTrigger)
    {
        uint8_t temp[6];
        waitTillNextTrigger=500;
        Read8(temp, 6);
        this->humid = ((temp[1] << 16) | (temp[2] << 8) | temp[3]) >> 4;
        this->temp = ((temp[3] & 0x0F) << 16) | (temp[4] << 8) | temp[5];
        ESP_LOGD(TAG, "Readout H=%lu T=%lu", this->humid, this->temp);
        return ErrorCode::OK;
    }
    ErrorCode M::Trigger(int64_t &waitTillReadout)
    {
        waitTillReadout = 500;
        ESP_LOGD(TAG, "Trigger!!!!");
        return Write8(eSensorMeasureCmd, sizeof(eSensorMeasureCmd));
    }

    ErrorCode M::Read(float &humidity, float &temperature)
    {
        humidity = this->humid*100.0f/1048576.0f;
        temperature = (this->temp*200.0f/1048576.0f)-50.0f;
        return ErrorCode::OK;
    }

    ErrorCode M::Reset(){
        return Write8(&eSensorResetCmd, 1);
    }

    uint8_t M::readStatus(){
        uint8_t result = 0;
        Read8(&result, 1);
        return result;
    }
}