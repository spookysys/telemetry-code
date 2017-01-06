#pragma once
#include <Arduino.h>
#define assert(x) 
#undef min
#undef max

namespace misc
{
    template <typename T>
    static T swapEndianness(T u)
    {
        union {
            T u;
            uint8_t u8[sizeof(T)];
        } source, dest;
    
        source.u = u;
    
        for (size_t k = 0; k < sizeof(T); k++)
            dest.u8[k] = source.u8[sizeof(T) - k - 1];
    
        return dest.u;
    }
}

