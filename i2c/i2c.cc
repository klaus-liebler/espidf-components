
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <array>
#include "i2c.hh"

static const char *TAG = "I2C_DEV";

SemaphoreHandle_t I2C::locks[I2C_NUM_MAX];

esp_err_t I2C::Init(i2c_port_t port, gpio_num_t scl, gpio_num_t sda)
{
    i2c_config_t conf;
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = sda;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_io_num = scl;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = 100000;
    conf.clk_flags = 0;
    ESP_ERROR_CHECK(i2c_param_config(port, &conf));
    ESP_ERROR_CHECK(i2c_driver_install(port, conf.mode, 0, 0, 0));
    I2C::locks[(int)port] = xSemaphoreCreateMutex();
    return ESP_OK;
}

iI2CPort* I2C::GetPort_DoNotForgetToDelete(const i2c_port_t port){
        return new iI2CPort_Impl(port);
    }

esp_err_t I2C::WriteReg(const i2c_port_t port, const uint8_t address7bit, const uint8_t reg_addr, const uint8_t *const reg_data, const size_t len)
{
    if (!xSemaphoreTake(locks[port], pdMS_TO_TICKS(1000)))
    {
        ESP_LOGE(TAG, "Could not take port mutex %d", port);
        return ESP_ERR_TIMEOUT;
    }
    if (!reg_data)
    {
        ESP_LOGE(TAG, "Data is NULL for address 0x%02X on port %d", address7bit, port);
        return ESP_FAIL;
    }
    uint8_t write_buf[1+len];
    write_buf[0]=reg_addr;
    memcpy(write_buf+1, reg_data, len);
    esp_err_t espRc = i2c_master_write_to_device(port, address7bit, write_buf, sizeof(write_buf), pdMS_TO_TICKS(1000));
    xSemaphoreGive(locks[port]);
    if (espRc != ESP_OK)
    {
        ESP_LOGE(TAG, "Error %d", espRc);
    }
    return espRc;
}

esp_err_t I2C::WriteSingleReg(const i2c_port_t port, const uint8_t address7bit, const uint8_t reg_addr, const uint8_t reg_data){
    
    if (!xSemaphoreTake(locks[port], pdMS_TO_TICKS(1000)))
    {
        ESP_LOGE(TAG, "Could not take port mutex %d", port);
        return ESP_ERR_TIMEOUT;
    }
    uint8_t write_buf[2] = {reg_addr, reg_data};
    esp_err_t ret = i2c_master_write_to_device(port, address7bit, write_buf, sizeof(write_buf), pdMS_TO_TICKS(1000));
    xSemaphoreGive(locks[port]);
    return ret;
}

esp_err_t I2C::ReadReg(const i2c_port_t port, uint8_t address7bit, uint8_t reg_addr, uint8_t *reg_data, size_t len)
{
    if (!xSemaphoreTake(locks[port], pdMS_TO_TICKS(1000)))
    {
        ESP_LOGE(TAG, "Could not take port mutex %d", port);
        return ESP_ERR_TIMEOUT;
    }
    esp_err_t espRc = i2c_master_write_read_device(port, address7bit, &reg_addr, 1, reg_data, len, pdMS_TO_TICKS(1000));
    xSemaphoreGive(locks[port]);
    return espRc;
}

esp_err_t I2C::ReadReg16(const i2c_port_t port, uint8_t address7bit, uint16_t reg_addr16, uint8_t *reg_data, size_t len)
{
    if (!xSemaphoreTake(locks[port], pdMS_TO_TICKS(1000)))
    {
        ESP_LOGE(TAG, "Could not take port mutex %d", port);
        return ESP_ERR_TIMEOUT;
    }
    esp_err_t espRc;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (address7bit << 1) | I2C_MASTER_WRITE, true);

    i2c_master_write(cmd, (uint8_t *)&reg_addr16, 2, true);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (address7bit << 1) | I2C_MASTER_READ, true);
    i2c_master_read(cmd, reg_data, len, I2C_MASTER_LAST_NACK);
    i2c_master_stop(cmd);
    espRc = i2c_master_cmd_begin(port, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);
    xSemaphoreGive(locks[port]);
    return espRc;
}

esp_err_t I2C::IsAvailable(const i2c_port_t port, uint8_t address7bit)
{

    if (!xSemaphoreTake(locks[port], pdMS_TO_TICKS(1000)))
    {
        ESP_LOGE(TAG, "Could not take port mutex %d", port);
        return ESP_ERR_TIMEOUT;
    }
    // Nothing to init. Just check if it is there...
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (address7bit << 1) | I2C_MASTER_WRITE, true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(port, cmd, pdMS_TO_TICKS(50));
    i2c_cmd_link_delete(cmd);
    xSemaphoreGive(locks[port]);
    return ret;
}

constexpr std::array<const char *, 128> address2name{
    "reserved", // 0
    "reserved",
    "reserved",
    "reserved",
    "reserved",
    "reserved",
    "reserved",
    "reserved",
    "reserved",
    "reserved",
    "reserved", // 0x0a
    "reserved",
    "AK8975",
    "AK8975",
    "MAG3110 AK8975 IST-8310",
    "AK8975",
    "VEML6075 VEML7700 VML6075 LM25066", // 0x10
    "SAA5243P/K SAA5243P/E SAA5243P/H LM25066 SAA5243P/L SAA5246 Si4713",
    "SEN-17374 PMSA003I LM25066",
    "VCNL40x0 SEN-17374 LM25066",
    "LM25066",
    "LM25066",
    "LM25066",
    "LM25066",
    "MCP9808 LIS3DH LSM303 COM-15093 47L04/47C04/47L16/47C16",
    "MCP9808 LIS3DH LSM303 COM-15093",
    "MCP9808 47L04/47C04/47L16/47C16",
    "MCP9808",
    "MCP9808 MMA845x FXOS8700 47L04/47C04/47L16/47C16",
    "MCP9808 MMA845x ADXL345 FXOS8700",
    "MCP9808 FXOS8700 HMC5883 LSM303 LSM303 47L04/47C04/47L16/47C16",
    "MCP9808 FXOS8700",
    "Chirp! HW-061 FXAS21002 PCA955x MA12070P MCP23017 MCP23008",
    "SAA4700 HW-061 FXAS21002 PCA955x MA12070P MCP23017 MCP23008",
    "PCA1070 HW-061 PCA955x MA12070P MCP23017 MCP23008",
    "BH1750FVI SAA4700 HW-061 PCA955x MA12070P MCP23017 MCP23008",
    "PCD3312C PCD3311C HW-061 PCA955x MCP23017 MCP23008",
    "MCP23008 MCP23017 PCD3311C PCD3312C PCA955x HW-061",
    "MCP23008 MCP23017 PCA955x HW-061",
    "MCP23008 MCP23017 HIH6130 PCA955x HW-061",
    "DS3502 DS1841 BNO055 FS3000 DS1881 CAP1188",
    "VL6180X DS3502 DS1841 BNO055 VL53L0x TCS34725 TSL2591 DS1881 CAP1188",
    "CAP1188 DS1841 DS3502 DS1881",
    "CAP1188 DS1841 DS3502 DS1881",
    "CAP1188 AD5248 AD5251 AD5252 CAT5171 DS1881",
    "CAT5171 AD5248 AD5252 AD5251 DS1881 CAP1188",
    "AD5248 AD5251 AD5252 LPS22HB DS1881",
    "AD5248 AD5243 AD5251 AD5252 DS1881",
    "SAA2502",
    "SAA2502",
    "UNKNOWN",
    "MLX90640",
    "UNKNOWN",
    "UNKNOWN",
    "MAX17048", // 0x36
    "UNKNOWN",
    "FT6x06 SEN-15892 BMA150 AHT10 SAA1064 VEML6070 PCF8574AP",
    "TSL2561 APDS-9960 VEML6070 SAA1064 PCF8574AP",
    "PCF8577C SAA1064 PCF8574AP",
    "SAA1064 PCF8569 PCF8574AP",
    "SSD1306 SH1106 PCF8569 PCF8578 PCF8574AP SSD1305",
    "SSD1306 SH1106 PCF8578 PCF8574AP SSD1305",
    "PCF8574AP BU9796",
    "PCF8574AP",
    "Si7021 HTU21D-F TMP007 TMP006 PCA9685 INA219 TEA6330 TEA6300 TDA9860 TEA6320 TDA8421 NE5751 INA260 PCF8574 HDC1080 LM25066",
    "TMP007 TMP006 PCA9685 INA219 STMPE610 STMPE811 TDA8426 TDA9860 TDA8424 TDA8421 TDA8425 NE5751 INA260 PCF8574 LM25066",
    "INA219 TDA8417 PCF8574 TDA8415 INA260 PCA9685 LM25066 HDC1008 TMP007 TMP006",
    "INA219 PCF8574 INA260 PCA9685 LM25066 HDC1008 TMP007 TMP006",
    "TMP007 TMP006 PCA9685 INA219 STMPE610 SHT31 ISL29125 STMPE811 TDA4688 TDA4672 TDA4780 TDA4670 TDA8442 TDA4687 TDA4671 TDA4680 INA260 PCF8574 LM25066",
    "INA219 TDA8376 TDA7433 PCF8574 INA260 PCA9685 LM25066 SHT31 TMP007 TMP006",
    "INA219 PCF8574 TDA8370 INA260 PCA9685 LM25066 TDA9150 TMP007 TMP006",
    "INA219 PCF8574 INA260 PCA9685 LM25066 TMP007 TMP006",
    "INA219 PCF8574 INA260 PCA9685 ADS1115 LM75b PN532 TMP102 ADS7828",
    "TSL2561 INA219 AS7262 PCF8574 INA260 PCA9685 ADS1115 LM75b TMP102 ADS7828",
    "INA219 PCF8574 MAX44009 INA260 PCA9685 ADS1115 LM75b TMP102 ADS7828",
    "INA219 PCF8574 MAX44009 INA260 PCA9685 ADS1115 LM75b TMP102 ADS7828",
    "PCA9685 INA219 INA260 PCF8574 LM75b EMC2101",
    "PCA9685 INA219 INA260 PCF8574 LM75b",
    "PCA9685 INA219 INA260 PCF8574 LM75b",
    "PCA9685 INA219 INA260 PCF8574 LM75b",
    "FS1015 MB85RC 47L04/47C04/47L16/47C16 PCA9685 CAT24C512 LM25066",
    "PCA9685 MB85RC CAT24C512 VCNL4200 LM25066 PCF8563",
    "SI1133 MB85RC Nunchuck controller 47L04/47C04/47L16/47C16 PCA9685 CAT24C512 LM25066 APDS-9250",
    "ADXL345 PCA9685 MB85RC CAT24C512 LM25066",
    "PCA9685 MB85RC CAT24C512 LM25066 47L04/47C04/47L16/47C16",
    "SI1133 MB85RC PCA9685 CAT24C512 LM25066 MAX30101",
    "PCA9685 MB85RC CAT24C512 LM25066 47L04/47C04/47L16/47C16",
    "PCA9685 MB85RC MAX3010x CAT24C512 LM25066",
    "PCA9685 TPA2016 SGP30 LM25066",
    "PCA9685 LM25066",
    "PCA9685 LM25066 MLX90614 DRV2605 MPR121 CCS811 CCS811",
    "PCA9685 CCS811 MPR121 CCS811",
    "PCA9685 AM2315 MPR121 BH1750FVI",
    "PCA9685 MPR121 SFA30",
    "PCA9685",
    "PCA9685 HTS221",
    "PCA9685 MPL115A2 MPL3115A2 Si5351A Si1145 MCP4725A0 TEA5767 TSA5511 SAB3037 SAB3035 MCP4725A1 ATECC508A ATECC608A SI1132 MCP4728",
    "MCP4728 MCP4725A1 SAB3037 TEA6100 PCA9685 TSA5511 SAB3035 MCP4725A0 Si5351A SCD30",
    "SCD41 SCD40-D-R2 MCP4728 MCP4725A1 UMA1014T SCD40 SAB3037 PCA9685 TSA5511 SAB3035",
    "MCP4728 MCP4725A1 UMA1014T SAB3037 PCA9685 TSA5511 SAB3035 Si4713",
    "PCA9685 MCP4725A2 MCP4725A1 MCP4728",
    "PCA9685 MCP4725A2 MCP4725A1 MCP4728",
    "PCA9685 MCP4725A3 IS31FL3731 MCP4725A1 MCP4728",
    "PCA9685 MCP4725A3 MCP4725A1 MCP4728",
    "PCA9685 AMG8833 DS1307 PCF8523 DS3231 MPU-9250 ITG3200 PCF8573 MPU6050 ICM-20948 WITTY PI 3 MCP3422 DS1371 MPU-9250",
    "MPU6050 WITTY PI 3 PCA9685 ICM-20948 PCF8573 ITG3200 MPU-9250 SPS30 MAX31341 AMG8833",
    "PCA9685 L3GD20H PCF8573",
    "PCA9685 L3GD20H PCF8573",
    "PCA9685",
    "PCA9685",
    "PCA9685",
    "PCA9685 MCP7940N",
    "TCA9548A PCA9541 PCA9685 (CALL ALL) HT16K33 TCA9548 SHTC3",
    "PCA9685 TCA9548 HT16K33 PCA9541 TCA9548A",
    "PCA9685 TCA9548 HT16K33 PCA9541 TCA9548A",
    "PCA9685 TCA9548 HT16K33 PCA9541 TCA9548A",
    "PCA9685 TCA9548 HT16K33 PCA9541 TCA9548A",
    "PCA9685 TCA9548 HT16K33 PCA9541 TCA9548A",
    "BME688 TCA9548A SPL06-007 BME280 MS5611 MS5607 PCA9541 PCA9685 HT16K33 BME680 BMP280 TCA9548",
    "PCA9685 TCA9548 HT16K33 IS31FL3731 BME280 BMP280 MS5607 BMP180 BMP085 BMA180 MS5611 BME680 BME688 PCA9541 SPL06-007 TCA9548A",
    "PCA9685 (res.)",
    "PCA9685 (res.)",
    "PCA9685 (res.)",
    "PCA9685 (res.)",
    "PCA9685 (res.)",
    "PCA9685 (res.)",
    "PCA9685 (res.)",
    "PCA9685 (res.)",
};
esp_err_t I2C::Discover(const i2c_port_t port){
    return Discover(port, nullptr);
}
esp_err_t I2C::Discover(const i2c_port_t port, FILE* fp)
{
    int cnt = 0;
    for (uint8_t addr = 1; addr < 128; addr++)
    {
        if (IsAvailable(port, addr) == ESP_OK)
        {
            ESP_LOGI(TAG, "Bus %d: Found %s  at 0x%02X ", (int)port, address2name[addr], addr);
            if(fp)fprintf(fp, "Bus %d: Found %s  at 0x%02X ", (int)port, address2name[addr], addr);
            cnt++;
        }
    }
    ESP_LOGI(TAG, "Bus %d: %d devices found", (int)port, cnt);
    if(fp)fprintf(fp, "Bus %d: %d devices found", (int)port, cnt);
    return ESP_OK;
}

esp_err_t I2C::Read(const i2c_port_t port, uint8_t address7bit, uint8_t *data, size_t len)
{
    if (!xSemaphoreTake(locks[port], pdMS_TO_TICKS(1000)))
    {
        ESP_LOGE(TAG, "Could not take port mutex %d", port);
        return ESP_ERR_TIMEOUT;
    }
    esp_err_t espRc;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (address7bit << 1) | I2C_MASTER_READ, true);
    if (len > 1)
    {
        i2c_master_read(cmd, data, len - 1, I2C_MASTER_ACK);
    }
    i2c_master_read_byte(cmd, data + len - 1, I2C_MASTER_NACK);
    i2c_master_stop(cmd);
    espRc = i2c_master_cmd_begin(port, cmd, 10 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
    xSemaphoreGive(locks[port]);
    return espRc;
}

esp_err_t I2C::Write(const i2c_port_t port, const uint8_t address7bit, const uint8_t *const data, const size_t len)
{
    if (!xSemaphoreTake(locks[port], pdMS_TO_TICKS(1000)))
    {
        ESP_LOGE(TAG, "Could not take port mutex %d", port);
        return ESP_ERR_TIMEOUT;
    }
    esp_err_t espRc;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, address7bit << 1 | I2C_MASTER_WRITE, true);
    if (!data)
    {
        ESP_LOGE(TAG, "Data is NULL for address 0x%02X on port %d", address7bit, port);
    }
    i2c_master_write(cmd, data, len, true);
    i2c_master_stop(cmd);
    espRc = i2c_master_cmd_begin(port, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);
    xSemaphoreGive(locks[port]);
    return espRc;
}