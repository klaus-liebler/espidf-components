#pragma once

#include <cstdint>
#include <i2c_sensor.hh>
#include <driver/i2c_master.h>

namespace BH1750{
enum class ADDRESS:uint8_t
{
    LOW=0x23,
    HIGH=0x5C,
};

enum class OPERATIONMODE:uint8_t
{
    
    CONTINU_H_RESOLUTION=0x10,
    CONTINU_H_RESOLUTION2=0x11,
    CONTINU_L_RESOLUTION=0x13,
    ONETIME_H_RESOLUTION=0x20,
    ONETIME_H_RESOLUTION2=0x21,
    ONETIME_L_RESOLUTION=0x23,

};


class M:public I2CSensor{
private:
    i2c_port_t i2c_num;
    ADDRESS address;
    OPERATIONMODE operation;
    uint16_t recentValueLux{0};
public:
    M(i2c_master_bus_handle_t bus_handle, ADDRESS address, OPERATIONMODE operation);

    ErrorCode Trigger(int64_t& waitTillReadout) override;
    ErrorCode Readout(int64_t& waitTillNextTrigger) override;
    ErrorCode Initialize(int64_t& waitTillFirstTrigger) override;
    void Read(uint16_t &lux){lux=recentValueLux;}
};
}