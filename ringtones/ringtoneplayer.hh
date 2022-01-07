#pragma once

#include <inttypes.h>
#include <driver/ledc.h>

struct Note
{
    uint16_t freq;
    uint16_t durationMs;
};

#include "songs.hh"
#define TAG "RINGTONE"
class Ringtoneplayer
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
        buzzer_timer.speed_mode = LEDC_HIGH_SPEED_MODE;   // timer mode
        buzzer_timer.timer_num = LEDC_TIMER_2;            // timer index
        buzzer_timer.clk_cfg = LEDC_AUTO_CLK;
        ledc_timer_config(&buzzer_timer);
        ledc_set_duty(LEDC_HIGH_SPEED_MODE, this->channel, 512);
        ledc_update_duty(LEDC_HIGH_SPEED_MODE, this->channel);
    }

    void EndBuzzer()
    {
        ledc_set_duty(LEDC_HIGH_SPEED_MODE, this->channel, 0);
        ledc_update_duty(LEDC_HIGH_SPEED_MODE, this->channel);
    }

public:
    uint32_t songNumber;
    uint64_t song_nextNoteTimeMs;
    size_t song_nextNoteIndex;
    ledc_channel_t channel;
    /**
     * timer_num = LEDC_TIMER_2;
     * channel = LEDC_CHANNEL_2;
    */
    void Setup(ledc_timer_t timer_num, ledc_channel_t channel, gpio_num_t pin)
    {
        this->channel=channel;
        //Buzzer
        ledc_timer_config_t buzzer_timer;
        buzzer_timer.duty_resolution = LEDC_TIMER_10_BIT; // resolution of PWM duty
        buzzer_timer.freq_hz = 440;                       // frequency of PWM signal
        buzzer_timer.speed_mode = LEDC_HIGH_SPEED_MODE;   // timer mode
        buzzer_timer.timer_num = timer_num;            // timer index
        buzzer_timer.clk_cfg = LEDC_AUTO_CLK;
        ESP_ERROR_CHECK(ledc_timer_config(&buzzer_timer));

        ledc_channel_config_t buzzer_channel;
        buzzer_channel.channel = channel;
        buzzer_channel.duty = 0;
        buzzer_channel.gpio_num = pin;
        buzzer_channel.speed_mode = LEDC_HIGH_SPEED_MODE;
        buzzer_channel.hpoint = 0;
        buzzer_channel.timer_sel = timer_num;
        ESP_ERROR_CHECK(ledc_channel_config(&buzzer_channel));
    }

    void PlaySong(RINGTONE_SONG songNumber){
        PlaySong((uint32_t)songNumber);
    }

    void PlaySong(uint32_t songNumber)
    {
        if (songNumber >= sizeof(SONGS) / sizeof(Note))
            songNumber = 0;
        this->song_nextNoteIndex = 0;
        this->song_nextNoteTimeMs = GetMillis64();
        this->songNumber = songNumber;
        ESP_LOGI(TAG, "Set Song to %d", songNumber);
    }

    void Loop()
    {
        if (songNumber == 0)
            return;

        if (GetMillis64() < song_nextNoteTimeMs)
            return;

        const Note note = SONGS[songNumber][song_nextNoteIndex];
        if (note.freq == 0)
        {
            ESP_LOGD(TAG, "Mute Note and wait %d", note.durationMs);
            EndBuzzer();
            if (note.durationMs == 0)
            {
                songNumber = 0;
                song_nextNoteTimeMs = UINT64_MAX;
                song_nextNoteIndex = 0;
            }
            else
            {
                song_nextNoteTimeMs += note.durationMs;
                song_nextNoteIndex++;
            }
        }
        else
        {
            ESP_LOGD(TAG, "Set Note to Frequency %d and wait %d", note.freq, note.durationMs);
            StartBuzzer(note.freq);
            song_nextNoteTimeMs += note.durationMs;
            song_nextNoteIndex++;
        }
    }
};
#undef TAG