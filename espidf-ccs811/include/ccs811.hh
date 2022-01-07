#pragma once
#include <stdint.h>
#include <driver/i2c.h>

namespace CCS811
{
  enum class ADDRESS : uint8_t
  {
    ADDR0 = 0x5A,
    ADDR1 = 0x5B,
  };

  enum class MODE : uint8_t
  {
    // The values for mode in ccs811_start()
    IDLE = 0,
    _1SEC = 1,
    _10SEC = 2,
    _60SEC = 3,
  };
};


// The flags for errstat in ccs811_read()
// ERRSTAT is a merge of two hardware registers: ERROR_ID (bits 15-8) and STATUS (bits 7-0)
// Also bit 1 (which is always 0 in hardware) is set to 1 when an I2C read error occurs
#define CCS811_ERRSTAT_ERROR 0x0001             // There is an error, the ERROR_ID register (0xE0) contains the error source
#define CCS811_ERRSTAT_I2CFAIL 0x0002           // Bit flag added by software: I2C transaction error
#define CCS811_ERRSTAT_DATA_READY 0x0008        // A new data sample is ready in ALG_RESULT_DATA
#define CCS811_ERRSTAT_APP_VALID 0x0010         // Valid application firmware loaded
#define CCS811_ERRSTAT_FW_MODE 0x0080           // Firmware is in application mode (not boot mode)
#define CCS811_ERRSTAT_WRITE_REG_INVALID 0x0100 // The CCS811 received an I²C write request addressed to this station but with invalid register address ID
#define CCS811_ERRSTAT_READ_REG_INVALID 0x0200  // The CCS811 received an I²C read request to a mailbox ID that is invalid
#define CCS811_ERRSTAT_MEASMODE_INVALID 0x0400  // The CCS811 received an I²C request to write an unsupported mode to MEAS_MODE
#define CCS811_ERRSTAT_MAX_RESISTANCE 0x0800    // The sensor resistance measurement has reached or exceeded the maximum range
#define CCS811_ERRSTAT_HEATER_FAULT 0x1000      // The heater current in the CCS811 is not in range
#define CCS811_ERRSTAT_HEATER_SUPPLY 0x2000     // The heater voltage is not being applied correctly

// These flags should not be set. They flag errors.
#define CCS811_ERRSTAT_HWERRORS (CCS811_ERRSTAT_ERROR | CCS811_ERRSTAT_WRITE_REG_INVALID | CCS811_ERRSTAT_READ_REG_INVALID | CCS811_ERRSTAT_MEASMODE_INVALID | CCS811_ERRSTAT_MAX_RESISTANCE | CCS811_ERRSTAT_HEATER_FAULT | CCS811_ERRSTAT_HEATER_SUPPLY)
#define CCS811_ERRSTAT_ERRORS (CCS811_ERRSTAT_I2CFAIL | CCS811_ERRSTAT_HWERRORS)
// These flags should normally be set - after a measurement. They flag data available (and valid app running).
#define CCS811_ERRSTAT_OK (CCS811_ERRSTAT_DATA_READY | CCS811_ERRSTAT_APP_VALID | CCS811_ERRSTAT_FW_MODE)
// These flags could be set after a measurement. They flag data is not yet available (and valid app running).
#define CCS811_ERRSTAT_OK_NODATA (CCS811_ERRSTAT_APP_VALID | CCS811_ERRSTAT_FW_MODE)

class CCS811Manager
{
public:                                                                                                        // Main interface
  CCS811Manager(i2c_port_t i2c_num,  gpio_num_t nwake = (gpio_num_t)GPIO_NUM_NC, CCS811::ADDRESS slaveaddr = CCS811::ADDRESS::ADDR0); // Pin number connected to nWAKE (nWAKE can also be bound to GND, then pass -1), slave address (5A or 5B)
  esp_err_t Init(void);                                                                                        // Reset the CCS811, switch to app mode and check HW_ID. Returns false on problems.
  esp_err_t Start(CCS811::MODE mode);                                                                           // Switch CCS811 to `mode`, use constants CCS811_MODE_XXX. Returns false on I2C problems.
  esp_err_t Read(uint16_t *eco2, uint16_t *etvoc, uint16_t *errstat, uint16_t *raw);                           // Get measurement results from the CCS811 (all args may be NULL), check status via errstat, e.g. ccs811_errstat(errstat)
  const char *errstat_str(uint16_t errstat);                                                                   // Returns a string version of an errstat. Note, each call, this string is updated.                                                                                                      // Extra interface
  int hardware_version(void);                                                                                  // Gets version of the CCS811 hardware (returns -1 on I2C failure).
  int bootloader_version(void);                                                                                // Gets version of the CCS811 bootloader (returns -1 on I2C failure).
  int application_version(void);                                                                               // Gets version of the CCS811 application (returns -1 on I2C failure).
  int get_errorid(void);                                                                                       // Gets the ERROR_ID [same as 'err' part of 'errstat' in 'read'] (returns -1 on I2C failure).
  bool set_envdata(uint16_t t, uint16_t h);                                                                    // Writes t and h to ENV_DATA (see datasheet for CCS811 format). Returns false on I2C problems.
  bool set_envdata210(uint16_t t, uint16_t h);                                                                 // Writes t and h (in ENS210 format) to ENV_DATA. Returns false on I2C problems.
  bool set_envdata_Celsius_percRH(float t, float h);                                                           // Writes t (in Celsius) and h (in percentage RH) to ENV_DATA. Returns false on I2C problems.
  bool get_baseline(uint16_t *baseline);                                                                       // Reads (encoded) baseline from BASELINE. Returns false on I2C problems. Get it, just before power down (but only when sensor was on at least 20min) - see CCS811_AN000370.
  bool set_baseline(uint16_t baseline);                                                                        // Writes (encoded) baseline to BASELINE. Returns false on I2C problems. Set it, after power up (and after 20min).
  bool flash(const uint8_t *image, int size);                                                                  // Flashes the firmware of the CCS811 with size bytes from image - image _must_ be in PROGMEM.
protected:                                                                                                     // Helper interface: nwake pin
  void wake_init(void);                                                                                        // Configure nwake pin for output. If nwake<0 (in constructor), then CCS811 nWAKE pin is assumed not connected to a pin of the host, so host will perform no action.
  void wake_up(void);                                                                                          // Wake up CCS811, i.e. pull nwake pin low.
  void wake_down(void);                                                                                        // CCS811 back to sleep, i.e. pull nwake pin high.
private:
  int i2c_port;
  gpio_num_t nwake;         // Pin number for nWAKE pin (or -1).
  CCS811::ADDRESS slaveaddr; // I2C slave address of the CCS811.
  int _appversion;          // Version of the app firmware inside the CCS811 (for workarounds).
  bool i2cwrite(int regaddr, int count, uint8_t *buf);
  bool i2cread(int regaddr, int count, uint8_t *buf);
};
