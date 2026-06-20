#include "surface-controller-xm32.h"

namespace OMC
{

SurfaceControllerXM32::SurfaceControllerXM32(X32BaseParameter* basepar) : SurfaceController(basepar)
{
    uart = new Uart(basepar);

    if (state->bodyless)
    {
        /* 
        
        How to connect x32ctrl bodyless mode to a X32 runnig Linux:

        Developer PC
        ############

        // create two virtual serial ports and connect them together as bridge
        # socat -d -d pty,raw,link=/tmp/ttyLocal,echo=0 pty,raw,link=/tmp/ttyRemote,echo=0

        // start netcat server on port 10000
        # nc -l 10000 </tmp/ttyRemote >/tmp/ttyRemote

        X32
        ###
        
        // set serial to 115200 baud
        # stty -F /dev/ttymxc1 115200 raw -echo -echoe -echok

        // start netcat client to transmit/receive serial from/to devloper pc
        # nc <ip of Developer PC> 10000 </dev/ttymxc1 >/dev/ttymxc1

        Developer PC
        ############        
        
        // start x32ctrl with bodyless commandline parameter "-b"
        # x32ctrl -b
        
        */

        uart->Open("/tmp/ttyLocal", 115200, true);
    }
    else if (state->raspi)
    {
        uart->Open("/dev/ttyUSB0", 115200, true);
    }
    else
    {
        uart->Open("/dev/ttymxc1", 115200, true);
    }
}

void SurfaceControllerXM32::ProcessUartData()
{
    uint8_t receivedClass = 0;
    uint8_t receivedIndex = 0;
    uint16_t receivedValue = 0;
    bool lastPackageIncomplete = false;

    int bytesToProcess = uart->Rx(&surfaceBufferUart[0], sizeof(surfaceBufferUart));

    if (bytesToProcess <= 0) {
        return;
    }

    // first init package buffer with 0x00s
    for (uint8_t package=0; package<SURFACE_MAX_PACKET_LENGTH;package++){
        // start at surfacePacketCurrentIndex to not overwrite saved data from last incomplete package
        for (int i = surfacePacketCurrentIndex; i < 6; i++) {
            surfacePacketBuffer[package][i]=0x00;
        }
        surfacePacketCurrentIndex=0;
    }

    if (helper->DEBUG_SURFACE(DEBUGLEVEL_TRACE)) {
        printf("DEBUG_SURFACE: ");

        // print received values on one row
        bool divide_after_next_dbg = false;
        for (int i = 0; i < bytesToProcess; i++) {
            if (divide_after_next_dbg && ((uint8_t)surfaceBufferUart[i] == 0xFE)) {
                printf("| ");
                divide_after_next_dbg = false;
            }
            printf("%02X ", (uint8_t)surfaceBufferUart[i]); // empfangene Bytes als HEX-Wert ausgeben
            if (divide_after_next_dbg){
                printf("| ");
                divide_after_next_dbg = false;
            } 
            if ((uint8_t)surfaceBufferUart[i] == 0xFE) {
                divide_after_next_dbg=true;
            }
        }
        printf("\n");
    }

    // break up received data into packages
    bool divide_after_next = false;
    for (int i = 0; i < bytesToProcess; i++) {

        if (divide_after_next && ((uint8_t)surfaceBufferUart[i] == 0xFE)) {
            // previous package had no checksum
            surfacePacketCurrent++;
            surfacePacketCurrentIndex=0;
            divide_after_next = false;
        }

        surfacePacketBuffer[surfacePacketCurrent][surfacePacketCurrentIndex++] = (uint8_t)surfaceBufferUart[i];

        if (divide_after_next) {
            surfacePacketCurrent++;
            surfacePacketCurrentIndex=0;
            divide_after_next = false;
        }

        // use 0xFE as package divider
        if (((uint8_t)surfaceBufferUart[i] == 0xFE))
        {
            divide_after_next = true;
        }
    }

    if (divide_after_next){
        // divide_after_next got no usage, because the uartBuffer was emptied out -> reason: no checksum was send
        // clean up this situation
        surfacePacketCurrent++;
        while (surfacePacketCurrentIndex < 6){  
            // fill with zero - maybe not needed
            surfacePacketBuffer[surfacePacketCurrent][surfacePacketCurrentIndex++]=0x00;
        }
        surfacePacketCurrentIndex=0;
    }

    if (
        (surfacePacketCurrentIndex!=0) &&
        !((surfacePacketBuffer[surfacePacketCurrent][3]==0xFE) | (surfacePacketBuffer[surfacePacketCurrent][4]==0xFE))
    ){
        // last package was incomplete, save it for next run
        /*
            Example1:                                  _____ incomplete, has no 0xFE (and is too short)
                                                    /  
            this run         66 01 FB 00 FE 12 | 66 02

            next run         46 02 FE 44 | 66 03 D6 02 FE 33 | 66 04 73 02 FE 15 | 66 05 4E 03 FE 38 | 66 06 21 02 FE 65 |
                            \
                            \____ take the bytes from the last incomplete package and glue it together


            Example2:                                        _____ incomplete, has no 0xFE
                                                            / 
            this run         66 05 EF 0E FE 0C | 66 06 52 0D

            next run         FE 29 | 66 07 C2 0C FE 39
                            \
                            \____ take the bytes from the last incomplete package and glue it together
            
        */

        helper->DEBUG_SURFACE(DEBUGLEVEL_TRACE, "surfacePacketCurrent=%d seems incomplete? surfacePacketCurrentIndex=%d", surfacePacketCurrent, surfacePacketCurrentIndex);
        lastPackageIncomplete = true;
    }


    if (helper->DEBUG_SURFACE(DEBUGLEVEL_TRACE)) {
        printf("DEBUG_SURFACE: ");
        
        // print packages, one in a row    
        uint8_t packagesToPrint = surfacePacketCurrent;
        if (lastPackageIncomplete){
            packagesToPrint++;
        }
        printf("surfacePacketCurrent=%d\n", surfacePacketCurrent);

        for (int package=0; package < packagesToPrint; package++) {
            printf("surfaceProcessUartData(): Package %d: ", package);
            for (uint8_t i = 0; i<6; i++){
                printf("%02X ", (uint8_t)surfacePacketBuffer[package][i]);
            }
            if (surfacePacketBuffer[package][0] == 0xFE){
                printf("  <--- Board %d", surfacePacketBuffer[package][1] & 0x7F);
            } else if (lastPackageIncomplete){
                printf("  <--- incomplete, saved for next run");
            }
            printf("\n");
        } 
    }   


    for (int8_t package=0; package < surfacePacketCurrent;package++){

        if (surfacePacketBuffer[package][0] == 0xFE){
            // received BoardId
            uint8_t receivedBoardIdtemp = surfacePacketBuffer[package][1] & 0x7F;
            switch(receivedBoardIdtemp){
                case 0:
                case 1:
                case 4:
                case 5:
                case 8:
                    receivedBoardId = receivedBoardIdtemp;
                    break;
            }
        } else
        {   
            receivedClass = surfacePacketBuffer[package][0];
            receivedIndex = surfacePacketBuffer[package][1];
            
            if ((uint8_t)(surfacePacketBuffer[package][3]) == 0xFE)
            {
                // short package - uint8_t !!

                receivedValue = (uint16_t)surfacePacketBuffer[package][2];
                
                // TODO: Check checksum
                //receivedChecksum = surfacePacketBuffer[package][4];
            }
            else if ((uint8_t)(surfacePacketBuffer[package][4]) == 0xFE)
            {
                // long package - uint16_t !!
                // for example: fader value

                receivedValue = ((uint16_t)surfacePacketBuffer[package][3] << 8) | (uint16_t)surfacePacketBuffer[package][2];
                
                // TODO: Check checksum
                //receivedChecksum = surfacePacketBuffer[package][5];
            }
        

            // only process valid packages
            bool valid = true;

            switch (receivedClass){
                case 'f':
                case 'b':
                case 'e':
                    break;
                default:
                    valid = false;
                    break;
            }       

            if (valid)
            {
                helper->DEBUG_SURFACE(DEBUGLEVEL_TRACE, "Callback: BoardId 0x%02X, Class 0x%02X, Index 0x%02X, Value 0x%04X", receivedBoardId, receivedClass, receivedIndex, receivedValue);
                surfaceCallback(callbackArg, (OMC_BOARD)receivedBoardId, receivedClass, receivedIndex, receivedValue);
            } 
        }
    }

    // all packages are processed
    // now clean up for next run

    if (lastPackageIncomplete){
        // copy last incomplete package to package0 for next run
        for (uint8_t i=0; i < surfacePacketCurrentIndex; i++){
            surfacePacketBuffer[0][i] = surfacePacketBuffer[surfacePacketCurrent][i];
        }

        // reset index for next run
        lastPackageIncomplete=false;
        surfacePacketCurrent=0;
        // do NOT touch surfacePacketCurrentIndex!
    }else {
        // reset index for next run
        surfacePacketCurrent=0;
        surfacePacketCurrentIndex=0;
    }
}

void SurfaceControllerXM32::Tick100ms()
{
    Blink();    
}

void SurfaceControllerXM32::SendData(MessageBase* message, bool addChecksum)
{
    message->AddRawByte(0xFE); // Endbyte

    if (addChecksum) {
        char checksum = 0;
        if (message->current_length >= 2) { // at least start- and end-byte
            checksum = calculateChecksum(message->buffer, message->current_length);
        }

        // add checksum to message and send data via serial-port
        message->AddRawByte(checksum);
    }

    uart->Tx(message);
}

// incoming message has the form: 0xFE 0x8i Class Index Data[] 0xFE
// Checksum is calculated using the following equation:
// chksum = ( 0xFE - i - class - index - sumof(data[]) - sizeof(data[]) ) and 0x7F
uint8_t SurfaceControllerXM32::calculateChecksum(const char* data, uint16_t len) {
  // a single message can contain up to max. 64 chars
  int32_t sum = 0xFE;
  for (uint8_t i = 0; i < (len-1); i++) {
    sum -= data[i];
  }
  sum -= (len - 3); // remove 2-byte HEADER (0xFE 0x8i) and 1-byte end (0xFE)

  // write the calculated sum to the last element of the array
  return (sum & 0x7F);
}


void SurfaceControllerXM32::Reset() {
    FaderReset();
    FaderReset();
}

void SurfaceControllerXM32::Blink()
{
    if (blinkwait == 0)
    {
        blinkstate = !blinkstate;

        for(SurfaceElementId button : blinklist) {
            SetLed(button, blinkstate, false);
        }

        blinkwait = 5;
    }

    blinkwait--; 
}


void SurfaceControllerXM32::FaderReset() {
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
        SetFaderRaw(GetBoardId(faderindex), GetFaderId(faderindex), 0);
    }
}

void SurfaceControllerXM32::SetFader(uint8_t boardId, uint8_t index, uint16_t position) {
    uint8_t faderindex = GetChannelstripIndex(boardId, index);
    helper->DEBUG_SURFACE(DEBUGLEVEL_VERBOSE, "Want to move fader at index %d to %d", faderindex, position);
    faders[faderindex].position_wanted = position;
}

void SurfaceControllerXM32::FaderMoved(uint8_t boardId, uint8_t index, uint16_t value) {
    uint8_t faderindex = GetChannelstripIndex(boardId, index);
    helper->DEBUG_SURFACE(DEBUGLEVEL_VERBOSE, "Fader at index %d moved to %d", faderindex, value);
    faders[faderindex].position_wanted = value;
    faders[faderindex].position_real = value;
    faders[faderindex].wait = 10; // wait 100x 10ms
}

void SurfaceControllerXM32::Touchcontrol() {
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
            SetFaderRaw(GetBoardId(faderindex), GetFaderId(faderindex), faders[faderindex].position_wanted);
        }
    }
}

uint8_t SurfaceControllerXM32::GetChannelstripIndex(uint8_t boardId, uint8_t index) {
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

uint8_t SurfaceControllerXM32::GetBoardId(uint8_t faderindex) {
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

uint8_t SurfaceControllerXM32::GetFaderId(uint8_t faderindex) {
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

// position = 0x0000 ... 0x0FFF
void SurfaceControllerXM32::SetFaderRaw(uint8_t boardId, uint8_t index, uint16_t position) {
    SurfaceMessage message;
    message.AddDataByte(0x80 + boardId); // start message for specific boardId
    message.AddDataByte('F'); // class: F = Fader
    message.AddDataByte(index); // index
    message.AddDataByte((position & 0xFF)); // LSB
    message.AddDataByte((char)((position & 0x0F00) >> 8)); // MSB

    helper->DEBUG_SURFACE(DEBUGLEVEL_TRACE, "Set fader position on board %d at index %d to %d", boardId, index, position);

    SendData(&message, true);
}

void SurfaceControllerXM32::SetLedRaw(uint board, uint index, bool ledOn)
{
    SurfaceMessage message;
    message.AddDataByte(0x80 + board);
    message.AddDataByte('L');  // class: L = LED
    message.AddDataByte(0x80); // index - fixed at 0x80 for LEDs
    if (ledOn)
    {
        message.AddDataByte(index + 0x80); // turn LED on
    }
    else
    {
        message.AddDataByte(index); // turn LED off
    }
    SendData(&message, true);
}

void SurfaceControllerXM32::SetMeterLed(uint8_t boardId, uint8_t index, uint8_t leds)
{
    // boardId = 0, 1, 4, 5, 8
    // index = 0 ... 8
    // leds = 8-bit bitwise (bit 0=-60dB ... 4=-6dB, 5=Clip, 6=Gate, 7=Comp)

    // 0xFE, 0x8i, class, index, data[], 0xFE, chksum
    // 0x4C, index, leds.b[]
    SurfaceMessage message;
    message.AddDataByte(0x80 + boardId); // start message for specific boardId
    message.AddDataByte('M'); // class: M = Meter
    message.AddDataByte(index); // index
    message.AddDataByte(leds);
    SendData(&message, true);
}

void SurfaceControllerXM32::SetLcd(LcdData* p_data, uint p_textCount)
{
    SurfaceMessage message;
    message.AddDataByte(0x80 + p_data->boardId);
    message.AddDataByte('D'); // class: D = Display
    message.AddDataByte(p_data->lcdIndex); 
    message.AddDataByte((p_data->color) & 0x0F);
    message.AddDataByte(p_data->icon.icon);
    message.AddDataByte(p_data->icon.x);
    message.AddDataByte(p_data->icon.y);
    for (int i=0;i<p_textCount;i++){
        message.AddDataByte(p_data->texts[i].size + strlen(p_data->texts[i].text.c_str())); // size + textLength
        message.AddDataByte(p_data->texts[i].x);
        message.AddDataByte(p_data->texts[i].y);
        message.AddString(p_data->texts[i].text.c_str()); // this is ASCII, so we can omit byte-stuffing  
    }
    SendData(&message, true);
}

void SurfaceControllerXM32::SetLed(SurfaceElementId buttonOrLed, bool ledOn, bool blink)
{
    if(blink)
    {
        blinklist.insert(buttonOrLed);
    }
    else
    {
        if (!blinklist.empty())
        {
            set<SurfaceElementId>::iterator it = blinklist.find(buttonOrLed);
            if (it != blinklist.end())
            {
                blinklist.erase(it);
            }
        }
    }

    SurfaceElement *element = config->GetSurfaceElement(buttonOrLed);
    SetLedRaw((uint)element->GetBoard(), (uint)element->GetIndex(), ledOn);
}

}