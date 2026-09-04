#pragma once
#include <cstdint>
#include <cstddef>
#include "hoymiles_types.hh"

// Reine (I/O-freie) Funktionen fuer das Hoymiles-nRF24-Funkprotokoll, 1:1 portiert aus dem
// quelloffenen OpenDTU-Projekt (https://github.com/tbnobody/OpenDTU, lib/Hoymiles/src/).
// Byte-Layout unten gilt fuer die 4-Kanal-Modelle HM-1000/1200/1500 (Klasse HM_4CH in OpenDTU).
namespace hoymiles
{
    // OpenDTU crc.cpp: bitweise, kein Tabellen-Lookup, Poly 0x01, Init 0x00, MSB-first.
    uint8_t Crc8(const uint8_t* buf, uint8_t len);

    // Standard CRC-16/MODBUS (Poly 0xA001 reflektiert, Init 0xFFFF, LSB-first).
    uint16_t Crc16Modbus(const uint8_t* buf, uint8_t len, uint16_t start = 0xFFFF);

    // Byte-reversed untere 4 Byte der 6-Byte-Seriennummer, big-endian (OpenDTU
    // CommandAbstract::convertSerialToPacketId) -- fuer die Ziel-/Quelladressfelder IM Frame.
    void SerialToPacketId(uint64_t serial, uint8_t out4[4]);

    // 5-Byte nRF24-Funkadresse {0x01, serial-bytes-big-endian} (OpenDTU
    // HoymilesRadio::convertSerialToRadioId) -- fuer TX_ADDR/RX_ADDR_Px.
    void SerialToRadioId(uint64_t serial, uint8_t outAddr5[5]);

    // Parst eine 12-stellige Hex-Seriennummer (z.B. vom Wechselrichter-Typenschild) in outSerial.
    // Gibt false zurueck, wenn hex nicht exakt 12 gueltige Hex-Ziffern enthaelt (z.B. noch nicht
    // konfigurierter Platzhalter).
    bool TryParseSerialHex(const char* hex, uint64_t& outSerial);

    // Baut die 27-Byte RealTimeRunDataCommand-Anfrage (inkl. CRC16 ueber Byte 10..23 und
    // abschliessendem CRC8) in outBuf (muss >=27 Byte gross sein). Gibt die Framelaenge (27) zurueck.
    uint8_t BuildRealTimeRunDataRequest(uint64_t inverterSerial, uint64_t dtuSerial, uint32_t unixTimeSec, uint8_t outBuf[27]);

    // Prueft das CRC8 am Ende eines rohen RX-Fragments (letztes Byte = CRC8 ueber alle vorherigen).
    bool CheckFragmentCrc(const uint8_t* fragment, uint8_t len);

    // Dekodiert einen reassemblierten Statistik-Payload (Konkatenation der Fragmente NACH Abschneiden
    // der jeweils ersten 10 Header-Bytes je Fragment; mind. 62 Byte) in data. Setzt nur die aus dem
    // Funk-Frame lesbaren/berechenbaren Felder (Dc[], Ac, Totals) -- Reachable/Producing/DataAgeMs
    // sind Sache des Aufrufers (haengen vom Poll-Verlauf ab, nicht vom einzelnen Frame).
    bool DecodeHm4chStatistics(const uint8_t* payload, size_t payloadLen, HoymilesLiveData& data);
}
