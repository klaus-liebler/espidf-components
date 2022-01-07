#include <nrf24.hh>
#include "nrf24_registers.h"
#include <esp_log.h>
#define TAG "Nrf24RECV"

constexpr char rf24_datarates[][8] = {"1MBPS", "2MBPS", "250KBPS"};
constexpr char rf24_crclength[][10] = {"Disabled", "8 bits", "16 bits"};
constexpr char rf24_pa_dbm[][8] = {"PA_MIN", "PA_LOW", "PA_HIGH", "PA_MAX"};
	void Nrf24Receiver::configRegister(uint8_t reg, uint8_t value)
	{
		buf16[0] = (W_REGISTER | (REGISTER_MASK & reg));
		buf16[1] = value;
		spiTransaction(buf16, 2);
	}

	void Nrf24Receiver::singleByteCommand(uint8_t cmd)
	{
		buf16[0] = cmd;
		spiTransaction(buf16, 1);
	}

	void Nrf24Receiver::writeRegistersStartingWith1inBuf(uint8_t reg, size_t len)
	{
		buf16[0] = (W_REGISTER | (REGISTER_MASK & reg));
		spiTransaction(buf16, 1 + len);
	}

	uint8_t Nrf24Receiver::readRegister(uint8_t reg)
	{
		buf16[0] = (R_REGISTER | (REGISTER_MASK & reg));
		spiTransaction(buf16, 2);
		return buf16[1];
	}

	void Nrf24Receiver::readRegisters(uint8_t reg, uint8_t len)
	{
		buf16[0] = (R_REGISTER | (REGISTER_MASK & reg));
		spiTransaction(buf16, 1 + len);
	}

	void Nrf24Receiver::ceHi()
	{
		if(cePin == GPIO_NUM_NC) return;
		gpio_set_level(cePin, 1);
	}

	void Nrf24Receiver::ceLow()
	{
		if(cePin == GPIO_NUM_NC) return;
		gpio_set_level(cePin, 0);
	}


	void Nrf24Receiver::Setup(spi_host_device_t hostDevice, int dmaChannel, gpio_num_t ce_pin, gpio_num_t csn_pin, gpio_num_t miso_pin, gpio_num_t mosi_pin, gpio_num_t sclk_pin)
	{
		this->cePin = ce_pin;
		gpio_pad_select_gpio(ce_pin);
		if(cePin != GPIO_NUM_NC){
			gpio_set_direction(ce_pin, GPIO_MODE_OUTPUT);
		}
		ceLow();
		spi_bus_config_t spi_bus_config{};
		ESP_LOGI(TAG, "Set GPIO pins in config");
		spi_bus_config.sclk_io_num = sclk_pin;
		spi_bus_config.mosi_io_num = mosi_pin;
		spi_bus_config.miso_io_num = miso_pin;
		spi_bus_config.quadwp_io_num = GPIO_NUM_NC;
		spi_bus_config.quadhd_io_num = GPIO_NUM_NC;

		ESP_ERROR_CHECK(spi_bus_initialize(hostDevice, &spi_bus_config, dmaChannel));
		ESP_LOGI(TAG, "Set GPIO pins in config FINISHED");
		spi_device_interface_config_t devcfg{};
		devcfg.clock_speed_hz = SPI_MASTER_FREQ_8M;
		devcfg.queue_size = 1;
		devcfg.mode = 0;
		devcfg.flags = 0;
		devcfg.spics_io_num = csn_pin;

		spi_device_handle_t handle;
		ESP_ERROR_CHECK(spi_bus_add_device(hostDevice, &devcfg, &handle));

		
		_SPIHandle = handle;
		payloadLen = 32;
	}

	void Nrf24Receiver::spiTransaction(uint8_t *buf, size_t len)
	{
		assert(((int)buf & 0b11) == 0);
		spi_transaction_t SPITransaction{};
		SPITransaction.length = len * 8;
		SPITransaction.tx_buffer = buf;
		SPITransaction.rx_buffer = buf;
		spi_device_transmit(_SPIHandle, &SPITransaction);
	}

	void Nrf24Receiver::Config(uint8_t channel, uint8_t payloadLen, const uint8_t *const readAddr, uint8_t readAddrLen, uint8_t en_aa, Rf24Datarate speed, Rf24PowerAmp txPower)

	// Sets the important registers in the MiRF module and powers the module
	// in receiving mode
	// NB: channel and payload must be set now.
	{
		this->payloadLen = payloadLen;

		ceLow();

		memcpy(buf16 + 1, readAddr, readAddrLen);
		writeRegistersStartingWith1inBuf(RX_ADDR_P1, readAddrLen);
		uint8_t val = 0b10;
		configRegister(EN_RXADDR, val);
		ceHi();

		if ((int)speed > 1)
		{
			configRegister(RF_SETUP, (1 << RF_DR_LOW));
		}
		else
		{
			configRegister(RF_SETUP, (((int)speed) << RF_DR_HIGH));
		}

		configRegister(EN_AA, en_aa);
		configRegister(RF_SETUP, ((int)txPower) << RF_PWR);
		configRegister(RF_CH, channel);		// Set RF channel
		configRegister(RX_PW_P0, payloadLen); // Set length of incoming payload
		configRegister(RX_PW_P1, payloadLen);
		PowerUpRx(); // Start receiver
		FlushRx();
	}

	bool Nrf24Receiver::IsDataReady() // Checks if data is available for reading
	{
		uint8_t status = GetStatus(); // See note in getData() function - just checking RX_DR isn't good enough
		if (status & (1 << RX_DR))
			return 1;			 // We can short circuit on RX_DR, but if it's not set, we still need
		return !IsRxFifoEmpty(); // to check the FIFO for any pending packets
	}

	bool Nrf24Receiver::IsRxFifoEmpty()
	{
		uint8_t fifoStatus = readRegister(FIFO_STATUS);
		return (fifoStatus & (1 << RX_EMPTY));
	}

	/*
First returned byte is status. data buffer must have a length of min PAYLOAD_LEN+1
*/
	void Nrf24Receiver::GetRxData(uint8_t *data) // Reads payload bytes into data array
	{
		data[0] = R_RX_PAYLOAD;
		spiTransaction(data, payloadLen + 1);
		// Pull up chip select
		// NVI: per product spec, p 67, note c:
		//	"The RX_DR IRQ is asserted by a new packet arrival event. The procedure
		//	for handling this interrupt should be: 1) read payload through SPI,
		//	2) clear RX_DR IRQ, 3) read FIFO_STATUS to check if there are more
		//	payloads available in RX FIFO, 4) if there are more data in RX FIFO,
		//	repeat from step 1)."
		// So if we're going to clear RX_DR here, we need to check the RX FIFO
		// in the dataReady() function
		configRegister(STATUS, (1 << RX_DR)); // Reset status register
	}

	uint8_t Nrf24Receiver::GetStatus()
	{
		return readRegister(STATUS);
	}

	void Nrf24Receiver::PowerUpRx()
	{
		PTX = 0;
		ceLow();
		configRegister(CONFIG, mirf_CONFIG | ((1 << PWR_UP) | (1 << PRIM_RX))); //set device as RX mode
		ceHi();
		configRegister(STATUS, (1 << TX_DS) | (1 << MAX_RT)); //Clear seeded interrupt and max tx number interrupt
	}

	void Nrf24Receiver::FlushRx()
	{
		singleByteCommand(FLUSH_RX);
	}

	void Nrf24Receiver::PowerDown()
	{
		ceLow();
		configRegister(CONFIG, mirf_CONFIG);
	}

#define _BV(x) (1 << (x))

	void Nrf24Receiver::PrintDetails()
	{

		printf("================ SPI Configuration ================\n");
		printf("CE Pin	\t = GPIO%d\n", cePin);
		printf("================ NRF Configuration ================\n");

		print_status(GetStatus());
		print_byte_register("CONFIG\t", CONFIG, 1);
		print_byte_register("EN_AA\t", EN_AA, 1);
		print_byte_register("EN_RXADDR", EN_RXADDR, 1);
		print_byte_register("SETUP_ADDRW", SETUP_AW, 1);
		print_byte_register("RF_CH\t", RF_CH, 1);
		print_byte_register("RF_SETUP", RF_SETUP, 1);

		print_address_register("RX_ADDR_P0-1", RX_ADDR_P0, 2);
		print_byte_register("RX_ADDR_P2-5", RX_ADDR_P2, 4);
		print_address_register("TX_ADDR\t", TX_ADDR, 1);
		print_byte_register("RX_PW_P0-6", RX_PW_P0, 6);
		print_byte_register("DYNPD/FEATURE", DYNPD, 2);
		//printf("getDataRate()=%d\n",Nrf24_getDataRate(dev));
		printf("Data Rate\t = %s\n", rf24_datarates[(int)GetDataRate()]);
#if 0
	printf_P(PSTR("Model\t\t = "
	PRIPSTR
	"\r\n"),pgm_read_ptr(&rf24_model_e_str_P[isPVariant()]));
#endif
		//printf("getCRCLength()=%d\n",Nrf24_getCRCLength(dev));
		printf("CRC Length\t = %s\n", rf24_crclength[(int)GetCRCLength()]);
		//printf("getPALevel()=%d\n",Nrf24_getPALevel());
		printf("PA Power\t = %s\n", rf24_pa_dbm[(int)GetPALevel()]);
	}

	void Nrf24Receiver::print_status(uint8_t status)
	{
		printf("STATUS\t\t = 0x%02x RX_DR=%x TX_DS=%x MAX_RT=%x RX_P_NO=%x TX_FULL=%x\r\n", status, (status & _BV(RX_DR)) ? 1 : 0,
			   (status & _BV(TX_DS)) ? 1 : 0, (status & _BV(MAX_RT)) ? 1 : 0, ((status >> RX_P_NO) & 0x07), (status & _BV(TX_FULL)) ? 1 : 0);
	}

	void Nrf24Receiver::print_address_register(const char *name, uint8_t reg, uint8_t qty)
	{
		printf("%s\t =", name);
		while (qty--)
		{
			readRegisters(reg++, 5);
			uint8_t *buffer = buf16 + 1;
			printf(" 0x");
			uint8_t *bufptr = buffer + 5;
			while (--bufptr >= buffer)
			{
				printf("%02x", *bufptr);
			}
		}
		printf("\r\n");
	}

	void Nrf24Receiver::print_byte_register(const char *name, uint8_t reg, uint8_t qty)
	{
		printf("%s\t =", name);
		while (qty--)
		{
			uint8_t buffer = readRegister(reg++);
			printf(" 0x%02x", buffer);
		}
		printf("\r\n");
	}

	Rf24Datarate Nrf24Receiver::GetDataRate()
	{
		Rf24Datarate result;
		uint8_t dr = readRegister(RF_SETUP);
		dr = dr & (_BV(RF_DR_LOW) | _BV(RF_DR_HIGH));

		// switch uses RAM (evil!)
		// Order matters in our case below
		if (dr == _BV(RF_DR_LOW))
		{
			// '10' = 250KBPS
			result = Rf24Datarate::RF24_250KBPS;
		}
		else if (dr == _BV(RF_DR_HIGH))
		{
			// '01' = 2MBPS
			result = Rf24Datarate::RF24_2MBPS;
		}
		else
		{
			// '00' = 1MBPS
			result = Rf24Datarate::RF24_1MBPS;
		}
		return result;
	}

	Rf24CrcLength Nrf24Receiver::GetCRCLength()
	{
		Rf24CrcLength result = Rf24CrcLength::RF24_CRC_DISABLED;

		uint8_t config = readRegister(CONFIG);
		config = config & (_BV(CRCO) | _BV(EN_CRC));
		uint8_t AA = readRegister(EN_AA);

		if (config & _BV(EN_CRC) || AA)
		{
			if (config & _BV(CRCO))
			{
				result = Rf24CrcLength::RF24_CRC_16;
			}
			else
			{
				result = Rf24CrcLength::RF24_CRC_8;
			}
		}

		return result;
	}

	Rf24PowerAmp Nrf24Receiver::GetPALevel()
	{
		uint8_t level = readRegister(RF_SETUP);
		level = (level & (_BV(RF_PWR_LOW) | _BV(RF_PWR_HIGH))) >> 1;
		return (Rf24PowerAmp)(level);
	}

