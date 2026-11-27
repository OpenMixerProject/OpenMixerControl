#pragma once
#include "page.h"

using namespace std;

namespace OMC
{

// Setup page to link a channel with its previous or next neighbour to a stereo pair
class PageSetupMixerConfig: public Page
{
    using enum MP_ID;

    private:
        static constexpr lv_coord_t ENCODER_HINT_RESERVED_HEIGHT = 60;
        static constexpr uint COLUMNS = 3;

        // vChannels shown in each column: inputs/aux/fx returns, busses, matrixes
        const uint column_first[COLUMNS] = { (uint)X32_VCHANNEL_BLOCK::NORMAL, (uint)X32_VCHANNEL_BLOCK::BUS, (uint)X32_VCHANNEL_BLOCK::MATRIX };
        const uint column_end[COLUMNS] = { (uint)X32_VCHANNEL_BLOCK::BUS, (uint)X32_VCHANNEL_BLOCK::MATRIX, (uint)X32_VCHANNEL_BLOCK::SPECIAL };
        const char* column_titles[COLUMNS] = { "Inputs / Aux / FX Ret", "Mix Busses", "Matrixes" };

        // each column is controlled by one encoder and the buttons of this and the next encoder
        const SurfaceElementId column_encoder[COLUMNS] = { SurfaceElementId::DISPLAY_ENCODER_1, SurfaceElementId::DISPLAY_ENCODER_3, SurfaceElementId::DISPLAY_ENCODER_5 };
        const SurfaceElementId column_button_next[COLUMNS] = { SurfaceElementId::DISPLAY_ENCODER_BUTTON_1, SurfaceElementId::DISPLAY_ENCODER_BUTTON_3, SurfaceElementId::DISPLAY_ENCODER_BUTTON_5 };
        const SurfaceElementId column_button_prev[COLUMNS] = { SurfaceElementId::DISPLAY_ENCODER_BUTTON_2, SurfaceElementId::DISPLAY_ENCODER_BUTTON_4, SurfaceElementId::DISPLAY_ENCODER_BUTTON_6 };

        lv_obj_t* buttons[(uint)X32_VCHANNEL_BLOCK::SPECIAL] = {};
        uint selected_column = 0;
        uint selected[COLUMNS] = { column_first[0], column_first[1], column_first[2] };

        // link change waiting for confirmation
        lv_obj_t* confirm_box = nullptr;
        uint pending_vchannel = 0;
        int pending_link = 0;
        X32_PAGE confirm_prev_page = X32_PAGE::NONE;
        X32_PAGE confirm_next_page = X32_PAGE::NONE;

        uint GetColumn(uint vchannel)
        {
            for (uint c = 0; c < COLUMNS; c++)
            {
                if (vchannel >= column_first[c] && vchannel < column_end[c])
                {
                    return c;
                }
            }
            return 0;
        }

        bool IsLinked(uint vchannel)
        {
            uint peer;
            return config->GetStereoPeer(vchannel, peer);
        }

        void UpdateButton(uint vchannel)
        {
            lv_obj_t* btn = buttons[vchannel];
            bool linked = IsLinked(vchannel);

            lv_obj_set_flag(btn, LV_OBJ_FLAG_HIDDEN, config->IsStereoLinkedRight(vchannel));
            lv_label_set_text(lv_obj_get_child(btn, 0), (config->GetChannelLabel(vchannel) + (linked ? " [ST]" : "")).c_str());
            lv_obj_set_style_bg_color(btn, linked ? lv_color_make(38, 162, 252) : lv_color_make(50, 60, 70), LV_PART_MAIN);

            bool focused = selected[selected_column] == vchannel;
            lv_obj_set_style_outline_width(btn, focused ? 2 : 0, LV_PART_MAIN);
            lv_obj_set_style_outline_color(btn, lv_palette_main(LV_PALETTE_YELLOW), LV_PART_MAIN);
        }

        void UpdateButtonLabels()
        {
            for (uint c = 0; c < COLUMNS; c++)
            {
                uint vchannel = selected[c];
                config->SurfaceBindCustom(column_encoder[c], String(LV_SYMBOL_UP) + " / " + LV_SYMBOL_DOWN + "\n" + column_titles[c]);

                if (IsLinked(vchannel))
                {
                    config->SurfaceBindCustom(column_button_next[c], "Unlink");
                    config->SurfaceUnbind(column_button_prev[c]);
                }
                else
                {
                    config->SurfaceBindCustom(column_button_next[c], config->IsStereoLinkable(vchannel, vchannel + 1) ? String("Link ") + LV_SYMBOL_DOWN : String(""));
                    config->SurfaceBindCustom(column_button_prev[c], config->IsStereoLinkable(vchannel, vchannel - 1) ? String("Link ") + LV_SYMBOL_UP : String(""));
                }
            }
        }

        void Select(uint vchannel)
        {
            uint column = GetColumn(vchannel);
            uint before = selected[selected_column];

            selected_column = column;
            selected[column] = vchannel;

            UpdateButton(before);
            UpdateButton(vchannel);
            UpdateButtonLabels();

            lv_obj_update_layout(buttons[vchannel]);
            lv_obj_scroll_to_view(buttons[vchannel], LV_ANIM_OFF);
        }

        // moves the selection of a column, the second channel of a stereo pair is skipped
        void MoveSelection(uint column, int amount)
        {
            int vchannel = selected[column];
            int step = amount > 0 ? 1 : -1;
            for (int i = 0; i != amount; i += step)
            {
                int next = vchannel + step;
                while (next >= (int)column_first[column] && next < (int)column_end[column] && config->IsStereoLinkedRight(next))
                {
                    next += step;
                }
                if (next < (int)column_first[column] || next >= (int)column_end[column])
                {
                    break;
                }
                vchannel = next;
            }
            Select(vchannel);
        }

        // asks before linking or unlinking, because both change the sound of the channels
        void AskForLink(uint vchannel, int link)
        {
            uint peer;
            String message;
            if (link == 0)
            {
                if (!config->GetStereoPeer(vchannel, peer))
                {
                    return;
                }
                message = String("Unlink ") + config->GetChannelLabel(vchannel) + "?\nThe pan of both channels is centered.";
            }
            else
            {
                peer = vchannel + link;
                if (!config->IsStereoLinkable(vchannel, peer))
                {
                    return;
                }
                uint left = min(vchannel, peer);
                message = String("Link ") + config->GetChannelLabel(vchannel, true) + " with " + config->GetChannelLabel(peer, true) + "?\n" +
                    config->GetChannelLabel(left + 1, true) + " takes over the settings of " + config->GetChannelLabel(left, true) + ", the faders are closed.";

                // linking can break up the pair the neighbour belongs to
                uint other;
                for (uint channel : {vchannel, peer})
                {
                    if (config->GetStereoPeer(channel, other))
                    {
                        message += String("\n") + config->GetChannelLabel(channel) + " is unlinked.";
                    }
                }
            }

            pending_vchannel = vchannel;
            pending_link = link;

            CloseConfirmBox();
            confirm_prev_page = prevPage;
            confirm_next_page = nextPage;
            prevPage = X32_PAGE::NONE;
            nextPage = X32_PAGE::NONE;

            confirm_box = lv_msgbox_create(objects.main);
            lv_msgbox_add_title(confirm_box, "Stereo Link");
            lv_msgbox_add_text(confirm_box, message.c_str());
            lv_obj_add_event_cb(lv_msgbox_add_footer_button(confirm_box, "Left: Cancel"), cancel_btn_event_cb, LV_EVENT_CLICKED, this);
            lv_obj_add_event_cb(lv_msgbox_add_footer_button(confirm_box, "Right: Confirm"), confirm_btn_event_cb, LV_EVENT_CLICKED, this);
            lv_obj_center(confirm_box);
        }

        void CloseConfirmBox()
        {
            if (confirm_box)
            {
                lv_msgbox_close_async(confirm_box);
                confirm_box = nullptr;
                prevPage = confirm_prev_page;
                nextPage = confirm_next_page;
            }
        }

        void ConfirmLink()
        {
            CloseConfirmBox();
            config->Set(CHANNEL_STEREO_LINK, (float)pending_link, pending_vchannel);
        }

        static void confirm_btn_event_cb(lv_event_t* e)
        {
            ((PageSetupMixerConfig*)lv_event_get_user_data(e))->ConfirmLink();
        }

        static void cancel_btn_event_cb(lv_event_t* e)
        {
            ((PageSetupMixerConfig*)lv_event_get_user_data(e))->CloseConfirmBox();
        }

        // touch: select the channel and ask for its default action
        static void channel_btn_event_cb(lv_event_t* e)
        {
            PageSetupMixerConfig* page = (PageSetupMixerConfig*)lv_event_get_user_data(e);
            uint vchannel = (uint)(uintptr_t)lv_obj_get_user_data((lv_obj_t*)lv_event_get_target(e));

            page->Select(vchannel);
            if (page->IsLinked(vchannel))
            {
                page->AskForLink(vchannel, 0);
            }
            else
            {
                page->AskForLink(vchannel, page->config->IsStereoLinkable(vchannel, vchannel + 1) ? 1 : -1);
            }
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
            // add the tab as 4th tab of the setup tabview
            lv_obj_t* tab = lv_tabview_add_tab(objects.setuptab, "Mixer Config");
            lv_obj_set_scrollbar_mode(tab, LV_SCROLLBAR_MODE_OFF);
            lv_obj_move_to_index(tab, 3);
            lv_obj_t* tab_btn = lv_tabview_get_tab_button(objects.setuptab, -1);
            if (tab_btn)
            {
                lv_obj_move_to_index(tab_btn, 3);
            }

            lv_obj_t* container = lv_obj_create(tab);
            lv_obj_update_layout(tab);
            lv_coord_t height = lv_obj_get_height(tab) - ENCODER_HINT_RESERVED_HEIGHT;
            lv_obj_set_size(container, LV_PCT(100), height > 0 ? height : 320);
            lv_obj_set_pos(container, 0, 0);
            lv_obj_set_style_pad_all(container, 10, LV_PART_MAIN);
            lv_obj_set_style_border_width(container, 0, LV_PART_MAIN);
            lv_obj_set_style_bg_opa(container, LV_OPA_TRANSP, LV_PART_MAIN);
            lv_obj_set_layout(container, LV_LAYOUT_FLEX);
            lv_obj_set_flex_flow(container, LV_FLEX_FLOW_ROW);
            lv_obj_set_flex_align(container, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

            for (uint c = 0; c < COLUMNS; c++)
            {
                lv_obj_t* column = lv_obj_create(container);
                lv_obj_set_size(column, LV_PCT(31), LV_PCT(100));
                lv_obj_set_style_pad_all(column, 5, LV_PART_MAIN);
                lv_obj_set_style_pad_bottom(column, 35, LV_PART_MAIN);
                lv_obj_set_style_bg_color(column, lv_color_make(30, 40, 50), LV_PART_MAIN);
                lv_obj_set_style_bg_opa(column, LV_OPA_COVER, LV_PART_MAIN);
                lv_obj_set_style_radius(column, 8, LV_PART_MAIN);
                lv_obj_set_layout(column, LV_LAYOUT_FLEX);
                lv_obj_set_flex_flow(column, LV_FLEX_FLOW_COLUMN);
                lv_obj_set_flex_align(column, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
                lv_obj_set_style_pad_row(column, 6, LV_PART_MAIN);
                lv_obj_set_scroll_dir(column, LV_DIR_VER);
                lv_obj_set_scrollbar_mode(column, LV_SCROLLBAR_MODE_AUTO);

                lv_obj_t* title = lv_label_create(column);
                lv_label_set_text(title, column_titles[c]);
                lv_obj_set_style_text_color(title, lv_color_make(255, 255, 255), LV_PART_MAIN);
                lv_obj_set_style_text_font(title, &lv_font_montserrat_14, LV_PART_MAIN);
                lv_obj_set_style_pad_bottom(title, 5, LV_PART_MAIN);

                for (uint vchannel = column_first[c]; vchannel < column_end[c]; vchannel++)
                {
                    lv_obj_t* btn = lv_btn_create(column);
                    lv_obj_set_size(btn, LV_PCT(95), 25);
                    lv_obj_set_style_radius(btn, 4, LV_PART_MAIN);
                    lv_obj_set_style_pad_all(btn, 0, LV_PART_MAIN);

                    lv_obj_t* label = lv_label_create(btn);
                    lv_obj_center(label);
                    lv_obj_set_style_text_font(label, &lv_font_montserrat_12, LV_PART_MAIN);

                    lv_obj_set_user_data(btn, (void*)(uintptr_t)vchannel);
                    lv_obj_add_event_cb(btn, channel_btn_event_cb, LV_EVENT_CLICKED, this);
                    buttons[vchannel] = btn;
                }
            }
        }

        void OnShow() override
        {
            config->SurfaceBindCustom(SurfaceElementId::DISPLAY_ENCODER_2, "");
            config->SurfaceBindCustom(SurfaceElementId::DISPLAY_ENCODER_4, "");
            config->SurfaceBindCustom(SurfaceElementId::DISPLAY_ENCODER_6, "");
            OnChange(true);
        }

        void OnHide() override
        {
            CloseConfirmBox();
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
                    ConfirmLink();
                    return;
                }
            }

            // only links change the buttons, a redraw would also reset the scroll position
            if (!force_update && !config->HasParameterChanged(CHANNEL_STEREO_LINK))
            {
                return;
            }

            // keep the selection on the first channel of a pair
            for (uint c = 0; c < COLUMNS; c++)
            {
                if (config->IsStereoLinkedRight(selected[c]))
                {
                    selected[c]--;
                }
            }

            for (uint vchannel = 0; vchannel < (uint)X32_VCHANNEL_BLOCK::SPECIAL; vchannel++)
            {
                UpdateButton(vchannel);
            }
            UpdateButtonLabels();
        }

        void OnChangeCustomEncoder(SurfaceElementId surface_element_id, int amount) override
        {
            for (uint c = 0; c < COLUMNS; c++)
            {
                if (surface_element_id == column_encoder[c])
                {
                    MoveSelection(c, amount);
                }
            }
        }

        void OnChangeCustomButton(SurfaceElementId surface_element_id) override
        {
            for (uint c = 0; c < COLUMNS; c++)
            {
                uint vchannel = selected[c];
                if (surface_element_id == column_button_next[c])
                {
                    selected_column = c;
                    AskForLink(vchannel, IsLinked(vchannel) ? 0 : 1);
                }
                else if (surface_element_id == column_button_prev[c] && !IsLinked(vchannel))
                {
                    selected_column = c;
                    AskForLink(vchannel, -1);
                }
            }
        }
};

}
