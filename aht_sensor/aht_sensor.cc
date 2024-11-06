#include <inttypes.h>
#include <i2c.hh>
#include "aht_sensor.hh"
#include <esp_log.h>
#include <string.h>
#include <common-esp32.hh>
#define TAG "AHT"

namespace AHT
{
    
    
    
    constexpr uint8_t SOFTRESET_CMD{0xBA};         ///< Soft reset command
    constexpr uint8_t STATUS_BUSY_BIT{0x80};       ///< Status bit for busy
    constexpr uint8_t STATUS_CALIBRATED_BIT{0x08}; ///< Status bit for calibrated
    constexpr uint8_t CALIBRATE_CMD[]{0xE1, 0x08, 0x00};
    constexpr uint8_t eSensorNormalCmd[]{0xA8, 0x00, 0x00};
    constexpr uint8_t TRIGGER_MEASUREMENT_CMD[]{0xAC, 0x33, 0x00}; //"SendAC"
    constexpr bool GetRHumidityCmd{true};
    constexpr bool GetTempCmd{false};

    M::M(i2c_master_bus_handle_t bus_handle, AHT::ADDRESS slaveaddr) : I2CSensor(bus_handle, (uint8_t)slaveaddr)
    {
    }

    void M::resetReg(uint8_t addr){
        uint8_t buf[]={addr, 0x00, 0x00};
        ERRORCODE_CHECK(Write8(buf,3));
        vTaskDelay(2);
        ERRORCODE_CHECK(Read8(buf, 3));
        vTaskDelay(3);       
        buf[0]=addr|0xB0;
        ERRORCODE_CHECK(Write8(buf, 3));
 
    }

    ErrorCode M::Initialize(int64_t &waitTillFirstTrigger)
    {
        vTaskDelay(pdMS_TO_TICKS(100));
        ERRORCODE_CHECK(this->Reset());
        ESP_LOGI(TAG, "AHTxy successfully reset");
        uint8_t status = 0;
        ERRORCODE_CHECK(Read8(&status, 1));
        if((status&0x18)!=0x18){
	        resetReg(0x1b);
	        resetReg(0x1c);
	        resetReg(0x1e);
        }
        ESP_LOGI(TAG, "AHTxy successfully initialized");
        return ErrorCode::OK;
    }
    ErrorCode M::Readout(int64_t &waitTillNextTrigger)
    {
        uint8_t temp[7];
        waitTillNextTrigger = 500;
        Read8(temp, 7);
        if(calcCRC(temp, 6)!=temp[6]){
            ESP_LOGE(TAG, "AHTxy CRC error");
            return ErrorCode::CRC;
        }
        this->humid = ((temp[1] << 16) | (temp[2] << 8) | temp[3]) >> 4;
        this->temp = ((temp[3] & 0x0F) << 16) | (temp[4] << 8) | temp[5];
        ESP_LOGD(TAG, "Readout H=%lu T=%lu", this->humid, this->temp);
        return ErrorCode::OK;
    }
    ErrorCode M::Trigger(int64_t &waitTillReadout)
    {
        waitTillReadout = 500;
        ESP_LOGD(TAG, "Trigger!!!!");
        return Write8(TRIGGER_MEASUREMENT_CMD, sizeof(TRIGGER_MEASUREMENT_CMD));
    }

    ErrorCode M::Read(float &humidity, float &temperature)
    {
        humidity = this->humid * 100.0f / 1048576.0f;
        temperature = (this->temp * 200.0f / 1048576.0f) - 50.0f;
        return ErrorCode::OK;
    }

    ErrorCode M::Reset()
    {
        ERRORCODE_CHECK(Write8(&SOFTRESET_CMD, 1));
        return waitWhileBusy();
    }

    ErrorCode M::waitWhileBusy(int64_t maxWaitMs){
        STATUS_REG status;
        while (maxWaitMs>0)
        {
            vTaskDelay(pdMS_TO_TICKS(20));
            ERRORCODE_CHECK(Read8((uint8_t*)&status, 1));
            if(!status.isBusy){
                return ErrorCode::OK;
            }
           maxWaitMs-=20;
        }
        return ErrorCode::TIMEOUT;
    }

    STATUS_REG M::readStatus()
    {
        uint8_t status = {};
        ERRORCODE_CHECK(Read8(&status, 1));
        return (STATUS_REG)status;
    }

    uint8_t M::calcCRC(uint8_t *buff, size_t len)
    {
        uint8_t i;
        size_t byte;
        uint8_t crc = 0xFF;
        for (byte = 0; byte < len; byte++)
        {
            crc ^= (buff[byte]);
            for (i = 8; i > 0; --i)
            {
                if (crc & 0x80)
                    crc = (crc << 1) ^ 0x31;
                else
                    crc = (crc << 1);
            }
        }
        return crc;
    }
}