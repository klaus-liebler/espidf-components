#pragma once
#include <stdint.h>
#include <array>
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
static inline void ClearBitIdx(T &value, const int bitIdx)
{
    value &= ~(1L << bitIdx);
}

template <typename T>
static inline void ClearBitMask(T &value, const int bitMask)
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