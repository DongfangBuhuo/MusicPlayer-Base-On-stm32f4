#ifndef GUI_APP_H
#define GUI_APP_H

#include "../../Gui/lvgl/lvgl.h"

#ifdef __cplusplus
extern "C"
{
#endif

    // 声明图标资源
    LV_IMG_DECLARE(music_player);
    // LV_IMG_DECLARE(back_btn); // gui_app.c 不再需要这个，gui_music_player.c 自己声明了
    LV_IMG_DECLARE(setting_btn);  // 保留您添加的

    void createHome(void);
    void gui_app_init(void);
    void gui_app_return_home(void);  // 【关键修复】恢复此声明

#ifdef __cplusplus
}
#endif

#endif  // GUI_APP_H
