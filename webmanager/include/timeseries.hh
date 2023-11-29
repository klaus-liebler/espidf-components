#pragma once
#include <cstring>
#include <ctime>
#include <algorithm>
#include <array>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/timers.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <esp_timer.h>
#include <esp_chip_info.h>
#include <time.h>
#include <common-esp32.hh>
#include <esp_log.h>
#include <sys/time.h>
#include <errorcodes.hh>

#define TAG "TMSRS"

namespace timeseries
{

    constexpr size_t SECTOR_SIZE_BYTES{4096};
    constexpr size_t SECTOR_HEADER_SIZE_BYTES{64};
    constexpr size_t SECTORS_10sec_START{0};
    constexpr size_t SECTORS_10sec{16}; // 64k need 16 sectors for wear leveling
    constexpr size_t SECTORS_1min_START{SECTORS_10sec_START + SECTORS_10sec};
    constexpr size_t SECTORS_1min{8}; // 32k
    constexpr size_t SECTORS_1hour_START{SECTORS_1min_START + SECTORS_1min};
    constexpr size_t SECTORS_1hour{4}; // 16k
    constexpr size_t SECTORS_1day_START{SECTORS_1hour_START + SECTORS_1hour};
    constexpr size_t SECTORS_1day{4}; // 16

    enum class Granularity:u8_t
    {
        TEN_SECONDS = 0,
        ONE_MINUTE = 1,
        ONE_HOUR = 2,
        ONE_DAY = 3,
        MAX = 4,
    };

    enum class ReadingFusionStrategy:u8_t
    {
        NONE,
        AFTER_CERTAIN_READINGS_OF_PREVIOUS,
        EACH_FULL_HOUR,
        EACH_FULL_DAY_AT_MIDNIGHT,
    };

    struct GranularityConfig
    {
        size_t SECTOR_START;
        size_t SECTOR_CNT;
        size_t SECTOR_HEADER_SIZE_BYTES;
        ReadingFusionStrategy strategy;
        size_t HOW_MANY_READING_DO_I_NEED_FROM_PREVIOUS;
    };

    struct FourSignals
    {
        int16_t values[4];
    };

    struct FlashSector
    {
        int64_t timeStampSecondsEpoch; // timestamp of the first write to this sector
        int64_t dummy[7];
        FourSignals data[(SECTOR_SIZE_BYTES - 8 * sizeof(int64_t)) / sizeof(FourSignals)];
    };

    constexpr std::array<GranularityConfig, (size_t)Granularity::MAX> GRANULARITY_CONFIG = {{{SECTORS_10sec_START, SECTORS_10sec, SECTOR_HEADER_SIZE_BYTES, ReadingFusionStrategy::NONE, 0},
                                                                                             {SECTORS_1min_START, SECTORS_1min, SECTOR_HEADER_SIZE_BYTES, ReadingFusionStrategy::AFTER_CERTAIN_READINGS_OF_PREVIOUS, 6},
                                                                                             {SECTORS_1hour_START, SECTORS_1hour, SECTOR_HEADER_SIZE_BYTES, ReadingFusionStrategy::AFTER_CERTAIN_READINGS_OF_PREVIOUS, 60},
                                                                                             {SECTORS_1day_START, SECTORS_1day, SECTOR_HEADER_SIZE_BYTES, ReadingFusionStrategy::AFTER_CERTAIN_READINGS_OF_PREVIOUS, 24}}};

    class GranularityRuntime
    {
    public:
        Granularity granularity;
        size_t sectorIndex; // absolute sector index wrt start of partition
        size_t offsetBytes; // write location; absolute byte offset in sector (not how many packets have been written ), 64byte header is included
        const GranularityConfig *cfg;
        size_t writeCounter;

        GranularityRuntime(Granularity granularity) : granularity(granularity),
                                                      sectorIndex(GRANULARITY_CONFIG[(size_t)granularity].SECTOR_START),
                                                      offsetBytes(0),
                                                      cfg(&GRANULARITY_CONFIG[(size_t)granularity]),
                                                      writeCounter(0)
        {
        }

        void Init(const esp_partition_t *partition)
        {
            int64_t lastTimeEpochSeconds{0};
            // wir suchen den sector, bei dem die Zeit erstmal im Vergleich zum vorherigen nach unten geht
            bool found{false};
            for (size_t secIndex = cfg->SECTOR_START; secIndex < cfg->SECTOR_START + cfg->SECTOR_CNT; secIndex++)
            {
                int64_t newTime{-1};
                ESP_ERROR_CHECK(esp_partition_read(partition, SECTOR_SIZE_BYTES * secIndex, &newTime, 8));
                if (newTime < lastTimeEpochSeconds)
                {
                    if (newTime != -1)
                    {
                        // wenn die Zeit nicht auf "-1" zurück geht (=binär zweierkomplement 0xFFFF..), dann muss formatiert werden
                        ESP_LOGI(TAG, "Found initial sector %d for granularity %d. As the sector is NOT empty, I am going to erase it", secIndex, (int)granularity);
                        ESP_ERROR_CHECK(esp_partition_erase_range(partition, SECTOR_SIZE_BYTES * secIndex, SECTOR_SIZE_BYTES));
                    }
                    else
                    {
                        ESP_LOGI(TAG, "Found initial sector %d for granularity %d. As the sector is empty, I do not need to erase it.", secIndex, (int)granularity);
                    }
                    found = true;
                    sectorIndex = secIndex;
                }
                lastTimeEpochSeconds = newTime;
            }
            if (!found)
            {
                // wenn die Zeit niemals kleiner wird und auch nirgends ein leerer Sektor ist, dann ist der erste betrachtete Sektor der älteste
                ESP_LOGI(TAG, "Found initial sector 0 for granularity %d. As the sector is NOT empty, I am going to erase it", (int)granularity);
                ESP_ERROR_CHECK(esp_partition_erase_range(partition, SECTOR_SIZE_BYTES * cfg->SECTOR_START, SECTOR_SIZE_BYTES));
                sectorIndex = 0;
            }
            //TODO: Start 10seconds-Timer
        }

        void PrepareNewSector(const esp_partition_t *partition, time_t secondsEpoch)
        {
            // checks, whether the sector needs to be formatted
            int64_t buf[8];
            ESP_ERROR_CHECK(esp_partition_read(partition, SECTOR_SIZE_BYTES * sectorIndex, buf, 8));
            if (buf[0] != -1)
            {
                ESP_ERROR_CHECK(esp_partition_erase_range(partition, SECTOR_SIZE_BYTES * sectorIndex, SECTOR_SIZE_BYTES));
            }
            buf[0]=secondsEpoch;
            buf[1]=(int64_t)this->granularity;
            ESP_ERROR_CHECK(esp_partition_write(partition, SECTOR_SIZE_BYTES * sectorIndex, buf, 16));
        }

        void AverageNReadingsAndResetWriteCounter(const esp_partition_t *partition, size_t n, FourSignals *signals)
        {
            // wir können beim Aufruf sicher sein, dass die Anzahl auch wirklich vorhanden ist. Aber wird müssen ggf über Buffergrezen hinweg
            size_t currentSectorIndex = this->sectorIndex;
            size_t currentOffsetBytes = this->offsetBytes;
            int32_t averageBuffer[4] = {0};
            int16_t readBuffer[4];
            for (size_t i = 0; i < n; i++)
            {
                currentOffsetBytes -= 8;
                if (currentOffsetBytes < 64)
                {
                    currentSectorIndex--;
                    currentOffsetBytes = SECTOR_SIZE_BYTES - 8;
                    if (currentSectorIndex < cfg->SECTOR_START)
                    {
                        currentOffsetBytes = cfg->SECTOR_START + cfg->SECTOR_CNT;
                    }
                }
                ESP_ERROR_CHECK(esp_partition_read(partition, SECTOR_SIZE_BYTES * currentSectorIndex + currentOffsetBytes, readBuffer, 8));
                for (int j = 0; j < 4; j++)
                    averageBuffer[j] += readBuffer[j];
            }
            for (int j = 0; j < 4; j++)
                signals->values[j] = averageBuffer[j] / n;
            this->writeCounter = 0;
        }

        void Write(const esp_partition_t *partition, time_t secondsEpoch, FourSignals *signals)
        {
            if (offsetBytes == 0)
            {
                PrepareNewSector(partition, secondsEpoch);
                offsetBytes = 64;
            }
            ESP_ERROR_CHECK(esp_partition_write(partition, SECTOR_SIZE_BYTES * sectorIndex + offsetBytes, signals->values, sizeof(FourSignals)));
            offsetBytes += sizeof(FourSignals);
            writeCounter++;
            if (offsetBytes >= SECTOR_SIZE_BYTES)
            {
                sectorIndex++;
                if (sectorIndex == cfg->SECTOR_START + cfg->SECTOR_CNT)
                {
                    sectorIndex = cfg->SECTOR_START;
                }
                offsetBytes = 0;
            }
        }

        void ReadFullSector4096bytes(const esp_partition_t *partition, int goBack, void *buf)
        {
            size_t s = (((sectorIndex - cfg->SECTOR_START) - goBack) % cfg->SECTOR_CNT) + cfg->SECTOR_START; // checked with online gdb
            ESP_ERROR_CHECK(esp_partition_read(partition, SECTOR_SIZE_BYTES * s, buf, SECTOR_SIZE_BYTES));
        }
    };

    class M;
    class M
    {
    private:
        static M *singleton;
        SemaphoreHandle_t semaphore{nullptr};
        TimerHandle_t tmr{nullptr};
        const esp_partition_t *partition{nullptr};
        std::array<GranularityRuntime, (size_t)Granularity::MAX> granularityRuntime = {{
            GranularityRuntime(Granularity::TEN_SECONDS),
            GranularityRuntime(Granularity::ONE_MINUTE),
            GranularityRuntime(Granularity::ONE_HOUR),
            GranularityRuntime(Granularity::ONE_DAY),
        }};

        int16_t *inputs[4];

        static void timerCallback10secondsStatic(TimerHandle_t xTimer)
        {
            M *myself = timeseries::M::GetSingleton();
            myself->timerCallback10seconds();
        }

        ErrorCode GetTimeseries4096byte(Granularity g, int goBack, void *buf)
        {
            GranularityRuntime *rt = G(g);
            rt->ReadFullSector4096bytes(partition, goBack, buf);
            return ErrorCode::OK;
        }

        GranularityRuntime *G(Granularity gran)
        {
            return &granularityRuntime[(size_t)gran];
        }

        void timerCallback10seconds()
        {
            struct timeval tv_now;
            gettimeofday(&tv_now, nullptr);
            time_t secondsEpoch = tv_now.tv_sec;
            secondsEpoch = (secondsEpoch / 10) * 10; // runden auf volle 10s
            // Suche Daten zusammen
            FourSignals sigs;
            for (int i = 0; i < 4; i++)
                sigs.values[i] = *this->inputs[i];
            G(Granularity::TEN_SECONDS)->Write(partition, secondsEpoch, &sigs);
            if (G(Granularity::TEN_SECONDS)->writeCounter == 6)
            {
                secondsEpoch = (secondsEpoch / 60) * 60; // runden auf volle Minuten
                G(Granularity::TEN_SECONDS)->AverageNReadingsAndResetWriteCounter(partition, 6, &sigs);
                G(Granularity::ONE_MINUTE)->Write(partition, secondsEpoch, &sigs);
            }
            if (G(Granularity::ONE_MINUTE)->writeCounter == 60)
            {
                // TODO: Dieser Prozess muss laufen, wenn eine volle Stunde erreicht wurde und mindestens 15 MInuten-Readings existieren
                G(Granularity::ONE_MINUTE)->AverageNReadingsAndResetWriteCounter(partition, 60, &sigs);
                G(Granularity::ONE_HOUR)->Write(partition, secondsEpoch, &sigs);
            }
            if (G(Granularity::ONE_HOUR)->writeCounter == 24)
            {
                // TODO: Die Tageslogik muss anders sein, weil ein Tag nicht immer nach 24h vorbei ist. Hier ist zu prüfen, ob es seit dem letzten Aufruf einen Tageswechsel gegeben hat und dann werden die letzten 23...25h zusammen gefasst
                G(Granularity::ONE_HOUR)->AverageNReadingsAndResetWriteCounter(partition, 60, &sigs);
                G(Granularity::ONE_DAY)->Write(partition, secondsEpoch, &sigs);
            }
        }

    public:
        static M *GetSingleton()
        {
            if (!singleton)
            {
                singleton = new M();
            }
            return singleton;
        }

        
        ErrorCode Init(int16_t *timeseries0, int16_t *timeseries1, int16_t *timeseries2, int16_t *timeseries3)
        {
            if (semaphore != nullptr)
            {
                ESP_LOGE(TAG, "webmanager already started");
                return ErrorCode::GENERIC_ERROR;
            }
            // TODO: check realtime available!
            semaphore = xSemaphoreCreateBinary();
            xSemaphoreGive(semaphore);

            this->partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, EXTRA_PARTITION_SUBTYPE_TIMESERIES, nullptr);
            RETURN_ERRORCODE_ON_FALSE(this->partition, ErrorCode::GENERIC_ERROR, "No Partition Found");
            for (int i = 0; i < (size_t)Granularity::MAX; i++)
                this->granularityRuntime[i].Init(this->partition);

            inputs[0] = timeseries0;
            inputs[1] = timeseries1;
            inputs[2] = timeseries2;
            inputs[3] = timeseries3;

            tmr = xTimerCreate("Timeseries", pdMS_TO_TICKS(10000), pdTRUE, this, timerCallback10secondsStatic);
            RETURN_ERRORCODE_ON_FALSE(xTimerStart(tmr, 10), ErrorCode::GENERIC_ERROR, "Timer start error");
            return ErrorCode::OK;
        }
    };
    
    timeseries::M* M::singleton;
}

#undef TAG
