#pragma once

#ifdef BODYLESS_SDL2

#include <lvgl/lvgl.h>
#include <cstdint>
#include <vector>
#include <string>
#include "ctrl.h"

// Forward declaration of X32Ctrl
class X32Ctrl;
union SDL_Event;

class SimulatorGUI {
public:
    static void Init(X32Ctrl* ctrl_inst);
    static void Tick();

private:
    static void CreateNavWindow(int x, int y);
    static void CreateDisplayEncodersWindow(int x, int y);
    static void CreateArrowsWindow(int x, int y);
    static void CreateChannelStripWindow(int x, int y);
    static void CreateFadersWindows(int y_start);
    static void PrepareDisplay(lv_display_t* disp);

    // UI creation helpers
    static void BuildDisplayEncoder(lv_obj_t* parent, int encoder_idx);
    static void BuildSurfaceEncoder(lv_obj_t* parent, const char* label, int x, int y, int size, SurfaceElementId elementId);
    static void BuildFaderStrip(lv_obj_t* parent, int local_col_idx, uint8_t boardId, uint8_t faderIndex, const char* default_label);
    static lv_obj_t* BuildConsoleButton(lv_obj_t* parent, const char* label, int x, int y, int w, int h, SurfaceElementId elementId);

    // Event callbacks
    static void DisplayEncoderEventCB(lv_event_t* e);
    static void DisplayEncoderButtonEventCB(lv_event_t* e);
    static void FaderSliderEventCB(lv_event_t* e);
    static void ButtonEventCB(lv_event_t* e);
    static int SDLEventWatch(void* userdata, SDL_Event* sdl_event);
    static void RaiseAllWindows(uint32_t focused_window_id = 0);

    // Protocol helper
    static void TurnEncoder(uintptr_t encoder_idx, int amount);
    static void PressSurfaceButton(SurfaceElementId elementId, bool pressed);
    static void SendEmulatedEvent(uint8_t boardId, uint8_t classId, uint8_t index, uint16_t value);
    static void ParseEmulatedTxData();
    static void ProcessEmulatedOutput(uint8_t boardId, uint8_t classId, uint8_t index, const std::vector<uint8_t>& data);

    // GUI Widget structure
    struct VirtualFaderStrip {
        uint8_t boardId;
        uint8_t faderIndex;
        lv_obj_t* select_btn;
        lv_obj_t* solo_btn;
        lv_obj_t* mute_btn;
        lv_obj_t* slider;
        lv_obj_t* lcd_container;
        lv_obj_t* lcd_label;
    };

    struct VirtualButton {
        uint8_t boardId;
        uint8_t buttonCode;
        lv_obj_t* btn_obj;
    };

    struct VirtualEncoder {
        uint8_t boardId;
        uint8_t encoderIndex;
        uint8_t buttonBoardId;
        uint8_t buttonCode;
        lv_obj_t* arc_obj;
        lv_obj_t* button_obj;
        int lastValue;
        int dragRemainder;
        uint32_t windowId;
        bool hasButton;
        bool showFeedback;
    };

    static X32Ctrl* ctrl;

    static lv_display_t* nav_disp;
    static lv_display_t* display_encoders_disp;
    static lv_display_t* arrows_disp;
    static lv_display_t* channel_strip_disp;
    static lv_display_t* left_sel_disp;
    static lv_display_t* board_l_disp;
    static lv_display_t* board_m_disp;
    static lv_display_t* right_sel_disp;
    static lv_display_t* board_r_disp;
    static std::vector<lv_indev_t*> simulator_indevs;

    static std::vector<VirtualFaderStrip> virtual_faders;
    static std::vector<VirtualButton> virtual_buttons;
    static std::vector<VirtualEncoder> virtual_display_encoders;
};

#endif
