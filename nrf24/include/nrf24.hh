#pragma once

#include <string.h>
#include <driver/spi_master.h>
#include <driver/gpio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"


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

	// -----------------------------------------------------------------------
	// Ab hier: rein additive Erweiterungen fuer Voll-Duplex-Protokolle (z.B.
	// Hoymiles/nRF24-Funkprotokoll). Der bestehende Empfangs-Workflow oben
	// (Config()+IsIrqAsserted()/IsDataReady()/GetRxData(), genutzt von
	// SNSCT_NODE_TERRASSE/Milight) bleibt davon unberuehrt.
	// -----------------------------------------------------------------------

	// Schreibt TX_ADDR und RX_ADDR_P0 (Pipe0 muss fuer Auto-Ack-Empfang auf
	// die TX-Adresse gesetzt sein). addr zeigt auf 5 Byte.
	void OpenWritingPipe(const uint8_t *addr);

	// Schreibt RX_ADDR_P1 (5 Byte).
	void OpenReadingPipe(const uint8_t *addr);

	void SetChannel(uint8_t channel);

	// delay in Vielfachen von 250us (0..15), count = Anzahl Hardware-Retries (0..15)
	void SetRetries(uint8_t delay, uint8_t count);

	void SetDataRateAndPaLevel(Rf24Datarate speed, Rf24PowerAmp txPower);

	// pipeMask: Bit0=Pipe0 ... Bit5=Pipe5
	void SetAutoAck(uint8_t pipeMask);

	void EnableDynamicPayload(uint8_t pipeMask);

	// Liefert die Laenge des naechsten Pakets im RX-FIFO (nur bei aktiviertem Dynamic Payload gueltig).
	uint8_t GetDynamicPayloadLength();

	// Wie GetRxData(), aber mit explizit uebergebener Laenge statt des Members payloadLen
	// (fuer Dynamic Payload zwingend -- die tatsaechliche Laenge variiert pro Paket und muss
	// vorher per GetDynamicPayloadLength() ermittelt werden). data muss mind. len+1 Byte gross sein.
	void ReadRxPayload(uint8_t *data, uint8_t len);

	void FlushTx();

	// Sendet synchron ueber die aktuell mit OpenWritingPipe() gesetzte Adresse.
	// Erwartet, dass der Aufrufer zuvor SetRetries()/SetChannel() passend gesetzt hat.
	// Laesst den Chip anschliessend im Standby (weder RX noch TX) -- PowerUpRx() ruft
	// der Aufrufer bei Bedarf selbst wieder auf, um in Empfangsmodus zurueckzukehren.
	ErrorCode Transmit(const uint8_t *data, uint8_t len);

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

