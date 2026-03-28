#include "i2c.hh"

#include <array>
#include <vector>

#include "esp_log.h"

namespace i2c {

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
    "MCP9808 47L04/47C04/47L16/47C16 NAU8822L",
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

ErrorCode iI2CDevice_Impl::ToErrorCode(esp_err_t err) {
    return err == ESP_OK ? ErrorCode::OK : ErrorCode::DEVICE_NOT_RESPONDING;
}

iI2CDevice_Impl::iI2CDevice_Impl(i2c_master_bus_handle_t bus_handle, i2c_master_dev_handle_t dev_handle, uint8_t address7bit)
    : bus_handle(bus_handle), dev_handle(dev_handle), address7bit(address7bit) {}

ErrorCode iI2CDevice_Impl::ReadRegister(const uint8_t reg_addr, uint8_t *reg_data, size_t len) {
    if (reg_data == nullptr || len == 0) {
        return ErrorCode::GENERIC_ERROR;
    }
    return ToErrorCode(i2c_master_transmit_receive(dev_handle, &reg_addr, 1, reg_data, len, 1000));
}

ErrorCode iI2CDevice_Impl::ReadRegisterAddress16(const uint16_t reg_addr16, uint8_t *reg_data, size_t len) {
    if (reg_data == nullptr || len == 0) {
        return ErrorCode::GENERIC_ERROR;
    }
    uint8_t addr_bytes[2] = {
        static_cast<uint8_t>(reg_addr16 & 0xFF),
        static_cast<uint8_t>((reg_addr16 >> 8) & 0xFF),
    };
    return ToErrorCode(i2c_master_transmit_receive(dev_handle, addr_bytes, sizeof(addr_bytes), reg_data, len, 1000));
}

ErrorCode iI2CDevice_Impl::ReadRegisterU16BE(const uint8_t reg_addr, uint16_t *reg_data) {
    if (reg_data == nullptr) {
        return ErrorCode::GENERIC_ERROR;
    }
    uint8_t tmp[2] = {0, 0};
    esp_err_t err = i2c_master_transmit_receive(dev_handle, &reg_addr, 1, tmp, sizeof(tmp), 1000);
    *reg_data = (static_cast<uint16_t>(tmp[0]) << 8) | static_cast<uint16_t>(tmp[1]);
    return ToErrorCode(err);
}

ErrorCode iI2CDevice_Impl::ReadRegisterU32BE(const uint8_t reg_addr, uint32_t *reg_data) {
    if (reg_data == nullptr) {
        return ErrorCode::GENERIC_ERROR;
    }
    uint8_t tmp[4] = {0, 0, 0, 0};
    esp_err_t err = i2c_master_transmit_receive(dev_handle, &reg_addr, 1, tmp, sizeof(tmp), 1000);
    *reg_data = (static_cast<uint32_t>(tmp[0]) << 24) |
                (static_cast<uint32_t>(tmp[1]) << 16) |
                (static_cast<uint32_t>(tmp[2]) << 8) |
                static_cast<uint32_t>(tmp[3]);
    return ToErrorCode(err);
}

ErrorCode iI2CDevice_Impl::ReadRaw(uint8_t *data, size_t len) {
    if (data == nullptr || len == 0) {
        return ErrorCode::GENERIC_ERROR;
    }
    return ToErrorCode(i2c_master_receive(dev_handle, data, len, 1000));
}

ErrorCode iI2CDevice_Impl::WriteRegister(const uint8_t reg_addr, const uint8_t *const reg_data, const size_t len) {
    if (reg_data == nullptr || len == 0) {
        return ErrorCode::GENERIC_ERROR;
    }

    std::vector<uint8_t> write_buf;
    write_buf.reserve(1 + len);
    write_buf.push_back(reg_addr);
    write_buf.insert(write_buf.end(), reg_data, reg_data + len);

    return ToErrorCode(i2c_master_transmit(dev_handle, write_buf.data(), write_buf.size(), 1000));
}

ErrorCode iI2CDevice_Impl::WriteRegisterU8(const uint8_t reg_addr, const uint8_t reg_data) {
    uint8_t write_buf[2] = {reg_addr, reg_data};
    return ToErrorCode(i2c_master_transmit(dev_handle, write_buf, sizeof(write_buf), 1000));
}

ErrorCode iI2CDevice_Impl::WriteRegisterU16BE(const uint8_t reg_addr, const uint16_t reg_data) {
    uint8_t tmp[2] = {
        static_cast<uint8_t>((reg_data >> 8) & 0xFF),
        static_cast<uint8_t>(reg_data & 0xFF),
    };
    return WriteRegister(reg_addr, tmp, sizeof(tmp));
}

ErrorCode iI2CDevice_Impl::WriteRegisterU32BE(const uint8_t reg_addr, const uint32_t reg_data) {
    uint8_t tmp[4] = {
        static_cast<uint8_t>((reg_data >> 24) & 0xFF),
        static_cast<uint8_t>((reg_data >> 16) & 0xFF),
        static_cast<uint8_t>((reg_data >> 8) & 0xFF),
        static_cast<uint8_t>(reg_data & 0xFF),
    };
    return WriteRegister(reg_addr, tmp, sizeof(tmp));
}

ErrorCode iI2CDevice_Impl::WriteRaw(const uint8_t *const data, const size_t len) {
    if (data == nullptr || len == 0) {
        return ErrorCode::GENERIC_ERROR;
    }
    return ToErrorCode(i2c_master_transmit(dev_handle, data, len, 1000));
}

ErrorCode iI2CDevice_Impl::Probe() {
    if (bus_handle == nullptr) {
        return ErrorCode::GENERIC_ERROR;
    }
    return ToErrorCode(i2c_master_probe(bus_handle, address7bit, 50));
}

ErrorCode iI2CBus_Impl::Init(i2c_port_t port, gpio_num_t scl, gpio_num_t sda) {
    i2c_master_bus_config_t bus_config = {
        .i2c_port = static_cast<i2c_port_num_t>(port),
        .sda_io_num = sda,
        .scl_io_num = scl,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .intr_priority = 0,
        .trans_queue_depth = 0,
        .flags = {
            .enable_internal_pullup = true,
            .allow_pd=false,
        },
    };

    if (i2c_new_master_bus(&bus_config, &bus_handle) != ESP_OK) {
        return ErrorCode::GENERIC_ERROR;
    }

    general_call_device = nullptr;
    if (CreateDevice(0x00, &general_call_device) != ErrorCode::OK) {
        return ErrorCode::FUNCTION_NOT_AVAILABLE;
    }
    return ErrorCode::OK;
}

ErrorCode iI2CBus_Impl::SetDefaultSpeed(I2CSpeed speed) {
    device_speed_hz = static_cast<uint32_t>(speed);
    return ErrorCode::OK;
}

ErrorCode iI2CBus_Impl::CreateDevice(const uint8_t address7bit, iI2CDevice **device) {
    if (device == nullptr || bus_handle == nullptr) {
        return ErrorCode::GENERIC_ERROR;
    }

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = address7bit,
        .scl_speed_hz = device_speed_hz,
        .scl_wait_us=0,
        .flags = {
            .disable_ack_check = false,
        },
    };

    i2c_master_dev_handle_t dev = nullptr;
    if (i2c_master_bus_add_device(bus_handle, &dev_cfg, &dev) != ESP_OK) {
        return ErrorCode::DEVICE_NOT_RESPONDING;
    }

    *device = new iI2CDevice_Impl(bus_handle, dev, address7bit);
    return ErrorCode::OK;
}

iI2CDevice* iI2CBus_Impl::GetGeneralCallDevice() {
    return general_call_device;
}

ErrorCode iI2CBus_Impl::ProbeAddress(const uint8_t address7bit) {
    if (bus_handle == nullptr) {
        return ErrorCode::GENERIC_ERROR;
    }
    return i2c_master_probe(bus_handle, address7bit, 50) == ESP_OK ? ErrorCode::OK : ErrorCode::DEVICE_NOT_RESPONDING;
}

ErrorCode iI2CBus_Impl::Scan(FILE *fp, const char* busname) {
    if (bus_handle == nullptr) {
        return ErrorCode::GENERIC_ERROR;
    }
    if(fp == nullptr) {
        return ErrorCode::OK; // Just return the count without printing if fp is nullptr.
    }
    std::fprintf(fp, "Scanning %s...\n", busname);
    int cnt = 0;
    for (uint8_t addr = 1; addr < 128; ++addr) {
        if (i2c_master_probe(bus_handle, addr, 50) == ESP_OK) {
            std::fprintf(fp, "  0x%02X: %s\n", addr, address2name[addr]);
            ++cnt;
        }
    }
    std::fprintf(fp, "Finished scanning %s. %d devices found\n", busname, cnt);
    return ErrorCode::OK;
}

} // namespace i2c
