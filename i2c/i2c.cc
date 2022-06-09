
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include "include/i2c.hh"

static const char *TAG = "I2C_DEV";

SemaphoreHandle_t *I2C::locks ={0};

esp_err_t I2C::Init()
{
    I2C::locks = new SemaphoreHandle_t[I2C_NUM_MAX];
    for (int i = 0; i < I2C_NUM_MAX; i++)
    {
        I2C::locks[i] = xSemaphoreCreateMutex();
    }
    return ESP_OK;
}

esp_err_t I2C::WriteReg(const i2c_port_t port, const uint8_t address7bit, const uint8_t reg_addr, const uint8_t *const reg_data, const size_t len)
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
    i2c_master_write_byte(cmd, reg_addr, true);
     if(!reg_data){
        ESP_LOGE(TAG, "Data is NULL for address 0x%02X on port %d", address7bit, port);
    }
    i2c_master_write(cmd, (uint8_t *)reg_data, len, true);
    i2c_master_stop(cmd);

    espRc = i2c_master_cmd_begin(port, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);
    xSemaphoreGive(locks[port]);
    if(espRc!=ESP_OK){
        ESP_LOGE(TAG, "Error %d", espRc);
    }
    return espRc;
}

esp_err_t I2C::ReadReg(const i2c_port_t port, uint8_t address7bit, uint8_t reg_addr, uint8_t *reg_data, size_t len)
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
    i2c_master_write_byte(cmd, reg_addr, true);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (address7bit << 1) | I2C_MASTER_READ, true);
    if (len > 1)
    {
        i2c_master_read(cmd, reg_data, len - 1, I2C_MASTER_ACK);
    }
    i2c_master_read_byte(cmd, reg_data + len - 1, I2C_MASTER_NACK);
    i2c_master_stop(cmd);
    espRc = i2c_master_cmd_begin(port, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);
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
    
    i2c_master_write(cmd, (uint8_t*)&reg_addr16, 2, true);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (address7bit << 1) | I2C_MASTER_READ, true);
    if (len > 1)
    {
        i2c_master_read(cmd, reg_data, len - 1, I2C_MASTER_ACK);
    }
    i2c_master_read_byte(cmd, reg_data + len - 1, I2C_MASTER_NACK);
    i2c_master_stop(cmd);
    espRc = i2c_master_cmd_begin(port, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);
    xSemaphoreGive(locks[port]);
    return espRc;
}

esp_err_t I2C::IsAvailable(const i2c_port_t port, uint8_t address7bit){

    if (!xSemaphoreTake(locks[port], pdMS_TO_TICKS(1000)))
    {
        ESP_LOGE(TAG, "Could not take port mutex %d", port);
        return ESP_ERR_TIMEOUT;
    }
    //Nothing to init. Just check if it is there...
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (address7bit << 1) | I2C_MASTER_WRITE  , true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(port, cmd, pdMS_TO_TICKS(50));
    i2c_cmd_link_delete(cmd);
    xSemaphoreGive(locks[port]);
    return ret; 
}

esp_err_t I2C::Read(const i2c_port_t port, uint8_t address7bit, uint8_t *data, size_t len){
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

esp_err_t I2C::Write(const i2c_port_t port, const uint8_t address7bit, const uint8_t * const data, const size_t len){
    if (!xSemaphoreTake(locks[port], pdMS_TO_TICKS(1000)))
    {
        ESP_LOGE(TAG, "Could not take port mutex %d", port);
        return ESP_ERR_TIMEOUT;
    }
    esp_err_t espRc;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, address7bit << 1 | I2C_MASTER_WRITE, true);
    if(!data){
        ESP_LOGE(TAG, "Data is NULL for address 0x%02X on port %d", address7bit, port);
    }
    i2c_master_write(cmd, data, len, true);
    i2c_master_stop(cmd);
    espRc = i2c_master_cmd_begin(port, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);
    xSemaphoreGive(locks[port]);
    return espRc;
}