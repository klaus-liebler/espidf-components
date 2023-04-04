#include <inttypes.h>
#include <i2c.hh>
#include <esp_log.h>
#include <string.h>
#include <common.hh>
#define TAG "PCA9685"
#include "pca9685.hh"

namespace PCA9685
{

	ErrorCode M::SoftwareReset(iI2CPort *i2cPort)
	{
		uint8_t data = SWRST;
		return i2cPort->Write(0x00, &data, 1);
	}

	ErrorCode M::Loop()
	{
		uint16_t firstOutput = 0;
		uint16_t lastOutput = 0;
		uint16_t buffer[32];
		size_t bufferIndex = 0;
		uint16_t nextOn = 0;
		// wir suchen den ersten output
		while (firstOutput < 16)
		{
			if (val[firstOutput] != written[firstOutput])
				break;
			firstOutput++;
		}
		if (firstOutput == 16)
		{
			return ErrorCode::OK;
		}
		for (int output = firstOutput; output < 16; output++)
		{
			uint16_t myval = val[output];
			if (myval != written[output])
			{
				lastOutput = output;
			}
			written[output] = myval;
			if (myval == 0)
			{
				buffer[bufferIndex++] = 0;		  // ON
				buffer[bufferIndex++] = FULL_OFF; // OFF
			}
			else if (myval == UINT16_MAX)
			{
				buffer[bufferIndex++] = FULL_ON; // ON
				buffer[bufferIndex++] = 0;		 // OFF
			}
			else
			{
				buffer[bufferIndex++] = nextOn;
				nextOn += myval >> 4; // ON
				nextOn%=4096;
				buffer[bufferIndex++] = nextOn;	 // OFF for this output ist the same as the ON for the next output
			}
		}
		size_t bytesToWrite = (lastOutput - firstOutput + 1) * 4;
		ESP_LOGI(TAG, "first=%d, last=%d, bytesToWrite=%i, buffer[0...5]=%04X %04X %04X %04X %04X %04X", firstOutput, lastOutput, bytesToWrite, buffer[0], buffer[1], buffer[2], buffer[3], buffer[4], buffer[5]);
		return i2cPort->WriteReg((uint8_t)device, 0x06 + 4 * firstOutput, (uint8_t *)buffer, bytesToWrite);
	}

	ErrorCode M::SetupStatic(iI2CPort *i2cPort, Device device, InvOutputs inv, OutputDriver outdrv, OutputNotEn outne, Frequency freq)
	{
		RETURN_ON_ERRORCODE(i2cPort->IsAvailable((uint8_t)device));
		RETURN_ON_ERRORCODE(SoftwareReset(i2cPort));
		
		//Sleep in to be able to program the frequency
		RETURN_ON_ERRORCODE(i2cPort->WriteSingleReg((uint8_t)device, MODE1, 1 << MODE1_SLEEP));

		// PRE_SCALE Register (possible in sleep mode only)
		RETURN_ON_ERRORCODE((i2cPort->WriteSingleReg((uint8_t)device, PRE_SCALE, (uint8_t)(freq))));

		/* MODE1 Register:
		 * Internal clock, not external
		 * Register Auto-Increment enabled
		 * Normal mode (not sleep)
		 * Does not respond to subaddresses
		 * Does not respond to All Call I2C-bus address
		 */
		RETURN_ON_ERRORCODE((i2cPort->WriteSingleReg((uint8_t)device, MODE1, (1 << MODE1_AI))));

		/* MODE2 Register:
		 * Outputs change on STOP command
		 */
		RETURN_ON_ERRORCODE(i2cPort->WriteSingleReg((uint8_t)device, MODE2, ((uint8_t)inv << MODE2_INVRT) | ((uint8_t)outdrv << MODE2_OUTDRV) | ((uint8_t)outne << MODE2_OUTNE0)));

		// Switch all off
		return SetOutputs(i2cPort, device, 0x0001, 0);
	}

	ErrorCode M::SetOutputs(iI2CPort *i2cPort, Device device, uint16_t mask, uint16_t val)
	{
		uint8_t data[64];
		// suche 1er Blöcke und übertrage die zusammen
		uint16_t offValue;
		uint16_t onValue;
		if (val == UINT16_MAX)
		{
			onValue = MAX_OUTPUT_VALUE;
			offValue = 0;
		}
		else if (val == 0)
		{
			onValue = 0;
			offValue = MAX_OUTPUT_VALUE;
		}
		else
		{
			onValue = 0;		   //((uint16_t)Output)*0xFF; //for phase shift to reduce EMI
			offValue = (val >> 4); // + onValue; //to make a 12bit-Value
		}
		uint8_t i = 0;
		while (mask > 0)
		{
			while (mask > 0 && !(mask & 0x0001))
			{
				mask >>= 1;
				i++;
			}
			uint8_t firstOne = i;
			while (mask > 0 && (mask & 0x0001))
			{
				mask >>= 1;
				i++;
			}
			uint8_t ones = i - firstOne;
			for (int j = 0; j < ones; j++)
			{
				data[4 * j + 0] = (uint8_t)(onValue & 0xFF);
				data[4 * j + 1] = (uint8_t)((onValue >> 8) & 0xFF);
				data[4 * j + 2] = (uint8_t)(offValue & 0xFF);
				data[4 * j + 3] = (uint8_t)((offValue >> 8) & 0xFF);
			}
			RETURN_ON_ERRORCODE(i2cPort->WriteReg((uint8_t)device, LEDn_ON_L(firstOne), (uint8_t *)data, 4 * ones));
		}
		return ErrorCode::OK;
	}

	/**
	 * @brief	Initializes the PCA9685
	 * @param	None
	 * @retval	1: A PCA9685 has been initialized
	 * @retval	0: Initialization failed
	 */
	ErrorCode M::Setup()
	{
		return SetupStatic(this->i2cPort, this->device, this->inv, this->outdrv, outne, freq);
	}

	ErrorCode M::SetOutputFull(Output Output, bool on)
	{
		if (this->i2cPort == 0)
		{
			ESP_LOGE(TAG, "i2c device is null");
			return ErrorCode::HARDWARE_NOT_INITIALIZED;
		}
		uint8_t data = 0xF0;
		// 07,4 on, 09,4 full off
		return i2cPort->WriteReg((uint8_t)device, (uint16_t)(0x06 + 4 * (uint8_t)Output + on ? 1 : 3), &data, 1);
	}

	/**
	 * @brief	Sets a specific output for a PCA9685
	 * @param	Address: The address to the PCA9685
	 * @param	Output: The output to set
	 * @param	OnValue: The value at which the output will turn on
	 * @param	OffValue: The value at which the output will turn off
	 * @retval	None
	 */
	ErrorCode M::SetOutput(Output Output, uint16_t OnValue, uint16_t OffValue)
	{
		if (this->i2cPort == 0)
		{
			ESP_LOGE(TAG, "i2c device is null");
			return ErrorCode::HARDWARE_NOT_INITIALIZED;
		}
		// Optional: PCA9685_I2C_SlaveAtAddress(Address), might make things slower
		uint8_t data[4] = {(uint8_t)(OnValue & 0xFF), (uint8_t)((OnValue >> 8) & 0x1F), (uint8_t)(OffValue & 0xFF), (uint8_t)((OffValue >> 8) & 0x1F)};
		return i2cPort->WriteReg((uint8_t)device, LEDn_ON_L((uint8_t)Output), data, 4);
	}

	/**
	 * @brief	Sets a specific output for a PCA9685 based on an approximate duty cycle
	 * @param	Address: The address to the PCA9685
	 * @param	Output: The output to set
	 * @param	val: The duty cycle for the output 0...UINT16_MAX
	 * @retval	None
	 */
	ErrorCode M::SetDutyCycleForOutput(Output Output, uint16_t val)
	{
		if (this->i2cPort == 0)
		{
			ESP_LOGE(TAG, "i2c device is null");
			return ErrorCode::HARDWARE_NOT_INITIALIZED;
		}
		uint16_t offValue;
		uint16_t onValue;
		if (val == UINT16_MAX)
		{
			onValue = MAX_OUTPUT_VALUE;
			offValue = 0;
		}
		else if (val == 0)
		{
			onValue = 0;
			offValue = MAX_OUTPUT_VALUE;
		}
		else
		{
			onValue = 0;		   //((uint16_t)Output)*0xFF; //for phase shift to reduce EMI
			offValue = (val >> 4); // + onValue; //to make a 12bit-Value
		}
		return SetOutput(Output, onValue, offValue);
	}

	M::M(iI2CPort *i2cPort, Device device, InvOutputs inv, OutputDriver outdrv, OutputNotEn outne, Frequency freq) : i2cPort(i2cPort), device(device), inv(inv), outdrv(outdrv), outne(outne), freq(freq)
	{
		for (int i = 0; i < 16; i++)
		{
			val[i] = 0;
			written[i] = 0;
		}
	}
}