#pragma once
#include <inttypes.h>
#include "errorcodes.hh"
#include "common.hh"
#include <esp_log.h>
#include <math.h>
#include <codec_manager.hh>
#define TAG "MAX98357"

namespace MAX98357
{
	enum class eGain
	{
		GND_through_100k_15dB=15,
        GND_12dB=12,
        OPEN_9db=9,
        VDD_6dB=6,
        VDD_through_100k_3dB=3,
	};

	class M:public CodecManager::aI2sCodecManager
	{
	private:
        gpio_num_t gain_pin;
		gpio_num_t shutdown_pin;
        uint8_t volumeSpeakers = 127;

	public:
		M(
            gpio_num_t gain_pin, 
            gpio_num_t shutdown_pin, 
            uint8_t initialVolume = 127,
            uint32_t initialSampleRateHz = 44100) :
            CodecManager::aI2sCodecManager(initialSampleRateHz, CodecManager::eChannels::ONE, CodecManager::eSampleBits::SIXTEEN),
            gain_pin(gain_pin), 
            shutdown_pin(shutdown_pin),
            volumeSpeakers(initialVolume)
		{
		}

        ErrorCode SetVolume(uint8_t volume) override{
            if(volume>160) SetAnalogGain(eGain::GND_12dB);
            else if(volume>80) SetAnalogGain(eGain::OPEN_9db);
            else SetAnalogGain(eGain::VDD_6dB);
            return ErrorCode::OK;
        }
		
		ErrorCode SetAnalogGain(eGain gain)
		{
			if(gain_pin == GPIO_NUM_NC){
                return ErrorCode::OK;
            }
            switch (gain)
            {
            case eGain::GND_through_100k_15dB:
            case eGain::GND_12dB:
                gpio_set_level(gain_pin, 0);
                gpio_set_direction(gain_pin, GPIO_MODE_OUTPUT);
                break;
            case eGain::OPEN_9db:
                gpio_set_direction(gain_pin, GPIO_MODE_INPUT);
                break;
            case eGain::VDD_6dB:
            case eGain::VDD_through_100k_3dB:
                gpio_set_level(gain_pin, 1);
                gpio_set_direction(gain_pin, GPIO_MODE_OUTPUT);
                break;
            }
            return ErrorCode::OK;
		}

		ErrorCode SetPowerState(bool power) override{
            gpio_set_level(shutdown_pin, power?0:1);
            return ErrorCode::OK;
		}

        
		ErrorCode Init(gpio_num_t bck, gpio_num_t ws, gpio_num_t data)
		{
			ESP_LOGI(TAG, "Setup I2S");
			RETURN_ON_ERRORCODE(this->InitI2sEsp32(GPIO_NUM_NC, bck, ws, data));
			ESP_LOGI(TAG, "Setup of the MAX98357 begins");
            gpio_set_level(gain_pin, 1);
            gpio_set_direction(shutdown_pin, GPIO_MODE_OUTPUT);
			SetAnalogGain(eGain::GND_12dB);
			return ErrorCode::OK;
		}
	};
}
#undef TAG