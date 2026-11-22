/*
    ____                  __   ______ ___  
   / __ \                 \ \ / /___ \__ \ 
  | |  | |_ __   ___ _ __  \ V /  __) | ) |
  | |  | | '_ \ / _ \ '_ \  > <  |__ < / / 
  | |__| | |_) |  __/ | | |/ . \ ___) / /_ 
   \____/| .__/ \___|_| |_/_/ \_\____/____|
         | |                               
         |_|                               
  
  OpenX32 - The OpenSource Operating System for the Behringer X32 Audio Mixing Console
  Copyright 2025 OpenMixerProject
  https://github.com/OpenMixerProject/OpenX32
  
  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  version 3 as published by the Free Software Foundation.
  
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU General Public License for more details.
*/

#include "page.h"

namespace OMC
{

Page::Page(PageBaseParameter* pagebasepar) : X32Base(pagebasepar)
{
    helper->DEBUG_GUI(DEBUGLEVEL_NORMAL, "Page::Page()");
    surface = pagebasepar->surface;
}

void Page::Init()
{
    helper->DEBUG_GUI(DEBUGLEVEL_NORMAL, "Page::Init()");

    // build list of lvgl encoder widgets for easy enumeration
    lvgl_encoder_widgets[SurfaceElementId::DISPLAY_ENCODER_1] = new LVGLEncoderWidget(objects.widget1__label, objects.widget1__slider, objects.widget1__label_buttonpress);
    lvgl_encoder_widgets[SurfaceElementId::DISPLAY_ENCODER_2] = new LVGLEncoderWidget(objects.widget2__label, objects.widget2__slider, objects.widget2__label_buttonpress);
    lvgl_encoder_widgets[SurfaceElementId::DISPLAY_ENCODER_3] = new LVGLEncoderWidget(objects.widget3__label, objects.widget3__slider, objects.widget3__label_buttonpress);
    lvgl_encoder_widgets[SurfaceElementId::DISPLAY_ENCODER_4] = new LVGLEncoderWidget(objects.widget4__label, objects.widget4__slider, objects.widget4__label_buttonpress);
    lvgl_encoder_widgets[SurfaceElementId::DISPLAY_ENCODER_5] = new LVGLEncoderWidget(objects.widget5__label, objects.widget5__slider, objects.widget5__label_buttonpress);
    lvgl_encoder_widgets[SurfaceElementId::DISPLAY_ENCODER_6] = new LVGLEncoderWidget(objects.widget6__label, objects.widget6__slider, objects.widget6__label_buttonpress);

    OnInit();
    initDone = true;
}

void Page::Show()
{
    helper->DEBUG_GUI(DEBUGLEVEL_NORMAL, "Page::Show()");

    // Turn off Utility
    if (config->GetBool(DISPLAY_UTILITY))
    {
        config->Set(DISPLAY_UTILITY, false);
    }
    // Turn off Mute Group Assign Mode
    if (config->GetBool(DISPLAY_MUTE_GROUP))
    {
        config->Set(DISPLAY_MUTE_GROUP, false);
    }

    // open tab on Layer0
	if (tabLayer0 != nullptr)
    {
		lv_tabview_set_active(tabLayer0, tabIndex0, LV_ANIM_OFF);
	}

	// open tab on Layer1
	if (tabLayer1 != nullptr)
    {
		lv_tabview_set_active(tabLayer1, tabIndex1, LV_ANIM_OFF);
	}

    ResetEncoderBinding();

    // show page and trigger a complete update
    OnShow();
    Change(true);
}

void Page::ResetEncoderBinding()
{
    // Reset Surfacebinding of Display Encoders
    config->SurfaceUnbind(SurfaceElementId::DISPLAY_ENCODER_1);
    config->SurfaceUnbind(SurfaceElementId::DISPLAY_ENCODER_2);
    config->SurfaceUnbind(SurfaceElementId::DISPLAY_ENCODER_3);
    config->SurfaceUnbind(SurfaceElementId::DISPLAY_ENCODER_4);
    config->SurfaceUnbind(SurfaceElementId::DISPLAY_ENCODER_5);
    config->SurfaceUnbind(SurfaceElementId::DISPLAY_ENCODER_6);

    config->SurfaceUnbind(SurfaceElementId::DISPLAY_ENCODER_BUTTON_1);
    config->SurfaceUnbind(SurfaceElementId::DISPLAY_ENCODER_BUTTON_2);
    config->SurfaceUnbind(SurfaceElementId::DISPLAY_ENCODER_BUTTON_3);
    config->SurfaceUnbind(SurfaceElementId::DISPLAY_ENCODER_BUTTON_4);
    config->SurfaceUnbind(SurfaceElementId::DISPLAY_ENCODER_BUTTON_5);
    config->SurfaceUnbind(SurfaceElementId::DISPLAY_ENCODER_BUTTON_6);
}

void Page::Change(bool syncAll)
{
    helper->DEBUG_GUI(DEBUGLEVEL_TRACE, "Page::Change()");

    if (initDone)
    {
        // Go to next page
        if (config->HasParameterChanged(DISPLAY_RIGHT) && this->GetNextPage() != X32_PAGE::NONE)
        {
            config->SetParameterUnchanged(DISPLAY_RIGHT);
            config->Set(ACTIVE_PAGE, (uint)(this->GetNextPage()));
        }

        // Go to previous page
        if (config->HasParameterChanged(DISPLAY_LEFT) && this->GetPrevPage() != X32_PAGE::NONE)
        {
            config->SetParameterUnchanged(DISPLAY_LEFT);
            config->Set(ACTIVE_PAGE, (uint)(this->GetPrevPage()));
        }

        syncAll |= config->HasParameterChanged(SELECTED_CHANNEL);
        OnChange(syncAll);

        // Mute Groups
        if (config->HasParameterChanged(DISPLAY_MUTE_GROUP))
        {
            if (config->GetBool(DISPLAY_MUTE_GROUP))
            {
                // Show Display Encoders
                lv_obj_set_flag(objects.display_encoder_sliders, LV_OBJ_FLAG_HIDDEN, false);

                // Bind Encoder Widgets to Mute Groups

                // on Page Config -> edit mute group assignment
                if (config->GetUint(ACTIVE_PAGE) == (uint)X32_PAGE::CONFIG)
                {
                    for (uint i = 0; i < MAX_DISPLAY_ENCODER; i++)
                    {
                        config->SurfaceBindCustom(config->CalcSurfaceElementId(SurfaceElementId::DISPLAY_ENCODER_1, i), String(LV_SYMBOL_LIST) + String("\nAssign to\n"));
                        config->SurfaceBind(config->CalcSurfaceElementId(SurfaceElementId::DISPLAY_ENCODER_BUTTON_1, i),
                                            MixerparameterAction::TOGGLE_SELECTED_CHANNEL, config->MpCalcId(MUTE_GROUP_1, i));
                    }

                    
                }
                // on all other pages -> toggle mute group
                else
                {
                    for (uint i = 0; i < MAX_DISPLAY_ENCODER; i++)
                    {
                        config->SurfaceBindCustom(config->CalcSurfaceElementId(SurfaceElementId::DISPLAY_ENCODER_1, i), String(LV_SYMBOL_MUTE) + String("\nToggle\n"));
                        config->SurfaceBind(config->CalcSurfaceElementId(SurfaceElementId::DISPLAY_ENCODER_BUTTON_1, i),
                                            MixerparameterAction::TOGGLE, config->MpCalcId(MUTE_GROUP_1_MUTE, i));
                    }
                }

                // force reload of Encoder Widgets
                syncAll = true;
            }
            else
            {
                // rebind Encoder Widgets of page
                ResetEncoderBinding();
                OnShow();
                OnChange(true);
            }
        }

        if (!config->GetBool(DISPLAY_MUTE_GROUP))
        {
            // hide encoders if wanted
            lv_obj_set_flag(objects.display_encoder_sliders, LV_OBJ_FLAG_HIDDEN, hideEncoders);
        }

        // Utility Groups
        if (config->HasParameterChanged(DISPLAY_UTILITY))
        {
            if (config->GetBool(DISPLAY_UTILITY))
            {
                // Show Display Encoders
                lv_obj_set_flag(objects.display_encoder_sliders, LV_OBJ_FLAG_HIDDEN, false);

                // Bind Encoder Widgets to DCA Groups

                // on Page Config -> edit mute group assignment
                if (config->GetUint(ACTIVE_PAGE) == (uint)X32_PAGE::CONFIG)
                {
                    for (uint i = 0; i < MAX_DISPLAY_ENCODER; i++)
                    {
                        config->SurfaceBindCustom(config->CalcSurfaceElementId(SurfaceElementId::DISPLAY_ENCODER_1, i), String(LV_SYMBOL_LIST) + String("\nAssign to\n"));
                        config->SurfaceBind(config->CalcSurfaceElementId(SurfaceElementId::DISPLAY_ENCODER_BUTTON_1, i),
                                            MixerparameterAction::TOGGLE_SELECTED_CHANNEL, config->MpCalcId(DCA_GROUP_1, i));
                    }

                    
                }

                // force reload of Encoder Widgets
                syncAll = true;
            }
            else
            {
                // rebind Encoder Widgets of page
                ResetEncoderBinding();
                OnShow();
                OnChange(true);
            }
        }

        SyncEncoderWidgets(syncAll);
    }
}

void Page::ChangeCustomButton(SurfaceElementId surface_element_id)
{
    OnChangeCustomButton(surface_element_id);
    SyncEncoderWidgets(false);
}

void Page::ChangeCustomEncoder(SurfaceElementId surface_element_id, int amount)
{
    OnChangeCustomEncoder(surface_element_id, amount);
    SyncEncoderWidgets(false);
}

void Page::UpdateMeters() {
    if (initDone) {
        OnUpdateMeters();
    }
}

X32_PAGE Page::GetNextPage() {
    return nextPage;
}

X32_PAGE Page::GetPrevPage() {
    return prevPage;
}

void Page::SyncEncoderWidgets(bool force)
{
    if (hideEncoders && !config->GetBool(DISPLAY_MUTE_GROUP)) {
        return;
    }

    SyncEncoderWidget(SurfaceElementId::DISPLAY_ENCODER_1, SurfaceElementId::DISPLAY_ENCODER_BUTTON_1, force);
    SyncEncoderWidget(SurfaceElementId::DISPLAY_ENCODER_2, SurfaceElementId::DISPLAY_ENCODER_BUTTON_2, force);
    SyncEncoderWidget(SurfaceElementId::DISPLAY_ENCODER_3, SurfaceElementId::DISPLAY_ENCODER_BUTTON_3, force);
    SyncEncoderWidget(SurfaceElementId::DISPLAY_ENCODER_4, SurfaceElementId::DISPLAY_ENCODER_BUTTON_4, force);
    SyncEncoderWidget(SurfaceElementId::DISPLAY_ENCODER_5, SurfaceElementId::DISPLAY_ENCODER_BUTTON_5, force);
    SyncEncoderWidget(SurfaceElementId::DISPLAY_ENCODER_6, SurfaceElementId::DISPLAY_ENCODER_BUTTON_6, force);
}

void Page::SyncEncoderWidget(SurfaceElementId elementIdEncoder, SurfaceElementId elementIdButton, bool force)
{
    SurfaceBindingParameter* surface_binding_encoder = config->GetSurfaceBinding(elementIdEncoder);
    SurfaceBindingParameter* surface_binding_button = config->GetSurfaceBinding(elementIdButton);
    LVGLEncoderWidget* lvgl_encoder_widget = lvgl_encoder_widgets[elementIdEncoder];

    // if Surface binding has changed -> refresh all
    if (config->HasSurfaceBindingChanged(elementIdEncoder) || config->HasSurfaceBindingChanged(elementIdButton))
    {
        force = true;
    }
    
    // encoder is not bound
    if (surface_binding_encoder == 0 || surface_binding_encoder->mp_action == MixerparameterAction::NONE)
    {
        ClearEncoder(lvgl_encoder_widget);
    }
    else
    {
        // Custom Display Encoder -> just show the label and clear slider and button
        if (surface_binding_encoder->mp_action == MixerparameterAction::CUSTOM)
        {
            // set label
            lv_label_set_text(lvgl_encoder_widget->Label, surface_binding_encoder->custom_label.c_str());

            // hide slider
            lv_obj_set_flag(lvgl_encoder_widget->Slider, LV_OBJ_FLAG_HIDDEN, true);
        }
        else
        {
            uint index = config->ParameterCalcIndex(surface_binding_encoder);
            MP_ID id = config->ParameterCalcId(surface_binding_encoder);

            if(config->HasParameterChanged(id, index) || force)
            {
                if (id == MP_ID::NONE)
                {
                    ClearEncoder(lvgl_encoder_widget);
                } 
                else
                { 
                    Mixerparameter* parameter = config->GetParameter(id);

                    lv_label_set_text(lvgl_encoder_widget->Label, parameter->GetLabelAndValue(index).c_str());

                    // Hide or Show Slider 
                    if (parameter->GetHideEncoderSlider())
                    {
                        lv_obj_set_flag(lvgl_encoder_widget->Slider, LV_OBJ_FLAG_HIDDEN, true);
                    }
                    else 
                    {
                        lv_obj_set_flag(lvgl_encoder_widget->Slider, LV_OBJ_FLAG_HIDDEN, false);
                        lv_slider_set_value(lvgl_encoder_widget->Slider, parameter->GetPercent(index), LV_ANIM_OFF);          
                    }
                }
            }
        }
    }
    
    // button is not bound
    if (surface_binding_button == 0 || surface_binding_button->mp_action == MixerparameterAction::NONE)
    {
        ClearEncoderButton(lvgl_encoder_widget);
    }
    else
    {
        MP_ID id = config->ParameterCalcId(surface_binding_button);
        uint index = config->ParameterCalcIndex(surface_binding_button);
            
        if (config->HasParameterChanged(id, index) || force)
        {
            if (id == MP_ID::NONE)
            {
                ClearEncoderButton(lvgl_encoder_widget);
            }
            else
            {
                MixerparameterAction action = surface_binding_button->mp_action;
                Mixerparameter* parameter_button = config->GetParameter(id);
                String text;

                // Button Text
                if (action == MixerparameterAction::RESET ||
                    action == MixerparameterAction::RESET_SELECTED_CHANNEL)
                {
                    text = parameter_button->GetResetLabel(index);
                }
                else
                {
                    text = parameter_button->GetName();
                }
                lv_label_set_text(lvgl_encoder_widget->ButtonLabel, text.c_str());

                // Button Highlight
                if (parameter_button->GetBool(index))
                {
                    add_style_label_bg_yellow(lvgl_encoder_widget->ButtonLabel);
                }
                else
                {
                    remove_style_label_bg_yellow(lvgl_encoder_widget->ButtonLabel);
                }
            }
        }
    }
}

void Page::ClearEncoder(LVGLEncoderWidget *binding)
{
    lv_label_set_text(binding->Label, "");
    lv_obj_set_flag(binding->Slider, LV_OBJ_FLAG_HIDDEN, true);
}

void Page::ClearEncoderButton(LVGLEncoderWidget *binding)
{
    lv_label_set_text(binding->ButtonLabel, "");
    remove_style_label_bg_yellow(binding->ButtonLabel);
}

static lv_obj_t* g_warning_banner = nullptr;
static lv_obj_t* g_warning_label = nullptr;
static lv_timer_t* g_warning_timer = nullptr;

static void warning_banner_timer_cb(lv_timer_t* timer)
{
    (void)timer;
    if (g_warning_banner != nullptr)
    {
        lv_obj_set_flag(g_warning_banner, LV_OBJ_FLAG_HIDDEN, true);
    }
    // LVGL auto-deletes one-shot timers (repeat_count == 1) after this callback returns
    g_warning_timer = nullptr;
}

void Page::ShowWarningBanner(const String& message)
{
    if (message.length() == 0)
    {
        return;
    }

    helper->Error("%s\n", message.c_str());

    if (objects.main == nullptr)
    {
        return;
    }

    if (g_warning_banner == nullptr)
    {
        g_warning_banner = lv_obj_create(lv_layer_top());
        lv_obj_set_size(g_warning_banner, DISPLAY_RESOLUTION_X - 40, LV_SIZE_CONTENT);
        lv_obj_align(g_warning_banner, LV_ALIGN_TOP_MID, 0, 38);
        lv_obj_set_style_bg_color(g_warning_banner, lv_palette_main(LV_PALETTE_AMBER), 0);
        lv_obj_set_style_bg_opa(g_warning_banner, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(g_warning_banner, lv_palette_darken(LV_PALETTE_ORANGE, 3), 0);
        lv_obj_set_style_border_width(g_warning_banner, 2, 0);
        lv_obj_set_style_radius(g_warning_banner, 6, 0);
        lv_obj_set_style_pad_ver(g_warning_banner, 6, 0);
        lv_obj_set_style_pad_hor(g_warning_banner, 12, 0);
        lv_obj_remove_flag(g_warning_banner, LV_OBJ_FLAG_SCROLLABLE);

        g_warning_label = lv_label_create(g_warning_banner);
        lv_obj_set_style_text_color(g_warning_label, lv_color_black(), 0);
        lv_label_set_long_mode(g_warning_label, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(g_warning_label, DISPLAY_RESOLUTION_X - 68);
        lv_obj_set_style_text_align(g_warning_label, LV_TEXT_ALIGN_CENTER, 0);
    }

    String bannerText = String(LV_SYMBOL_WARNING) + String(" ") + message;
    lv_label_set_text(g_warning_label, bannerText.c_str());
    lv_obj_set_flag(g_warning_banner, LV_OBJ_FLAG_HIDDEN, false);

    if (g_warning_timer != nullptr)
    {
        lv_timer_delete(g_warning_timer);
        g_warning_timer = nullptr;
    }
    g_warning_timer = lv_timer_create(warning_banner_timer_cb, 4500, nullptr);
    lv_timer_set_repeat_count(g_warning_timer, 1);
}

}