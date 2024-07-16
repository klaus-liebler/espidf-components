#include "bme280.h"
#include "bme280.hh"
#include "bme280_defs.h"
#include <inttypes.h>

namespace BME280
{
    M::M(i2c_master_bus_handle_t bus_handle, ADDRESS address):I2CSensor(bus_handle, (uint8_t)address){}


    M::~M() {}


    ErrorCode M::Initialize(int64_t &calculatedDelay)
    {
        int8_t rslt = BME280_OK;
        struct bme280_settings settings;
        dev.intf_ptr = this;
        dev.intf = BME280_I2C_INTF;
        dev.read = M::user_i2c_read;
        dev.write = M::user_i2c_write;
        dev.delay_us = M::user_delay_us;
        rslt = bme280_init(&dev);
        rslt = bme280_get_sensor_settings(&settings, &dev);
        if (rslt != 0){
            return ErrorCode::GENERIC_ERROR;
        }
        settings.filter = BME280_FILTER_COEFF_16;
        settings.osr_h = BME280_OVERSAMPLING_1X;
        settings.osr_p = BME280_OVERSAMPLING_16X;
        settings.osr_t = BME280_OVERSAMPLING_2X;
        settings.standby_time = BME280_STANDBY_TIME_0_5_MS;
        
        rslt = bme280_set_sensor_settings(BME280_SEL_ALL_SETTINGS, &settings, &dev);
        if (rslt != 0)
            return ErrorCode::GENERIC_ERROR;
        
        rslt = bme280_set_sensor_mode(BME280_POWERMODE_NORMAL, &dev);

        rslt = bme280_cal_meas_delay(&this->calculatedDelayMs, &settings);
        calculatedDelay=this->calculatedDelayMs;

        return rslt == BME280_OK ? ErrorCode::OK : ErrorCode::GENERIC_ERROR;
    }

    ErrorCode M::GetData(float *tempDegCel, float *pressurePa, float *relHumidityPercent){
        *tempDegCel = this->tempDegCel;
        *pressurePa = this->pressurePa;
        *relHumidityPercent = this->relHumidityPercent;
        return ErrorCode::OK;
    }
    
    ErrorCode M::Readout(int64_t &waitTillNextTrigger)
    {
        struct bme280_data comp_data;
        int8_t com_rslt = bme280_get_sensor_data(BME280_ALL, &comp_data, &dev);
        tempDegCel = comp_data.temperature;
        pressurePa = comp_data.pressure;
        relHumidityPercent = comp_data.humidity;
        if (com_rslt != 0)
            return ErrorCode::GENERIC_ERROR;
        com_rslt = bme280_set_sensor_mode(BME280_POWERMODE_FORCED, &dev);
        waitTillNextTrigger=this->calculatedDelayMs;
        return com_rslt == BME280_OK ? ErrorCode::OK : ErrorCode::GENERIC_ERROR;
    }

    ErrorCode M::Trigger(int64_t &waitTillReadout)
    {
        waitTillReadout=this->calculatedDelayMs;
        return bme280_set_sensor_mode(BME280_POWERMODE_FORCED, &dev) == BME280_OK ? ErrorCode::OK : ErrorCode::GENERIC_ERROR;
    }
}