#pragma once

#include <stdint.h>
#include <list>
#include <map>
#include <math.h>

#include "base.h"
#include "uart.h"
#include "defines.h"
#include "types.h"
#include "surface-lcd.h"
#include "surface-message.h"
#include "surface-fader.h"
#include "surface-controller-xm32.h"
#include "surface-controller-wing.h"
#include "helper.h"

using namespace std;

class SurfaceController;

class Surface : public X32Base
{
    private:

        SurfaceController* surfaceController;

        X32FaderBank* banks[(uint)OMCBankId::__ELEMENT_COUNTER_DO_NOT_MOVE];

        std::map<OMCBankTarget, OMCBankId> bankloaded;

        uint blinkwait = 0;
        bool blinkstate = false;
        set<SurfaceElementId> blinklist;

        uint8_t int2segment(int8_t p_value);

        uint16_t CalcEncoderRingLedDirect(uint8_t num_leds_to_light);
        uint16_t CalcEncoderRingLedIncrement(uint8_t pct);
        uint16_t CalcEncoderRingLedDecrement(uint8_t pct);
        uint16_t CalcEncoderRingLedPosition(uint8_t pct);
        uint16_t CalcEncoderRingLedDbfs(float dbfs, bool onlyPosition);
        uint16_t CalcEncoderRingLedBalance(uint8_t pct);
        uint16_t CalcEncoderRingLedWidth(uint8_t pct);



        void InitBanks();
        void InitBank_Channelstrip_WING(X32FaderBank* bank, uint offset);
        void InitBank_Channelstrip(X32FaderBank *bank, uint offset);
        void InitBank_Channelstrip_DCA(X32FaderBank* bank, uint offset);
        void InitBank_Flex(X32FaderBank* bank);
        void InitBank_DMX(X32FaderBank* bank, uint offset);

    public:
        
        Surface(X32BaseParameter* basepar);

        void SetCallback(SurfaceCallback callback, void* arg);

        void Init(SurfaceCallback callback, void* arg);
        void Reset();

        void ProcessUartDataSurface();
        
        void LoadBank(OMCBankTarget target, OMCBankId id);
        void LoadAssignBank(X32AssignBankId id);
        OMCBankId GetLoadedBankId(OMCBankTarget target);
        X32FaderBank* GetLoadedBank(OMCBankTarget target);
        X32FaderBank* GetBank(OMCBankId id);
        void SetChannelstripBinding(X32FaderBank *bank, uint i, uint chanIndex);
        void LoadDefaultSurfaceBinding();
        void LoadMainFaderSurfaceBinding_XM32();
        void ResetBank(OMCBankId id);

        void LoadX32CoreDefinitions();

        void SetBrightnessAllBoards(uint8_t brightness);

        void SetBrightness(uint8_t boardId, uint8_t brightness);
        void SetContrastAllBoards(uint8_t contrast);
        void SetContrast(uint8_t boardId, uint8_t contrast);
        void SetFader(uint8_t boardId, uint8_t index, uint16_t position);
        void SetX32RackDisplayRaw(uint8_t p_value2, uint8_t p_value1);
        void SetX32RackDisplay(uint8_t p_value);
        void SetLed(SurfaceElementId buttonOrLed, bool state, bool blink);
        void SetLed(SurfaceElementId buttonOrLed, bool ledOn);
        void SetMeterLed(uint8_t boardId, uint8_t index, uint8_t leds);
        void SetMeterLedMain_Rack(uint8_t preamp, uint32_t meterL, uint32_t meterR, uint32_t meterSolo);
        void SetMeterLedMain_Producer(uint8_t preamp, uint8_t dynamics, uint32_t meterL, uint32_t meterR, uint32_t meterSolo);
        void SetMeterLedMain_FullOrCompact(uint8_t preamp, uint8_t dynamics, uint32_t meterL, uint32_t meterR, uint32_t meterSolo);
        void SetEncoderRing(uint8_t boardId, uint8_t index, uint8_t ledMode, uint8_t ledPct, bool backlight);
        void SetLcd(LcdData* p_data, uint p_textCount);


        void FaderReset();
        void FaderMoved(uint8_t boardId, uint8_t index, uint16_t value);
        
        void BlockFader(uint8_t boardId, uint8_t faderIndex);
        bool IsFaderBlocked(uint8_t boardId, uint8_t faderIndex);
        void Touchcontrol();

        void Blink();
};