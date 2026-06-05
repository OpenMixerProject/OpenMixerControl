#include "surface-fader-controller-xm32.h"

X32FaderController::X32FaderController(X32BaseParameter* basepar, Surface* surface)
    : FaderController(basepar), surface(surface) {}

void X32FaderController::Init() {
    // UART already opened in Surface::Init()
}

void X32FaderController::Reset() {
    FaderReset();
    FaderReset();
}

void X32FaderController::FaderReset() {
    // Reset touchcontrol wait time
    for(uint8_t faderindex=0; faderindex<XM32_MAX_FADERS; faderindex++){
        faders[faderindex].wait = 0;
    }

    // Reset position of faders
    uint8_t maxfaderindex = 0;
    if (config->IsModelX32FullOrM32()){
        maxfaderindex = XM32_MAX_FADERS;
    }
    if (config->IsModelX32CompactOrProducerOrM32R()){
        maxfaderindex = XM32_MAX_FADERS-8;
    }

    for(uint8_t faderindex=0; faderindex<maxfaderindex; faderindex++)
    {
        faders[faderindex].position_real = 0;
        surface->SetFaderRaw(GetBoardId(faderindex), GetFaderId(faderindex), 0);
    }
}

void X32FaderController::SetFader(uint8_t boardId, uint8_t index, uint16_t position) {
    uint8_t faderindex = GetChannelstripIndex(boardId, index);
    helper->DEBUG_SURFACE(DEBUGLEVEL_VERBOSE, "Want to move fader at index %d to %d", faderindex, position);
    faders[faderindex].position_wanted = position;
}

void X32FaderController::FaderMoved(uint8_t boardId, uint8_t index, uint16_t value) {
    uint8_t faderindex = GetChannelstripIndex(boardId, index);
    helper->DEBUG_SURFACE(DEBUGLEVEL_VERBOSE, "Fader at index %d moved to %d", faderindex, value);
    faders[faderindex].position_wanted = value;
    faders[faderindex].position_real = value;
    faders[faderindex].wait = 10; // wait 100x 10ms
}

void X32FaderController::Touchcontrol() {
    uint8_t maxfaderindex = 0;
    if (config->IsModelX32FullOrM32()){
        maxfaderindex = XM32_MAX_FADERS;
    }
    if (config->IsModelX32CompactOrProducerOrM32R()){
        maxfaderindex = XM32_MAX_FADERS-8;
    }

    for(uint8_t faderindex=0; faderindex<maxfaderindex; faderindex++)
    {
        if (faders[faderindex].wait > 0)
        {
            faders[faderindex].wait--;
            helper->DEBUG_SURFACE(DEBUGLEVEL_TRACE, "Reduced wait time on fader at index %d to %d", faderindex, faders[faderindex].wait);
        }
        else if (faders[faderindex].position_real != faders[faderindex].position_wanted)
        {
            helper->DEBUG_SURFACE(DEBUGLEVEL_VERBOSE, "Move fader at index %d from %d to %d", faderindex, faders[faderindex].position_real, faders[faderindex].position_wanted);
            faders[faderindex].position_real = faders[faderindex].position_wanted;
            surface->SetFaderRaw(GetBoardId(faderindex), GetFaderId(faderindex), faders[faderindex].position_wanted);
        }
    }
}

uint8_t X32FaderController::GetChannelstripIndex(uint8_t boardId, uint8_t index) {
    switch (boardId)
    {
        case X32_BOARD_L:
            return index;
        case X32_BOARD_M: // only X32 Full
            return index + 8;
        case X32_BOARD_R:
            return index + (config->IsModelX32FullOrM32() ? 16 : 8);  // 16 - X32 Full, 8 - X32 Compact/Producer
        default:
            return 0;
    }
}

uint8_t X32FaderController::GetBoardId(uint8_t faderindex) {
    if(config->IsModelX32FullOrM32())
    {
        if (faderindex < 8){
            return X32_BOARD_L;
        }
        if (faderindex < 16){
            return X32_BOARD_M;
        }
        return X32_BOARD_R;
    }

    if(config->IsModelX32CompactOrProducerOrM32R())
    {
        if (faderindex < 8){
            return X32_BOARD_L;
        }
        return X32_BOARD_R;
    }

    return 0;
}

uint8_t X32FaderController::GetFaderId(uint8_t faderindex) {
    if(config->IsModelX32FullOrM32())
    {
        if (faderindex < 8){
            return faderindex;
        }
        if (faderindex < 16){
            return faderindex-8;
        }
        return faderindex-16;
    }

    if(config->IsModelX32CompactOrProducerOrM32R())
    {
        if (faderindex < 8){
            return faderindex;
        }
        return faderindex-8;
    }

    return 0;
}
