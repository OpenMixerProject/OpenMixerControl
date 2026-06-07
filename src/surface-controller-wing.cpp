#include "surface-controller-wing.h"

SurfaceControllerWing::SurfaceControllerWing(X32BaseParameter* basepar) : SurfaceController(basepar)
{
    memset(&parser, 0, sizeof(parser));

    uart_csc = new Uart(basepar);
    uart_pnlc = new Uart(basepar);

    uart_csc->Open("/dev/ttymxc4", 115200, true);
    
    Reset();
}

void SurfaceControllerWing::ProcessUartData()
{
    char buf[256];
    int n = uart_csc->Rx(buf, sizeof(buf));
    if (n > 0) {
        char hex[3 * 256 + 1];
        int pos = 0;
        for (int i = 0; i < n && i < 256; ++i) {
            pos += snprintf(hex + pos, sizeof(hex) - pos, "%02x ", (uint8_t)buf[i]);
        }
        hex[pos] = '\0';
        helper->DEBUG_SURFACE(DEBUGLEVEL_TRACE, "WingFaderController RX %d bytes: %s", n, hex);
        for (int i = 0; i < n; ++i) {
            WingParserFeed((uint8_t)buf[i]);
        }
    }
}

void SurfaceControllerWing::WingParserFeed(uint8_t byte) {
    if (!parser.in_frame) {
        if (byte == 0x2a) {
            parser.in_frame = 1;
            parser.after_star = 1;
            parser.have_cmd = 0;
            parser.len = 0;
        }
        return;
    }

    if (parser.after_star) {
        parser.after_star = 0;
        if (byte == 0x2a) {
            parser.have_cmd = 0;
            parser.len = 0;
            parser.after_star = 1;
        } else if (byte == 0x40) {
            if (parser.len < sizeof(parser.payload))
                parser.payload[parser.len++] = 0x2a;
        } else if (byte & 0x80) {
            if (parser.have_cmd) {
                uint8_t expect = helper->CalculateWingChecksum(parser.payload, parser.len);
                if (byte == expect) {
                    WingHandleParsedFrame(parser.cmd, parser.payload, parser.len);
                }
            }
            memset(&parser, 0, sizeof(parser));
        } else if (!parser.have_cmd) {
            parser.cmd = byte;
            parser.have_cmd = 1;
        } else if (parser.len + 2 <= sizeof(parser.payload)) {
            parser.payload[parser.len++] = 0x2a;
            parser.payload[parser.len++] = byte;
        }
        return;
    }

    if (byte == 0x2a) {
        parser.after_star = 1;
    } else if (!parser.have_cmd) {
        parser.cmd = byte;
        parser.have_cmd = 1;
    } else if (parser.len < sizeof(parser.payload)) {
        parser.payload[parser.len++] = byte;
    }
}

void SurfaceControllerWing::WingHandleParsedFrame(uint8_t cmd, const uint8_t* payload, size_t len)
{
    helper->DEBUG_SURFACE(DEBUGLEVEL_TRACE, "WingFaderController parsed frame cmd=0x%02x len=%zu", cmd, len);

    if (len > 0)
    {
        char hex[3 * 256 + 1];
        int pos = 0;
        for (size_t i = 0; i < len && i < 256; ++i)
        {
            pos += snprintf(hex + pos, sizeof(hex) - pos, "%02x ", payload[i]);
        }
        hex[pos] = '\0';
        helper->DEBUG_SURFACE(DEBUGLEVEL_TRACE, "WingFaderController payload: %s", hex);
    }

    if (cmd == 'f' && len == 3)
    {
        uint8_t index = payload[0];
        uint16_t value = payload[1] | (payload[2] << 8);

        helper->DEBUG_SURFACE(DEBUGLEVEL_VERBOSE, "WingFaderController Fader: index=0x%02x value=%u", index, value);
		//ProcessSurface(OMC_BOARD_WING, 'f', index, value);
        surfaceCallback(callbackArg, OMC_BOARD_WING, cmd, index, value);
    }

    if (cmd == 'b' && len == 2)
    {
        uint8_t index = payload[0];
        uint16_t value = payload[1];

		helper->DEBUG_SURFACE(DEBUGLEVEL_VERBOSE, "WingFaderController Button: index=0x%02x value=%u", index, value);
		//ProcessSurface(OMC_BOARD_WING, 'b', index, value);
        surfaceCallback(callbackArg, OMC_BOARD_WING, cmd, index, value);
    }
}

void SurfaceControllerWing::Reset()
{
    FaderReset();
}

void SurfaceControllerWing::FaderReset()
{
    for (uint8_t i = 0; i < 13; ++i)
    {
        faders[i].wait = 0;
        faders[i].position_real = 0;
        SetFaderRaw(i, 0);
    }
}

void SurfaceControllerWing::SetFader(uint8_t boardId, uint8_t index, uint16_t position)
{
    uint8_t wingFaderIndex = GetWingFaderIndex(boardId, index);
    if (wingFaderIndex != 255)
    {
        helper->DEBUG_SURFACE(DEBUGLEVEL_VERBOSE, "Want to move WING fader at index %d to %d", wingFaderIndex, position);
        faders[wingFaderIndex].position_wanted = position;
    }
}

void SurfaceControllerWing::FaderMoved(uint8_t boardId, uint8_t index, uint16_t value)
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

void SurfaceControllerWing::Touchcontrol()
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

uint8_t SurfaceControllerWing::GetWingFaderIndex(uint8_t boardId, uint8_t index)
{
    return index;
}



void SurfaceControllerWing::SendWingFrame(uint8_t cmd, const uint8_t* payload, size_t len)
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

    uart_csc->Tx(&msg);
}

void SurfaceControllerWing::SetFaderRaw(uint8_t wingFaderIndex, uint16_t position)
{
    if (wingFaderIndex > 12 || position > 4095) return;

    uint8_t payload[3];
    payload[0] = wingFaderIndex;
    payload[1] = (uint8_t)(position & 0xff);
    payload[2] = (uint8_t)((position >> 8) & 0x0f);

    SendWingFrame('F', payload, 3);
}
