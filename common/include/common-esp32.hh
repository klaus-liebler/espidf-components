#pragma once
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <driver/gpio.h>
#include <esp_check.h>
#include "common.hh"

#define FLASH_FILE(x)                                             \
    extern const uint8_t x##_start[] asm("_binary_" #x "_start"); \
    extern const uint8_t x##_end[] asm("_binary_" #x "_end");     \
    extern const size_t x##_size asm(#x "_length");
constexpr uint64_t IO(int n)
{
    return (1ULL << n);
}

#define MESSAGELOG_ON_ERROR(x, ml)                                                          \
    do                                                                                      \
    {                                                                                       \
        esp_err_t err_rc_ = (x);                                                            \
        if (unlikely(err_rc_ != ESP_OK))                                                    \
        {                                                                                   \
            webmanager::M::GetSingleton()->Log(ml, (uint32_t)err_rc_);                      \
            ESP_LOGE(TAG, "%s(%d) %s %lu", __FUNCTION__, __LINE__, #ml, (uint32_t)err_rc_); \
        }                                                                                   \
    } while (0)
#define MESSAGELOG_ON_ERRORCODE(x, mc)                                                      \
    do                                                                                      \
    {                                                                                       \
        ErrorCode err_rc_ = (x);                                                            \
        if (err_rc_ != ErrorCode::OK)                                                       \
        {                                                                                   \
            webmanager::M::GetSingleton()->Log(mc, (uint32_t)err_rc_);                      \
            ESP_LOGE(TAG, "%s(%d) %s %lu", __FUNCTION__, __LINE__, #mc, (uint32_t)err_rc_); \
        }                                                                                   \
    } while (0)

#define BREAK_ON_ERROR(x, format, ...)                                               \
    do                                                                               \
    {                                                                                \
        esp_err_t err_rc_ = (x);                                                     \
        if (unlikely(err_rc_ != ESP_OK))                                             \
        {                                                                            \
            ESP_LOGE(TAG, "%s(%d): " format, __FUNCTION__, __LINE__, ##__VA_ARGS__); \
            ret = err_rc_;                                                           \
            break;                                                                   \
        }                                                                            \
    } while (0)

#define RETURN_ON_ERROR(x)                                                                                    \
    do                                                                                                        \
    {                                                                                                         \
        esp_err_t err_rc_ = (x);                                                                              \
        if (unlikely(err_rc_ != ESP_OK))                                                                      \
        {                                                                                                     \
            ESP_LOGE(TAG, "%s(%d): Function returned %s!", __FUNCTION__, __LINE__, esp_err_to_name(err_rc_)); \
            return err_rc_;                                                                                   \
        }                                                                                                     \
    } while (0)

#define RETURN_ON_ERRORCODE(x)                                                                    \
    do                                                                                            \
    {                                                                                             \
        ErrorCode err_rc_ = (x);                                                                  \
        if (err_rc_ != ErrorCode::OK)                                                             \
        {                                                                                         \
            return err_rc_;                                                                       \
        }                                                                                         \
    } while (0)

#define RETURN_ERRORCODE_ON_ERROR(x, errorCode)                                                   \
    do                                                                                            \
    {                                                                                             \
        esp_err_t err_rc_ = (x);                                                                  \
        if (err_rc_ != ESP_OK)                                                                    \
        {                                                                                         \
            return errorCode;                                                                     \
        }                                                                                         \
    } while (0)

#define GOTO_ERROR_ON_ERROR(x, format, ...)                                          \
    do                                                                               \
    {                                                                                \
        esp_err_t err_rc_ = (x);                                                     \
        if (unlikely(err_rc_ != ESP_OK))                                             \
        {                                                                            \
            ESP_LOGE(TAG, "%s(%d): " format, __FUNCTION__, __LINE__, ##__VA_ARGS__); \
            ret = err_rc_;                                                           \
            goto error;                                                              \
        }                                                                            \
    } while (0)

#define GOTO_ERROR_ON_FALSE(a, format, ...)                                          \
    do                                                                               \
    {                                                                                \
        if (unlikely(!(a)))                                                          \
        {                                                                            \
            ESP_LOGE(TAG, "%s(%d): " format, __FUNCTION__, __LINE__, ##__VA_ARGS__); \
            goto error;                                                              \
        }                                                                            \
    } while (0)

#define RETURN_FAIL_ON_FALSE(a, format, ...)                                         \
    do                                                                               \
    {                                                                                \
        if (unlikely(!(a)))                                                          \
        {                                                                            \
            ESP_LOGE(TAG, "%s(%d): " format, __FUNCTION__, __LINE__, ##__VA_ARGS__); \
            return ESP_FAIL;                                                         \
        }                                                                            \
    } while (0)

#define ERRORCODE_CHECK(x)                                                                                            \
    do                                                                                                                \
    {                                                                                                                 \
        ErrorCode __err_rc = (x);                                                                                     \
        if (__err_rc != ErrorCode::OK)                                                                                \
        {                                                                                                             \
            ESP_LOGE(TAG, "Error %d occured in File %s in line %d in expression %s", (int)__err_rc, __FILE__, __LINE__, #x); \
        }                                                                                                             \
    } while (0)

esp_err_t nvs_flash_init_and_erase_lazily();
constexpr int64_t ms2ticks(int64_t ms);
void delayAtLeastMs(int64_t ms);
void delayMs(int64_t ms);
int64_t millis();
esp_err_t ConfigGpioInput(gpio_num_t gpio, gpio_pull_mode_t pullMode);
esp_err_t ConfigGpioOutputOD(gpio_num_t gpio, bool pullup);
esp_err_t ConfigGpioOutputPP(gpio_num_t gpio);