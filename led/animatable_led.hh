#pragma once
#include <errorcodes.hh>
#include <common.hh>
#include "animation_pattern.hh"

// Gemeinsames Interface fuer alle animierbaren LED-Treiber (SingleLed, RgbStrip -- s. single_led.hh/
// rgb_strip.hh). SingleLed und RgbStrip haben natuerlich eigene, individuelle Zusatzmethoden
// (insb. Begin() mit treiberspezifischen Parametern), aber ueber dieses Interface ist jede LED
// gleich ansprechbar: setzen, animieren, refreshen, loeschen.
namespace led
{
    class IAnimatableLED
    {
    public:
        virtual ErrorCode SetPixel(size_t index, CRGB color, bool refresh = false) = 0;

        // timeToAutoOff (relativ, ms; 0 = nie): nach Ablauf faellt die LED automatisch auf ihr
        // standbyPattern zurueck (s. AutoOffDeadline). Ein wiederholter Aufruf mit demselben
        // pattern (Zeiger-Identitaet) laesst eine bereits laufende Deadline unangetastet -- sonst
        // wuerde ein regelmaessig wiederholter Aufruf mit unveraendertem Pattern (z.B. ein
        // Health-Check-Tick) die Deadline immer wieder verlaengern und Auto-Off nie greifen.
        virtual ErrorCode AnimatePixel(size_t index, AnimationPattern *pattern, tms_t timeToAutoOff = 0) = 0;

        virtual ErrorCode Refresh(uint32_t timeout_ms = 1000, bool forceRefreshEvenIfNotNecessary = false) = 0;

        virtual ErrorCode Clear(uint32_t timeout_ms = 1000) = 0;

        virtual size_t Size() const = 0;

        virtual ~IAnimatableLED() = default;
    };
}
