#pragma once

#include <stdint.h>
#include "base.h"

namespace OMC
{

class SurfaceController : public X32Base
{
    protected:

        SurfaceCallback surfaceCallback = nullptr;
        void* callbackArg = nullptr;

    public:

        SurfaceController(X32BaseParameter* basepar) : X32Base(basepar) {}
        virtual ~SurfaceController() = default;

        void Init(SurfaceCallback callback, void* arg)
        {
            surfaceCallback = callback;
            callbackArg = arg;
        }
        virtual void Reset() {};

        virtual void Tick10ms() {};
        virtual void Tick100ms() {};

        virtual void ProcessUartData() {};

        virtual void SendData(MessageBase* message, bool addChecksum) {};

        virtual void SetFader(uint8_t boardId, uint8_t index, uint16_t position) {};
        virtual void FaderMoved(uint8_t boardId, uint8_t index, uint16_t value) {};
        virtual void Touchcontrol() {};
        virtual void FaderReset() {};
        
        virtual void SetLed(SurfaceElementId buttonOrLed, bool ledOn, bool blink) {};
        virtual void SetMeterLed(uint8_t boardId, uint8_t index, uint8_t leds) {};
        virtual void SetLcd(LcdData* p_data, uint p_textCount) {};
};

}