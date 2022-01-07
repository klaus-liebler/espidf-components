#include <inttypes.h>
#include <i2c.hh>
#include "pca9555.hh"
#include <esp_log.h>
#include <string.h>
#define TAG "PCA9555"

namespace PCA9555
{
    M::M(i2c_port_t i2c_num, Device device, uint16_t initialValue):i2c_num(i2c_num), device(device), initialValue(initialValue){}
    ErrorCode M::Setup(){
		esp_err_t err = I2C::IsAvailable(i2c_num, (uint8_t)device);
		if(err != ESP_OK)
		{
			return ErrorCode::DEVICE_NOT_RESPONDING;
		}
		
		return Update();
	}
    uint16_t M::GetCachedInput(void){
		return this->cache;
	}
    ErrorCode M::Update(void){
		volatile uint16_t ret=0xFFFF;
		esp_err_t err = I2C::ReadReg(i2c_num, (uint8_t)device, (uint16_t)Register::InputPort0, (uint8_t*)&ret, 2);
		if(err!=ESP_OK){
			return ErrorCode::GENERIC_ERROR;
		}
		this->cache=ret;
		return ErrorCode::OK;
	}
  
}