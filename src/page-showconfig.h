#pragma once
#include "page.h"

using namespace std;
using enum MP_ID;

namespace OMC
{

/**
 * SHOW CONFIG / FADER MAP — full-screen port of the browser prototype.
 *
 * Renders an 800x480 overlay on lv_layer_top() that reproduces the
 * index.html layout: top bar with brand + 16 tabs, left soft-key column,
 * page content, and bottom bar. It is shown only while this page is active.
 *
 * The original static prototype is kept under contrib/openx32-quantum/.
 */
class PageShowConfig : public Page
{
    private:
        static lv_obj_t* overlay;

        // Palette from contrib/openx32-quantum/styles.css
        static constexpr uint32_t C_BG       = 0x0d0e10;
        static constexpr uint32_t C_PANEL    = 0x191b1e;
        static constexpr uint32_t C_PANEL2   = 0x22252a;
        static constexpr uint32_t C_PANEL3   = 0x2c3036;
        static constexpr uint32_t C_LINE     = 0x353a41;
        static constexpr uint32_t C_TXT      = 0xd9dde1;
        static constexpr uint32_t C_TXT_HI   = 0xffffff;
        static constexpr uint32_t C_DIM      = 0x7d858d;
        static constexpr uint32_t C_DIMMER   = 0x545b62;
        static constexpr uint32_t C_AMBER    = 0xff9d00;
        static constexpr uint32_t C_AMBER_S  = 0xffc45e;
        static constexpr uint32_t C_AMBER_D  = 0x8a5600;
        static constexpr uint32_t C_GREEN    = 0x65d472;

        int activeBank = 0;
        int selectedSlot = 0;
        bool customBank = false;

        lv_obj_t* bankBtns[12] = {};
        lv_obj_t* slotCells[12] = {};
        lv_obj_t* tabBtns[16] = {};
        lv_obj_t* softBtns[14] = {};
        lv_obj_t* headNote = nullptr;

        static const char* TAB_LABELS[16];
        static const char* SOFT_LABELS[14][2];
        static const char* BANK_NAMES[12];

        // ---- helpers -------------------------------------------------------
        static lv_obj_t* Box(lv_obj_t* parent, int x, int y, int w, int h,
                             uint32_t bg, uint32_t border = C_LINE, int borderW = 1)
        {
            lv_obj_t* o = lv_obj_create(parent);
            lv_obj_set_pos(o, x, y);
            lv_obj_set_size(o, w, h);
            lv_obj_set_style_bg_color(o, lv_color_hex(bg), LV_PART_MAIN);
            lv_obj_set_style_border_color(o, lv_color_hex(border), LV_PART_MAIN);
            lv_obj_set_style_border_width(o, borderW, LV_PART_MAIN);
            lv_obj_set_style_pad_all(o, 0, LV_PART_MAIN);
            lv_obj_set_style_radius(o, 0, LV_PART_MAIN);
            lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
            return o;
        }

        static lv_obj_t* Label(lv_obj_t* parent, int x, int y, int w,
                               const char* text, uint32_t color,
                               const lv_font_t* font = nullptr,
                               lv_text_align_t align = LV_TEXT_ALIGN_LEFT)
        {
            lv_obj_t* l = lv_label_create(parent);
            lv_obj_set_pos(l, x, y);
            if (w > 0) lv_obj_set_width(l, w);
            lv_label_set_text(l, text);
            lv_obj_set_style_text_color(l, lv_color_hex(color), LV_PART_MAIN);
            lv_obj_set_style_text_font(l, font ? font : &lv_font_montserrat_14, LV_PART_MAIN);
            lv_obj_set_style_text_align(l, align, LV_PART_MAIN);
            return l;
        }

        static void BtnLook(lv_obj_t* b, bool on, bool amber = false)
        {
            lv_obj_set_style_bg_color(b, lv_color_hex(on ? (amber ? C_AMBER : C_PANEL3) : C_PANEL2), LV_PART_MAIN);
            lv_obj_set_style_border_color(b, lv_color_hex(on ? C_AMBER : C_LINE), LV_PART_MAIN);
            lv_obj_set_style_text_color(b, lv_color_hex(on ? (amber ? 0x151515 : C_TXT_HI) : C_DIM), LV_PART_MAIN);
        }

        void BuildOverlay()
        {
            if (overlay) return;

            overlay = lv_obj_create(lv_layer_top());
            lv_obj_set_pos(overlay, 0, 0);
            lv_obj_set_size(overlay, 800, 480);
            lv_obj_set_style_bg_color(overlay, lv_color_hex(C_BG), LV_PART_MAIN);
            lv_obj_set_style_border_width(overlay, 0, LV_PART_MAIN);
            lv_obj_set_style_pad_all(overlay, 0, LV_PART_MAIN);
            lv_obj_set_style_radius(overlay, 0, LV_PART_MAIN);
            lv_obj_clear_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_add_flag(overlay, LV_OBJ_FLAG_HIDDEN);

            // ===== TOP BAR (0,0 800x34) =====
            lv_obj_t* top = Box(overlay, 0, 0, 800, 34, 0x202328, 0x15171a, 0);
            lv_obj_set_style_border_side(top, LV_BORDER_SIDE_BOTTOM, LV_PART_MAIN);

            // Brand mark "X"
            lv_obj_t* mark = Box(top, 7, 7, 20, 20, C_AMBER, C_AMBER, 0);
            Label(mark, 0, 2, 20, "X", 0x151515, &lv_font_montserrat_14, LV_TEXT_ALIGN_CENTER);
            // Brand name
            lv_obj_t* bn = Label(top, 32, 10, 100, "OPENX32", C_TXT_HI, &lv_font_montserrat_14);
            (void)bn;
            Label(top, 111, 12, 30, "/M32", C_AMBER, &lv_font_montserrat_10);

            // Tabs (start x=132, 16 tabs, ~38px each -> 608px, status 60px)
            const int tabX = 132, tabW = 38, tabY = 0, tabH = 34;
            for (int i = 0; i < 16; ++i)
            {
                lv_obj_t* b = lv_btn_create(top);
                lv_obj_set_pos(b, tabX + i * tabW, tabY);
                lv_obj_set_size(b, tabW, tabH);
                lv_obj_set_style_bg_color(b, lv_color_hex(0x15171a), LV_PART_MAIN);
                lv_obj_set_style_border_color(b, lv_color_hex(0x2a2e33), LV_PART_MAIN);
                lv_obj_set_style_border_width(b, 1, LV_PART_MAIN);
                lv_obj_set_style_pad_all(b, 0, LV_PART_MAIN);
                lv_obj_set_style_radius(b, 0, LV_PART_MAIN);
                lv_obj_t* l = lv_label_create(b);
                lv_obj_center(l);
                lv_label_set_text(l, TAB_LABELS[i]);
                lv_obj_set_style_text_font(l, &lv_font_montserrat_10, LV_PART_MAIN);
                lv_obj_set_style_text_color(l, lv_color_hex(C_DIM), LV_PART_MAIN);
                tabBtns[i] = b;
            }
            // Highlight MAP tab (index 5)
            lv_obj_set_style_bg_color(tabBtns[5], lv_color_hex(C_AMBER), LV_PART_MAIN);
            lv_obj_set_style_text_color(lv_obj_get_child(tabBtns[5], 0), lv_color_hex(0x121212), LV_PART_MAIN);

            // Status
            lv_obj_t* led = Box(top, 746, 14, 6, 6, C_GREEN, C_GREEN, 0);
            (void)led;
            Label(top, 756, 11, 44, "00:00", C_DIM, &lv_font_montserrat_10);
            Label(top, 792, 11, 8, "/", C_DIMMER, &lv_font_montserrat_10);

            // ===== LEFT SOFT KEYS (0,34 54x414) =====
            lv_obj_t* soft = Box(overlay, 0, 34, 54, 414, 0x15171a, 0x000000, 0);
            lv_obj_set_style_border_side(soft, LV_BORDER_SIDE_RIGHT, LV_PART_MAIN);
            const int skY = 38, skH = 27, skGap = 2;
            for (int i = 0; i < 14; ++i)
            {
                lv_obj_t* b = lv_btn_create(soft);
                lv_obj_set_pos(b, 4, skY + i * (skH + skGap));
                lv_obj_set_size(b, 46, skH);
                lv_obj_set_style_bg_color(b, lv_color_hex(0x252930), LV_PART_MAIN);
                lv_obj_set_style_border_color(b, lv_color_hex(0x3b4148), LV_PART_MAIN);
                lv_obj_set_style_border_width(b, 1, LV_PART_MAIN);
                lv_obj_set_style_pad_all(b, 0, LV_PART_MAIN);
                lv_obj_set_style_radius(b, 0, LV_PART_MAIN);
                lv_obj_t* l1 = lv_label_create(b);
                lv_obj_set_pos(l1, 0, 4);
                lv_obj_set_width(l1, 46);
                lv_label_set_text(l1, SOFT_LABELS[i][0]);
                lv_obj_set_style_text_font(l1, &lv_font_montserrat_10, LV_PART_MAIN);
                lv_obj_set_style_text_color(l1, lv_color_hex(C_TXT), LV_PART_MAIN);
                lv_obj_set_style_text_align(l1, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
                lv_obj_t* l2 = lv_label_create(b);
                lv_obj_set_pos(l2, 0, 16);
                lv_obj_set_width(l2, 46);
                lv_label_set_text(l2, SOFT_LABELS[i][1]);
                lv_obj_set_style_text_font(l2, &lv_font_montserrat_10, LV_PART_MAIN);
                lv_obj_set_style_text_color(l2, lv_color_hex(C_DIM), LV_PART_MAIN);
                lv_obj_set_style_text_align(l2, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
                softBtns[i] = b;
            }
            // Highlight MAP soft key (index 8)
            lv_obj_set_style_bg_color(softBtns[8], lv_color_hex(C_AMBER_D), LV_PART_MAIN);
            lv_obj_set_style_border_color(softBtns[8], lv_color_hex(C_AMBER), LV_PART_MAIN);

            // ===== SCREEN AREA (54,34 746x414) =====
            lv_obj_t* screen = Box(overlay, 54, 34, 746, 414, 0x0e0f11, 0x000000, 0);

            // Page head
            Label(screen, 8, 7, 200, "FADER ASSIGN", C_AMBER, &lv_font_montserrat_14);
            headNote = Label(screen, 400, 7, 338, "", C_DIM, &lv_font_montserrat_10, LV_TEXT_ALIGN_RIGHT);

            // Bank picker panel (height 96)
            lv_obj_t* bankPanel = Box(screen, 8, 30, 730, 96, C_PANEL, C_LINE, 1);
            Label(bankPanel, 6, 4, 300, "FADER BANKS · 8 INPUT (2×4) + 4 OUTPUT (1×4)", C_DIM, &lv_font_montserrat_10);
            for (int i = 0; i < 12; ++i)
            {
                int col = i % 6, row = i / 6;
                lv_obj_t* b = lv_btn_create(bankPanel);
                lv_obj_set_pos(b, 6 + col * 120, 20 + row * 34);
                lv_obj_set_size(b, 116, 30);
                lv_obj_set_style_bg_color(b, lv_color_hex(C_PANEL2), LV_PART_MAIN);
                lv_obj_set_style_border_color(b, lv_color_hex(C_LINE), LV_PART_MAIN);
                lv_obj_set_style_border_width(b, 1, LV_PART_MAIN);
                lv_obj_set_style_pad_all(b, 0, LV_PART_MAIN);
                lv_obj_set_style_radius(b, 0, LV_PART_MAIN);
                lv_obj_t* l = lv_label_create(b);
                lv_obj_center(l);
                lv_label_set_text_fmt(l, "%s", BANK_NAMES[i]);
                lv_obj_set_style_text_font(l, &lv_font_montserrat_10, LV_PART_MAIN);
                lv_obj_set_style_text_color(l, lv_color_hex(C_TXT), LV_PART_MAIN);
                bankBtns[i] = b;
            }

            // Main row: slot grid + sources
            lv_obj_t* leftPanel = Box(screen, 8, 132, 360, 252, C_PANEL, C_LINE, 1);
            Label(leftPanel, 6, 4, 250, "FADER BANK · SELECT SLOT", C_DIM, &lv_font_montserrat_10);
            Label(leftPanel, 250, 4, 100, "SEL SLOT → SRC", C_AMBER, &lv_font_montserrat_10, LV_TEXT_ALIGN_RIGHT);

            for (int i = 0; i < 12; ++i)
            {
                int col = i % 4, row = i / 4;
                lv_obj_t* c = Box(leftPanel, 6 + col * 87, 20 + row * 52, 83, 48,
                                  0x16191c, 0x30363c, 1);
                lv_obj_t* num = Label(c, 4, 2, 20, "", C_DIM, &lv_font_montserrat_10);
                lv_label_set_text_fmt(num, "%02d", i + 1);
                lv_obj_t* nm = Label(c, 4, 16, 75, "CH", C_TXT_HI, &lv_font_montserrat_10);
                (void)nm;
                Label(c, 4, 32, 75, "—", C_DIMMER, &lv_font_montserrat_10);
                slotCells[i] = c;
            }

            // Action buttons
            const char* acts[] = {"FREE ALL", "STRAIGHT", "CUSTOM OFF"};
            for (int i = 0; i < 3; ++i)
            {
                lv_obj_t* b = lv_btn_create(leftPanel);
                lv_obj_set_pos(b, 6 + i * 117, 194);
                lv_obj_set_size(b, 112, 24);
                lv_obj_set_style_bg_color(b, lv_color_hex(C_PANEL2), LV_PART_MAIN);
                lv_obj_set_style_border_color(b, lv_color_hex(C_LINE), LV_PART_MAIN);
                lv_obj_set_style_border_width(b, 1, LV_PART_MAIN);
                lv_obj_set_style_pad_all(b, 0, LV_PART_MAIN);
                lv_obj_set_style_radius(b, 0, LV_PART_MAIN);
                lv_obj_t* l = lv_label_create(b);
                lv_obj_center(l);
                lv_label_set_text(l, acts[i]);
                lv_obj_set_style_text_font(l, &lv_font_montserrat_10, LV_PART_MAIN);
                lv_obj_set_style_text_color(l, lv_color_hex(C_DIM), LV_PART_MAIN);
            }
            Label(leftPanel, 6, 224, 348,
                  "12 faders per bank · 8 input + 4 output · saved in scene",
                  C_DIM, &lv_font_montserrat_10);

            // Sources panel
            lv_obj_t* rightPanel = Box(screen, 374, 132, 364, 252, C_PANEL, C_LINE, 1);
            Label(rightPanel, 6, 4, 250, "SOURCES / DESTINATIONS", C_DIM, &lv_font_montserrat_10);
            Label(rightPanel, 250, 4, 108, "CLICK = ASSIGN", C_AMBER, &lv_font_montserrat_10, LV_TEXT_ALIGN_RIGHT);

            const char* listTabs[] = {"INPUT", "AUX", "FX RET", "BUS", "OUTPUT"};
            for (int i = 0; i < 5; ++i)
            {
                lv_obj_t* b = lv_btn_create(rightPanel);
                lv_obj_set_pos(b, 6 + i * 71, 20);
                lv_obj_set_size(b, 68, 20);
                lv_obj_set_style_bg_color(b, lv_color_hex(C_PANEL2), LV_PART_MAIN);
                lv_obj_set_style_border_color(b, lv_color_hex(C_LINE), LV_PART_MAIN);
                lv_obj_set_style_border_width(b, 1, LV_PART_MAIN);
                lv_obj_set_style_pad_all(b, 0, LV_PART_MAIN);
                lv_obj_set_style_radius(b, 0, LV_PART_MAIN);
                lv_obj_t* l = lv_label_create(b);
                lv_obj_center(l);
                lv_label_set_text(l, listTabs[i]);
                lv_obj_set_style_text_font(l, &lv_font_montserrat_10, LV_PART_MAIN);
                lv_obj_set_style_text_color(l, lv_color_hex(C_DIM), LV_PART_MAIN);
            }

            for (int i = 0; i < 24; ++i)
            {
                int col = i % 4, row = i / 4;
                lv_obj_t* c = Box(rightPanel, 6 + col * 88, 46 + row * 22, 84, 20,
                                  0x16191c, 0x30363c, 1);
                lv_obj_t* l = Label(c, 4, 3, 76, "", C_DIM, &lv_font_montserrat_10);
                lv_label_set_text_fmt(l, "%02d  CH %02d", i + 1, i + 1);
            }

            // Footer panel
            lv_obj_t* foot = Box(screen, 8, 390, 730, 18, C_PANEL, C_LINE, 1);
            Label(foot, 6, 2, 718,
                  "INPUT 2×4 + OUTPUT 1×4 · FREE ALL → SEL BANK → SEL SLOT → SEL IN/OUT · SAVED IN SCENE",
                  C_DIM, &lv_font_montserrat_10);

            // ===== BOTTOM BAR (0,448 800x32) =====
            lv_obj_t* bottom = Box(overlay, 0, 448, 800, 32, 0x15171a, 0x000000, 0);
            lv_obj_set_style_border_side(bottom, LV_BORDER_SIDE_TOP, LV_PART_MAIN);
            Label(bottom, 8, 9, 784,
                  "ENC 1: BANK   ENC 2: SLOT   BTN 2: APPLY   BTN 6: RESET   •   7: SHOW FIRST UI PROTOTYPE",
                  C_DIM, &lv_font_montserrat_10);

            Refresh();
        }

        void Refresh()
        {
            if (!overlay) return;

            lv_label_set_text_fmt(headNote, "BANK %s · SLOT %02d · %s",
                BANK_NAMES[activeBank], selectedSlot + 1,
                customBank ? "CUSTOM BANK ON" : "AUTO BANK");

            for (int i = 0; i < 12; ++i)
            {
                BtnLook(bankBtns[i], i == activeBank);
                bool sel = (i == selectedSlot);
                lv_obj_set_style_bg_color(slotCells[i],
                    lv_color_hex(sel ? C_AMBER_D : 0x16191c), LV_PART_MAIN);
                lv_obj_set_style_border_color(slotCells[i],
                    lv_color_hex(sel ? C_AMBER : 0x30363c), LV_PART_MAIN);
            }
        }

    public:
        static void HideOverlay()
        {
            if (overlay) lv_obj_add_flag(overlay, LV_OBJ_FLAG_HIDDEN);
        }

        PageShowConfig(PageBaseParameter* pagebasepar) : Page(pagebasepar)
        {
            prevPage = X32_PAGE::PROTOTYPEGUI;
            nextPage = X32_PAGE::LIBRARY;
            tabLayer0 = objects.maintab;
            tabIndex0 = 3;
            tabLayer1 = objects.setuptab;
            tabIndex1 = 5;
            hideEncoders = false;
        }

        void OnInit() override
        {
            BuildOverlay();
        }

        void OnShow() override
        {
            if (!overlay) BuildOverlay();
            lv_obj_clear_flag(overlay, LV_OBJ_FLAG_HIDDEN);
            lv_obj_move_foreground(overlay);

            config->SurfaceBindCustom(SurfaceElementId::DISPLAY_ENCODER_1, "Bank");
            config->SurfaceBindCustom(SurfaceElementId::DISPLAY_ENCODER_2, "Slot");
            config->SurfaceBindCustom(SurfaceElementId::DISPLAY_ENCODER_BUTTON_2);
            config->SurfaceBindCustom(SurfaceElementId::DISPLAY_ENCODER_BUTTON_6);
            Refresh();
        }

        void OnChangeCustomEncoder(SurfaceElementId id, int amount) override
        {
            switch (id)
            {
                case SurfaceElementId::DISPLAY_ENCODER_1:
                    activeBank += amount;
                    if (activeBank < 0) activeBank = 11;
                    if (activeBank > 11) activeBank = 0;
                    break;
                case SurfaceElementId::DISPLAY_ENCODER_2:
                    selectedSlot += amount;
                    if (selectedSlot < 0) selectedSlot = 11;
                    if (selectedSlot > 11) selectedSlot = 0;
                    break;
                default:
                    return;
            }
            Refresh();
        }

        void OnChangeCustomButton(SurfaceElementId id) override
        {
            switch (id)
            {
                case SurfaceElementId::DISPLAY_ENCODER_BUTTON_2:
                    config->Set(BANKING_INPUT, (uint)activeBank, 0);
                    break;
                case SurfaceElementId::DISPLAY_ENCODER_BUTTON_6:
                    activeBank = 0;
                    selectedSlot = 0;
                    break;
                default:
                    return;
            }
            Refresh();
        }
};

// Static member definitions
lv_obj_t* PageShowConfig::overlay = nullptr;

const char* PageShowConfig::TAB_LABELS[16] = {
    "OVER", "CHAN", "EQ", "DYN", "AUX", "MAP", "MON", "ROUTE",
    "DLY", "MET", "FX", "CARD", "MACRO", "SCENE", "CFG", "SYS"
};

const char* PageShowConfig::SOFT_LABELS[14][2] = {
    {"SEL", "select"}, {"MUTE", "channel"}, {"SOLO", "channel"},
    {"◀", "bank"}, {"▶", "bank"}, {"EQ", "view"}, {"DYN", "view"},
    {"AUX", "sends"}, {"MAP", "fader bank"}, {"MON", "listen"},
    {"MAC", "macro"}, {"DLY", "align"}, {"CFG", "show setup"},
    {"HOME", "overview"}
};

const char* PageShowConfig::BANK_NAMES[12] = {
    "IN 1-8 A", "IN 9-16 A", "IN 17-24 A", "IN 25-32 A",
    "IN 1-8 B", "IN 9-16 B", "IN 17-24 B", "IN 25-32 B",
    "OUT 1-4", "OUT 5-8", "OUT 9-12", "OUT 13-16"
};

}
