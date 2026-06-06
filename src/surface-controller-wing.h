#pragma once

#include <stddef.h>
#include <cstdio>

#include "surface.h"
#include "surface-fader.h"
#include "surface-controller.h"

class Surface;

class SurfaceControllerWing : public SurfaceController
{
    private:

        Uart* uart_csc;
        Uart* uart_pnlc;

        SurfaceFader faders[13];

        WingFrameParser parser;
        void WingParserFeed(uint8_t byte);
        void WingHandleParsedFrame(uint8_t cmd, const uint8_t* payload, size_t len);

        uint8_t GetWingFaderIndex(uint8_t boardId, uint8_t index);
        void SendWingFrame(uint8_t cmd, const uint8_t* payload, size_t len);
        void SetFaderRaw(uint8_t wingFaderIndex, uint16_t position);

    public:
        SurfaceControllerWing(X32BaseParameter* basepar);
        ~SurfaceControllerWing() override = default;

        void Reset() override;
        void ProcessUartData() override;

        void SetFader(uint8_t boardId, uint8_t index, uint16_t position) override;
        void FaderMoved(uint8_t boardId, uint8_t index, uint16_t value) override;
        void Touchcontrol() override;
        void FaderReset() override;
};
