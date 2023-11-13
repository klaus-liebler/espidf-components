#pragma once
#include <common.hh>
#include <errorcodes.hh>
#include <driver/i2c.h>
#include <i2c.hh>
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
        iI2CPort*  i2c_port;
        uint8_t address_7bit;
        I2CSensor(iI2CPort*  i2c_port, uint8_t address_7bit):i2c_port(i2c_port), address_7bit(address_7bit){}
        virtual ErrorCode Trigger(int64_t& waitTillReadout)=0;
        virtual ErrorCode Readout(int64_t& waitTillNExtTrigger)=0;
        virtual ErrorCode Initialize(int64_t& waitTillFirstTrigger)=0;
        void ReInit(){
            this->state=STATE::FOUND;
        }
    public:

    virtual bool HasValidData(){
        
        return state == STATE::READOUT || state == STATE::RETRIGGERED;
    }

    ErrorCode Loop(int64_t currentMs){
        if(currentMs<nextAction) return ErrorCode::OK;
        ErrorCode e;
        int64_t wait{0};
        switch (state)
        {
        case STATE::INITIAL:
            if(ErrorCode::OK!= i2c_port->IsAvailable(address_7bit)){
                state = STATE::ERROR_NOT_FOUND;
                ESP_LOGD(TAG, "state = STATE::ERROR_NOT_FOUND; return ErrorCode::DEVICE_NOT_RESPONDING");
                return ErrorCode::DEVICE_NOT_RESPONDING;
            }
            state=STATE::FOUND;
            break;
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