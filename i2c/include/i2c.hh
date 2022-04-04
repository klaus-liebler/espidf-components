#pragma once
 
#include <stdio.h>
#include "driver/i2c.h"
#include "driver/gpio.h"

class I2C{
    private:
    static SemaphoreHandle_t *locks;
    public:
    static esp_err_t Init();
    static esp_err_t ReadReg(const i2c_port_t port, uint8_t address7bit, uint8_t reg_addr, uint8_t *reg_data, size_t len);
    static esp_err_t ReadReg16(const i2c_port_t port, uint8_t address7bit, uint16_t reg_addr16, uint8_t *reg_data, size_t len);
    static esp_err_t Read(const i2c_port_t port, uint8_t address7bit, uint8_t *data, size_t len);
    static esp_err_t WriteReg(const i2c_port_t port, uint8_t address7bit, uint8_t reg_addr, uint8_t *reg_data, size_t len);
    static esp_err_t WriteSingleReg(const i2c_port_t port, uint8_t address7bit, uint8_t reg_addr, uint8_t reg_data){return WriteReg(port, address7bit, reg_addr, &reg_data, 1);}
    static esp_err_t Write(const i2c_port_t port, uint8_t address7bit, uint8_t *data, size_t len);
    static esp_err_t IsAvailable(const i2c_port_t port, uint8_t address7bit);
};