#include <inttypes.h>
#include <i2c.hh>
#include "ccs811.hh"
#include <esp_log.h>
#include <string.h>
#define TAG "CCS811"

// Timings
#define CCS811_WAIT_AFTER_RESET_US 2000    // The CCS811 needs a wait after reset
#define CCS811_WAIT_AFTER_APPSTART_US 1000 // The CCS811 needs a wait after app start
#define CCS811_WAIT_AFTER_WAKE_US 50       // The CCS811 needs a wait after WAKE signal
#define CCS811_WAIT_AFTER_APPERASE_MS 500  // The CCS811 needs a wait after app erase (300ms from spec not enough)
#define CCS811_WAIT_AFTER_APPVERIFY_MS 70  // The CCS811 needs a wait after app verify
#define CCS811_WAIT_AFTER_APPDATA_MS 50    // The CCS811 needs a wait after writing app data

// Main interface =====================================================================================================

// CCS811 registers/mailboxes, all 1 byte except when stated otherwise
#define CCS811_STATUS 0x00
#define CCS811_MEAS_MODE 0x01
#define CCS811_ALG_RESULT_DATA 0x02 // up to 8 bytes
#define CCS811_RAW_DATA 0x03        // 2 bytes
#define CCS811_ENV_DATA 0x05        // 4 bytes
#define CCS811_THRESHOLDS 0x10      // 5 bytes
#define CCS811_BASELINE 0x11        // 2 bytes
#define CCS811_HW_ID 0x20
#define CCS811_HW_VERSION 0x21
#define CCS811_FW_BOOT_VERSION 0x23 // 2 bytes
#define CCS811_FW_APP_VERSION 0x24  // 2 bytes
#define CCS811_ERROR_ID 0xE0
#define CCS811_APP_ERASE 0xF1  // 4 bytes
#define CCS811_APP_DATA 0xF2   // 9 bytes
#define CCS811_APP_VERIFY 0xF3 // 0 bytes
#define CCS811_APP_START 0xF4  // 0 bytes
#define CCS811_SW_RESET 0xFF   // 4 bytes

// Pin number connected to nWAKE (nWAKE can also be bound to GND, then pass -1), slave address (5A or 5B)
CCS811Manager::CCS811Manager(i2c_port_t i2c_port, gpio_num_t nwake, CCS811::ADDRESS slaveaddr) : i2c_port(i2c_port), nwake(nwake), slaveaddr(slaveaddr)
{
}

// Reset the CCS811, switch to app mode and check HW_ID. Returns false on problems.
esp_err_t CCS811Manager::Init(void)
{
  uint8_t sw_reset[] = {0x11, 0xE5, 0x72, 0x8A};
  uint8_t app_start[] = {};
  uint8_t hw_id;
  uint8_t hw_version;
  uint8_t app_version[2];
  uint8_t status;
  esp_err_t ok;
  wake_init();
  // Wakeup CCS811
  wake_up();

  // Try to ping CCS811 (can we reach CCS811 via I2C?)
  esp_err_t err = I2C::IsAvailable(this->i2c_port, (uint8_t)this->slaveaddr);
  if (err != ESP_OK)
  {
    // Try the other slave address
    err = I2C::IsAvailable(this->i2c_port, (uint8_t)(CCS811::ADDRESS::ADDR0) + (uint8_t)(CCS811::ADDRESS::ADDR1) - (uint8_t)(this->slaveaddr));
    if (err == ESP_OK)
    {
      ESP_LOGE(TAG, "wrong slave address, ping successful on other address");
    }
    else
    {
      ESP_LOGE(TAG, "ping failed (VDD/GND connected? SDA/SCL connected?)");
    }
    goto abort_begin;
  }

  // Invoke a SW reset (bring CCS811 in a know state)
  ok = i2cwrite(CCS811_SW_RESET, 4, sw_reset);
  if (!ok)
  {
    ESP_LOGE(TAG, "reset failed");
    goto abort_begin;
  }
  vTaskDelay(pdMS_TO_TICKS(100));

  // Check that HW_ID is 0x81
  ok = i2cread(CCS811_HW_ID, 1, &hw_id);
  if (!ok)
  {
    ESP_LOGE(TAG, "HW_ID read failed");
    goto abort_begin;
  }
  if (hw_id != 0x81)
  {
    ESP_LOGE(TAG, "Wrong HW_ID: 0x%02X", hw_id);
    goto abort_begin;
  }

  // Check that HW_VERSION is 0x1X
  ok = i2cread(CCS811_HW_VERSION, 1, &hw_version);
  if (!ok)
  {
    ESP_LOGE(TAG, "HW_VERSION read failed");
    goto abort_begin;
  }
  if ((hw_version & 0xF0) != 0x10)
  {
    ESP_LOGE(TAG, "Wrong HW_VERSION: 0x%02X", hw_version);
    goto abort_begin;
  }

  // Check status (after reset, CCS811 should be in boot mode with valid app)
  ok = i2cread(CCS811_STATUS, 1, &status);
  if (!ok)
  {
    ESP_LOGE(TAG, "STATUS read (boot mode) failed");
    goto abort_begin;
  }
  if (status != 0x10)
  {
    ESP_LOGE(TAG, "Not in boot mode, or no valid app: 0x%02X", status);
    goto abort_begin;
  }

  // Read the application version
  ok = i2cread(CCS811_FW_APP_VERSION, 2, app_version);
  if (!ok)
  {
    ESP_LOGE(TAG, "APP_VERSION read failed");
    goto abort_begin;
  }
  _appversion = app_version[0] * 256 + app_version[1];

  // Switch CCS811 from boot mode into app mode
  ok = i2cwrite(CCS811_APP_START, 0, app_start);
  if (!ok)
  {
    ESP_LOGE(TAG, "Goto app mode failed");
    goto abort_begin;
  }
  vTaskDelay(pdMS_TO_TICKS(10));

  // Check if the switch was successful
  ok = i2cread(CCS811_STATUS, 1, &status);
  if (!ok)
  {
    ESP_LOGE(TAG, "STATUS read (app mode) failed");
    goto abort_begin;
  }
  if (status != 0x90)
  {
    ESP_LOGE(TAG, "Not in app mode, or no valid app: 0x%02X", status);
    goto abort_begin;
  }

  // CCS811 back to sleep
  wake_down();
  // Return success
  return ESP_OK;

abort_begin:
  // CCS811 back to sleep
  wake_down();
  // Return failure
  return ESP_FAIL;
}

// Switch CCS811 to `mode`, use constants CCS811_MODE_XXX. Returns false on I2C problems.
esp_err_t CCS811Manager::Start(CCS811::MODE mode)
{
  uint8_t meas_mode[] = {(uint8_t)((uint8_t)(mode) << 4)};
  wake_up();
  bool ok = i2cwrite(CCS811_MEAS_MODE, 1, meas_mode);
  wake_down();
  if (ok)
  {
    return ESP_OK;
  }
  return ESP_FAIL;
}

// Get measurement results from the CCS811 (all args may be NULL), check status via errstat, e.g. ccs811_errstat(errstat)
esp_err_t CCS811Manager::Read(uint16_t *eco2, uint16_t *etvoc, uint16_t *errstat, uint16_t *raw)
{
  bool ok;
  uint8_t buf[8];
  uint8_t stat;
  wake_up();
  if (_appversion < 0x2000)
  {
    ok = i2cread(CCS811_STATUS, 1, &stat); // CCS811 with pre 2.0.0 firmware has wrong STATUS in CCS811_ALG_RESULT_DATA
    if (ok && stat == CCS811_ERRSTAT_OK)
      ok = i2cread(CCS811_ALG_RESULT_DATA, 8, buf);
    else
      buf[5] = 0;
    buf[4] = stat; // Update STATUS field with correct STATUS
  }
  else
  {
    ok = i2cread(CCS811_ALG_RESULT_DATA, 8, buf);
  }
  wake_down();
  // Status and error management
  uint16_t combined = buf[5] * 256 + buf[4];
  if (combined & ~(CCS811_ERRSTAT_HWERRORS | CCS811_ERRSTAT_OK))
    ok = false;                                            // Unused bits are 1: I2C transfer error
  combined &= CCS811_ERRSTAT_HWERRORS | CCS811_ERRSTAT_OK; // Clear all unused bits
  if (!ok)
    combined |= CCS811_ERRSTAT_I2CFAIL;
  // Clear ERROR_ID if flags are set
  if (combined & CCS811_ERRSTAT_HWERRORS)
  {
    int err = get_errorid();
    if (err == -1)
      combined |= CCS811_ERRSTAT_I2CFAIL; // Propagate I2C error
  }
  // Outputs
  if (eco2)
    *eco2 = buf[0] * 256 + buf[1];
  if (etvoc)
    *etvoc = buf[2] * 256 + buf[3];
  if (errstat)
    *errstat = combined;
  if (raw)
    *raw = buf[6] * 256 + buf[7];
  return ESP_OK;
}

// Returns a string version of an errstat. Note, each call, this string is updated.
const char *CCS811Manager::errstat_str(uint16_t errstat)
{
  static char s[17]; // 16 bits plus terminating zero
                     // First the ERROR_ID flags
  s[0] = '-';
  s[1] = '-';
  if (errstat & CCS811_ERRSTAT_HEATER_SUPPLY)
    s[2] = 'V';
  else
    s[2] = 'v';
  if (errstat & CCS811_ERRSTAT_HEATER_FAULT)
    s[3] = 'H';
  else
    s[3] = 'h';
  if (errstat & CCS811_ERRSTAT_MAX_RESISTANCE)
    s[4] = 'X';
  else
    s[4] = 'x';
  if (errstat & CCS811_ERRSTAT_MEASMODE_INVALID)
    s[5] = 'M';
  else
    s[5] = 'm';
  if (errstat & CCS811_ERRSTAT_READ_REG_INVALID)
    s[6] = 'R';
  else
    s[6] = 'r';
  if (errstat & CCS811_ERRSTAT_WRITE_REG_INVALID)
    s[7] = 'W';
  else
    s[7] = 'w';
  // Then the STATUS flags
  if (errstat & CCS811_ERRSTAT_FW_MODE)
    s[8] = 'F';
  else
    s[8] = 'f';
  s[9] = '-';
  s[10] = '-';
  if (errstat & CCS811_ERRSTAT_APP_VALID)
    s[11] = 'A';
  else
    s[11] = 'a';
  if (errstat & CCS811_ERRSTAT_DATA_READY)
    s[12] = 'D';
  else
    s[12] = 'd';
  s[13] = '-';
  // Next bit is used by SW to signal I2C transfer error
  if (errstat & CCS811_ERRSTAT_I2CFAIL)
    s[14] = 'I';
  else
    s[14] = 'i';
  if (errstat & CCS811_ERRSTAT_ERROR)
    s[15] = 'E';
  else
    s[15] = 'e';
  s[16] = '\0';
  return s;
}

// Extra interface ========================================================================================

// Gets version of the CCS811 hardware (returns -1 on I2C failure).
int CCS811Manager::hardware_version(void)
{
  uint8_t buf[1];
  wake_up();
  bool ok = i2cread(CCS811_HW_VERSION, 1, buf);
  wake_down();
  int version = -1;
  if (ok)
    version = buf[0];
  return version;
}

// Gets version of the CCS811 bootloader (returns -1 on I2C failure).
int CCS811Manager::bootloader_version(void)
{
  uint8_t buf[2];
  wake_up();
  bool ok = i2cread(CCS811_FW_BOOT_VERSION, 2, buf);
  wake_down();
  int version = -1;
  if (ok)
    version = buf[0] * 256 + buf[1];
  return version;
}

// Gets version of the CCS811 application (returns -1 on I2C failure).
int CCS811Manager::application_version(void)
{
  uint8_t buf[2];
  wake_up();
  bool ok = i2cread(CCS811_FW_APP_VERSION, 2, buf);
  wake_down();
  int version = -1;
  if (ok)
    version = buf[0] * 256 + buf[1];
  return version;
}

// Gets the ERROR_ID [same as 'err' part of 'errstat' in 'read'] (returns -1 on I2C failure).
// Note, this actually clears CCS811_ERROR_ID (hardware feature)
int CCS811Manager::get_errorid(void)
{
  uint8_t buf[1];
  wake_up();
  bool ok = i2cread(CCS811_ERROR_ID, 1, buf);
  wake_down();
  int version = -1;
  if (ok)
    version = buf[0];
  return version;
}

#define HI(u16) ((uint8_t)(((u16) >> 8) & 0xFF))
#define LO(u16) ((uint8_t)(((u16) >> 0) & 0xFF))

// Writes t and h to ENV_DATA (see datasheet for CCS811 format). Returns false on I2C problems.
bool CCS811Manager::set_envdata(uint16_t t, uint16_t h)
{
  uint8_t envdata[] = {HI(h), LO(h), HI(t), LO(t)};
  wake_up();
  // Serial.print(" [T="); Serial.print(t); Serial.print(" H="); Serial.print(h); Serial.println("] ");
  bool ok = i2cwrite(CCS811_ENV_DATA, 4, envdata);
  wake_down();
  return ok;
}

// Writes t and h (in ENS210 format) to ENV_DATA. Returns false on I2C problems.
bool CCS811Manager::set_envdata210(uint16_t t, uint16_t h)
{
  // Humidity formats of ENS210 and CCS811 are equal, we only need to map temperature.
  // The lowest and highest (raw) ENS210 temperature values the CCS811 can handle
  uint16_t lo = 15882; // (273.15-25)*64 = 15881.6 (float to int error is 0.4)
  uint16_t hi = 24073; // 65535/8+lo = 24073.875 (24074 would map to 65539, so overflow)
  // Check if ENS210 temperature is within CCS811 range, if not clip, if so map
  bool ok;
  if (t < lo)
    ok = set_envdata(0, h);
  else if (t > hi)
    ok = set_envdata(65535, h);
  else
    ok = set_envdata((t - lo) * 8 + 3, h); // error in 'lo' is 0.4; times 8 is 3.2; so we correct 3
  // Returns I2C transaction status
  return ok;
}

// Writes t (in Celsius) and h (in percentage RH) to ENV_DATA. Returns false on I2C problems.
bool CCS811Manager::set_envdata_Celsius_percRH(float t, float h)
{
  // We need to map the floats to CCS811 format.
  // Humidity needs to be written as an unsigned 16 bits in 1/512%RH, e.g. 48.5%RH must be written as 0x6100.
  // Temperature is also unsigned 16 bits in 1/512 degrees; but with an offset of 25°C, e.g. 23.5% maps to 0x6100.
  bool ok = set_envdata((t + 25.0) * 512, h * 512);
  // Returns I2C transaction status
  return ok;
}

// Reads (encoded) baseline from BASELINE. Returns false on I2C problems.
// Get it, just before power down (but only when sensor was on at least 20min) - see CCS811_AN000370.
bool CCS811Manager::get_baseline(uint16_t *baseline)
{
  uint8_t buf[2];
  wake_up();
  bool ok = i2cread(CCS811_BASELINE, 2, buf);
  wake_down();
  *baseline = (buf[0] << 8) + buf[1];
  return ok;
}

// Writes (encoded) baseline to BASELINE. Returns false on I2C problems. Set it, after power up (and after 20min).
bool CCS811Manager::set_baseline(uint16_t baseline)
{
  uint8_t buf[] = {HI(baseline), LO(baseline)};
  wake_up();
  bool ok = i2cwrite(CCS811_BASELINE, 2, buf);
  wake_down();
  return ok;
}

// Flashes the firmware of the CCS811 with size bytes from image - image _must_ be in PROGMEM.
bool CCS811Manager::flash(const uint8_t *image, int size)
{
  uint8_t sw_reset[] = {0x11, 0xE5, 0x72, 0x8A};
  uint8_t app_erase[] = {0xE7, 0xA7, 0xE6, 0x09};
  uint8_t app_verify[] = {};
  uint8_t status;
  int count;
  bool ok;
  wake_up();

  // Try to ping CCS811 (can we reach CCS811 via I2C?)
  ESP_LOGI(TAG, "ping ");
  ok = i2cwrite(0, 0, 0);
  if (!ok)
  {
    ESP_LOGE(TAG, "FAILED");
    goto abort_begin;
  }
  ESP_LOGI(TAG, "ok");

  // Invoke a SW reset (bring CCS811 in a know state)
  ESP_LOGI(TAG, "reset ");
  ok = i2cwrite(CCS811_SW_RESET, 4, sw_reset);
  if (!ok)
  {
    ESP_LOGE(TAG, "FAILED");
    goto abort_begin;
  }
  vTaskDelay(1);
  ESP_LOGI(TAG, "ok");

  // Check status (after reset, CCS811 should be in boot mode with or without valid app)
  ESP_LOGI(TAG, "status (reset1) ");
  ok = i2cread(CCS811_STATUS, 1, &status);
  if (!ok)
  {
    ESP_LOGE(TAG, "FAILED");
    goto abort_begin;
  }
  ESP_LOGI(TAG, "0x%02X", status);
  if (status != 0x00 && status != 0x10)
  {
    ESP_LOGE(TAG, "ERROR - ignoring"); // Seems to happens when there is no valid app
  }
  else
  {
    ESP_LOGI(TAG, "ok");
  }

  // Invoke app erase
  ESP_LOGI(TAG, "app-erase ");
  ok = i2cwrite(CCS811_APP_ERASE, 4, app_erase);
  if (!ok)
  {
    ESP_LOGE(TAG, "FAILED");
    goto abort_begin;
  }
  vTaskDelay(pdMS_TO_TICKS(CCS811_WAIT_AFTER_APPERASE_MS));
  ESP_LOGI(TAG, "ok");

  // Check status (CCS811 should be in boot mode without valid app, with erase completed)
  ESP_LOGI(TAG, "status (app-erase) ");
  ok = i2cread(CCS811_STATUS, 1, &status);
  if (!ok)
  {
    ESP_LOGE(TAG, "FAILED");
    goto abort_begin;
  }
  ESP_LOGI(TAG, "0x%02X", status);
  if (status != 0x40)
  {
    ESP_LOGE(TAG, "ERROR");
    goto abort_begin;
  }
  ESP_LOGI(TAG, "ok");

  // Write all blocks
  count = 0;
  while (size > 0)
  {
    if (count % 64 == 0)
    {
      ESP_LOGI(TAG, "writing %d", size);
    }
    int len = size < 8 ? size : 8;
    // Copy PROGMEM to RAM
    uint8_t ram[8];
    memcpy(ram, image, len);
    // Send 8 bytes from RAM to CCS811
    ok = i2cwrite(CCS811_APP_DATA, len, ram);
    if (!ok)
    {
      ESP_LOGE(TAG, "app data FAILED");
      goto abort_begin;
    }
    ESP_LOGI(TAG, ".");
    vTaskDelay(pdMS_TO_TICKS(CCS811_WAIT_AFTER_APPDATA_MS));
    image += len;
    size -= len;
    count++;
    if (count % 64 == 0)
    {
      ESP_LOGI(TAG, " %d", size);
    }
  }
  if (count % 64 != 0)
  {
    ESP_LOGI(TAG, " %d", size);
  }

  // Invoke app verify
  ESP_LOGI(TAG, "app-verify ");
  ok = i2cwrite(CCS811_APP_VERIFY, 0, app_verify);
  if (!ok)
  {
    ESP_LOGE(TAG, "FAILED");
    goto abort_begin;
  }
  vTaskDelay(pdMS_TO_TICKS(CCS811_WAIT_AFTER_APPVERIFY_MS));
  ESP_LOGI(TAG, "ok");

  // Check status (CCS811 should be in boot mode with valid app, and erased and verified)
  ESP_LOGI(TAG, "status (app-verify) ");
  ok = i2cread(CCS811_STATUS, 1, &status);
  if (!ok)
  {
    ESP_LOGE(TAG, "FAILED");
    goto abort_begin;
  }
  ESP_LOGI(TAG, "0x%02X", status);
  if (status != 0x30)
  {
    ESP_LOGE(TAG, "ERROR");
    goto abort_begin;
  }
  ESP_LOGI(TAG, "ok");

  // Invoke a second SW reset (clear flashing flags)
  ESP_LOGI(TAG, "reset2 ");
  ok = i2cwrite(CCS811_SW_RESET, 4, sw_reset);
  if (!ok)
  {
    ESP_LOGE(TAG, "FAILED");
    goto abort_begin;
  }
  vTaskDelay(1);
  ESP_LOGI(TAG, "ok");

  // Check status (after reset, CCS811 should be in boot mode with valid app)
  ESP_LOGI(TAG, "status (reset2) ");
  ok = i2cread(CCS811_STATUS, 1, &status);
  if (!ok)
  {
    ESP_LOGE(TAG, "FAILED");
    goto abort_begin;
  }
  ESP_LOGI(TAG, "0x%02X", status);
  if (status != 0x10)
  {
    ESP_LOGE(TAG, "ERROR");
    goto abort_begin;
  }
  ESP_LOGI(TAG, "ok");

  // CCS811 back to sleep
  wake_down();
  // Return success
  return true;

abort_begin:
  // CCS811 back to sleep
  wake_down();
  // Return failure
  return false;
}

// Helper interface: nwake pin ========================================================================================

// Configure nwake pin for output.
// If nwake<0 (in constructor), then CCS811 nWAKE pin is assumed not connected to a pin of the host, so host will perform no action.
void CCS811Manager::wake_init(void)
{
  if (nwake == GPIO_NUM_NC)
    return;
  gpio_reset_pin(nwake);
  gpio_set_direction(nwake, GPIO_MODE_OUTPUT);
}

// Wake up CCS811, i.e. pull nwake pin low.
void CCS811Manager::wake_up(void)
{
  if (nwake == GPIO_NUM_NC)
    return;
  gpio_set_level(nwake, 0);
  vTaskDelay(1);
}

// CCS811 back to sleep, i.e. pull nwake pin high.
void CCS811Manager::wake_down(void)
{
  if (nwake == GPIO_NUM_NC)
    return;
  gpio_set_level(nwake, 1);
}

// Helper interface: i2c wrapper ======================================================================================

// Writes `count` from `buf` to register at address `regaddr` in the CCS811. Returns false on I2C problems.
bool CCS811Manager::i2cwrite(int regaddr, int count, uint8_t *buf)
{
  return I2C::WriteReg(this->i2c_port, (uint8_t)this->slaveaddr, regaddr, buf, count)==ESP_OK;
}

// Reads 'count` bytes from register at address `regaddr`, and stores them in `buf`. Returns false on I2C problems.
bool CCS811Manager::i2cread(int regaddr, int count, uint8_t *buf)
{
  return I2C::ReadReg(this->i2c_port, (uint8_t)this->slaveaddr, regaddr, buf, count)==ESP_OK;
}
