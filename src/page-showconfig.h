#pragma once
#include "page.h"

using namespace std;
using enum MP_ID;

namespace OMC
{

/**
 * SHOW CONFIG / FADER MAP - full-screen port of the browser prototype.
 *
 * The 800x480 overlay on lv_layer_top() reproduces the index.html chrome
 * (top bar, 16 tabs, left soft keys, 12-page bottom bar) and all 16 page
 * bodies with panels, knobs, faders and matrices.
 * Only ASCII is used: the bundled Montserrat fonts have no arrows/bullets.
 */
class PageShowConfig : public Page
{
    private:
        static lv_obj_t* overlay;
        static PageShowConfig* self;

        static constexpr uint32_t C_BG      = 0x0d0e10;
        static constexpr uint32_t C_PANEL   = 0x191b1e;
        static constexpr uint32_t C_PANEL2  = 0x22252a;
        static constexpr uint32_t C_PANEL3  = 0x2c3036;
        static constexpr uint32_t C_LINE    = 0x353a41;
        static constexpr uint32_t C_TXT     = 0xd9dde1;
        static constexpr uint32_t C_TXT_HI  = 0xffffff;
        static constexpr uint32_t C_DIM     = 0x7d858d;
        static constexpr uint32_t C_DIMMER  = 0x545b62;
        static constexpr uint32_t C_AMBER   = 0xff9d00;
        static constexpr uint32_t C_AMBER_S = 0xffc45e;
        static constexpr uint32_t C_AMBER_D = 0x8a5600;
        static constexpr uint32_t C_GREEN   = 0x65d472;
        static constexpr uint32_t C_YELLOW  = 0xffd23f;
        static constexpr uint32_t C_RED     = 0xff5142;
        static constexpr uint32_t C_CYAN    = 0x4aa8d8;
        static constexpr uint32_t C_VIOLET  = 0xb98cff;

        int activeTab = 5;
        int activeBank = 0;
        int selectedSlot = 0;
        int activeList = 0;
        bool customBank = false;

        lv_obj_t* body = nullptr;
        lv_obj_t* headNote = nullptr;
        lv_obj_t* bankBtns[12] = {};
        lv_obj_t* slotCells[12] = {};
        lv_obj_t* slotNames[12] = {};
        lv_obj_t* listLabels[24] = {};
        lv_obj_t* listBtns[5] = {};
        lv_obj_t* tabBtns[16] = {};
        lv_obj_t* softBtns[14] = {};
        lv_obj_t* bbSel = nullptr;
        lv_obj_t* bbMs = nullptr;
        lv_obj_t* bbFader = nullptr;
        lv_obj_t* bbView = nullptr;
        lv_obj_t* bbExtra = nullptr;
        lv_obj_t* bbMsg = nullptr;
        char slotLabel[12][12][16] = {};
        char statusMsg[96] = {};
        bool toggles[200] = {};

        static const char* TAB_LABELS[16];
        static const char* PAGE_TITLES[16];
        static const char* PAGE_HEAD_NOTE[16];
        static const char* PAGE_BB_EXTRA[16];
        static const char* SOFT_LABELS[14][2];
        static const char* BANK_NAMES[12];
        static const char* LIST_PREFIX[5];
        static const int SOFT_TAB[14];

        // ---- primitives ----------------------------------------------------
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
            lv_obj_set_style_text_font(l, font ? font : &lv_font_montserrat_10, LV_PART_MAIN);
            lv_obj_set_style_text_align(l, align, LV_PART_MAIN);
            return l;
        }

        static lv_obj_t* Button(lv_obj_t* parent, int x, int y, int w, int h,
                                const char* text, int code)
        {
            lv_obj_t* b = lv_btn_create(parent);
            lv_obj_set_pos(b, x, y);
            lv_obj_set_size(b, w, h);
            lv_obj_set_style_bg_color(b, lv_color_hex(C_PANEL2), LV_PART_MAIN);
            lv_obj_set_style_border_color(b, lv_color_hex(C_LINE), LV_PART_MAIN);
            lv_obj_set_style_border_width(b, 1, LV_PART_MAIN);
            lv_obj_set_style_pad_all(b, 0, LV_PART_MAIN);
            lv_obj_set_style_radius(b, 0, LV_PART_MAIN);
            lv_obj_t* l = lv_label_create(b);
            lv_obj_center(l);
            lv_label_set_long_mode(l, LV_LABEL_LONG_CLIP);
            lv_label_set_text(l, text);
            lv_obj_set_style_text_font(l, &lv_font_montserrat_10, LV_PART_MAIN);
            lv_obj_set_style_text_color(l, lv_color_hex(C_TXT), LV_PART_MAIN);
            if (code >= 0) lv_obj_add_event_cb(b, Clicked, LV_EVENT_CLICKED, (void*)(intptr_t)code);
            return b;
        }

        static lv_obj_t* Panel(lv_obj_t* parent, int x, int y, int w, int h,
                               const char* title, const char* accent = nullptr)
        {
            lv_obj_t* p = Box(parent, x, y, w, h, C_PANEL, C_LINE, 1);
            int aw = w < 150 ? 70 : (w < 250 ? 100 : 174);
            Label(p, 6, 4, w - aw - 12, title, C_DIM);
            if (accent) Label(p, w - aw - 6, 4, aw, accent, C_AMBER, nullptr, LV_TEXT_ALIGN_RIGHT);
            return p;
        }

        static void BtnLook(lv_obj_t* b, bool on, bool amber = false)
        {
            lv_obj_set_style_bg_color(b, lv_color_hex(on ? (amber ? C_AMBER : C_PANEL3) : C_PANEL2), LV_PART_MAIN);
            lv_obj_set_style_border_color(b, lv_color_hex(on ? C_AMBER : C_LINE), LV_PART_MAIN);
            lv_obj_set_style_text_color(lv_obj_get_child(b, 0), lv_color_hex(on ? (amber ? 0x151515 : C_TXT_HI) : C_DIM), LV_PART_MAIN);
        }

        static lv_obj_t* ClickBox(lv_obj_t* parent, int x, int y, int w, int h, int code)
        {
            lv_obj_t* o = Box(parent, x, y, w, h, 0x16191c, 0x30363c, 1);
            lv_obj_add_flag(o, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_event_cb(o, Clicked, LV_EVENT_CLICKED, (void*)(intptr_t)code);
            return o;
        }

        static lv_obj_t* Knob(lv_obj_t* parent, int x, int y, int d,
                              const char* label, const char* val, uint32_t color = C_AMBER)
        {
            lv_obj_t* c = lv_obj_create(parent);
            lv_obj_set_pos(c, x, y);
            lv_obj_set_size(c, d, d + 30);
            lv_obj_set_style_bg_opa(c, LV_OPA_TRANSP, LV_PART_MAIN);
            lv_obj_set_style_border_width(c, 0, LV_PART_MAIN);
            lv_obj_set_style_pad_all(c, 0, LV_PART_MAIN);
            lv_obj_clear_flag(c, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_t* k = lv_obj_create(c);
            lv_obj_set_pos(k, 0, 0);
            lv_obj_set_size(k, d, d);
            lv_obj_set_style_bg_color(k, lv_color_hex(0x23272c), LV_PART_MAIN);
            lv_obj_set_style_border_color(k, lv_color_hex(0x4d545b), LV_PART_MAIN);
            lv_obj_set_style_border_width(k, 2, LV_PART_MAIN);
            lv_obj_set_style_radius(k, d / 2, LV_PART_MAIN);
            Box(k, d / 2 - 1, 3, 2, d / 3, color, color, 0);
            Label(c, 0, d + 2, d, label, C_DIM, nullptr, LV_TEXT_ALIGN_CENTER);
            Label(c, 0, d + 15, d, val, C_AMBER_S, nullptr, LV_TEXT_ALIGN_CENTER);
            return c;
        }

        static lv_obj_t* Fader(lv_obj_t* parent, int x, int y, int h, int level, bool stereo)
        {
            lv_obj_t* c = lv_obj_create(parent);
            lv_obj_set_pos(c, x, y);
            lv_obj_set_size(c, stereo ? 54 : 36, h);
            lv_obj_set_style_bg_opa(c, LV_OPA_TRANSP, LV_PART_MAIN);
            lv_obj_set_style_border_width(c, 0, LV_PART_MAIN);
            lv_obj_set_style_pad_all(c, 0, LV_PART_MAIN);
            lv_obj_clear_flag(c, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_t* tr = Box(c, 12, 0, 9, h, 0x08090a, 0x3d434a, 1);
            int fh = (h - 4) * level / 100;
            Box(tr, 1, h - 3 - fh, 7, fh, C_AMBER, C_AMBER, 0);
            int capY = (h - 13) * (100 - level) / 100;
            Box(c, 1, capY, 30, 13, 0xc9ced4, 0x15181b, 1);
            lv_obj_t* m = Box(c, 28, 0, 7, h, 0x08090a, 0x30363c, 1);
            Box(m, 1, h - 3 - fh, 5, fh, C_GREEN, C_GREEN, 0);
            if (stereo)
            {
                lv_obj_t* m2 = Box(c, 40, 0, 7, h, 0x08090a, 0x30363c, 1);
                Box(m2, 1, h - 3 - fh, 5, fh, C_GREEN, C_GREEN, 0);
            }
            return c;
        }

        static lv_obj_t* Graph(lv_obj_t* parent, int x, int y, int w, int h,
                               const float* coords, int n, uint32_t color)
        {
            lv_obj_t* g = Box(parent, x, y, w, h, 0x0b0c0d, 0x2d3238, 1);
            for (int i = 1; i < 4; ++i) Box(g, 0, i * h / 4, w, 1, 0x2a2f35, 0x2a2f35, 0);
            for (int i = 1; i < 8; ++i) Box(g, i * w / 8, 0, 1, h, 0x2a2f35, 0x2a2f35, 0);
            lv_point_precise_t* pts = new lv_point_precise_t[n];
            for (int i = 0; i < n; ++i)
            {
                pts[i].x = coords[2 * i] * (w - 2);
                pts[i].y = coords[2 * i + 1] * (h - 2);
            }
            lv_obj_t* line = lv_line_create(g);
            lv_line_set_points(line, pts, n);
            lv_obj_set_style_line_color(line, lv_color_hex(color), LV_PART_MAIN);
            lv_obj_set_style_line_width(line, 2, LV_PART_MAIN);
            return g;
        }

        static void Bar(lv_obj_t* parent, int x, int y, int w, int h, int pct, uint32_t color)
        {
            Box(parent, x, y, w, h, 0x090a0b, 0x2c3137, 1);
            Box(parent, x + 1, y + 1, (w - 2) * pct / 100, h - 2, color, color, 0);
        }

        // ---- click handling ------------------------------------------------
        // 0..11 bank, 100..111 slot, 200..223 source, 300..302 action,
        // 400..404 source list, 500..513 soft key, 600..615 tab,
        // 700..707 row toggle, 800..999 cell toggle, 1000+ action button.
        static void Clicked(lv_event_t* e)
        {
            if (!self) return;
            self->HandleClick((int)(intptr_t)lv_event_get_user_data(e),
                              (lv_obj_t*)lv_event_get_target(e));
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

            lv_obj_clean(body);
            headNote = nullptr;
            bankBtns[0] = slotCells[0] = slotNames[0] = listLabels[0] = listBtns[0] = nullptr;
            BuildPage(activeTab);
            snprintf(statusMsg, sizeof(statusMsg), "TAB %s", TAB_LABELS[activeTab]);
            Refresh();
        }

        void HandleClick(int code, lv_obj_t* target = nullptr)
        {
            if (code >= 600 && code < 616) { SwitchTab(code - 600); return; }
            if (code >= 700 && code < 708)
            {
                int r = code - 700;
                toggles[r] = !toggles[r];
                char buf[64];
                snprintf(buf, sizeof(buf), "ROW %d %s", r + 1, toggles[r] ? "ON" : "OFF");
                Note(buf);
                return;
            }
            if (code >= 800 && code < 1000)
            {
                int i = code - 800;
                toggles[i] = !toggles[i];
                if (target)
                {
                    lv_obj_set_style_bg_color(target, lv_color_hex(toggles[i] ? C_AMBER_D : 0x16191c), LV_PART_MAIN);
                    lv_obj_set_style_border_color(target, lv_color_hex(toggles[i] ? C_AMBER : 0x30363c), LV_PART_MAIN);
                }
                char buf[40];
                snprintf(buf, sizeof(buf), "CELL %d %s", i + 1, toggles[i] ? "ON" : "OFF");
                Note(buf);
                return;
            }
            if (code >= 1000)
            {
                char buf[40];
                snprintf(buf, sizeof(buf), "ACTION %d", code - 1000);
                Note(buf);
                return;
            }
            if (activeTab != 5) { Note("Use the tabs to change page"); return; }

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
                snprintf(slotLabel[activeBank][selectedSlot], 16, "%s %02d", LIST_PREFIX[activeList], src + 1);
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
                    for (int i = 0; i < 12; ++i) snprintf(slotLabel[activeBank][i], 16, "CH %02d", i + 1);
                    Note("Straight 1:1 mapping applied");
                }
                else { customBank = false; Note("Custom bank disabled"); }
            }
            else if (code >= 400 && code < 405)
            {
                activeList = code - 400;
                for (int i = 0; i < 24; ++i)
                    lv_label_set_text_fmt(listLabels[i], "%s %02d", LIST_PREFIX[activeList], i + 1);
                for (int i = 0; i < 4; ++i) BtnLook(listBtns[i], i == activeList);
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
                else if (k == 13) config->Set(ACTIVE_PAGE, (uint)X32_PAGE::HOME);
                else if (SOFT_TAB[k] >= 0) SwitchTab(SOFT_TAB[k]);
                else Note(SOFT_LABELS[k][0]);
            }
        }

        // ---- shared head ---------------------------------------------------
        void PageHead(const char* note)
        {
            Label(body, 0, 1, 360, PAGE_TITLES[activeTab], C_AMBER, &lv_font_montserrat_14);
            headNote = Label(body, 350, 3, 378, note, C_DIM, nullptr, LV_TEXT_ALIGN_RIGHT);
        }

        // ---- page bodies ---------------------------------------------------
        void BuildOverview()
        {
            PageHead(PAGE_HEAD_NOTE[0]);
            static const char* bankLabels[12] = {
                "A01","A02","A03","A04","A05","A06","A07","A08",
                "O01","O02","O03","O04"};
            lv_obj_t* p = Panel(body, 0, 26, 728, 242, "FADER BANKS", "12 FADERS / 12 BANKS");
            for (int i = 0; i < 12; ++i)
            {
                int col = i % 8, row = i / 8;
                lv_obj_t* b = Button(p, 6 + col * 90, 20 + row * 16, 86, 14, bankLabels[i], -1);
                BtnLook(b, i == activeBank);
            }
            Label(p, 585, 20, 137, "A01 / 12 FADER", C_DIM, nullptr, LV_TEXT_ALIGN_RIGHT);
            const char* names[12] = {"KICK","SNARE","HAT","BASS DI","VOX LEAD","VOX 2","GT L","GT R","KEYS L","KEYS R","FX RET","BUS"};
            for (int i = 0; i < 12; ++i)
            {
                int x = 7 + i * 60;
                lv_obj_t* s = Box(p, x, 54, 57, 182, 0x1a1d21, i == 4 ? C_AMBER : 0x3a4047, 1);
                Label(s, 4, 3, 24, "", i == 4 ? C_AMBER : C_DIM);
                lv_label_set_text_fmt(lv_obj_get_child(s, 0), "%02d", i + 1);
                Label(s, 3, 16, 51, names[i], i == 4 ? C_AMBER_S : C_TXT_HI);
                Label(s, 3, 34, 51, "XLR 01", C_DIMMER);
                Box(s, 4, 49, 49, 5, 0x1c2024, 0x1c2024, 0);
                Button(s, 5, 58, 20, 14, "M", 800 + i);
                Button(s, 30, 58, 20, 14, "S", 810 + i);
                Fader(s, 12, 78, 78, 35 + (i * 7) % 55, false);
                Label(s, 4, 158, 49, "-6.3", C_AMBER_S, nullptr, LV_TEXT_ALIGN_CENTER);
                Label(s, 2, 172, 53, "GT EQ CP", C_GREEN, nullptr, LV_TEXT_ALIGN_CENTER);
            }

            lv_obj_t* master = Panel(body, 0, 274, 236, 124, "MASTER", "STEREO L/R");
            Knob(master, 12, 24, 68, "MASTER LEVEL", "-6.3");
            Box(master, 96, 26, 6, 74, 0x08090a, 0x30363c, 1);
            Box(master, 110, 26, 6, 74, 0x08090a, 0x30363c, 1);
            Button(master, 136, 30, 28, 18, "M", 1000);
            Button(master, 136, 54, 28, 18, "S", 1001);

            lv_obj_t* quick = Panel(body, 242, 274, 486, 124, "QUICK CONTROL", "CH 05");
            Knob(quick, 14, 22, 34, "GAIN", "12 dB");
            Knob(quick, 72, 22, 34, "HPF", "80 Hz", C_VIOLET);
            Knob(quick, 130, 22, 34, "PAN", "C", C_CYAN);
            for (int i = 0; i < 4; ++i)
            {
                lv_obj_t* b = Button(quick, 200 + i * 42, 30, 38, 18,
                                     i == 0 ? "PHASE" : i == 1 ? "HPF" : i == 2 ? "GATE" : "COMP", 1010 + i);
                if (i % 2) BtnLook(b, true);
            }
            Label(quick, 380, 26, 96, "INSERT REVERB", C_TXT, nullptr, LV_TEXT_ALIGN_CENTER);
            Label(quick, 380, 48, 96, "SEND 12", C_AMBER, nullptr, LV_TEXT_ALIGN_CENTER);
        }

        void BuildChannel()
        {
            PageHead(PAGE_HEAD_NOTE[1]);
            lv_obj_t* main = Panel(body, 0, 26, 572, 250, "CHANNEL 05 / BASS DI", "4 PROCESSORS");
            const char* titles[4] = {"INPUT","FILTER","GATE","COMPRESSOR"};
            const char* accents[4] = {"XLR 05","HPF / LPF","ACTIVE","ACTIVE"};
            for (int i = 0; i < 4; ++i)
            {
                int x = 6 + (i % 2) * 284, y = 22 + (i / 2) * 114;
                lv_obj_t* b = Panel(main, x, y, 278, 108, titles[i], accents[i]);
                if (i < 2)
                {
                    Knob(b, 20, 26, 34, i == 0 ? "GAIN" : "HPF FREQ", i == 0 ? "12 dB" : "80 Hz");
                    Knob(b, 100, 26, 34, i == 0 ? "PAN" : "LPF", i == 0 ? "C" : "18k", C_CYAN);
                }
                else
                {
                    Knob(b, 12, 26, 34, "THRESH", i == 2 ? "-42 dB" : "-18 dB");
                    Knob(b, 82, 26, 34, i == 2 ? "ATTACK" : "RATIO", i == 2 ? "5 ms" : "4:1");
                    Knob(b, 152, 26, 34, i == 2 ? "RELEASE" : "RELEASE", "120 ms");
                }
                for (int j = 0; j < 3; ++j)
                    Button(b, 190 + j * 28, 30, 26, 14, j == 0 ? "ON" : j == 1 ? "AUTO" : "GR", 1020 + i * 3 + j);
            }
            lv_obj_t* fv = Panel(body, 578, 26, 150, 250, "FADER / VU", "STEREO");
            Fader(fv, 42, 24, 150, 62, true);
            Label(fv, 8, 180, 134, "-6.3", C_AMBER, &lv_font_montserrat_20, LV_TEXT_ALIGN_CENTER);
            Button(fv, 28, 206, 42, 18, "MUTE", 1030);
            Button(fv, 80, 206, 42, 18, "SOLO", 1031);

            lv_obj_t* mode = Panel(body, 0, 282, 728, 76, "MODE / INSERT / CH 05", "STEREO REVERB POST FADER");
            for (int i = 0; i < 4; ++i)
                Button(mode, 10 + i * 34, 26, 32, 16, i == 0 ? "MONO" : i == 1 ? "STEREO" : i == 2 ? "LINK" : "PAIR", 1040 + i);
            for (int i = 0; i < 4; ++i)
                Button(mode, 180 + i * 42, 26, 40, 16, i == 0 ? "OFF" : i == 1 ? "REV" : i == 2 ? "CHR" : "DLY", 1050 + i);
            for (int i = 0; i < 4; ++i)
                Button(mode, 380 + i * 52, 26, 50, 16, i == 0 ? "PRE EQ" : i == 1 ? "POST EQ" : i == 2 ? "PRE FDR" : "POST FDR", 1060 + i);
            Label(mode, 600, 28, 120, "VU L/R LINK", C_DIM);

            lv_obj_t* bank = Panel(body, 0, 364, 728, 34, "CHANNEL BANK", "A01 / 12 FADER");
            for (int i = 0; i < 12; ++i)
            {
                char num[4];
                snprintf(num, sizeof(num), "%02d", i + 1);
                lv_obj_t* b = Button(bank, 160 + i * 46, 8, 43, 16, num, -1);
                BtnLook(b, i == 4);
            }
        }

        void BuildEQ()
        {
            PageHead(PAGE_HEAD_NOTE[2]);
            static const float curve[] = {0,0.25f, 0.08f,0.38f, 0.22f,0.55f, 0.42f,0.42f,
                                          0.62f,0.30f, 0.82f,0.20f, 1.0f,0.33f};
            lv_obj_t* g = Panel(body, 0, 26, 610, 196, "EQ RESPONSE / CH 05 BASS DI", "+/-12 dB");
            Graph(g, 8, 24, 594, 160, curve, 7, C_AMBER);
            lv_obj_t* sp = Panel(body, 616, 26, 112, 196, "SPECTRUM", "RTA");
            for (int i = 0; i < 20; ++i)
            {
                int h = 12 + (i * 37) % 130;
                Box(sp, 8 + i * 5, 164 - h, 3, h, (i % 5 == 0) ? C_RED : C_GREEN, (i % 5 == 0) ? C_RED : C_GREEN, 0);
            }
            const char* modes[6] = {"LCUT","LSHF","PEQ","VEQ","HSHF","HCUT"};
            const char* vals[4][3] = {{"120Hz","-3dB","Q1.0"},{"420Hz","-2dB","Q1.4"},
                                      {"2.4k","+2dB","Q1.2"},{"9.0k","+3.5dB","Q0.8"}};
            const char* names[4] = {"LOW SHELF","PEAK","PEAK","HIGH SHELF"};
            for (int b = 0; b < 4; ++b)
            {
                lv_obj_t* band = Panel(body, b * 183, 228, 178, 128, "", nullptr);
                Label(band, 6, 3, 90, "", C_DIM);
                lv_label_set_text_fmt(lv_obj_get_child(band, 0), "BAND %d", b + 1);
                Label(band, 88, 3, 84, names[b], C_AMBER, nullptr, LV_TEXT_ALIGN_RIGHT);
                for (int m = 0; m < 6; ++m)
                    Button(band, 5 + m * 28, 17, 27, 12, modes[m], 900 + b * 6 + m);
                Knob(band, 10, 33, 28, "FREQ", vals[b][0]);
                Knob(band, 60, 33, 28, "GAIN", vals[b][1]);
                Knob(band, 110, 33, 28, "Q", vals[b][2]);
                Button(band, 10, 106, 158, 14, "SELECT BAND", 1100 + b);
            }
            lv_obj_t* lc = Panel(body, 0, 362, 728, 36, "LOW CUT", "MODE PER BAND / DRAG NODES");
            Knob(lc, 12, 4, 24, "", "80 Hz");
            Button(lc, 62, 10, 70, 16, "LOW CUT ON", 1110);
        }

        void BuildDyn()
        {
            PageHead(PAGE_HEAD_NOTE[3]);
            static const float gate[] = {0,0.95f, 0.35f,0.90f, 0.5f,0.55f, 0.7f,0.12f, 1,0.1f};
            static const float comp[] = {0,0.05f, 0.3f,0.08f, 0.6f,0.42f, 0.8f,0.65f, 1,0.8f};
            lv_obj_t* gp = Panel(body, 0, 26, 361, 372, "DYNAMICS - GATE", "GATE ACTIVE");
            Graph(gp, 8, 24, 345, 120, gate, 5, C_AMBER);
            for (int i = 0; i < 3; ++i) Button(gp, 8 + i * 56, 152, 54, 16, i == 0 ? "GATE" : i == 1 ? "EXPAND" : "DUCK", 1120 + i);
            Knob(gp, 20, 182, 34, "THRESH", "-42 dB");
            Knob(gp, 100, 182, 34, "ATTACK", "5 ms");
            Knob(gp, 180, 182, 34, "RELEASE", "120 ms");
            Button(gp, 270, 190, 70, 18, "GATE ON", 1130);
            Label(gp, 8, 300, 340, "RANGE -60 dB", C_DIM);
            lv_obj_t* cp = Panel(body, 367, 26, 361, 372, "COMPRESSOR", "VCA ACTIVE");
            Graph(cp, 8, 24, 345, 120, comp, 5, C_AMBER);
            for (int i = 0; i < 4; ++i) Button(cp, 8 + i * 44, 152, 42, 16, i == 0 ? "VCA" : i == 1 ? "FET" : i == 2 ? "OPT" : "TUBE", 1140 + i);
            Knob(cp, 12, 182, 34, "THRESH", "-18 dB");
            Knob(cp, 82, 182, 34, "RATIO", "4:1");
            Knob(cp, 152, 182, 34, "ATTACK", "10 ms");
            Knob(cp, 222, 182, 34, "RELEASE", "140 ms");
            Button(cp, 285, 190, 60, 18, "COMP ON", 1150);
            Label(cp, 8, 300, 340, "GR -3.2 dB", C_AMBER);
        }

        void BuildAux()
        {
            PageHead(PAGE_HEAD_NOTE[4]);
            lv_obj_t* bus = Panel(body, 0, 26, 728, 34, "AUX BUS", "BUS ON / POST");
            for (int i = 0; i < 8; ++i)
            {
                char num[4];
                snprintf(num, sizeof(num), "%02d", i + 1);
                lv_obj_t* b = Button(bus, 120 + i * 34, 8, 32, 16, num, -1);
                BtnLook(b, i == 0);
            }
            Label(bus, 410, 10, 300, "BUS / MASTER + INTERNAL + EXTERNAL MAP", C_DIM, nullptr, LV_TEXT_ALIGN_RIGHT);
            lv_obj_t* sf = Panel(body, 0, 66, 582, 174, "SEND FADERS / A01", "AUX 01");
            for (int i = 0; i < 12; ++i)
            {
                int x = 8 + i * 47;
                Box(sf, x, 20, 44, 148, 0x1a1d21, 0x3a4047, 1);
                Label(sf, x + 4, 22, 36, "", C_DIM);
                lv_label_set_text_fmt(lv_obj_get_child(sf, lv_obj_get_child_cnt(sf) - 1), "%02d", i + 1);
                Fader(sf, x + 4, 40, 90, 30 + (i * 11) % 60, false);
            }
            lv_obj_t* det = Panel(body, 588, 66, 140, 174, "SEND DETAIL", "CH 05");
            Fader(det, 48, 24, 90, 55, false);
            Label(det, 8, 116, 124, "BASS DI > AUX 01", C_AMBER, nullptr, LV_TEXT_ALIGN_CENTER);
            Button(det, 18, 136, 48, 16, "MUTE", 1160);
            Button(det, 74, 136, 48, 16, "POST", 1161);
            lv_obj_t* bm = Panel(body, 0, 246, 112, 120, "BUS MASTER", "AUX 01");
            Fader(bm, 40, 22, 70, 70, false);
            lv_obj_t* im = Panel(body, 118, 246, 268, 120, "INTERNAL MAP", "CLICK = ON/OFF");
            for (int i = 0; i < 16; ++i)
            {
                int x = 8 + (i % 8) * 32, y = 24 + (i / 8) * 40;
                ClickBox(im, x, y, 29, 36, 800 + i);
                Label(im, x + 4, y + 4, 22, "", C_DIM);
                lv_label_set_text_fmt(lv_obj_get_child(im, lv_obj_get_child_cnt(im) - 1), "%02d", i + 1);
            }
            lv_obj_t* em = Panel(body, 392, 246, 336, 120, "EXTERNAL MAP", "AUX 01 > OUT");
            for (int i = 0; i < 11; ++i)
                Button(em, 6 + (i % 6) * 55, 20 + (i / 6) * 18, 52, 15, i == 0 ? "XLR" : i == 1 ? "AUX" : i == 2 ? "AES A" : "USB", 1170 + i);
            for (int i = 0; i < 8; ++i) ClickBox(em, 6 + i * 41, 74, 38, 38, 830 + i);
            lv_obj_t* ft = Panel(body, 0, 372, 728, 26, "INTERNAL = DSP SENDS / EXTERNAL = PATCHES", "CLEAR SENDS / CLEAR MAP");
        }

        void BuildMonitor()
        {
            PageHead(PAGE_HEAD_NOTE[6]);
            lv_obj_t* src = Panel(body, 0, 26, 508, 336, "MONITOR SOURCE", "LR");
            const char* names[22] = {"LR","A01","A02","A03","A04","A05","A06","A07","A08",
                                     "G01","G02","G03","G04","G05","G06","G07","G08",
                                     "CD1","CD2","USB","FX1","FX2"};
            for (int i = 0; i < 22; ++i)
            {
                lv_obj_t* b = Button(src, 8 + (i % 6) * 82, 24 + (i / 6) * 30, 78, 24, names[i], 1180 + i);
                if (i == 0) BtnLook(b, true);
            }
            Label(src, 8, 150, 490, "SOURCE FROM DSP1 / AUX / MIXBUS / CARD / USB / FX RET", C_DIM);
            lv_obj_t* ls = Panel(body, 514, 26, 214, 336, "LISTEN", "ON");
            Knob(ls, 70, 24, 68, "MON LEVEL", "72%");
            Box(ls, 20, 112, 8, 70, 0x08090a, 0x30363c, 1);
            Box(ls, 36, 112, 8, 70, 0x08090a, 0x30363c, 1);
            Button(ls, 70, 122, 74, 18, "DIM -20", 1200);
            Button(ls, 70, 146, 74, 18, "MONO", 1201);
            Button(ls, 70, 170, 74, 18, "MUTE", 1202);
            for (int i = 0; i < 3; ++i) Button(ls, 8 + i * 68, 270, 64, 18, i == 0 ? "PHON1" : i == 1 ? "PHON2" : "CR SPK", 1210 + i);
            lv_obj_t* ft = Panel(body, 0, 368, 728, 30, "INDEPENDENT LISTENING / DIM -20 / PHONES 1-2 AND CONTROL ROOM", "LR 72%");
        }

        void BuildRouting()
        {
            PageHead(PAGE_HEAD_NOTE[7]);
            lv_obj_t* sg = Panel(body, 0, 26, 728, 34, "SOURCE GROUP", "INPUT");
            const char* srcg[11] = {"INPUT","AUX","AES A","AES B","UNET","CARD1","CARD2","USB","GROUP","FX RET","MTX"};
            for (int i = 0; i < 11; ++i)
            {
                lv_obj_t* b = Button(sg, 110 + i * 55, 8, 52, 16, srcg[i], -1);
                BtnLook(b, i == 0);
            }
            lv_obj_t* dg = Panel(body, 0, 66, 728, 34, "DESTINATION GROUP", "XLR OUT");
            const char* dstg[11] = {"XLR","AUX","AES A","AES B","UNET","CARD1","CARD2","USB","GRP IN","MTX IN","DSP"};
            for (int i = 0; i < 11; ++i) Button(dg, 110 + i * 55, 8, 52, 16, dstg[i], -1);
            lv_obj_t* mx = Panel(body, 0, 106, 728, 258, "ROUTING MATRIX", "SRC / DST");
            Label(mx, 6, 20, 80, "SRC / DST", C_AMBER);
            for (int c = 0; c < 12; ++c) Label(mx, 92 + c * 52, 20, 50, "", C_AMBER, nullptr, LV_TEXT_ALIGN_CENTER);
            for (int r = 0; r < 12; ++r)
            {
                Label(mx, 6, 40 + r * 17, 82, "", C_DIM);
                lv_label_set_text_fmt(lv_obj_get_child(mx, lv_obj_get_child_cnt(mx) - 1), "%02d XLR %02d", r + 1, r + 1);
                for (int c = 0; c < 12; ++c)
                {
                    lv_obj_t* cell = ClickBox(mx, 92 + c * 52, 38 + r * 17, 48, 15, 840 + r * 12 + c);
                    if ((r + c) % 5 == 0)
                    {
                        lv_obj_set_style_bg_color(cell, lv_color_hex(C_AMBER_D), LV_PART_MAIN);
                        lv_obj_set_style_border_color(cell, lv_color_hex(C_AMBER), LV_PART_MAIN);
                    }
                }
            }
            lv_obj_t* ft = Panel(body, 0, 370, 728, 28, "GROUP -> GROUP / GROUP -> AUX / MATRIX -> GROUP", "CLEAR / LOAD PRESET");
        }

        void BuildDelay()
        {
            PageHead(PAGE_HEAD_NOTE[8]);
            lv_obj_t* in = Panel(body, 0, 26, 361, 302, "INPUT DELAY", "M / MS / FRAMES");
            lv_obj_t* out = Panel(body, 367, 26, 361, 302, "OUTPUT DELAY", "M / MS / FRAMES");
            for (int p = 0; p < 2; ++p)
            {
                lv_obj_t* par = p ? out : in;
                for (int i = 0; i < 14; ++i)
                {
                    lv_obj_t* row = ClickBox(par, 6, 22 + i * 19, 349, 17, 1000 + p * 20 + i);
                    Label(row, 4, 2, 150, "", C_DIM);
                    lv_label_set_text_fmt(lv_obj_get_child(row, 0), "%02d CH %02d", i + 1, i + 1);
                    Label(row, 160, 2, 60, "0.00 m", C_DIM);
                    Label(row, 225, 2, 60, "0.0 ms", C_DIM);
                    Label(row, 290, 2, 55, "0.00 f", C_DIM);
                }
            }
            lv_obj_t* ed = Panel(body, 0, 334, 728, 64, "DELAY EDIT", "48 kHz / 343 m/s / 25 FPS");
            Button(ed, 8, 22, 52, 16, "INPUT", 1220);
            Button(ed, 64, 22, 52, 16, "OUTPUT", 1221);
            Knob(ed, 130, 12, 34, "DELAY", "0.0 ms");
            Label(ed, 200, 22, 90, "0.00 m", C_TXT);
            Label(ed, 300, 22, 90, "0.0 ms", C_TXT);
            Label(ed, 400, 22, 90, "0.00 f", C_TXT);
            for (int i = 0; i < 4; ++i) Button(ed, 520 + i * 50, 22, 46, 16, i == 0 ? "0ms" : i == 1 ? "1ms" : i == 2 ? "5ms" : "10ms", 1230 + i);
        }

        void BuildMeter()
        {
            PageHead(PAGE_HEAD_NOTE[9]);
            lv_obj_t* p = Panel(body, 0, 26, 728, 322, "INPUT METERS", "24 IN / 16 OUT / 8 GROUP");
            for (int i = 0; i < 16; ++i)
            {
                char num[8];
                snprintf(num, sizeof(num), "%02d", i + 1);
                Label(p, 8, 22 + i * 18, 30, num, C_DIM);
                Bar(p, 44, 23 + i * 18, 620, 9, 25 + (i * 29) % 70, i % 5 == 0 ? C_YELLOW : C_GREEN);
                Label(p, 672, 22 + i * 18, 44, i % 3 ? "-12.4" : "-INF", C_DIM);
            }
            lv_obj_t* m = Panel(body, 0, 354, 728, 44, "MASTER", "L / R");
            Bar(m, 70, 22, 240, 9, 74, C_GREEN);
            Bar(m, 410, 22, 240, 9, 70, C_GREEN);
        }

        void BuildFX()
        {
            PageHead(PAGE_HEAD_NOTE[10]);
            const char* names[4] = {"REVERB","CHORUS","DELAY","OVERDRIVE"};
            const char* params[4][3] = {{"SIZE","DECAY","MIX"},{"RATE","DEPTH","MIX"},
                                        {"TIME","FEEDBK","MIX"},{"DRIVE","TONE","MIX"}};
            for (int i = 0; i < 4; ++i)
            {
                lv_obj_t* p = Panel(body, (i % 2) * 367, 26 + (i / 2) * 168, 361, 162, "", nullptr);
                Label(p, 8, 4, 180, names[i], C_AMBER, &lv_font_montserrat_14);
                for (int k = 0; k < 3; ++k) Knob(p, 22 + k * 80, 32, 34, params[i][k], "50%");
                Button(p, 260, 40, 70, 18, "ON", 1240 + i);
                Label(p, 8, 108, 344, "POS PRE FADER / POST FADER", C_DIM);
                Label(p, 8, 130, 344, "OUT XLR USB CARD1 CARD2", C_DIM);
            }
            lv_obj_t* ft = Panel(body, 0, 364, 728, 34, "FX RET TO XLR / USB / CARD - DSP2 LOAD 42%", "5.1 UPMIX AVAILABLE");
        }

        void BuildCard()
        {
            PageHead(PAGE_HEAD_NOTE[11]);
            lv_obj_t* s = Panel(body, 0, 26, 200, 372, "SOURCE", "REVERSED CMDS");
            Button(s, 12, 24, 80, 20, "SD CARD", 1250);
            Button(s, 104, 24, 80, 20, "USB", 1251);
            Label(s, 12, 60, 176, "00:74:12", C_AMBER, &lv_font_montserrat_20, LV_TEXT_ALIGN_CENTER);
            for (int i = 0; i < 5; ++i)
                Button(s, 12 + (i % 3) * 60, 110 + (i / 3) * 30, 56, 24,
                       i == 0 ? "<<<" : i == 1 ? "PLAY" : i == 2 ? "STOP" : i == 3 ? "REC" : ">>>", 1260 + i);
            lv_obj_t* t = Panel(body, 206, 26, 522, 372, "SD / TRACKS", "6 FILES");
            const char* tracks[6] = {"01 INTRO / CLICK 00:32 WAV 48k","02 SETLIST / PT 1 04:18 WAV 48k",
                                     "03 SETLIST / PT 2 05:44 WAV 48k","04 ENCORE 03:09 WAV 48k",
                                     "05 BACKING / AMBIENCE 02:51 WAV 48k","06 RECORDING 22-11 18:04 WAV 48k"};
            for (int i = 0; i < 6; ++i)
            {
                lv_obj_t* row = ClickBox(t, 8, 24 + i * 30, 506, 26, 1270 + i);
                Label(row, 6, 6, 494, tracks[i], i == 0 ? C_AMBER : C_TXT);
            }
        }

        void BuildMacro()
        {
            PageHead(PAGE_HEAD_NOTE[12]);
            lv_obj_t* k = Panel(body, 0, 26, 236, 336, "ASSIGNABLE KEYS", "A01-A08");
            for (int i = 0; i < 8; ++i)
                Button(k, 8 + (i % 2) * 112, 24 + (i / 2) * 34, 108, 28, "", 1280 + i);
            Button(k, 8, 300, 52, 22, "REC", 1290);
            Button(k, 66, 300, 52, 22, "PLAY", 1291);
            Button(k, 124, 300, 46, 22, "CLEAR", 1292);
            Button(k, 176, 300, 46, 22, "SAVE", 1293);
            lv_obj_t* s = Panel(body, 242, 26, 486, 336, "SEQUENCE", "A01 IDLE");
            for (int i = 0; i < 14; ++i)
            {
                lv_obj_t* row = ClickBox(s, 8, 24 + i * 21, 470, 18, 1300 + i);
                Label(row, 6, 2, 40, "", C_AMBER);
                lv_label_set_text_fmt(lv_obj_get_child(row, 0), "%02d", i + 1);
                Label(row, 50, 2, 300, i == 0 ? "NO ACTIONS RECORDED" : "STEP", C_DIM);
            }
            lv_obj_t* ft = Panel(body, 0, 368, 728, 30, "SELECT A KEY AND PRESS REC", "REC CAPTURES CLICKS / FADERS / KNOBS");
        }

        void BuildScenes()
        {
            PageHead(PAGE_HEAD_NOTE[13]);
            lv_obj_t* l = Panel(body, 0, 26, 466, 338, "SCENE STRUCTURE / BANK A", "0/32 SCENES");
            for (int i = 0; i < 4; ++i) Button(l, 8 + i * 60, 22, 56, 16, i == 0 ? "BANK A" : i == 1 ? "BANK B" : i == 2 ? "BANK C" : "BANK D", 1310 + i);
            Label(l, 260, 24, 190, "SLOT 01", C_AMBER, nullptr, LV_TEXT_ALIGN_RIGHT);
            for (int i = 0; i < 8; ++i)
            {
                lv_obj_t* row = ClickBox(l, 8, 46 + i * 34, 450, 30, 1320 + i);
                Label(row, 6, 7, 32, "", C_AMBER);
                lv_label_set_text_fmt(lv_obj_get_child(row, 0), "%02d", i + 1);
                Label(row, 44, 7, 240, i == 0 ? "01 INTRO / CLICK" : "-- EMPTY --", i == 0 ? C_TXT_HI : C_DIMMER);
                Label(row, 300, 7, 60, "12:04", C_DIM);
                Label(row, 370, 7, 70, i == 0 ? "ALL" : "SAFE", C_DIM);
            }
            lv_obj_t* e = Panel(body, 472, 26, 256, 338, "SCENE 01", "EMPTY");
            Label(e, 8, 24, 80, "NAME", C_DIM);
            Box(e, 8, 40, 240, 24, 0x101214, 0x4a515a, 1);
            for (int i = 0; i < 8; ++i)
                Button(e, 8 + (i % 4) * 61, 72 + (i / 4) * 26, 58, 22,
                       i == 0 ? "STORE" : i == 1 ? "RECALL" : i == 2 ? "COPY" : i == 3 ? "PASTE" :
                       i == 4 ? "PREV" : i == 5 ? "NEXT" : i == 6 ? "DELETE" : "AUTO", 1340 + i);
            Label(e, 8, 132, 240, "RECALL SCOPE / SAFE", C_AMBER);
            const char* scopes[8] = {"SETUP","FADER MAP","INPUT/CH","AUX","ROUTING","OUT/FX/CLK","MONITOR","MACRO"};
            for (int i = 0; i < 8; ++i)
            {
                lv_obj_t* b = Button(e, 8 + (i % 2) * 122, 152 + (i / 2) * 22, 118, 19, scopes[i], 800 + i);
                if (i % 3 == 0) BtnLook(b, true);
            }
            lv_obj_t* ft = Panel(body, 0, 370, 728, 28, "STRUCTURE: SHOW > 4 BANKS x 8 SCENES > 32 SLOTS", "PER-GROUP SAFE");
        }

        void BuildSetup()
        {
            PageHead(PAGE_HEAD_NOTE[14]);
            lv_obj_t* l = Panel(body, 0, 26, 186, 338, "QUANTITY", "DEFAULT");
            const char* rows[6] = {"INPUT","AUX","FX","GROUP","OUTPUT","MATRIX"};
            const char* counts[6] = {"24","8","4","8","16","6"};
            for (int i = 0; i < 6; ++i)
            {
                int y = 26 + i * 25;
                Box(l, 6, y, 174, 22, 0x1c1f23, 0x2d3238, 1);
                Label(l, 10, y + 5, 70, rows[i], C_TXT);
                Button(l, 88, y + 3, 16, 16, "-", 1350 + i);
                Label(l, 110, y + 4, 30, counts[i], C_AMBER, nullptr, LV_TEXT_ALIGN_CENTER);
                Button(l, 148, y + 3, 16, 16, "+", 1360 + i);
            }
            Button(l, 6, 182, 174, 20, "HIDE UNUSED ON", 1370);
            const char* presets[5] = {"DEFAULT","BAND","CORO","THEATRE","MIN"};
            for (int i = 0; i < 5; ++i) Button(l, 6 + (i % 2) * 88, 212 + (i / 2) * 24, 84, 20, presets[i], 1380 + i);
            Label(l, 6, 292, 174, "SETUP DEFINES WHAT EXISTS.", C_DIM);
            lv_obj_t* c = Panel(body, 192, 26, 308, 338, "INPUT CHANNELS / MONO-STEREO", "24 ACTIVE");
            for (int i = 0; i < 24; ++i)
            {
                int x = 8 + (i % 6) * 49, y = 24 + (i / 6) * 27;
                ClickBox(c, x, y, 46, 24, 860 + i);
                Label(c, x + 3, y + 6, 40, "", C_DIM);
                lv_label_set_text_fmt(lv_obj_get_child(c, lv_obj_get_child_cnt(c) - 1), "%02d M S x", i + 1);
            }
            Label(c, 8, 140, 292, "AUX FORMAT / MONO-STEREO - 8 BUS", C_AMBER);
            for (int i = 0; i < 8; ++i)
            {
                int x = 8 + i * 36;
                ClickBox(c, x, 160, 33, 24, 890 + i);
                Label(c, x + 4, 167, 26, "", C_DIM);
                lv_label_set_text_fmt(lv_obj_get_child(c, lv_obj_get_child_cnt(c) - 1), "%02d", i + 1);
            }
            lv_obj_t* r = Panel(body, 506, 26, 222, 338, "GROUP / DESTINATION", "INTERNAL ROUTING");
            for (int i = 0; i < 8; ++i)
            {
                Box(r, 6, 24 + i * 21, 210, 19, 0x1c1f23, 0x2d3238, 1);
                Label(r, 10, 27 + i * 21, 42, "", C_DIM);
                lv_label_set_text_fmt(lv_obj_get_child(r, lv_obj_get_child_cnt(r) - 1), "G%02d", i + 1);
                Button(r, 60, 25 + i * 21, 26, 15, "LR", 1390 + i);
                Button(r, 92, 25 + i * 21, 26, 15, "M", 1400 + i);
            }
            Label(r, 6, 200, 210, "FX MIX / PRE-POST FADER", C_AMBER);
            for (int i = 0; i < 4; ++i) Button(r, 6 + i * 52, 220, 49, 18, i == 0 ? "FX1" : i == 1 ? "FX2" : i == 2 ? "FX3" : "FX4", 1410 + i);
            lv_obj_t* ft = Panel(body, 0, 370, 728, 28, "SHOW CONFIG / INPUT / AUX / FX / GROUP / OUTPUT", "HIDE UNUSED");
        }

        void BuildMap()
        {
            PageHead(PAGE_HEAD_NOTE[5]);
            lv_obj_t* bp = Panel(body, 0, 26, 728, 96, "FADER BANKS", "12 BANKS / 12 SLOTS");
            Label(bp, 6, 20, 280, "INPUT BANKS - 2 ROWS x 4", C_DIM);
            for (int i = 0; i < 8; ++i)
            {
                int col = i % 4, row = i / 4;
                bankBtns[i] = Button(bp, 6 + col * 74, 34 + row * 16, 70, 14, BANK_NAMES[i], i);
                BtnLook(bankBtns[i], i == activeBank);
            }
            Label(bp, 6, 67, 280, "OUTPUT BANKS - 1 ROW x 4", C_DIM);
            for (int i = 8; i < 12; ++i)
            {
                bankBtns[i] = Button(bp, 6 + (i - 8) * 74, 82, 70, 12, BANK_NAMES[i], i);
                BtnLook(bankBtns[i], i == activeBank);
            }
            Label(bp, 310, 22, 410, "BANK A01 / SLOT 01 / CUSTOM BANK ON",
                  C_DIM, nullptr, LV_TEXT_ALIGN_RIGHT);
            lv_obj_t* lp = Panel(body, 0, 128, 302, 238, "FADER BANK / SELECT SLOT", "SEL SLOT > SRC");
            for (int i = 0; i < 12; ++i)
            {
                int col = i % 3, row = i / 3;
                lv_obj_t* c = ClickBox(lp, 6 + col * 97, 20 + row * 46, 93, 42, 100 + i);
                Label(c, 4, 2, 26, "", C_AMBER);
                lv_label_set_text_fmt(lv_obj_get_child(c, 0), "%02d", i + 1);
                slotNames[i] = Label(c, 4, 16, 84, "CH", C_TXT_HI);
                Label(c, 4, 30, 84, "XLR 01", C_DIMMER);
                slotCells[i] = c;
            }
            for (int i = 0; i < 3; ++i)
                Button(lp, 6 + i * 98, 212, 94, 20, i == 0 ? "FREE ALL" : i == 1 ? "STRAIGHT" : "CUSTOM OFF", 300 + i);
            lv_obj_t* rp = Panel(body, 308, 128, 420, 238, "SOURCES / DESTINATIONS", "CLICK = ASSIGN");
            const char* lt[4] = {"INPUT","FX RET","GROUP","OUT"};
            for (int i = 0; i < 4; ++i)
            {
                listBtns[i] = Button(rp, 6 + i * 102, 20, 99, 20, lt[i], 400 + i);
                BtnLook(listBtns[i], i == activeList);
            }
            for (int i = 4; i < 5; ++i) listBtns[i] = nullptr;
            for (int i = 0; i < 24; ++i)
            {
                int col = i % 4, row = i / 4;
                lv_obj_t* c = ClickBox(rp, 6 + col * 102, 46 + row * 24, 98, 21, 200 + i);
                listLabels[i] = Label(c, 5, 4, 88, "", C_DIM);
                lv_label_set_text_fmt(listLabels[i], "%s %02d", LIST_PREFIX[activeList], i + 1);
            }
            lv_obj_t* ft = Panel(body, 0, 372, 728, 26, "INPUT 2x4 + OUTPUT 1x4 / SAVED IN SCENE", "FREE ALL > SEL BANK > SEL SLOT > SEL IN/OUT");
        }

        void BuildAbout()
        {
            PageHead(PAGE_HEAD_NOTE[15]);
            lv_obj_t* clk = Panel(body, 0, 26, 728, 70, "CLOCK", "LOCKED / INTERNAL");
            Button(clk, 100, 22, 60, 18, "44.1k", 1420);
            Button(clk, 166, 22, 60, 18, "48k", 1421);
            for (int i = 0; i < 5; ++i) Button(clk, 260 + i * 42, 22, 38, 18, i == 0 ? "24" : i == 1 ? "25" : i == 2 ? "30" : i == 3 ? "50" : "60", 1430 + i);
            const char* srcs[6] = {"INTERNAL","EXTERNAL","AES A","AES B","CARD1","CARD2"};
            for (int i = 0; i < 6; ++i) Button(clk, 100 + i * 82, 46, 78, 16, srcs[i], 1440 + i);
            Button(body, 0, 102, 728, 20, "SHOW CONFIG / INPUT / AUX / FX / GROUP / MATRIX", 600 + 14);
            lv_obj_t* g1 = Panel(body, 0, 128, 361, 210, "SYSTEM", "BETA");
            const char* kv[13][2] = {{"KERNEL","Linux 6.18 LTS"},{"DISPLAY","800x480 / 30 FPS"},
                {"UI","LVGL 9.5.0"},{"CPU","i.MX25 ARM926"},{"DSP","2x SHARC"},{"FPGA","Spartan-3A"},
                {"AUDIO","32 IN / 16 OUT"},{"CLOCK","48 kHz INTERNAL"},{"VIDEO","25 fps / 40 ms"},
                {"SHOW","DEFAULT 24/8/8/6/16"},{"FADER MAP","CUSTOM A01"},{"BUILD","prototype 0.3"},
                {"NOTE","NO PROTECTED ASSETS"}};
            for (int i = 0; i < 13; ++i)
            {
                Label(g1, 8, 22 + i * 14, 90, kv[i][0], C_DIM);
                Label(g1, 104, 22 + i * 14, 250, kv[i][1], C_TXT);
            }
            lv_obj_t* g2 = Panel(body, 367, 128, 361, 210, "M32 SURFACE MAPPING", "PLANNED");
            const char* map[8][2] = {{"25 FADERS","12 logic faders + banks"},{"ENCODER","scroll / select"},
                {"SOFT KEYS","page + mute / solo"},{"CH SELECT","select / assign"},{"AUX SENDS","8 bus / pre / post"},
                {"TOUCH","drag nodes / faders"},{"AES50","stagebox / third party"},{"SCENE","store / recall"}};
            for (int i = 0; i < 8; ++i)
            {
                Label(g2, 8, 24 + i * 22, 100, map[i][0], C_AMBER);
                Label(g2, 112, 24 + i * 22, 240, map[i][1], C_TXT);
            }
            lv_obj_t* rm = Panel(body, 0, 344, 728, 54, "ROADMAP", "NEXT");
            Label(rm, 8, 24, 710, "0 UI SPEC -> 1 EEZ STUDIO -> 2 LVGL BINDINGS -> 3 USB BOOT -> 4 DSP PARITY", C_DIM);
        }

        void BuildPage(int tab)
        {
            switch (tab)
            {
                case 0:  BuildOverview(); break;
                case 1:  BuildChannel(); break;
                case 2:  BuildEQ(); break;
                case 3:  BuildDyn(); break;
                case 4:  BuildAux(); break;
                case 5:  BuildMap(); break;
                case 6:  BuildMonitor(); break;
                case 7:  BuildRouting(); break;
                case 8:  BuildDelay(); break;
                case 9:  BuildMeter(); break;
                case 10: BuildFX(); break;
                case 11: BuildCard(); break;
                case 12: BuildMacro(); break;
                case 13: BuildScenes(); break;
                case 14: BuildSetup(); break;
                default: BuildAbout(); break;
            }
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

            lv_obj_t* top = Box(overlay, 0, 0, 800, 34, 0x202328, 0x15171a, 0);
            lv_obj_set_style_border_side(top, LV_BORDER_SIDE_BOTTOM, LV_PART_MAIN);
            lv_obj_t* mark = Box(top, 7, 7, 20, 20, C_AMBER, C_AMBER, 0);
            Label(mark, 0, 2, 20, "X", 0x151515, &lv_font_montserrat_14, LV_TEXT_ALIGN_CENTER);
            Label(top, 32, 10, 100, "OPENX32", C_TXT_HI, &lv_font_montserrat_14);
            Label(top, 111, 12, 30, "/M32", C_AMBER);
            // Browser geometry: brand 132 px, tabs 141..694 (553 px), status at 702.
            for (int i = 0; i < 16; ++i)
            {
                int tx = 141 + (i * 553) / 16;
                int tw = ((i + 1) * 553) / 16 - (i * 553) / 16;
                tabBtns[i] = Button(top, tx, 0, tw, 34, TAB_LABELS[i], 600 + i);
            }
            Box(top, 708, 14, 6, 6, C_GREEN, C_GREEN, 0);
            Label(top, 720, 11, 34, "00:00", C_DIM);
            Label(top, 757, 11, 8, "/", C_DIMMER);
            Label(top, 769, 11, 24, "48k", C_DIM);

            lv_obj_t* soft = Box(overlay, 0, 34, 54, 414, 0x15171a, 0x000000, 0);
            lv_obj_set_style_border_side(soft, LV_BORDER_SIDE_RIGHT, LV_PART_MAIN);
            for (int i = 0; i < 14; ++i)
            {
                lv_obj_t* b = lv_btn_create(soft);
                lv_obj_set_pos(b, 5, 5 + i * 29);
                lv_obj_set_size(b, 45, 27);
                lv_obj_set_style_bg_color(b, lv_color_hex(0x252930), LV_PART_MAIN);
                lv_obj_set_style_border_color(b, lv_color_hex(0x3b4148), LV_PART_MAIN);
                lv_obj_set_style_border_width(b, 1, LV_PART_MAIN);
                lv_obj_set_style_pad_all(b, 0, LV_PART_MAIN);
                lv_obj_set_style_radius(b, 0, LV_PART_MAIN);
                lv_obj_add_event_cb(b, Clicked, LV_EVENT_CLICKED, (void*)(intptr_t)(500 + i));
                Label(b, 0, 4, 45, SOFT_LABELS[i][0], C_TXT, nullptr, LV_TEXT_ALIGN_CENTER);
                Label(b, 0, 16, 45, SOFT_LABELS[i][1], C_DIM, nullptr, LV_TEXT_ALIGN_CENTER);
                softBtns[i] = b;
            }

            lv_obj_t* screen = Box(overlay, 55, 35, 744, 412, 0x0e0f11, 0x000000, 0);
            body = lv_obj_create(screen);
            lv_obj_set_pos(body, 8, 7);
            lv_obj_set_size(body, 728, 398);
            lv_obj_set_style_bg_opa(body, LV_OPA_TRANSP, LV_PART_MAIN);
            lv_obj_set_style_border_width(body, 0, LV_PART_MAIN);
            lv_obj_set_style_pad_all(body, 0, LV_PART_MAIN);
            lv_obj_clear_flag(body, LV_OBJ_FLAG_SCROLLABLE);

            lv_obj_t* bottom = Box(overlay, 0, 447, 800, 32, 0x15171a, 0x000000, 0);
            lv_obj_set_style_border_side(bottom, LV_BORDER_SIDE_TOP, LV_PART_MAIN);
            bbSel = Label(bottom, 8, 9, 110, "", C_TXT);
            bbMs = Label(bottom, 126, 9, 70, "", C_TXT);
            bbFader = Label(bottom, 204, 9, 90, "", C_TXT);
            bbView = Label(bottom, 302, 9, 180, "", C_AMBER);
            bbExtra = Label(bottom, 490, 9, 180, "", C_DIM);
            bbMsg = Label(bottom, 676, 9, 118, "", C_DIMMER, nullptr, LV_TEXT_ALIGN_RIGHT);

            SwitchTab(activeTab);
        }

        void Refresh()
        {
            if (!overlay) return;
            if (headNote) lv_label_set_text(headNote, PAGE_HEAD_NOTE[activeTab]);
            if (bbSel) lv_label_set_text(bbSel, "SEL 05 BASS DI");
            if (bbMs) lv_label_set_text(bbMs, "M/S ON");
            if (bbFader) lv_label_set_text(bbFader, "FDR -6.3");
            if (bbView) lv_label_set_text_fmt(bbView, "VIEW %s", PAGE_TITLES[activeTab]);
            if (bbExtra) lv_label_set_text(bbExtra, PAGE_BB_EXTRA[activeTab]);
            if (bbMsg) lv_label_set_text(bbMsg, statusMsg[0] ? statusMsg : "SYSTEM READY LVGL 9.5");

            if (activeTab != 5) return;
            char buf[16];
            for (int i = 0; i < 12; ++i)
            {
                if (!slotCells[i]) continue;
                if (slotLabel[activeBank][i][0]) snprintf(buf, sizeof(buf), "%s", slotLabel[activeBank][i]);
                else snprintf(buf, sizeof(buf), "CH %02d", i + 1);
                if (slotNames[i]) lv_label_set_text(slotNames[i], buf);
                bool sel = (i == selectedSlot);
                lv_obj_set_style_bg_color(slotCells[i], lv_color_hex(sel ? C_AMBER_D : 0x16191c), LV_PART_MAIN);
                lv_obj_set_style_border_color(slotCells[i], lv_color_hex(sel ? C_AMBER : 0x30363c), LV_PART_MAIN);
                if (bankBtns[i]) BtnLook(bankBtns[i], i == activeBank);
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

        void OnInit() override { BuildOverlay(); }

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
                    snprintf(statusMsg, sizeof(statusMsg), "Encoder 1 bank");
                    break;
                case SurfaceElementId::DISPLAY_ENCODER_2:
                    selectedSlot = (selectedSlot + amount + 120) % 12;
                    snprintf(statusMsg, sizeof(statusMsg), "Encoder 2 slot");
                    break;
                default: return;
            }
            Refresh();
        }

        void OnChangeCustomButton(SurfaceElementId id) override
        {
            switch (id)
            {
                case SurfaceElementId::DISPLAY_ENCODER_BUTTON_2:
                    snprintf(slotLabel[activeBank][selectedSlot], 16, "CH %02d", activeBank * 12 + selectedSlot + 1);
                    snprintf(statusMsg, sizeof(statusMsg), "Button 2 applied");
                    break;
                case SurfaceElementId::DISPLAY_ENCODER_BUTTON_6:
                    for (int i = 0; i < 12; ++i) slotLabel[activeBank][i][0] = 0;
                    activeBank = 0; selectedSlot = 0;
                    snprintf(statusMsg, sizeof(statusMsg), "Button 6 reset");
                    break;
                default: return;
            }
            Refresh();
        }
};

lv_obj_t* PageShowConfig::overlay = nullptr;
PageShowConfig* PageShowConfig::self = nullptr;

const char* PageShowConfig::TAB_LABELS[16] = {
    "OVER","CHAN","EQ","DYN","AUX","MAP","MON","ROUTE",
    "DLY","MET","FX","CARD","MACRO","SCENE","CFG","SYS"};

const char* PageShowConfig::PAGE_TITLES[16] = {
    "OVERVIEW","CHANNEL STRIP","EQ","DYNAMICS","AUX SENDS","FADER ASSIGN",
    "MONITOR AUDIO","ROUTING MATRIX","DELAY / ALIGNMENT","METERBRIDGE",
    "EFFECTS RACK","X-LIVE CARD","MACRO / RECALL","SCENE MANAGER",
    "SHOW CONFIG","SYSTEM"};

const char* PageShowConfig::PAGE_HEAD_NOTE[16] = {
    "SEL 05 BASS DI","CHANNEL 05 BASS DI","4-BAND EQ / SELECTABLE FILTER MODES",
    "GATE + COMPRESSOR CH 05","AUX 01 POST 12 SOURCES","BANK A01 SLOT 01 CUSTOM ON",
    "LR MASTER MONITOR","INPUT > XLR OUT","48 kHz 343 m/s 25 FPS","24 IN 16 OUT 8 GROUP",
    "DSP2 4 EFFECT SLOTS","X-LIVE RECORD / PLAYBACK","A01 IDLE",
    "SHOW M32 SHOW / SHOW > BANK > SCENE","SHOW M32 SHOW / HIDE UNUSED ON",
    "OPENX32 M32 UI CONCEPT"};

const char* PageShowConfig::PAGE_BB_EXTRA[16] = {
    "","","","","AUX 01 POST","MAP A01 SLOT 01","MON LR 72%","","","","","","MAC A01 IDLE","SCENE SLOT 01","CFG 24IN 8AUX 8GRP 6MTX 16OUT","CLK 48k 25fps"};

const char* PageShowConfig::SOFT_LABELS[14][2] = {
    {"SEL","select"},{"MUTE","channel"},{"SOLO","channel"},{"<","bank"},{">","bank"},
    {"EQ","view"},{"DYN","view"},{"AUX","sends"},{"MAP","fader bank"},{"MON","listen"},
    {"MAC","macro"},{"DLY","align"},{"CFG","show setup"},{"HOME","overview"}};

const char* PageShowConfig::BANK_NAMES[12] = {
    "A01","A02","A03","A04","A05","A06","A07","A08","O01","O02","O03","O04"};

const char* PageShowConfig::LIST_PREFIX[5] = {"IN","FX","GRP","OUT","AUX"};

const int PageShowConfig::SOFT_TAB[14] = {0,1,2,-1,-1,2,3,4,5,6,12,8,14,0};

}
