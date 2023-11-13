#pragma once
 
#include <stdio.h>
#include "driver/i2c.h"
#include "driver/gpio.h"
#include "errorcodes.hh"

class iI2CPort{
    public:
    virtual ErrorCode ReadReg(const uint8_t address7bit, uint8_t reg_addr, uint8_t *reg_data, size_t len=1)=0;
    virtual ErrorCode ReadReg16(const uint8_t address7bit, uint16_t reg_addr16, uint8_t *reg_data, size_t len)=0;
	virtual ErrorCode ReadSingleReg16(const uint8_t address7bit, uint8_t reg_addr, uint16_t *reg_data)=0;
	virtual ErrorCode ReadSingleReg32(const uint8_t address7bit, uint8_t reg_addr, uint32_t *reg_data)=0;
    virtual ErrorCode Read(const uint8_t address7bit, uint8_t *data, size_t len)=0;
    virtual ErrorCode WriteReg(const uint8_t address7bit, const uint8_t reg_addr, const uint8_t * const reg_data, const size_t len)=0;
    virtual ErrorCode WriteSingleReg(const uint8_t address7bit, const uint8_t reg_addr, const uint8_t reg_data)=0;
	virtual ErrorCode WriteSingleReg16(const uint8_t address7bit, const uint8_t reg_addr, const uint16_t reg_data)=0;
	virtual ErrorCode WriteSingleReg32(const uint8_t address7bit, const uint8_t reg_addr, const uint32_t reg_data)=0;
    virtual ErrorCode Write(const uint8_t address7bit, const uint8_t * const data, const size_t len)=0;
    virtual ErrorCode IsAvailable(const uint8_t address7bit)=0;
    virtual ErrorCode Discover()=0;
    
};

class iI2CPort;

class I2C{
    private:
    static SemaphoreHandle_t locks[I2C_NUM_MAX];
    public:
    static esp_err_t Init(const i2c_port_t port, const gpio_num_t scl, const gpio_num_t sda, int intr_alloc_flags=0);
    static esp_err_t ReadReg(const i2c_port_t port, uint8_t address7bit, uint8_t reg_addr, uint8_t *reg_data, size_t len);
    static esp_err_t ReadDoubleReg(const i2c_port_t port, uint8_t address7bit, uint8_t reg_addr, uint16_t *reg_data);
    static esp_err_t ReadReg16(const i2c_port_t port, uint8_t address7bit, uint16_t reg_addr16, uint8_t *reg_data, size_t len);

    static esp_err_t Read(const i2c_port_t port, uint8_t address7bit, uint8_t *data, size_t len);
    static esp_err_t WriteReg(const i2c_port_t port, const uint8_t address7bit, const uint8_t reg_addr, const uint8_t * const reg_data, const size_t len);
    static esp_err_t WriteSingleReg(const i2c_port_t port, const uint8_t address7bit, const uint8_t reg_addr, const uint8_t reg_data);
    static esp_err_t WriteDoubleReg(const i2c_port_t port, const uint8_t address7bit, const uint8_t reg_addr, const uint16_t reg_data);
    static esp_err_t Write(const i2c_port_t port, const uint8_t address7bit, const uint8_t * const data, const size_t len);
    static esp_err_t IsAvailable(const i2c_port_t port, uint8_t address7bit);
    static esp_err_t Discover(const i2c_port_t port);
    static esp_err_t Discover(const i2c_port_t port, FILE * fp);

    static iI2CPort* GetPort_DoNotForgetToDelete(const i2c_port_t port);
};

class iI2CPort_Impl:public iI2CPort{
    private:
    const i2c_port_t port;
    public:
    iI2CPort_Impl(const i2c_port_t port):port(port){

    }
    ErrorCode ReadReg(const uint8_t address7bit, uint8_t reg_addr, uint8_t *reg_data, size_t len) override{
        return I2C::ReadReg(port, address7bit, reg_addr, reg_data, len)==ESP_OK?ErrorCode::OK:ErrorCode::DEVICE_NOT_RESPONDING;
    }
	
	ErrorCode ReadSingleReg16(const uint8_t address7bit, uint8_t reg_addr, uint16_t *reg_data) override{
		uint8_t tmp[2];
		esp_err_t err=I2C::ReadReg(port, address7bit, reg_addr, tmp, 2);
        *reg_data  = tmp[0] << 8; // value high byte
        *reg_data |= tmp[1] << 0;     // value low byte
		return err==ESP_OK?ErrorCode::OK:ErrorCode::DEVICE_NOT_RESPONDING;
	}
	
	ErrorCode ReadSingleReg32(const uint8_t address7bit, uint8_t reg_addr, uint32_t *reg_data) override{
		uint8_t tmp[4];
		esp_err_t err=I2C::ReadReg(port, address7bit, reg_addr, tmp, 4);
        *reg_data  = tmp[0] << 24; 
		*reg_data |= tmp[1] << 16; 
		*reg_data |= tmp[2] <<  8; 
        *reg_data |= tmp[3] <<  0;     // value low byte
		return err==ESP_OK?ErrorCode::OK:ErrorCode::DEVICE_NOT_RESPONDING;
	}

    ErrorCode ReadReg16(const uint8_t address7bit, uint16_t reg_addr16, uint8_t *reg_data, size_t len) override{
        return I2C::ReadReg16(port, address7bit, reg_addr16, reg_data, len)==ESP_OK?ErrorCode::OK:ErrorCode::DEVICE_NOT_RESPONDING;
    }
    
    ErrorCode Read(const uint8_t address7bit, uint8_t *data, size_t len) override{
        return I2C::Read(port, address7bit, data, len)==ESP_OK?ErrorCode::OK:ErrorCode::DEVICE_NOT_RESPONDING;
    }

    ErrorCode WriteReg(const uint8_t address7bit, const uint8_t reg_addr, const uint8_t * const reg_data, const size_t len) override{
        return I2C::WriteReg(port, address7bit, reg_addr, reg_data, len)==ESP_OK?ErrorCode::OK:ErrorCode::DEVICE_NOT_RESPONDING;
    }

    ErrorCode WriteSingleReg(const uint8_t address7bit, const uint8_t reg_addr, const uint8_t reg_data) override{
        return I2C::WriteReg(port, address7bit, reg_addr, &reg_data, 1)==ESP_OK?ErrorCode::OK:ErrorCode::DEVICE_NOT_RESPONDING;
    }
	
	ErrorCode WriteSingleReg16(const uint8_t address7bit, const uint8_t reg_addr, const uint16_t reg_data) override{
        uint8_t tmp[2];
		tmp[0]=((uint8_t)(reg_data >> 8)); // value high byte
        tmp[1]=((uint8_t)(reg_data));      // value low byte
		return I2C::WriteReg(port, address7bit, reg_addr, tmp, 2)==ESP_OK?ErrorCode::OK:ErrorCode::DEVICE_NOT_RESPONDING;
    }
	
	ErrorCode WriteSingleReg32(const uint8_t address7bit, const uint8_t reg_addr, const uint32_t reg_data) override{
        uint8_t tmp[4]={(uint8_t)(reg_data >> 24), (uint8_t)(reg_data >> 16), (uint8_t)(reg_data >>  8), (uint8_t)(reg_data >>  0)};
		return I2C::WriteReg(port, address7bit, reg_addr, tmp, 4)==ESP_OK?ErrorCode::OK:ErrorCode::DEVICE_NOT_RESPONDING;
    }


    ErrorCode Write(const uint8_t address7bit, const uint8_t * const data, const size_t len) override{
        return I2C::Write(port, address7bit, data, len)==ESP_OK?ErrorCode::OK:ErrorCode::DEVICE_NOT_RESPONDING;
    }

    ErrorCode IsAvailable(const uint8_t address7bit) override{
        return I2C::IsAvailable(port, address7bit)==ESP_OK?ErrorCode::OK:ErrorCode::DEVICE_NOT_RESPONDING;
    }

    ErrorCode Discover() override{
        return I2C::Discover(port)==ESP_OK?ErrorCode::OK:ErrorCode::DEVICE_NOT_RESPONDING;
    }
};