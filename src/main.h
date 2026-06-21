#pragma once

#include <stdint.h>

// includes for timer
#include <time.h>
#include <signal.h>

#include "../lib_ext/lvgl/lvgl.h"

namespace OMC
{
    void init10msTimer_NonGUI(void);

    void timer100msCallbackLvgl(_lv_timer_t* lv_timer);
    void timer50msCallbackLvgl(_lv_timer_t* lv_timer);
    void timer10msCallbackLvgl(_lv_timer_t* lv_timer);
}