
#include "include/ads1115.hh"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "i2c/interfaces.hh"


static const char *TAG = "ADS1115";


ADS1115::ADS1115(i2c::iI2CBus* i2c_bus, uint8_t address)
    : i2c_bus(i2c_bus), i2c_device(nullptr), address(address) {}

constexpr uint8_t SPSindex2WaitingTime[] = {125, 63, 32, 16, 8, 4, 3, 2};

ErrorCode ADS1115::Init(ads1115_sps_t sps, int64_t *howLongToWaitForResultMilliseconds)
{
    if (howLongToWaitForResultMilliseconds == nullptr) {
        return ErrorCode::INVALID_ARGUMENT_VALUES;
    }

    if (this->i2c_bus == nullptr) {
        *howLongToWaitForResultMilliseconds = INT64_MAX;
        return ErrorCode::INVALID_ARGUMENT_VALUES;
    }

    if (this->i2c_device == nullptr && this->i2c_bus->CreateDevice(this->address, &this->i2c_device) != ErrorCode::OK) {
        *howLongToWaitForResultMilliseconds = INT64_MAX;
        return ErrorCode::DEVICE_NOT_RESPONDING;
    }

    if (this->i2c_device == nullptr || this->i2c_device->Probe() != ErrorCode::OK) {
        *howLongToWaitForResultMilliseconds=INT64_MAX;
        return ErrorCode::DEVICE_NOT_RESPONDING;
    }
    config.bit.OS = 0; // always start conversion
    config.bit.MUX = (uint16_t)ads1115_mux_t::ADS1115_MUX_0_GND;
    config.bit.PGA = (uint16_t)ads1115_fsr_t::ADS1115_FSR_4_096;
    config.bit.MODE = (uint16_t)ads1115_mode_t::ADS1115_MODE_SINGLE;
    config.bit.DR = (uint16_t)sps;
    config.bit.COMP_MODE = 0;
    config.bit.COMP_POL = 0;
    config.bit.COMP_LAT = 0;
    config.bit.COMP_QUE = 0b11;
    ESP_LOGD(TAG, "Initial content of config register will be 0x%04X", config.reg);
    *howLongToWaitForResultMilliseconds=SPSindex2WaitingTime[config.bit.DR];

    return this->i2c_device->WriteRegisterU16BE(
        (uint8_t)ads1115_register_addresses_t::ADS1115_CONFIG_REGISTER_ADDR,
        config.reg
    );
}



ErrorCode ADS1115::TriggerMeasurement(ads1115_mux_t mux)
{
    config.bit.MUX=(uint16_t)mux;
    config.bit.OS=1;

    if (this->i2c_device == nullptr) {
        return ErrorCode::NOT_YET_INITIALIZED;
    }

    return this->i2c_device->WriteRegisterU16BE(
        (uint8_t)ads1115_register_addresses_t::ADS1115_CONFIG_REGISTER_ADDR,
        config.reg
    );
}
ErrorCode ADS1115::GetRaw(int16_t *val)
{
    if (this->i2c_device == nullptr || val == nullptr) {
        return ErrorCode::INVALID_ARGUMENT_VALUES;
    }

    uint16_t raw_u16 = 0;
    ErrorCode err = this->i2c_device->ReadRegisterU16BE(
        (uint8_t)ads1115_register_addresses_t::ADS1115_CONVERSION_REGISTER_ADDR,
        &raw_u16
    );
    if (err != ErrorCode::OK) {
        return err;
    }
    *val = static_cast<int16_t>(raw_u16);
    return ErrorCode::OK;
}

ErrorCode ADS1115::GetVoltage(float *val)
{
    if (val == nullptr) {
        return ErrorCode::INVALID_ARGUMENT_VALUES;
    }

    const double fsr[] = {6.144, 4.096, 2.048, 1.024, 0.512, 0.256};
    const int16_t bits = (1L << 15) - 1;
    int16_t raw;

    ErrorCode ret = GetRaw(&raw);
    if (ret != ErrorCode::OK) {
        return ret;
    }
    *val= (float)raw * fsr[config.bit.PGA] / (double)bits;
    return ret;
}