#include "hoymiles_manager.hh"
#include "hoymiles_protocol.hh"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <esp_timer.h>
#include <esp_log.h>
#include <ctime>
#include <cstring>
#define TAG "HOYMILES"

namespace hoymiles
{
    constexpr uint8_t Manager::CHANNELS[Manager::CHANNEL_COUNT];

    Manager::Manager(Nrf24Receiver* radio, uint64_t dtuSerial, uint64_t inverterSerial, uint32_t pollIntervalMs)
        : radio(radio), dtuSerial(dtuSerial), inverterSerial(inverterSerial), pollIntervalMs(pollIntervalMs)
    {
    }

    void Manager::Begin(iHoymilesListener* listener)
    {
        this->listener = listener;
        xTaskCreate([](void* p) { ((Manager*)p)->Task(); }, "hoymiles", 4096, this, 10, nullptr);
    }

    // Sendet 'buf' an den Wechselrichter und schaltet den Chip anschliessend wieder in Empfangsmodus
    // auf der eigenen DTU-Adresse zurueck (OpenDTU HoymilesRadio_NRF::sendEsbPacket-Ablauf).
    bool Manager::SendRequestFrame(const uint8_t* buf, uint8_t len)
    {
        radio->SetChannel(CHANNELS[txChannelIdx]);
        txChannelIdx = (uint8_t)((txChannelIdx + 1) % CHANNEL_COUNT);

        radio->OpenWritingPipe(inverterAddr);
        radio->SetRetries(15, 3); // nur fuer diesen einen Sendevorgang: Hardware-Auto-Retry aktiv
        ErrorCode res = radio->Transmit(buf, len);
        radio->SetRetries(0, 0);

        radio->OpenReadingPipe(dtuAddr);
        radio->SetChannel(CHANNELS[rxChannelIdx]);
        radio->PowerUpRx();

        return res == ErrorCode::OK;
    }

    // Sendet die Echtzeitdaten-Anfrage und sammelt bis zu 500ms lang Antwort-Fragmente ein.
    // Vereinfachung ggue. OpenDTU: bei fehlendem/kaputtem Fragment wird die GESAMTE Anfrage neu
    // gesendet (statt gezielt nur das fehlende Fragment nachzufordern) -- protokollkonform (der
    // Wechselrichter beantwortet jede gueltige Anfrage neu von vorn), nur etwas mehr Funk-Traffic,
    // was fuer einen einzelnen privat genutzten Wechselrichter irrelevant ist.
    bool Manager::PollOnce(HoymilesLiveData& outData)
    {
        static uint8_t assembled[13 * 22]; // grosszuegig bemessen (max. 13 Fragmente a ~22 Nutzbyte)

        for (int attempt = 0; attempt < 4; attempt++)
        {
            uint8_t reqBuf[27];
            uint8_t reqLen = BuildRealTimeRunDataRequest(inverterSerial, dtuSerial, (uint32_t)time(nullptr), reqBuf);
            if (!SendRequestFrame(reqBuf, reqLen)) continue;

            size_t assembledLen = 0;
            uint8_t expectedFragmentId = 1;
            bool lastFragmentSeen = false;

            int64_t startUs = esp_timer_get_time();
            int64_t lastHopUs = startUs;
            while (!lastFragmentSeen && (esp_timer_get_time() - startUs) < 500000 /* 500ms Timeout */)
            {
                int64_t nowUs = esp_timer_get_time();
                if (nowUs - lastHopUs >= 4000) // alle ~4ms RX-Kanal wechseln, wie OpenDTU
                {
                    rxChannelIdx = (uint8_t)((rxChannelIdx + 1) % CHANNEL_COUNT);
                    radio->SetChannel(CHANNELS[rxChannelIdx]);
                    lastHopUs = nowUs;
                }

                if (!radio->IsDataReady())
                {
                    vTaskDelay(pdMS_TO_TICKS(1));
                    continue;
                }

                uint8_t len = radio->GetDynamicPayloadLength();
                if (len == 0 || len > 32)
                {
                    radio->FlushRx();
                    continue;
                }
                uint8_t raw[33] __attribute__((aligned(4)));
                radio->ReadRxPayload(raw, len);
                const uint8_t* frag = raw + 1; // raw[0] ist der beim SPI-Kommando zurueckgelesene Status
                uint8_t fragLen = len;

                if (fragLen < 11) continue;              // zu kurz fuer 10-Byte-Header + CRC8
                if (!CheckFragmentCrc(frag, fragLen)) continue;
                if (frag[0] != 0x95) continue;            // Antwort auf Kommando 0x15 -> mainCmd 0x15|0x80

                uint8_t fragmentIdByte = frag[9];
                uint8_t fragmentId = (uint8_t)(fragmentIdByte & 0x7F);
                bool isLast = (fragmentIdByte & 0x80) != 0;
                if (fragmentId != expectedFragmentId) continue; // Duplikat/ausser der Reihe -> ignorieren

                size_t dataLen = (size_t)fragLen - 10 - 1; // minus 10-Byte-Header, minus CRC8-Byte
                if (assembledLen + dataLen > sizeof(assembled)) break;
                memcpy(assembled + assembledLen, frag + 10, dataLen);
                assembledLen += dataLen;
                expectedFragmentId++;

                if (isLast)
                {
                    lastFragmentSeen = true;
                }
            }

            if (lastFragmentSeen && DecodeHm4chStatistics(assembled, assembledLen, outData))
                return true;
        }
        return false;
    }

    void Manager::Task()
    {
        SerialToRadioId(dtuSerial, dtuAddr);
        SerialToRadioId(inverterSerial, inverterAddr);

        radio->SetDataRateAndPaLevel(Rf24Datarate::RF24_250KBPS, Rf24PowerAmp::RF24_PA_MIN);
        radio->SetAutoAck(0b00000011);        // Pipe0 (TX-Ack) + Pipe1 (RX)
        radio->EnableDynamicPayload(0b00000011);
        radio->SetRetries(0, 0);
        radio->OpenReadingPipe(dtuAddr);
        radio->SetChannel(CHANNELS[rxChannelIdx]);
        radio->PowerUpRx();

        HoymilesLiveData lastGood{};
        while (true)
        {
            HoymilesLiveData frame{};
            bool ok = PollOnce(frame);
            uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);

            if (ok)
            {
                lastGood = frame;
                consecutiveFailures = 0;
                lastSuccessMs = now;
                everSucceeded = true;
            }
            else
            {
                consecutiveFailures++;
            }

            HoymilesLiveData toPublish = lastGood;
            toPublish.Reachable = everSucceeded && (consecutiveFailures < 3);
            toPublish.Producing = toPublish.Reachable && (lastGood.Ac.PacW > 0.0f);
            toPublish.DataAgeMs = everSucceeded ? (now - lastSuccessMs) : 0xFFFFFFFFu;

            ESP_LOGI(TAG, "Poll %s: reachable=%d producing=%d Pac=%.1fW Pdc=%.1fW",
                     ok ? "OK" : "FAILED", toPublish.Reachable, toPublish.Producing, lastGood.Ac.PacW, lastGood.Totals.PdcW);

            if (listener) listener->OnLiveDataUpdated(toPublish);

            vTaskDelay(pdMS_TO_TICKS(pollIntervalMs));
        }
    }
}
#undef TAG
