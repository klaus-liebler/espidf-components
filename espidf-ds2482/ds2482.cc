
#include "ds2482.hh"
#include <i2c.hh>
#include <common.hh>

namespace DS2482
{

	// misc constants
	constexpr uint8_t POLL_LIMIT = 200;
	constexpr uint8_t POLL_TIMEOUT = UINT8_MAX;
	constexpr uint8_t TIMEOUT = 100;

	constexpr uint8_t CMD_DRST = 0xF0;
	constexpr uint8_t CMD_WCFG = 0xD2;
	constexpr uint8_t CMD_CHSL = 0xC3;
	constexpr uint8_t CMD_SRP = 0xE1;
	constexpr uint8_t CMD_1WRS = 0xB4;
	constexpr uint8_t CMD_1WWB = 0xA5;
	constexpr uint8_t CMD_1WRB = 0x96;
	constexpr uint8_t CMD_1WSB = 0x87;
	constexpr uint8_t CMD_1WT = 0x78;

	constexpr uint8_t CONFIG_APU = 0x01;
	constexpr uint8_t CONFIG_PPM = 0x02;
	constexpr uint8_t CONFIG_SPU = 0x04;
	constexpr uint8_t CONFIG_1WS = 0x08;

	constexpr uint8_t STATUS_1WB = 0x01;
	constexpr uint8_t STATUS_PPD = 0x02;
	constexpr uint8_t STATUS_SD = 0x04;
	constexpr uint8_t STATUS_LL = 0x08;
	constexpr uint8_t STATUS_RST = 0x10;
	constexpr uint8_t STATUS_SBR = 0x20;
	constexpr uint8_t STATUS_TSB = 0x40;
	constexpr uint8_t STATUS_DIR = 0x80;


	//--------------------------------------------------------------------------
	// DS2428 Detect routine that sets the I2C address and then performs a
	// device reset followed by writing the configuration byte to default values:
	//   1-Wire speed (c1WS) = standard (0)
	//   Strong pull-up (cSPU) = off (0)
	//   Presence pulse masking (cPPM) = off (0)
	//   Active pull-up (cAPU) = on (CONFIG_APU = 0x01)
	//
	// Returns: TRUE if device was detected and written
	//          FALSE device not detected or failure to write configuration byte
	//
	ErrorCode M::Setup()
	{

		if (I2C::IsAvailable(this->i2c_port, (uint8_t)this->device) != ESP_OK)
			return ErrorCode::DEVICE_NOT_RESPONDING;

		// reset the DS2482 ON selected address
		if (!reset())
			return ErrorCode::DEVICE_NOT_RESPONDING;

		// default configuration
		CLEAR_BIT(currCfg, CONFIG_1WS);
		CLEAR_BIT(currCfg, CONFIG_SPU);
		CLEAR_BIT(currCfg, CONFIG_PPM);
		SET_BIT(currCfg, CONFIG_APU);

		// write the default configuration setup
		if (!writeConfig())
			return ErrorCode::DEVICE_NOT_RESPONDING;

		return ErrorCode::OK;
	}

	//--------------------------------------------------------------------------
	// Perform a device reset on the DS2482
	//
	// Returns: TRUE if device was reset
	//          FALSE device not detected or failure to perform reset
	//
	bool M::reset()
	{
		uint8_t status;

		// Device Reset
		//   S AD,0 [A] DRST [A] Sr AD,1 [A] [SS] A\ P
		//  [] indicates from slave
		//  SS status byte to read to verify state

		//ist wie ein MemoryRead von der "CMD_DRST"-Speicherzelle
		I2C::ReadReg(i2c_port, (uint8_t)device, CMD_DRST, &status, 1);
		// check for failure due to incorrect read back of status
		return ((status & 0xF7) == 0x10);
	}

	//--------------------------------------------------------------------------
	// Write the configuration register in the DS2482. The configuration
	// options are provided in the lower nibble of the provided config byte.
	// The uppper nibble in bitwise inverted when written to the DS2482.
	//
	// Returns:  TRUE: config written and response correct
	//           FALSE: response incorrect
	//
	bool M::writeConfig()
	{
		uint8_t read_config;
		// Write configuration (Case A)
		//   S AD,0 [A] WCFG [A] CF [A] Sr AD,1 [A] [CF] A\ P
		//  [] indicates from slave
		//  CF configuration byte to write
		uint16_t pseudoAddress = (CMD_WCFG << 8) | (((~currCfg << 4) | currCfg) & 0xFF);
		if (I2C::ReadReg16(i2c_port, (uint8_t)device, pseudoAddress, &read_config, 1) != ESP_OK)
			return false;
		if (currCfg != read_config) {
			// handle error
			// ...
			reset();
			return false;
		}
		return true;
	}

	//--------------------------------------------------------------------------
	// Select the 1-Wire channel on a DS2482-800.
	//
	// Returns: TRUE if channel selected
	//          FALSE device not detected or failure to perform select
	//
	bool M::channelSelect(uint8_t channel)
	{
		uint8_t ch, ch_read, check;

		switch (channel)
		{
		default:
		case 0:
			ch = 0xF0;
			ch_read = 0xB8;
			break;
		case 1:
			ch = 0xE1;
			ch_read = 0xB1;
			break;
		case 2:
			ch = 0xD2;
			ch_read = 0xAA;
			break;
		case 3:
			ch = 0xC3;
			ch_read = 0xA3;
			break;
		case 4:
			ch = 0xB4;
			ch_read = 0x9C;
			break;
		case 5:
			ch = 0xA5;
			ch_read = 0x95;
			break;
		case 6:
			ch = 0x96;
			ch_read = 0x8E;
			break;
		case 7:
			ch = 0x87;
			ch_read = 0x87;
			break;
		};

		uint16_t pseudoAddress = (CMD_CHSL << 8) | ch;
		I2C::ReadReg16(i2c_port, (uint8_t)device, pseudoAddress, &check, 1);

		// check for failure due to incorrect read back of channel
		return (check == ch_read);
	}

	//---------------------------------------------------------------------------
	//-------- Basic 1-Wire functions
	//---------------------------------------------------------------------------

	//--------------------------------------------------------------------------
	// Reset all of the devices on the 1-Wire Net and return the result.
	//
	// Returns: TRUE(1):  presence pulse(s) detected, device(s) reset
	//          FALSE(0): no presence pulses detected
	//
	bool M::OWReset(void)
	{
		// 1-Wire reset (Case B)
		//   S AD,0 [A] 1WRS [A] Sr AD,1 [A] [Status] A [Status] A\ P
		//                                   \--------/
		//                       Repeat until 1WB bit has changed to 0
		//  [] indicates from slave
		uint8_t cmd = CMD_1WRS;
		I2C::Write(i2c_port, (uint8_t)device, &cmd, 1);

		uint8_t status = pollStatus();
		// check for short condition
		if (status & STATUS_SD)
			short_detected = true;
		else
			short_detected = false;

		// check for presence detect
		if (status & STATUS_PPD)
			return true;
		else
			return false;
	}

	uint8_t M::pollStatus()
	{
		// loop checking 1WB bit for completion of 1-Wire operation
		// abort if poll limit reached
		uint8_t status;
		int poll_count = 0;
		do
		{
			I2C::Read(i2c_port, (uint8_t)device, &status, 1);
		} while ((status & STATUS_1WB) && (poll_count++ < POLL_LIMIT));

		// check for failure due to poll limit reached
		if (poll_count >= POLL_LIMIT)
		{
			// handle error
			// ...
			reset();
			return UINT8_MAX;
		}
		return status;
	}

	//--------------------------------------------------------------------------
	// Send 1 bit of communication to the 1-Wire Net.
	// The parameter 'sendbit' least significant bit is used.
	//
	// 'sendbit' - 1 bit to send (least significant byte)
	//
	void M::OWWriteBit(bool sendbit)
	{
		OWTouchBit(sendbit);
	}

	//--------------------------------------------------------------------------
	// Reads 1 bit of communication from the 1-Wire Net and returns the
	// result
	//
	// Returns:  1 bit read from 1-Wire Net
	//
	bool M::OWReadBit(void)
	{
		return OWTouchBit(true);
	}

	//--------------------------------------------------------------------------
	// Send 1 bit of communication to the 1-Wire Net and return the
	// result 1 bit read from the 1-Wire Net.  The parameter 'sendbit'
	// least significant bit is used and the least significant bit
	// of the result is the return bit.
	//
	// 'sendbit' - the least significant bit is the bit to send
	//
	// Returns: 0:   0 bit read from sendbit
	//          1:   1 bit read from sendbit
	//
	bool M::OWTouchBit(bool sendbit)
	{
		// 1-Wire bit (Case B)
		//   S AD,0 [A] 1WSB [A] BB [A] Sr AD,1 [A] [Status] A [Status] A\ P
		//                                          \--------/
		//                           Repeat until 1WB bit has changed to 0
		//  [] indicates from slave
		//  BB indicates byte containing bit value in msbit

		uint8_t cmd[2] = {CMD_1WSB, sendbit ? (uint8_t)0x80 : (uint8_t)0x00};
		I2C::Write(i2c_port, (uint8_t)device, cmd, 2);

		uint8_t status = pollStatus();

		// return bit state
		if (status & STATUS_SBR)
			return true;
		else
			return false;
	}

	//--------------------------------------------------------------------------
	// Send 8 bits of communication to the 1-Wire Net and verify that the
	// 8 bits read from the 1-Wire Net is the same (write operation).
	// The parameter 'sendbyte' least significant 8 bits are used.
	//
	// 'sendbyte' - 8 bits to send (least significant byte)
	//
	// Returns:  TRUE: bytes written and echo was the same
	//           FALSE: echo was not the same
	//
	void M::OWWriteByte(uint8_t sendbyte)
	{

		// 1-Wire Write Byte (Case B)
		//   S AD,0 [A] 1WWB [A] DD [A] Sr AD,1 [A] [Status] A [Status] A\ P
		//                                          \--------/
		//                             Repeat until 1WB bit has changed to 0
		//  [] indicates from slave
		//  DD data to write

		uint8_t cmd[2] = {CMD_1WWB, sendbyte};
		I2C::Write(i2c_port, (uint8_t)device, cmd, 2);
		pollStatus();
	}

	//--------------------------------------------------------------------------
	// Send 8 bits of read communication to the 1-Wire Net and return the
	// result 8 bits read from the 1-Wire Net.
	//
	// Returns:  8 bits read from 1-Wire Net
	//
	uint8_t M::OWReadByte(void)
	{
		uint8_t data;
		/*
	 1-Wire Read Bytes (Case C)
	   S AD,0 [A] 1WRB [A] Sr AD,1 [A] [Status] A [Status] A\
	                                   \--------/
	                     Repeat until 1WB bit has changed to 0
	   Sr AD,0 [A] SRP [A] E1 [A] Sr AD,1 [A] DD A\ P

	  [] indicates from slave
	  DD data read
*/
		uint8_t cmd = CMD_1WRB;
		I2C::Write(i2c_port, (uint8_t)device, &cmd, 1);
		pollStatus();
		uint16_t pseudoAddress = (CMD_SRP << 8) | 0xE1;
		I2C::ReadReg16(i2c_port, (uint8_t)device, pseudoAddress, &data, 1);
		return data;
	}

	//--------------------------------------------------------------------------
	// Send 8 bits of communication to the 1-Wire Net and return the
	// result 8 bits read from the 1-Wire Net.  The parameter 'sendbyte'
	// least significant 8 bits are used and the least significant 8 bits
	// of the result is the return byte.
	//
	// 'sendbyte' - 8 bits to send (least significant byte)
	//
	// Returns:  8 bits read from sendbyte
	//
	uint8_t M::OWTouchByte(uint8_t sendbyte)
	{
		if (sendbyte == 0xFF)
			return OWReadByte();
		else
		{
			OWWriteByte(sendbyte);
			return sendbyte;
		}
	}

	//--------------------------------------------------------------------------
	// The 'OWBlock' transfers a block of data to and from the
	// 1-Wire Net. The result is returned in the same buffer.
	//
	// 'tran_buf' - pointer to a block of unsigned
	//              chars of length 'tran_len' that will be sent
	//              to the 1-Wire Net
	// 'tran_len' - length in bytes to transfer
	//
	void M::OWBlock(uint8_t *tran_buf, uint32_t tran_len)
	{
		uint32_t i;

		for (i = 0; i < tran_len; i++)
			tran_buf[i] = OWTouchByte(tran_buf[i]);
	}

	//--------------------------------------------------------------------------
	// Find the 'first' devices on the 1-Wire network
	// Return TRUE  : device found, ROM number in ROM_NO buffer
	//        FALSE : no device present
	//
	bool M::OWFirst(bool alarmOnly)
	{
		// reset the search state
		LastDiscrepancy = 0;
		LastDeviceFlag = false;
		LastFamilyDiscrepancy = 0;
		this->alarmOnly = alarmOnly;
		return OWSearch();
	}

	//--------------------------------------------------------------------------
	// Find the 'next' devices on the 1-Wire network
	// Return TRUE  : device found, ROM number in ROM_NO buffer
	//        FALSE : device not found, end of search
	//
	bool M::OWNext()
	{
		// leave the search state alone
		return OWSearch();
	}

	bool M::BeginTransactionForAll(Command cmd)
	{
		if (!OWReset())
		{
			return false;
		}
		OWWriteByte((uint8_t)Command::Skip_ROM);
		OWWriteByte((uint8_t)cmd);
		return true;
	}
	bool M::BeginTransaction(FamilyCode family, const uint8_t *address, Command cmd)
	{
		if (!OWReset())
		{
			return false;
		}
		uint8_t crc8 = 0;
		OWWriteByte((uint8_t)Command::Match_ROM);
		OWWriteByte((uint8_t)family);
		calcCrc8((uint8_t)family, &crc8);
		int i = 0;
		for (i = 0; i < 6; i++)
		{
			OWWriteByte(address[i]);
			calcCrc8(address[i], &crc8);
		}
		OWWriteByte(crc8);
		OWWriteByte((uint8_t)cmd);
		return true;
	}

	void M::fillRomAddressBuffer(FamilyCode family, const uint8_t *address)
	{
		uint8_t crc8 = 0;
		ROM_NO[0] = (uint8_t)family;
		calcCrc8((uint8_t)family, &crc8);
		int i = 0;
		for (i = 0; i < 6; i++)
		{
			ROM_NO[i + 1] = address[i];
			calcCrc8(address[i], &crc8);
		}
		ROM_NO[7] = crc8;
		crc8 = 0;
	}
	bool M::OWVerifyPresence(FamilyCode family, const uint8_t *romAddressWithoutCRC)
	{
		fillRomAddressBuffer(family, romAddressWithoutCRC);
		return OWVerify();
	}

	/*
 * In temp steht die mit 16 multiplizierte Temperatur
 * also: Wenn Du reale Temp willst: teile durch 16
 * RED - VCC, GREEN - DATA, YELLOW - GND
 */
	bool M::OWReadDS18B20Temp(const uint8_t *address, int16_t *temp)
	{
		BeginTransaction(FamilyCode::DS18B20, address, Command::READ_SCRATCHPAD);
		uint8_t crc8 = 0;
		uint8_t scratchpad[8];
		uint8_t i;
		for (i = 0; i < 8; i++)
		{
			scratchpad[i] = OWReadByte();
			calcCrc8(scratchpad[i], &crc8);
		}
		if (crc8 != OWReadByte())
		{
			return false;
		}

		*temp = 0;
		//TEST
		*temp = 0xFF | 0x5E << 8; //-10,125�C --> in temp sollte -162 stehen
		*temp = scratchpad[0] | scratchpad[1] << 8;
		return true;
	}

	bool M::OWWriteDS2413(const FamilyCode family, const uint8_t *address, bool outputA, bool outputB)
	{
		BeginTransaction(family, address, Command::PIO_Access_Write);
		uint8_t read = 0;
		if (outputA)
		{
			SET_BIT(read, 1);
		}
		else
		{
			CLEAR_BIT(read, 1);
		}
		if (outputB)
		{
			SET_BIT(read, 2);
		}
		else
		{
			CLEAR_BIT(read, 2);
		}
		OWWriteByte(read);
		OWWriteByte(~read);
		return true;
	}

	bool M::OWReadScratchpad(const FamilyCode family, const uint8_t *address, uint8_t *buffer, const uint8_t cnt)
	{
		BeginTransaction(family, address, Command::READ_SCRATCHPAD);
		uint8_t crc8 = 0;
		for (uint8_t i = 0; i < cnt; i++)
		{
			buffer[i] = OWReadByte();
			calcCrc8(buffer[i], &crc8);
		}
		if (crc8 != OWReadByte())
		{
			return false;
		}
		return true;
	}

	bool M::OWWriteScratchpad(const FamilyCode family, uint8_t const *const address, uint8_t *buffer, const uint8_t cnt)
	{
		BeginTransaction(family, address, Command::WRITE_SCRATCHPAD);
		uint8_t crc8 = 0;
		for (uint8_t i = 0; i < cnt; i++)
		{
			OWWriteByte(buffer[i]);
			calcCrc8(buffer[i], &crc8);
		}
		OWWriteByte(crc8);

		return true;
	}

	bool M::OWReadDS2413(const FamilyCode family, uint8_t const *const address, uint8_t *setOrClearBit0And1)
	{
		BeginTransaction(family, address, Command::READ_SCRATCHPAD);
		uint8_t read = OWReadByte();
		//die unteren vier maskieren, negieren und nach oben schieben
		//und dann mit den unteren 4 verodern
		uint8_t lower4 = (read & 0x0F);
		uint8_t upper4 = (~(read & 0x0F));
		upper4 = upper4 << 4;

		if (read != (lower4 | upper4))
		{
			return false;
		}
		if (READ_BIT(read, 0x01))
		{
			SET_BIT(*setOrClearBit0And1, 0);
		}
		else
		{
			CLEAR_BIT(*setOrClearBit0And1, 0);
		}
		if (READ_BIT(read, 0x04))
		{
			SET_BIT(*setOrClearBit0And1, 1);
		}
		else
		{
			CLEAR_BIT(*setOrClearBit0And1, 1);
		}
		return true;
	}

	bool M::OWReadDS2413(const FamilyCode family, uint8_t const *const address, uint8_t bitPosToSetOrClear, uint32_t *inputState)
	{
		BeginTransaction(family, address, Command::READ_SCRATCHPAD);
		uint8_t read = OWReadByte();
		//die unteren vier maskieren, negieren und nach oben schieben
		//und dann mit den unteren 4 verodern
		uint8_t lower4 = (read & 0x0F);
		uint8_t upper4 = (~(read & 0x0F));
		upper4 = upper4 << 4;

		if (read != (lower4 | upper4))
		{
			return false;
		}
		if (READ_BIT(read, 0x01))
		{
			SET_BIT(*inputState, bitPosToSetOrClear);
		}
		else
		{
			CLEAR_BIT(*inputState, bitPosToSetOrClear);
		}
		if (READ_BIT(read, 0x04))
		{
			SET_BIT(*inputState, bitPosToSetOrClear + 1);
		}
		else
		{
			CLEAR_BIT(*inputState, bitPosToSetOrClear + 1);
		}
		return true;
	}

	//--------------------------------------------------------------------------
	// Verify the device with the ROM number in ROM_NO buffer is present.
	// Return TRUE  : device verified present
	//        FALSE : device not present
	//
	bool M::OWVerify()
	{
		uint8_t rom_backup[8];
		int i, rslt, ld_backup, ldf_backup, lfd_backup;
		bool alarmOnly_backup;

		// keep a backup copy of the current state
		for (i = 0; i < 8; i++)
			rom_backup[i] = ROM_NO[i];
		ld_backup = LastDiscrepancy;
		ldf_backup = LastDeviceFlag;
		lfd_backup = LastFamilyDiscrepancy;
		alarmOnly_backup = alarmOnly;

		// set search to find the same device
		LastDiscrepancy = 64;
		LastDeviceFlag = false;
		alarmOnly = false;

		if (OWSearch())
		{
			// check if same device found
			rslt = true;
			for (i = 0; i < 8; i++)
			{
				if (rom_backup[i] != ROM_NO[i])
				{
					rslt = false;
					break;
				}
			}
		}
		else
			rslt = false;

		// restore the search state
		for (i = 0; i < 8; i++)
			ROM_NO[i] = rom_backup[i];
		LastDiscrepancy = ld_backup;
		LastDeviceFlag = ldf_backup;
		LastFamilyDiscrepancy = lfd_backup;
		alarmOnly = alarmOnly_backup;

		// return the result of the verify
		return rslt;
	}

	//--------------------------------------------------------------------------
	// Setup the search to find the device type 'family_code' on the next call
	// to OWNext() if it is present.
	//
	void M::OWTargetSetup(FamilyCode family_code)
	{
		int i;

		// set the search state to find SearchFamily type devices
		ROM_NO[0] = (uint8_t)family_code;
		for (i = 1; i < 8; i++)
			ROM_NO[i] = 0;
		LastDiscrepancy = 64;
		LastFamilyDiscrepancy = 0;
		LastDeviceFlag = false;
	}

	//--------------------------------------------------------------------------
	// Setup the search to skip the current device type on the next call
	// to OWNext().
	//
	void M::OWFamilySkipSetup()
	{
		// set the Last discrepancy to last family discrepancy
		LastDiscrepancy = LastFamilyDiscrepancy;

		// clear the last family discrpepancy
		LastFamilyDiscrepancy = 0;

		// check for end of list
		if (LastDiscrepancy == 0)
			LastDeviceFlag = false;
	}

	//--------------------------------------------------------------------------
	// The 'OWSearch' function does a general search.  This function
	// continues from the previous search state. The search state
	// can be reset by using the 'OWFirst' function.
	// This function contains one parameter 'alarm_only'.
	// When 'alarm_only' is TRUE (1) the find alarm command
	// 0xEC is sent instead of the normal search command 0xF0.
	// Using the find alarm command 0xEC will limit the search to only
	// 1-Wire devices that are in an 'alarm' state.
	//
	// Returns:   TRUE (1) : when a 1-Wire device was found and its
	//                       Serial Number placed in the global ROM
	//            FALSE (0): when no new device was found.  Either the
	//                       last search was the last device or there
	//                       are no devices on the 1-Wire Net.
	//
	bool M::OWSearch()
	{
		uint8_t id_bit_number = 1;
		uint8_t last_zero = 0;
		uint8_t rom_byte_number = 0;
		uint8_t rom_byte_mask = 1;
		bool search_result = false;
		uint8_t crc8 = 0;

		uint8_t id_bit, cmp_id_bit;
		uint8_t search_direction, status;

		// if the last call was not the last one
		if (!LastDeviceFlag)
		{
			// 1-Wire reset
			if (!OWReset())
			{
				// reset the search
				LastDiscrepancy = 0;
				LastDeviceFlag = false;
				LastFamilyDiscrepancy = 0;
				return false;
			}

			// issue the search command
			if (alarmOnly)
			{
				OWWriteByte(0xEC);
			}
			else
			{
				OWWriteByte(0xF0);
			}

			// loop to do the search
			do
			{
				// if this discrepancy if before the Last Discrepancy
				// on a previous next then pick the same as last time
				if (id_bit_number < LastDiscrepancy)
				{
					if ((ROM_NO[rom_byte_number] & rom_byte_mask) > 0)
						search_direction = 1;
					else
						search_direction = 0;
				}
				else
				{
					// if equal to last pick 1, if not then pick 0
					if (id_bit_number == LastDiscrepancy)
						search_direction = 1;
					else
						search_direction = 0;
				}

				// Perform a triple operation on the DS2482 which will perform 2 read bits and 1 write bit
				status = searchTriplet(search_direction);

				// check bit results in status byte
				id_bit = ((status & STATUS_SBR) == STATUS_SBR);
				cmp_id_bit = ((status & STATUS_TSB) == STATUS_TSB);
				search_direction =
					((status & STATUS_DIR) == STATUS_DIR) ? (uint8_t)1 : (uint8_t)0;

				// check for no devices on 1-Wire
				if ((id_bit) && (cmp_id_bit))
					break;
				else
				{
					if ((!id_bit) && (!cmp_id_bit) && (search_direction == 0))
					{
						last_zero = id_bit_number;

						// check for Last discrepancy in family
						if (last_zero < 9)
							LastFamilyDiscrepancy = last_zero;
					}

					// set or clear the bit in the ROM byte rom_byte_number
					// with mask rom_byte_mask
					if (search_direction == 1)
						ROM_NO[rom_byte_number] |= rom_byte_mask;
					else
						ROM_NO[rom_byte_number] &= (uint8_t)~rom_byte_mask;

					// increment the byte counter id_bit_number
					// and shift the mask rom_byte_mask
					id_bit_number++;
					rom_byte_mask <<= 1;

					// if the mask is 0 then go to new SerialNum byte rom_byte_number and reset mask
					if (rom_byte_mask == 0)
					{
						calcCrc8(ROM_NO[rom_byte_number], &crc8); // accumulate the CRC
						rom_byte_number++;
						rom_byte_mask = 1;
					}
				}
			} while (rom_byte_number < 8); // loop until through all ROM bytes 0-7

			// if the search was successful then
			if (!((id_bit_number < 65) || (crc8 != 0)))
			{
				// search successful so set LastDiscrepancy,LastDeviceFlag,search_result
				LastDiscrepancy = last_zero;

				// check for last device
				if (LastDiscrepancy == 0)
					LastDeviceFlag = true;

				search_result = true;
			}
		}

		// if no device found then reset counters so next 'search' will be like a first
		if (!search_result || (ROM_NO[0] == 0))
		{
			LastDiscrepancy = 0;
			LastDeviceFlag = false;
			LastFamilyDiscrepancy = 0;
			search_result = false;
		}

		return search_result;
	}

	//--------------------------------------------------------------------------
	// Use the DS2482 help command '1-Wire triplet' to perform one bit of a 1-Wire
	// search. This command does two read bits and one write bit. The write bit
	// is either the default direction (all device have same bit) or in case of
	// a discrepancy, the 'search_direction' parameter is used.
	//
	// Returns � The DS2482 status byte result from the triplet command
	//
	uint8_t M::searchTriplet(uint8_t search_direction)
	{
		// 1-Wire Triplet (Case B)
		//   S AD,0 [A] 1WT [A] SS [A] Sr AD,1 [A] [Status] A [Status] A\ P
		//                                         \--------/
		//                           Repeat until 1WB bit has changed to 0
		//  [] indicates from slave
		//  SS indicates byte containing search direction bit value in msbit
		uint8_t cmd[2] = {CMD_1WT, search_direction ? (uint8_t)0x80 : (uint8_t)0x00};
		I2C::Write(i2c_port, (uint8_t)device, cmd, 2);

		return pollStatus();
	}

	//--------------------------------------------------------------------------
	// Calculate the CRC8 of the byte value provided with the current
	// global 'crc8' value.
	// Returns current global crc8 value
	//
	void M::calcCrc8(uint8_t data, uint8_t *crc)
	{
		int i;

		// See Application Note 27
		*crc = *crc ^ data;
		for (i = 0; i < 8; ++i)
		{
			if (*crc & 1)
				*crc = (*crc >> 1) ^ 0x8c;
			else
				*crc = (*crc >> 1);
		}
		return;
	}

	//---------------------------------------------------------------------------
	//-------- Extended 1-Wire functions
	//---------------------------------------------------------------------------

	//--------------------------------------------------------------------------
	// Set the 1-Wire Net communication speed.
	//
	// 'new_speed' - new speed defined as
	//                MODE_STANDARD   0x00
	//                MODE_OVERDRIVE  0x01
	//
	// Returns:  current 1-Wire Net speed
	//
	Speed M::OWSpeed(Speed new_speed)
	{
		// set the speed
		if (new_speed == Speed::OVERDRIVE)
			SET_BIT(currCfg, CONFIG_1WS);
		else
			CLEAR_BIT(currCfg, CONFIG_1WS);

		// write the new config
		writeConfig();

		return new_speed;
	}

	//--------------------------------------------------------------------------
	// Set the 1-Wire Net line level pull-up to normal. The DS2482 does only
	// allows enabling strong pull-up on a bit or byte event. Consequently this
	// function only allows the MODE_STANDARD argument. To enable strong pull-up
	// use OWWriteBytePower or OWReadBitPower.
	//
	// 'new_level' - new level defined as
	//                MODE_STANDARD     0x00
	//
	// Returns:  current 1-Wire Net level
	//
	Pullup M::OWTrySetPullup(Pullup new_level)
	{
		// function only will turn back to non-strong pull-up
		if (new_level != Pullup::STANDARD)
			return Pullup::STRONG;

		// clear the strong pull-up bit in the global config state
		CLEAR_BIT(currCfg, CONFIG_SPU);

		// write the new config
		writeConfig();

		return Pullup::STANDARD;
	}

	//--------------------------------------------------------------------------
	// Send 8 bits of communication to the 1-Wire Net and verify that the
	// 8 bits read from the 1-Wire Net is the same (write operation).
	// The parameter 'sendbyte' least significant 8 bits are used.  After the
	// 8 bits are sent change the level of the 1-Wire net.
	//
	// 'sendbyte' - 8 bits to send (least significant bit)
	//
	// Returns:  TRUE: bytes written and echo was the same, strong pullup now on
	//           FALSE: echo was not the same
	//
	bool M::OWWriteBytePower(uint8_t sendbyte)
	{
		// set strong pull-up enable
		SET_BIT(currCfg, CONFIG_SPU);

		// write the new config
		if (!writeConfig())
			return false;

		// perform write byte
		OWWriteByte(sendbyte);

		return true;
	}

	//--------------------------------------------------------------------------
	// Send 1 bit of communication to the 1-Wire Net and verify that the
	// response matches the 'applyPowerResponse' bit and apply power delivery
	// to the 1-Wire net.  Note that some implementations may apply the power
	// first and then turn it off if the response is incorrect.
	//
	// 'applyPowerResponse' - 1 bit response to check, if correct then start
	//                        power delivery
	//
	// Returns:  TRUE: bit written and response correct, strong pullup now on
	//           FALSE: response incorrect
	//
	bool M::OWReadBitPower(bool applyPowerResponse)
	{
		unsigned char rdbit;

		// set strong pull-up enable
		SET_BIT(currCfg, CONFIG_SPU);

		// write the new config
		if (!writeConfig())
			return false;

		// perform read bit
		rdbit = OWReadBit();

		// check if response was correct, if not then turn off strong pull-up
		if (rdbit != applyPowerResponse)
		{
			OWTrySetPullup(Pullup::STANDARD);
			return false;
		}

		return true;
	}
}
