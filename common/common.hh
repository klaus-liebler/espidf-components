

#pragma once

template <typename T>
static inline void SET_BIT(T &x, int bitNum)
{
    x |= (1L << bitNum);
}



template <typename T>
static inline bool READ_BIT(T x, int bitNum) //same as CHECK_BIT
{
    return x & (1L << bitNum);
}



template <typename T>
static inline bool CHECK_BIT(T x, int bitNum) //same as READ_BIT
{
    return x & (1L << bitNum);
}


template <typename T>
static inline void CLEAR_BIT(T &x, int bitNum)
{
    ((x) &= ~((1) << (bitNum)));
}