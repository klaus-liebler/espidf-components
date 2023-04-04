#pragma once
#include <inttypes.h>
#include "tas5805m_reg_cfg.h"
#include "errorcodes.hh"
#include "common.hh"
#include <esp_log.h>

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

	constexpr uint8_t TAS5805M_VOLUME_MAX = (158);
	constexpr uint8_t TAS5805M_VOLUME_MIN = (0);

	constexpr uint32_t tas5805m_volume[] = {
		0x0000001B, //0, -110dB
		0x0000001E, //1, -109dB
		0x00000021, //2, -108dB
		0x00000025, //3, -107dB
		0x0000002A, //4, -106dB
		0x0000002F, //5, -105dB
		0x00000035, //6, -104dB
		0x0000003B, //7, -103dB
		0x00000043, //8, -102dB
		0x0000004B, //9, -101dB
		0x00000054, //10, -100dB
		0x0000005E, //11, -99dB
		0x0000006A, //12, -98dB
		0x00000076, //13, -97dB
		0x00000085, //14, -96dB
		0x00000095, //15, -95dB
		0x000000A7, //16, -94dB
		0x000000BC, //17, -93dB
		0x000000D3, //18, -92dB
		0x000000EC, //19, -91dB
		0x00000109, //20, -90dB
		0x0000012A, //21, -89dB
		0x0000014E, //22, -88dB
		0x00000177, //23, -87dB
		0x000001A4, //24, -86dB
		0x000001D8, //25, -85dB
		0x00000211, //26, -84dB
		0x00000252, //27, -83dB
		0x0000029A, //28, -82dB
		0x000002EC, //29, -81dB
		0x00000347, //30, -80dB
		0x000003AD, //31, -79dB
		0x00000420, //32, -78dB
		0x000004A1, //33, -77dB
		0x00000532, //34, -76dB
		0x000005D4, //35, -75dB
		0x0000068A, //36, -74dB
		0x00000756, //37, -73dB
		0x0000083B, //38, -72dB
		0x0000093C, //39, -71dB
		0x00000A5D, //40, -70dB
		0x00000BA0, //41, -69dB
		0x00000D0C, //42, -68dB
		0x00000EA3, //43, -67dB
		0x0000106C, //44, -66dB
		0x0000126D, //45, -65dB
		0x000014AD, //46, -64dB
		0x00001733, //47, -63dB
		0x00001A07, //48, -62dB
		0x00001D34, //49, -61dB
		0x000020C5, //50, -60dB
		0x000024C4, //51, -59dB
		0x00002941, //52, -58dB
		0x00002E49, //53, -57dB
		0x000033EF, //54, -56dB
		0x00003A45, //55, -55dB
		0x00004161, //56, -54dB
		0x0000495C, //57, -53dB
		0x0000524F, //58, -52dB
		0x00005C5A, //59, -51dB
		0x0000679F, //60, -50dB
		0x00007444, //61, -49dB
		0x00008274, //62, -48dB
		0x0000925F, //63, -47dB
		0x0000A43B, //64, -46dB
		0x0000B845, //65, -45dB
		0x0000CEC1, //66, -44dB
		0x0000E7FB, //67, -43dB
		0x00010449, //68, -42dB
		0x0001240C, //69, -41dB
		0x000147AE, //70, -40dB
		0x00016FAA, //71, -39dB
		0x00019C86, //72, -38dB
		0x0001CEDC, //73, -37dB
		0x00020756, //74, -36dB
		0x000246B5, //75, -35dB
		0x00028DCF, //76, -34dB
		0x0002DD96, //77, -33dB
		0x00033718, //78, -32dB
		0x00039B87, //79, -31dB
		0x00040C37, //80, -30dB
		0x00048AA7, //81, -29dB
		0x00051884, //82, -28dB
		0x0005B7B1, //83, -27dB
		0x00066A4A, //84, -26dB
		0x000732AE, //85, -25dB
		0x00081385, //86, -24dB
		0x00090FCC, //87, -23dB
		0x000A2ADB, //88, -22dB
		0x000B6873, //89, -21dB
		0x000CCCCD, //90, -20dB
		0x000E5CA1, //91, -19dB
		0x00101D3F, //92, -18dB
		0x0012149A, //93, -17dB
		0x00144961, //94, -16dB
		0x0016C311, //95, -15dB
		0x00198A13, //96, -14dB
		0x001CA7D7, //97, -13dB
		0x002026F3, //98, -12dB
		0x00241347, //99, -11dB
		0x00287A27, //100, -10dB
		0x002D6A86, //101, -9dB
		0x0032F52D, //102, -8dB
		0x00392CEE, //103, -7dB
		0x004026E7, //104, -6dB
		0x0047FACD, //105, -5dB
		0x0050C336, //106, -4dB
		0x005A9DF8, //107, -3dB
		0x0065AC8C, //108, -2dB
		0x00721483, //109, -1dB
		0x00800000, //110, 0dB
		0x008F9E4D, //111, 1dB
		0x00A12478, //112, 2dB
		0x00B4CE08, //113, 3dB
		0x00CADDC8, //114, 4dB
		0x00E39EA9, //115, 5dB
		0x00FF64C1, //116, 6dB
		0x011E8E6A, //117, 7dB
		0x0141857F, //118, 8dB
		0x0168C0C6, //119, 9dB
		0x0194C584, //120, 10dB
		0x01C62940, //121, 11dB
		0x01FD93C2, //122, 12dB
		0x023BC148, //123, 13dB
		0x02818508, //124, 14dB
		0x02CFCC01, //125, 15dB
		0x0327A01A, //126, 16dB
		0x038A2BAD, //127, 17dB
		0x03F8BD7A, //128, 18dB
		0x0474CD1B, //129, 19dB
		0x05000000, //130, 20dB
		0x059C2F02, //131, 21dB
		0x064B6CAE, //132, 22dB
		0x07100C4D, //133, 23dB
		0x07ECA9CD, //134, 24dB
		0x08E43299, //135, 25dB
		0x09F9EF8E, //136, 26dB
		0x0B319025, //137, 27dB
		0x0C8F36F2, //138, 28dB
		0x0E1787B8, //139, 29dB
		0x0FCFB725, //140, 30dB
		0x11BD9C84, //141, 31dB
		0x13E7C594, //142, 32dB
		0x16558CCB, //143, 33dB
		0x190F3254, //144, 34dB
		0x1C1DF80E, //145, 35dB
		0x1F8C4107, //146, 36dB
		0x2365B4BF, //147, 37dB
		0x27B766C2, //148, 38dB
		0x2C900313, //149, 39dB
		0x32000000, //150, 40dB
		0x3819D612, //151, 41dB
		0x3EF23ECA, //152, 42dB
		0x46A07B07, //153, 43dB
		0x4F3EA203, //154, 44dB
		0x58E9F9F9, //155, 45dB
		0x63C35B8E, //156, 46dB
		0x6FEFA16D, //157, 47dB
		0x7D982575, //158, 48dB
	};

	namespace R
	{
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

	class M
	{
	private:
		iI2CPort* i2c_port;
		TAS580x::ADDR7bit addr;
		gpio_num_t power_down;
		bool muted{false};
		

		ErrorCode tas5805m_transmit_registers(const tas5805m_cfg_reg_t *conf_buf, int size)
		{
			int i = 0;
			ErrorCode ret = ErrorCode::OK;
			while (i < size)
			{
				switch (conf_buf[i].offset)
				{
				case CFG_META_SWITCH:
					// Used in legacy applications.  Ignored here.
					break;
				case CFG_META_DELAY:
					vTaskDelay(pdMS_TO_TICKS(conf_buf[i].value));
					break;
				case CFG_META_BURST:
					ret = i2c_port->WriteReg((uint8_t)this->addr, conf_buf[i + 1].offset, (unsigned char *)(&conf_buf[i + 1].value), conf_buf[i].value);
					i += (conf_buf[i].value / 2) + 1;
					break;
				case CFG_END_1:
					if (CFG_END_2 == conf_buf[i + 1].offset && CFG_END_3 == conf_buf[i + 2].offset)
					{
						ESP_LOGI(TAG, "End of tms5805m reg: %d\n", i);
					}
					break;
				default:
					ret = i2c_port->WriteSingleReg((uint8_t)this->addr, conf_buf[i].offset, conf_buf[i].value);
					break;
				}
				i++;
			}
			if (ret != ErrorCode::OK)
			{
				ESP_LOGE(TAG, "Fail to load configuration to tas5805m");
				return ret;
			}
			ESP_LOGI(TAG, "%s:  write %d reg done", __FUNCTION__, i);
			return ret;
		}

	public:
		M(iI2CPort* i2c_port, TAS580x::ADDR7bit addr, gpio_num_t power_down) : i2c_port(i2c_port), addr(addr), power_down(power_down)
		{
		}
		//0b00000=0dB, 0b11111=-15,5dB
		ErrorCode SetAnalogGain(uint8_t gain_0to31)
		{
			return i2c_port->WriteSingleReg((uint8_t)this->addr, R::AGAIN, gain_0to31);
		}
		//0=MAX Volume, 254=MIN Volume, 255=Mute
		ErrorCode SetDigitalVolume(uint8_t volume = 0b00110000)
		{
			return i2c_port->WriteSingleReg((uint8_t)this->addr, R::DIG_VOL_CTRL, volume);
		}

		static inline int get_volume_index(int vol)
		{
			int index;

			index = vol;

			if (index < TAS5805M_VOLUME_MIN)
				index = TAS5805M_VOLUME_MIN;

			if (index > TAS5805M_VOLUME_MAX)
				index = TAS5805M_VOLUME_MAX;

			return index;
		}

		ErrorCode SetVolume(int vol)
		{
			int vol_idx = 0;

			if (vol < TAS5805M_VOLUME_MIN) {
				vol = TAS5805M_VOLUME_MIN;
			}
			if (vol > TAS5805M_VOLUME_MAX) {
				vol = TAS5805M_VOLUME_MAX;
			}
			vol_idx = vol / 5;

			ErrorCode ret = ErrorCode::OK;


			ret = i2c_port->WriteSingleReg((uint8_t)this->addr, R::DIG_VOL_CTRL, tas5805m_volume[vol_idx]);
			ESP_LOGI(TAG, "volume = 0x%x", (unsigned int)tas5805m_volume[vol_idx]);
			return ret;
		}

		ErrorCode SetVolumeOld(int vol)
		{
			unsigned int index;
			uint32_t volume_hex;
			uint8_t byte4;
			uint8_t byte3;
			uint8_t byte2;
			uint8_t byte1;

			index = get_volume_index(vol);
			volume_hex = tas5805m_volume[index];

			byte4 = ((volume_hex >> 24) & 0xFF);
			byte3 = ((volume_hex >> 16) & 0xFF);
			byte2 = ((volume_hex >> 8) & 0xFF);
			byte1 = ((volume_hex >> 0) & 0xFF);

			//w 58 00 00
			RETURN_ON_ERRORCODE(i2c_port->WriteSingleReg((uint8_t)this->addr, R::SELECT_PAGE, 0));
			//w 58 7f 8c
			RETURN_ON_ERRORCODE(i2c_port->WriteSingleReg( (uint8_t)this->addr, R::SELECT_BOOK, 0x8c));
			//w 58 00 2a
			RETURN_ON_ERRORCODE(i2c_port->WriteSingleReg((uint8_t)this->addr, R::SELECT_PAGE, 0x2A));
			//w 58 24 xx xx xx xx
			RETURN_ON_ERRORCODE(i2c_port->WriteSingleReg((uint8_t)this->addr, 0x24, byte4));
			RETURN_ON_ERRORCODE(i2c_port->WriteSingleReg((uint8_t)this->addr, 0x25, byte3));
			RETURN_ON_ERRORCODE(i2c_port->WriteSingleReg((uint8_t)this->addr, 0x26, byte2));
			RETURN_ON_ERRORCODE(i2c_port->WriteSingleReg((uint8_t)this->addr, 0x27, byte1));
			//w 58 28 xx xx xx xx
			RETURN_ON_ERRORCODE(i2c_port->WriteSingleReg((uint8_t)this->addr, 0x28, byte4));
			RETURN_ON_ERRORCODE(i2c_port->WriteSingleReg((uint8_t)this->addr, 0x29, byte3));
			RETURN_ON_ERRORCODE(i2c_port->WriteSingleReg((uint8_t)this->addr, 0x2a, byte2));
			RETURN_ON_ERRORCODE(i2c_port->WriteSingleReg((uint8_t)this->addr, 0x2b, byte1));
			return ErrorCode::OK;
		}

		ErrorCode Mute(bool mute)
		{
			if(mute==muted) return ErrorCode::OK;
			uint8_t DEVICE_CTRL_2_value = 0;
			uint8_t SAP_CTRL3_value = 0;

			if (mute)
			{
				//mute both left & right channels
				DEVICE_CTRL_2_value = 0x0b;
				SAP_CTRL3_value = 0x00;
				muted=true;
			}
			else
			{
				//unmute
				DEVICE_CTRL_2_value = 0x03;
				SAP_CTRL3_value = 0x11;
				muted=false;
			}
			RETURN_ON_ERRORCODE(i2c_port->WriteSingleReg((uint8_t)this->addr, R::SELECT_PAGE, 0));
			RETURN_ON_ERRORCODE(i2c_port->WriteSingleReg((uint8_t)this->addr, R::SELECT_BOOK, 0x00));
			RETURN_ON_ERRORCODE(i2c_port->WriteSingleReg((uint8_t)this->addr, R::SELECT_PAGE, 0));
			RETURN_ON_ERRORCODE(i2c_port->WriteSingleReg((uint8_t)this->addr, R::DEVICE_CTRL_2, DEVICE_CTRL_2_value));
			RETURN_ON_ERRORCODE(i2c_port->WriteSingleReg((uint8_t)this->addr, R::SAP_CTRL3, SAP_CTRL3_value));

			return ErrorCode::OK;
		}

		void PowerUp(){
			gpio_set_level(power_down, 1);
		}

		void PowerDown(){
			gpio_set_level(power_down, 0);
		}

		ErrorCode Init(uint8_t initialVolume_0_158)
		{
			ESP_LOGI(TAG, "setup of the audio amp begins");
			ErrorCode ret = ErrorCode::OK;
			/* Register the PDN pin as output and write 1 to enable the TAS chip */
			gpio_set_level(power_down, 1);
			gpio_reset_pin(power_down);
			gpio_set_direction(power_down, GPIO_MODE_OUTPUT);
			gpio_set_pull_mode(power_down, GPIO_FLOATING);

			/* sound is ready */
			vTaskDelay(pdMS_TO_TICKS(200));

			/* set PDN to 1 */
			gpio_set_level(power_down, 1);
			vTaskDelay(pdMS_TO_TICKS(100));

			ESP_LOGI(TAG, "Setting to HI Z");
			RETURN_ON_ERRORCODE(i2c_port->WriteSingleReg((uint8_t)this->addr, R::DEVICE_CTRL_2, (uint8_t)0x02));
			vTaskDelay(pdMS_TO_TICKS(100));


			ESP_LOGI(TAG, "Setting to PLAY");
			RETURN_ON_ERRORCODE(i2c_port->WriteSingleReg((uint8_t)this->addr, R::DEVICE_CTRL_2, (uint8_t)0x03));

			vTaskDelay(pdMS_TO_TICKS(100));

			uint8_t h70h71h72[3];
			RETURN_ON_ERRORCODE(i2c_port->ReadReg((uint8_t)this->addr, R::CHAN_FAULT, h70h71h72, 3));

			ESP_LOGI(TAG, "0x70 Register: %d", h70h71h72[0]);
			ESP_LOGI(TAG, "0x71 Register: %d", h70h71h72[1]);
			ESP_LOGI(TAG, "0x72 Register: %d", h70h71h72[2]);

			RETURN_ON_ERRORCODE(SetVolume(initialVolume_0_158));
			return ret;
		}
	};
}

#undef TAG