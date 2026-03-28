#pragma once
#include <inttypes.h>

#define LOG_AND_RETURN_ON_ERRORCODE(x, errorCode, format, ...)                                          \
    do                                                                               \
    {                                                                                \
        ErrorCode err_rc_ = (x);                                                     \
        if (err_rc_ != ErrorCode::OK)                                             \
        {                                                                            \
            ESP_LOGE(TAG, "%s(%d): " format, __FUNCTION__, __LINE__, ##__VA_ARGS__); \
            return errorCode;                                                           \
        }                                                                            \
    } while (0)

#define LOG_AND_GOTO_ERROR_ON_ERRORCODE(x, format, ...)                                          \
    do                                                                               \
    {                                                                                \
        ErrorCode err_rc_ = (x);                                                     \
        if (err_rc_ != ErrorCode::OK)                                             \
        {                                                                            \
            ESP_LOGE(TAG, "%s(%d): " format, __FUNCTION__, __LINE__, ##__VA_ARGS__); \
            goto error;                                                           \
        }                                                                            \
    } while (0)



#undef _
#define _(n) n 
enum class ErrorCode:int
{
#include "errorcodes.inc"
};

#undef _
#define _(n) #n 

constexpr const char* ErrorCodeStr[]{
    #include "errorcodes.inc"
};
#undef _