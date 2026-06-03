#pragma once

#include <stdint.h>
#include "defines.h"
#include "types.h"
#include "../lib/WString.h"

class SurfaceEvent{
    public:
        OMC_BOARD boardId;
        uint8_t classId;
        uint8_t index;
        uint16_t value;

        SurfaceEvent(OMC_BOARD boardId, uint8_t classId, uint8_t index, uint16_t value);
        String ToString(void);
};