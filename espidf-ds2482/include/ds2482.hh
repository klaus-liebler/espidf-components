#pragma once
#include <driver/i2c.h>
#include <errorcodes.hh>


namespace DS2482
{
  constexpr uint8_t DEVICE_ADDRESS_BASE = 0x30;
  enum class Mode{
    STANDARD = 0x00,
    OVERDRIVE = 0x01,
    STRONG = 0x02,
  };


  enum struct Speed : uint8_t
  {
    STANDARD,
    OVERDRIVE,
  };

  enum struct Pullup : uint8_t
  {
    STANDARD,
    STRONG,
  };

  enum struct Device : uint8_t
  {
    Dev0 = DEVICE_ADDRESS_BASE+0,
    Dev1 = DEVICE_ADDRESS_BASE+1,
    Dev2 = DEVICE_ADDRESS_BASE+2,
    Dev3 = DEVICE_ADDRESS_BASE+3,
  };

  enum struct FamilyCode : uint8_t
  {
    DS2413 = 0x3A,
    DS18B20 = 0x28,
    DS2406 = 0x12,
    _3A2100H = 0x85,
    SENSACTSE = 0xCE,
  };

  enum struct Command : uint8_t
  {
    PIO_Access_Read = 0xF5,
    Channel_Access = 0xF5,
    PIO_Access_Write = 0x5A,
    CONVERT_T = 0x44,
    WRITE_SCRATCHPAD = 0x4E,
    READ_SCRATCHPAD = 0xBE,
    COPY_SCRATCHPAD = 0x48,
    RECALL_EE = 0xB8,
    READ_POWER_SUPPLY = 0xB4,

    Read_ROM = 0x33,
    Match_ROM = 0x55,
    Search_ROM = 0xF0,
    Skip_ROM = 0xCC,
    Resume = 0xA5,
    Overdrive_Skip = 0x3C,
    Overdrive_Match = 0x69,
  };

  class M
  {
  private:
    i2c_port_t i2c_port;
    Device device;
    uint8_t currCfg;
    bool LastDeviceFlag;
    int32_t LastDiscrepancy;
    int32_t LastFamilyDiscrepancy;
    bool alarmOnly;
    bool short_detected;
    bool reset();
    bool writeConfig();
    bool channelSelect(uint8_t channel);
    uint8_t pollStatus();
    uint8_t searchTriplet(uint8_t search_direction);
    static void calcCrc8(uint8_t data, uint8_t *crc);
    void fillRomAddressBuffer(FamilyCode familiy, const uint8_t *address);
    bool OWSearch();

  public:
    uint8_t ROM_NO[8];
    ErrorCode Setup();
    bool OWReset();
    bool OWTouchBit(bool sendbit);
    void OWWriteBit(bool sendbit);
    bool OWReadBit(void);
    void OWWriteByte(uint8_t sendbyte);
    uint8_t OWReadByte(void);
    uint8_t OWTouchByte(uint8_t sendbyte);
    void OWBlock(uint8_t *tran_buf, uint32_t tran_len);
    bool OWFirst(bool alarmOnly);
    bool OWNext();
    bool OWVerify();
    bool OWVerifyPresence(FamilyCode family, const uint8_t *romAddressWithoutCRC);

    bool OWReadDS2413(const FamilyCode family, uint8_t const *const address, uint8_t bitPosToSetOrClear, uint32_t *inputState);
    bool OWReadDS2413(const FamilyCode family, uint8_t const *const address, uint8_t *setOrClearBit0And1);
    bool OWWriteDS2413(const FamilyCode family, const uint8_t *address, bool outputA, bool outputB);
    bool OWWriteDS2406(const uint8_t *address, bool outputA, bool outputB);
    bool OWReadDS18B20Temp(const uint8_t *address, int16_t *temp);
    bool OWReadScratchpad(const FamilyCode family, uint8_t const *const address, uint8_t *buffer, const uint8_t cnt);
    bool OWWriteScratchpad(const FamilyCode family, const uint8_t *address, uint8_t *buffer, const uint8_t cnt);
    bool BeginTransaction(FamilyCode family, const uint8_t *address, Command cmd);
    bool BeginTransactionForAll(Command cmd);

    void OWTargetSetup(FamilyCode family_code);
    void OWFamilySkipSetup();

    Speed OWSpeed(Speed new_speed);
    Pullup OWTrySetPullup(Pullup new_level);
    bool OWWriteBytePower(uint8_t sendbyte);
    bool OWReadBitPower(bool applyPowerResponse);

    M(i2c_port_t i2c_port, Device device): i2c_port(i2c_port), device(device), currCfg(0), LastDeviceFlag(false), LastDiscrepancy(0), LastFamilyDiscrepancy(0), alarmOnly(false), short_detected(0)
    {
    }

    


  };

};
