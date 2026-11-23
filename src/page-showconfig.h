#pragma once
#include "page.h"

using namespace std;
using enum MP_ID;

namespace OMC
{

/**
 * SHOW CONFIG / FADER MAP - full-screen port of the browser prototype.
 *
 * Renders an 800x480 overlay on lv_layer_top() that reproduces the
 * index.html layout: top bar with brand + 16 tabs, left soft-key column,
 * page content, and bottom bar. It is shown only while this page is active.
 *
 * The original static prototype is kept under contrib/openx32-quantum/.
 * All labels use ASCII only: the bundled Montserrat fonts do not contain
 * arrows, bullets or multiplication signs, which would render as boxes.
 */
class PageShowConfig : public Page
{
    private:
        static lv_obj_t* overlay;
        static PageShowConfig* self;

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

        int activeTab = 5;      // MAP, as in the browser prototype
        int activeBank = 0;
        int selectedSlot = 0;
        int activeList = 0;
        bool customBank = false;

        lv_obj_t* body = nullptr;
        lv_obj_t* bankBtns[12] = {};
        lv_obj_t* slotCells[12] = {};
        lv_obj_t* slotNames[12] = {};
        lv_obj_t* listLabels[24] = {};
        lv_obj_t* listBtns[5] = {};
        lv_obj_t* tabBtns[16] = {};
        lv_obj_t* softBtns[14] = {};
        lv_obj_t* genericVals[8] = {};
        lv_obj_t* headNote = nullptr;
        char slotLabel[12][12][16] = {};
        char statusMsg[96] = {};
        int genericState[8] = {};

        static const char* TAB_LABELS[16];
        static const char* PAGE_TITLES[16];
        static const char* PAGE_ROWS[16][8];
        static const char* SOFT_LABELS[14][2];
        static const char* BANK_NAMES[12];
        static const char* LIST_PREFIX[5];
        static const int SOFT_TAB[14];

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
            lv_label_set_long_mode(l, LV_LABEL_LONG_CLIP);
            lv_label_set_text(l, text);
            lv_obj_set_style_text_color(l, lv_color_hex(color), LV_PART_MAIN);
            lv_obj_set_style_text_font(l, font ? font : &lv_font_montserrat_14, LV_PART_MAIN);
            lv_obj_set_style_text_align(l, align, LV_PART_MAIN);
            return l;
        }

        static lv_obj_t* Button(lv_obj_t* parent, int x, int y, int w, int h,
                                const char* text, uint32_t bg, uint32_t fg,
                                int code)
        {
            lv_obj_t* b = lv_btn_create(parent);
            lv_obj_set_pos(b, x, y);
            lv_obj_set_size(b, w, h);
            lv_obj_set_style_bg_color(b, lv_color_hex(bg), LV_PART_MAIN);
            lv_obj_set_style_border_color(b, lv_color_hex(C_LINE), LV_PART_MAIN);
            lv_obj_set_style_border_width(b, 1, LV_PART_MAIN);
            lv_obj_set_style_pad_all(b, 0, LV_PART_MAIN);
            lv_obj_set_style_radius(b, 0, LV_PART_MAIN);
            lv_obj_t* l = lv_label_create(b);
            lv_obj_center(l);
            lv_label_set_long_mode(l, LV_LABEL_LONG_CLIP);
            lv_label_set_text(l, text);
            lv_obj_set_style_text_font(l, &lv_font_montserrat_10, LV_PART_MAIN);
            lv_obj_set_style_text_color(l, lv_color_hex(fg), LV_PART_MAIN);
            lv_obj_add_event_cb(b, Clicked, LV_EVENT_CLICKED, (void*)(intptr_t)code);
            return b;
        }

        static lv_obj_t* ClickableBox(lv_obj_t* parent, int x, int y, int w, int h,
                                      uint32_t bg, int code)
        {
            lv_obj_t* o = Box(parent, x, y, w, h, bg, 0x30363c, 1);
            lv_obj_add_flag(o, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_event_cb(o, Clicked, LV_EVENT_CLICKED, (void*)(intptr_t)code);
            return o;
        }

        static void BtnLook(lv_obj_t* b, bool on, bool amber = false)
        {
            lv_obj_set_style_bg_color(b, lv_color_hex(on ? (amber ? C_AMBER : C_PANEL3) : C_PANEL2), LV_PART_MAIN);
            lv_obj_set_style_border_color(b, lv_color_hex(on ? C_AMBER : C_LINE), LV_PART_MAIN);
            lv_obj_set_style_text_color(lv_obj_get_child(b, 0), lv_color_hex(on ? (amber ? 0x151515 : C_TXT_HI) : C_DIM), LV_PART_MAIN);
        }

        // ---- click handling ------------------------------------------------
        // Codes: 0..11 bank, 100..111 slot, 200..223 source,
        //        300..302 action, 400..404 source list,
        //        500..513 soft key, 600..615 top tab, 700..707 generic row.
        static void Clicked(lv_event_t* e)
        {
            if (!self) return;
            self->HandleClick((int)(intptr_t)lv_event_get_user_data(e));
        }

        void Note(const char* text)
        {
            snprintf(statusMsg, sizeof(statusMsg), "%s", text);
            Refresh();
        }

        void SwitchTab(int tab)
        {
            if (tab < 0 || tab > 15) return;
            activeTab = tab;
            for (int i = 0; i < 16; ++i) BtnLook(tabBtns[i], i == activeTab, true);
            for (int i = 0; i < 14; ++i) BtnLook(softBtns[i], false);
            int soft = -1;
            for (int i = 0; i < 14; ++i) if (SOFT_TAB[i] == activeTab) soft = i;
            if (soft >= 0) BtnLook(softBtns[soft], true);

            if (body) lv_obj_clean(body);
            if (activeTab == 5) BuildMapBody();
            else BuildGenericBody(activeTab);
            snprintf(statusMsg, sizeof(statusMsg), "TAB %s", TAB_LABELS[activeTab]);
            Refresh();
        }

        void HandleClick(int code)
        {
            if (code >= 600 && code < 616)
            {
                SwitchTab(code - 600);
                return;
            }
            if (code >= 700 && code < 708)
            {
                int r = code - 700;
                genericState[r] = !genericState[r];
                lv_label_set_text(genericVals[r], genericState[r] ? "ON" : "OFF");
                char buf[64];
                snprintf(buf, sizeof(buf), "%s = %s", PAGE_ROWS[activeTab][r], genericState[r] ? "ON" : "OFF");
                Note(buf);
                return;
            }
            if (activeTab != 5)
            {
                Note("Use the top tabs to change page");
                return;
            }
            if (code >= 0 && code < 12)
            {
                activeBank = code;
                Note("Bank selected - edit slot, then press X to apply");
            }
            else if (code >= 100 && code < 112)
            {
                selectedSlot = code - 100;
                Note("Slot selected - pick a source on the right");
            }
            else if (code >= 200 && code < 224)
            {
                int src = code - 200;
                snprintf(slotLabel[activeBank][selectedSlot], 16,
                         "%s %02d", LIST_PREFIX[activeList], src + 1);
                selectedSlot = (selectedSlot + 1) % 12;
                Note("Source assigned to slot");
            }
            else if (code >= 300 && code < 303)
            {
                if (code == 300)
                {
                    for (int i = 0; i < 12; ++i) slotLabel[activeBank][i][0] = 0;
                    Note("All faders in this bank freed");
                }
                else if (code == 301)
                {
                    for (int i = 0; i < 12; ++i)
                        snprintf(slotLabel[activeBank][i], 16, "CH %02d", i + 1);
                    Note("Straight 1:1 mapping applied");
                }
                else
                {
                    customBank = false;
                    Note("Custom bank disabled");
                }
            }
            else if (code >= 400 && code < 405)
            {
                activeList = code - 400;
                for (int i = 0; i < 24; ++i)
                    lv_label_set_text_fmt(listLabels[i], "%s %02d", LIST_PREFIX[activeList], i + 1);
                for (int i = 0; i < 5; ++i) BtnLook(listBtns[i], i == activeList);
                Note("Source list changed");
            }
            else if (code >= 500 && code < 514)
            {
                int k = code - 500;
                if (k == 3 || k == 4)
                {
                    activeBank = (activeBank + (k == 3 ? -1 : 1) + 12) % 12;
                    Note("Bank changed");
                }
                else if (k == 13)
                {
                    config->Set(ACTIVE_PAGE, (uint)X32_PAGE::HOME);
                }
                else if (SOFT_TAB[k] >= 0)
                {
                    SwitchTab(SOFT_TAB[k]);
                }
                else
                {
                    Note(SOFT_LABELS[k][0]);
                }
            }
        }

        // ---- page bodies ---------------------------------------------------
        void BuildMapBody()
        {
            lv_obj_t* bankPanel = Box(body, 8, 0, 730, 96, C_PANEL, C_LINE, 1);
            Label(bankPanel, 6, 4, 400,
                  "FADER BANKS - 8 INPUT (2x4) + 4 OUTPUT (1x4)",
                  C_DIM, &lv_font_montserrat_10);
            for (int i = 0; i < 12; ++i)
            {
                int col = i % 6, row = i / 6;
                bankBtns[i] = Button(bankPanel, 6 + col * 120, 20 + row * 34,
                                     116, 30, BANK_NAMES[i], C_PANEL2, C_TXT, i);
            }

            lv_obj_t* leftPanel = Box(body, 8, 102, 360, 252, C_PANEL, C_LINE, 1);
            Label(leftPanel, 6, 4, 250, "FADER BANK - SELECT SLOT", C_DIM, &lv_font_montserrat_10);
            Label(leftPanel, 250, 4, 100, "SEL SLOT > SRC", C_AMBER, &lv_font_montserrat_10, LV_TEXT_ALIGN_RIGHT);

            for (int i = 0; i < 12; ++i)
            {
                int col = i % 4, row = i / 4;
                lv_obj_t* c = ClickableBox(leftPanel, 6 + col * 87, 20 + row * 52,
                                           83, 48, 0x16191c, 100 + i);
                lv_obj_t* num = Label(c, 4, 2, 24, "", C_DIM, &lv_font_montserrat_10);
                lv_label_set_text_fmt(num, "%02d", i + 1);
                slotNames[i] = Label(c, 4, 16, 75, "CH", C_TXT_HI, &lv_font_montserrat_10);
                Label(c, 4, 32, 75, "-", C_DIMMER, &lv_font_montserrat_10);
                slotCells[i] = c;
            }

            const char* acts[] = {"FREE ALL", "STRAIGHT", "CUSTOM OFF"};
            for (int i = 0; i < 3; ++i)
                Button(leftPanel, 6 + i * 117, 194, 112, 24,
                       acts[i], C_PANEL2, C_DIM, 300 + i);

            Label(leftPanel, 6, 224, 348,
                  "12 faders per bank - 8 input + 4 output - saved in scene",
                  C_DIM, &lv_font_montserrat_10);

            lv_obj_t* rightPanel = Box(body, 374, 102, 364, 252, C_PANEL, C_LINE, 1);
            Label(rightPanel, 6, 4, 250, "SOURCES / DESTINATIONS", C_DIM, &lv_font_montserrat_10);
            Label(rightPanel, 250, 4, 108, "CLICK = ASSIGN", C_AMBER, &lv_font_montserrat_10, LV_TEXT_ALIGN_RIGHT);

            const char* listTabs[] = {"INPUT", "AUX", "FX RET", "BUS", "OUTPUT"};
            for (int i = 0; i < 5; ++i)
                listBtns[i] = Button(rightPanel, 6 + i * 71, 20, 68, 20,
                                     listTabs[i], C_PANEL2, C_DIM, 400 + i);
            BtnLook(listBtns[0], true);

            for (int i = 0; i < 24; ++i)
            {
                int col = i % 4, row = i / 4;
                lv_obj_t* c = ClickableBox(rightPanel, 6 + col * 88, 46 + row * 22,
                                           84, 20, 0x16191c, 200 + i);
                listLabels[i] = Label(c, 4, 3, 76, "", C_DIM, &lv_font_montserrat_10);
                lv_label_set_text_fmt(listLabels[i], "%s %02d", LIST_PREFIX[activeList], i + 1);
            }

            lv_obj_t* foot = Box(body, 8, 360, 730, 18, C_PANEL, C_LINE, 1);
            Label(foot, 6, 2, 718,
                  "INPUT 2x4 + OUTPUT 1x4 - FREE ALL > SEL BANK > SEL SLOT > SEL IN/OUT - SAVED IN SCENE",
                  C_DIM, &lv_font_montserrat_10);
        }

        void BuildGenericBody(int tab)
        {
            lv_obj_t* p = Box(body, 8, 0, 730, 378, C_PANEL, C_LINE, 1);
            Label(p, 8, 8, 400, PAGE_TITLES[tab], C_AMBER, &lv_font_montserrat_14);
            Label(p, 430, 10, 290, "NATIVE PREVIEW - SEE BROWSER PROTOTYPE",
                  C_DIM, &lv_font_montserrat_10, LV_TEXT_ALIGN_RIGHT);

            for (int i = 0; i < 8; ++i)
            {
                lv_obj_t* row = ClickableBox(p, 8, 32 + i * 42, 714, 38,
                                             0x16191c, 700 + i);
                Label(row, 10, 11, 380, PAGE_ROWS[tab][i], C_TXT, &lv_font_montserrat_10);
                genericVals[i] = Label(row, 400, 11, 300,
                                       genericState[i] ? "ON" : "OFF",
                                       genericState[i] ? C_AMBER : C_DIM,
                                       &lv_font_montserrat_10, LV_TEXT_ALIGN_RIGHT);
            }
            Label(p, 8, 374, 714,
                  "CLICK A ROW TO TOGGLE - TOP TABS CHANGE PAGE - 7 RETURNS TO MAP",
                  C_DIM, &lv_font_montserrat_10);
        }

        void BuildOverlay()
        {
            if (overlay) return;
            self = this;

            overlay = lv_obj_create(lv_layer_top());
            lv_obj_set_pos(overlay, 0, 0);
            lv_obj_set_size(overlay, 800, 480);
            lv_obj_set_style_bg_color(overlay, lv_color_hex(C_BG), LV_PART_MAIN);
            lv_obj_set_style_border_width(overlay, 0, LV_PART_MAIN);
            lv_obj_set_style_pad_all(overlay, 0, LV_PART_MAIN);
            lv_obj_set_style_radius(overlay, 0, LV_PART_MAIN);
            lv_obj_clear_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_add_flag(overlay, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_flag(overlay, LV_OBJ_FLAG_HIDDEN);

            // ===== TOP BAR (0,0 800x34) =====
            lv_obj_t* top = Box(overlay, 0, 0, 800, 34, 0x202328, 0x15171a, 0);
            lv_obj_set_style_border_side(top, LV_BORDER_SIDE_BOTTOM, LV_PART_MAIN);

            lv_obj_t* mark = Box(top, 7, 7, 20, 20, C_AMBER, C_AMBER, 0);
            Label(mark, 0, 2, 20, "X", 0x151515, &lv_font_montserrat_14, LV_TEXT_ALIGN_CENTER);
            Label(top, 32, 10, 100, "OPENX32", C_TXT_HI, &lv_font_montserrat_14);
            Label(top, 111, 12, 30, "/M32", C_AMBER, &lv_font_montserrat_10);

            const int tabX = 132, tabW = 38;
            for (int i = 0; i < 16; ++i)
                tabBtns[i] = Button(top, tabX + i * tabW, 0, tabW, 34,
                                    TAB_LABELS[i], 0x15171a, C_DIM, 600 + i);

            Box(top, 746, 14, 6, 6, C_GREEN, C_GREEN, 0);
            Label(top, 756, 11, 44, "00:00", C_DIM, &lv_font_montserrat_10);
            Label(top, 792, 11, 8, "/", C_DIMMER, &lv_font_montserrat_10);

            // ===== LEFT SOFT KEYS (0,34 54x414) =====
            lv_obj_t* soft = Box(overlay, 0, 34, 54, 414, 0x15171a, 0x000000, 0);
            lv_obj_set_style_border_side(soft, LV_BORDER_SIDE_RIGHT, LV_PART_MAIN);
            for (int i = 0; i < 14; ++i)
            {
                lv_obj_t* b = lv_btn_create(soft);
                lv_obj_set_pos(b, 4, 38 + i * 29);
                lv_obj_set_size(b, 46, 27);
                lv_obj_set_style_bg_color(b, lv_color_hex(0x252930), LV_PART_MAIN);
                lv_obj_set_style_border_color(b, lv_color_hex(0x3b4148), LV_PART_MAIN);
                lv_obj_set_style_border_width(b, 1, LV_PART_MAIN);
                lv_obj_set_style_pad_all(b, 0, LV_PART_MAIN);
                lv_obj_set_style_radius(b, 0, LV_PART_MAIN);
                lv_obj_add_event_cb(b, Clicked, LV_EVENT_CLICKED, (void*)(intptr_t)(500 + i));
                Label(b, 0, 4, 46, SOFT_LABELS[i][0], C_TXT, &lv_font_montserrat_10, LV_TEXT_ALIGN_CENTER);
                Label(b, 0, 16, 46, SOFT_LABELS[i][1], C_DIM, &lv_font_montserrat_10, LV_TEXT_ALIGN_CENTER);
                softBtns[i] = b;
            }

            // ===== SCREEN AREA (54,34 746x414) =====
            lv_obj_t* screen = Box(overlay, 54, 34, 746, 414, 0x0e0f11, 0x000000, 0);
            Label(screen, 8, 7, 200, "SHOW CONFIG", C_AMBER, &lv_font_montserrat_14);
            headNote = Label(screen, 400, 7, 338, "", C_DIM, &lv_font_montserrat_10, LV_TEXT_ALIGN_RIGHT);

            body = lv_obj_create(screen);
            lv_obj_set_pos(body, 0, 30);
            lv_obj_set_size(body, 746, 378);
            lv_obj_set_style_bg_opa(body, LV_OPA_TRANSP, LV_PART_MAIN);
            lv_obj_set_style_border_width(body, 0, LV_PART_MAIN);
            lv_obj_set_style_pad_all(body, 0, LV_PART_MAIN);
            lv_obj_clear_flag(body, LV_OBJ_FLAG_SCROLLABLE);

            // ===== BOTTOM BAR (0,448 800x32) =====
            lv_obj_t* bottom = Box(overlay, 0, 448, 800, 32, 0x15171a, 0x000000, 0);
            lv_obj_set_style_border_side(bottom, LV_BORDER_SIDE_TOP, LV_PART_MAIN);
            Label(bottom, 8, 9, 784,
                  "Q/A: BANK   W/S: SLOT   X: APPLY   N: RESET   8: NEXT TAB   7: OPEN",
                  C_DIM, &lv_font_montserrat_10);

            SwitchTab(activeTab);
        }

        void Refresh()
        {
            if (!overlay) return;
            lv_label_set_text_fmt(headNote, "%s | %s | BANK %s | SLOT %02d",
                PAGE_TITLES[activeTab],
                statusMsg[0] ? statusMsg : "READY",
                BANK_NAMES[activeBank], selectedSlot + 1);

            if (activeTab != 5 || !body) return;
            char buf[16];
            for (int i = 0; i < 12; ++i)
            {
                if (!bankBtns[i] || !slotCells[i]) continue;
                BtnLook(bankBtns[i], i == activeBank);
                bool sel = (i == selectedSlot);
                lv_obj_set_style_bg_color(slotCells[i],
                    lv_color_hex(sel ? C_AMBER_D : 0x16191c), LV_PART_MAIN);
                lv_obj_set_style_border_color(slotCells[i],
                    lv_color_hex(sel ? C_AMBER : 0x30363c), LV_PART_MAIN);
                if (slotLabel[activeBank][i][0])
                    snprintf(buf, sizeof(buf), "%s", slotLabel[activeBank][i]);
                else
                    snprintf(buf, sizeof(buf), "CH %02d", i + 1);
                if (slotNames[i]) lv_label_set_text(slotNames[i], buf);
            }
        }

    public:
        static void HideOverlay()
        {
            if (overlay) lv_obj_add_flag(overlay, LV_OBJ_FLAG_HIDDEN);
        }

        static void CycleTabStatic()
        {
            if (overlay && self) self->SwitchTab((self->activeTab + 1) % 16);
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
                    activeBank = (activeBank + amount + 120) % 12;
                    snprintf(statusMsg, sizeof(statusMsg), "Encoder 1 - bank");
                    break;
                case SurfaceElementId::DISPLAY_ENCODER_2:
                    selectedSlot = (selectedSlot + amount + 120) % 12;
                    snprintf(statusMsg, sizeof(statusMsg), "Encoder 2 - slot");
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
                    snprintf(slotLabel[activeBank][selectedSlot], 16,
                             "CH %02d", activeBank * 12 + selectedSlot + 1);
                    snprintf(statusMsg, sizeof(statusMsg), "Button 2 - slot applied");
                    break;
                case SurfaceElementId::DISPLAY_ENCODER_BUTTON_6:
                    for (int i = 0; i < 12; ++i) slotLabel[activeBank][i][0] = 0;
                    activeBank = 0;
                    selectedSlot = 0;
                    snprintf(statusMsg, sizeof(statusMsg), "Button 6 - reset");
                    break;
                default:
                    return;
            }
            Refresh();
        }
};

// Static member definitions
lv_obj_t* PageShowConfig::overlay = nullptr;
PageShowConfig* PageShowConfig::self = nullptr;

const char* PageShowConfig::TAB_LABELS[16] = {
    "OVER", "CHAN", "EQ", "DYN", "AUX", "MAP", "MON", "ROUTE",
    "DLY", "MET", "FX", "CARD", "MACRO", "SCENE", "CFG", "SYS"
};

const char* PageShowConfig::PAGE_TITLES[16] = {
    "OVERVIEW", "CHANNEL STRIP", "EQ", "DYNAMICS", "AUX SENDS",
    "FADER ASSIGN", "MONITOR AUDIO", "ROUTING", "DELAY", "METERS",
    "EFFECTS", "CARD / I/O", "MACROS", "SCENES", "SHOW CONFIG", "SYSTEM"
};

const char* PageShowConfig::PAGE_ROWS[16][8] = {
    {"ACTIVE INPUTS", "ACTIVE AUX", "ACTIVE FX RETURNS", "ACTIVE GROUPS",
     "ACTIVE OUTPUTS", "ACTIVE MATRIX", "SELECTED CHANNEL", "SHOW SUMMARY"},
    {"CHANNEL NAME", "INPUT SOURCE", "FADER LEVEL", "MUTE", "SOLO",
     "PHASE", "WIDTH", "RECORD ARM"},
    {"HIGH SHELF", "LOW-MID 1", "MID 2", "HIGH-MID 3", "LOW SHELF",
     "FILTER TYPE", "FILTER FREQ", "EQ BYPASS"},
    {"GATE THRESHOLD", "GATE RANGE", "COMP THRESHOLD", "COMP RATIO",
     "COMP ATTACK", "COMP RELEASE", "DINAMICS BYPASS", "AUTO GAIN"},
    {"AUX 01 LEVEL", "AUX 02 LEVEL", "AUX 03 LEVEL", "AUX 04 LEVEL",
     "AUX 05 LEVEL", "AUX 06 LEVEL", "AUX 07 LEVEL", "AUX 08 LEVEL"},
    {"BANK 01 - 12", "SLOT 01 - 12", "SOURCE LIST", "FREE ALL",
     "STRAIGHT MAP", "CUSTOM BANK", "SCENE SAFE", "FADER MAP"},
    {"SELECTED CHANNEL", "MONITOR SOURCE", "MONITOR LEVEL", "MUTE",
     "SOLO", "LISTEN BUS", "HEADPHONE OUT", "AUTO SOLO"},
    {"INPUT SOURCE", "AUX SOURCE", "FX RETURN", "GROUP SOURCE",
     "MATRIX SOURCE", "OUTPUT DESTINATION", "CARD SOURCE", "PATCH BAY"},
    {"CHANNEL 01 DELAY", "CHANNEL 02 DELAY", "CHANNEL 03 DELAY",
     "CHANNEL 04 DELAY", "DELAY UNIT", "DELAY MUTE", "ALIGN GROUP", "PRESET"},
    {"INPUT 01 - 32", "AUX 01 - 16", "FX RETURNS", "GROUPS",
     "MATRIX", "OUTPUTS", "PPM PEAK", "RMS LEVEL"},
    {"FX SLOT 01", "FX SLOT 02", "FX SLOT 03", "FX SLOT 04",
     "FX RETURN LEVEL", "FX SEND LEVEL", "BYPASS", "TAP TEMPO"},
    {"CARD SLOT 1", "CARD SLOT 2", "CARD FORMAT", "SAMPLE RATE",
     "CLOCK SOURCE", "WORD CLOCK", "RECORD ARM", "PLAYBACK"},
    {"MACRO 01", "MACRO 02", "MACRO 03", "MACRO 04",
     "MACRO 05", "MACRO 06", "MACRO 07", "MACRO 08"},
    {"SCENE 01", "SCENE 02", "SCENE 03", "SCENE 04",
     "SAFE SETUP", "SAFE FADER MAP", "SAFE ROUTING", "SAFE MONITOR"},
    {"INPUT FORMAT", "AUX FORMAT", "FX RETURN FORMAT", "GROUP FORMAT",
     "OUTPUT FORMAT", "INTERNAL DEST", "HIDE UNUSED", "SHOW SUMMARY"},
    {"FIRMWARE", "DEVICE NAME", "IP ADDRESS", "NET MASK",
     "GATEWAY", "SURFACE LAYOUT", "ENCODER MODE", "RESTART"}
};

const char* PageShowConfig::SOFT_LABELS[14][2] = {
    {"SEL", "select"}, {"MUTE", "channel"}, {"SOLO", "channel"},
    {"<", "bank"}, {">", "bank"}, {"EQ", "view"}, {"DYN", "view"},
    {"AUX", "sends"}, {"MAP", "fader bank"}, {"MON", "listen"},
    {"MAC", "macro"}, {"DLY", "align"}, {"CFG", "show setup"},
    {"HOME", "overview"}
};

const char* PageShowConfig::BANK_NAMES[12] = {
    "IN 1-8 A", "IN 9-16 A", "IN 17-24 A", "IN 25-32 A",
    "IN 1-8 B", "IN 9-16 B", "IN 17-24 B", "IN 25-32 B",
    "OUT 1-4", "OUT 5-8", "OUT 9-12", "OUT 13-16"
};

const char* PageShowConfig::LIST_PREFIX[5] = {"IN", "AUX", "FX", "BUS", "OUT"};

const int PageShowConfig::SOFT_TAB[14] = {
    0, 1, 2, -1, -1, 2, 3, 4, 5, 6, 12, 8, 14, 0
};

}
