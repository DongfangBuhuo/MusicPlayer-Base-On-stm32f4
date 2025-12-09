/*
 * gui_camera.c
 *
 *  Created on: Dec 9, 2025
 *      Author: Administrator
 */
#include "gui_camera.h"
#include "gui_app.h"
static lv_obj_t *scr_camera = NULL;

static void back_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED)
    {
        gui_app_return_home();
        scr_camera = NULL;
    }
}

void gui_camera_open()
{
    if (scr_camera) return;
    scr_camera = lv_obj_create(NULL);
    lv_obj_set_layout(scr_camera, LV_LAYOUT_NONE);
    lv_obj_set_style_bg_color(scr_camera, lv_color_white(), 0);

    // --- 1. 返回按钮 (左上角) ---
    lv_obj_t *btn_back = lv_btn_create(scr_camera);
    lv_obj_set_size(btn_back, 50, 50);
    lv_obj_set_pos(btn_back, 20, 20);
    lv_obj_set_style_bg_opa(btn_back, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btn_back, 0, 0);
    lv_obj_set_style_shadow_width(btn_back, 0, 0);
    lv_obj_add_event_cb(btn_back, back_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *img_back = lv_image_create(btn_back);
    lv_image_set_src(img_back, &back_btn);
    lv_obj_center(img_back);
    lv_obj_add_flag(img_back, LV_OBJ_FLAG_EVENT_BUBBLE);

    lv_scr_load(scr_camera);
}
