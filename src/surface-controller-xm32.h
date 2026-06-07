#pragma once

#include "defines.h"
#include "types.h"

#include "surface.h"
#include "surface-fader.h"
#include "surface-controller.h"

class Surface;

class SurfaceControllerXM32 : public SurfaceController
{
    private:

        Uart* uart;

        int surfacePacketCurrentIndex = 0;
        int surfacePacketCurrent = 0;
        uint8_t surfacePacketBuffer[SURFACE_MAX_PACKET_LENGTH][6];
        char surfaceBufferUart[256]; // buffer for UART-readings
        uint8_t receivedBoardId = 0; // BoardID from last received surface event, needed for short messages!

        uint blinkwait = 0;
        bool blinkstate = false;
        set<SurfaceElementId> blinklist;

        SurfaceFader faders[XM32_MAX_FADERS];

        uint8_t GetBoardId(uint8_t faderindex);
        uint8_t GetFaderId(uint8_t faderindex);
        uint8_t GetChannelstripIndex(uint8_t boardId, uint8_t index);

        uint8_t calculateChecksum(const char* data, uint16_t len);

        void Blink();

        void SetLedRaw(uint board, uint index, bool ledOn);
        void SetFaderRaw(uint8_t boardId, uint8_t index, uint16_t position);

    public:

        SurfaceControllerXM32(X32BaseParameter* basepar);
        ~SurfaceControllerXM32() override = default;

        void Reset() override;
        void ProcessUartData() override;

        void Tick100ms() override;

        void SendData(MessageBase* message, bool addChecksum) override;

        void SetFader(uint8_t boardId, uint8_t index, uint16_t position) override;
        void FaderMoved(uint8_t boardId, uint8_t index, uint16_t value) override;
        void Touchcontrol() override;
        void FaderReset() override;

        
        void SetLed(SurfaceElementId buttonOrLed, bool ledOn, bool blink) override;
        void SetMeterLed(uint8_t boardId, uint8_t index, uint8_t leds) override;
        void SetLcd(LcdData* p_data, uint p_textCount) override;
};
