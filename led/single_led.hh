#pragma once
#include <common.hh>
#include <errorcodes.hh>
#include <driver/gpio.h>
#include <esp_timer.h>
#include <esp_log.h>
#include "animatable_led.hh"
#include "auto_off.hh"
#define TAG "LED"

// Nachfolger von led::Animator (ledanimator-Komponente). Steuert genau EINE einfache (nicht
// farbfaehige) LED an einem GPIO. Kann nur zwei "Farben" darstellen: die konfigurierte onColor
// (LED an) und Schwarz (LED aus). Faellt ein AnimationPattern eine andere Farbe zurueck als
// onColor, wird das als "aus" interpretiert (s. IAnimatableLED::AnimatePixel-Doku).
namespace led
{
    class SingleLed : public IAnimatableLED
    {
    private:
        gpio_num_t gpio{GPIO_NUM_NC};
        CRGB onColor;
        bool invert{false};
        AnimationPattern *pattern{nullptr};
        AnimationPattern *standbyPattern{nullptr};
        AutoOffDeadline autoOff{};

        static tms_t NowMs() { return esp_timer_get_time() / 1000; }

        void WriteGpio(bool on)
        {
            gpio_set_level(this->gpio, on ^ invert);
        }

    public:
        SingleLed(gpio_num_t gpio, CRGB onColor, bool invert = false, AnimationPattern *standbyPattern = &CONST_OFF)
            : gpio(gpio), onColor(onColor), invert(invert), pattern(standbyPattern), standbyPattern(standbyPattern) {}

        // Setzt die LED statisch (kein Pattern mehr aktiv) -- color==onColor -> an, sonst aus.
        ErrorCode SetPixel(size_t index, CRGB color, bool refresh = false) override
        {
            if (index != 0)
                return ErrorCode::INDEX_OUT_OF_BOUNDS;
            this->pattern = nullptr;
            WriteGpio(color == onColor);
            return ErrorCode::OK;
        }

        ErrorCode AnimatePixel(size_t index, AnimationPattern *pattern, tms_t timeToAutoOff = 0) override
        {
            if (index != 0)
                return ErrorCode::INDEX_OUT_OF_BOUNDS;
            if (pattern == nullptr)
            {
                pattern = standbyPattern;
            }
            tms_t now = NowMs();
            if (pattern == this->pattern)
            {
                ESP_LOGD(TAG, "LED @ %d already animating with the same pattern", this->gpio);
                return ErrorCode::OK;
            }
            this->pattern = pattern;
            autoOff.Arm(now, timeToAutoOff);
            this->pattern->Reset(now);
            return ErrorCode::OK;
        }

        ErrorCode Refresh(uint32_t timeout_ms = 1000, bool forceRefreshEvenIfNotNecessary = false) override
        {
            tms_t now = NowMs();
            if (autoOff.Expired(now) && this->pattern != standbyPattern)
            {
                this->pattern = standbyPattern;
                this->pattern->Reset(now);
                ESP_LOGI(TAG, "LED @ %d switched to standby pattern", this->gpio);
            }
            if (this->pattern != nullptr)
            {
                WriteGpio(this->pattern->Animate(now) == onColor);
            }
            return ErrorCode::OK;
        }

        ErrorCode Clear(uint32_t timeout_ms = 1000) override
        {
            this->pattern = nullptr;
            WriteGpio(false);
            return ErrorCode::OK;
        }

        size_t Size() const override { return 1; }

        ErrorCode Begin(AnimationPattern *pattern = &CONST_OFF, tms_t timeToAutoOff = 0)
        {
            gpio_set_direction(this->gpio, GPIO_MODE_OUTPUT);
            return AnimatePixel(0, pattern, timeToAutoOff);
        }
    };
}
#undef TAG
