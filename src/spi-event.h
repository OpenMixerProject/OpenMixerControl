#pragma once

#include <stdint.h>

namespace OMC
{

class SpiEvent{

    public:
        uint8_t dsp;
        uint8_t classId;
        uint8_t channel;
        uint8_t index;
        uint8_t valueCount;
        void* values;

        SpiEvent(uint8_t dsp, uint8_t classId, uint8_t channel, uint8_t index, uint8_t valueCount, void* values);
};

}