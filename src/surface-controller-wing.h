#pragma once

#include <stddef.h>
#include <cstdio>
#include <map>

#include "surface.h"
#include "surface-fader.h"
#include "surface-controller.h"

namespace OMC
{

struct WingFrameParser {
    uint8_t payload[512];
    size_t len;
    int in_frame;
    int after_star;
    int have_cmd;
    uint8_t cmd;
};

// Die 4 möglichen Zustände einer LED
enum class LedState : uint8_t 
{
    AUS             = 0b00,
    HINTERGRUND_AN  = 0b01,
    LED_AN          = 0b10,
    BLINKEN         = 0b11
};

class SurfaceControllerWing : public SurfaceController
{
    private:

        Uart* uart_csc;
        Uart* uart_pnlc;

        SurfaceFader faders[13];

        uint heartbeatWaitCounter = 0;

        uint8_t csc_ledBuffer[10] = {0};
        uint8_t csc_led_Strip_buffer[13] = {0};
        uint8_t pnlc_led_buffer[3] = {0};
        std::map<SurfaceElementId, uint> csc_led_map;
        std::map<SurfaceElementId, uint> csc_led_strip_map;
        std::map<SurfaceElementId, uint> pnlc_led_map;

        uint ledDebug = 0;

        WingFrameParser parser;
        void WingParserFeed(bool pnlc, uint8_t byte);
        void WingHandleParsedFrame(bool pnlc, uint8_t cmd, const uint8_t* payload, size_t len);

        uint8_t GetWingFaderIndex(uint8_t boardId, uint8_t index);
        void SendWingFrame(bool pnlc, uint8_t cmd, const uint8_t* payload, uint len);
        void SetFaderRaw(uint8_t wingFaderIndex, uint16_t position);

        void SendHeartbeat();
        void setCSCBrightnessRaw();

        // LED CSC
        void setCSCLedRaw(int ledIndex, LedState state);
        const uint8_t* getCSCLedBuffer() const;
        void debugCSCLedPrint() const;
        
        // LED CSC Channel Strip
        void setCSCLedStripRaw(int ledIndex, LedState state);
        const uint8_t* getCSCLedStripBuffer() const;

        // LED PNLC
        void setPnlcLedRaw(int ledIndex, LedState state);
        const uint8_t* getPnlcLedBuffer() const;

        void SetUserLcd(LcdData* p_data, uint p_textCount);

    public:
        SurfaceControllerWing(X32BaseParameter* basepar);
        ~SurfaceControllerWing() override = default;

        void Reset() override;

        void Tick100ms() override;

        void ProcessUartData() override;

        void SetFader(uint8_t boardId, uint8_t index, uint16_t position) override;
        void FaderMoved(uint8_t boardId, uint8_t index, uint16_t value) override;
        void Touchcontrol() override;
        void FaderReset() override;

        void SetLed(SurfaceElementId buttonOrLed, bool ledOn, bool blink) override;
        void SetLcd(LcdData* p_data, uint p_textCount);
};


}