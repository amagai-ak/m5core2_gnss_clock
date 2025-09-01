#include <iostream>
#include <string>
#include <sstream>
#include <time.h>
#include <math.h>
#include <M5Unified.h>

#include "scrn_main.h"
#include "screen_id.h"

LV_FONT_DECLARE(font_opensans_bold_48);

void BoxLabel::init(lv_obj_t *parent, int x, int y, int w, int h, const char *text)
{
    bkgrnd = lv_obj_create(parent);
    lv_obj_set_size(bkgrnd, w, h);
    lv_obj_align(bkgrnd, LV_ALIGN_OUT_TOP_LEFT, x, y);
    lv_obj_set_style_bg_color(bkgrnd, lv_color_make(0, 0, 0), 0);
    lv_obj_set_style_radius(bkgrnd, 0, 0);
    lv_obj_set_style_border_width(bkgrnd, 0, 0);
    lv_obj_set_scrollbar_mode(bkgrnd, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(bkgrnd, LV_OBJ_FLAG_SCROLLABLE);

    label = lv_label_create(bkgrnd);
    lv_label_set_text(label, text);
    lv_obj_align(label, LV_ALIGN_TOP_LEFT, -8, -11);    // 微調整
    lv_obj_set_size(label, w, h);
    lv_obj_set_style_pad_left(label, 0, 0);
    lv_obj_set_style_pad_top(label, 0, 0);
    lv_obj_set_style_pad_right(label, 0, 0);
    lv_obj_set_style_pad_bottom(label, 0, 0);
//    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(label, lv_color_make(255, 255, 255), 0);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_scrollbar_mode(label, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(label, LV_OBJ_FLAG_SCROLLABLE);

    label2 = lv_label_create(bkgrnd);
    lv_label_set_text(label2, "");
    lv_obj_align(label2, LV_ALIGN_TOP_LEFT, -14, -11); // 微調整
    lv_obj_set_size(label2, w, h);
    lv_obj_set_style_pad_left(label2, 0, 0);
    lv_obj_set_style_pad_top(label2, 0, 0);
    lv_obj_set_style_pad_right(label2, 0, 0);
    lv_obj_set_style_pad_bottom(label2, 0, 0);
    lv_obj_set_style_text_font(label2, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(label2, lv_color_make(255, 255, 255), 0);
    lv_obj_set_style_text_align(label2, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_scrollbar_mode(label2, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(label2, LV_OBJ_FLAG_SCROLLABLE);
}

void SatelliteDisplay::paint_canvas()
{
    lv_canvas_init_layer(canvas, &layer);
    lv_canvas_fill_bg(canvas, lv_color_make(0x00, 0x00, 0x40), LV_OPA_COVER); // キャンバスの背景を黒に設定

    lv_draw_arc_dsc_t arc_dsc;
    lv_draw_arc_dsc_init(&arc_dsc);
    arc_dsc.color = lv_color_make(0x80, 0x80, 0x80); // 線の色をグレーに設定
    arc_dsc.width = 1; // 線の太さを1ピクセルに設定
    arc_dsc.center.x = img_w / 2;
    arc_dsc.center.y = img_h / 2;
    arc_dsc.radius = r_0;
    arc_dsc.start_angle = 0;
    arc_dsc.end_angle = 360;
    arc_dsc.width = 1;
    // 中心(100, 100)、半径30の円を描画
    lv_draw_arc(&layer, &arc_dsc);
    arc_dsc.radius = r_45;
    lv_draw_arc(&layer, &arc_dsc);

    // 中心を通る水平線と垂直線を描画
    lv_draw_line_dsc_t line_dsc;
    lv_draw_line_dsc_init(&line_dsc);
    line_dsc.color = lv_color_make(0x80, 0x80, 0x80); // 線の色をグレーに設定
    line_dsc.width = 1; // 線の太さを1ピクセルに設定
    line_dsc.round_start = 1;
    line_dsc.round_end = 1;
    line_dsc.p1.x = 0;
    line_dsc.p1.y = img_h / 2;
    line_dsc.p2.x = img_w;
    line_dsc.p2.y = img_h / 2;
    lv_draw_line(&layer, &line_dsc);

    line_dsc.p1.x = img_w / 2;
    line_dsc.p1.y = 0;
    line_dsc.p2.x = img_w / 2;
    line_dsc.p2.y = img_h;
    lv_draw_line(&layer, &line_dsc);

    // 衛星の位置を描画
    for( int i = 0; i < MAX_SATELLITES; i++ )
    {
        if( sat_positions[i][0] > 0 ) // PRNが設定されている衛星のみ描画
        {
            int x = img_w / 2 + sat_positions[i][1];
            int y = img_h / 2 - sat_positions[i][2]; // Y座標は上方向が小さいので反転
            // SNRに応じて色を変える
            if( sat_positions[i][3] < 20 )
            {
                arc_dsc.color = lv_color_make(0xff, 0x00, 0x00); // 赤色
            }
            else if( sat_positions[i][3] < 30 )
            {
                arc_dsc.color = lv_color_make(0xff, 0xa5, 0x00); // オレンジ色
            }
            else
            {
                arc_dsc.color = lv_color_make(0x00, 0xff, 0x00); // 緑色
            }
            arc_dsc.radius = 4;
            arc_dsc.center.x = x;
            arc_dsc.center.y = y;
            lv_draw_arc(&layer, &arc_dsc);
//            Serial.printf("Sat PRN=%d, x=%d, y=%d, SNR=%d\n", sat_positions[i][0], x, y, sat_positions[i][3]);
        }
    }
    lv_canvas_finish_layer(canvas, &layer);
}


void SatelliteDisplay::init(lv_obj_t *parent, int x, int y)
{
    canvas = lv_canvas_create(parent);
    lv_canvas_set_buffer(canvas, cbuf, img_w, img_h, LV_COLOR_FORMAT_NATIVE);
    lv_canvas_fill_bg(canvas, lv_color_hex3(0xccc), LV_OPA_COVER);
    lv_obj_align(canvas, LV_ALIGN_TOP_LEFT, x, y);
    paint_canvas(); // キャンバスの初期描画
}

SatelliteDisplay::SatelliteDisplay()
{
    int i;
    for( i = 0; i < MAX_SATELLITES; i++ )
    {
        sat_positions[i][0] = 0; // PRN
        sat_positions[i][1] = 0; // x座標
        sat_positions[i][2] = 0; // y座標
    }
    r_0 = img_h / 2; // 半径0の位置
    r_45 = r_0 / 2; // 半径45度の位置

    cbuf = new uint8_t[LV_CANVAS_BUF_SIZE(img_w, img_h, 32, LV_DRAW_BUF_STRIDE_ALIGN)];
    if( cbuf == NULL )
    {
        // メモリ確保失敗
        return;
    }
}

SatelliteDisplay::~SatelliteDisplay()
{
    if( cbuf != NULL )
    {
        delete[] cbuf;
        cbuf = NULL;
    }
}

int SatelliteDisplay::set_sat_pos(int prn, int elv, int azm, int snr)
{
    int i;
    int x, y, r;
    double theta;

    if( prn <= 0 )
        return -1; // PRNが無効
    if( elv < 0 || elv > 90 )
        return -1; // Elevationが無効

    // ElevationとAzimuthからx, y座標を計算
    // Azimuthは0度が北で時計回り．90度が東、180度が南、270度が西．
    // Elevationは0度が地平線、90度が真上とする
    r = r_0 * (1.0 - (elv / 90.0)); // 半径はr_0の範囲で計算
    theta = (90.0 - azm) * M_PI / 180.0;
    x = (int)(r * cos(theta));
    y = (int)(r * sin(theta));

    // そのPRNの衛星が既に存在するか確認
    for( i = 0; i < MAX_SATELLITES; i++ )
    {
        if( sat_positions[i][0] == prn )
        {
            sat_positions[i][1] = x;
            sat_positions[i][2] = y;
            sat_positions[i][3] = snr; // SNRを保存
            return 0; // 成功
        }
    }
    // 存在しない場合は空いている位置を探す
    for( i = 0; i < MAX_SATELLITES; i++ )
    {
        if( sat_positions[i][0] == 0 ) // PRNが0の位置を探す
        {
            sat_positions[i][0] = prn;
            sat_positions[i][1] = x;
            sat_positions[i][2] = y;
            sat_positions[i][3] = snr; // SNRを保存
            return 0; // 成功
        }
    }
    return -1; // エラー
}

int SatelliteDisplay::remove_sat(int prn)
{
    int i;
    // PRNが一致する衛星を探して削除
    for( i = 0; i < MAX_SATELLITES; i++ )
    {
        if( sat_positions[i][0] == prn )
        {
            sat_positions[i][0] = 0; // PRNを0にして削除
            sat_positions[i][1] = 0; // x座標をリセット
            sat_positions[i][2] = 0; // y座標をリセット
            return 0; // 成功
        }
    }
    return -1; // エラー（見つからなかった）
}


void ScreenMain::callback(lv_event_t *e)
{
    lv_obj_t *obj = (lv_obj_t *)lv_event_get_target(e);
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED)
    {
        ScreenMain *scrn = static_cast<ScreenMain *>(lv_event_get_user_data(e));
        scrn->on_button(obj);
    }
    else if (code == LV_EVENT_GESTURE)
    {
        ScreenMain *scrn = static_cast<ScreenMain *>(lv_event_get_user_data(e));
        lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
        scrn->on_swipe(dir);
    }
}


void ScreenMain::setup()
{
    int lbl_h, lbl_y;
    // Setup code for the main screen
    ScreenBase::setup();

    last_update = 0;
    sync_state = 0;
    sync_state_prev = 0;
    lv_obj_set_style_bg_color(lv_screen, lv_color_make(0, 0, 0), 0);

    // 時刻を表示するラベルを作成
    label_clock = lv_label_create(lv_screen);
    lv_label_set_text(label_clock, "12:34:56");
    lv_obj_align(label_clock, LV_ALIGN_OUT_TOP_LEFT, 0, 24);
    lv_obj_set_style_text_font(label_clock, &font_opensans_bold_48, 0);
    lv_obj_set_style_text_color(label_clock, lv_color_make(128, 0, 0), 0);
    lv_obj_set_style_text_align(label_clock, LV_TEXT_ALIGN_CENTER, 0);

    // 日付を表示するラベルを作成
    label_date = lv_label_create(lv_screen);
    lv_label_set_text(label_date, "2021/01/01");
    lv_obj_align(label_date, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_text_font(label_date, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(label_date, lv_color_make(182, 182, 182), 0);
    lv_obj_set_style_text_align(label_date, LV_TEXT_ALIGN_CENTER, 0);

    // LED
    led = lv_led_create(lv_screen);
    lv_obj_set_size(led, 24, 24);
    lv_obj_align(led, LV_ALIGN_BOTTOM_RIGHT, -12, -12);
    lv_led_set_color(led, lv_color_make(0, 255, 0));
    lv_led_off(led);

    // 衛星の配置を描画するキャンバス
    sat_display.init(lv_screen, 220, 0);
    sat_display.paint_canvas();

    // モード表示用のボックスラベル
    lbl_h = 28;
    lbl_y = 72;
    boxl_mode.init(lv_screen, 0, lbl_y, 200, lbl_h, "Mode:");
    boxl_mode.set_bg_color(lv_color_make(0, 0, 64));
    boxl_mode.set_text_color(lv_color_make(255, 255, 255));
    boxl_mode.set_font(&lv_font_montserrat_24);
//    boxl_mode.set_align(LV_TEXT_ALIGN_RIGHT);

    // 緯度経度表示用のボックスラベル
    lbl_y += lbl_h +1;
    boxl_lat.init(lv_screen, 0, lbl_y, 200, lbl_h, "Lat:");
    boxl_lat.set_bg_color(lv_color_make(0, 64, 0));
    boxl_lat.set_text_color(lv_color_make(255, 255, 255));
    boxl_lat.set_font(&lv_font_montserrat_24);
    boxl_lat.set_align(LV_TEXT_ALIGN_LEFT);

    lbl_y += lbl_h +1;
    boxl_lon.init(lv_screen, 0, lbl_y, 200, lbl_h, "Lon:");
    boxl_lon.set_bg_color(lv_color_make(0, 64, 0));
    boxl_lon.set_text_color(lv_color_make(255, 255, 255));
    boxl_lon.set_font(&lv_font_montserrat_24);
    boxl_lon.set_align(LV_TEXT_ALIGN_LEFT);

    // 温度と気圧表示用のボックスラベル
    lbl_y += lbl_h +1;
    boxl_temp.init(lv_screen, 0, lbl_y, 200, lbl_h, "Temp:");
    boxl_temp.set_bg_color(lv_color_make(64, 64, 0));
    boxl_temp.set_text_color(lv_color_make(255, 255, 255));
    boxl_temp.set_font(&lv_font_montserrat_24);
    boxl_temp.set_align(LV_TEXT_ALIGN_LEFT);

    lbl_y += lbl_h +1;
    boxl_pres.init(lv_screen, 0, lbl_y, 200, lbl_h, "hPa:");
    boxl_pres.set_bg_color(lv_color_make(64, 64, 0));
    boxl_pres.set_text_color(lv_color_make(255, 255, 255));
    boxl_pres.set_font(&lv_font_montserrat_24);
    boxl_pres.set_align(LV_TEXT_ALIGN_LEFT);

    // スワイプジェスチャーの有効化
    lv_obj_add_event_cb(lv_screen, callback, LV_EVENT_GESTURE, this);
}


void ScreenMain::led_trigger()
{
    led_duration = 100; // 100ms
    lv_led_on(led);
}


void ScreenMain::loop()
{
    struct tm tm;
    char buf[32];
    static int last_sec = -1;
    struct timeval tv;

    // 時計の更新
    gettimeofday(&tv, NULL);
    if( tv.tv_sec != last_sec )
    {
        tm = *localtime(&tv.tv_sec);
        snprintf(buf, sizeof(buf), "%02d:%02d:%02d", tm.tm_hour, tm.tm_min, tm.tm_sec);
        lv_label_set_text(label_clock, buf);
        snprintf(buf, sizeof(buf), "%04d/%02d/%02d", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
        lv_label_set_text(label_date, buf);
        last_sec = tv.tv_sec;
    }
    if( sys_status.update_count != last_update )
    {
        last_update = sys_status.update_count;
        // 衛星データの更新
        update_satellite_all();
        sat_display.paint_canvas();

        // 測位モード
        if( sys_status.rmc_data.data_valid )
        {
            switch(sys_status.rmc_data.fix_type)
            {
                case NMEA_FIX_TYPE_NOFIX:
                    snprintf(buf, sizeof(buf), "No Fix");
                    break;
                case NMEA_FIX_TYPE_AUTONOMOUS:
                    snprintf(buf, sizeof(buf), "SPS");
                    break;
                case NMEA_FIX_TYPE_DIFFERENTIAL:
                    snprintf(buf, sizeof(buf), "DIFF");
                    break;
                default:
                    snprintf(buf, sizeof(buf), "Unknown");
                    break;
            }
        }
        else
            snprintf(buf, sizeof(buf), "-");
        boxl_mode.set_text2(buf);

        // 測位できている場合は緯度経度を表示
        if( sys_status.rmc_data.data_valid && sys_status.rmc_data.fix_type > NMEA_FIX_TYPE_NOFIX )
        {
            // 緯度
            snprintf(buf, sizeof(buf), "%.6f", sys_status.rmc_data.latitude);
            boxl_lat.set_text2(buf);
            // 経度
            snprintf(buf, sizeof(buf), "%.6f", sys_status.rmc_data.longitude);
            boxl_lon.set_text2(buf);
        }
        else
        {
            boxl_lat.set_text2("-");
            boxl_lon.set_text2("-");
        }

        // 温度と気圧
        snprintf(buf, sizeof(buf), "%.1f", sys_status.temp);
        boxl_temp.set_text2(buf);
        snprintf(buf, sizeof(buf), "%.1f", sys_status.pressure);
        boxl_pres.set_text2(buf);
    }

    // LEDの更新
    if( led_duration > 0 ) {
        led_duration -= 10;
        if( led_duration <= 0 ) {
            led_duration = 0;
            lv_led_off(led);
        }
    }
}


void ScreenMain::on_button(lv_obj_t *btn)
{
}


void ScreenMain::on_swipe(lv_dir_t dir)
{
    if (dir == LV_DIR_LEFT)
    {
        // 左スワイプでシャットダウン画面へ
        change_screen(SCREEN_ID_SHUTDOWN, SCREEN_ANIM_LEFT);
    }
    else if (dir == LV_DIR_RIGHT)
    {
        // 右スワイプでシャットダウン画面へ
        change_screen(SCREEN_ID_SHUTDOWN, SCREEN_ANIM_RIGHT);
    }
}


void ScreenMain::update_satellite(nmea_gsv_data_t *gsv_data)
{
    nmea_gsv_data_t *current_gsv = gsv_data;
    int sat_count;
    int i;
    while( current_gsv != NULL )
    {
        // 衛星の位置を更新
        sat_count = current_gsv->num_sats;
        for(i = 0; i < sat_count; i++)
        {
            // 衛星のPRN, Elevation, Azimuth, SNRを取得
            int prn = current_gsv->satellites[i].prn;
            int elv = current_gsv->satellites[i].elevation;
            int azm = current_gsv->satellites[i].azimuth;
            int snr = current_gsv->satellites[i].snr;

            // 衛星の位置を設定
            sat_display.set_sat_pos(prn, elv, azm, snr);
        }
        current_gsv = current_gsv->next;
    }
}

void ScreenMain::update_satellite_all()
{
    nmea_clear_old_gsv_data_all(&sys_status.gsv_data, 3);
    update_satellite(&sys_status.gsv_data.gps);
    update_satellite(&sys_status.gsv_data.glonass);
    update_satellite(&sys_status.gsv_data.galileo);
    update_satellite(&sys_status.gsv_data.beidou);
    update_satellite(&sys_status.gsv_data.qzss);
}

void ScreenMain::set_sync_state(int state)
{
    sync_state = state;
    if(sync_state != sync_state_prev)
    {
        switch( sync_state )
        {
            case 0:
                lv_obj_set_style_text_color(label_clock, lv_color_make(128, 0, 0), 0);
                break;
            case 1:
                lv_obj_set_style_text_color(label_clock, lv_color_make(128, 128, 0), 0);
                break;
            case 2:
                lv_obj_set_style_text_color(label_clock, lv_color_make(0, 255, 0), 0);
                break;
            default:
                lv_obj_set_style_text_color(label_clock, lv_color_make(128, 128, 128), 0);
                break;
        }
        sync_state_prev = sync_state;
    }
}
