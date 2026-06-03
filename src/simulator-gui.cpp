#include "simulator-gui.h"

#ifdef BODYLESS_SDL2
#include "enum.h"
#include <SDL2/SDL.h>
#include <algorithm>
#include <string>
#include <utility>

X32Ctrl* SimulatorGUI::ctrl = nullptr;
lv_display_t* SimulatorGUI::nav_disp = nullptr;
lv_display_t* SimulatorGUI::display_encoders_disp = nullptr;
lv_display_t* SimulatorGUI::arrows_disp = nullptr;
lv_display_t* SimulatorGUI::channel_strip_disp = nullptr;
lv_display_t* SimulatorGUI::left_sel_disp = nullptr;
lv_display_t* SimulatorGUI::board_l_disp = nullptr;
lv_display_t* SimulatorGUI::board_m_disp = nullptr;
lv_display_t* SimulatorGUI::right_sel_disp = nullptr;
lv_display_t* SimulatorGUI::board_r_disp = nullptr;
std::vector<lv_indev_t*> SimulatorGUI::simulator_indevs;
std::vector<SimulatorGUI::VirtualFaderStrip> SimulatorGUI::virtual_faders;
std::vector<SimulatorGUI::VirtualButton> SimulatorGUI::virtual_buttons;
std::vector<SimulatorGUI::VirtualEncoder> SimulatorGUI::virtual_display_encoders;

namespace {
constexpr int kMainWindowX = 100;
constexpr int kMainWindowY = 100;
constexpr int kMainWindowW = DISPLAY_RESOLUTION_X;
constexpr int kMainWindowH = DISPLAY_RESOLUTION_Y;
constexpr int kWindowGap = 10;
constexpr int kDisplayEncoderWindowW = DISPLAY_RESOLUTION_X;
constexpr int kDisplayEncoderWindowH = 130;
constexpr int kChannelStripWindowW = 1180;
constexpr int kChannelStripWindowH = 390;
constexpr int kFaderStripWidth = 96;
constexpr int kFaderWindowHeight = 390;
constexpr int kSelectorWindowWidth = 112;
constexpr int kSelectorButtonHeight = 42;
constexpr int kSelectorButtonGap = 8;
constexpr int kEncoderDragPixelsPerStep = 4;
constexpr int kEncoderWheelStepsPerNotch = 2;
bool sdl_event_watch_added = false;

uint32_t CurrentDisplayWindowId() {
    lv_display_t* disp = lv_display_get_default();
    if (disp == nullptr) return 0;

    struct SDL_Window* win = lv_sdl_window_get_window(disp);
    if (win == nullptr) return 0;

    return SDL_GetWindowID(win);
}

lv_obj_t* BuildEncoderHitbox(lv_obj_t* parent, int x, int y, int size) {
    lv_obj_t* hitbox = lv_obj_create(parent);
    lv_obj_set_pos(hitbox, x, y);
    lv_obj_set_size(hitbox, size, size);
    lv_obj_set_style_bg_opa(hitbox, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(hitbox, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(hitbox, 0, LV_PART_MAIN);
    lv_obj_add_flag(hitbox, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(hitbox, LV_OBJ_FLAG_SCROLLABLE);
    return hitbox;
}

lv_obj_t* BuildPanel(lv_obj_t* parent, const char* title, int x, int y, int w, int h) {
    lv_obj_t* panel = lv_obj_create(parent);
    lv_obj_set_pos(panel, x, y);
    lv_obj_set_size(panel, w, h);
    lv_obj_set_style_pad_all(panel, 4, LV_PART_MAIN);
    lv_obj_set_style_radius(panel, 12, LV_PART_MAIN);
    lv_obj_set_style_border_width(panel, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(panel, lv_color_hex(0x5E6969), LV_PART_MAIN);
    lv_obj_set_style_bg_color(panel, lv_color_hex(0x485454), LV_PART_MAIN);
    lv_obj_remove_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title_lbl = lv_label_create(panel);
    lv_label_set_text(title_lbl, title);
    lv_obj_set_style_text_color(title_lbl, lv_color_hex(0xF0F0F0), LV_PART_MAIN);
    lv_obj_set_style_text_font(title_lbl, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_set_style_text_letter_space(title_lbl, 1, LV_PART_MAIN);
    lv_obj_align(title_lbl, LV_ALIGN_TOP_MID, 0, 2);
    return panel;
}

void BuildSmallLabel(lv_obj_t* parent, const char* text, int x, int y, int w = 70) {
    lv_obj_t* label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_pos(label, x, y);
    lv_obj_set_width(label, w);
    lv_obj_set_style_text_color(label, lv_color_hex(0xF0F0F0), LV_PART_MAIN);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_10, LV_PART_MAIN);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
}
}

void SimulatorGUI::Init(X32Ctrl* ctrl_inst) {
    ctrl = ctrl_inst;
    virtual_faders.clear();
    virtual_buttons.clear();
    virtual_display_encoders.clear();
    simulator_indevs.clear();
    if (!sdl_event_watch_added) {
        SDL_AddEventWatch(SDLEventWatch, nullptr);
        sdl_event_watch_added = true;
    }

    // Find the main display and position it
    lv_display_t* main_disp = lv_display_get_next(nullptr);
    if (main_disp) {
        lv_sdl_window_set_title(main_disp, "OpenX32 - omc - Main Screen");
        struct SDL_Window* main_win = lv_sdl_window_get_window(main_disp);
        if (main_win) {
            SDL_SetWindowPosition(main_win, kMainWindowX, kMainWindowY);
        }
        PrepareDisplay(main_disp);
    }

    // Create and position split windows
    CreateNavWindow(kMainWindowX + kMainWindowW + kWindowGap, kMainWindowY);
    CreateDisplayEncodersWindow(kMainWindowX, kMainWindowY + kMainWindowH + kWindowGap);
    CreateArrowsWindow(kMainWindowX + (kMainWindowW - 150) / 2, kMainWindowY + kMainWindowH + kWindowGap + kDisplayEncoderWindowH + kWindowGap);
    int channel_strip_y = kMainWindowY + kMainWindowH + kWindowGap + kDisplayEncoderWindowH + kWindowGap + 130 + kWindowGap;
    CreateChannelStripWindow(kMainWindowX, channel_strip_y);
    CreateFadersWindows(channel_strip_y + kChannelStripWindowH + kWindowGap);
}

void SimulatorGUI::PrepareDisplay(lv_display_t* disp) {
    lv_display_set_default(disp);
    lv_indev_t* mouse = lv_sdl_mouse_create();
    lv_indev_set_display(mouse, disp);
    simulator_indevs.push_back(mouse);
    lv_indev_t* mousewheel = lv_sdl_mousewheel_create();
    lv_indev_set_display(mousewheel, disp);
    simulator_indevs.push_back(mousewheel);
}

void SimulatorGUI::RaiseAllWindows(uint32_t focused_window_id) {
    static bool raising = false;
    if (raising) return;

    raising = true;
    std::vector<SDL_Window*> windows;
    for (lv_display_t* disp = lv_display_get_next(nullptr); disp != nullptr; disp = lv_display_get_next(disp)) {
        SDL_Window* win = lv_sdl_window_get_window(disp);
        if (win != nullptr) {
            windows.push_back(win);
        }
    }

    for (SDL_Window* win : windows) {
        if (SDL_GetWindowID(win) != focused_window_id) {
            SDL_RaiseWindow(win);
        }
    }
    for (SDL_Window* win : windows) {
        if (SDL_GetWindowID(win) == focused_window_id) {
            SDL_RaiseWindow(win);
            break;
        }
    }
    raising = false;
}

void SimulatorGUI::CreateNavWindow(int x, int y) {
    nav_disp = lv_sdl_window_create(110, 480);
    lv_sdl_window_set_title(nav_disp, "OpenX32 - omc - Navigation Buttons");
    struct SDL_Window* win = lv_sdl_window_get_window(nav_disp);
    if (win) {
        SDL_SetWindowPosition(win, x, y);
    }
    PrepareDisplay(nav_disp);

    lv_obj_t* scr = lv_obj_create(nullptr);
    lv_scr_load(scr);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x1B1B1B), LV_PART_MAIN);

    BuildConsoleButton(scr, "HOME", 10, 10, 90, 45, SurfaceElementId::HOME);
    BuildConsoleButton(scr, "METERS", 10, 65, 90, 45, SurfaceElementId::METERS);
    BuildConsoleButton(scr, "ROUTING", 10, 120, 90, 45, SurfaceElementId::ROUTING);
    BuildConsoleButton(scr, "SETUP", 10, 175, 90, 45, SurfaceElementId::SETUP);
    BuildConsoleButton(scr, "LIBRARY", 10, 230, 90, 45, SurfaceElementId::LIBRARY);
    BuildConsoleButton(scr, "EFFECTS", 10, 285, 90, 45, SurfaceElementId::EFFECTS);
    BuildConsoleButton(scr, "MUTE GRP", 10, 340, 90, 45, SurfaceElementId::MUTE_GRP);
    BuildConsoleButton(scr, "UTILITY", 10, 395, 90, 45, SurfaceElementId::UTILITY);
}

void SimulatorGUI::CreateDisplayEncodersWindow(int x, int y) {
    display_encoders_disp = lv_sdl_window_create(kDisplayEncoderWindowW, kDisplayEncoderWindowH);
    lv_sdl_window_set_title(display_encoders_disp, "OpenX32 - omc - Display Encoders");
    struct SDL_Window* win = lv_sdl_window_get_window(display_encoders_disp);
    if (win) {
        SDL_SetWindowPosition(win, x, y);
    }
    PrepareDisplay(display_encoders_disp);

    lv_obj_t* scr = lv_obj_create(nullptr);
    lv_scr_load(scr);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x1B1B1B), LV_PART_MAIN);

    for (int i = 0; i < MAX_DISPLAY_ENCODER; i++) {
        BuildDisplayEncoder(scr, i);
    }
}

void SimulatorGUI::CreateArrowsWindow(int x, int y) {
    arrows_disp = lv_sdl_window_create(150, 130);
    lv_sdl_window_set_title(arrows_disp, "OpenX32 - omc - Direction Buttons");
    struct SDL_Window* win = lv_sdl_window_get_window(arrows_disp);
    if (win) {
        SDL_SetWindowPosition(win, x, y);
    }
    PrepareDisplay(arrows_disp);

    lv_obj_t* scr = lv_obj_create(nullptr);
    lv_scr_load(scr);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x1B1B1B), LV_PART_MAIN);

    BuildConsoleButton(scr, "UP", 55, 10, 40, 35, SurfaceElementId::UP);
    BuildConsoleButton(scr, "LEFT", 10, 50, 45, 35, SurfaceElementId::LEFT);
    BuildConsoleButton(scr, "RIGHT", 95, 50, 45, 35, SurfaceElementId::RIGHT);
    BuildConsoleButton(scr, "DOWN", 55, 90, 40, 35, SurfaceElementId::DOWN);
}

void SimulatorGUI::CreateChannelStripWindow(int x, int y) {
    channel_strip_disp = lv_sdl_window_create(kChannelStripWindowW, kChannelStripWindowH);
    lv_sdl_window_set_title(channel_strip_disp, "OpenX32 - omc - Channel Controls");
    struct SDL_Window* win = lv_sdl_window_get_window(channel_strip_disp);
    if (win) {
        SDL_SetWindowPosition(win, x, y);
    }
    PrepareDisplay(channel_strip_disp);

    lv_obj_t* scr = lv_obj_create(nullptr);
    lv_scr_load(scr);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x303838), LV_PART_MAIN);

    lv_obj_t* preamp = BuildPanel(scr, "CONFIG / PREAMP", 0, 0, 370, 190);
    BuildSurfaceEncoder(preamp, "GAIN", 36, 35, 70, SurfaceElementId::GAIN_ENCODER);
    BuildSurfaceEncoder(preamp, "FREQUENCY", 242, 35, 70, SurfaceElementId::LOW_CUT_FREQ_ENCODER);
    BuildConsoleButton(preamp, "48V", 30, 136, 44, 38, SurfaceElementId::PHANTOM_48V);
    BuildConsoleButton(preamp, "Ø", 102, 136, 44, 38, SurfaceElementId::PHASE_INVERT);
    BuildConsoleButton(preamp, "LOW CUT", 256, 136, 46, 38, SurfaceElementId::LOW_CUT);
    BuildConsoleButton(preamp, "VIEW", 330, 142, 30, 26, SurfaceElementId::VIEW_CONFIG);
    BuildSmallLabel(preamp, "SIG", 170, 124, 40);

    lv_obj_t* gate = BuildPanel(scr, "GATE", 0, 198, 180, 190);
    BuildSurfaceEncoder(gate, "THRESHOLD", 50, 35, 70, SurfaceElementId::GATE_THRESHOLD_ENCODER);
    BuildConsoleButton(gate, "GATE", 32, 136, 46, 38, SurfaceElementId::GATE);
    BuildConsoleButton(gate, "VIEW", 140, 142, 30, 26, SurfaceElementId::VIEW_GATE);

    lv_obj_t* dynamics = BuildPanel(scr, "DYNAMICS", 188, 198, 180, 190);
    BuildSurfaceEncoder(dynamics, "THRESHOLD", 50, 35, 70, SurfaceElementId::DYNAMICS_THRESHOLD_ENCODER);
    BuildConsoleButton(dynamics, "COMP / EXP", 64, 136, 58, 38, SurfaceElementId::COMP_EXP);
    BuildConsoleButton(dynamics, "VIEW", 140, 142, 30, 26, SurfaceElementId::VIEW_DYNAMICS);

    lv_obj_t* eq = BuildPanel(scr, "EQUALIZER", 378, 0, 418, 388);
    BuildSurfaceEncoder(eq, "Q", 136, 32, 82, SurfaceElementId::EQ_Q_ENCODER);
    BuildSurfaceEncoder(eq, "FREQ", 210, 150, 70, SurfaceElementId::EQ_FREQ_ENCODER);
    BuildSurfaceEncoder(eq, "GAIN", 88, 218, 70, SurfaceElementId::EQ_GAIN_ENCODER);
    BuildConsoleButton(eq, "MODE", 76, 140, 42, 34, SurfaceElementId::EQ_MODE);
    BuildConsoleButton(eq, "EQUALIZER", 106, 336, 48, 38, SurfaceElementId::EQ);
    BuildConsoleButton(eq, "HIGH", 334, 88, 42, 34, SurfaceElementId::EQ_HIGH);
    BuildConsoleButton(eq, "HIGH\nMID", 334, 154, 42, 34, SurfaceElementId::EQ_HIGH_MID);
    BuildConsoleButton(eq, "LOW\nMID", 334, 220, 42, 34, SurfaceElementId::EQ_LOW_MID);
    BuildConsoleButton(eq, "LOW", 334, 286, 42, 34, SurfaceElementId::EQ_LOW);
    BuildConsoleButton(eq, "VIEW", 378, 354, 30, 26, SurfaceElementId::VIEW_EQ);
    BuildSmallLabel(eq, "HCUT\nHSHV\n\nVEQ\nPEQ\n\nLSHV\nLCUT", 0, 90, 54);

    lv_obj_t* sends = BuildPanel(scr, "BUS SENDS", 802, 0, 206, 388);
    BuildSurfaceEncoder(sends, "1", 28, 32, 70, SurfaceElementId::BUS_SEND_ENCODER_1);
    BuildSurfaceEncoder(sends, "2", 28, 124, 70, SurfaceElementId::BUS_SEND_ENCODER_2);
    BuildSurfaceEncoder(sends, "3", 28, 216, 70, SurfaceElementId::BUS_SEND_ENCODER_3);
    BuildSurfaceEncoder(sends, "4", 28, 308, 70, SurfaceElementId::BUS_SEND_ENCODER_4);
    BuildConsoleButton(sends, "1-4", 126, 88, 44, 34, SurfaceElementId::BUS_SEND_1_4);
    BuildConsoleButton(sends, "5-8", 126, 154, 44, 34, SurfaceElementId::BUS_SEND_5_8);
    BuildConsoleButton(sends, "9-12", 126, 220, 44, 34, SurfaceElementId::BUS_SEND_9_12);
    BuildConsoleButton(sends, "13-16", 126, 286, 44, 34, SurfaceElementId::BUS_SEND_13_16);
    BuildConsoleButton(sends, "VIEW", 168, 354, 30, 26, SurfaceElementId::VIEW_MIX_BUS_SENDS);

    lv_obj_t* main_bus = BuildPanel(scr, "MAIN BUS", 1016, 0, 162, 388);
    BuildSurfaceEncoder(main_bus, "LEVEL", 46, 32, 70, SurfaceElementId::MAIN_BUS_LEVEL_ENCODER);
    BuildConsoleButton(main_bus, "MONO BUS", 58, 136, 44, 38, SurfaceElementId::MONO_BUS);
    BuildSurfaceEncoder(main_bus, "PAN / BAL", 46, 214, 70, SurfaceElementId::PAN_BAL_ENCODER);
    BuildConsoleButton(main_bus, "STEREO BUS", 58, 326, 44, 38, SurfaceElementId::MAIN_LR_BUS);
    BuildConsoleButton(main_bus, "VIEW", 124, 354, 30, 26, SurfaceElementId::VIEW_MAIN);
}

void SimulatorGUI::CreateFadersWindows(int y_start) {
    int x_offset = kMainWindowX;
    const int fader_window_width = kFaderStripWidth * 8;

    // --- 1. Left Bank Selector Window ---
    left_sel_disp = lv_sdl_window_create(kSelectorWindowWidth, kFaderWindowHeight);
    lv_sdl_window_set_title(left_sel_disp, "OpenX32 - omc - Left Bank Selector");
    struct SDL_Window* win_lsel = lv_sdl_window_get_window(left_sel_disp);
    if (win_lsel) {
        SDL_SetWindowPosition(win_lsel, x_offset, y_start);
    }
    PrepareDisplay(left_sel_disp);

    lv_obj_t* scr_lsel = lv_obj_create(nullptr);
    lv_scr_load(scr_lsel);
    lv_obj_set_style_bg_color(scr_lsel, lv_color_hex(0x1B1B1B), LV_PART_MAIN);
    lv_obj_add_flag(scr_lsel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(scr_lsel, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(scr_lsel, LV_SCROLLBAR_MODE_AUTO);

    if (ctrl->config->IsModelX32FullOrM32()) {
        BuildConsoleButton(scr_lsel, "CH 1-16", 10, 34, 92, 50, SurfaceElementId::CH1_16);
        BuildConsoleButton(scr_lsel, "CH 17-32", 10, 116, 92, 50, SurfaceElementId::CH17_32);
        BuildConsoleButton(scr_lsel, "AUX/USB", 10, 198, 92, 50, SurfaceElementId::AUX_USB_RX_RET);
        BuildConsoleButton(scr_lsel, "BUS MAST", 10, 280, 92, 50, SurfaceElementId::BUS_MASTER);
    } else {
        const std::pair<const char*, SurfaceElementId> buttons[] = {
            {"CH 1-8", SurfaceElementId::CH1_8},
            {"CH 9-16", SurfaceElementId::CH9_16},
            {"CH 17-24", SurfaceElementId::CH17_24},
            {"CH 25-32", SurfaceElementId::CH25_32},
            {"AUX/USB", SurfaceElementId::AUX_USB},
            {"FX RET", SurfaceElementId::FX_RET},
            {"BUS 1-8", SurfaceElementId::BUS1_8_MASTER},
            {"BUS 9-16", SurfaceElementId::BUS9_16_MASTER},
        };
        for (int i = 0; i < 8; i++) {
            BuildConsoleButton(scr_lsel, buttons[i].first, 10, 10 + i * (kSelectorButtonHeight + kSelectorButtonGap),
                92, kSelectorButtonHeight, buttons[i].second);
        }
    }

    x_offset += kSelectorWindowWidth + kWindowGap;

    // --- 2. Board L Faders Window (8 faders) ---
    board_l_disp = lv_sdl_window_create(fader_window_width, kFaderWindowHeight);
    lv_sdl_window_set_title(board_l_disp, "OpenX32 - omc - Input Faders");
    struct SDL_Window* win_bl = lv_sdl_window_get_window(board_l_disp);
    if (win_bl) {
        SDL_SetWindowPosition(win_bl, x_offset, y_start);
    }
    PrepareDisplay(board_l_disp);

    lv_obj_t* scr_bl = lv_obj_create(nullptr);
    lv_scr_load(scr_bl);
    lv_obj_set_style_bg_color(scr_bl, lv_color_hex(0x222222), LV_PART_MAIN);

    for (int i = 0; i < 8; i++) {
        char buf[16];
        snprintf(buf, sizeof(buf), "IN %d", i + 1);
        BuildFaderStrip(scr_bl, i, (uint8_t)X32_BOARD::X32_BOARD_L, i, buf);
    }

    x_offset += fader_window_width + kWindowGap;

    // --- 3. Board M Faders Window (8 faders, if full model) ---
    bool is_full = ctrl->config->IsModelX32FullOrM32();
    if (is_full) {
        board_m_disp = lv_sdl_window_create(fader_window_width, kFaderWindowHeight);
        lv_sdl_window_set_title(board_m_disp, "OpenX32 - omc - Mid Faders");
        struct SDL_Window* win_bm = lv_sdl_window_get_window(board_m_disp);
        if (win_bm) {
            SDL_SetWindowPosition(win_bm, x_offset, y_start);
        }
        PrepareDisplay(board_m_disp);

        lv_obj_t* scr_bm = lv_obj_create(nullptr);
        lv_scr_load(scr_bm);
        lv_obj_set_style_bg_color(scr_bm, lv_color_hex(0x222222), LV_PART_MAIN);

        for (int i = 0; i < 8; i++) {
            char buf[16];
            snprintf(buf, sizeof(buf), "MID %d", i + 1);
            BuildFaderStrip(scr_bm, i, (uint8_t)X32_BOARD::X32_BOARD_M, i, buf);
        }

        x_offset += fader_window_width + kWindowGap;
    }

    // --- 4. Right Bank Selector Window ---
    right_sel_disp = lv_sdl_window_create(kSelectorWindowWidth, kFaderWindowHeight);
    lv_sdl_window_set_title(right_sel_disp, "OpenX32 - omc - Right Bank Selector");
    struct SDL_Window* win_rsel = lv_sdl_window_get_window(right_sel_disp);
    if (win_rsel) {
        SDL_SetWindowPosition(win_rsel, x_offset, y_start);
    }
    PrepareDisplay(right_sel_disp);

    lv_obj_t* scr_rsel = lv_obj_create(nullptr);
    lv_scr_load(scr_rsel);
    lv_obj_set_style_bg_color(scr_rsel, lv_color_hex(0x1B1B1B), LV_PART_MAIN);
    lv_obj_add_flag(scr_rsel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(scr_rsel, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(scr_rsel, LV_SCROLLBAR_MODE_AUTO);

    const std::pair<const char*, SurfaceElementId> right_buttons_full[] = {
        {"SND ON FD", SurfaceElementId::SEND_ON_FADER},
        {"DCA", SurfaceElementId::DCA},
        {"BUS 1-8", SurfaceElementId::BUS1_8},
        {"BUS 9-16", SurfaceElementId::BUS9_16},
        {"MTX/MAIN", SurfaceElementId::MATRIX_MAIN},
        {"CLR SOLO", SurfaceElementId::CLEAR_SOLO},
    };
    const std::pair<const char*, SurfaceElementId> right_buttons_compact[] = {
        {"DAW REM", SurfaceElementId::DAW_REMOTE},
        {"SND ON FD", SurfaceElementId::SEND_ON_FADER},
        {"DCA", SurfaceElementId::DCA},
        {"BUS 1-8", SurfaceElementId::BUS1_8},
        {"BUS 9-16", SurfaceElementId::BUS9_16},
        {"MTX/MAIN", SurfaceElementId::MATRIX_MAIN},
        {"CLR SOLO", SurfaceElementId::CLEAR_SOLO},
    };
    const auto* right_buttons = is_full ? right_buttons_full : right_buttons_compact;
    const int right_button_count = is_full ? 6 : 7;
    for (int i = 0; i < right_button_count; i++) {
        BuildConsoleButton(scr_rsel, right_buttons[i].first, 10, 14 + i * (kSelectorButtonHeight + kSelectorButtonGap),
            92, kSelectorButtonHeight, right_buttons[i].second);
    }

    x_offset += kSelectorWindowWidth + kWindowGap;

    // --- 5. Board R Faders Window (8 faders + 1 main LR fader) ---
    board_r_disp = lv_sdl_window_create(kFaderStripWidth * 9, kFaderWindowHeight);
    lv_sdl_window_set_title(board_r_disp, "OpenX32 - omc - Bus Faders");
    struct SDL_Window* win_br = lv_sdl_window_get_window(board_r_disp);
    if (win_br) {
        SDL_SetWindowPosition(win_br, x_offset, y_start);
    }
    PrepareDisplay(board_r_disp);

    lv_obj_t* scr_br = lv_obj_create(nullptr);
    lv_scr_load(scr_br);
    lv_obj_set_style_bg_color(scr_br, lv_color_hex(0x222222), LV_PART_MAIN);

    for (int i = 0; i < 8; i++) {
        char buf[16];
        snprintf(buf, sizeof(buf), "BUS %d", i + 1);
        BuildFaderStrip(scr_br, i, (uint8_t)X32_BOARD::X32_BOARD_R, i, buf);
    }
    BuildFaderStrip(scr_br, 8, (uint8_t)X32_BOARD::X32_BOARD_R, 8, "MAIN LR");
}

void SimulatorGUI::BuildDisplayEncoder(lv_obj_t* parent, int encoder_idx) {
    SurfaceElementId encoder_id = ctrl->config->CalcSurfaceElementId(SurfaceElementId::DISPLAY_ENCODER_1, encoder_idx);
    SurfaceElementId button_id = ctrl->config->CalcSurfaceElementId(SurfaceElementId::DISPLAY_ENCODER_BUTTON_1, encoder_idx);
    SurfaceElement* encoder = ctrl->config->GetSurfaceElement(encoder_id);
    SurfaceElement* button = ctrl->config->GetSurfaceElement(button_id);
    if (encoder == nullptr || button == nullptr || encoder->GetType() != SurfaceElementType::Encoder || button->GetType() != SurfaceElementType::Button) {
        return;
    }

    constexpr int cell_width = kDisplayEncoderWindowW / MAX_DISPLAY_ENCODER;
    int x = encoder_idx * cell_width;

    lv_obj_t* cell = lv_obj_create(parent);
    lv_obj_set_pos(cell, x + 4, 5);
    lv_obj_set_size(cell, cell_width - 8, kDisplayEncoderWindowH - 10);
    lv_obj_set_style_pad_all(cell, 2, LV_PART_MAIN);
    lv_obj_set_style_border_width(cell, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(cell, lv_color_hex(0x444444), LV_PART_MAIN);
    lv_obj_set_style_bg_color(cell, lv_color_hex(0x242424), LV_PART_MAIN);
    lv_obj_remove_flag(cell, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = lv_label_create(cell);
    lv_label_set_text_fmt(title, "ENC %d", encoder_idx + 1);
    lv_obj_set_style_text_color(title, lv_color_hex(0xCCCCCC), LV_PART_MAIN);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_10, LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 2);

    lv_obj_t* arc = lv_arc_create(cell);
    lv_obj_set_size(arc, 72, 72);
    lv_obj_align(arc, LV_ALIGN_TOP_MID, 0, 22);
    lv_arc_set_range(arc, 0, 127);
    lv_arc_set_value(arc, 64);
    lv_arc_set_rotation(arc, 135);
    lv_arc_set_bg_angles(arc, 0, 270);
    lv_obj_set_style_arc_width(arc, 7, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, 7, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(arc, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_set_style_arc_color(arc, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc, lv_color_hex(0x333333), LV_PART_INDICATOR);
    lv_obj_set_style_arc_opa(arc, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_arc_opa(arc, LV_OPA_TRANSP, LV_PART_INDICATOR);
    lv_obj_remove_flag(arc, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t* knob = lv_obj_create(cell);
    lv_obj_set_size(knob, 44, 44);
    lv_obj_align(knob, LV_ALIGN_TOP_MID, 0, 36);
    lv_obj_set_style_radius(knob, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(knob, lv_color_hex(0x2A2A2A), LV_PART_MAIN);
    lv_obj_set_style_border_width(knob, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(knob, lv_color_hex(0x111111), LV_PART_MAIN);
    lv_obj_remove_flag(knob, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(knob, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* push_btn = lv_button_create(cell);
    lv_obj_set_size(push_btn, 48, 24);
    lv_obj_align(push_btn, LV_ALIGN_BOTTOM_MID, 0, -4);
    lv_obj_set_style_radius(push_btn, 3, LV_PART_MAIN);
    lv_obj_set_style_bg_color(push_btn, lv_color_hex(0x353535), LV_PART_MAIN);
    lv_obj_set_style_bg_color(push_btn, lv_color_hex(0x00A0A0), LV_STATE_CHECKED);
    lv_obj_t* push_lbl = lv_label_create(push_btn);
    lv_label_set_text(push_lbl, "PUSH");
    lv_obj_set_style_text_font(push_lbl, &lv_font_montserrat_10, LV_PART_MAIN);
    lv_obj_center(push_lbl);

    uintptr_t encoder_vec_idx = virtual_display_encoders.size();
    VirtualEncoder vencoder = {
        (uint8_t)encoder->GetBoard(),
        (uint8_t)encoder->GetIndex(),
        (uint8_t)button->GetBoard(),
        (uint8_t)button->GetIndex(),
        arc,
        push_btn,
        64,
        0,
        CurrentDisplayWindowId(),
        true,
        false
    };
    virtual_display_encoders.push_back(vencoder);
    lv_obj_t* hitbox = BuildEncoderHitbox(cell, (cell_width - 8 - 78) / 2, 19, 78);
    lv_obj_add_event_cb(hitbox, DisplayEncoderEventCB, LV_EVENT_ALL, (void*)encoder_vec_idx);
    lv_obj_add_event_cb(push_btn, DisplayEncoderButtonEventCB, LV_EVENT_ALL, (void*)encoder_vec_idx);
}

void SimulatorGUI::BuildSurfaceEncoder(lv_obj_t* parent, const char* label, int x, int y, int size, SurfaceElementId elementId) {
    SurfaceElement* encoder = ctrl->config->GetSurfaceElement(elementId);
    if (encoder == nullptr || encoder->GetType() != SurfaceElementType::Encoder) {
        return;
    }

    lv_obj_t* arc = lv_arc_create(parent);
    lv_obj_set_pos(arc, x, y);
    lv_obj_set_size(arc, size, size);
    lv_arc_set_range(arc, 0, 127);
    lv_arc_set_value(arc, 64);
    lv_arc_set_rotation(arc, 135);
    lv_arc_set_bg_angles(arc, 0, 270);
    lv_obj_set_style_arc_width(arc, 7, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, 7, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc, lv_color_hex(0x222222), LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc, lv_color_hex(0xFF7A16), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(arc, lv_color_hex(0x222222), LV_PART_KNOB);
    lv_obj_set_style_radius(arc, LV_RADIUS_CIRCLE, LV_PART_KNOB);
    lv_obj_remove_flag(arc, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t* inner = lv_obj_create(parent);
    int inner_size = size / 2;
    lv_obj_set_pos(inner, x + (size - inner_size) / 2, y + (size - inner_size) / 2);
    lv_obj_set_size(inner, inner_size, inner_size);
    lv_obj_set_style_radius(inner, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(inner, lv_color_hex(0x2A2A2A), LV_PART_MAIN);
    lv_obj_set_style_border_width(inner, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(inner, lv_color_hex(0x111111), LV_PART_MAIN);
    lv_obj_remove_flag(inner, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(inner, LV_OBJ_FLAG_CLICKABLE);

    BuildSmallLabel(parent, label, x - 8, y + size + 2, size + 16);

    uintptr_t encoder_vec_idx = virtual_display_encoders.size();
    VirtualEncoder vencoder = {
        (uint8_t)encoder->GetBoard(),
        (uint8_t)encoder->GetIndex(),
        0,
        0,
        arc,
        nullptr,
        64,
        0,
        CurrentDisplayWindowId(),
        false,
        true
    };
    virtual_display_encoders.push_back(vencoder);
    lv_obj_t* hitbox = BuildEncoderHitbox(parent, x, y, size);
    lv_obj_add_event_cb(hitbox, DisplayEncoderEventCB, LV_EVENT_ALL, (void*)encoder_vec_idx);
}

void SimulatorGUI::BuildFaderStrip(lv_obj_t* parent, int local_col_idx, uint8_t boardId, uint8_t faderIndex, const char* default_label) {
    int strip_width = kFaderStripWidth;

    // Fader strip container
    lv_obj_t* strip_container = lv_obj_create(parent);
    lv_obj_set_pos(strip_container, local_col_idx * strip_width + 1, 5);
    lv_obj_set_size(strip_container, strip_width - 2, kFaderWindowHeight - 10);
    lv_obj_set_style_pad_all(strip_container, 2, LV_PART_MAIN);
    lv_obj_set_style_border_width(strip_container, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(strip_container, lv_color_hex(0x444444), LV_PART_MAIN);
    lv_obj_set_style_bg_color(strip_container, lv_color_hex(0x2D2D2D), LV_PART_MAIN);
    lv_obj_remove_flag(strip_container, LV_OBJ_FLAG_SCROLLABLE);

    // Calculate element indices
    uint8_t sel_code = (faderIndex == 8) ? 0x28 : (0x20 + faderIndex);
    uint8_t solo_code = (faderIndex == 8) ? 0x38 : (0x30 + faderIndex);
    uint8_t mute_code = (faderIndex == 8) ? 0x48 : (0x40 + faderIndex);

    // 1. Select Button
    lv_obj_t* sel_btn = lv_button_create(strip_container);
    lv_obj_set_pos(sel_btn, 2, 5);
    lv_obj_set_size(sel_btn, strip_width - 8, 32);
    lv_obj_set_style_radius(sel_btn, 2, LV_PART_MAIN);
    lv_obj_set_style_bg_color(sel_btn, lv_color_hex(0x3E3E3E), LV_PART_MAIN);
    lv_obj_set_style_bg_color(sel_btn, lv_color_hex(0x0078D7), LV_STATE_CHECKED); // blue glow
    lv_obj_t* sel_lbl = lv_label_create(sel_btn);
    lv_label_set_text(sel_lbl, "SEL");
    lv_obj_set_style_text_font(sel_lbl, &lv_font_montserrat_10, LV_PART_MAIN);
    lv_obj_center(sel_lbl);

    VirtualButton vbtn_sel = { boardId, sel_code, sel_btn };
    uintptr_t sel_idx = virtual_buttons.size();
    virtual_buttons.push_back(vbtn_sel);
    lv_obj_add_event_cb(sel_btn, ButtonEventCB, LV_EVENT_ALL, (void*)sel_idx);

    // 2. Solo Button
    lv_obj_t* solo_btn = lv_button_create(strip_container);
    lv_obj_set_pos(solo_btn, 2, 42);
    lv_obj_set_size(solo_btn, strip_width - 8, 32);
    lv_obj_set_style_radius(solo_btn, 2, LV_PART_MAIN);
    lv_obj_set_style_bg_color(solo_btn, lv_color_hex(0x3E3E3E), LV_PART_MAIN);
    lv_obj_set_style_bg_color(solo_btn, lv_color_hex(0xD19A0A), LV_STATE_CHECKED); // yellow glow
    lv_obj_t* solo_lbl = lv_label_create(solo_btn);
    lv_label_set_text(solo_lbl, "SOLO");
    lv_obj_set_style_text_font(solo_lbl, &lv_font_montserrat_10, LV_PART_MAIN);
    lv_obj_center(solo_lbl);

    VirtualButton vbtn_solo = { boardId, solo_code, solo_btn };
    uintptr_t solo_idx = virtual_buttons.size();
    virtual_buttons.push_back(vbtn_solo);
    lv_obj_add_event_cb(solo_btn, ButtonEventCB, LV_EVENT_ALL, (void*)solo_idx);

    // 3. Scribble LCD Container
    lv_obj_t* lcd_cont = lv_obj_create(strip_container);
    lv_obj_set_pos(lcd_cont, 2, 80);
    lv_obj_set_size(lcd_cont, strip_width - 8, 70);
    lv_obj_set_style_pad_all(lcd_cont, 1, LV_PART_MAIN);
    lv_obj_set_style_border_width(lcd_cont, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(lcd_cont, lv_color_hex(0x111111), LV_PART_MAIN);
    lv_obj_set_style_bg_color(lcd_cont, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_remove_flag(lcd_cont, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* lcd_lbl = lv_label_create(lcd_cont);
    lv_label_set_text(lcd_lbl, default_label);
    lv_label_set_long_mode(lcd_lbl, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(lcd_lbl, strip_width - 14);
    lv_obj_set_style_text_color(lcd_lbl, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(lcd_lbl, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_align(lcd_lbl, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_center(lcd_lbl);

    // 4. Mute Button
    lv_obj_t* mute_btn = lv_button_create(strip_container);
    lv_obj_set_pos(mute_btn, 2, 155);
    lv_obj_set_size(mute_btn, strip_width - 8, 32);
    lv_obj_set_style_radius(mute_btn, 2, LV_PART_MAIN);
    lv_obj_set_style_bg_color(mute_btn, lv_color_hex(0x3E3E3E), LV_PART_MAIN);
    lv_obj_set_style_bg_color(mute_btn, lv_color_hex(0xE06C75), LV_STATE_CHECKED); // red glow
    lv_obj_t* mute_lbl = lv_label_create(mute_btn);
    lv_label_set_text(mute_lbl, "MUTE");
    lv_obj_set_style_text_font(mute_lbl, &lv_font_montserrat_10, LV_PART_MAIN);
    lv_obj_center(mute_lbl);

    VirtualButton vbtn_mute = { boardId, mute_code, mute_btn };
    uintptr_t mute_idx = virtual_buttons.size();
    virtual_buttons.push_back(vbtn_mute);
    lv_obj_add_event_cb(mute_btn, ButtonEventCB, LV_EVENT_ALL, (void*)mute_idx);

    // 5. Slider
    lv_obj_t* slider = lv_slider_create(strip_container);
    lv_obj_set_pos(slider, (strip_width - 44) / 2, 195);
    lv_obj_set_size(slider, 44, 175);
    lv_slider_set_range(slider, 0, 4095);
    lv_obj_set_style_bg_color(slider, lv_color_hex(0x1B1B1B), LV_PART_MAIN);
    lv_obj_set_style_bg_color(slider, lv_color_hex(0x555555), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(slider, lv_color_hex(0xEEEEEE), LV_PART_KNOB);
    lv_obj_set_style_radius(slider, 0, LV_PART_KNOB);
    lv_obj_set_style_width(slider, 44, LV_PART_KNOB);
    lv_obj_set_style_height(slider, 20, LV_PART_KNOB);

    uintptr_t fader_idx = virtual_faders.size();
    VirtualFaderStrip vstrip = {
        boardId,
        faderIndex,
        sel_btn,
        solo_btn,
        mute_btn,
        slider,
        lcd_cont,
        lcd_lbl
    };
    virtual_faders.push_back(vstrip);
    lv_obj_add_event_cb(slider, FaderSliderEventCB, LV_EVENT_VALUE_CHANGED, (void*)fader_idx);
}

lv_obj_t* SimulatorGUI::BuildConsoleButton(lv_obj_t* parent, const char* label, int x, int y, int w, int h, SurfaceElementId elementId) {
    SurfaceElement* element = ctrl->config->GetSurfaceElement(elementId);
    if (element == nullptr || element->GetType() != SurfaceElementType::Button) {
        return nullptr;
    }

    lv_obj_t* btn = lv_button_create(parent);
    lv_obj_set_pos(btn, x, y);
    lv_obj_set_size(btn, w, h);
    lv_obj_set_style_radius(btn, 3, LV_PART_MAIN);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x353535), LV_PART_MAIN);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x00A0A0), LV_STATE_CHECKED); // greenish/cyan glow for active panel buttons

    lv_obj_t* lbl = lv_label_create(btn);
    lv_label_set_text(lbl, label);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_10, LV_PART_MAIN);
    lv_obj_center(lbl);

    VirtualButton vbtn = { (uint8_t)element->GetBoard(), (uint8_t)element->GetIndex(), btn };
    uintptr_t btn_idx = virtual_buttons.size();
    virtual_buttons.push_back(vbtn);
    lv_obj_add_event_cb(btn, ButtonEventCB, LV_EVENT_ALL, (void*)btn_idx);
    return btn;
}

void SimulatorGUI::DisplayEncoderEventCB(lv_event_t* e) {
    uintptr_t encoder_idx = (uintptr_t)lv_event_get_user_data(e);
    if (encoder_idx >= virtual_display_encoders.size()) return;

    VirtualEncoder& vencoder = virtual_display_encoders[encoder_idx];
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_PRESSED) {
        vencoder.dragRemainder = 0;
    } else if (code == LV_EVENT_PRESSING) {
        lv_indev_t* indev = lv_event_get_indev(e);
        if (indev == nullptr) return;

        lv_point_t vect;
        lv_indev_get_vect(indev, &vect);
        vencoder.dragRemainder += -vect.y;

        int amount = vencoder.dragRemainder / kEncoderDragPixelsPerStep;
        if (amount == 0) return;

        vencoder.dragRemainder -= amount * kEncoderDragPixelsPerStep;
        amount = std::max(-24, std::min(24, amount));
        TurnEncoder(encoder_idx, amount);
    } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST || code == LV_EVENT_CANCEL) {
        vencoder.dragRemainder = 0;
    }
}

void SimulatorGUI::TurnEncoder(uintptr_t encoder_idx, int amount) {
    if (encoder_idx >= virtual_display_encoders.size()) return;
    if (amount == 0) return;

    if (amount > 127) amount = 127;
    if (amount < -127) amount = -127;

    VirtualEncoder& vencoder = virtual_display_encoders[encoder_idx];
    if (vencoder.showFeedback) {
        vencoder.lastValue = std::max(0, std::min(127, vencoder.lastValue + amount));
        lv_arc_set_value(vencoder.arc_obj, vencoder.lastValue);
    }

    uint8_t encoded_amount = amount > 0 ? (uint8_t)amount : (uint8_t)(256 + amount);
    SendEmulatedEvent(vencoder.boardId, 'e', vencoder.encoderIndex, encoded_amount);
}

void SimulatorGUI::PressSurfaceButton(SurfaceElementId elementId, bool pressed) {
    if (ctrl == nullptr || ctrl->config == nullptr) return;

    SurfaceElement* element = ctrl->config->GetSurfaceElement(elementId);
    if (element == nullptr || element->GetType() != SurfaceElementType::Button) {
        return;
    }

    uint8_t board_id = (uint8_t)element->GetBoard();
    uint8_t button_code = (uint8_t)element->GetIndex();
    SendEmulatedEvent(board_id, 'b', 0, pressed ? button_code + 0x80 : button_code);

    for (auto& vbtn : virtual_buttons) {
        if (vbtn.boardId == board_id && vbtn.buttonCode == button_code) {
            if (pressed) {
                lv_obj_add_state(vbtn.btn_obj, LV_STATE_PRESSED);
            } else {
                lv_obj_clear_state(vbtn.btn_obj, LV_STATE_PRESSED);
            }
        }
    }
}

int SimulatorGUI::SDLEventWatch(void* userdata, SDL_Event* sdl_event) {
    (void)userdata;
    if (sdl_event == nullptr) {
        return 0;
    }

    if (sdl_event->type == SDL_WINDOWEVENT && sdl_event->window.event == SDL_WINDOWEVENT_FOCUS_GAINED) {
        RaiseAllWindows(sdl_event->window.windowID);
        return 0;
    }

    if (sdl_event->type == SDL_KEYDOWN || sdl_event->type == SDL_KEYUP) {
        if (sdl_event->type == SDL_KEYDOWN && sdl_event->key.repeat != 0) {
            return 0;
        }

        SurfaceElementId element_id = SurfaceElementId::NONE;
        switch (sdl_event->key.keysym.sym) {
            case SDLK_UP: element_id = SurfaceElementId::UP; break;
            case SDLK_DOWN: element_id = SurfaceElementId::DOWN; break;
            case SDLK_LEFT: element_id = SurfaceElementId::LEFT; break;
            case SDLK_RIGHT: element_id = SurfaceElementId::RIGHT; break;
            default: break;
        }

        if (element_id != SurfaceElementId::NONE) {
            PressSurfaceButton(element_id, sdl_event->type == SDL_KEYDOWN);
        }
        return 0;
    }

    if (sdl_event->type != SDL_MOUSEWHEEL) {
        return 0;
    }

    int mouse_x = 0;
    int mouse_y = 0;
    SDL_GetMouseState(&mouse_x, &mouse_y);

    for (uintptr_t i = 0; i < virtual_display_encoders.size(); i++) {
        VirtualEncoder& vencoder = virtual_display_encoders[i];
        if (vencoder.windowId != sdl_event->wheel.windowID || vencoder.arc_obj == nullptr) {
            continue;
        }

        lv_area_t coords;
        lv_obj_get_coords(vencoder.arc_obj, &coords);
        if (mouse_x >= coords.x1 && mouse_x <= coords.x2 && mouse_y >= coords.y1 && mouse_y <= coords.y2) {
            TurnEncoder(i, sdl_event->wheel.y * kEncoderWheelStepsPerNotch);
            break;
        }
    }

    return 0;
}

void SimulatorGUI::DisplayEncoderButtonEventCB(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    uintptr_t encoder_idx = (uintptr_t)lv_event_get_user_data(e);
    if (encoder_idx >= virtual_display_encoders.size()) return;

    VirtualEncoder& vencoder = virtual_display_encoders[encoder_idx];
    if (!vencoder.hasButton) return;
    if (code == LV_EVENT_PRESSED) {
        SendEmulatedEvent(vencoder.buttonBoardId, 'b', 0, vencoder.buttonCode + 0x80);
    } else if (code == LV_EVENT_RELEASED) {
        SendEmulatedEvent(vencoder.buttonBoardId, 'b', 0, vencoder.buttonCode);
    }
}

void SimulatorGUI::FaderSliderEventCB(lv_event_t* e) {
    lv_obj_t* slider = (lv_obj_t*)lv_event_get_target(e);
    uintptr_t fader_idx = (uintptr_t)lv_event_get_user_data(e);
    if (fader_idx >= virtual_faders.size()) return;

    VirtualFaderStrip& vstrip = virtual_faders[fader_idx];
    uint16_t val = (uint16_t)lv_slider_get_value(slider);

    SendEmulatedEvent(vstrip.boardId, 'f', vstrip.faderIndex, val);
}

void SimulatorGUI::ButtonEventCB(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    uintptr_t btn_idx = (uintptr_t)lv_event_get_user_data(e);
    if (btn_idx >= virtual_buttons.size()) return;

    VirtualButton& vbtn = virtual_buttons[btn_idx];

    if (code == LV_EVENT_PRESSED) {
        SendEmulatedEvent(vbtn.boardId, 'b', 0, vbtn.buttonCode + 0x80);
    } else if (code == LV_EVENT_RELEASED) {
        SendEmulatedEvent(vbtn.boardId, 'b', 0, vbtn.buttonCode);
    }
}

void SimulatorGUI::SendEmulatedEvent(uint8_t boardId, uint8_t classId, uint8_t index, uint16_t value) {
    if (ctrl == nullptr || ctrl->surface == nullptr || ctrl->surface->uart == nullptr) return;

    // 1. Send Board ID Announcement Frame
    char boardMsg[4];
    boardMsg[0] = 0xFE;
    boardMsg[1] = 0x80 | boardId;
    boardMsg[2] = 0xFE;
    int32_t sumBoard = 0xFE - 0xFE - (0x80 | boardId) - 1;
    boardMsg[3] = sumBoard & 0x7F;
    ctrl->surface->uart->WriteEmulatedRx(boardMsg, 4);

    // 2. Send Event Frame
    if (classId == 'b' || classId == 'e') { // Short package
        char eventMsg[5];
        eventMsg[0] = classId;
        eventMsg[1] = index;
        eventMsg[2] = (uint8_t)value;
        eventMsg[3] = 0xFE;
        int32_t sum = 0xFE - classId - index - (uint8_t)value - 2;
        eventMsg[4] = sum & 0x7F;
        ctrl->surface->uart->WriteEmulatedRx(eventMsg, 5);
    } else if (classId == 'f') { // Long package
        char eventMsg[6];
        eventMsg[0] = classId;
        eventMsg[1] = index;
        eventMsg[2] = value & 0xFF;
        eventMsg[3] = (value >> 8) & 0xFF;
        eventMsg[4] = 0xFE;
        int32_t sum = 0xFE - classId - index - (value & 0xFF) - ((value >> 8) & 0xFF) - 3;
        eventMsg[5] = sum & 0x7F;
        ctrl->surface->uart->WriteEmulatedRx(eventMsg, 6);
    }
}

void SimulatorGUI::Tick() {
    if (ctrl == nullptr || ctrl->surface == nullptr || ctrl->surface->uart == nullptr) return;

    // Parse commands from emulated TX buffer
    ParseEmulatedTxData();
}

void SimulatorGUI::ParseEmulatedTxData() {
    char buf[1024];
    size_t len = ctrl->surface->uart->ReadEmulatedTx(buf, sizeof(buf));
    if (len == 0) return;

    static std::vector<uint8_t> parse_buf;
    parse_buf.insert(parse_buf.end(), buf, buf + len);

    size_t i = 0;
    while (i < parse_buf.size()) {
        if (parse_buf[i] != 0xFE) {
            i++;
            continue;
        }

        if (i + 1 >= parse_buf.size()) {
            break; // Needs more bytes
        }

        uint8_t first = parse_buf[i + 1];
        if (first >= 0x80) {
            // Find next end divider 0xFE
            size_t end_idx = i + 2;
            while (end_idx < parse_buf.size() && parse_buf[end_idx] != 0xFE) {
                end_idx++;
            }

            if (end_idx >= parse_buf.size()) {
                break; // Incomplete packet
            }

            if (end_idx + 1 >= parse_buf.size()) {
                break; // Checksum not ready
            }

            uint8_t board = first & 0x7F;
            if (i + 2 < end_idx) {
                uint8_t cls = parse_buf[i + 2];
                uint8_t idx = (i + 3 < end_idx) ? parse_buf[i + 3] : 0;

                std::vector<uint8_t> data;
                for (size_t d = i + 4; d < end_idx; d++) {
                    data.push_back(parse_buf[d]);
                }

                ProcessEmulatedOutput(board, cls, idx, data);
            }

            i = end_idx + 2; // Jump past 0xFE and checksum
        } else {
            i++; // Skip invalid
        }
    }

    if (i > 0) {
        parse_buf.erase(parse_buf.begin(), parse_buf.begin() + std::min(i, parse_buf.size()));
    }
}

void SimulatorGUI::ProcessEmulatedOutput(uint8_t boardId, uint8_t classId, uint8_t index, const std::vector<uint8_t>& data) {
    if (classId == 'F') { // Fader update
        if (data.size() < 2) return;
        uint16_t position = ((uint16_t)data[1] << 8) | data[0];

        // Find matching fader strip
        for (auto& vstrip : virtual_faders) {
            if (vstrip.boardId == boardId && vstrip.faderIndex == index) {
                if (!lv_obj_has_state(vstrip.slider, LV_STATE_PRESSED)) {
                    lv_slider_set_value(vstrip.slider, position, LV_ANIM_OFF);
                }
                break;
            }
        }
    } else if (classId == 'L') { // LED update
        if (data.empty()) return;
        uint8_t val = data[0];
        bool ledOn = (val >= 0x80);
        uint8_t buttonCode = ledOn ? (val - 0x80) : val;

        // Search virtual buttons
        for (auto& vbtn : virtual_buttons) {
            if (vbtn.boardId == boardId && vbtn.buttonCode == buttonCode) {
                if (ledOn) {
                    lv_obj_add_state(vbtn.btn_obj, LV_STATE_CHECKED);
                } else {
                    lv_obj_clear_state(vbtn.btn_obj, LV_STATE_CHECKED);
                }
            }
        }
        for (auto& vencoder : virtual_display_encoders) {
            if (vencoder.buttonBoardId == boardId && vencoder.buttonCode == buttonCode) {
                if (ledOn) {
                    if (vencoder.button_obj) lv_obj_add_state(vencoder.button_obj, LV_STATE_CHECKED);
                } else {
                    if (vencoder.button_obj) lv_obj_clear_state(vencoder.button_obj, LV_STATE_CHECKED);
                }
            }
        }
    } else if (classId == 'R') { // Encoder ring update
        if (data.size() < 2) return;
        uint16_t ring = data[0] | ((uint16_t)data[1] << 8);
        ring &= 0x1FFF;

        int highest_led = 0;
        for (int bit = 0; bit < 13; bit++) {
            if (ring & (1U << bit)) {
                highest_led = bit;
            }
        }
        int value = (highest_led * 127) / 12;

        for (auto& vencoder : virtual_display_encoders) {
            if (vencoder.boardId == boardId && vencoder.encoderIndex == index) {
                if (vencoder.showFeedback && !lv_obj_has_state(vencoder.arc_obj, LV_STATE_PRESSED)) {
                    vencoder.lastValue = value;
                    lv_arc_set_value(vencoder.arc_obj, value);
                }
                break;
            }
        }
    } else if (classId == 'D') { // Scribble LCD update
        if (data.empty()) return;
        uint8_t color_byte = data[0];

        // Map color (bits 0,1,2) to hexadecimal
        // color values: BLACK=0, RED=1, GREEN=2, YELLOW=3, BLUE=4, PINK=5, CYAN=6, WHITE=7
        uint8_t color_idx = color_byte & 0x07;
        uint32_t color_hex = 0x000000;
        switch (color_idx) {
            case 0: color_hex = 0x1A1A1A; break; // Black
            case 1: color_hex = 0x990000; break; // Red
            case 2: color_hex = 0x008000; break; // Green
            case 3: color_hex = 0xD4AF37; break; // Yellow
            case 4: color_hex = 0x000080; break; // Blue
            case 5: color_hex = 0xC000C0; break; // Pink
            case 6: color_hex = 0x008080; break; // Cyan
            case 7: color_hex = 0xCCCCCC; break; // White
        }

        // Parse text blocks starting at index 4 (skip icon metadata)
        std::string display_text;
        size_t d_idx = 4;
        while (d_idx + 3 < data.size()) {
            uint8_t size_and_len = data[d_idx];
            uint8_t len = size_and_len & 0x1F;
            std::string s;
            for (size_t k = 0; k < len && d_idx + 3 + k < data.size(); k++) {
                s += (char)data[d_idx + 3 + k];
            }
            if (!s.empty()) {
                if (!display_text.empty()) display_text += " ";
                display_text += s;
            }
            d_idx += 3 + len;
        }

        // Update virtual fader LCD strip matching board and index
        for (auto& vstrip : virtual_faders) {
            // Index of display matches the channel strip index
            if (vstrip.boardId == boardId && vstrip.faderIndex == index) {
                lv_obj_set_style_bg_color(vstrip.lcd_container, lv_color_hex(color_hex), LV_PART_MAIN);
                lv_label_set_text(vstrip.lcd_label, display_text.c_str());

                // Set text color to contrast with the background
                bool is_dark = (color_idx == 0 || color_idx == 4 || color_idx == 1);
                lv_obj_set_style_text_color(vstrip.lcd_label, is_dark ? lv_color_hex(0xFFFFFF) : lv_color_hex(0x000000), LV_PART_MAIN);
                break;
            }
        }
    }
}
#endif
