#pragma once

#include "mixerparameter.h"
#include "../lib_ext/lvgl/lvgl.h"

using namespace std;

namespace OMC
{

class LVGLEncoderWidget {
    public:

        // GUI Handles
        lv_obj_t* Label;
        lv_obj_t* Slider;
        lv_obj_t* ButtonLabel;

        LVGLEncoderWidget(lv_obj_t* encoder_label, lv_obj_t* encoder_slider, lv_obj_t* encoder_button_label) {
            Label = encoder_label;
            Slider = encoder_slider;
            ButtonLabel = encoder_button_label;
        }
};

}