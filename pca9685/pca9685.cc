#include <inttypes.h>
#include <esp_log.h>
#include <string.h>
#include <common-esp32.hh>
#define TAG "PCA9685"
#include "pca9685.hh"

namespace PCA9685
{
	static constexpr uint32_t I2C_TIMEOUT_MS = 1000;
	static constexpr uint32_t I2C_SCL_SPEED_HZ = 100000;

	static ErrorCode EnsureDeviceHandle(i2c_master_bus_handle_t master_bus_handle, Device device, i2c_master_dev_handle_t &dev_handle)
	{
		if (master_bus_handle == nullptr)
		{
			ESP_LOGE(TAG, "I2C bus handle is null");
			return ErrorCode::GENERIC_ERROR;
		}
		if (dev_handle != nullptr)
		{
			return ErrorCode::OK;
		}

		const uint8_t address = (uint8_t)device;
		esp_err_t rc = i2c_master_probe(master_bus_handle, address, I2C_TIMEOUT_MS);
		if (rc != ESP_OK)
		{
			ESP_LOGW(TAG, "Device 0x%02X not available, err=%d", address, rc);
			return ErrorCode::DEVICE_NOT_RESPONDING;
		}

		i2c_device_config_t dev_cfg = {
			.dev_addr_length = I2C_ADDR_BIT_LEN_7,
			.device_address = address,
			.scl_speed_hz = I2C_SCL_SPEED_HZ,
		};
		rc = i2c_master_bus_add_device(master_bus_handle, &dev_cfg, &dev_handle);
		if (rc != ESP_OK)
		{
			ESP_LOGE(TAG, "Could not add I2C device 0x%02X, err=%d", address, rc);
			return ErrorCode::DEVICE_NOT_RESPONDING;
		}
		return ErrorCode::OK;
	}

	static ErrorCode WriteReg(i2c_master_dev_handle_t dev_handle, uint8_t reg, const uint8_t *data, size_t len)
	{
		if (dev_handle == nullptr || data == nullptr)
		{
			return ErrorCode::GENERIC_ERROR;
		}
		if (len > 64)
		{
			return ErrorCode::INDEX_OUT_OF_BOUNDS;
		}

		uint8_t txbuf[65];
		txbuf[0] = reg;
		memcpy(txbuf + 1, data, len);
		esp_err_t rc = i2c_master_transmit(dev_handle, txbuf, len + 1, I2C_TIMEOUT_MS);
		return rc == ESP_OK ? ErrorCode::OK : ErrorCode::DEVICE_NOT_RESPONDING;
	}

	static ErrorCode WriteSingleReg(i2c_master_dev_handle_t dev_handle, uint8_t reg, uint8_t value)
	{
		uint8_t txbuf[2] = {reg, value};
		esp_err_t rc = i2c_master_transmit(dev_handle, txbuf, sizeof(txbuf), I2C_TIMEOUT_MS);
		return rc == ESP_OK ? ErrorCode::OK : ErrorCode::DEVICE_NOT_RESPONDING;
	}

	ErrorCode M::SoftwareReset(i2c_master_bus_handle_t master_bus_handle)
	{
		if (master_bus_handle == nullptr)
		{
			return ErrorCode::GENERIC_ERROR;
		}

		uint8_t data = SWRST;
		i2c_master_dev_handle_t general_call_dev = nullptr;
		i2c_device_config_t dev_cfg = {
			.dev_addr_length = I2C_ADDR_BIT_LEN_7,
			.device_address = 0x00,
			.scl_speed_hz = I2C_SCL_SPEED_HZ,
		};
		esp_err_t rc = i2c_master_bus_add_device(master_bus_handle, &dev_cfg, &general_call_dev);
		if (rc != ESP_OK)
		{
			ESP_LOGE(TAG, "Could not add general-call device, err=%d", rc);
			return ErrorCode::DEVICE_NOT_RESPONDING;
		}
		rc = i2c_master_transmit(general_call_dev, &data, 1, I2C_TIMEOUT_MS);
		return rc == ESP_OK ? ErrorCode::OK : ErrorCode::DEVICE_NOT_RESPONDING;
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
		//ESP_LOGI(TAG, "first=%d, last=%d, bytesToWrite=%i, buffer[0...5]=%04X %04X %04X %04X %04X %04X", firstOutput, lastOutput, bytesToWrite, buffer[0], buffer[1], buffer[2], buffer[3], buffer[4], buffer[5]);
		if (this->dev_handle == nullptr)
		{
			return ErrorCode::NOT_YET_INITIALIZED;
		}

		uint8_t write_buf[64] = {0};
		for (size_t i = 0; i < (bytesToWrite / 2); ++i)
		{
			write_buf[2 * i] = (uint8_t)(buffer[i] & 0xFF);
			write_buf[2 * i + 1] = (uint8_t)((buffer[i] >> 8) & 0xFF);
		}
		return WriteReg(this->dev_handle, (uint8_t)(0x06 + 4 * firstOutput), write_buf, bytesToWrite);
	}

	ErrorCode M::SetupStatic(i2c_master_bus_handle_t master_bus_handle, Device device, InvOutputs inv, OutputDriver outdrv, OutputNotEn outne, Frequency freq)
	{
		i2c_master_dev_handle_t dev_handle = nullptr;
		RETURN_ON_ERRORCODE(EnsureDeviceHandle(master_bus_handle, device, dev_handle));
		RETURN_ON_ERRORCODE(SoftwareReset(master_bus_handle));
		
		//Sleep in to be able to program the frequency
		RETURN_ON_ERRORCODE(WriteSingleReg(dev_handle, MODE1, 1 << MODE1_SLEEP));

		// PRE_SCALE Register (possible in sleep mode only)
		RETURN_ON_ERRORCODE(WriteSingleReg(dev_handle, PRE_SCALE, (uint8_t)(freq)));

		/* MODE1 Register:
		 * Internal clock, not external
		 * Register Auto-Increment enabled
		 * Normal mode (not sleep)
		 * Does not respond to subaddresses
		 * Does not respond to All Call I2C-bus address
		 */
		RETURN_ON_ERRORCODE(WriteSingleReg(dev_handle, MODE1, (1 << MODE1_AI)));

		/* MODE2 Register:
		 * Outputs change on STOP command
		 */
		RETURN_ON_ERRORCODE(WriteSingleReg(dev_handle, MODE2, ((uint8_t)inv << MODE2_INVRT) | ((uint8_t)outdrv << MODE2_OUTDRV) | ((uint8_t)outne << MODE2_OUTNE0)));

		// Switch all off
		return SetOutputs(master_bus_handle, device, 0x0001, 0);
	}

	ErrorCode M::SetOutputs(i2c_master_bus_handle_t master_bus_handle, Device device, uint16_t mask, uint16_t val)
	{
		i2c_master_dev_handle_t dev_handle = nullptr;
		RETURN_ON_ERRORCODE(EnsureDeviceHandle(master_bus_handle, device, dev_handle));

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
			RETURN_ON_ERRORCODE(WriteReg(dev_handle, LEDn_ON_L(firstOne), (uint8_t *)data, 4 * ones));
		}
		return ErrorCode::OK;
	}

	/**
	 * @brief	Initializes the PCA9685
	 * @param	None
	 * @retval	1: A PCA9685 has been initialized
	 * @retval	0: Initialization failed
	 */
	ErrorCode M::Setup(i2c_master_bus_handle_t master_bus_handle)
	{
		this->master_bus_handle = master_bus_handle;
		this->dev_handle = nullptr;
		RETURN_ON_ERRORCODE(EnsureDeviceHandle(this->master_bus_handle, this->device, this->dev_handle));
		return SetupStatic(this->master_bus_handle, this->device, this->inv, this->outdrv, outne, freq);
	}

	ErrorCode M::SetOutputFull(Output Output, bool on)
	{
		if (this->dev_handle == nullptr)
		{
			ESP_LOGE(TAG, "i2c device is null");
			return ErrorCode::NOT_YET_INITIALIZED;
		}
		uint8_t data = 0xF0;
		// 07,4 on, 09,4 full off
		const uint8_t reg = (uint8_t)(0x06 + 4 * (uint8_t)Output + (on ? 1 : 3));
		return WriteReg(this->dev_handle, reg, &data, 1);
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
		if (this->dev_handle == nullptr)
		{
			//ESP_LOGE(TAG, "i2c device is null");
			return ErrorCode::NOT_YET_INITIALIZED;
		}
		// Optional: PCA9685_I2C_SlaveAtAddress(Address), might make things slower
		uint8_t data[4] = {(uint8_t)(OnValue & 0xFF), (uint8_t)((OnValue >> 8) & 0x1F), (uint8_t)(OffValue & 0xFF), (uint8_t)((OffValue >> 8) & 0x1F)};
		return WriteReg(this->dev_handle, LEDn_ON_L((uint8_t)Output), data, 4);
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
		if (this->dev_handle == nullptr)
		{
			//ESP_LOGE(TAG, "i2c device is null");
			return ErrorCode::NOT_YET_INITIALIZED;
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

	ErrorCode M::SetAllOutputs(i2c_master_bus_handle_t master_bus_handle, Device device, uint16_t dutyCycle)
	{
		return SetOutputs(master_bus_handle, device, 0xFFFF, dutyCycle);
	}

	ErrorCode M::SetAll(uint16_t OnValue, uint16_t OffValue)
	{
		if (this->dev_handle == nullptr)
		{
			return ErrorCode::NOT_YET_INITIALIZED;
		}
		uint8_t data[4] = {
			(uint8_t)(OnValue & 0xFF),
			(uint8_t)((OnValue >> 8) & 0x1F),
			(uint8_t)(OffValue & 0xFF),
			(uint8_t)((OffValue >> 8) & 0x1F),
		};
		return WriteReg(this->dev_handle, ALL_LED_ON_L, data, 4);
	}

	M::M(Device device, InvOutputs inv, OutputDriver outdrv, OutputNotEn outne, Frequency freq) : master_bus_handle(nullptr), dev_handle(nullptr), device(device), inv(inv), outdrv(outdrv), outne(outne), freq(freq)
	{
		for (int i = 0; i < 16; i++)
		{
			val[i] = 0;
			written[i] = 0;
		}
	}
}