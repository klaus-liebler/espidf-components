#include <nrf24.hh>
#include "nrf24_registers.h"
#include <esp_log.h>
#define TAG "Nrf24RECV"

constexpr char rf24_datarates[][9] = {"1Mbps","2Mbps", "250kbps", "Reserved"};
constexpr char rf24_crclength[][9] = {"Disabled", "8 bits", "16 bits"};
constexpr char rf24_pa_dbm[][5] = {"MIN", "LOW", "HIGH", "MAX"};
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
		gpio_set_level(cePin, 1);
	}

	void Nrf24Receiver::ceLow()
	{
		gpio_set_level(cePin, 0);
	}


	void Nrf24Receiver::SetupSpi(spi_host_device_t hostDevice, gpio_num_t miso_pin, gpio_num_t mosi_pin, gpio_num_t sclk_pin, gpio_num_t csn_pin)
	{
		spi_bus_config_t spi_bus_config{};
		spi_bus_config.mosi_io_num = mosi_pin;
		spi_bus_config.miso_io_num = miso_pin;
		spi_bus_config.sclk_io_num = sclk_pin;
		spi_bus_config.quadwp_io_num = GPIO_NUM_NC;
		spi_bus_config.quadhd_io_num = GPIO_NUM_NC;
		spi_bus_config.data4_io_num = GPIO_NUM_NC;     ///< GPIO pin for spi data4 signal in octal mode, or -1 if not used.
		spi_bus_config.data5_io_num = GPIO_NUM_NC;     ///< GPIO pin for spi data5 signal in octal mode, or -1 if not used.
		spi_bus_config.data6_io_num = GPIO_NUM_NC;     ///< GPIO pin for spi data6 signal in octal mode, or -1 if not used.
		spi_bus_config.data7_io_num = GPIO_NUM_NC;     ///< GPIO pin for spi data7 signal in octal mode, or -1 if not used.
		spi_bus_config.data_io_default_level=false; ///< Output data IO default level when no transaction.
		spi_bus_config.max_transfer_sz=0;  ///< Maximum transfer size, in bytes. Defaults to 4092 if 0 when DMA enabled, or to `SOC_SPI_MAXIMUM_BUFFER_SIZE` if DMA is disabled.
		spi_bus_config.flags=0;       ///< Abilities of bus to be checked by the driver. Or-ed value of ``SPICOMMON_BUSFLAG_*`` flags.
    	spi_bus_config.isr_cpu_id=ESP_INTR_CPU_AFFINITY_AUTO;    ///< Select cpu core to register SPI ISR.
		spi_bus_config.intr_flags=0;
		ESP_ERROR_CHECK(spi_bus_initialize(hostDevice, &spi_bus_config, SPI_DMA_CH_AUTO));
		spi_device_interface_config_t devcfg{};
		devcfg.clock_speed_hz = SPI_MASTER_FREQ_8M;
		devcfg.queue_size = 1;
		devcfg.mode = 0;
		devcfg.flags = 0;
		devcfg.spics_io_num = csn_pin;
		ESP_ERROR_CHECK(spi_bus_add_device(hostDevice, &devcfg, &spiHandle));
		
	}

	void Nrf24Receiver::spiTransaction(uint8_t *buf, size_t len)
	{
		assert(((int)buf & 0b11) == 0);
		spi_transaction_t SPITransaction{};
		SPITransaction.length = len * 8;
		SPITransaction.tx_buffer = buf;
		SPITransaction.rx_buffer = buf;
		spi_device_transmit(this->spiHandle, &SPITransaction);
	}

	ErrorCode Nrf24Receiver::Config(uint8_t channel, uint8_t payloadLen, const uint8_t *const readAddr, uint8_t readAddrLen, uint8_t en_aa, Rf24Datarate speed, Rf24PowerAmp txPower)
	{
		ceLow();
		gpio_set_direction(cePin, GPIO_MODE_OUTPUT);
		
		this->payloadLen = payloadLen;

		memcpy(buf16 + 1, readAddr, readAddrLen);
		writeRegistersStartingWith1inBuf(REG::RX_ADDR_P1, readAddrLen);
		uint8_t val = 0b10;
		configRegister(REG::EN_RXADDR, val);
		ceHi();

		if ((int)speed > 1)
		{
			configRegister(REG::RF_SETUP, (1 << RF_DR_LOW));
		}
		else
		{
			configRegister(REG::RF_SETUP, (((int)speed) << RF_DR_HIGH));
		}

		configRegister(REG::EN_AA, en_aa);
		configRegister(REG::RF_SETUP, ((int)txPower) << RF_PWR);
		configRegister(REG::RF_CH, channel);		// Set RF channel
		configRegister(REG::RX_PW_P0, payloadLen); // Set length of incoming payload
		configRegister(REG::RX_PW_P1, payloadLen);
		PowerUpRx(); // Start receiver
		FlushRx();
		PrintDetails();
		return ErrorCode::OK;
	}

	bool Nrf24Receiver::IsIrqAsserted()
	{
		ESP_ERROR_CHECK(irqPin!=GPIO_NUM_NC?ESP_OK:ESP_FAIL);
		return !gpio_get_level(irqPin);
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
		uint8_t fifoStatus = readRegister(REG::FIFO_STATUS);
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
		configRegister(REG::STATUS, (1 << RX_DR)); // Reset status register
	}

	uint8_t Nrf24Receiver::GetStatus()
	{
		return readRegister(REG::STATUS);
	}

	void Nrf24Receiver::PowerUpRx()
	{
		PTX = 0;
		ceLow();
		configRegister(REG::CONFIG, defaultConfigRegisterValue | ((1 << PWR_UP) | (1 << PRIM_RX))); //set device as RX mode
		ceHi();
		configRegister(REG::STATUS, (1 << TX_DS) | (1 << MAX_RT)); //Clear seeded interrupt and max tx number interrupt
	}

	void Nrf24Receiver::FlushRx()
	{
		singleByteCommand(FLUSH_RX);
	}

	void Nrf24Receiver::PowerDown()
	{
		ceLow();
		configRegister(REG::CONFIG, defaultConfigRegisterValue);
	}

#define _BV(x) (1 << (x))

	void Nrf24Receiver::PrintDetails()
	{

		printf("================ SPI Configuration ================\n");
		printf("CE Pin	\t = GPIO%d\n", cePin);
		printf("================ NRF Configuration ================\n");

		print_status(GetStatus());
		print_byte_register("CONFIG\t", REG::CONFIG, 1);
		print_byte_register("EN_AA\t", REG::EN_AA, 1);
		print_byte_register("EN_RXADDR", REG::EN_RXADDR, 1);
		print_byte_register("SETUP_ADDRW", REG::SETUP_AW, 1);
		print_byte_register("RF_CH\t", REG::RF_CH, 1);
		print_byte_register("RF_SETUP", REG::RF_SETUP, 1);

		print_address_register("RX_ADDR_P0-1", REG::RX_ADDR_P0, 2);
		print_byte_register("RX_ADDR_P2-5", REG::RX_ADDR_P2, 4);
		print_address_register("TX_ADDR\t", REG::TX_ADDR, 1);
		print_byte_register("RX_PW_P0-6", REG::RX_PW_P0, 6);
		print_byte_register("DYNPD/FEATURE", REG::DYNPD, 2);
		printf("Data Rate\t = %s\n", rf24_datarates[(int)GetDataRate()]);
		//printf("getCRCLength()=%d\n",Nrf24_getCRCLength(dev));
		printf("CRC Length\t = %dbyte\n", GetCRCByteLength());
		//printf("getPALevel()=%d\n",Nrf24_getPALevel());
		printf("PA Power\t = PA_%s\n", rf24_pa_dbm[(int)GetPALevel()]);
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
		uint8_t dr = readRegister(REG::RF_SETUP);
		dr = ((dr & _BV(RF_DR_LOW))>>(RF_DR_LOW-1)) |((dr & _BV(RF_DR_HIGH)) >> _BV(RF_DR_HIGH));
		return (Rf24Datarate)dr;
	}

	uint8_t Nrf24Receiver::GetCRCByteLength()
	{
		uint8_t crcBits = readRegister(REG::CONFIG);
		if(!(crcBits & 0b00001000)) return 0;
		return (crcBits & 0b00000100) ? 2 : 1;
	}

	Rf24PowerAmp Nrf24Receiver::GetPALevel()
	{
		uint8_t level = readRegister(REG::RF_SETUP);
		level = (level & (_BV(RF_PWR_LOW) | _BV(RF_PWR_HIGH))) >> 1;
		return (Rf24PowerAmp)(level);
	}

