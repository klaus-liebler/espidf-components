#pragma once

#include <inttypes.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <driver/ledc.h>
#include <driver/gpio.h>

namespace BUZZER
{
   
    struct Note
    {
        uint16_t freq;
        uint16_t durationMs;
    };
    
    enum class RINGTONE_SONG;

    class M
    {
    private:
        const Note *nextNote{nullptr};
        int64_t nextNoteTimeMs{0};
        ledc_channel_t channel;
        ledc_timer_t timer_num;
        int64_t GetMillis64();
        void StartBuzzer(double freqHz);
        void EndBuzzer();
        bool pauseBetweenNotesFinished{false};
    public:
        M(ledc_channel_t channel=LEDC_CHANNEL_2, ledc_timer_t timer_num=LEDC_TIMER_2):channel(channel), timer_num(timer_num){}
        void Begin(gpio_num_t pin);
        void PlaySong(RINGTONE_SONG songNumber);
        void PlaySong(uint32_t songNumber);
        void PlayNotes(const Note *note);
        void Loop();
    };
}
