#include "buzzer.hh"
#define TAG "RINGTONE"
namespace BUZZER
{
#define END_OF_SONG {0, 0}
#include "songs.hh.inc"
#undef END_OF_SONG

    int64_t M::GetMillis64()
    {
        return esp_timer_get_time() / 1000ULL;
    }

    void M::StartBuzzer(double freqHz)
    {
        ledc_set_freq(LEDC_LOW_SPEED_MODE, this->timer_num, freqHz);
        ledc_set_duty(LEDC_LOW_SPEED_MODE, this->channel, 512);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, this->channel);
    }

    void M::EndBuzzer()
    {
        ledc_set_duty(LEDC_LOW_SPEED_MODE, this->channel, 0);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, this->channel);
    }


    void M::Begin(gpio_num_t pin)
    {
        // Buzzer
        ESP_LOGI(TAG, "Configuring Buzzer Timer at pin %i with channel %i", (int)pin, (int)channel);
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

    void M::PlaySong(RINGTONE_SONG songNumber)
    {
        PlaySong((uint32_t)songNumber);
    }

    void M::PlaySong(uint32_t songNumber)
    {
        if (songNumber >= sizeof(RINGTONE_SOUNDS) / sizeof(Note))
            songNumber = 0;
        PlayNotes(&RINGTONE_SOUNDS[songNumber][0]);
    }

    void M::PlayNotes(const Note *note)
    {
        int64_t now = GetMillis64();
        StartBuzzer(note->freq);
        nextNoteTimeMs = now + note->durationMs;
        nextNote = note;
        this->nextNote++;
    }
    
    void M::Loop()
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
        else if (!pauseBetweenNotesFinished)
        {
            EndBuzzer();
            nextNoteTimeMs += 40;
            pauseBetweenNotesFinished = true;
        }
        else
        {
            ESP_LOGD(TAG, "Set Note to Frequency %d and wait %d", nextNote->freq, nextNote->durationMs);
            StartBuzzer(nextNote->freq);
            nextNoteTimeMs += nextNote->durationMs;
            pauseBetweenNotesFinished = false;
            nextNote++;
        }
    }

}