#pragma once
#include <common.hh>
#include "crgb.hh"
#define TAG "LED"

// Ein einziger, farbiger AnimationPattern-Typ fuer alle LED-Ansteuerungen (SingleLed und RgbStrip,
// s. animatable_led.hh) -- ersetzt das vormals getrennte, boolean-basierte led::AnimationPattern
// (ledanimator-Komponente) und das bereits farbige RGBLED::AnimationPattern (rgbled-Komponente).
namespace led
{
    class AnimationPattern
    {
    public:
        virtual void Reset(tms_t now) = 0;
        virtual CRGB Animate(tms_t now) = 0;
        virtual ~AnimationPattern() = default;
    };

    class BlinkPattern : public AnimationPattern
    {
    private:
        tms_t lastChange{0};
        bool state{false};
        CRGB color0;
        tms_t time0;
        CRGB color1;
        tms_t time1;

    public:
        void Reset(tms_t now) override
        {
            lastChange = now;
            state = true;
        }
        CRGB Animate(tms_t now) override
        {
            if (state)
            {
                if (lastChange + time1 <= now)
                {
                    state = false;
                    lastChange = now;
                }
            }
            else
            {
                if (lastChange + time0 <= now)
                {
                    state = true;
                    lastChange = now;
                }
            }
            return state ? color1 : color0;
        }
        BlinkPattern(CRGB color0, tms_t time0, CRGB color1, tms_t time1) : color0(color0), time0(time0), color1(color1), time1(time1) {}
    };

    class MultipleFlashesPattern : public AnimationPattern
    {
    private:
        CRGB colorFlash;
        size_t flashCount;
        CRGB colorIdle;
        size_t idleDuration;
        tms_t lastChange{0};
        size_t flashDuration{150};

    public:
        void Reset(tms_t now) override
        {
            lastChange = now;
        }
        CRGB Animate(tms_t now) override
        {
            if (flashCount == 0)
            {
                return colorIdle;
            }

            const tms_t pulseWindow = static_cast<tms_t>(flashCount * 2 * flashDuration);
            const tms_t cycleDuration = pulseWindow + static_cast<tms_t>(idleDuration);
            if (cycleDuration == 0)
            {
                return colorIdle;
            }

            tms_t elapsed = now - lastChange;
            if (elapsed >= cycleDuration)
            {
                elapsed %= cycleDuration;
                lastChange = now - elapsed;
            }

            if (elapsed >= pulseWindow)
            {
                return colorIdle;
            }

            const tms_t slot = elapsed / static_cast<tms_t>(flashDuration);
            return (slot % 2 == 0) ? colorFlash : colorIdle;
        }
        MultipleFlashesPattern(CRGB colorFlash, size_t flashCount, CRGB colorIdle = CRGB::Black, size_t idleDuration = 1000) : colorFlash(colorFlash), flashCount(flashCount), colorIdle(colorIdle), idleDuration(idleDuration) {}
    };

    // Konstantes Pattern (immer dieselbe Farbe) -- Nachfolger der frueheren CONST_OFF/CONST_ON-
    // Singletons; CONST_ON entfaellt, da "an" fuer eine beliebige RGB-Farbe nicht sinnvoll
    // verallgemeinerbar ist (im Gegensatz zu "aus" == CRGB::Black, das eindeutig ist).
    class ConstPattern : public AnimationPattern
    {
    private:
        CRGB color;

    public:
        explicit ConstPattern(CRGB color) : color(color) {}
        void Reset(tms_t now) override {}
        CRGB Animate(tms_t now) override { return color; }
    };

    inline ConstPattern CONST_OFF_INSTANCE{CRGB::Black};
    inline AnimationPattern &CONST_OFF = CONST_OFF_INSTANCE;
}
#undef TAG
