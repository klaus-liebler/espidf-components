#pragma once

#include <inttypes.h>
#include <esp_log.h>
#include "esp_timer.h"
#include <driver/ledc.h>
#include <driver/gpio.h>

namespace BUZZER
{

    struct Note
    {
        uint16_t freq;
        uint16_t durationMs;
    };

#include "songs.hh"
#define TAG "RINGTONE"
    class M
    {
    private:
        int64_t GetMillis64()
        {
            return esp_timer_get_time() / 1000ULL;
        }

        void StartBuzzer(double freqHz)
        {
            ledc_timer_config_t buzzer_timer;
            buzzer_timer.duty_resolution = LEDC_TIMER_10_BIT; // resolution of PWM duty
            buzzer_timer.freq_hz = freqHz;                    // frequency of PWM signal
            buzzer_timer.speed_mode = LEDC_LOW_SPEED_MODE;    // timer mode
            buzzer_timer.timer_num = LEDC_TIMER_2;            // timer index
            buzzer_timer.clk_cfg = LEDC_AUTO_CLK;
            ledc_timer_config(&buzzer_timer);
            ledc_set_duty(LEDC_LOW_SPEED_MODE, this->channel, 512);
            ledc_update_duty(LEDC_LOW_SPEED_MODE, this->channel);
        }

        void EndBuzzer()
        {
            ledc_set_duty(LEDC_LOW_SPEED_MODE, this->channel, 0);
            ledc_update_duty(LEDC_LOW_SPEED_MODE, this->channel);
        }

    public:
        const Note *nextNote{nullptr};
        int64_t nextNoteTimeMs;
        ledc_channel_t channel;

        M(ledc_channel_t channel=LEDC_CHANNEL_2):channel(channel){}

        void Begin(gpio_num_t pin, ledc_timer_t timer_num=LEDC_TIMER_2)
        {
            // Buzzer
            ledc_timer_config_t buzzer_timer = {};
            buzzer_timer.duty_resolution = LEDC_TIMER_10_BIT; // resolution of PWM duty
            buzzer_timer.freq_hz = 440;                       // frequency of PWM signal
            buzzer_timer.speed_mode = LEDC_LOW_SPEED_MODE;    // timer mode
            buzzer_timer.timer_num = timer_num;               // timer index
            buzzer_timer.clk_cfg = LEDC_AUTO_CLK;
            ESP_ERROR_CHECK(ledc_timer_config(&buzzer_timer));

            ledc_channel_config_t buzzer_channel = {};
            buzzer_channel.gpio_num = pin;
            buzzer_channel.speed_mode = LEDC_LOW_SPEED_MODE;
            buzzer_channel.channel = channel;
            buzzer_channel.intr_type = LEDC_INTR_DISABLE;
            buzzer_channel.timer_sel = timer_num;
            buzzer_channel.duty = 0;
            buzzer_channel.hpoint = 0;
            ESP_ERROR_CHECK(ledc_channel_config(&buzzer_channel));
        }

        void PlaySong(RINGTONE_SONG songNumber)
        {
            PlaySong((uint32_t)songNumber);
        }

        void PlaySong(uint32_t songNumber)
        {
            if (songNumber >= sizeof(RINGTONE_SOUNDS) / sizeof(Note))
                songNumber = 0;
            PlayNotes(&RINGTONE_SOUNDS[songNumber][0]);
        }

        void PlayNotes(const Note *note)
        {
            int64_t now = GetMillis64();
            StartBuzzer(note->freq);
            nextNoteTimeMs = now + note->durationMs;
            nextNote = note;
            this->nextNote++;
        }

        void Loop()
        {
            if (nextNote == nullptr)
                return;

            if (GetMillis64() < nextNoteTimeMs)
                return;

            if (nextNote->freq == 0)
            {
                ESP_LOGD(TAG, "Mute Note and wait %d", nextNote->durationMs);
                EndBuzzer();
                if (nextNote->durationMs == 0)
                {
                    nextNote = nullptr;
                    nextNoteTimeMs = INT64_MAX;
                }
                else
                {
                    nextNoteTimeMs += nextNote->durationMs;
                    nextNote++;
                }
            }
            else
            {
                ESP_LOGD(TAG, "Set Note to Frequency %d and wait %d", nextNote->freq, nextNote->durationMs);
                StartBuzzer(nextNote->freq);
                nextNoteTimeMs += nextNote->durationMs;
                nextNote++;
            }
        }
    };
}
#undef TAG