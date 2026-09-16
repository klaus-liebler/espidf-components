#pragma once
#include <inttypes.h>
#include "errorcodes.hh"
#include "common.hh"
#include <esp_log.h>
#include <codec_manager.hh>
#define TAG "NS4168"

// NS4168: mono class-D I2S-Verstaerker, kein I2C, keine Software-Lautstaerke -- fixer analoger
// Gain. Einziger nicht-I2S-Pin ist SD (Shutdown, aktiv LOW lt. Datenblatt: LOW=Shutdown,
// HIGH/offen=aktiv). shutdown_pin=GPIO_NUM_NC bedeutet "nicht separat angesteuert" (z.B. auf der
// Platine fest auf HIGH gezogen).
namespace NS4168
{
	class M:public CodecManager::aI2sCodecManager
	{
	private:
        gpio_num_t shutdown_pin;

	public:
		M(
            gpio_num_t shutdown_pin,
            uint32_t initialSampleRateHz = 44100) :
            CodecManager::aI2sCodecManager(initialSampleRateHz, CodecManager::eChannels::ONE, CodecManager::eSampleBits::SIXTEEN),
            shutdown_pin(shutdown_pin)
		{
		}

        ErrorCode SetVolume(uint8_t volume) override{
            // Kein Software-Lautstaerkeregler auf dem Chip -- Lautstaerke kommt ausschliesslich
            // aus dem PCM-Signalpegel (s. AudioPlayer::Player, das volume0_255 dafuer nutzt).
            return ErrorCode::OK;
        }

		ErrorCode SetPowerState(bool power) override{
            if(shutdown_pin != GPIO_NUM_NC){
                gpio_set_level(shutdown_pin, power?1:0);
            }
            return ErrorCode::OK;
		}

		ErrorCode Init(gpio_num_t bck, gpio_num_t ws, gpio_num_t data)
		{
			RETURN_ON_ERRORCODE(this->InitI2sEsp32(GPIO_NUM_NC, bck, ws, data));
            if(shutdown_pin != GPIO_NUM_NC){
                gpio_set_level(shutdown_pin, 1);
                gpio_set_direction(shutdown_pin, GPIO_MODE_OUTPUT);
            }
			return ErrorCode::OK;
		}
	};
}
#undef TAG
