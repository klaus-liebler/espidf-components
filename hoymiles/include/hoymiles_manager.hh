#pragma once
#include <cstdint>
#include <nrf24.hh>
#include "hoymiles_types.hh"

namespace hoymiles
{
    // Eigener FreeRTOS-Task, der zyklisch (alle pollIntervalMs) eine Echtzeitdaten-Anfrage an den
    // Wechselrichter sendet, die Fragment-Antwort einsammelt und dekodiert. Konfiguriert 'radio'
    // (muss bereits per SetupSpi() initialisiert sein) selbst vollstaendig fuer das Hoymiles-Protokoll
    // (Kanaele, Adressen, Dynamic Payload, Auto-Ack) -- der Aufrufer muss nur SetupSpi() erledigt haben.
    class Manager
    {
    public:
        Manager(Nrf24Receiver* radio, uint64_t dtuSerial, uint64_t inverterSerial, uint32_t pollIntervalMs);

        // Startet den Hintergrund-Task. listener->OnLiveDataUpdated() wird nach jedem Poll-Zyklus
        // aufgerufen (auch bei Fehlschlag -- dann mit den zuletzt bekannten guten Werten und
        // Reachable=false).
        void Begin(iHoymilesListener* listener);

    private:
        Nrf24Receiver* radio;
        uint64_t dtuSerial;
        uint64_t inverterSerial;
        uint32_t pollIntervalMs;
        iHoymilesListener* listener{nullptr};

        uint8_t dtuAddr[5]{};
        uint8_t inverterAddr[5]{};

        static constexpr uint8_t CHANNEL_COUNT = 5;
        static constexpr uint8_t CHANNELS[CHANNEL_COUNT] = {3, 23, 40, 61, 75};
        uint8_t txChannelIdx{0};
        uint8_t rxChannelIdx{0};

        uint32_t consecutiveFailures{0};
        uint32_t lastSuccessMs{0};
        bool everSucceeded{false};

        void Task();
        bool SendRequestFrame(const uint8_t* buf, uint8_t len);
        bool PollOnce(HoymilesLiveData& outData);
    };
}
