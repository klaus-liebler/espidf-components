#pragma once

#include <array>
#include <ctime>
#include <algorithm>
#include <sys/time.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <esp_attr.h>
#include <esp_log.h>
#define TAG "MELO"
namespace messagecodes
{
#define DEF(thecode, thestring) thecode,
#define DEF_(thecode) thecode,
    enum class C
    {
#include "messages.inc"
    };
#undef DEF
#undef DEF_
#define DEF(thecode, thestring)     thestring,
#define DEF_(thecode) #thecode,
    constexpr const char *N[] = {

#include "messages.inc"
    };
#undef DEF
#undef DEF_
}
namespace messagelog
{
    constexpr size_t STORAGE_LENGTH{16};
    struct MessageLogEntry
    {
        uint32_t messageCode;
        uint32_t lastMessageData;
        uint32_t messageCount;
        time_t lastMessageTimestamp;

        MessageLogEntry(uint32_t messageCode, uint32_t lastMessageData, uint32_t messageCount, time_t lastMessageTimestamp) : 
            messageCode(messageCode), 
            lastMessageData(lastMessageData), 
            messageCount(messageCount), 
            lastMessageTimestamp(lastMessageTimestamp)
            {}
            MessageLogEntry() : 
            messageCode(0), 
            lastMessageData(0), 
            messageCount(0), 
            lastMessageTimestamp(0)
            {}

        bool operator < (const MessageLogEntry& str) const
        {
            return (lastMessageTimestamp < str.lastMessageTimestamp);
        }
    };

    __NOINIT_ATTR std::array<MessageLogEntry, STORAGE_LENGTH> BUFFER;
    

    class M{
        private:
        static M* singleton;
        bool hasRealtime{false};
        SemaphoreHandle_t xSemaphore{nullptr};
        M(){
            xSemaphore = xSemaphoreCreateBinary();
            xSemaphoreGive(xSemaphore);
            struct timeval tv_now;
            gettimeofday(&tv_now, nullptr);
            time_t  seconds_epoch= tv_now.tv_sec;
            if(seconds_epoch>1684412222){//epoch time when this code has been written
            ESP_LOGI(TAG, "As seconds_epoch = %llu, we expect having realtime", seconds_epoch);
                hasRealtime=true;
            }
        }
    public:
    static M* GetSingleton(){
        if(!singleton){
            singleton=new M();
        }
        return singleton;
    }
    void NotifyNetworkTimeSet(){
        if(hasRealtime) return;
        this->Log(messagecodes::C::SNTP, esp_timer_get_time()/1000);
    }

    void Init(){
        esp_reset_reason_t reason = esp_reset_reason();
        switch (reason)
        {
        case ESP_RST_POWERON:
            ESP_LOGI(TAG, "Reset Reason is RESET %d. MessageLog Memory is reset", reason);
            BUFFER.fill({0,0,0,0});
            break;
        case ESP_RST_EXT:
        case ESP_RST_SW:
            ESP_LOGI(TAG, "Reset Reason is EXT/SW %d. MessageLogEntry is added", reason);
            this->Log(messagecodes::C::SW, 0);
            break;
        case ESP_RST_PANIC:
            ESP_LOGI(TAG, "Reset Reason is PANIC %d. MessageLogEntry is added", reason);
            this->Log(messagecodes::C::PANIC, 0);
            break;
        case ESP_RST_BROWNOUT:
            ESP_LOGI(TAG, "Reset Reason is BROWNOUT %d. MessageLogEntry is added", reason);
            this->Log(messagecodes::C::BROWNOUT, 0);
            break;
        case ESP_RST_TASK_WDT:
        case ESP_RST_INT_WDT:
            ESP_LOGI(TAG, "Reset Reason is WATCHDOG %d. MessageLogEntry is added", reason);
            this->Log(messagecodes::C::WATCHDOG, 0);
            break;
        default:
            ESP_LOGI(TAG, "Reset Reason is %d. MessageLog Memory is reset", reason);
            BUFFER.fill({0,0,0,0});
            break;
        }
        
    }

    size_t CreateHTMLTable(char* buffer, size_t maxlen){
        ESP_LOGI(TAG, "Creating HTML");
        xSemaphoreTake(xSemaphore, portMAX_DELAY);
        std::sort(BUFFER.rbegin(), BUFFER.rend());
        size_t currentLen{0};
        currentLen+=snprintf(buffer+currentLen, maxlen-currentLen, "<!DOCTYPE html>\n<html><head><style>#messages {font-family: Arial, Helvetica, sans-serif;border-collapse: collapse;width:100%%;}#messages td, #messages th {border: 1px solid #ddd;padding: 8px;}#messages tr:nth-child(even){background-color: #f2f2f2;}#messages tr:hover {background-color: #ddd;}#messages th {padding-top: 12px;padding-bottom: 12px; text-align: left;background-color: #04AA6D;color: white;}</style></head><body><h1>Log Messages</h1><table id='messages'><tr><th>Timestamp</th><th>Code</th><th>Description</th><th>LastMessageData</th><th>Count</th></tr>");
        for(int i=0;i<BUFFER.size();i++){
            if(BUFFER[i].messageCode==0) continue;
            currentLen+=snprintf(buffer+currentLen, maxlen-currentLen, "<tr><td class='epochtime'>%llu</td><td>%lu</td><td>%s</td><td>%lu</td><td>%lu</td></tr>", BUFFER[i].lastMessageTimestamp, BUFFER[i].messageCode, messagecodes::N[BUFFER[i].messageCode], BUFFER[i].lastMessageData, BUFFER[i].messageCount);
        }
        currentLen+=snprintf(buffer+currentLen, maxlen-currentLen, "</table><script>Array.from(document.getElementsByClassName('epochtime')).forEach((item)=>{item.innerHTML=new Date(1000*item.innerHTML).toLocaleString();});</script></body></html>");
        xSemaphoreGive(xSemaphore);
        return currentLen;
    }



    void Log(messagecodes::C messageCode, uint32_t messageData){
        bool entryFound{false};
        struct timeval tv_now;
        gettimeofday(&tv_now, nullptr);
        xSemaphoreTake(xSemaphore, portMAX_DELAY);
        for(int i=0;i<BUFFER.size();i++){
            if(BUFFER[i].messageCode==0){
                ESP_LOGI(TAG, "Found an empty logging slot on pos %d for messageCode %lu", i, (uint32_t)messageCode);
                BUFFER[i].messageCode=(uint32_t)messageCode;
                BUFFER[i].lastMessageData=messageData;
                BUFFER[i].lastMessageTimestamp=tv_now.tv_sec;
                BUFFER[i].messageCount=1;
                entryFound=true;
                break;
            }
            else if(BUFFER[i].messageCode==(uint32_t)messageCode){
                ESP_LOGI(TAG, "Found an updateable logging slot on pos %d for messageCode %lu", i, (uint32_t)messageCode);
                BUFFER[i].lastMessageData=messageData;
                BUFFER[i].lastMessageTimestamp=tv_now.tv_sec;
                BUFFER[i].messageCount++;
                entryFound=true;
                break;
            }
        }
        if(!entryFound){
            //search oldest and overwrite
            time_t  oldestTimestamp=BUFFER[0].lastMessageTimestamp;
            size_t oldestIndex{0};
            for(size_t i=1;i<BUFFER.size();i++){
                if(BUFFER[i].lastMessageTimestamp<oldestTimestamp){
                    oldestTimestamp=BUFFER[i].lastMessageTimestamp;
                    oldestIndex=i;
                }
            }
            ESP_LOGI(TAG, "Found the oldest logging slot on pos %d for messageCode %lu", oldestIndex, (uint32_t)messageCode);
            BUFFER[oldestIndex].messageCode=(uint32_t)messageCode;
            BUFFER[oldestIndex].lastMessageData=messageData;
            BUFFER[oldestIndex].lastMessageTimestamp=tv_now.tv_sec;
            BUFFER[oldestIndex].messageCount=1;

        }
        xSemaphoreGive(xSemaphore);
        return;
    }
};

M* M::singleton{nullptr};
}
#undef TAG