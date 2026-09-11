#pragma once
#include "page.h"

using namespace std;

namespace OMC
{

class PageSetupMixerConfig: public Page
{
    using enum MP_ID;

    private:
        lv_obj_t* scroll_container = nullptr;
        
        static constexpr lv_coord_t ENCODER_HINT_RESERVED_HEIGHT = 60;

        // Navigation state for physical encoders
        uint selected_type = 0; // 0: Channels/Auxes, 1: Busses, 2: Matrixes
        uint selected_idx[3] = { 0, 0, 0 }; // Index within each type (0..19, 0..7, 0..2)
        uint pending_toggle_type = 0;
        uint pending_toggle_idx = 0;
        lv_obj_t* confirm_box = nullptr;
        X32_PAGE confirm_prev_page = X32_PAGE::NONE;
        X32_PAGE confirm_next_page = X32_PAGE::NONE;
        
        // We will store pointers to our buttons so we can dynamically style/update them
        vector<lv_obj_t*> ch_buttons;
        vector<lv_obj_t*> bus_buttons;
        vector<lv_obj_t*> mtx_buttons;
        
        // Flex columns
        lv_obj_t* col_channels = nullptr;
        lv_obj_t* col_busses = nullptr;
        lv_obj_t* col_matrixes = nullptr;

        void ScrollSelectionIntoView()
        {
            vector<lv_obj_t*>* btn_lists[3] = { &ch_buttons, &bus_buttons, &mtx_buttons };
            if (selected_type > 2 || selected_idx[selected_type] >= btn_lists[selected_type]->size())
            {
                return;
            }

            lv_obj_t* btn = btn_lists[selected_type]->at(selected_idx[selected_type]);
            lv_obj_update_layout(btn);
            lv_obj_scroll_to_view(btn, LV_ANIM_OFF);
        }

        String GetPairLabel(MP_ID mp_id, uint pair_index)
        {
            if (mp_id == CHANNEL_LINKED)
            {
                if (pair_index >= 16)
                {
                    return String("AUX ") + String((pair_index - 16) * 2 + 1) + "/" + String((pair_index - 16) * 2 + 2);
                }
                return String("CH ") + String(pair_index * 2 + 1) + "/" + String(pair_index * 2 + 2);
            }
            else if (mp_id == BUS_LINKED)
            {
                return String("BUS ") + String(pair_index * 2 + 1) + "/" + String(pair_index * 2 + 2);
            }
            else if (mp_id == MATRIX_LINKED)
            {
                return String("MTX ") + String(pair_index * 2 + 1) + "/" + String(pair_index * 2 + 2);
            }

            return "Unknown";
        }

        void UpdateButtonVisuals(lv_obj_t* btn, MP_ID mp_id, uint pair_index, bool is_focused)
        {
            bool is_linked = config->GetBool(mp_id, pair_index);
            
            // Format labels
            String label_text = GetPairLabel(mp_id, pair_index);
            
            if (is_linked)
            {
                label_text += " [ST]";
                lv_obj_set_style_bg_color(btn, lv_color_make(38, 162, 252), LV_PART_MAIN | LV_STATE_DEFAULT); // bright blue
            }
            else
            {
                label_text += " [M]";
                lv_obj_set_style_bg_color(btn, lv_color_make(50, 60, 70), LV_PART_MAIN | LV_STATE_DEFAULT); // gray
            }
            
            // Set label
            lv_obj_t* label = lv_obj_get_child(btn, 0);
            if (label)
            {
                lv_label_set_text(label, label_text.c_str());
            }
            
            // Handle focus outline
            if (is_focused)
            {
                lv_obj_set_style_outline_width(btn, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_outline_color(btn, lv_palette_main(LV_PALETTE_YELLOW), LV_PART_MAIN | LV_STATE_DEFAULT);
            }
            else
            {
                lv_obj_set_style_outline_width(btn, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            }
        }

        void OnButtonClicked(lv_obj_t* btn)
        {
            uintptr_t pack = (uintptr_t)lv_obj_get_user_data(btn);
            MP_ID mp_id = (MP_ID)(pack >> 16);
            uint pair_index = pack & 0xFFFF;
            if (mp_id == CHANNEL_LINKED) { selected_type = 0; }
            else if (mp_id == BUS_LINKED) { selected_type = 1; }
            else if (mp_id == MATRIX_LINKED) { selected_type = 2; }
            selected_idx[selected_type] = pair_index;
            
            // Toggle link parameter
            config->Toggle(mp_id, pair_index);
            
            // Force redraw of all buttons to stay in sync
            OnChange(true);
        }

        static void link_btn_event_cb(lv_event_t * e)
        {
            lv_obj_t * btn = (lv_obj_t*)lv_event_get_target(e);
            PageSetupMixerConfig* page = (PageSetupMixerConfig*)lv_event_get_user_data(e);
            page->OnButtonClicked(btn);
        }

        MP_ID GetTypeParameter(uint type)
        {
            MP_ID mps[3] = { CHANNEL_LINKED, BUS_LINKED, MATRIX_LINKED };
            return mps[type];
        }

        uint GetTypeMaxRows(uint type)
        {
            uint max_rows[3] = { 20, 8, 3 };
            return max_rows[type];
        }

        void ChangeTypeSelection(uint type, int amount)
        {
            selected_type = type;
            selected_idx[type] = helper->CheckBoundaries(selected_idx[type], amount, 0, GetTypeMaxRows(type) - 1);
            OnChange(true);
        }

        void CloseConfirmBox()
        {
            if (confirm_box)
            {
                lv_msgbox_close(confirm_box);
                confirm_box = nullptr;
                prevPage = confirm_prev_page;
                nextPage = confirm_next_page;
            }
        }

        void ConfirmPendingModeChange()
        {
            config->Toggle(GetTypeParameter(pending_toggle_type), pending_toggle_idx);
            CloseConfirmBox();
            OnChange(true);
        }

        void ShowToggleConfirm(uint type)
        {
            selected_type = type;
            pending_toggle_type = type;
            pending_toggle_idx = selected_idx[type];

            MP_ID mp_id = GetTypeParameter(type);
            bool is_linked = config->GetBool(mp_id, pending_toggle_idx);
            String pair_label = GetPairLabel(mp_id, pending_toggle_idx);
            String next_mode = is_linked ? "mono" : "stereo";
            String message = String("Change ") + pair_label + String(" to ") + next_mode + String(" mode?");

            CloseConfirmBox();
            confirm_prev_page = prevPage;
            confirm_next_page = nextPage;
            prevPage = X32_PAGE::NONE;
            nextPage = X32_PAGE::NONE;

            confirm_box = lv_msgbox_create(objects.main);
            lv_msgbox_add_title(confirm_box, "Confirm Mode Change");
            lv_msgbox_add_text(confirm_box, message.c_str());

            lv_obj_t* cancel_btn = lv_msgbox_add_footer_button(confirm_box, "Left: Cancel");
            lv_obj_add_event_cb(cancel_btn, cancel_btn_event_cb, LV_EVENT_CLICKED, this);

            lv_obj_t* confirm_btn = lv_msgbox_add_footer_button(confirm_box, "Right: Confirm");
            lv_obj_add_event_cb(confirm_btn, confirm_btn_event_cb, LV_EVENT_CLICKED, this);

            lv_obj_center(confirm_box);
        }

        static void confirm_btn_event_cb(lv_event_t * e)
        {
            PageSetupMixerConfig* page = (PageSetupMixerConfig*)lv_event_get_user_data(e);
            page->ConfirmPendingModeChange();
        }

        static void cancel_btn_event_cb(lv_event_t * e)
        {
            PageSetupMixerConfig* page = (PageSetupMixerConfig*)lv_event_get_user_data(e);
            page->CloseConfirmBox();
        }

    public:
        PageSetupMixerConfig(PageBaseParameter* pagebasepar) : Page(pagebasepar)
        {
            prevPage = X32_PAGE::SETUP_SURFACE;
            nextPage = X32_PAGE::ABOUT;
            tabLayer0 = objects.maintab;
            tabIndex0 = 3;
            tabLayer1 = objects.setuptab;
            tabIndex1 = 3;
        }

        void OnInit() override
        {
            // Dynamically add a 4th tab to setuptab!
            lv_obj_t* tab = lv_tabview_add_tab(objects.setuptab, "Mixer Config");
            lv_obj_set_scrollbar_mode(tab, LV_SCROLLBAR_MODE_OFF);
            
            // Move the tab page to index 3 in the tab content container
            lv_obj_t* content = lv_tabview_get_content(objects.setuptab);
            if (content)
            {
                lv_obj_move_to_index(tab, 3);
            }
            
            // Move the tab button to index 3 in the tab bar
            lv_obj_t* tab_bar = lv_tabview_get_tab_bar(objects.setuptab);
            if (tab_bar && lv_obj_get_child_count(tab_bar) > 0)
            {
                lv_obj_t* tab_btn = lv_obj_get_child(tab_bar, lv_obj_get_child_count(tab_bar) - 1);
                if (tab_btn) lv_obj_move_to_index(tab_btn, 3);
            }
            
            // Main container inside the tab - using a horizontal layout for the three columns
            scroll_container = lv_obj_create(tab);
            lv_obj_update_layout(tab);
            lv_coord_t scroll_height = lv_obj_get_height(tab) - ENCODER_HINT_RESERVED_HEIGHT;
            if (scroll_height <= 0)
            {
                scroll_height = 320;
            }
            lv_obj_set_size(scroll_container, LV_PCT(100), scroll_height);
            lv_obj_set_pos(scroll_container, 0, 0);
            lv_obj_set_style_pad_all(scroll_container, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(scroll_container, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(scroll_container, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_DEFAULT);
            
            lv_obj_set_layout(scroll_container, LV_LAYOUT_FLEX);
            lv_obj_set_flex_flow(scroll_container, LV_FLEX_FLOW_ROW);
            lv_obj_set_flex_align(scroll_container, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

            // Columns
            col_channels = lv_obj_create(scroll_container);
            col_busses = lv_obj_create(scroll_container);
            col_matrixes = lv_obj_create(scroll_container);

            lv_obj_t* cols[3] = { col_channels, col_busses, col_matrixes };
            const char* titles[3] = { "Input Channels / Auxes", "Mix Busses", "Matrixes" };
            uint count[3] = { 20, 8, 3 };
            MP_ID mps[3] = { CHANNEL_LINKED, BUS_LINKED, MATRIX_LINKED };
            vector<lv_obj_t*>* btn_lists[3] = { &ch_buttons, &bus_buttons, &mtx_buttons };

            for (int c = 0; c < 3; c++)
            {
                lv_obj_set_size(cols[c], LV_PCT(31), LV_PCT(100));
                lv_obj_set_style_pad_all(cols[c], 5, LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_pad_bottom(cols[c], 35, LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_bg_color(cols[c], lv_color_make(30, 40, 50), LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_bg_opa(cols[c], LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_radius(cols[c], 8, LV_PART_MAIN | LV_STATE_DEFAULT);
                
                lv_obj_set_layout(cols[c], LV_LAYOUT_FLEX);
                lv_obj_set_flex_flow(cols[c], LV_FLEX_FLOW_COLUMN);
                lv_obj_set_flex_align(cols[c], LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
                lv_obj_set_style_pad_row(cols[c], 6, LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_add_flag(cols[c], LV_OBJ_FLAG_SCROLLABLE);
                lv_obj_set_scroll_dir(cols[c], LV_DIR_VER);
                lv_obj_set_scrollbar_mode(cols[c], LV_SCROLLBAR_MODE_AUTO);

                // Title Label
                lv_obj_t* title_label = lv_label_create(cols[c]);
                lv_label_set_text(title_label, titles[c]);
                lv_obj_set_style_text_color(title_label, lv_color_make(255, 255, 255), LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_text_font(title_label, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_pad_bottom(title_label, 5, LV_PART_MAIN | LV_STATE_DEFAULT);

                // Buttons
                for (uint i = 0; i < count[c]; i++)
                {
                    lv_obj_t* btn = lv_button_create(cols[c]);
                    lv_obj_set_size(btn, LV_PCT(95), 25);
                    lv_obj_set_style_radius(btn, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_pad_all(btn, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

                    lv_obj_t* lbl = lv_label_create(btn);
                    lv_obj_center(lbl);
                    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, LV_PART_MAIN | LV_STATE_DEFAULT);

                    uintptr_t pack = ((uintptr_t)mps[c] << 16) | i;
                    lv_obj_set_user_data(btn, (void*)pack);
                    lv_obj_add_event_cb(btn, link_btn_event_cb, LV_EVENT_CLICKED, this);

                    btn_lists[c]->push_back(btn);
                }
            }
        }

        void OnShow() override
        {
            // Bind display encoders for navigation
            config->SurfaceBindCustom(SurfaceElementId::DISPLAY_ENCODER_1, String(LV_SYMBOL_UP) + " / " + LV_SYMBOL_DOWN + "\nCh/Aux");
            config->SurfaceBindCustom(SurfaceElementId::DISPLAY_ENCODER_BUTTON_1, "Change Mode");
            config->SurfaceBindCustom(SurfaceElementId::DISPLAY_ENCODER_3, String(LV_SYMBOL_UP) + " / " + LV_SYMBOL_DOWN + "\nMix Busses");
            config->SurfaceBindCustom(SurfaceElementId::DISPLAY_ENCODER_BUTTON_3, "Change Mode");
            config->SurfaceBindCustom(SurfaceElementId::DISPLAY_ENCODER_5, String(LV_SYMBOL_UP) + " / " + LV_SYMBOL_DOWN + "\nMatrixes");
            config->SurfaceBindCustom(SurfaceElementId::DISPLAY_ENCODER_BUTTON_5, "Change Mode");

            OnChange(true);
        }

        void OnChange(bool force_update) override
        {
            if (confirm_box)
            {
                if (config->HasParameterChanged(DISPLAY_LEFT))
                {
                    config->SetParameterUnchanged(DISPLAY_LEFT);
                    CloseConfirmBox();
                    return;
                }

                if (config->HasParameterChanged(DISPLAY_RIGHT))
                {
                    config->SetParameterUnchanged(DISPLAY_RIGHT);
                    ConfirmPendingModeChange();
                    return;
                }
            }

            // Redraw/re-style all buttons according to their state and current focus
            MP_ID mps[3] = { CHANNEL_LINKED, BUS_LINKED, MATRIX_LINKED };
            vector<lv_obj_t*>* btn_lists[3] = { &ch_buttons, &bus_buttons, &mtx_buttons };
            uint count[3] = { 20, 8, 3 };

            for (int c = 0; c < 3; c++)
            {
                for (uint i = 0; i < count[c]; i++)
                {
                    bool is_focused = (selected_type == (uint)c && selected_idx[c] == i);
                    UpdateButtonVisuals(btn_lists[c]->at(i), mps[c], i, is_focused);
                }
            }

            ScrollSelectionIntoView();
        }

        void OnChangeCustomEncoder(SurfaceElementId surface_element_id, int amount) override
        {
            switch (surface_element_id)
            {
                case SurfaceElementId::DISPLAY_ENCODER_1:
                    ChangeTypeSelection(0, amount);
                    break;
                case SurfaceElementId::DISPLAY_ENCODER_3:
                    ChangeTypeSelection(1, amount);
                    break;
                case SurfaceElementId::DISPLAY_ENCODER_5:
                    ChangeTypeSelection(2, amount);
                    break;
                default:
                    break;
            }
        }

        void OnChangeCustomButton(SurfaceElementId surface_element_id) override
        {
            switch (surface_element_id)
            {
                case SurfaceElementId::DISPLAY_ENCODER_BUTTON_1:
                    ShowToggleConfirm(0);
                    break;
                case SurfaceElementId::DISPLAY_ENCODER_BUTTON_3:
                    ShowToggleConfirm(1);
                    break;
                case SurfaceElementId::DISPLAY_ENCODER_BUTTON_5:
                    ShowToggleConfirm(2);
                    break;
                default:
                    break;
            }
        }
};


}
