#pragma once
#include <stdint.h>
#include <i2c_sensor.hh>

namespace AHT
{
     enum class ADDRESS
    {
        default_address = 0x38,
        Low = 0x38,
        High = 0x39,
    };
 class M:public I2CSensor
  {
  public:                                                                              
    M(i2c_master_bus_handle_t bus_handle,  AHT::ADDRESS slaveaddr = AHT::ADDRESS::default_address); 
    ErrorCode Initialize(int64_t& waitTillFirstTrigger) override;                                                                                                       
    ErrorCode Trigger(int64_t& waitTillReadout) override;
    ErrorCode Readout(int64_t& waitTillNExtTrigger)override;
    ErrorCode Read(float &humidity, float &temperatur);
    ErrorCode Reset();
  private:
    uint32_t temp{0};
    uint32_t humid{0};
    uint8_t readStatus();
  };
}
