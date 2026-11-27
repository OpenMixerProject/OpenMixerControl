#pragma once

#include "page.h"
using namespace std;

namespace OMC
{

class PageConfig : public Page
{
    private:
        
        uint lastImageOffset;
        uint lastImageOffsetRight;
        int lastStereo = -1;

        // second meter for the right channel of a stereo pair, both meters are shown side by side with half width
        lv_obj_t* vumeterRight = nullptr;

        lv_obj_t* config_mute_group[MUTE_GROUPS];
        lv_obj_t* config_dca_group[8];

        int get_vu_pixels_non_linear(int dbfs)
        {                
            // 2. Auf den GUI-Bereich begrenzen
            if (dbfs > 0)  dbfs = 0;
            if (dbfs < -60) dbfs = -60;
            
            // 3. Versatz einrechnen, sodass -60 dBFS -> Index 0 wird
            int lut_index = dbfs + 60;
            
            // 4. Skalierten Pixelwert aus der Tabelle holen
            int scaled_pixels = dbfs_to_pixel_lut[lut_index];
            
            // 5. Zurückskalieren (Division durch 2) mittels schnellem Bit-Shift nach rechts.
            // Falls deine GUI-Engine Fließkommazahlen für Pixel erlaubt (z.B. Qt/QML oder Cairo),
            // könntest du hier auch: return (float)scaled_pixels / 2.0f; nutzen.
            return scaled_pixels >> 1; 
        }

        const uint8_t dbfs_to_pixel_lut[61] = {
            0,  /* -60 dBFS (0.0 px) */
            0,  /* -59 dBFS */
            1,  /* -58 dBFS */
            1,  /* -57 dBFS */
            1,  /* -56 dBFS */
            2,  /* -55 dBFS */
            2,  /* -54 dBFS */
            3,  /* -53 dBFS */
            3,  /* -52 dBFS */
            3,  /* -51 dBFS */
            4,  /* -50 dBFS */
            4,  /* -49 dBFS */
            5,  /* -48 dBFS */
            5,  /* -47 dBFS */
            5,  /* -46 dBFS */
            6,  /* -45 dBFS */
            6,  /* -44 dBFS */
            7,  /* -43 dBFS */
            7,  /* -42 dBFS */
            7,  /* -41 dBFS */
            8,  /* -40 dBFS */
            8,  /* -39 dBFS */
            9,  /* -38 dBFS */
            9,  /* -37 dBFS */
            9,  /* -36 dBFS */
            10, /* -35 dBFS */
            10, /* -34 dBFS */
            10, /* -33 dBFS */
            10, /* -32 dBFS */
            10, /* -31 dBFS */
            10, /* -30 dBFS (5.0 px) */ // Dein Stützpunkt
            11, /* -29 dBFS */
            12, /* -28 dBFS */
            13, /* -27 dBFS */
            14, /* -26 dBFS */
            15, /* -25 dBFS */
            16, /* -24 dBFS */
            17, /* -23 dBFS */
            17, /* -22 dBFS */
            18, /* -21 dBFS */
            19, /* -20 dBFS */
            20, /* -119 dBFS */
            21, /* -18 dBFS (10.5 px) */ // Dein Stützpunkt
            23, /* -17 dBFS */
            24, /* -16 dBFS */
            26, /* -15 dBFS */
            28, /* -14 dBFS */
            29, /* -13 dBFS */
            31, /* -12 dBFS (15.5 px) */ // Dein Stützpunkt
            33, /* -11 dBFS */
            35, /* -10 dBFS */
            37, /* -9 dBFS */
            38, /* -8 dBFS */
            40, /* -7 dBFS */
            42, /* -6 dBFS (21.0 px) */  // Dein Stützpunkt
            45, /* -5 dBFS */
            49, /* -4 dBFS */
            52, /* -3 dBFS (26.0 px) */  // Dein Stützpunkt
            55, /* -2 dBFS */
            57, /* -1 dBFS */
            60  /* 0 dBFS (30.0 px) */  // Dein Stützpunkt
        };

    public:

        PageConfig(PageBaseParameter* pagebasepar) : Page(pagebasepar)
        {
            prevPage = X32_PAGE::HOME;
            nextPage = X32_PAGE::GATE;
            tabLayer0 = objects.maintab;
            tabIndex0 = 0;
            tabLayer1 = objects.hometab;
            tabIndex1 = 1;
            noLedOnRack = true;
        }

        void OnInit() override
        {
            vumeterRight = lv_image_create(lv_obj_get_parent(objects.config_vumeter));
            lv_image_set_src(vumeterRight, lv_image_get_src(objects.config_vumeter));
            lv_image_set_inner_align(vumeterRight, LV_IMAGE_ALIGN_TOP_LEFT);
            lv_obj_set_size(vumeterRight, VUMETER_HALF, LV_SIZE_CONTENT);
            lv_obj_set_pos(vumeterRight, lv_obj_get_x_aligned(objects.config_vumeter) + VUMETER_HALF, lv_obj_get_y_aligned(objects.config_vumeter));
            lv_obj_set_flag(vumeterRight, LV_OBJ_FLAG_HIDDEN, true);

            config_mute_group[0] = objects.config_mute_group_1;
            config_mute_group[1] = objects.config_mute_group_2;
            config_mute_group[2] = objects.config_mute_group_3;
            config_mute_group[3] = objects.config_mute_group_4;
            config_mute_group[4] = objects.config_mute_group_5;
            config_mute_group[5] = objects.config_mute_group_6;

            config_dca_group[0] = objects.config_dca_group_1;
            config_dca_group[1] = objects.config_dca_group_2;
            config_dca_group[2] = objects.config_dca_group_3;
            config_dca_group[3] = objects.config_dca_group_4;
            config_dca_group[4] = objects.config_dca_group_5;
            config_dca_group[5] = objects.config_dca_group_6;
            config_dca_group[6] = objects.config_dca_group_7;
            config_dca_group[7] = objects.config_dca_group_8;
        }

        void OnShow() override 
        {
            config->SurfaceBind(SurfaceElementId::DISPLAY_ENCODER_1, MixerparameterAction::CHANGE, SELECTED_CHANNEL);
            config->SurfaceBind(SurfaceElementId::DISPLAY_ENCODER_2, MixerparameterAction::CHANGE_SELECTED_CHANNEL, CHANNEL_GAIN);
            config->SurfaceBind(SurfaceElementId::DISPLAY_ENCODER_BUTTON_2, MixerparameterAction::TOGGLE_SELECTED_CHANNEL, CHANNEL_PHANTOM);
            config->SurfaceBind(SurfaceElementId::DISPLAY_ENCODER_3, MixerparameterAction::CHANGE_SELECTED_CHANNEL, ROUTING_DSP_INPUT);
            config->SurfaceBind(SurfaceElementId::DISPLAY_ENCODER_BUTTON_3, MixerparameterAction::TOGGLE_SELECTED_CHANNEL, CHANNEL_PHASE_INVERT);
            config->SurfaceBind(SurfaceElementId::DISPLAY_ENCODER_4, MixerparameterAction::CHANGE_SELECTED_CHANNEL, DELAY_DSP_INPUT);
            config->SurfaceBind(SurfaceElementId::DISPLAY_ENCODER_BUTTON_4, MixerparameterAction::TOGGLE_SELECTED_CHANNEL, CHANNEL_SOLO);
            config->SurfaceBind(SurfaceElementId::DISPLAY_ENCODER_5, MixerparameterAction::CHANGE_SELECTED_CHANNEL, CHANNEL_VOLUME);
            config->SurfaceBind(SurfaceElementId::DISPLAY_ENCODER_BUTTON_5, MixerparameterAction::TOGGLE_SELECTED_CHANNEL, CHANNEL_MUTE);
            config->SurfaceBind(SurfaceElementId::DISPLAY_ENCODER_6, MixerparameterAction::CHANGE_SELECTED_CHANNEL, CHANNEL_PANORAMA);
            config->SurfaceBind(SurfaceElementId::DISPLAY_ENCODER_BUTTON_6, MixerparameterAction::RESET_SELECTED_CHANNEL, CHANNEL_PANORAMA);
        }

        void OnChange(bool force_update) override
        {

            using enum MP_ID;

            uint8_t chanIndex = config->GetUint(MP_ID::SELECTED_CHANNEL);

            if (config->HasParameterChanged(SELECTED_CHANNEL))
            {
                force_update = true;
            }

            if (config->HasParameterChanged(CHANNEL_GAIN, chanIndex) || force_update)
            {
                lv_label_set_text(objects.current_channel_gain, config->GetParameter(CHANNEL_GAIN)->GetFormatedValue(chanIndex).c_str());
                lv_image_set_rotation(objects.config_gain_knob, helper->rescale(config->GetParameter(CHANNEL_GAIN)->GetFloat(chanIndex), -12.0f, 45.5f, 0.0f, 2900.0f));
            }

            if (config->HasParameterChanged(CHANNEL_PHANTOM, chanIndex) || force_update)
            {
                lv_image_set_offset_x(objects.config_phantom_button, config->GetUint(CHANNEL_PHANTOM, chanIndex) * -lv_obj_get_width(objects.config_phantom_button));
                lv_image_set_offset_x(objects.config_phantom_checkbox, config->GetUint(CHANNEL_PHANTOM, chanIndex) * -lv_obj_get_width(objects.config_phantom_checkbox));
            }

            if (config->HasParameterChanged(CHANNEL_PHASE_INVERT, chanIndex) || force_update)
            {
                lv_image_set_offset_x(objects.config_phase_checkbox, config->GetUint(CHANNEL_PHASE_INVERT, chanIndex) * -lv_obj_get_width(objects.config_phase_checkbox));
            }

            if (config->HasParameterChanged(CHANNEL_PANORAMA, chanIndex) || force_update)
            {
                lv_label_set_text(objects.current_channel_pan_bal, config->GetParameter(CHANNEL_PANORAMA)->GetFormatedValue(chanIndex).c_str());
                lv_image_set_rotation(objects.config_pan_knob, helper->rescale(config->GetFloat(CHANNEL_PANORAMA, chanIndex), -100.0f, 100.0f, 0.0f, 2900.0f));
            }

            if (config->HasParameterChanged(CHANNEL_VOLUME, chanIndex) || force_update)
            {
                lv_label_set_text(objects.current_channel_volume, config->GetParameter(CHANNEL_VOLUME)->GetFormatedValue(chanIndex).c_str());
                lv_image_set_rotation(objects.config_volume_knob, helper->rescale(config->GetFloat(CHANNEL_VOLUME, chanIndex), -100.0f, 10.0f, 0.0f, 2900.0f));

                uint imageOffset = helper->rescale(config->GetFloat(CHANNEL_VOLUME, chanIndex), -100.0f, 10.0f, 0.0f, 18.0f);
                lv_image_set_offset_x(objects.openx32_demo_knob, imageOffset * -lv_obj_get_width(objects.openx32_demo_knob));
            }

            if (config->HasParameterChanged(CHANNEL_MUTE, chanIndex) || force_update)
            {
                lv_image_set_offset_x(objects.config_mute_checkbox, config->GetUint(CHANNEL_MUTE, chanIndex) * -lv_obj_get_width(objects.config_mute_checkbox));
            }

            for (uint i = 0; i < MUTE_GROUPS; i++)
            {
                if (config->HasParameterChanged(config->MpCalcId(MUTE_GROUP_1, i), chanIndex) || force_update)
                {
                    lv_obj_set_state(config_mute_group[i], LV_STATE_CHECKED, config->GetBool(config->MpCalcId(MUTE_GROUP_1, i), chanIndex));
                }
            }

            for (uint i = 0; i < DCA_GROUPS; i++)
            {
                if (config->HasParameterChanged(config->MpCalcId(DCA_GROUP_1, i), chanIndex) || force_update)
                {
                    lv_obj_set_state(config_dca_group[i], LV_STATE_CHECKED, config->GetBool(config->MpCalcId(DCA_GROUP_1, i), chanIndex));
                }
            }

        }

        static constexpr lv_coord_t VUMETER_WIDTH = 40; // width of one frame of the meter image
        static constexpr lv_coord_t VUMETER_HALF = VUMETER_WIDTH / 2;

        void SetMeter(lv_obj_t* meter, int dbValue, lv_coord_t shift, uint& last)
        {
            uint newImageOffset = get_vu_pixels_non_linear(dbValue) * -VUMETER_WIDTH + shift;

            // only set new offset if it has changed
            if (newImageOffset != last)
            {
                lv_image_set_offset_x(meter, newImageOffset);
                last = newImageOffset;
            }
        }

        void OnUpdateMeters() override
        {
            uint selected = config->GetUint(MP_ID::SELECTED_CHANNEL);
            uint peer;
            bool stereo = config->GetStereoPeer(selected, peer);

            // a stereo pair shows a split meter: the middle of the left frame for the left, of the right frame for the right channel
            if ((int)stereo != lastStereo)
            {
                lv_obj_set_width(objects.config_vumeter, stereo ? VUMETER_HALF : VUMETER_WIDTH);
                lv_obj_set_flag(vumeterRight, LV_OBJ_FLAG_HIDDEN, !stereo);
                lastStereo = stereo;
                lastImageOffset = lastImageOffsetRight = UINT_MAX;
            }

            SetMeter(objects.config_vumeter, config->GetInt(MP_ID::CHANNEL_METER_DECAYED_POST_GAIN, selected), stereo ? -VUMETER_HALF / 2 : 0, lastImageOffset);
            if (stereo)
            {
                SetMeter(vumeterRight, config->GetInt(MP_ID::CHANNEL_METER_DECAYED_POST_GAIN, peer), -VUMETER_HALF / 2, lastImageOffsetRight);
            }
        }
};

}