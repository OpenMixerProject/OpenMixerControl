#ifndef EEZ_LVGL_UI_GUI_H
#define EEZ_LVGL_UI_GUI_H

#include <../../../lib_ext/lvgl/lvgl.h>

#include "screens.h"

#ifdef __cplusplus
extern "C" {
#endif

void ui_init(bool is_wing);
void ui_tick();

void loadScreen(enum ScreensEnum screenId);

#ifdef __cplusplus
}
#endif

#endif // EEZ_LVGL_UI_GUI_H