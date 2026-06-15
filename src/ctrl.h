#pragma once

#include "mixer.h"
#include "surface.h"
#include "xremote.h"
#include "wsm.h"
#include "lcd-menu.h"
#if ENABLE_ARTNET
    #include "artnet.h"
#endif
#include "page.h"

using namespace std;

class X32Ctrl : public X32Base
{
    using enum MP_ID;

    private:
        
        bool intialized = false; // is the whole mixer ready?

        Mixer* mixer;
        Surface* surface;
        XRemote* xremote;
        WSM* wsm;
        LcdMenu* lcdmenu;
        #if ENABLE_ARTNET
        Artnet* artnet;
        #endif
        
        OMCBankId preSpillLoadedBank = OMCBankId::None;

        map<X32_PAGE, Page*> pages;
        X32_PAGE lastPage = X32_PAGE::HOME;

        sTouchControl touchcontrol;

        void my_handler(int s);

        // currently pressed button
        SurfaceElement* buttonPressed = 0;
        
        // second button pressed, while first button is also pressed
        SurfaceElement* secondbuttonPressed = 0;

        static void OnSurfaceCallback(void* arg, OMC_BOARD board, char command, uint8_t index, uint16_t value);
        void ProcessSurface(OMC_BOARD board, char command, uint8_t index, uint16_t value);

        void ProcessUartDataAdda();
        void ProcessUartDataAES50();

        uint autosavewait = 0;
        void AutoSave();

    public:

        X32Ctrl(X32BaseParameter* basepar);
        void Init();
        void writeConfigEntry(Mixerparameter *const &parameter, uint index);
        void Tick10ms(void);
        void Tick50ms(void);
        void Tick100ms(void);
        void UdpHandleCommunication_WSM(void);

        void InitPagesAndGUI();
        bool ShowPrevPage();
        bool ShowNextPage();

        void syncGuiOrLcd(void);
        void syncSurface(bool fullSync);
        
        void SetLcdFromChannel(uint8_t p_boardId, uint8_t lcdIndex, uint8_t channelIndex);
        void SetLcdFromAssign(uint8_t p_boardId, uint8_t lcdIndex, SurfaceElementId element_id);
        void GetAssignLcdText(LcdData *data, SurfaceElementId encoder, SurfaceElementId upper_button, SurfaceElementId lower_button);
#if ENABLE_ARTNET
        void SetLcdFromArtnet(uint8_t p_boardId, uint8_t lcdIndex, uint8_t artnetIndex);
        #endif
        void SetLcdDark(uint8_t p_boardId, uint8_t lcdIndex);
        void UpdateMeters(void);
        void setLedChannelIndicator_Rack(void);        
        void setLedChannelIndicator_Core(void);        
        uint8_t CalcPreampMeter_FullOrCompact(uint8_t channel);
        uint8_t surfaceCalcDynamicMeter(uint8_t channel);
        void syncXRemote(bool syncAll);

        void SimulatorButton(uint key);
}; 