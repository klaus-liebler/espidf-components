#pragma once
#include <inttypes.h>





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