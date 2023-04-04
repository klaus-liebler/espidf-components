#pragma once
#include <stdint.h>
#include <driver/i2c.h>
#include <i2c.hh>
#include <errorcodes.hh>

namespace PCA9555
{
  constexpr uint8_t DEVICE_ADDRESS_BASE = 0x20;

  enum class Register : uint8_t
  {
    InputPort0 = 0,
    InputPort1 = 1,
    OutputPort0 = 2,
    OutputPort1 = 3,
    PolarityInversionPort0 = 4,
    PolarityInversionPort1 = 5,
    ConfigurationPort0 = 6,
    ConfigurationPort1 = 7,
  };

  enum class Device : uint8_t
  {
    Dev0 = DEVICE_ADDRESS_BASE + 0,
    Dev1 = DEVICE_ADDRESS_BASE + 1,
    Dev2 = DEVICE_ADDRESS_BASE + 2,
    Dev3 = DEVICE_ADDRESS_BASE + 3,
    Dev4 = DEVICE_ADDRESS_BASE + 4,
    Dev5 = DEVICE_ADDRESS_BASE + 5,
    Dev6 = DEVICE_ADDRESS_BASE + 6,
    Dev7 = DEVICE_ADDRESS_BASE + 7,
  };

  class M
  {
  private:
    iI2CPort* i2cPort;
    Device device;
    uint16_t cachedInput;
    uint16_t configurationRegister;
    uint16_t polarityInversionRegister;
  public:
    M(iI2CPort* i2cPort, Device device, uint16_t initialInputValue=0, uint16_t configurationRegister=0xFFFF, uint16_t polarityInversionRegister=0x0000);
    ErrorCode Setup();
    uint16_t GetCachedInput(void);
    ErrorCode Update(void);
    ErrorCode SetOutput(uint16_t output);
    ErrorCode SetInputOutputConfig(uint16_t config);
    ErrorCode SetInversionConfig(uint16_t config);
  };

}
