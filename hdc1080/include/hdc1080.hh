#pragma once
#include <cstdint>
#include <cmath>
#include <i2c_sensor.hh>

namespace HDC1080{
    constexpr uint8_t I2C_ADDRESS=0x40;
    constexpr uint8_t TEMPERATURE_OFFSET = 0x00;
    constexpr uint8_t HUMIDITY_OFFSET = 0x01;
    constexpr uint8_t CONFIG_OFFSET = 0x02;
    enum class HUMRESOLUTION{
        __8bit=0b10,
        _11bit=0b01,
        _14bit=0b00,
    };
    enum class TEMPRESOLUTION{
        _14bit=0b0,
        _11bit=0b1,
    };
    enum class MODE{
        TorH=0b0,
        TandH=0b1,
    };
    enum class HEATER{
        DISABLE = 0b0,
        ENABLE  = 0b1,
    };

    typedef union {
	    uint8_t rawData;
        struct {
            uint8_t HumidityMeasurementResolution : 2;
            uint8_t TemperatureMeasurementResolution : 1;
            uint8_t BatteryStatus : 1;
            uint8_t ModeOfAcquisition : 1;
            uint8_t Heater : 1;
            uint8_t ReservedAgain : 1;
            uint8_t SoftwareReset : 1;
        };
    } ConfigRegister;
    
    
    class M:public I2CSensor{
        public:
            M(i2c_master_bus_handle_t i2c_port):I2CSensor(i2c_port, I2C_ADDRESS){}
            ErrorCode Reconfigure(TEMPRESOLUTION tempRes, HUMRESOLUTION humRes, HEATER heater){
                this->tempRes=tempRes;
                this->humRes=humRes;
                this->heater=heater;
                ReInit();
                return ErrorCode::OK;
            }

            ErrorCode ReadOut(float &humidity, float &temperature){
                humidity=this->h;
                temperature=this->t;
                return ErrorCode::OK;
            }
        private:
           
           TEMPRESOLUTION tempRes{TEMPRESOLUTION::_14bit};
           HUMRESOLUTION humRes{HUMRESOLUTION::_14bit};
           HEATER heater{HEATER::DISABLE};
           float t{0.0};
           float h{0.0};
        protected:
        
        ErrorCode Initialize(int64_t& wait) override{
            ConfigRegister config;
            ReadRegs8(CONFIG_OFFSET, &config.rawData, 1);
            config.HumidityMeasurementResolution= (uint8_t)humRes;
            config.TemperatureMeasurementResolution= (uint8_t)tempRes;
            config.Heater= (uint8_t)heater;
            uint8_t dummy[2] = {config.rawData, 0x00};
            return WriteRegs8(CONFIG_OFFSET, dummy, 2);
        }
        ErrorCode Trigger(int64_t& wait) override{
            wait=100;
            uint8_t data = TEMPERATURE_OFFSET;
            return Write8(&data, 1);
        }

        ErrorCode Readout(int64_t& wait) override{
            wait=100;
            uint16_t rawT{0};
            ReadRegs8(TEMPERATURE_OFFSET, (uint8_t*)&rawT, 2);
	        t= (rawT / pow(2, 16)) * 165.0 - 40.0;
            uint16_t rawH{0};
            ReadRegs8(HUMIDITY_OFFSET, (uint8_t*)&rawH, 2);
	        h = (rawH / pow(2, 16)) * 100.0;
            return ErrorCode::OK;
        }
    };

}
