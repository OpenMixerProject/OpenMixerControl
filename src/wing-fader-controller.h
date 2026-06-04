#pragma once

#include "fader-controller.h"
#include "surface-fader.h"
#include <stddef.h>

class Surface;

struct WingFrameParser {
    uint8_t payload[512];
    size_t len;
    int in_frame;
    int after_star;
    int have_cmd;
    uint8_t cmd;
};

class WingFaderController : public FaderController {
private:
    Surface* surface;
    SurfaceFader faders[13];
    WingFrameParser parser;

    uint8_t GetWingFaderIndex(uint8_t boardId, uint8_t index);
    uint8_t CalculateWingChecksum(const uint8_t *payload, size_t len);
    void SendWingFrame(uint8_t cmd, const uint8_t* payload, size_t len);
    void SetFaderRaw(uint8_t wingFaderIndex, uint16_t position);
    void ParserFeed(uint8_t byte);
    void HandleParsedFrame(uint8_t cmd, const uint8_t* payload, size_t len);

public:
    WingFaderController(X32BaseParameter* basepar, Surface* surface);
    ~WingFaderController() override = default;

    void Init() override;
    void Reset() override;
    void SetFader(uint8_t boardId, uint8_t index, uint16_t position) override;
    void FaderMoved(uint8_t boardId, uint8_t index, uint16_t value) override;
    void Touchcontrol() override;
    void FaderReset() override;
    void ProcessIncomingData() override;
};
