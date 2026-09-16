#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <cstring>
#include <algorithm>
#include <span>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <esp_system.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <driver/gpio.h>
#include <driver/i2s_std.h>
#include "driver/dac_continuous.h"
#include <codec_manager.hh>
#include <common.hh>

#define MINIMP3_ONLY_MP3
#define MINIMP3_NO_SIMD
#define MINIMP3_NONSTANDARD_BUT_LOGICAL
#define MINIMP3_IMPLEMENTATION
#include "minimp3.h"

// https://voicemaker.in/
// Neural TTS, German, Katja, 24000Hz, VoiceSpeed+20%

#define TAG "AUDIO"

namespace AudioPlayer
{
    namespace MP3{
        constexpr size_t SAMPLES_PER_FRAME = 1152;
        constexpr size_t CHANNELS_PER_SAMPLE = 2;
        constexpr size_t FRAMES_IN_BUFFER = 4;

        constexpr size_t FRAME_MAX_SIZE_BYTES = 1024;
        constexpr size_t HEADER_LENGTH_BYTES = 4;
        constexpr uint8_t SYNCWORDH = 0xff;
        constexpr uint8_t SYNCWORDL = 0xf0;
    }

    enum class AudioType
    {
        SILENCE,
        MP3,
        PCM,
        VOLTAGE, //for future use
        SYNTHESIZER, // for future use
    };

    struct AudioOrder{
        AudioType type;
        std::span<const uint8_t> file;
        uint32_t sampleRate;//0 means "auto detect"
        uint8_t volume;//0 means: do not change volume
        bool cancelPrevious; //false means: play current AudioOrder to its end
    };

    constexpr AudioOrder SILENCE_ORDER{.type=AudioType::SILENCE, .file={}, .sampleRate=0, .volume=0, .cancelPrevious=true};


    class Player
    {
    private:
        QueueHandle_t orderQueue{nullptr};
        CodecManager::aCodecManager* codecManager{nullptr};
        mp3dec_t *decoder;
        int32_t frameStart{0};
        int16_t *outBuffer{nullptr};
        AudioOrder currentOrder;
        

        static int FindSyncWordOnAnyPosition(std::span<const uint8_t> file, int offset){
            for (int i = offset; i < (int)file.size() - 1; i++)
            {
                if ((file[i + 0] & MP3::SYNCWORDH) == MP3::SYNCWORDH && (file[i + 1] & MP3::SYNCWORDL) == MP3::SYNCWORDL)
                    return i;
            }
            return -1;
        }




        ErrorCode LoopPCM(){
            codecManager->WriteAudioData(CodecManager::eChannels::TWO, CodecManager::eSampleBits::SIXTEEN, 44100, currentOrder.file.size()/4, (void*)currentOrder.file.data());//TODO AudioFormat!
            currentOrder=SILENCE_ORDER;
            return ErrorCode::OK;
        }

        ErrorCode LoopMP3(){
            if (frameStart >= (int32_t)currentOrder.file.size())
            {
                ESP_LOGI(TAG, "Reached End of MP3 File.");
                currentOrder=SILENCE_ORDER;
                return ErrorCode::OK;
            }
            int samples{0};
            int16_t* buf=this->outBuffer;
            mp3dec_frame_info_t info = {};
            //Bei Samplerate >24000..48000: 1152 Samples/Frame, sonst 576
            //-->Ein Frame dauert maximal 24ms
            //-->Decodiere immer 4 Frames, damit wir knapp 100ms überbrücken können
            for(int i=0;i<MP3::FRAMES_IN_BUFFER;i++){
                int bytesLeft = currentOrder.file.size() - frameStart;
                samples += mp3dec_decode_frame(decoder, currentOrder.file.data() + frameStart, bytesLeft, buf, &info);
                this->frameStart += info.frame_bytes;
                if(samples==0 && info.frame_bytes==0){
                    // Kein Sample UND kein Fortschritt: mp3dec_decode_frame kann an dieser Position keinen
                    // vollstaendigen Frame mehr parsen (z.B. abgeschnittener/unvollstaendiger letzter Frame
                    // am Dateiende). frameStart wuerde sonst nie mehr voranschreiten -> Endlos-Stall, statt
                    // die Wiedergabe zu beenden.
                    ESP_LOGW(TAG, "MP3 decode stuck at offset %ld (no full frame left) -> stopping playback.", frameStart);
                    currentOrder=SILENCE_ORDER;
                    return ErrorCode::OK;
                }
                if(samples==0) break;
                buf=this->outBuffer+(samples*info.channels); //kein "+=", weil ja die samples bereits summiert werden!
            }
            if (samples == 0){return ErrorCode::OK;}
            ESP_LOGD(TAG, "ch=%d, hz=%d, samples=%d", info.channels, info.hz, samples);
            auto before = esp_timer_get_time();
            auto err = codecManager->WriteAudioData((CodecManager::eChannels)info.channels, CodecManager::eSampleBits::SIXTEEN, info.hz,samples, outBuffer);
            auto duration = esp_timer_get_time()-before;
            if(duration<1000){
                ESP_LOGW(TAG, "codecManager->WriteAudioData block shorter than 1000us. This means, buffer was already empty -->broken audio --> check task config!");
            }
            return err;
        }

        ErrorCode InitMP3(){
            frameStart = FindSyncWordOnAnyPosition(currentOrder.file, 0);
            if (frameStart < 0)
            {
                ESP_LOGE(TAG, "No synch word found in file!");
                return ErrorCode::DATA_FORMAT_ERROR;
            }
            mp3dec_init(decoder);
            codecManager->SetPowerState(true);
            if(currentOrder.volume!=0){
                codecManager->SetVolume(currentOrder.volume);
            }
            ESP_LOGI(TAG, "Successfully initialized a new MP3 sound play task. File=%p; FileLen=%zu; FrameStart=%ld;", currentOrder.file.data(), currentOrder.file.size(), frameStart);
            return ErrorCode::OK;
        }

        esp_err_t InitPCM(){
            frameStart=0;
            codecManager->SetPowerState(true);
            if(currentOrder.volume!=0){
                codecManager->SetVolume(currentOrder.volume);
            }
            ESP_LOGI(TAG, "Successfully initialized a new PCM sound play task. File=%p; FileLen=%zu; SampleRate %lu", currentOrder.file.data(), currentOrder.file.size(), currentOrder.sampleRate);
            return ESP_OK;
        }

        esp_err_t InitSilence(){
            if(codecManager) codecManager->SetPowerState(false);
            return ESP_OK;
        }


    public:
        bool IsEmittingSamples()
        {
            return currentOrder.type!=AudioType::SILENCE;
        }

        [[nodiscard]] ErrorCode PlayMP3(std::span<const uint8_t> file, uint8_t volume, bool cancelPrevious)
        {
            if(!orderQueue) return ErrorCode::NOT_YET_INITIALIZED;
            if(file.empty()){
                xQueueOverwrite(orderQueue, &SILENCE_ORDER);
                return ErrorCode::OK;
            }
            AudioOrder ao{.type=AudioType::MP3, .file=file, .sampleRate=0, .volume=volume, .cancelPrevious=cancelPrevious};
            xQueueOverwrite(orderQueue, &ao);
            return ErrorCode::OK;
        }

        [[nodiscard]] ErrorCode PlayPCM(std::span<const uint8_t> file, uint32_t sampleRate, uint8_t volume, bool cancelPrevious)
        {
            if(!orderQueue) return ErrorCode::NOT_YET_INITIALIZED;
            AudioOrder ao{.type=AudioType::PCM, .file=file, .sampleRate=sampleRate, .volume=volume, .cancelPrevious=cancelPrevious};
            xQueueOverwrite(orderQueue, &ao);
            return ErrorCode::OK;
        }

        [[nodiscard]] ErrorCode Stop()
        {
            if(!orderQueue) return ErrorCode::NOT_YET_INITIALIZED;
            xQueueOverwrite(orderQueue, &SILENCE_ORDER);
            return ErrorCode::OK;
        }

 
        ErrorCode Loop(){
            if(!orderQueue) return ErrorCode::NOT_YET_INITIALIZED;
            AudioOrder temp;
            if(currentOrder.type==AudioType::SILENCE || (xQueuePeek(orderQueue, &temp, 0) && temp.cancelPrevious)){
                if(xQueueReceive(orderQueue, &currentOrder, 0)){
                    switch (currentOrder.type){
                        case AudioType::MP3:
                            if(InitMP3()!=ErrorCode::OK){
                                currentOrder=SILENCE_ORDER;
                                InitSilence();
                            }
                            break;
                        case AudioType::PCM:
                            if(InitPCM()!=ESP_OK){
                                currentOrder=SILENCE_ORDER;
                                InitSilence();
                            }
                            break;
                        default:
                            InitSilence();
                            break;
                    }
                }
            }

            switch (currentOrder.type)
            {
            case AudioType::MP3:
                return LoopMP3();
            case AudioType::PCM:
                return LoopPCM();
            default:
                return ErrorCode::OK;
            }
        }

        Player(CodecManager::aCodecManager* codecManager)
        {
            this->outBuffer = new int16_t[MP3::CHANNELS_PER_SAMPLE * MP3::FRAMES_IN_BUFFER * MP3::SAMPLES_PER_FRAME];
            this->decoder = new mp3dec_t();
            this->orderQueue = xQueueCreate(1,sizeof(AudioOrder));
            this->codecManager=codecManager;
            mp3dec_init(decoder);
        }
    };

}

#undef TAG