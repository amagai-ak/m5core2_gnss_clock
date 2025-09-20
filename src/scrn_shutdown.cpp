
#include <M5Unified.h>
#include "scrn_shutdown.h"

/**
 * @brief コールバック関数
 * 
 * @param e 
 */
void ScreenShutdown::callback(lv_event_t *e)
{
    lv_obj_t *obj = (lv_obj_t *)lv_event_get_target(e);
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_CLICKED)
    {
        ScreenShutdown *scrn = static_cast<ScreenShutdown *>(lv_event_get_user_data(e));
        scrn->on_button(obj);
    } 
    else if (code == LV_EVENT_LONG_PRESSED)
    {
        ScreenShutdown *scrn = static_cast<ScreenShutdown *>(lv_event_get_user_data(e));
        scrn->on_button_long_press(obj);
    }
    else if (code == LV_EVENT_GESTURE)
    {
        ScreenShutdown *scrn = static_cast<ScreenShutdown *>(lv_event_get_user_data(e));
        lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
        scrn->on_swipe(dir);
    }
}


void ScreenShutdown::on_button(lv_obj_t *btn)
{
}


/**
 * @brief ボタンが長押しされたときの処理
 * 
 * @param btn 
 */
void ScreenShutdown::on_button_long_press(lv_obj_t *btn)
{
    if (btn == btn_poweroff)
    {
        // Power off button long pressed
        if( shutdown_request != nullptr )
        {
            *shutdown_request = 1; // シャットダウン要求をセット
        }
    }
}


/**
 * @brief セットアップ
 * 
 */
void ScreenShutdown::setup()
{
    // Setup code for the shutdown screen
    ScreenBase::setup();

    shutdown_request = nullptr;

    // 長押し判定時間を設定
    lv_indev_t *indev = lv_indev_get_next(nullptr);
    while (indev != nullptr) 
    {
        if (lv_indev_get_type(indev) == LV_INDEV_TYPE_POINTER) 
        {
            lv_indev_set_long_press_time(indev, 2000);
            break;
        }
        indev = lv_indev_get_next(indev);
    }

    lv_obj_set_style_bg_color(lv_screen, lv_color_make(64, 0, 0), 0);

    label_goodbye = lv_label_create(lv_screen);
    lv_label_set_text(label_goodbye, "Long press the button to power off");
    lv_obj_set_style_text_color(label_goodbye, lv_color_make(255, 255, 255), 0);
    lv_obj_align(label_goodbye, LV_ALIGN_CENTER, 0, 0);

    btn_poweroff = lv_btn_create(lv_screen);
    lv_obj_set_size(btn_poweroff, 200, 50);
    // ボタンの色を赤に設定
    lv_obj_set_style_bg_color(btn_poweroff, lv_color_make(255, 0, 0), 0);
    lv_obj_align(btn_poweroff, LV_ALIGN_CENTER, 0, -40);
    lv_obj_add_event_cb(btn_poweroff, callback, LV_EVENT_LONG_PRESSED, this);
    lv_obj_t *label = lv_label_create(btn_poweroff);
    lv_label_set_text(label, LV_SYMBOL_POWER " Power Off");
    lv_obj_center(label);

    // スワイプジェスチャーの有効化
    lv_obj_add_event_cb(lv_screen, callback, LV_EVENT_GESTURE, this);
}


void ScreenShutdown::loop()
{
}


void ScreenShutdown::on_swipe(lv_dir_t dir)
{
    if (dir == LV_DIR_LEFT)
    {
        // 左スワイプでメイン画面へ
        change_screen(SCREEN_ID_MAIN, SCREEN_ANIM_LEFT);
    }
    else if (dir == LV_DIR_RIGHT)
    {
        // 右スワイプでメイン画面へ
        change_screen(SCREEN_ID_MAIN, SCREEN_ANIM_RIGHT);
    }
}
