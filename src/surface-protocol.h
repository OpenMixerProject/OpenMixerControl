#pragma once

#include <stdint.h>
#include "base.h"

class SurfaceProtocol : public X32Base
{
    public:
        SurfaceProtocol(X32BaseParameter* basepar) : X32Base(basepar) {}
        virtual ~SurfaceProtocol() = default;

        virtual void Init() = 0;
        virtual void Reset() = 0;
        virtual void SetFader(uint8_t boardId, uint8_t index, uint16_t position) = 0;
};