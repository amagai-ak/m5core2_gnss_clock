
#ifndef SCRN_MAIN_H
#define SCRN_MAIN_H

#include "nmea_parser.h"
#include "screen_base.h"
#include "system_status.h"

/**
 * 背景色指定が出来るラベル
 */
class BoxLabel
{
protected:
    lv_obj_t *bkgrnd;
    lv_obj_t *label;
    lv_obj_t *label2;

public:
    BoxLabel()
    {
        bkgrnd = NULL; // 背景オブジェクト
        label = NULL; // ラベルオブジェクト
        label2 = NULL; // 2つ目のラベルオブジェクト
    }

    void init(lv_obj_t *parent, int x, int y, int w, int h, const char *text);

    void set_text(const char *text)
    {
        lv_label_set_text(label, text);
    }

    void set_text2(const char *text)
    {
        lv_label_set_text(label2, text);
    }

    void set_bg_color(lv_color_t color)
    {
        lv_obj_set_style_bg_color(bkgrnd, color, 0);
    }

    void set_text_color(lv_color_t color)
    {
        lv_obj_set_style_text_color(label, color, 0);
    }

    void set_font(const lv_font_t *font)
    {
        lv_obj_set_style_text_font(label, font, 0);
    }

    void set_align(lv_text_align_t align)
    {
//        lv_obj_align(label, align, 0, 0);
        lv_obj_set_style_text_align(label, align, 0);
    }

    void set_hidden(int hidden)
    {
        if( hidden )
            lv_obj_add_flag(bkgrnd, LV_OBJ_FLAG_HIDDEN);
        else
            lv_obj_clear_flag(bkgrnd, LV_OBJ_FLAG_HIDDEN);
    }
};


/**
 * @brief 衛星の配置を描画するクラス
 * 
 */
class SatelliteDisplay
{
protected:
    lv_obj_t *screen;
    lv_obj_t *satellite_circle;
    static const int MAX_SATELLITES = 32; // 最大衛星数
    int sat_positions[MAX_SATELLITES][4]; // 衛星の位置 (PRN, x, y, SNR) 座標
    static const int img_h = 100;
    static const int img_w = 100;
    lv_color_t draw_buf[img_w * img_h]; // 描画用バッファ
    lv_obj_t *canvas;
    int r_0, r_45;
    uint8_t *cbuf;
    lv_layer_t layer;

public:
    void paint_canvas();
    void init(lv_obj_t *parent, int x, int y);
    SatelliteDisplay();
    ~SatelliteDisplay();
    int set_sat_pos(int prn, int elv, int azm, int snr);
    int remove_sat(int prn);
};

class ScreenMain : public ScreenBase
{
protected:
    lv_obj_t *label_clock;
    lv_obj_t *label_date;
    lv_obj_t *led;
    int led_duration;
    unsigned int last_update;
    BoxLabel boxl_mode;
    BoxLabel boxl_lat;
    BoxLabel boxl_lon;
    BoxLabel boxl_temp;
    BoxLabel boxl_pres;
    int sync_state; // 0: 未同期, 1: 同期中, 2: 同期完了
    int sync_state_prev;

public:
    void setup();
    void loop();
    static void callback(lv_event_t *e);
    void on_button(lv_obj_t *btn);
    void on_swipe(lv_dir_t dir);

    void led_trigger();
    SatelliteDisplay sat_display;
    void update_satellite(nmea_gsv_data_t *gsv_data);
    void update_satellite_all();
    void set_sync_state(int state); // 0: 未同期, 1: 同期中, 2: 同期完了
};

#endif // SCRN_MAIN_H
