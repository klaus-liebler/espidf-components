#pragma once

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <driver/spi_master.h>
#include <driver/gpio.h>

#include "errorcodes.hh"
 
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
	RESERVED=3
};

class Nrf24Receiver
{
	private:
	uint8_t defaultConfigRegisterValue{0b00001000};
	gpio_num_t cePin;	// CE Pin controls RX / TX
	gpio_num_t irqPin;
	uint8_t PTX;		//In sending mode.
	uint8_t payloadLen=32; // Payload width in bytes
	spi_device_handle_t spiHandle;
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
	Nrf24Receiver(uint8_t defaultConfigRegisterValue, gpio_num_t cePin, gpio_num_t irqPin):defaultConfigRegisterValue(defaultConfigRegisterValue), cePin(cePin), irqPin(irqPin){

	}

	void SetupSpi(spi_host_device_t hostDevice, gpio_num_t miso_pin, gpio_num_t mosi_pin, gpio_num_t sclk_pin, gpio_num_t csn_pin);

	ErrorCode Config(uint8_t channel, uint8_t payloadLen, const uint8_t *const readAddr, uint8_t readAddrLen, uint8_t en_aa, Rf24Datarate speed, Rf24PowerAmp txPower);

	bool IsIrqAsserted();
	bool IsDataReady();

	bool IsRxFifoEmpty();

	void GetRxData(uint8_t *data);

	uint8_t GetStatus();

	void PowerUpRx();

	void FlushRx();

	void PowerDown();

	void PrintDetails();

	Rf24Datarate GetDataRate();

	uint8_t GetCRCByteLength();

	Rf24PowerAmp GetPALevel();
};

