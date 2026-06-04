#pragma once

#include "defines.h"

#include "surface.h"
#include "surface-fader.h"
#include "surface-fader-controller.h"



class Surface;

class X32FaderController : public FaderController {
private:
    Surface* surface;
    SurfaceFader faders[XM32_MAX_FADERS];

    uint8_t GetBoardId(uint8_t faderindex);
    uint8_t GetFaderId(uint8_t faderindex);
    uint8_t GetChannelstripIndex(uint8_t boardId, uint8_t index);

public:
    X32FaderController(X32BaseParameter* basepar, Surface* surface);
    ~X32FaderController() override = default;

    void Init() override;
    void Reset() override;
    void SetFader(uint8_t boardId, uint8_t index, uint16_t position) override;
    void FaderMoved(uint8_t boardId, uint8_t index, uint16_t value) override;
    void Touchcontrol() override;
    void FaderReset() override;
};
