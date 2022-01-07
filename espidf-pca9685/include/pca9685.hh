#pragma once
#include <stdint.h>
#include <driver/i2c.h>
#include <errorcodes.hh>

namespace PCA9685
{
  constexpr uint8_t DEVICE_ADDRESS_BASE = 0x40;

  constexpr uint8_t MODE1 = 0x00;
  constexpr uint8_t MODE1_ALLCALL = 0;
  constexpr uint8_t MODE1_SUB3 = 1;
  constexpr uint8_t MODE1_SUB2 = 2;
  constexpr uint8_t MODE1_SUB1 = 3;
  constexpr uint8_t MODE1_SLEEP = 4;
  constexpr uint8_t MODE1_AI = 5;
  constexpr uint8_t MODE1_EXTCLK = 6;
  constexpr uint8_t MODE1_RESTART = 7;
  constexpr uint8_t MODE2 = 0x01;
  constexpr uint8_t MODE2_OUTNE0 = 0;
  constexpr uint8_t MODE2_OUTNE1 = 1;
  constexpr uint8_t MODE2_OUTDRV = 2;
  constexpr uint8_t MODE2_OCH = 3;
  constexpr uint8_t MODE2_INVRT = 4;
  constexpr uint8_t SUBADR1 = 0x02;
  constexpr uint8_t SUBADR2 = 0x03;
  constexpr uint8_t SUBADR3 = 0x04;
  constexpr uint8_t ALLCALLADR = 0x05;
  constexpr uint8_t ALL_LED_ON_L = 0xFA;
  constexpr uint8_t ALL_LED_ON_H = 0xFB;
  constexpr uint8_t ALL_LED_OFF_L = 0xFC;
  constexpr uint8_t ALL_LED_OFF_H = 0xFD;
  constexpr uint8_t PRE_SCALE = 0xFE;
  constexpr uint16_t MAX_OUTPUT_VALUE = 0x1000;

  constexpr uint8_t SWRST = 0b00000110;
	constexpr uint8_t ALL_CALL =0b11100000;

  enum struct InvOutputs : uint8_t
  {
    NotInvOutputs = 0,
    InvOutputs = 1
  };

  /**
 * @brief  PCA9685 Output driver types
 */
  enum struct OutputDriver : uint8_t
  {
    OpenDrain = 0,
    TotemPole = 1
  };

  /**
 * @brief	PCA9685 Not enabled LED outputs defines the behaviour of the outputs
 *			when OE is pulled low
 */
  enum struct OutputNotEn : uint8_t
  {
    OutputNotEn_0 = 0,
    OutputNotEn_OUTDRV = 1,
    OutputNotEn_High_Z1 = 2,
    OutputNotEn_High_Z2 = 3
  };

  /**
 * @brief	PCA9685 Frequency
 *			Set by prescale = round(25 MHz / (4096 * freq)) - 1
 */
  enum struct Frequency : uint8_t
  {
    Frequency_400Hz = 14,
    Frequency_200Hz = 32,
    Frequency_100Hz = 65,
    Frequency_60Hz = 108,
    Frequency_50Hz = 130
  };

  enum struct Device : uint8_t
  {
    Dev00 = DEVICE_ADDRESS_BASE+0,
    Dev01 = DEVICE_ADDRESS_BASE+1,
    Dev02 = DEVICE_ADDRESS_BASE+2,
    Dev03 = DEVICE_ADDRESS_BASE+3,
    Dev04 = DEVICE_ADDRESS_BASE+4,
    Dev05 = DEVICE_ADDRESS_BASE+5,
    Dev06 = DEVICE_ADDRESS_BASE+6,
    Dev07 = DEVICE_ADDRESS_BASE+7,
    Dev08 = DEVICE_ADDRESS_BASE+8,
    Dev09 = DEVICE_ADDRESS_BASE+9,
    Dev10 = DEVICE_ADDRESS_BASE+10,
    Dev11 = DEVICE_ADDRESS_BASE+11,
    Dev12 = DEVICE_ADDRESS_BASE+12,
    Dev13 = DEVICE_ADDRESS_BASE+13,
    Dev14 = DEVICE_ADDRESS_BASE+14,
    Dev15 = DEVICE_ADDRESS_BASE+15,
    Dev16 = DEVICE_ADDRESS_BASE+16,
    Dev17 = DEVICE_ADDRESS_BASE+17,
    Dev18 = DEVICE_ADDRESS_BASE+18,
    Dev19 = DEVICE_ADDRESS_BASE+19,
    Dev20 = DEVICE_ADDRESS_BASE+20,
    Dev21 = DEVICE_ADDRESS_BASE+21,
    Dev22 = DEVICE_ADDRESS_BASE+22,
    Dev23 = DEVICE_ADDRESS_BASE+23,
    Dev24 = DEVICE_ADDRESS_BASE+24,
    Dev25 = DEVICE_ADDRESS_BASE+25,
    Dev26 = DEVICE_ADDRESS_BASE+26,
    Dev27 = DEVICE_ADDRESS_BASE+27,
    Dev28 = DEVICE_ADDRESS_BASE+28,
    Dev29 = DEVICE_ADDRESS_BASE+29,
    Dev30 = DEVICE_ADDRESS_BASE+30,
    Dev31 = DEVICE_ADDRESS_BASE+31,
  };

  enum struct Output : uint16_t
  {
    O00 = 0,
    O01 = 1,
    O02 = 2,
    O03 = 3,
    O04 = 4,
    O05 = 5,
    O06 = 6,
    O07 = 7,
    O08 = 8,
    O09 = 9,
    O10 = 10,
    O11 = 11,
    O12 = 12,
    O13 = 13,
    O14 = 14,
    O15 = 15,

  };

  enum struct Register : uint8_t
  {
	  InputPort0=0,
	  InputPort1=1,
	  OutputPort0=2,
	  OutputPort1=3,
	  PolarityInversionPort0=4,
	  PolarityInversionPort1=5,
	  ConfigurationPort0=6,
	  ConfigurationPort1=7,
  };

  

  class M
  {
  public:                                                                                                                  
    M(i2c_port_t i2c_num, Device device, InvOutputs inv, OutputDriver outdrv, OutputNotEn outne, Frequency freq);
    ErrorCode Setup();
	  static ErrorCode SoftwareReset(i2c_port_t i2c_num);
	  ErrorCode SetOutput(Output Output, uint16_t OnValue, uint16_t OffValue);
	  static ErrorCode SetupStatic(i2c_port_t i2c_num, Device device, InvOutputs inv, OutputDriver outdrv, OutputNotEn outne, Frequency freq);
	  static ErrorCode SetOutputs(i2c_port_t i2c_num, Device device, uint16_t mask, uint16_t dutyCycle);
	  static ErrorCode SetAllOutputs(i2c_port_t i2c_num, Device device, uint16_t dutyCycle);
	  ErrorCode SetOutputFull(Output Output, bool on);
	  ErrorCode SetAll(uint16_t OnValue, uint16_t OffValue);
	  ErrorCode SetDutyCycleForOutput(Output Output, uint16_t val);
  private:
  	int i2c_port;
	  Device device;
	  InvOutputs inv;
	  OutputDriver outdrv;
	  OutputNotEn outne;
	  Frequency freq;
    static uint8_t LEDn_ON_L(uint8_t n) {return (uint8_t)(0x06 + (n)*4);}
    static uint8_t LEDn_ON_H(uint8_t n) {return (uint8_t)(0x07 + (n)*4);}
    static uint8_t LEDn_OFF_L(uint8_t n){return (uint8_t)(0x08 + (n)*4);}
    static uint8_t LEDn_OFF_H(uint8_t n){return (uint8_t)(0x09 + (n)*4);}
  };

};
