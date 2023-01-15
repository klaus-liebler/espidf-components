#pragma once
#include <stdint.h>
#include <stddef.h>
#include <array>

#define ALL4  __attribute__ ((aligned (16)))

#define FLASH_FILE(x) \
extern const uint8_t x##_start[] asm("_binary_" #x "_start");\
extern const uint8_t x##_end[] asm("_binary_" #x "_end");\
extern const size_t  x##_size asm(#x"_length");

constexpr uint64_t IO(int n){
    return (1ULL<<n);
}

#define BREAK_ON_ERROR(x, format, ...) do {                               \
        esp_err_t err_rc_ = (x);                                                                \
        if (unlikely(err_rc_ != ESP_OK)) {                                                      \
            ESP_LOGE(TAG, "%s(%d): " format, __FUNCTION__, __LINE__, ##__VA_ARGS__);        \
            ret = err_rc_;                                                                      \
            break;                                                                      \
        }                                                                                       \
    } while(0)

#define RETURN_ON_ERROR(x) do {                                       \
        esp_err_t err_rc_ = (x);                                                                \
        if (unlikely(err_rc_ != ESP_OK)) {                                                      \
            ESP_LOGE(TAG, "%s(%d): Function returned %s!", __FUNCTION__, __LINE__, esp_err_to_name(err_rc_));        \
            return err_rc_;                                                                     \
        }                                                                                       \
    } while(0)

#define RETURN_ON_ERRORCODE(x) do {                                       \
        ErrorCode err_rc_ = (x);                                                                \
        if (err_rc_ != ErrorCode::OK) {                                                      \
            ESP_LOGE(TAG, "%s(%d): Function returned %d!", __FUNCTION__, __LINE__, (int)err_rc_);        \
            return err_rc_;                                                                     \
        }                                                                                       \
    } while(0)

#define RETURN_ERRORCODE_ON_ERROR(x, errorCode) do {                                       \
        esp_err_t err_rc_ = (x);                                                                \
        if (err_rc_ != ESP_OK) {                                                      \
            ESP_LOGE(TAG, "%s(%d): Function returned %d!", __FUNCTION__, __LINE__, (int)err_rc_);        \
            return errorCode;                                                                     \
        }                                                                                       \
    } while(0)

#define GOTO_ERROR_ON_ERROR(x, format, ...) do {                               \
        esp_err_t err_rc_ = (x);                                                                \
        if (unlikely(err_rc_ != ESP_OK)) {                                                      \
            ESP_LOGE(TAG, "%s(%d): " format, __FUNCTION__, __LINE__, ##__VA_ARGS__);        \
            ret = err_rc_;                                                                      \
            goto error;                                                                      \
        }                                                                                       \
    } while(0)

#define GOTO_ERROR_ON_FALSE(a, format, ...) do { \
        if (unlikely(!(a))) { \
            ESP_LOGE(TAG, "%s(%d): " format, __FUNCTION__, __LINE__, ##__VA_ARGS__); \
            goto error; \
        } \
    } while(0)

#define RETURN_FAIL_ON_FALSE(a, format, ...) do {                             \
        if (unlikely(!(a))) {                                                                   \
            ESP_LOGE(TAG, "%s(%d): " format, __FUNCTION__, __LINE__, ##__VA_ARGS__);        \
            return ESP_FAIL;                                                                    \
        }                                                                                       \
    } while(0)


#define ERRORCODE_CHECK(x)                                        \
    do                                                            \
    {                                                             \
        ErrorCode __err_rc = (x);                                 \
        if (__err_rc != ErrorCode::OK)                            \
        {                                                         \
            printf("Error %d occured in File %s in line %d in expression %s", (int)__err_rc, __FILE__, __LINE__, #x);\
        }                                                         \
    } while (0)

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;
typedef int64_t tms_t;

template <typename T>
void SetBitIdx(T &value, const int bitIdx) {
    value |= (1 << bitIdx);
}

template <typename T>
void SetBitMask(T &value, const int bitMask) {
    value |= bitMask;
}

template <typename T>
bool GetBitIdx(const T value, const int bitIdx) {
    return value & (1L << bitIdx);
}

template <typename T>
bool GetBitMask(const T value, const int bitMask) {
    return value & bitMask;
}

template <typename T>
inline void ClearBitIdx(T &value, const int bitIdx)
{
    value &= ~(1L << bitIdx);
}

template <typename T>
inline void ClearBitMask(T &value, const int bitMask)
{
    value &= ~(bitMask);
}

template <typename T> 
bool IntervalIntersects(const T& aLow, const T& aHigh, const T& bLow, const T& bHigh) {
    T min = std::max(aLow, bLow);
    T max =  std::min(aHigh, bHigh);
    return  max>=min;
}

template <size_t K>
bool GetBitInU8Array(const std::array<uint8_t, K> *arr, size_t offset, size_t bitIdx){
    uint8_t b = (*arr)[offset + (bitIdx>>3)];
    uint32_t bitpos = bitIdx & 0b111;
    return b & (1<<bitpos);
}



template<class T>
constexpr const T clamp_kl( const T v, const T lo, const T hi)
{
    return v<lo?lo:v>hi?hi:v;
}



bool GetBitInU8Buf(const uint8_t *buf, size_t offset, size_t bitIdx);

void WriteInt8(int8_t value, uint8_t *message, uint32_t offset);
int16_t ParseInt16(const uint8_t * const message, uint32_t offset);

void WriteInt16(int16_t value, uint8_t *message, uint32_t offset);

uint16_t ParseUInt16(const uint8_t * const message, uint32_t offset);

uint64_t ParseUInt64(const uint8_t * const message, uint32_t offset);

void WriteUInt16(uint16_t value, uint8_t *message, uint32_t offset);

uint32_t ParseUInt32(const uint8_t * const message, uint32_t offset);

int32_t ParseInt32(const uint8_t * const message, uint32_t offset);

void WriteUInt32(uint32_t value, uint8_t *message, uint32_t offset);

float ParseFloat32(const uint8_t * const message, uint32_t offset);

void WriteInt64(int64_t value, uint8_t *message, uint32_t offset);