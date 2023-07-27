#include <inttypes.h>
#include <i2c.hh>
#include <esp_log.h>
#include <string.h>
#include <common-esp32.hh>
#include <errorcodes.hh>
#include "pca9555.hh"
#define TAG "PCA9555"

namespace PCA9555
{
    M::M(iI2CPort* i2cPort, Device device, uint16_t initialInputValue,uint16_t configurationRegister, uint16_t polarityInversionRegister):
		i2cPort(i2cPort), device(device), cachedInput(initialInputValue),configurationRegister(configurationRegister),polarityInversionRegister(polarityInversionRegister) {
			assert(i2cPort!=nullptr);
		}
    ErrorCode M::Setup(){
		RETURN_ON_ERRORCODE(i2cPort->IsAvailable((uint8_t)device));
    	RETURN_ON_ERRORCODE(i2cPort->WriteReg((uint8_t)device, (uint16_t)Register::ConfigurationPort0, (uint8_t*)&this->configurationRegister, 2));
		RETURN_ON_ERRORCODE(i2cPort->WriteReg((uint8_t)device, (uint16_t)Register::PolarityInversionPort0, (uint8_t*)&this->polarityInversionRegister, 2));
		return Update();
	}
    uint16_t M::GetCachedInput(void){
		return this->cachedInput;
	}
    ErrorCode M::Update(void){
		volatile uint16_t ret=0xFFFF;
		RETURN_ON_ERRORCODE(i2cPort->ReadReg((uint8_t)device, (uint16_t)Register::InputPort0, (uint8_t*)&ret, 2));
		this->cachedInput=ret;
		return ErrorCode::OK;
	}

	ErrorCode M::SetOutput(uint16_t output){
		RETURN_ON_ERRORCODE(i2cPort->WriteReg((uint8_t)device, (uint16_t)Register::OutputPort0, (uint8_t*)&output, 2));
		return ErrorCode::OK;
	}

	ErrorCode M::SetInputOutputConfig(uint16_t config){
		RETURN_ON_ERRORCODE(i2cPort->WriteReg((uint8_t)device, (uint16_t)Register::ConfigurationPort0, (uint8_t*)&config, 2));
		return ErrorCode::OK;
	}
    ErrorCode M::SetInversionConfig(uint16_t config){
		RETURN_ON_ERRORCODE(i2cPort->WriteReg((uint8_t)device, (uint16_t)Register::PolarityInversionPort0, (uint8_t*)&config, 2));
		return ErrorCode::OK;
	}
}