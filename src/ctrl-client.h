#pragma once

#include "artnet.h"

#include "surface.h"

#include "page.h"
#include "lcd-menu.h"
#include "osc-client.h"



// LVGL
static lv_display_t *display;
#ifdef TARGET_PC_SDL2
static lv_indev_t *mouse;
static lv_indev_t *mouse_wheel;
static lv_indev_t *keyboard;
#endif

using namespace std;

namespace OMC
{

class CtrlClient : public X32Base
{
    private:

        LcdMenu* lcdmenu;
        Surface* surface;
        OscClient* osc_client;

        String ipAddress;

        OMCBankId preSpillLoadedBank = OMCBankId::None;

        map<X32_PAGE, Page*> pages;
        X32_PAGE lastPage = X32_PAGE::HOME;

        sTouchControl touchcontrol;

        Artnet* artnet;

        // currently pressed button
        SurfaceElement* buttonPressed = 0;
        
        // second button pressed, while first button is also pressed
        SurfaceElement* secondbuttonPressed = 0;

        char time_str[50];
		time_t timestamp;
		struct tm datetime;

        static void OnSurfaceCallback(void* arg, OMC_BOARD board, char command, uint8_t index, uint16_t value);
        void ProcessSurface(OMC_BOARD board, char command, uint8_t index, uint16_t value);

        static void OnOscSendToServerCallbackSet(void* arg, MP_ID parameterId, WString::String strValue, float floatValue, uint index);
        static void OnOscSendToServerCallbackChange(void* arg, MP_ID parameterId, int amount, uint index);
        static void OnOscSendToServerCallbackToogle(void* arg, MP_ID parameterId, uint index);
        static void OnOscSendToServerCallbackReset(void* arg, MP_ID parameterId, uint index);

        const char * getenv_default(const char * name, const char * default_val);

    public:

        CtrlClient(X32BaseParameter* basepar);
        void Init();
        
        void Tick10ms();
        void Tick50ms();
        void Tick100ms();
        void Tick1000ms();

        void InitPagesAndGUI();

        bool ShowPrevPage();
        bool ShowNextPage();

        void syncGuiOrLcd();
        void syncSurface(bool fullSync);
        
        void SetLcdFromChannel(uint8_t p_boardId, uint8_t lcdIndex, uint8_t channelIndex);
        void SetLcdFromAssign(uint8_t p_boardId, uint8_t lcdIndex, SurfaceElementId element_id);
        void GetAssignLcdText(LcdData *data, SurfaceElementId encoder, SurfaceElementId upper_button, SurfaceElementId lower_button);

        void SetLcdFromArtnet(uint8_t p_boardId, uint8_t lcdIndex, uint8_t artnetIndex);

        void SetLcdDark(uint8_t p_boardId, uint8_t lcdIndex);
        void UpdateMeters();
        void setLedChannelIndicator_Rack();        
        void setLedChannelIndicator_Core();        
        uint8_t CalcPreampMeter_FullOrCompact(uint8_t channel);
        uint8_t surfaceCalcDynamicMeter(uint8_t channel);

        void SimulatorButton();
};

}