#include <cstdio>
#include <cstring>
#include <common.hh>
#include <esp_log.h>
#include <driver/gpio.h>
#include <esp_timer.h>
#include "led_animator.hh"

#include <array>
#define TAG "LED"
namespace led
{



    esp_err_t Animator::AnimatePixel(AnimationPattern *pattern, tms_t timeToAutoOff) // time is relative, "0" means: no auto off
    {
        if (pattern == nullptr)
            this->pattern = standbyPattern;
        tms_t now = (esp_timer_get_time() / 1000);
        if (timeToAutoOff == 0)
        {
            this->timeToAutoOff = INT64_MAX;
        }
        else
        {
            this->timeToAutoOff = now + timeToAutoOff;
        }
        pattern->Reset(now);
        this->pattern = pattern;
        return ESP_OK;
    }

    esp_err_t Animator::Refresh()
    {
        tms_t now = (esp_timer_get_time() / 1000);
        if (now >= timeToAutoOff)
        {
            this->pattern = standbyPattern;
        }
        bool on = this->pattern->Animate(now);
        gpio_set_level(this->gpio, on ^ invert);
        return ESP_OK;
    }

    esp_err_t Animator::Begin(AnimationPattern *pattern, tms_t timeToAutoOff)
    {
        this->AnimatePixel(pattern, timeToAutoOff);
        return gpio_set_direction(this->gpio, GPIO_MODE_OUTPUT);
    }
};