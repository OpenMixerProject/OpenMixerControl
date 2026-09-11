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

        void BindMainEncoders()
        {
            bool stereo = config->IsStereoLinkedMainRouted(config->GetUint(SELECTED_CHANNEL));

            config->SurfaceBind(SurfaceElementId::DISPLAY_ENCODER_1, MixerparameterAction::CHANGE_SELECTED_CHANNEL, CHANNEL_VOLUME);
            config->SurfaceBind(SurfaceElementId::DISPLAY_ENCODER_BUTTON_1, MixerparameterAction::TOGGLE_SELECTED_CHANNEL, CHANNEL_SEND_LR);
            config->SurfaceBind(SurfaceElementId::DISPLAY_ENCODER_2, MixerparameterAction::CHANGE_SELECTED_CHANNEL, stereo ? CHANNEL_STEREO_PAN : CHANNEL_PANORAMA);
            config->SurfaceBind(SurfaceElementId::DISPLAY_ENCODER_BUTTON_2, MixerparameterAction::RESET_SELECTED_CHANNEL, stereo ? CHANNEL_STEREO_PAN : CHANNEL_PANORAMA);

            if (stereo)
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
            stereoControlsBound = stereo;
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
            BindMainEncoders();
        }

        void OnChange(bool force_update) override
        {
            bool stereo = config->IsStereoLinkedMainRouted(config->GetUint(SELECTED_CHANNEL));
            if (force_update || stereo != stereoControlsBound ||
                config->HasParametersChanged({CHANNEL_LINKED, BUS_LINKED, CHANNEL_SEND_LR}))
            {
                BindMainEncoders();
            }
        }
};

}
