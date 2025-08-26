#pragma once
#include <cstring>
#include <common.hh>
#include <errorcodes.hh>
#include <driver/i2c_master.h>
#include "esp_check.h"
#include <esp_log.h>
#define TAG "sensor"


class I2CSensor{
    enum class STATE{
        INITIAL,
        FOUND,
        INITIALIZED,
        TRIGGERED,
        RETRIGGERED,
        READOUT,
        ERROR_NOT_FOUND,
        ERROR_COMMUNICATION,

    };
    private:
        int64_t nextAction{0};
        I2CSensor::STATE state{STATE::INITIAL};
    protected:
        i2c_master_bus_handle_t bus_handle;
        i2c_master_dev_handle_t dev_handle;
        uint8_t address_7bit;
        I2CSensor(i2c_master_bus_handle_t bus_handle, uint8_t address_7bit):bus_handle(bus_handle), address_7bit(address_7bit){}
        virtual ErrorCode Trigger(int64_t& waitTillReadout)=0;
        virtual ErrorCode Readout(int64_t& waitTillNExtTrigger)=0;
        //Precondition: dev_handle exists!
        virtual ErrorCode Initialize(int64_t& waitTillFirstTrigger)=0;
        void ReInit(){
            this->state=STATE::FOUND;
        }

        ErrorCode Probe(uint8_t dev_addr){
            
            return i2c_master_probe(bus_handle, dev_addr, 1000)==ESP_OK?ErrorCode::OK:ErrorCode::GENERIC_ERROR;
        }

        ErrorCode WriteReg8(uint8_t reg_addr, const uint8_t reg_data){
            ESP_RETURN_ON_FALSE(dev_handle, ErrorCode::GENERIC_ERROR, TAG, "dev_handle is null");
            uint8_t buf[1+1];
            buf[0]=reg_addr;
            buf[1]=reg_data;
            return i2c_master_transmit(dev_handle, buf, 1+1, 1000)==ESP_OK?ErrorCode::OK:ErrorCode::GENERIC_ERROR;
        }

        ErrorCode WriteRegs8(uint8_t reg_addr, const uint8_t *reg_data, size_t data_len){
            ESP_RETURN_ON_FALSE(dev_handle, ErrorCode::GENERIC_ERROR, TAG, "dev_handle is null");
            uint8_t buf[1+data_len];
            buf[0]=reg_addr;
            memcpy(buf+1, reg_data, data_len);
            return i2c_master_transmit(dev_handle, buf, 1+data_len, 1000)==ESP_OK?ErrorCode::OK:ErrorCode::GENERIC_ERROR;
        }

        ErrorCode ReadRegs8(uint8_t reg_addr, uint8_t *reg_data, size_t data_len){
            ESP_RETURN_ON_FALSE(dev_handle, ErrorCode::GENERIC_ERROR, TAG, "dev_handle is null");
            return i2c_master_transmit_receive(dev_handle, &reg_addr, 1, reg_data, data_len, 1000)==ESP_OK?ErrorCode::OK:ErrorCode::GENERIC_ERROR;
        }

        ErrorCode Read8(uint8_t *data, size_t data_len){
            ESP_RETURN_ON_FALSE(dev_handle, ErrorCode::GENERIC_ERROR, TAG, "dev_handle is null");
            return i2c_master_receive(dev_handle, data, data_len, 1000)==ESP_OK?ErrorCode::OK:ErrorCode::GENERIC_ERROR;
        }

        ErrorCode Write8(const uint8_t *data, size_t data_len){
            ESP_RETURN_ON_FALSE(dev_handle, ErrorCode::GENERIC_ERROR, TAG, "dev_handle is null");
            return (i2c_master_transmit(dev_handle, data, data_len, 1000)==ESP_OK)?ErrorCode::OK:ErrorCode::GENERIC_ERROR;
        }
        
    public:

    virtual bool HasValidData(){
        
        return state == STATE::READOUT || state == STATE::RETRIGGERED;
    }

    STATE GetState(){
        return state;
    }

    ErrorCode MakeDeviceReady_Blocking(int64_t currentMs){
        if(i2c_master_probe(bus_handle, this->address_7bit, 1000)!=ESP_OK){
            state = STATE::ERROR_NOT_FOUND;
            ESP_LOGD(TAG, "state = STATE::ERROR_NOT_FOUND; return ErrorCode::DEVICE_NOT_RESPONDING");
            
            return ErrorCode::DEVICE_NOT_RESPONDING;
        }
        
        i2c_device_config_t dev_cfg = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = address_7bit,
            .scl_speed_hz = 100000,
            .scl_wait_us=0,
            .flags=0,
        };
        ESP_ERROR_CHECK(i2c_master_bus_add_device(this->bus_handle, &dev_cfg, &(this->dev_handle)));
        int64_t wait;
        auto e=Initialize(wait);
        if(e!=ErrorCode::OK){
            state = STATE::ERROR_COMMUNICATION;
            ESP_LOGD(TAG, "state = STATE::ERROR_COMMUNICATION; return ErrorCode::DEVICE_NOT_RESPONDING");
            return ErrorCode::DEVICE_NOT_RESPONDING;
        }
        
        nextAction=currentMs+wait;
        state=STATE::INITIALIZED;
        return ErrorCode::OK;
    }

    ErrorCode Loop(int64_t currentMs){
        if(currentMs<nextAction) return ErrorCode::OK;
        ErrorCode e;
        int64_t wait{0};
        switch (state)
        {
        case STATE::INITIAL:{
            if(i2c_master_probe(bus_handle, this->address_7bit, 1000)!=ESP_OK){
                state = STATE::ERROR_NOT_FOUND;
                ESP_LOGE(TAG, "Device with address %02x not found state = STATE::ERROR_NOT_FOUND; return ErrorCode::DEVICE_NOT_RESPONDING", this->address_7bit);
                nextAction=INT64_MAX;
                return ErrorCode::DEVICE_NOT_RESPONDING;
            }
            
            i2c_device_config_t dev_cfg = {
                .dev_addr_length = I2C_ADDR_BIT_LEN_7,
                .device_address = address_7bit,
                .scl_speed_hz = 100000,
                .scl_wait_us=0,
                .flags=0,
            };
            ESP_ERROR_CHECK(i2c_master_bus_add_device(this->bus_handle, &dev_cfg, &(this->dev_handle)));
            state=STATE::FOUND;
            break;
        }
            
        case STATE::FOUND:
            e=Initialize(wait);
            if(e!=ErrorCode::OK){
                state = STATE::ERROR_COMMUNICATION;
                ESP_LOGD(TAG, "state = STATE::ERROR_COMMUNICATION; return ErrorCode::DEVICE_NOT_RESPONDING");
                return ErrorCode::DEVICE_NOT_RESPONDING;
            }
            nextAction=currentMs+wait;
            state=STATE::INITIALIZED;
            break;
        case STATE::INITIALIZED:
        case STATE::READOUT:
            e=Trigger(wait);
            if(e!=ErrorCode::OK){
                state = STATE::ERROR_COMMUNICATION;
                ESP_LOGE(TAG, "state = STATE::ERROR_COMMUNICATION;; return ErrorCode::DEVICE_NOT_RESPONDING");
                return ErrorCode::DEVICE_NOT_RESPONDING;
            }
            nextAction=currentMs+wait;
            state=state==STATE::INITIALIZED?STATE::TRIGGERED:STATE::RETRIGGERED;
            
            break;
        case STATE::TRIGGERED:
        case STATE::RETRIGGERED:
            e=Readout(wait);
            if(e!=ErrorCode::OK){
                state = STATE::ERROR_COMMUNICATION;
                return ErrorCode::DEVICE_NOT_RESPONDING;
            }
            nextAction=currentMs+wait;
            state=STATE::READOUT;
            break;   
        default:
            break;
        }
        ESP_LOGD(TAG, "New state: %d, next action %d", (int)state, (int)nextAction);
        return ErrorCode::OK;

    }
};
#undef TAG