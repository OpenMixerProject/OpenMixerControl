#pragma once

#include "surface.h"
#include "page.h"
#include "lcd-menu.h"

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

        OMCBankId preSpillLoadedBank = OMCBankId::None;

        map<X32_PAGE, Page*> pages;
        X32_PAGE lastPage = X32_PAGE::HOME;

        sTouchControl touchcontrol;

        #if ENABLE_ARTNET
        Artnet* artnet;
        #endif

        // currently pressed button
        SurfaceElement* buttonPressed = 0;
        
        // second button pressed, while first button is also pressed
        SurfaceElement* secondbuttonPressed = 0;

        static void OnSurfaceCallback(void* arg, OMC_BOARD board, char command, uint8_t index, uint16_t value);
        void ProcessSurface(OMC_BOARD board, char command, uint8_t index, uint16_t value);

    public:

        CtrlClient(X32BaseParameter* basepar);
        void Init();
        void guiInit();
        void Tick10ms(void);
        void Tick50ms(void);
        void Tick100ms(void);

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

        void SimulatorButton();
};

}