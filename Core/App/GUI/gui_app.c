#include "gui_app.h"
#include "gui_music_player.h"
#include "../../Gui/lvgl/lvgl.h"

// 定义 App ID
typedef enum
{
    APP_ID_MUSIC_PLAYER = 0,
    APP_ID_CAMERA,
    // APP_ID_SETTINGS, // 预留
    APP_MAX_COUNT
} app_id_e;

// 定义 App 结构体
typedef struct
{
    app_id_e id;
    const char *name;
    const void *icon_src;
} app_t;

// 声明函数
static void app_item_event_cb(lv_event_t *e);
void createHome(void);  // Not static, as it's called by gui_app_init

// App 列表
static const app_t app_list[] = {
    {APP_ID_MUSIC_PLAYER, "Music", &music_player},  // Assuming 'music_player' is an image descriptor
    {APP_ID_CAMERA,"Camera",&camera_icon},
};

lv_obj_t *scr_home;

// 4列
static int32_t col_dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};

// 6行
static int32_t row_dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1),        LV_GRID_FR(1),
                            LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};

void gui_app_init(void)
{
    createHome();
}

// 返回主页函数 (供子应用调用)
void gui_app_return_home(void)
{
    lv_scr_load_anim(scr_home, LV_SCR_LOAD_ANIM_NONE, 0, 0, true);
}

// 通用 App 事件回调
static void app_item_event_cb(lv_event_t *e)
{
    app_t *app = (app_t *)lv_event_get_user_data(e);
    if (!app) return;

    switch (app->id)
    {
        case APP_ID_MUSIC_PLAYER:
            gui_music_player_open();
            break;
        case APP_ID_CAMERA:
            gui_camera_open();
            break;
        default:
            break;
    }
}

// 创建主屏幕
void createHome(void)
{
    scr_home = lv_screen_active();
    // 使用白色背景
    lv_obj_set_style_bg_color(scr_home, lv_color_white(), 0);

    // 设置 Grid 布局
    lv_obj_set_layout(scr_home, LV_LAYOUT_GRID);
    lv_obj_set_grid_dsc_array(scr_home, col_dsc, row_dsc);

    // 遍历 App 列表创建图标
    for (int i = 0; i < sizeof(app_list) / sizeof(app_list[0]); i++)
    {
        const app_t *app = &app_list[i];

        // 创建容器作为 App 图标项
        lv_obj_t *item = lv_obj_create(scr_home);
        lv_obj_remove_style_all(item);  // 移除默认样式

        lv_obj_set_style_bg_opa(item, LV_OPA_TRANSP, 0);  // 背景全透明
        lv_obj_set_style_border_width(item, 0, 0);        // 去掉边框
        lv_obj_set_style_shadow_width(item, 0, 0);        // 去掉阴影
        // 设置大小
        lv_obj_set_size(item, LV_SIZE_CONTENT, LV_SIZE_CONTENT);

        // 设置 Flex 布局，垂直排列 (图标在上，文字在下)
        lv_obj_set_layout(item, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(item, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(item, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_gap(item, 5, 0);  // 图标和文字之间的间距

        // 将 Item 放入 Grid
        int col = i % 4;
        int row = i / 4;
        lv_obj_set_grid_cell(item, LV_GRID_ALIGN_CENTER, col, 1, LV_GRID_ALIGN_CENTER, row, 1);

        // 使其可点击
        lv_obj_add_flag(item, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(item, app_item_event_cb, LV_EVENT_CLICKED, (void *)app);

        // 创建图标图片
        if (app->icon_src)
        {
            lv_obj_t *img = lv_img_create(item);
            lv_img_set_src(img, app->icon_src);
            lv_obj_add_flag(img, LV_OBJ_FLAG_EVENT_BUBBLE);
        }

        // 创建 App 名称标签
        lv_obj_t *label = lv_label_create(item);
        lv_label_set_text(label, app->name);
        lv_obj_set_style_text_color(label, lv_color_black(), 0);
        lv_obj_add_flag(label, LV_OBJ_FLAG_EVENT_BUBBLE);
    }

    // 强制刷新显示
    lv_refr_now(NULL);
}
