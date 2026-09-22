#pragma once

#include "base.h"
#include "defines.h"

#include "adda.h"



using namespace WString;

namespace OMC
{

class Card : X32Base
{
    using enum MP_ID;

    private:
        Adda* adda;

        void XUSB_XLIVE_SetConfig(uint8_t channelparamter, uint source);
        void GetMetadata(uint card);

    public:
        Card(X32BaseParameter* basepar, Adda* _adda);

        uint currentSongNumberChannels; // number of channels (e.g. 16 or 32)
        uint currentSongTotalSeconds;
        uint currentSongPositionSeconds;

        bool XLIVE_Playing = false;
        bool XLIVE_Recording = false;
        uint XLIVE_CardTotalSpaceMB[2] = {0, 0};
        uint XLIVE_CardUsedSpaceMB[2] = {0, 0};

        void Init();
        void Sync();
        void Tick100ms();
        String SendCommand(String command);
        bool ProcessReturnCode(String returnCode);
        void ProcessCommand(String command);
        void FlushRxBuffer();

        // X-LIVE specific functions
        bool XLIVE_Stop();
        bool XLIVE_PlayPause();
        bool XLIVE_Seek(uint sampleIndex);
        String XLIVE_RequestToc(uint* numberOfEntries);
        void XLIVE_ReadRemainingCardSpace(uint card);
        void XLIVE_ReadTotalCardSpaceMB(uint card);
        bool XLIVE_SelectInterface(uint option, uint interface);
        bool XLIVE_DeleteSession(String session);
        bool XLIVE_RecordNewSession();
        bool XLIVE_FormatCard();
        bool XLIVE_SelectCard(uint card);
        bool XLIVE_SelectSession(String session);
        String XLIVE_SecondsToSampleIndex(uint seconds);
        uint XLIVE_SampleIndexToSeconds(String sampleIndexHex);
        
        String XLIVE_SessionNameToString(String timecodeHex);
        String XLIVE_DateTimeToSessionName(uint8_t day, uint8_t month, uint16_t year, uint8_t hour, uint8_t minute, uint8_t second);
        String XLIVE_CardUsedSpaceToString(uint card);
        String XLIVE_GetCardNominalSizeString(uint card);
   };

}