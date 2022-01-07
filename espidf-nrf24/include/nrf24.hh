#pragma once

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <driver/spi_master.h>
#include <driver/gpio.h>
#include "esp_log.h"
 
/**
 * Power Amplifier level.
 *
 * For use with setPALevel()
 */
enum class Rf24PowerAmp
{
	RF24_PA_MIN = 0,
	RF24_PA_LOW = 1,
	RF24_PA_HIGH = 2,
	RF24_PA_MAX = 3,
	RF24_PA_ERROR
};

/**
 * Data rate.  How fast data moves through the air.
 *
 * For use with setDataRate()
 */
enum class Rf24Datarate
{
	RF24_1MBPS = 0,
	RF24_2MBPS = 1,
	RF24_250KBPS = 2,
};

/**
 * CRC Length.  How big (if any) of a CRC is included.
 *
 * For use with setCRCLength()
 */
enum class Rf24CrcLength
{
	RF24_CRC_DISABLED = 0,
	RF24_CRC_8,
	RF24_CRC_16
};

class Nrf24Receiver
{
private:
	uint8_t PTX;		//In sending mode.
	gpio_num_t cePin;	// CE Pin controls RX / TX
	uint8_t payloadLen; // Payload width in bytes
	spi_device_handle_t _SPIHandle;
	uint8_t buf16[16] __attribute__((aligned(4)));

	void spiTransaction(uint8_t *buf, size_t len);
	void configRegister(uint8_t reg, uint8_t value);
	void singleByteCommand(uint8_t cmd);
	void writeRegistersStartingWith1inBuf(uint8_t reg, size_t len);
	uint8_t readRegister(uint8_t reg);
	void readRegisters(uint8_t reg, uint8_t len);
	void ceHi();
	void ceLow();

    void print_status(uint8_t status);

	void print_address_register(const char *name, uint8_t reg, uint8_t qty);

	void print_byte_register(const char *name, uint8_t reg, uint8_t qty);

public:
	void Setup(spi_host_device_t hostDevice, int dmaChannel, gpio_num_t ce_pin, gpio_num_t csn_pin, gpio_num_t miso_pin, gpio_num_t mosi_pin, gpio_num_t sclk_pin);

	void Config(uint8_t channel, uint8_t payloadLen, const uint8_t *const readAddr, uint8_t readAddrLen, uint8_t en_aa, Rf24Datarate speed, Rf24PowerAmp txPower);

	bool IsDataReady();

	bool IsRxFifoEmpty();

	void GetRxData(uint8_t *data);

	uint8_t GetStatus();

	void PowerUpRx();

	void FlushRx();

	void PowerDown();

	void PrintDetails();

	Rf24Datarate GetDataRate();

	Rf24CrcLength GetCRCLength();

	Rf24PowerAmp GetPALevel();
};

