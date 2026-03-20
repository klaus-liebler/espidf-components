#pragma once
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <errorcodes.hh>

namespace i2c {

class iI2CDevice{
    public:
    // Old names mapping:
    // ReadReg -> ReadRegister
    // ReadReg16 -> ReadRegisterAddress16
    // ReadDoubleReg / ReadSingleReg16 -> ReadRegisterU16BE
    // ReadSingleReg32 -> ReadRegisterU32BE
    // Read -> ReadRaw
    // WriteReg -> WriteRegister
    // WriteSingleReg -> WriteRegisterU8
    // WriteDoubleReg / WriteSingleReg16 -> WriteRegisterU16BE
    // WriteSingleReg32 -> WriteRegisterU32BE
    // Write -> WriteRaw
    // IsAvailable -> Probe

    virtual ErrorCode ReadRegister(const uint8_t reg_addr, uint8_t *reg_data, size_t len=1)=0;
    // 16-bit register address is transferred little-endian: low byte first, then high byte.
    virtual ErrorCode ReadRegisterAddress16(const uint16_t reg_addr16, uint8_t *reg_data, size_t len)=0;
    // Reads a 16-bit register value encoded big-endian in payload: MSB first, then LSB.
    virtual ErrorCode ReadRegisterU16BE(const uint8_t reg_addr, uint16_t *reg_data)=0;
    // Reads a 32-bit register value encoded big-endian in payload: byte3..byte0 (MSB to LSB).
    virtual ErrorCode ReadRegisterU32BE(const uint8_t reg_addr, uint32_t *reg_data)=0;
    virtual ErrorCode ReadRaw(uint8_t *data, size_t len)=0;
    virtual ErrorCode WriteRegister(const uint8_t reg_addr, const uint8_t * const reg_data, const size_t len)=0;
    virtual ErrorCode WriteRegisterU8(const uint8_t reg_addr, const uint8_t reg_data)=0;
    // Writes a 16-bit register value as big-endian payload: MSB first, then LSB.
    virtual ErrorCode WriteRegisterU16BE(const uint8_t reg_addr, const uint16_t reg_data)=0;
    // Writes a 32-bit register value as big-endian payload: byte3..byte0 (MSB to LSB).
    virtual ErrorCode WriteRegisterU32BE(const uint8_t reg_addr, const uint32_t reg_data)=0;
    virtual ErrorCode WriteRaw(const uint8_t * const data, const size_t len)=0;
    virtual ErrorCode Probe()=0;
};

enum class I2CSpeed {
    SPEED_100K=100000,
    SPEED_400K=400000,
    SPEED_1M=1000000,
};

class iI2CBus{
    public:
    virtual ErrorCode CreateDevice(const uint8_t address7bit, iI2CDevice **device)=0;
    // Returns a cached device for general-call address 0x00, or nullptr if unsupported.
    virtual iI2CDevice* GetGeneralCallDevice()=0;
    virtual ErrorCode ProbeAddress(const uint8_t address7bit)=0;
    virtual ErrorCode Scan(FILE* fp)=0;
};

} // namespace i2c