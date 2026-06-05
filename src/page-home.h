#pragma once
#include "page.h"
using namespace std;

class PageHome : public Page
{


    private:

        uint num_strips = 6;
        lv_obj_t* vumeters[12];
        lv_obj_t* labels[12];

        uint lastImageOffset[12] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

        uint channelindex[12] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};


    public:

        PageHome(PageBaseParameter* pagebasepar) : Page(pagebasepar)
        {
            nextPage = X32_PAGE::CONFIG;
            if (config->IsModelAnyWing()) {
                tabLayer0 = objects.wing_maintab;
                tabIndex0 = 0;
                tabLayer1 = NULL;
                tabIndex1 = 0;
            } else {
                tabLayer0 = objects.maintab;
                tabIndex0 = 0;
                tabLayer1 = objects.hometab;
                tabIndex1 = 0;
            }
        }

        void OnShow() override 
        {
            EncoderBind_NormalMode();
        }

        void EncoderBind_NormalMode()
        {
            config->SurfaceBind(SurfaceElementId::DISPLAY_ENCODER_1, MixerparameterAction::CHANGE, CHANNEL_VOLUME, channelindex[0]);
            config->SurfaceBind(SurfaceElementId::DISPLAY_ENCODER_2, MixerparameterAction::CHANGE, CHANNEL_VOLUME, channelindex[1]);
            config->SurfaceBind(SurfaceElementId::DISPLAY_ENCODER_3, MixerparameterAction::CHANGE, CHANNEL_VOLUME, channelindex[2]);
            config->SurfaceBind(SurfaceElementId::DISPLAY_ENCODER_4, MixerparameterAction::CHANGE, CHANNEL_VOLUME, channelindex[3]);
            config->SurfaceBind(SurfaceElementId::DISPLAY_ENCODER_5, MixerparameterAction::CHANGE, CHANNEL_VOLUME, channelindex[4]);
            config->SurfaceBind(SurfaceElementId::DISPLAY_ENCODER_6, MixerparameterAction::CHANGE, CHANNEL_VOLUME, channelindex[5]);

            config->SurfaceBind(SurfaceElementId::DISPLAY_ENCODER_BUTTON_1, MixerparameterAction::TOGGLE, CHANNEL_MUTE, channelindex[0]);
            config->SurfaceBind(SurfaceElementId::DISPLAY_ENCODER_BUTTON_2, MixerparameterAction::TOGGLE, CHANNEL_MUTE, channelindex[1]);
            config->SurfaceBind(SurfaceElementId::DISPLAY_ENCODER_BUTTON_3, MixerparameterAction::TOGGLE, CHANNEL_MUTE, channelindex[2]);
            config->SurfaceBind(SurfaceElementId::DISPLAY_ENCODER_BUTTON_4, MixerparameterAction::TOGGLE, CHANNEL_MUTE, channelindex[3]);
            config->SurfaceBind(SurfaceElementId::DISPLAY_ENCODER_BUTTON_5, MixerparameterAction::TOGGLE, CHANNEL_MUTE, channelindex[4]);
            config->SurfaceBind(SurfaceElementId::DISPLAY_ENCODER_BUTTON_6, MixerparameterAction::TOGGLE, CHANNEL_MUTE, channelindex[5]);
        }

        void EncoderBind_EditMode()
        {
            String edittext = String("\n\nSelect ") + String(LV_SYMBOL_REFRESH);

            // Show the Channelname
            config->SurfaceBindCustom(SurfaceElementId::DISPLAY_ENCODER_1, config->GetString(CHANNEL_NAME, channelindex[0]) + edittext);
            config->SurfaceBindCustom(SurfaceElementId::DISPLAY_ENCODER_2, config->GetString(CHANNEL_NAME, channelindex[1]) + edittext);
            config->SurfaceBindCustom(SurfaceElementId::DISPLAY_ENCODER_3, config->GetString(CHANNEL_NAME, channelindex[2]) + edittext);
            config->SurfaceBindCustom(SurfaceElementId::DISPLAY_ENCODER_4, config->GetString(CHANNEL_NAME, channelindex[3]) + edittext);
            config->SurfaceBindCustom(SurfaceElementId::DISPLAY_ENCODER_5, config->GetString(CHANNEL_NAME, channelindex[4]) + edittext);
            config->SurfaceBindCustom(SurfaceElementId::DISPLAY_ENCODER_6, config->GetString(CHANNEL_NAME, channelindex[5]) + edittext);

            // Button has no function in this mode
            config->SurfaceUnbind(SurfaceElementId::DISPLAY_ENCODER_BUTTON_1);
            config->SurfaceUnbind(SurfaceElementId::DISPLAY_ENCODER_BUTTON_2);
            config->SurfaceUnbind(SurfaceElementId::DISPLAY_ENCODER_BUTTON_3);
            config->SurfaceUnbind(SurfaceElementId::DISPLAY_ENCODER_BUTTON_4);
            config->SurfaceUnbind(SurfaceElementId::DISPLAY_ENCODER_BUTTON_5);
            config->SurfaceUnbind(SurfaceElementId::DISPLAY_ENCODER_BUTTON_6);
        }

        void OnInit() override
        {
            num_strips = config->IsModelAnyWing() ? 12 : 6;
            if (config->IsModelAnyWing()) {
                vumeters[0] = objects.wing_home_channelstrip_1__vumeter;
                vumeters[1] = objects.wing_home_channelstrip_2__vumeter;
                vumeters[2] = objects.wing_home_channelstrip_3__vumeter;
                vumeters[3] = objects.wing_home_channelstrip_4__vumeter;
                vumeters[4] = objects.wing_home_channelstrip_5__vumeter;
                vumeters[5] = objects.wing_home_channelstrip_6__vumeter;
                vumeters[6] = objects.wing_home_channelstrip_7__vumeter;
                vumeters[7] = objects.wing_home_channelstrip_8__vumeter;
                vumeters[8] = objects.wing_home_channelstrip_9__vumeter;
                vumeters[9] = objects.wing_home_channelstrip_10__vumeter;
                vumeters[10] = objects.wing_home_channelstrip_11__vumeter;
                vumeters[11] = objects.wing_home_channelstrip_12__vumeter;

                labels[0] = objects.wing_home_channelstrip_1__ch;
                labels[1] = objects.wing_home_channelstrip_2__ch;
                labels[2] = objects.wing_home_channelstrip_3__ch;
                labels[3] = objects.wing_home_channelstrip_4__ch;
                labels[4] = objects.wing_home_channelstrip_5__ch;
                labels[5] = objects.wing_home_channelstrip_6__ch;
                labels[6] = objects.wing_home_channelstrip_7__ch;
                labels[7] = objects.wing_home_channelstrip_8__ch;
                labels[8] = objects.wing_home_channelstrip_9__ch;
                labels[9] = objects.wing_home_channelstrip_10__ch;
                labels[10] = objects.wing_home_channelstrip_11__ch;
                labels[11] = objects.wing_home_channelstrip_12__ch;
            } else {
                vumeters[0] = objects.home_channelstrip_1__vumeter;
                vumeters[1] = objects.home_channelstrip_2__vumeter;
                vumeters[2] = objects.home_channelstrip_3__vumeter;
                vumeters[3] = objects.home_channelstrip_4__vumeter;
                vumeters[4] = objects.home_channelstrip_5__vumeter;
                vumeters[5] = objects.home_channelstrip_6__vumeter;
                for (int i = 6; i < 12; i++) {
                    vumeters[i] = NULL;
                }

                labels[0] = objects.home_channelstrip_1__ch;
                labels[1] = objects.home_channelstrip_2__ch;
                labels[2] = objects.home_channelstrip_3__ch;
                labels[3] = objects.home_channelstrip_4__ch;
                labels[4] = objects.home_channelstrip_5__ch;
                labels[5] = objects.home_channelstrip_6__ch;
                for (int i = 6; i < 12; i++) {
                    labels[i] = NULL;
                }
            }
        }

        void OnChange(bool force_update) override
        {
            if (config->HasParameterChanged(CHANNEL_NAME) || force_update)
            {
                for (uint i = 0; i < num_strips; i++) {
                    if (labels[i]) {
                        lv_label_set_text(labels[i], config->GetString(CHANNEL_NAME, channelindex[i]).c_str());
                    }
                }
            }

            if (config->HasParameterChanged(DISPLAY_UTILITY))
            {
                if (config->GetBool(DISPLAY_UTILITY))
                {
                    EncoderBind_EditMode();
                }
                else
                {
                    EncoderBind_NormalMode();
                }
            }
        }

        void OnChangeCustomEncoder(SurfaceElementId surface_element_id, int amount)
        {
            switch(surface_element_id)
            {
                case SurfaceElementId::DISPLAY_ENCODER_1:
                case SurfaceElementId::DISPLAY_ENCODER_2:
                case SurfaceElementId::DISPLAY_ENCODER_3:
                case SurfaceElementId::DISPLAY_ENCODER_4:
                case SurfaceElementId::DISPLAY_ENCODER_5:
                case SurfaceElementId::DISPLAY_ENCODER_6:
                    {
                        uint array_index = (uint)surface_element_id - (uint)SurfaceElementId::DISPLAY_ENCODER_1;
                        channelindex[array_index] = helper->CheckBoundaries(channelindex[array_index], amount, 0, MAX_VCHANNELS-1);

                        EncoderBind_EditMode();
                        OnChange(true);
                    }
                    break;
                default:
                    break;
            }
        }

        void OnUpdateMeters() override
        {
            for (uint i = 0; i < num_strips; i++)
            {
                if (!vumeters[i]) continue;
                float dbValue = helper->sample2Dbfs(mixer->dsp->rChannel[channelindex[i]].meterDecay);
                uint imageOffset = helper->rescale(dbValue, -100.0f, 10.0f, 0.0f, 31.0f);
                uint newImageOffset = imageOffset * -lv_obj_get_width(vumeters[i]);
             
                // only set new offset if it has changed
                if (newImageOffset != lastImageOffset[i])
                {
                    lv_image_set_offset_x(vumeters[i], newImageOffset);
                    lastImageOffset[i] = newImageOffset;
                }
            }
        }
};