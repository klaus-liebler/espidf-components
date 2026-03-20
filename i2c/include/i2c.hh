#pragma once

#include <cstdint>
#include <cstddef>
#include <cstdio>

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "errorcodes.hh"
#include "i2c/interfaces.hh"

namespace i2c {

class iI2CDevice_Impl : public iI2CDevice {
private:
    i2c_master_bus_handle_t bus_handle;
    i2c_master_dev_handle_t dev_handle;
    uint8_t address7bit;

    static ErrorCode ToErrorCode(esp_err_t err);

public:
    iI2CDevice_Impl(i2c_master_bus_handle_t bus_handle, i2c_master_dev_handle_t dev_handle, uint8_t address7bit);

    ErrorCode ReadRegister(const uint8_t reg_addr, uint8_t *reg_data, size_t len = 1) override;
    ErrorCode ReadRegisterAddress16(const uint16_t reg_addr16, uint8_t *reg_data, size_t len) override;
    ErrorCode ReadRegisterU16BE(const uint8_t reg_addr, uint16_t *reg_data) override;
    ErrorCode ReadRegisterU32BE(const uint8_t reg_addr, uint32_t *reg_data) override;
    ErrorCode ReadRaw(uint8_t *data, size_t len) override;
    ErrorCode WriteRegister(const uint8_t reg_addr, const uint8_t *const reg_data, const size_t len) override;
    ErrorCode WriteRegisterU8(const uint8_t reg_addr, const uint8_t reg_data) override;
    ErrorCode WriteRegisterU16BE(const uint8_t reg_addr, const uint16_t reg_data) override;
    ErrorCode WriteRegisterU32BE(const uint8_t reg_addr, const uint32_t reg_data) override;
    ErrorCode WriteRaw(const uint8_t *const data, const size_t len) override;
    ErrorCode Probe() override;
};

class iI2CBus_Impl : public iI2CBus {
private:
    i2c_master_bus_handle_t bus_handle = nullptr;
    uint32_t device_speed_hz = static_cast<uint32_t>(I2CSpeed::SPEED_100K);
    iI2CDevice* general_call_device = nullptr;

public:
    iI2CBus_Impl() = default;

    ErrorCode Init(i2c_port_t port, gpio_num_t scl, gpio_num_t sda);
    ErrorCode SetDefaultSpeed(I2CSpeed speed);
    ErrorCode CreateDevice(const uint8_t address7bit, iI2CDevice **device) override;
    iI2CDevice* GetGeneralCallDevice() override;
    ErrorCode ProbeAddress(const uint8_t address7bit) override;
    ErrorCode Scan(FILE *fp) override;
};

} // namespace i2c
