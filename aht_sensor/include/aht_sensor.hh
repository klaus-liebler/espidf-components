#pragma once
#include <stdint.h>
#include <i2c_sensor.hh>

namespace AHT
{
  enum class ADDRESS
  {
    DEFAULT_ADDRESS = 0x38,
    Low = 0x38,
    High = 0x39,
  };

  struct STATUS_REG{
        uint8_t retain1:3;
        bool isCalibrated:1;
        bool BitWithSomeUnknownSignificanceBasedOnExampleSoftware:1;
        uint8_t retain2:2;
        bool isBusy:1;
    };
  class M : public I2CSensor
  {
  public:
    M(i2c_master_bus_handle_t bus_handle, AHT::ADDRESS slaveaddr = AHT::ADDRESS::DEFAULT_ADDRESS);
    ErrorCode Initialize(int64_t &waitTillFirstTrigger) override;
    ErrorCode Trigger(int64_t &waitTillReadout) override;
    ErrorCode Readout(int64_t &waitTillNExtTrigger) override;
    ErrorCode Read(float &humidity, float &temperatur);
    ErrorCode Reset();

  private:
    uint32_t temp{0};
    uint32_t humid{0};
    STATUS_REG readStatus();
    uint8_t calcCRC(uint8_t *buff,size_t len);
    ErrorCode waitWhileBusy(int64_t maxWaitMs=200);
    void resetReg(uint8_t reg);
  };
}
