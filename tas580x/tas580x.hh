#pragma once
#include <inttypes.h>
#include "tas5805m_reg_cfg.h"
#include "errorcodes.hh"
#include "common.hh"
#include <esp_log.h>
#include <math.h>
#include <codec_manager.hh>
#define TAG "tas580x"

namespace TAS580x
{
	enum class ADDR7bit
	{
		DVDD_4k7 = 0x2C,
		DVDD_15k = 0x2D, //=0x5A as 8 bit
		DVDD_47K = 0x2E,
		DVDD_120k = 0x2F,
	};

	enum class MuteFadingTime{
		_11ms,
		_53ms,
		_106ms,
		_266ms,
		_535ms,
		_1065ms,
		_2665ms,
		_5330ms,
	};

	enum class CTRL_STATE{
		DEEP_SLEEP=0,
		SLEEP=1,
		Hi_Z=2,
		PLAY=3,
	};

	namespace R
	{
		constexpr uint8_t PAGE_00 = 0x00;
		constexpr uint8_t PAGE_2A = 0x2a;

		constexpr uint8_t BOOK_00 = 0x00;
		constexpr uint8_t BOOK_8C = 0x8c;

		constexpr uint8_t MASTER_VOL_REG_ADDR = 0X4C;
		constexpr uint8_t MUTE_TIME_REG_ADDR = 0X51;

		constexpr uint8_t DAMP_MODE_BTL = 0x0;
		constexpr uint8_t DAMP_MODE_PBTL = 0x04;

		constexpr uint8_t SELECT_PAGE = 0x00;
		constexpr uint8_t RESET_CTRL = 0x01;
		constexpr uint8_t DEVICE_CTRL_1 = 0x02;
		constexpr uint8_t DEVICE_CTRL_2 = 0x03;
		constexpr uint8_t I2C_PAGE_AUTO_INC = 0x0f;
		constexpr uint8_t SIG_CH_CTRL = 0x28;
		constexpr uint8_t CLOCK_DET_CTRL = 0x29;
		constexpr uint8_t SDOUT_SEL = 0x30;
		constexpr uint8_t I2S_CTRL = 0x31;
		constexpr uint8_t SAP_CTRL1 = 0x33;
		constexpr uint8_t SAP_CTRL2 = 0x34;
		constexpr uint8_t SAP_CTRL3 = 0x35;
		constexpr uint8_t FS_MON = 0x37;
		constexpr uint8_t BCK_MON = 0x38;
		constexpr uint8_t CLKDET_STATUS = 0x39;
		constexpr uint8_t CHANNEL_FORCE_HIZ = 0x40;
		constexpr uint8_t DIG_VOL_CTRL = 0x4c;
		constexpr uint8_t DIG_VOL_CTRL2 = 0x4e;
		constexpr uint8_t DIG_VOL_CTRL3 = 0x4f;
		constexpr uint8_t AUTO_MUTE_CTRL = 0x50;
		constexpr uint8_t AUTO_MUTE_TIME = 0x51;
		constexpr uint8_t ANA_CTRL = 0x53;
		constexpr uint8_t AGAIN = 0x54;
		constexpr uint8_t BQ_WR_CTRL1 = 0x5c;
		constexpr uint8_t DAC_CTRL = 0x5d;
		constexpr uint8_t ADR_PIN_CTRL = 0x60;
		constexpr uint8_t ADR_PIN_CONFIG = 0x61;
		constexpr uint8_t DSP_MISC = 0x66;
		constexpr uint8_t DIE_ID = 0x67;
		constexpr uint8_t POWER_STATE = 0x68;
		constexpr uint8_t AUTOMUTE_STATE = 0x69;
		constexpr uint8_t PHASE_CTRL = 0x6a;
		constexpr uint8_t SS_CTRL0 = 0x6b;
		constexpr uint8_t SS_CTRL1 = 0x6c;
		constexpr uint8_t SS_CTRL2 = 0x6d;
		constexpr uint8_t SS_CTRL3 = 0x6e;
		constexpr uint8_t SS_CTRL4 = 0x6f;
		constexpr uint8_t CHAN_FAULT = 0x70;
		constexpr uint8_t GLOBAL_FAULT1 = 0x71;
		constexpr uint8_t GLOBAL_FAULT2 = 0x72;
		constexpr uint8_t OT_WARNING = 0x73;
		constexpr uint8_t PIN_CONTROL1 = 0x74;
		constexpr uint8_t PIN_CONTROL2 = 0x75;
		constexpr uint8_t MISC_CONTROL = 0x76;
		constexpr uint8_t FAULT_CLEAR = 0x78;
		constexpr uint8_t SELECT_BOOK = 0x7F;
	}

	class M:public CodecManager::M
	{
	private:
		iI2CPort *i2c_port;
		TAS580x::ADDR7bit addr;
		gpio_num_t power_down;
		
		ErrorCode transmitRegisters(const CFG::tas5805m_cfg_reg_t *conf_buf, int size)
		{
			int i = 0;
			while (i < size)
			{
				switch (conf_buf[i].offset)
				{
				case CFG::META_SWITCH:
					// Used in legacy applications.  Ignored here.
					break;
				case CFG::META_DELAY:
					vTaskDelay(pdMS_TO_TICKS(conf_buf[i].value));
					break;
				case CFG::META_BURST:
					RETURN_ON_ERRORCODE(i2c_port->WriteReg((uint8_t)this->addr, conf_buf[i + 1].offset, (unsigned char *)(&conf_buf[i + 1].value), conf_buf[i].value));
					i += (conf_buf[i].value / 2) + 1;
					break;
				default:
					RETURN_ON_ERRORCODE(i2c_port->WriteSingleReg((uint8_t)this->addr, conf_buf[i].offset, conf_buf[i].value));
					break;
				}
				i++;
			}
			return ErrorCode::OK;
		}

		ErrorCode SwitchToBookAndPage(uint8_t book, uint8_t page){
			RETURN_ON_ERRORCODE(i2c_port->WriteSingleReg((uint8_t)this->addr, R::SELECT_PAGE, 0));
			RETURN_ON_ERRORCODE(i2c_port->WriteSingleReg((uint8_t)this->addr, R::SELECT_BOOK, book));
			RETURN_ON_ERRORCODE(i2c_port->WriteSingleReg((uint8_t)this->addr, R::SELECT_PAGE, page));
			return ErrorCode::OK;
		}

	public:
		M(iI2CPort *i2c_port, TAS580x::ADDR7bit addr, gpio_num_t power_down) : i2c_port(i2c_port), addr(addr), power_down(power_down)
		{
		}
		
		// 0b00000=0dB, 0b11111=-15.5dB
		ErrorCode SetAnalogGain(uint8_t gain_0to31=0x00)
		{
			return i2c_port->WriteSingleReg((uint8_t)this->addr, R::AGAIN, gain_0to31 & 0x1F);
		}
		// 0=MAX Volume, 254=MIN Volume, 255=Mute, 50 is loud, 120 is quiet
		ErrorCode SetDigitalVolume(uint8_t volume = 0b00110000)
		{
			ESP_LOGI(TAG, "SetDigitalVolume: Writing %02X to DIG_VOL_CTRL", volume);
			return i2c_port->WriteSingleReg((uint8_t)this->addr, R::DIG_VOL_CTRL, volume);
		}

		ErrorCode GetDigitalVolume(uint8_t *volume)
		{
			return i2c_port->ReadReg((uint8_t)this->addr, R::DIG_VOL_CTRL, volume, 1);
		}

		ErrorCode SetPowerState(bool power) override{
			uint8_t reg = 0;
			RETURN_ON_ERRORCODE(i2c_port->ReadReg((uint8_t)this->addr, R::DEVICE_CTRL_2, &reg, 1));
			if((power && ((reg&0x03)==0x03)) || (!power && ((reg&0x03)==0x00))){
				//if power setting is as we want to have it --> return!
				return ErrorCode::OK;
			}
			if (power)
			{
				reg |= 0x3;
			}
			else
			{
				reg &= (~0x03);
			}
			
			ESP_LOGI(TAG, "SetPowerState: Writing %02X to DEVICE_CTRL_2", reg);
			return i2c_port->WriteSingleReg((uint8_t)this->addr, R::DEVICE_CTRL_2, reg);
		}
        
		ErrorCode SetVolume(uint8_t volume) override{
			return SetDigitalVolume(volume);
		}

		void PowerUp()
		{
			gpio_set_level(power_down, 1);
		}

		void PowerDown()
		{
			gpio_set_level(power_down, 0);
		}

		ErrorCode SetControlState(CTRL_STATE state){
			uint8_t reg = 0;
			RETURN_ON_ERRORCODE(i2c_port->ReadReg((uint8_t)this->addr, R::DEVICE_CTRL_2, &reg, 1));
			reg = (reg && 0xFC)|(uint8_t)state;
			return i2c_port->WriteSingleReg((uint8_t)this->addr, R::DEVICE_CTRL_2, reg);
		}

		ErrorCode Init(uint8_t initialVolume = 50)
		{
			ESP_LOGI(TAG, "setup of the audio amp begins");
			gpio_set_level(power_down, 0);
			gpio_reset_pin(power_down);
			gpio_set_direction(power_down, GPIO_MODE_OUTPUT);
			PowerDown();
			vTaskDelay(pdMS_TO_TICKS(20));
			PowerUp();
			vTaskDelay(pdMS_TO_TICKS(200));
			//set the device into HiZ state and enable DSP via the I2C control port.
			//Wait 5ms at least. Then initialize the DSP Coefficient
			SetControlState(CTRL_STATE::Hi_Z);
			vTaskDelay(pdMS_TO_TICKS(10));
			RETURN_ON_ERRORCODE(transmitRegisters(CFG::tas5805m_registers, sizeof(CFG::tas5805m_registers) / sizeof(CFG::tas5805m_registers[0])));
			//Reset pointers to book 0, page 0, aka. the main control port...
			RETURN_ON_ERRORCODE(SwitchToBookAndPage(0,0));
			RETURN_ON_ERRORCODE(SetAnalogGain(16));
			RETURN_ON_ERRORCODE(SetDigitalVolume(initialVolume));
			RETURN_ON_ERRORCODE(SetMuteFadingTime(MuteFadingTime::_53ms));
			//set the device to Play state.
			SetControlState(CTRL_STATE::PLAY);
			return ErrorCode::OK;
		}

		ErrorCode SetMuteFadingTime(MuteFadingTime fadingTimeMs)
		{
			uint8_t fade_reg = (uint8_t)fadingTimeMs;
			fade_reg |= (fade_reg << 4);
			ErrorCode ret = i2c_port->WriteSingleReg((uint8_t)this->addr, R::MUTE_TIME_REG_ADDR, fade_reg);
			return ret;
		}

		ErrorCode Mute(bool mute)
		{
			uint8_t mute_reg = 0;
			RETURN_ON_ERRORCODE(i2c_port->ReadReg((uint8_t)this->addr, R::DEVICE_CTRL_2, &mute_reg, 1));
			if (mute)
			{
				mute_reg |= 0x8;
			}
			else
			{
				mute_reg &= (~0x08);
			}
			return i2c_port->WriteSingleReg((uint8_t)this->addr, R::DEVICE_CTRL_2, mute_reg);
		}
	};
}
#undef TAG