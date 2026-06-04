#include "wing-fader-controller.h"
#include "surface.h"
#include <cstdio>

WingFaderController::WingFaderController(X32BaseParameter* basepar, Surface* surface)
    : FaderController(basepar), surface(surface) {
    memset(&parser, 0, sizeof(parser));
}

void WingFaderController::Init() {
    // UART already opened in Surface::Init()
    Reset();
}

void WingFaderController::Reset() {
    FaderReset();
}

void WingFaderController::FaderReset() {
    for (uint8_t i = 0; i < 13; ++i) {
        faders[i].wait = 0;
        faders[i].position_real = 0;
        SetFaderRaw(i, 0);
    }
}

void WingFaderController::SetFader(uint8_t boardId, uint8_t index, uint16_t position) {
    uint8_t wingFaderIndex = GetWingFaderIndex(boardId, index);
    if (wingFaderIndex != 255) {
        helper->DEBUG_SURFACE(DEBUGLEVEL_VERBOSE, "Want to move WING fader at index %d to %d", wingFaderIndex, position);
        faders[wingFaderIndex].position_wanted = position;
    }
}

void WingFaderController::FaderMoved(uint8_t boardId, uint8_t index, uint16_t value) {
    uint8_t wingFaderIndex = GetWingFaderIndex(boardId, index);
    if (wingFaderIndex != 255) {
        helper->DEBUG_SURFACE(DEBUGLEVEL_VERBOSE, "WING fader at index %d moved to %d", wingFaderIndex, value);
        faders[wingFaderIndex].position_wanted = value;
        faders[wingFaderIndex].position_real = value;
        faders[wingFaderIndex].wait = 10; // wait 100ms
    }
}

void WingFaderController::Touchcontrol() {
    for (uint8_t i = 0; i < 13; ++i) {
        if (faders[i].wait > 0) {
            faders[i].wait--;
        } else if (faders[i].position_real != faders[i].position_wanted) {
            helper->DEBUG_SURFACE(DEBUGLEVEL_VERBOSE, "Move WING fader at index %d from %d to %d", i, faders[i].position_real, faders[i].position_wanted);
            faders[i].position_real = faders[i].position_wanted;
            SetFaderRaw(i, faders[i].position_wanted);
        }
    }
}

uint8_t WingFaderController::GetWingFaderIndex(uint8_t boardId, uint8_t index) {
    if (boardId == X32_BOARD_L) {
        if (index < 8) {
            return index; // WING fader 1-8
        }
    } else if (boardId == X32_BOARD_R) {
        if (index == 8) { // Master fader
            return 12;
        }
        if (index < 4) {
            return 8 + index; // WING fader 9-12
        }
    }
    return 255;
}

uint8_t WingFaderController::CalculateWingChecksum(const uint8_t *payload, size_t len) {
    unsigned int sum = 0;
    for (size_t i = 0; i < len; ++i) {
        sum = (sum + payload[i]) & 0xffu;
    }
    return (uint8_t)(((sum & 0xffu) ^ (len & 0xffu)) | 0x80u);
}

void WingFaderController::SendWingFrame(uint8_t cmd, const uint8_t* payload, size_t len) {
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
    uint8_t chk = CalculateWingChecksum(payload, len);
    if (chk == 0x2a) {
        msg.AddRawByte(0x2a);
        msg.AddRawByte(0x40);
    } else {
        msg.AddRawByte(chk);
    }

    surface->uart->Tx(&msg);
}

void WingFaderController::SetFaderRaw(uint8_t wingFaderIndex, uint16_t position) {
    if (wingFaderIndex > 12 || position > 4095) return;

    uint8_t payload[3];
    payload[0] = wingFaderIndex;
    payload[1] = (uint8_t)(position & 0xff);
    payload[2] = (uint8_t)((position >> 8) & 0x0f);

    SendWingFrame('F', payload, 3);
}

void WingFaderController::ParserFeed(uint8_t byte) {
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
                uint8_t expect = CalculateWingChecksum(parser.payload, parser.len);
                if (byte == expect) {
                    HandleParsedFrame(parser.cmd, parser.payload, parser.len);
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

void WingFaderController::HandleParsedFrame(uint8_t cmd, const uint8_t* payload, size_t len) {
    helper->DEBUG_SURFACE(DEBUGLEVEL_VERBOSE, "WingFaderController parsed frame cmd=0x%02x len=%zu", cmd, len);
    if (len > 0) {
        char hex[3 * 256 + 1];
        int pos = 0;
        for (size_t i = 0; i < len && i < 256; ++i) {
            pos += snprintf(hex + pos, sizeof(hex) - pos, "%02x ", payload[i]);
        }
        hex[pos] = '\0';
        helper->DEBUG_SURFACE(DEBUGLEVEL_VERBOSE, "WingFaderController payload: %s", hex);
    }
    if (cmd == 'f' && len == 3) {
        uint8_t wingFaderIndex = payload[0];
        uint16_t value = payload[1] | (payload[2] << 8);

        uint8_t boardId = 255;
        uint8_t index = 255;

        if (wingFaderIndex < 8) {
            boardId = X32_BOARD_L;
            index = wingFaderIndex;
        } else if (wingFaderIndex < 12) {
            boardId = X32_BOARD_R;
            index = wingFaderIndex - 8;
        } else if (wingFaderIndex == 12) {
            boardId = X32_BOARD_R;
            index = 8; // Master
        }

        if (boardId != 255 && faderMovedCb) {
            fprintf(stderr, "WingFaderController decoded: cmd=0x%02x boardId=%u index=%u value=%u\n", cmd, (unsigned)boardId, (unsigned)index, (unsigned)value);
            faderMovedCb(callbackArg, boardId, index, value);
        }
    }
}

void WingFaderController::ProcessIncomingData() {
    char buf[256];
    int n = surface->uart->Rx(buf, sizeof(buf));
    if (n > 0) {
        char hex[3 * 256 + 1];
        int pos = 0;
        for (int i = 0; i < n && i < 256; ++i) {
            pos += snprintf(hex + pos, sizeof(hex) - pos, "%02x ", (uint8_t)buf[i]);
        }
        hex[pos] = '\0';
        helper->DEBUG_SURFACE(DEBUGLEVEL_VERBOSE, "WingFaderController RX %d bytes: %s", n, hex);
        for (int i = 0; i < n; ++i) {
            ParserFeed((uint8_t)buf[i]);
        }
    }
}
