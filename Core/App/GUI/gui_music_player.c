#include "gui_music_player.h"
#include "gui_app.h"

// 声明外部资源
LV_IMG_DECLARE(back_btn);

static lv_obj_t *scr_player = NULL;

static void back_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED)
    {
        // 返回主页
        gui_app_return_home();
        scr_player = NULL;
    }
}

void gui_music_player_open(void)
{
    if (scr_player) return;

    scr_player = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr_player, lv_color_white(), 0);

    // --- 1. 返回按钮 (左上角) ---
    lv_obj_t *btn_back = lv_btn_create(scr_player);
    lv_obj_set_size(btn_back, 60, 60);
    lv_obj_set_pos(btn_back, 10, 10);  // 绝对坐标定位
    lv_obj_remove_style_all(btn_back);
    lv_obj_set_style_bg_opa(btn_back, LV_OPA_TRANSP, 0);
    lv_obj_add_event_cb(btn_back, back_event_cb, LV_EVENT_CLICKED, NULL);

    // 加载图片
    lv_obj_t *img_back = lv_image_create(btn_back);
    lv_image_set_src(img_back, &back_btn);
    lv_obj_center(img_back);
    lv_obj_add_flag(img_back, LV_OBJ_FLAG_EVENT_BUBBLE);

    // --- 2. 标题 (顶部居中) ---
    lv_obj_t *label_title = lv_label_create(scr_player);
    lv_label_set_text(label_title, "Music Player");
    lv_obj_set_style_text_color(label_title, lv_color_black(), 0);
    // 如果启用了 24 号字体，请取消下面这行的注释
    // lv_obj_set_style_text_font(label_title, &lv_font_montserrat_24, 0);
    lv_obj_align(label_title, LV_ALIGN_TOP_MID, 0, 25);  // 顶部居中，下移 25px

    // --- 3. 封面 (屏幕正中心) ---
    lv_obj_t *cover = lv_obj_create(scr_player);
    lv_obj_set_size(cover, 200, 200);
    lv_obj_align(cover, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_radius(cover, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(cover, lv_palette_main(LV_PALETTE_GREY), 0);

    lv_obj_t *label_cover = lv_label_create(cover);
    lv_label_set_text(label_cover, "Cover");
    lv_obj_center(label_cover);

    // 切换屏幕
    lv_scr_load_anim(scr_player, LV_SCR_LOAD_ANIM_NONE, 0, 0, false);
}
