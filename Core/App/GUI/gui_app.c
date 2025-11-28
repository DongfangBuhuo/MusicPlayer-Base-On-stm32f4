#include "gui_app.h"

#include "../../Gui/lvgl/lvgl.h"

void gui_app_init(void) {
  /* LVGL Test Code */
  lv_obj_t *btn =
      lv_button_create(lv_screen_active()); /*Add a button the current screen*/
  lv_obj_set_pos(btn, 10, 10);              /*Set its position*/
  lv_obj_set_size(btn, 120, 50);            /*Set its size*/

  lv_obj_t *label = lv_label_create(btn); /*Add a label to the button*/
  lv_label_set_text(label, "Hello LVGL"); /*Set the labels text*/
  lv_obj_center(label);                   /*Center the label on the button*/
}
