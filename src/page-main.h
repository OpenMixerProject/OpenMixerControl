#pragma once

#include "page.h"
using namespace std;

namespace OMC
{

class PageMain : public Page
{
    using enum MP_ID;

    private:

        uint bankingSends = 0;
        uint bankingSendsBefore = 0;
        bool stereoControlsBound = false;

        // matrixes have no panorama, so their width is meaningless
        bool HasStereoWidth()
        {
            uint peer;
            uint selected = config->GetUint(SELECTED_CHANNEL);
            return selected < (uint)X32_VCHANNEL_BLOCK::MATRIX && config->GetStereoPeer(selected, peer);
        }

        void BindEncoders()
        {
            // stereo pairs get the width next to their balance
            stereoControlsBound = HasStereoWidth();
            MP_ID pan = CHANNEL_PANORAMA;

            config->SurfaceBind(SurfaceElementId::DISPLAY_ENCODER_1, MixerparameterAction::CHANGE_SELECTED_CHANNEL, CHANNEL_VOLUME);
            config->SurfaceBind(SurfaceElementId::DISPLAY_ENCODER_BUTTON_1, MixerparameterAction::TOGGLE_SELECTED_CHANNEL, CHANNEL_SEND_LR);
            config->SurfaceBind(SurfaceElementId::DISPLAY_ENCODER_2, MixerparameterAction::CHANGE_SELECTED_CHANNEL, pan);
            config->SurfaceBind(SurfaceElementId::DISPLAY_ENCODER_BUTTON_2, MixerparameterAction::RESET_SELECTED_CHANNEL, pan);

            if (stereoControlsBound)
            {
                config->SurfaceBind(SurfaceElementId::DISPLAY_ENCODER_3, MixerparameterAction::CHANGE_SELECTED_CHANNEL, CHANNEL_STEREO_WIDTH);
                config->SurfaceBind(SurfaceElementId::DISPLAY_ENCODER_BUTTON_3, MixerparameterAction::RESET_SELECTED_CHANNEL, CHANNEL_STEREO_WIDTH);
            }
            else
            {
                config->SurfaceUnbind(SurfaceElementId::DISPLAY_ENCODER_3);
                config->SurfaceUnbind(SurfaceElementId::DISPLAY_ENCODER_BUTTON_3);
            }

            config->SurfaceBind(SurfaceElementId::DISPLAY_ENCODER_4, MixerparameterAction::CHANGE_SELECTED_CHANNEL, CHANNEL_VOLUME_SUB);
            config->SurfaceBind(SurfaceElementId::DISPLAY_ENCODER_BUTTON_4, MixerparameterAction::TOGGLE_SELECTED_CHANNEL, CHANNEL_SEND_SUB);
        }

    public:

        PageMain(PageBaseParameter* pagebasepar) : Page(pagebasepar)
        {
            prevPage = X32_PAGE::SENDS;
            tabLayer0 = objects.maintab;
            tabIndex0 = 0;
            tabLayer1 = objects.hometab;
            tabIndex1 = 6;
            noLedOnRack = true;
        }

        void OnShow() override
        {
            BindEncoders();
        }

        void OnChange(bool force_update) override
        {
            if (HasStereoWidth() != stereoControlsBound)
            {
                BindEncoders();
            }
        }
};

}