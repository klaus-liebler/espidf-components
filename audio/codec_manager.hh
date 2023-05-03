#pragma once
#include <stdint.h>
#include <errorcodes.hh>

namespace CodecManager{
    class M
    {
    public:
        virtual ErrorCode SetPowerState(bool power)=0;
        virtual ErrorCode SetVolume(uint8_t volume)=0;
    };
    
  
    
}