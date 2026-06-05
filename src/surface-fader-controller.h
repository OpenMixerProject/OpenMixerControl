#pragma once

#include <stdint.h>
#include "base.h"

class FaderController : public X32Base
{
    public:
        typedef void (*FaderMovedCallback)(void* arg, uint8_t boardId, uint8_t index, uint16_t value);

        FaderController(X32BaseParameter* basepar) : X32Base(basepar) {}
        virtual ~FaderController() = default;

        virtual void Init() = 0;
        virtual void Reset() = 0;
        virtual void SetFader(uint8_t boardId, uint8_t index, uint16_t position) = 0;
        virtual void FaderMoved(uint8_t boardId, uint8_t index, uint16_t value) = 0;
        virtual void Touchcontrol() = 0;
        virtual void FaderReset() = 0;

        void SetCallback(FaderMovedCallback cb, void* arg)
        {
            faderMovedCb = cb;
            callbackArg = arg;
        }

    protected:
        FaderMovedCallback faderMovedCb = nullptr;
        void* callbackArg = nullptr;
};
