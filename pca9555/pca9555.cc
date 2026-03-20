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
		i2c_bus(nullptr), i2c_device(nullptr), device(device), cachedInput(initialInputValue), configurationRegister(configurationRegister), polarityInversionRegister(polarityInversionRegister) {
		}

    ErrorCode M::Setup(i2c::iI2CBus* i2c_bus){
		this->i2c_bus = i2c_bus;
		this->i2c_device = nullptr;
		if (i2c_bus == nullptr) {
			ESP_LOGE(TAG, "i2c_bus is null");
			return ErrorCode::GENERIC_ERROR;
		}

		const uint8_t address = (uint8_t)device;
		if (this->i2c_bus->ProbeAddress(address) != ErrorCode::OK) {
			ESP_LOGW(TAG, "Device 0x%02X not available", address);
			return ErrorCode::DEVICE_NOT_RESPONDING;
		}

		if (this->i2c_bus->CreateDevice(address, &this->i2c_device) != ErrorCode::OK || this->i2c_device == nullptr) {
			ESP_LOGE(TAG, "Could not create I2C device 0x%02X", address);
			return ErrorCode::DEVICE_NOT_RESPONDING;
		}

		uint8_t config_data[2] = {
			(uint8_t)(this->configurationRegister & 0xFF),
			(uint8_t)((this->configurationRegister >> 8) & 0xFF),
		};
		if (this->i2c_device->WriteRegister((uint8_t)Register::ConfigurationPort0, config_data, sizeof(config_data)) != ErrorCode::OK) {
			ESP_LOGE(TAG, "Config write failed");
			return ErrorCode::DEVICE_NOT_RESPONDING;
		}

		uint8_t invert_data[2] = {
			(uint8_t)(this->polarityInversionRegister & 0xFF),
			(uint8_t)((this->polarityInversionRegister >> 8) & 0xFF),
		};
		if (this->i2c_device->WriteRegister((uint8_t)Register::PolarityInversionPort0, invert_data, sizeof(invert_data)) != ErrorCode::OK) {
			ESP_LOGE(TAG, "Polarity write failed");
			return ErrorCode::DEVICE_NOT_RESPONDING;
		}

		return Update();
	}
    uint16_t M::GetCachedInput(void){
		return this->cachedInput;
	}
    ErrorCode M::Update(void){
		if (i2c_device == nullptr) {
			return ErrorCode::GENERIC_ERROR;
		}
		uint8_t ret[2] = {0xFF, 0xFF};
		if (i2c_device->ReadRegister((uint8_t)Register::InputPort0, ret, sizeof(ret)) != ErrorCode::OK) {
			return ErrorCode::DEVICE_NOT_RESPONDING;
		}
		this->cachedInput = (uint16_t)ret[0] | ((uint16_t)ret[1] << 8);
		return ErrorCode::OK;
	}

	ErrorCode M::SetOutput(uint16_t output){
		if (i2c_device == nullptr) {
			return ErrorCode::GENERIC_ERROR;
		}
		uint8_t data[2] = {
			(uint8_t)(output & 0xFF),
			(uint8_t)((output >> 8) & 0xFF),
		};
		if (i2c_device->WriteRegister((uint8_t)Register::OutputPort0, data, sizeof(data)) != ErrorCode::OK) {
			return ErrorCode::DEVICE_NOT_RESPONDING;
		}
		return ErrorCode::OK;
	}

	ErrorCode M::SetInputOutputConfig(uint16_t config){
		if (i2c_device == nullptr) {
			return ErrorCode::GENERIC_ERROR;
		}
		uint8_t data[2] = {
			(uint8_t)(config & 0xFF),
			(uint8_t)((config >> 8) & 0xFF),
		};
		if (i2c_device->WriteRegister((uint8_t)Register::ConfigurationPort0, data, sizeof(data)) != ErrorCode::OK) {
			return ErrorCode::DEVICE_NOT_RESPONDING;
		}
		this->configurationRegister = config;
		return ErrorCode::OK;
	}
    ErrorCode M::SetInversionConfig(uint16_t config){
		if (i2c_device == nullptr) {
			return ErrorCode::GENERIC_ERROR;
		}
		uint8_t data[2] = {
			(uint8_t)(config & 0xFF),
			(uint8_t)((config >> 8) & 0xFF),
		};
		if (i2c_device->WriteRegister((uint8_t)Register::PolarityInversionPort0, data, sizeof(data)) != ErrorCode::OK) {
			return ErrorCode::DEVICE_NOT_RESPONDING;
		}
		this->polarityInversionRegister = config;
		return ErrorCode::OK;
	}
}