#include "surface-controller-wing.h"

SurfaceControllerWing::SurfaceControllerWing(X32BaseParameter* basepar) : SurfaceController(basepar)
{
    memset(&parser, 0, sizeof(parser));

    uart_csc = new Uart(basepar);
    uart_pnlc = new Uart(basepar);

    uart_pnlc->Open("/dev/ttymxc3", 115200, true);
    uart_csc->Open("/dev/ttymxc4", 115200, true);

    csc_led_map.insert({
        {SurfaceElementId::WING_CH1_12, 1},
        {SurfaceElementId::WING_CH13_24, 2},
        {SurfaceElementId::WING_CH25_36, 3}
    });
    
    if(config->IsModelWingCompact())
    {
        // Channel Strips
        for (uint i = 0; i < 13; i++)
        {
            csc_led_strip_map.insert({
                {config->CalcSurfaceElementId(SurfaceElementId::WING_SELECT_1, i), i},
                {config->CalcSurfaceElementId(SurfaceElementId::WING_SOLO_1, i), i + 13},
                {config->CalcSurfaceElementId(SurfaceElementId::WING_MUTE_1, i), i + 13 + 13}
            });
        }
    }

    for (int i = 0; i < 40; ++i) {
            setCSCLedRaw(i, LedState::AUS);
    }

    for (int i = 0; i < 39; ++i) {
            setCSCLedStripRaw(i, LedState::AUS);
    }
    
    Reset();
}


void SurfaceControllerWing::Tick100ms()
{
    heartbeatWaitCounter++;
    if (heartbeatWaitCounter > 30) // 3 seconds
    {
        SendHeartbeat();

        heartbeatWaitCounter = 0;
    }
    
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

        // DEBUG
        if (value && (index == 0x46 || index == 0x47))
        {
            if (index == 0x46)
            {
                ledDebug--;
            }
            else if (index == 0x47)
            {
                ledDebug++;
            }

            printf("LED DEBUG: %d\n", ledDebug);
            setCSCLedRaw(ledDebug, LedState::LED_AN);
            debugCSCLedPrint();
            SendWingFrame('L', getCSCLedBuffer(), 10);
        }   
    }
}




void SurfaceControllerWing::Reset()
{
    FaderReset();
    setCSCBrightnessRaw();
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



void SurfaceControllerWing::SendWingFrame(uint8_t cmd, const uint8_t* payload, uint len)
{
    MessageBase msg;
    msg.AddRawByte(0x2a); // WING_FRAME_STAR
    
    if (cmd == 0x2a) {
        msg.AddRawByte(0x2a);
        msg.AddRawByte(0x40);
    } else {
        msg.AddRawByte(cmd);
    }

    for (uint i = 0; i < len; ++i) {
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

// Setzt den Zustand einer einzelnen LED (0 bis 39)
void SurfaceControllerWing::setCSCLedRaw(int ledIndex, LedState state)
{
    if (ledIndex < 0 || ledIndex >= 40) {
        return; // Index außerhalb des gültigen Bereichs
    }

    // 1. Bestimmen, in welchem Byte sich die LED befindet (4 LEDs pro Byte)
    int byteIndex = ledIndex / 4;

    // 2. Bestimmen, welches Bit-Paar im Byte adressiert wird (von links nach rechts)
    // LED 0 -> Paar 3 (Bits 7,6), LED 1 -> Paar 2 (Bits 5,4), etc.
    int pairPosition = (ledIndex % 4);
    int bitShift = pairPosition * 2;

    // 3. Altes Bit-Paar an dieser Position löschen (auf 00 setzen)
    csc_ledBuffer[byteIndex] &= ~(0x03 << bitShift);

    // 4. Neuen Zustand an die richtige Position schieben und per ODER einfügen
    csc_ledBuffer[byteIndex] |= (static_cast<uint8_t>(state) << bitShift);

    return;
}


const uint8_t* SurfaceControllerWing::getCSCLedBuffer() const
{
    return csc_ledBuffer;
}

// Hilfsfunktion zur Ausgabe auf der Konsole (Hex-Format)
void SurfaceControllerWing::debugCSCLedPrint() const {
    for (int i = 0; i < 10; ++i) {
        std::cout << "0x" << std::hex << std::setw(2) << std::setfill('0') 
                    << static_cast<int>(csc_ledBuffer[i]) << " ";
    }
    std::cout << "\n";
}

// Setzt den Zustand einer einzelnen LED (0 bis 39)
void SurfaceControllerWing::setCSCLedStripRaw(int ledIndex, LedState state)
{
    if (ledIndex < 0 || ledIndex >= 39) {
        return; // Ungültiger LED-Index
    }

    // Berechnung der Spalte und der Reihe:
    // Wir nehmen an: LED 0-12 ist Reihe 1, 13-25 Reihe 2, 26-38 Reihe 3.
    // Falls deine LEDs spaltenweise durchnummeriert sind, müsste man hier / und % tauschen.
    int row = ledIndex / 13; // Bestimmt die Reihe (0, 1 oder 2)
    int col = ledIndex % 13; // Bestimmt die Spalte (0 bis 12)

    // Zuordnung im Byte (von rechts nach links):
    // Reihe 0 -> Bits 1 & 0 (Shift 0)
    // Reihe 1 -> Bits 3 & 2 (Shift 2)
    // Reihe 2 -> Bits 5 & 4 (Shift 4)
    int bitShift = row * 2;

    // 1. Altes Bit-Paar an dieser Position im jeweiligen Spalten-Byte löschen
    csc_led_Strip_buffer[col] &= ~(0x03 << bitShift);

    // 2. Neuen Zustand an die richtige Position schieben und einfügen
    csc_led_Strip_buffer[col] |= (static_cast<uint8_t>(state) << bitShift);
    
    return;
}

const uint8_t* SurfaceControllerWing::getCSCLedStripBuffer() const
{
    return csc_led_Strip_buffer;
}

//###################################################################################
//
//  ########  ########   #######  ########  #######   ######   #######  ##       
//  ##     ## ##     ## ##     ##    ##    ##     ## ##    ## ##     ## ##       
//  ##     ## ##     ## ##     ##    ##    ##     ## ##       ##     ## ##       
//  ########  ########  ##     ##    ##    ##     ## ##       ##     ## ##       
//  ##        ##   ##   ##     ##    ##    ##     ## ##       ##     ## ##       
//  ##        ##    ##  ##     ##    ##    ##     ## ##    ## ##     ## ##       
//  ##        ##     ##  #######     ##     #######   ######   #######  ######## 
//
//###################################################################################


void SurfaceControllerWing::SendHeartbeat()
{
    SendWingFrame('H', nullptr, 0);
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

void SurfaceControllerWing::setCSCBrightnessRaw()
{
    // <ButtonBacklight> <ButtonLeds> <Meters> <ColorLeds> <Scribble> <ContrastScribble> <User LCD> <PatchLeds> 0x02

    uint8_t payload[9];
    payload[0] = 100; // % ButtonBacklight
    payload[1] = 100; // % ButtonLeds
    payload[2] = 100; // % Meters
    payload[3] = 80; // % ColorLeds
    payload[4] = 40; // % Scribble
    payload[5] = 40; // % ContrastScribble
    payload[6] = 60; // % User LCD
    payload[7] = 0; // % PatchLeds
    payload[8] = 0x02; // unknown
    SendWingFrame('B', payload, 9);
}

void SurfaceControllerWing::SetLed(SurfaceElementId buttonOrLed, bool ledOn, bool blink)
{
    LedState state = LedState::AUS;

    if (ledOn)
    {
        state = LedState::LED_AN;
    }
    else if (blink)
    {
        state = LedState::BLINKEN;
    }
    else
    {
        state = LedState::AUS;
    }

    if (csc_led_map.count(buttonOrLed))
    {
        setCSCLedRaw(csc_led_map.at(buttonOrLed), state);
        SendWingFrame('L', getCSCLedBuffer(), 10);
    }
    
    if (csc_led_strip_map.count(buttonOrLed))
    {
        setCSCLedStripRaw(csc_led_strip_map.at(buttonOrLed), state);
        SendWingFrame('l', getCSCLedStripBuffer(), 13);
    }
}

