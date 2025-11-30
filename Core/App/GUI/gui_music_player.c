#include "gui_music_player.h"
#include "gui_app.h"
#include "../Player/music_player.h"
// 强制启用并包含中文字体
#define LV_FONT_SOURCE_HAN_SANS_SC_14_CJK 1
#include "../../Gui/lvgl/src/font/lv_font_source_han_sans_sc_14_cjk.c"

// 声明外部资源
LV_IMG_DECLARE(back_btn);
LV_IMG_DECLARE(play_music_btn);
LV_IMG_DECLARE(pause_btn);
LV_IMG_DECLARE(next_btn);
LV_IMG_DECLARE(previous_btn);
LV_IMG_DECLARE(music_player);  // 封面图标
LV_IMG_DECLARE(music_list);    // 列表图标
LV_IMG_DECLARE(setting_btn);   // 设置图标

static lv_obj_t *scr_player = NULL;
static lv_obj_t *btn_play = NULL;
static lv_obj_t *img_play = NULL;
static lv_obj_t *cover = NULL;  // 封面图片对象
static lv_anim_t cover_anim;    // 封面旋转动画
static bool is_playing = false;
static int32_t current_cover_angle = 0;

// 音量变量 (0-100)
static int32_t vol_speaker = 0;
static int32_t vol_headphone = 50;

// 封面旋转动画回调函数
static void cover_anim_cb(void *obj, int32_t value)
{
    lv_image_set_rotation(obj, value);
    current_cover_angle = value;
}

static void back_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED)
    {
        gui_app_return_home();
        scr_player = NULL;
    }
}

static void play_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED)
    {
        is_playing = !is_playing;
        if (is_playing)
        {
            lv_image_set_src(img_play, &pause_btn);
            int32_t start = current_cover_angle % 3600;
            lv_anim_set_values(&cover_anim, start, start + 3600);
            lv_anim_start(&cover_anim);
        }
        else
        {
            lv_image_set_src(img_play, &play_music_btn);
            lv_anim_del(cover, NULL);
        }
    }
}

// 列表按钮回调 (暂时为空)
static void list_event_cb(lv_event_t *e)
{
    // TODO: 显示音乐列表
}

// 关闭设置弹窗
static void close_settings_cb(lv_event_t *e)
{
    lv_obj_t *mask = lv_event_get_user_data(e);
    lv_obj_del(mask);
}

// 音量滑块回调
static void vol_slider_cb(lv_event_t *e)
{
    lv_obj_t *slider = lv_event_get_target(e);
    int32_t *val_ptr = (int32_t *)lv_event_get_user_data(e);
    *val_ptr = lv_slider_get_value(slider);

    // 映射 0-100 到 0-33
    uint8_t hw_volume = (uint8_t)(*val_ptr * 33 / 100);

    // 调用底层音量设置
    if (val_ptr == &vol_speaker)
    {
        music_player_set_speaker_volume(hw_volume);
    }
    else if (val_ptr == &vol_headphone)
    {
        music_player_set_headphone_volume(hw_volume);
    }
}

// 设置按钮回调：显示弹窗
static void settings_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED)
    {
        // 1. 创建全屏半透明遮罩 (Mask)
        lv_obj_t *mask = lv_obj_create(lv_scr_act());  // 创建在当前屏幕上
        lv_obj_set_size(mask, 480, 800);
        lv_obj_set_style_bg_opa(mask, LV_OPA_50, 0);
        lv_obj_set_style_bg_color(mask, lv_color_black(), 0);
        lv_obj_set_style_border_width(mask, 0, 0);
        lv_obj_add_event_cb(mask, close_settings_cb, LV_EVENT_CLICKED, mask);  // 点击遮罩关闭

        // 2. 创建设置面板 (Panel)
        lv_obj_t *panel = lv_obj_create(mask);
        lv_obj_set_size(panel, 360, 300);
        lv_obj_center(panel);
        lv_obj_set_style_bg_color(panel, lv_color_white(), 0);
        lv_obj_set_style_radius(panel, 20, 0);
        lv_obj_clear_flag(panel, LV_OBJ_FLAG_CLICKABLE);  // 面板本身不响应点击关闭

        // 标题
        lv_obj_t *title = lv_label_create(panel);
        lv_label_set_text(title, "Volume Settings");
        // lv_obj_set_style_text_font(title, &lv_font_source_han_sans_sc_14_cjk, 0); // 暂时切回默认字体
        lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

        // --- 扬声器音量 ---
        lv_obj_t *label_spk = lv_label_create(panel);
        lv_label_set_text(label_spk, "Speaker");
        // lv_obj_set_style_text_font(label_spk, &lv_font_source_han_sans_sc_14_cjk, 0);
        lv_obj_align(label_spk, LV_ALIGN_LEFT_MID, 10, -40);

        lv_obj_t *slider_spk = lv_slider_create(panel);
        lv_obj_set_size(slider_spk, 200, 15);
        lv_obj_align(slider_spk, LV_ALIGN_RIGHT_MID, -10, -40);
        lv_slider_set_value(slider_spk, vol_speaker, LV_ANIM_OFF);
        lv_obj_add_event_cb(slider_spk, vol_slider_cb, LV_EVENT_VALUE_CHANGED, &vol_speaker);

        // --- 耳机音量 ---
        lv_obj_t *label_hp = lv_label_create(panel);
        lv_label_set_text(label_hp, "Headphone");
        // lv_obj_set_style_text_font(label_hp, &lv_font_source_han_sans_sc_14_cjk, 0);
        lv_obj_align(label_hp, LV_ALIGN_LEFT_MID, 10, 40);

        lv_obj_t *slider_hp = lv_slider_create(panel);
        lv_obj_set_size(slider_hp, 200, 15);
        lv_obj_align(slider_hp, LV_ALIGN_RIGHT_MID, -10, 40);
        lv_slider_set_value(slider_hp, vol_headphone, LV_ANIM_OFF);
        lv_obj_add_event_cb(slider_hp, vol_slider_cb, LV_EVENT_VALUE_CHANGED, &vol_headphone);

        // 阻止点击面板时触发遮罩的关闭事件
        lv_obj_add_flag(panel, LV_OBJ_FLAG_EVENT_BUBBLE);
    }
}

void gui_music_player_open(void)
{
    if (scr_player) return;

    scr_player = lv_obj_create(NULL);
    lv_obj_set_layout(scr_player, LV_LAYOUT_NONE);
    lv_obj_set_style_bg_color(scr_player, lv_color_white(), 0);

    // --- 1. 返回按钮 (左上角) ---
    lv_obj_t *btn_back = lv_btn_create(scr_player);
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

    // --- 2. 设置按钮 (右上角) ---
    lv_obj_t *btn_settings = lv_btn_create(scr_player);
    lv_obj_set_size(btn_settings, 50, 50);
    lv_obj_set_pos(btn_settings, 410, 20);  // 移到右上角
    lv_obj_set_style_bg_opa(btn_settings, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btn_settings, 0, 0);
    lv_obj_set_style_shadow_width(btn_settings, 0, 0);
    lv_obj_add_event_cb(btn_settings, settings_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *img_settings = lv_image_create(btn_settings);
    lv_image_set_src(img_settings, &setting_btn);
    lv_obj_center(img_settings);
    lv_obj_add_flag(img_settings, LV_OBJ_FLAG_EVENT_BUBBLE);

    // --- 3. 标题 ---
    lv_obj_t *label_title = lv_label_create(scr_player);
    lv_label_set_text(label_title, "Music Player");
    lv_obj_set_style_text_color(label_title, lv_color_black(), 0);
    lv_obj_align(label_title, LV_ALIGN_TOP_MID, 0, 40);

    // --- 4. 封面 ---
    cover = lv_image_create(scr_player);
    lv_image_set_src(cover, &music_player);
    lv_obj_align(cover, LV_ALIGN_TOP_MID, 0, 150);

    lv_anim_init(&cover_anim);
    lv_anim_set_var(&cover_anim, cover);
    lv_anim_set_exec_cb(&cover_anim, cover_anim_cb);
    lv_anim_set_values(&cover_anim, 0, 3600);
    lv_anim_set_time(&cover_anim, 10000);
    lv_anim_set_repeat_count(&cover_anim, LV_ANIM_REPEAT_INFINITE);

    // --- 5. 进度条 ---
    lv_obj_t *slider = lv_slider_create(scr_player);
    lv_obj_set_size(slider, 400, 10);
    lv_obj_set_pos(slider, (480 - 400) / 2, 550);
    lv_obj_set_style_bg_color(slider, lv_palette_lighten(LV_PALETTE_GREY, 2), LV_PART_MAIN);
    lv_obj_set_style_bg_color(slider, lv_palette_main(LV_PALETTE_BLUE), LV_PART_INDICATOR);
    lv_obj_remove_style(slider, NULL, LV_PART_KNOB);

    lv_obj_t *label_curr_time = lv_label_create(scr_player);
    lv_label_set_text(label_curr_time, "00:00");
    lv_obj_set_pos(label_curr_time, 40, 570);
    lv_obj_set_style_text_color(label_curr_time, lv_color_black(), 0);

    lv_obj_t *label_total_time = lv_label_create(scr_player);
    lv_label_set_text(label_total_time, "03:45");
    lv_obj_set_pos(label_total_time, 480 - 40 - 50, 570);
    lv_obj_set_style_text_color(label_total_time, lv_color_black(), 0);

    // --- 6. 控制按钮 ---
    // 播放 (居中)
    btn_play = lv_btn_create(scr_player);
    lv_obj_set_size(btn_play, 60, 60);
    lv_obj_set_pos(btn_play, (480 - 60) / 2, 670);
    lv_obj_set_style_bg_opa(btn_play, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btn_play, 0, 0);
    lv_obj_set_style_shadow_width(btn_play, 0, 0);
    lv_obj_add_event_cb(btn_play, play_event_cb, LV_EVENT_CLICKED, NULL);

    img_play = lv_image_create(btn_play);
    lv_image_set_src(img_play, &play_music_btn);
    lv_obj_center(img_play);
    lv_obj_add_flag(img_play, LV_OBJ_FLAG_EVENT_BUBBLE);

    // 下一首 (右侧)
    lv_obj_t *btn_next = lv_btn_create(scr_player);
    lv_obj_set_size(btn_next, 40, 40);
    lv_obj_set_pos(btn_next, 350, 680);
    lv_obj_set_style_bg_opa(btn_next, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btn_next, 0, 0);
    lv_obj_set_style_shadow_width(btn_next, 0, 0);

    lv_obj_t *img_next = lv_image_create(btn_next);
    lv_image_set_src(img_next, &next_btn);
    lv_obj_center(img_next);
    lv_obj_add_flag(img_next, LV_OBJ_FLAG_EVENT_BUBBLE);

    // 上一首 (左侧)
    lv_obj_t *btn_prev = lv_btn_create(scr_player);
    lv_obj_set_size(btn_prev, 40, 40);
    lv_obj_set_pos(btn_prev, 90, 680);
    lv_obj_set_style_bg_opa(btn_prev, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btn_prev, 0, 0);
    lv_obj_set_style_shadow_width(btn_prev, 0, 0);

    lv_obj_t *img_prev = lv_image_create(btn_prev);
    lv_image_set_src(img_prev, &previous_btn);
    lv_obj_center(img_prev);
    lv_obj_add_flag(img_prev, LV_OBJ_FLAG_EVENT_BUBBLE);

    // --- 7. 音乐列表按钮 (右下角) ---
    lv_obj_t *btn_list = lv_btn_create(scr_player);
    lv_obj_set_size(btn_list, 50, 50);
    lv_obj_set_pos(btn_list, 410, 680);  // 移到底部，与播放控制行对齐
    lv_obj_set_style_bg_opa(btn_list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btn_list, 0, 0);
    lv_obj_set_style_shadow_width(btn_list, 0, 0);
    lv_obj_add_event_cb(btn_list, list_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *img_list = lv_image_create(btn_list);
    lv_image_set_src(img_list, &music_list);
    lv_obj_center(img_list);
    lv_obj_add_flag(img_list, LV_OBJ_FLAG_EVENT_BUBBLE);

    lv_scr_load_anim(scr_player, LV_SCR_LOAD_ANIM_NONE, 0, 0, false);
}
