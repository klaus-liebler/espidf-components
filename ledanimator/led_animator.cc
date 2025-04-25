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
        tms_t now = (esp_timer_get_time() / 1000);
        if (pattern == nullptr){
            this->pattern = standbyPattern;
            ESP_LOGI(TAG, "LED @ %d switched to standby pattern", this->gpio);
        }
        if(pattern==this->pattern){
            ESP_LOGD(TAG, "LED @ %d already animating with the same pattern", this->gpio);
            return ESP_OK;
        }
        this->pattern = pattern;
        if (timeToAutoOff == 0)
        {
            this->timeToAutoOff = INT64_MAX;
        }
        else
        {
            this->timeToAutoOff = now + timeToAutoOff;
        }
        this->pattern->Reset(now);
        return ESP_OK;
    }

    esp_err_t Animator::Refresh()
    {
        tms_t now = (esp_timer_get_time() / 1000);
        if (now >= timeToAutoOff)
        {
            this->pattern = standbyPattern;
            this->pattern->Reset(now);
            ESP_LOGI(TAG, "LED @ %d switched to standby pattern", this->gpio);
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