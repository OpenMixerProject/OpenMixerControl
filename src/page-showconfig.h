#pragma once
#include "page.h"

using namespace std;
using enum MP_ID;

namespace OMC
{

/**
 * SHOW CONFIG / FADER MAP — show-first navigation prototype.
 *
 * This page is a live LVGL port of the static browser prototype stored in
 * contrib/openx32-quantum/. It reuses the empty "Prototype GUI" tab so no
 * EEZ Studio regeneration is required. It is intentionally self-contained:
 * all widgets are created programmatically in OnInit().
 *
 * Layout (800x480, tab content area ~800x340 above the encoder strip):
 *   - Header with title and active bank
 *   - 12 x 12 fader-bank matrix (banks as rows, fader slots as columns)
 *   - Footer hint for encoder bindings
 */
class PageShowConfig : public Page
{
    private:
        static constexpr int BANK_COUNT = 12;
        static constexpr int SLOT_COUNT = 12;

        lv_obj_t* container = nullptr;
        lv_obj_t* titleLabel = nullptr;
        lv_obj_t* bankLabel = nullptr;
        lv_obj_t* hintLabel = nullptr;
        lv_obj_t* bankCells[BANK_COUNT] = {};
        lv_obj_t* slotCells[BANK_COUNT][SLOT_COUNT] = {};

        int activeBank = 0;
        int selectedSlot = 0;
        bool built = false;

        void BuildUI()
        {
            if (built) return;

            // The "Prototype GUI" tab is the 6th child (index 5) of setuptab.
            lv_obj_t* content = lv_tabview_get_content(objects.setuptab);
            container = lv_obj_get_child(content, 5);
            if (!container) return;

            lv_obj_clear_flag(container, LV_OBJ_FLAG_SCROLLABLE);

            // Palette matching the browser prototype (amber accent #ff9d00).
            const lv_color_t bg     = lv_color_hex(0x0d0e10);
            const lv_color_t panel  = lv_color_hex(0x191b1e);
            const lv_color_t line   = lv_color_hex(0x353a41);
            const lv_color_t text   = lv_color_hex(0xd9dde1);
            const lv_color_t dim    = lv_color_hex(0x7d858d);
            const lv_color_t amber  = lv_color_hex(0xff9d00);
            const lv_color_t amberD = lv_color_hex(0x8a5600);

            lv_obj_set_style_bg_color(container, bg, LV_PART_MAIN);
            lv_obj_set_style_pad_all(container, 0, LV_PART_MAIN);

            // ---- Header ----------------------------------------------------
            titleLabel = lv_label_create(container);
            lv_obj_set_pos(titleLabel, 8, 4);
            lv_obj_set_style_text_font(titleLabel, &lv_font_montserrat_16, LV_PART_MAIN);
            lv_obj_set_style_text_color(titleLabel, amber, LV_PART_MAIN);
            lv_label_set_text_static(titleLabel, "SHOW CONFIG  /  FADER MAP");

            bankLabel = lv_label_create(container);
            lv_obj_set_pos(bankLabel, 560, 6);
            lv_obj_set_style_text_font(bankLabel, &lv_font_montserrat_14, LV_PART_MAIN);
            lv_obj_set_style_text_color(bankLabel, text, LV_PART_MAIN);

            // ---- Matrix ----------------------------------------------------
            const int rowH = 22;
            const int rowY = 30;
            const int nameW = 78;
            const int cellW = 57;
            const int cellH = 18;
            const int cellGap = 2;

            for (int b = 0; b < BANK_COUNT; ++b)
            {
                int y = rowY + b * rowH;

                bankCells[b] = lv_label_create(container);
                lv_obj_set_pos(bankCells[b], 8, y + 2);
                lv_obj_set_size(bankCells[b], nameW - 4, cellH);
                lv_obj_set_style_text_font(bankCells[b], &lv_font_montserrat_14, LV_PART_MAIN);
                lv_obj_set_style_text_color(bankCells[b], dim, LV_PART_MAIN);
                lv_label_set_text_fmt(bankCells[b], "BANK %02d", b + 1);

                for (int s = 0; s < SLOT_COUNT; ++s)
                {
                    lv_obj_t* cell = lv_obj_create(container);
                    slotCells[b][s] = cell;
                    lv_obj_set_pos(cell, 8 + nameW + s * (cellW + cellGap), y);
                    lv_obj_set_size(cell, cellW, cellH);
                    lv_obj_set_style_bg_color(cell, panel, LV_PART_MAIN);
                    lv_obj_set_style_border_color(cell, line, LV_PART_MAIN);
                    lv_obj_set_style_border_width(cell, 1, LV_PART_MAIN);
                    lv_obj_set_style_pad_all(cell, 0, LV_PART_MAIN);
                    lv_obj_clear_flag(cell, LV_OBJ_FLAG_SCROLLABLE);

                    lv_obj_t* lbl = lv_label_create(cell);
                    lv_obj_center(lbl);
                    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, LV_PART_MAIN);
                    lv_obj_set_style_text_color(lbl, text, LV_PART_MAIN);
                    lv_label_set_text_fmt(lbl, "CH%02d", s + 1);
                }
            }

            // ---- Footer ----------------------------------------------------
            hintLabel = lv_label_create(container);
            lv_obj_set_pos(hintLabel, 8, 318);
            lv_obj_set_style_text_font(hintLabel, &lv_font_montserrat_14, LV_PART_MAIN);
            lv_obj_set_style_text_color(hintLabel, dim, LV_PART_MAIN);
            lv_label_set_text_static(hintLabel,
                "ENC 1: select bank    ENC 2: select slot    BTN 2: apply    BTN 6: reset");

            built = true;
            Refresh();
        }

        void Refresh()
        {
            if (!built) return;

            lv_label_set_text_fmt(bankLabel, "BANK %02d / SLOT %02d", activeBank + 1, selectedSlot + 1);

            for (int b = 0; b < BANK_COUNT; ++b)
            {
                bool isActive = (b == activeBank);
                lv_obj_set_style_text_color(bankCells[b],
                    isActive ? lv_color_hex(0xff9d00) : lv_color_hex(0x7d858d), LV_PART_MAIN);

                for (int s = 0; s < SLOT_COUNT; ++s)
                {
                    bool isSelected = isActive && (s == selectedSlot);
                    lv_obj_set_style_bg_color(slotCells[b][s],
                        isSelected ? lv_color_hex(0x8a5600) : lv_color_hex(0x191b1e), LV_PART_MAIN);
                    lv_obj_set_style_border_color(slotCells[b][s],
                        isSelected ? lv_color_hex(0xff9d00) : lv_color_hex(0x353a41), LV_PART_MAIN);
                }
            }
        }

    public:
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
            BuildUI();
        }

        void OnShow() override
        {
            if (!built) BuildUI();

            config->SurfaceBindCustom(SurfaceElementId::DISPLAY_ENCODER_1, "Select Bank");
            config->SurfaceBindCustom(SurfaceElementId::DISPLAY_ENCODER_2, "Select Slot");
            config->SurfaceBindCustom(SurfaceElementId::DISPLAY_ENCODER_BUTTON_2, "Apply");
            config->SurfaceBindCustom(SurfaceElementId::DISPLAY_ENCODER_BUTTON_6, "Reset");
            Refresh();
        }

        void OnChangeCustomEncoder(SurfaceElementId surface_element_id, int amount) override
        {
            switch (surface_element_id)
            {
                case SurfaceElementId::DISPLAY_ENCODER_1:
                    activeBank += amount;
                    if (activeBank < 0) activeBank = BANK_COUNT - 1;
                    if (activeBank >= BANK_COUNT) activeBank = 0;
                    Refresh();
                    break;
                case SurfaceElementId::DISPLAY_ENCODER_2:
                    selectedSlot += amount;
                    if (selectedSlot < 0) selectedSlot = SLOT_COUNT - 1;
                    if (selectedSlot >= SLOT_COUNT) selectedSlot = 0;
                    Refresh();
                    break;
                default:
                    break;
            }
        }

        void OnChangeCustomButton(SurfaceElementId surface_element_id) override
        {
            switch (surface_element_id)
            {
                case SurfaceElementId::DISPLAY_ENCODER_BUTTON_2:
                    // Apply: point the first fader bank at the selected channel.
                    config->Set(BANKING_INPUT, (uint)selectedSlot, 0);
                    break;
                case SurfaceElementId::DISPLAY_ENCODER_BUTTON_6:
                    activeBank = 0;
                    selectedSlot = 0;
                    Refresh();
                    break;
                default:
                    break;
            }
        }
};

}
