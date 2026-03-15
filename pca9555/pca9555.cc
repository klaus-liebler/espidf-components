#include <inttypes.h>
#include <esp_log.h>
#include <string.h>
#include <common-esp32.hh>
#include <errorcodes.hh>
#include "pca9555.hh"
#define TAG "PCA9555"

namespace PCA9555
{
    M::M(Device device, uint16_t initialInputValue, uint16_t configurationRegister, uint16_t polarityInversionRegister):
		master_bus_handle(nullptr), dev_handle(nullptr), device(device), cachedInput(initialInputValue), configurationRegister(configurationRegister), polarityInversionRegister(polarityInversionRegister) {
		}

    ErrorCode M::Setup(i2c_master_bus_handle_t master_bus_handle){
		this->master_bus_handle = master_bus_handle;
		this->dev_handle = nullptr;
		if (master_bus_handle == nullptr) {
			ESP_LOGE(TAG, "master_bus_handle is null");
			return ErrorCode::GENERIC_ERROR;
		}

		const uint8_t address = (uint8_t)device;
		esp_err_t rc = i2c_master_probe(master_bus_handle, address, 1000);
		if (rc != ESP_OK) {
			ESP_LOGW(TAG, "Device 0x%02X not available, err=%d", address, rc);
			return ErrorCode::DEVICE_NOT_RESPONDING;
		}

		i2c_device_config_t dev_cfg = {
			.dev_addr_length = I2C_ADDR_BIT_LEN_7,
			.device_address = address,
			.scl_speed_hz = 100000,
		};
		rc = i2c_master_bus_add_device(master_bus_handle, &dev_cfg, &dev_handle);
		if (rc != ESP_OK) {
			ESP_LOGE(TAG, "Could not add I2C device 0x%02X, err=%d", address, rc);
			return ErrorCode::DEVICE_NOT_RESPONDING;
		}

		uint8_t config_msg[3] = {
			(uint8_t)Register::ConfigurationPort0,
			(uint8_t)(this->configurationRegister & 0xFF),
			(uint8_t)((this->configurationRegister >> 8) & 0xFF),
		};
		rc = i2c_master_transmit(dev_handle, config_msg, sizeof(config_msg), 1000);
		if (rc != ESP_OK) {
			ESP_LOGE(TAG, "Config write failed, err=%d", rc);
			return ErrorCode::DEVICE_NOT_RESPONDING;
		}

		uint8_t invert_msg[3] = {
			(uint8_t)Register::PolarityInversionPort0,
			(uint8_t)(this->polarityInversionRegister & 0xFF),
			(uint8_t)((this->polarityInversionRegister >> 8) & 0xFF),
		};
		rc = i2c_master_transmit(dev_handle, invert_msg, sizeof(invert_msg), 1000);
		if (rc != ESP_OK) {
			ESP_LOGE(TAG, "Polarity write failed, err=%d", rc);
			return ErrorCode::DEVICE_NOT_RESPONDING;
		}

		return Update();
	}
    uint16_t M::GetCachedInput(void){
		return this->cachedInput;
	}
    ErrorCode M::Update(void){
		if (dev_handle == nullptr) {
			return ErrorCode::GENERIC_ERROR;
		}
		uint8_t reg = (uint8_t)Register::InputPort0;
		uint8_t ret[2] = {0xFF, 0xFF};
		esp_err_t rc = i2c_master_transmit_receive(dev_handle, &reg, 1, ret, sizeof(ret), 1000);
		if (rc != ESP_OK) {
			return ErrorCode::DEVICE_NOT_RESPONDING;
		}
		this->cachedInput = (uint16_t)ret[0] | ((uint16_t)ret[1] << 8);
		return ErrorCode::OK;
	}

	ErrorCode M::SetOutput(uint16_t output){
		if (dev_handle == nullptr) {
			return ErrorCode::GENERIC_ERROR;
		}
		uint8_t msg[3] = {
			(uint8_t)Register::OutputPort0,
			(uint8_t)(output & 0xFF),
			(uint8_t)((output >> 8) & 0xFF),
		};
		if (i2c_master_transmit(dev_handle, msg, sizeof(msg), 1000) != ESP_OK) {
			return ErrorCode::DEVICE_NOT_RESPONDING;
		}
		return ErrorCode::OK;
	}

	ErrorCode M::SetInputOutputConfig(uint16_t config){
		if (dev_handle == nullptr) {
			return ErrorCode::GENERIC_ERROR;
		}
		uint8_t msg[3] = {
			(uint8_t)Register::ConfigurationPort0,
			(uint8_t)(config & 0xFF),
			(uint8_t)((config >> 8) & 0xFF),
		};
		if (i2c_master_transmit(dev_handle, msg, sizeof(msg), 1000) != ESP_OK) {
			return ErrorCode::DEVICE_NOT_RESPONDING;
		}
		return ErrorCode::OK;
	}
    ErrorCode M::SetInversionConfig(uint16_t config){
		if (dev_handle == nullptr) {
			return ErrorCode::GENERIC_ERROR;
		}
		uint8_t msg[3] = {
			(uint8_t)Register::PolarityInversionPort0,
			(uint8_t)(config & 0xFF),
			(uint8_t)((config >> 8) & 0xFF),
		};
		if (i2c_master_transmit(dev_handle, msg, sizeof(msg), 1000) != ESP_OK) {
			return ErrorCode::DEVICE_NOT_RESPONDING;
		}
		return ErrorCode::OK;
	}
}