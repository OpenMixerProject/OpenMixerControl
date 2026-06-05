#pragma once

#include <stddef.h>
#include <cstdio>

#include "surface.h"
#include "surface-fader.h"
#include "surface-fader-controller.h"

class Surface;

class WingFaderController : public FaderController
{
    private:
        Surface* surface;
        SurfaceFader faders[13];


        uint8_t GetWingFaderIndex(uint8_t boardId, uint8_t index);
        void SendWingFrame(uint8_t cmd, const uint8_t* payload, size_t len);
        void SetFaderRaw(uint8_t wingFaderIndex, uint16_t position);

    public:
        WingFaderController(X32BaseParameter* basepar, Surface* surface);
        ~WingFaderController() override = default;

        void Init() override;
        void Reset() override;
        void SetFader(uint8_t boardId, uint8_t index, uint16_t position) override;
        void FaderMoved(uint8_t boardId, uint8_t index, uint16_t value) override;
        void Touchcontrol() override;
        void FaderReset() override;
};
