#pragma once
#include <stdio.h>
#include <string.h>
#include <common.hh>
#include <esp_log.h>
#include <driver/gpio.h>
#include <esp_timer.h>


namespace led
{
    class AnimationPattern
    {
    public:
        virtual void Reset(tms_t now) = 0;
        virtual bool Animate(tms_t now) = 0;
    };

    class BlinkPattern : public AnimationPattern
    {
    private:
        tms_t nextChange{0};
        bool state{false};
        tms_t timeOn;
        tms_t timeOff;

    public:
        void Reset(tms_t now) override
        {
            nextChange = now+timeOn;
            state = true;
        }
        bool Animate(tms_t now) override
        {
            if(now < nextChange) return state;
            if (state)
            {
                state = false;
                nextChange = now+timeOff;
            }
            else
            {
                state = true;
                nextChange = now+timeOn;
            }
            return state;
        }
        BlinkPattern(tms_t timeOn, tms_t timeOff) : timeOn(timeOn), timeOff(timeOff) {}
    };

    class : public AnimationPattern
    {
        void Reset(tms_t now) {}
        bool Animate(tms_t now) { return false; }
    } CONST_OFF;

    class : public AnimationPattern
    {
        void Reset(tms_t now) {}
        bool Animate(tms_t now) { return true; }
    } CONST_ON;

    class Animator
    {
    private:
    	
        gpio_num_t gpio{GPIO_NUM_NC};
        bool invert{false};
        AnimationPattern* pattern{nullptr};
        tms_t timeToAutoOff=INT64_MAX;//time is absolute!
        AnimationPattern* standbyPattern{nullptr};
    public:
        Animator(gpio_num_t gpio, bool invert=false, AnimationPattern* standbyPattern=&CONST_OFF):gpio(gpio), invert(invert), pattern(standbyPattern), standbyPattern(standbyPattern) {}
        esp_err_t AnimatePixel(AnimationPattern *pattern, tms_t timeToAutoOff=0);
        esp_err_t Refresh();
        esp_err_t Begin(AnimationPattern *pattern=&CONST_OFF, tms_t timeToAutoOff=0);
    };
}
