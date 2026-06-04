#include "surface-fader-controller-wing.h"

WingFaderController::WingFaderController(X32BaseParameter* basepar, Surface* surface) : FaderController(basepar), surface(surface)
{
    
}

void WingFaderController::Init()
{
    // UART already opened in Surface::Init()
    Reset();
}

void WingFaderController::Reset()
{
    FaderReset();
}

void WingFaderController::FaderReset()
{
    for (uint8_t i = 0; i < 13; ++i)
    {
        faders[i].wait = 0;
        faders[i].position_real = 0;
        SetFaderRaw(i, 0);
    }
}

void WingFaderController::SetFader(uint8_t boardId, uint8_t index, uint16_t position)
{
    uint8_t wingFaderIndex = GetWingFaderIndex(boardId, index);
    if (wingFaderIndex != 255)
    {
        helper->DEBUG_SURFACE(DEBUGLEVEL_VERBOSE, "Want to move WING fader at index %d to %d", wingFaderIndex, position);
        faders[wingFaderIndex].position_wanted = position;
    }
}

void WingFaderController::FaderMoved(uint8_t boardId, uint8_t index, uint16_t value)
{
    uint8_t wingFaderIndex = GetWingFaderIndex(boardId, index);
    if (wingFaderIndex != 255)
    {
        helper->DEBUG_SURFACE(DEBUGLEVEL_VERBOSE, "WING fader at index %d moved to %d", wingFaderIndex, value);
        faders[wingFaderIndex].position_wanted = value;
        faders[wingFaderIndex].position_real = value;
        faders[wingFaderIndex].wait = 10; // wait 100ms
    }
}

void WingFaderController::Touchcontrol()
{
    for (uint8_t i = 0; i < 13; ++i)
    {
        if (faders[i].wait > 0)
        {
            faders[i].wait--;
        }
        else if (faders[i].position_real != faders[i].position_wanted)
        {
            helper->DEBUG_SURFACE(DEBUGLEVEL_VERBOSE, "Move WING fader at index %d from %d to %d", i, faders[i].position_real, faders[i].position_wanted);
            faders[i].position_real = faders[i].position_wanted;
            SetFaderRaw(i, faders[i].position_wanted);
        }
    }
}

uint8_t WingFaderController::GetWingFaderIndex(uint8_t boardId, uint8_t index)
{
    return index;
}



void WingFaderController::SendWingFrame(uint8_t cmd, const uint8_t* payload, size_t len)
{
    MessageBase msg;
    msg.AddRawByte(0x2a); // WING_FRAME_STAR
    
    if (cmd == 0x2a) {
        msg.AddRawByte(0x2a);
        msg.AddRawByte(0x40);
    } else {
        msg.AddRawByte(cmd);
    }

    for (size_t i = 0; i < len; ++i) {
        if (payload[i] == 0x2a) {
            msg.AddRawByte(0x2a);
            msg.AddRawByte(0x40);
        } else {
            msg.AddRawByte(payload[i]);
        }
    }

    msg.AddRawByte(0x2a);
    uint8_t chk = helper->CalculateWingChecksum(payload, len);
    if (chk == 0x2a) {
        msg.AddRawByte(0x2a);
        msg.AddRawByte(0x40);
    } else {
        msg.AddRawByte(chk);
    }

    surface->uart->Tx(&msg);
}

void WingFaderController::SetFaderRaw(uint8_t wingFaderIndex, uint16_t position)
{
    if (wingFaderIndex > 12 || position > 4095) return;

    uint8_t payload[3];
    payload[0] = wingFaderIndex;
    payload[1] = (uint8_t)(position & 0xff);
    payload[2] = (uint8_t)((position >> 8) & 0x0f);

    SendWingFrame('F', payload, 3);
}
