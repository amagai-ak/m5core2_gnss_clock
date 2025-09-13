/**
 * @file main.cpp
 * @author amagai
 * @brief GNSSモジュールの動作テスト．Core2用．
 * @version 0.1
 * @date 2025-08-30
 * 
 * @copyright Copyright (c) 2025
 * 
 */

// GNSSモジュールのディップスイッチ
// PPS: GPIO35
// TX: GPIO14
// RX: GPIO13

#define GNSS_PPS_PIN 35
#define GNSS_RX_PIN 13
#define GNSS_TX_PIN 14

// 1にするとGNSSモジュールのシリアル通信をM5StackのSerialに接続する．
// PCからu-centerでGNSSモジュールにアクセスしたい場合には1にする．
#define GNSS_BYPASS 1

#include <Arduino.h>
#include <M5Unified.h>
#include <time.h>
#include <Adafruit_BMP280.h>

// #define LV_CONF_INCLUDE_SIMPLE
#include <lvgl.h>

#include "lvgl_setup.h"

#include "scrn_main.h"
#include "scrn_shutdown.h"
#include "screen_id.h"

#include "nmea_parser.h"
#include "system_status.h"

#include "sd_logger.h"
#include "i2cmutex.h"
#include "sensor_logger.h"

I2CMutex i2c_mutex;

static const char* time_zone  = "JST-9";
const int time_zone_offset = 9 * 3600; // JSTはUTC+9時間

system_status_t sys_status;

// 各スクリーンのインスタンスを生成
static ScreenMain scrn_main;
static ScreenShutdown scrn_shutdown;

// スクリーンマネージャのインスタンスを生成
static ScreenManager scrn_manager;

// 気圧センサ
#define BMP280_SENSOR_ADDR 0x76
Adafruit_BMP280 bmp280(&Wire);

// SDカードのNMEAロガー
SDLogger nmea_logger;

// IMUロガー
SensorLogger sensor_logger;

volatile uint32_t ppsTimestamp = 0;

void IRAM_ATTR onPPSInterrupt() 
{
    ppsTimestamp = micros();  // PPS信号受信時のタイムスタンプ（マイクロ秒）
}

/**
 * @brief RTC読み出し
 * @param tm 時刻情報格納先
 */
void rtc_read(struct tm *tm)
{
    m5::rtc_date_t DateStruct;
    m5::rtc_date_t DateStruct2;
    m5::rtc_time_t TimeStruct;

    do{
        M5.Rtc.getDate(&DateStruct);
        M5.Rtc.getTime(&TimeStruct);
        M5.Rtc.getDate(&DateStruct2);
    }while( DateStruct.date != DateStruct2.date);

    tm->tm_year = DateStruct.year - 1900;
    tm->tm_mon = DateStruct.month - 1;
    tm->tm_mday = DateStruct.date;
    tm->tm_hour = TimeStruct.hours;
    tm->tm_min = TimeStruct.minutes;
    tm->tm_sec = TimeStruct.seconds;
}

/**
 * @brief RTC読み出し．時刻が進むまで待つ．
 * @param tm 時刻情報格納先
 */
void rtc_read_step(struct tm *tm)
{
    struct tm told;

    rtc_read(&told);
    do
    {
        rtc_read(tm);
        if( told.tm_sec == tm->tm_sec )
        {
            delay(10);
        }
    }while(told.tm_sec == tm->tm_sec);
}


/**
 * @brief RTC書き込み
 * @param tm 時刻情報
 */
void rtc_write(struct tm *tm)
{
    m5::rtc_date_t DateStruct;
    m5::rtc_time_t TimeStruct;

    DateStruct.year = tm->tm_year + 1900;
    DateStruct.month = tm->tm_mon + 1;
    DateStruct.date = tm->tm_mday;
    TimeStruct.hours = tm->tm_hour;
    TimeStruct.minutes = tm->tm_min;
    TimeStruct.seconds = tm->tm_sec;

    M5.Rtc.setDate(&DateStruct);
    M5.Rtc.setTime(&TimeStruct);
}


/**
 * @brief RTCを読み込み，system timeにセットする
 */
void rtc_to_system_time()
{
    struct tm tm;
    struct timeval tv;

    rtc_read_step(&tm);
    time_t t = mktime(&tm);
    tv.tv_sec = t;
    tv.tv_usec = 0;
    settimeofday(&tv, NULL);
}


/**
 * @brief system time からRTCに書き込む
 */
void rtc_from_system_time()
{
    struct tm tm;
    struct timeval tv;

    gettimeofday(&tv, NULL);
    tm = *localtime(&tv.tv_sec);
    rtc_write(&tm);
}


 /**
 * @brief システムステータスの初期化
 * 
 * @param status 初期化するシステムステータス
 */
static void sys_status_init(system_status_t *status) 
{
    status->update_count = 0;
    nmea_init_gsv_data_all(&status->gsv_data);
    nmea_init_rmc(&status->rmc_data);
    nmea_init_gga(&status->gga_data);
    status->sync_state = SYNC_STATE_NONE;
    status->shutdown_request = 0;
}


/**
 * @brief RMCデータをシステム時刻に変換する
 * 
 * @param rmc RMCデータ
 */
void rmc_to_systime(nmea_rmc_data_t *rmc)
{
    struct tm t;
    time_t epoch;
    uint32_t ppsLatency = 0;
    uint32_t usec_now;
    struct timeval tvnow;
    int tdelta;

    if( !rmc->data_valid ) 
    {
        scrn_main.set_sync_state(0);
        return; // データが無効な場合は何もしない
    }

    // RMCデータからtm構造体を作成
    t.tm_year = rmc->date_year - 1900; // tm_yearは1900年からの年数
    t.tm_mon = rmc->date_month - 1;    // tm_monは0-11の範囲
    t.tm_mday = rmc->date_day;
    t.tm_hour = rmc->time_hour;
    t.tm_min = rmc->time_minute;
    t.tm_sec = rmc->time_second;
    t.tm_isdst = -1; // 夏時間情報は不明

    // tの値をローカルタイムとして解釈し、UNIXエポック秒に変換
    epoch = mktime(&t);
    if (epoch != -1) 
    {
        // タイムゾーンの補正を行う
        epoch += time_zone_offset;

        // PPS入力からの経過時間を計算
        gettimeofday(&tvnow, NULL);
        usec_now = micros();
        if (ppsTimestamp != 0) 
        {
            ppsLatency = usec_now - ppsTimestamp;
            if( ppsLatency >= 1000000 ) 
            {
                ppsLatency = 0;
            }
        }
        else 
        {
            ppsLatency = 0;
        }

        // システム時刻を設定
        struct timeval tv;
        tv.tv_sec = epoch;
        tv.tv_usec = ppsLatency; // マイクロ秒はPPS信号からの経過時間を設定
        if( abs(tv.tv_sec - tvnow.tv_sec) >= 2 ) 
        {
            // 2秒以上の差がある場合は差の計算でオーバーフローする可能性があるので強制的にジャンプさせる
            tdelta = 1000000;
        }
        else 
        {
            // 時間差をマイクロ秒単位で計算
            tdelta = (tv.tv_sec - tvnow.tv_sec)*1000000 + (tv.tv_usec - tvnow.tv_usec);
        }

        // tvnowとtvの差が500ms以上の場合は時刻をジャンプさせる
        if( abs(tdelta) >= 500000 )
        {
            settimeofday(&tv, NULL);
        }
        else
        {
            // 差が小さいときはadjtimeで補正する
            struct timeval tv_adj;
            tv_adj.tv_sec = 0;
            tv_adj.tv_usec = tdelta;
            adjtime(&tv_adj, NULL);
        }
        // 設定値を確認のため表示
        // Serial.printf("System time set to: %s", ctime(&tv.tv_sec));
        if( ppsLatency > 0 )
        {
            scrn_main.set_sync_state(2);        // PPS有効
            sys_status.sync_state = SYNC_STATE_PPS;
        }
        else
        {
            scrn_main.set_sync_state(1);        // PPS無効
            sys_status.sync_state = SYNC_STATE_GNSS;
        }
    }
}


/**
 * @brief GNSSデータの行を解析する
 * 
 * @param line 解析する行
 */
void gnss_parse_line(char *line)
{
    nmea_gga_data_t new_gga;

    if (strncmp(line, "$GNGGA", 6) == 0) 
    {
        // GGAメッセージの処理
        if( nmea_parse_gga(line, &new_gga) == 0 ) 
        {
            // GGAメッセージの解析に成功
            sys_status.gga_data = new_gga; // 新しいGGAデータを更新
            sys_status.gps_status = new_gga.fix_type; // GPSの状態を更新
            sys_status.gps_satellites = new_gga.num_sats; // 使用衛星数を更新
        }
    }
    else if( line[1] == 'G' && line[3] == 'G' && line[4] == 'S' && line[5] == 'V' )
    {
        // GSVメッセージの処理
        nmea_update_gsv_data_all(&sys_status.gsv_data, line);
    }
    else if( line[1] == 'G' && line[3] == 'R' && line[4] == 'M' && line[5] == 'C' )
    {
        // RMCメッセージの処理
//        Serial.printf("RMC: %s", line);
        nmea_parse_rmc(line, &sys_status.rmc_data);
        if( sys_status.rmc_data.data_valid && sys_status.rmc_data.fix_type > NMEA_FIX_TYPE_NOFIX ) 
        {
            // RMCデータが有効な場合、システム時刻を更新
            rmc_to_systime(&sys_status.rmc_data);
            rtc_from_system_time();
        }
        else 
        {
            scrn_main.set_sync_state(0); // 測位できていない場合は同期状態を0に
        }
        ppsTimestamp = 0;
        sys_status.update_count++; // 更新回数をインクリメント
    }
    // デバッグ用に受信した行を表示
    // Serial.printf("%s\r\n", line);
}


/**
 * @brief GNSSデータのポーリング
 * 
 */
void gnss_poll()
{
    static char linebuf[256];
    static int linepos = 0;
    static int linestate = 0;
    char c;

    while( Serial1.available() )
    {
        c = Serial1.read();
        nmea_logger.write_data((uint8_t *)&c, 1);
        #if GNSS_BYPASS
        Serial.write(c); // GNSS_BYPASSが1の場合は受信したデータをそのままSerialに流す
        #endif
        switch( linestate ) 
        {
            case 0: // 待機状態
                if( c == '$' ) 
                {
                    linebuf[0] = c;
                    linepos = 1;
                    linestate = 1; // 受信状態へ
                }
                break;
            case 1: // 受信状態
                if( c == '\n' ) 
                {
                    linebuf[linepos] = '\0';
                    linestate = 0; // 待機状態へ
                    // 行の終端に達したのでパースする
                    gnss_parse_line(linebuf);
                    linepos = 0;
                }
                else if( c == '\r' ) 
                {
                    // 無視
                }
                else {
                    if( linepos < sizeof(linebuf) - 1 ) 
                    {
                        linebuf[linepos++] = c;
                    }
                    else 
                    {
                        // バッファオーバーフロー防止のためリセット
                        linestate = 0;
                        linepos = 0;
                    }
                }
                break;
            default:
                linestate = 0; // 不正な状態になったら待機状態へ
                linepos = 0;
                break;
        }
    }
}


/**
 * @brief システム初期化
 * 
 */
void setup() 
{
    auto cfg = M5.config();
    M5.begin(cfg);

    Serial.begin(115200);
    Wire.begin(21, 22, 100000);
    setenv("TZ", time_zone, 1);
    tzset();

    // RTCを読んでシステム時刻を設定
    rtc_to_system_time();

    // センサの初期化
    unsigned status = bmp280.begin(BMP280_SENSOR_ADDR);

	Serial1.begin(38400, SERIAL_8N1, GNSS_RX_PIN, GNSS_TX_PIN); // RX, TX
    sys_status_init(&sys_status);

    ppsTimestamp = 0;
    pinMode(GNSS_PPS_PIN, INPUT);
    attachInterrupt(GNSS_PPS_PIN, onPPSInterrupt, RISING);  // PPS信号の立ち上がりで割り込み

    // LVGLの初期化
    lvgl_setup();

    // 各スクリーンのセットアップ
    scrn_main.setup();
    scrn_shutdown.setup();
    scrn_shutdown.set_shutdown_request_ptr(&sys_status.shutdown_request);

    // スクリーンマネージャにスクリーンを追加
    // 最初に追加したスクリーンが最初に表示されるスクリーンになる
    scrn_manager.add_screen(SCREEN_ID_MAIN, &scrn_main);
    scrn_manager.add_screen(SCREEN_ID_SHUTDOWN, &scrn_shutdown);

    if (sd_init() != 0) 
    {
        scrn_main.set_sdcard_status(0); // SDカード利用不可
    }
    else 
    {
        scrn_main.set_sdcard_status(1); // SDカード使用可能
        nmea_logger.set_prefix("/nmea");
    }
}


void loop() 
{
    static uint32_t prev_pps_timestamp = 0;
    static int prev_sync_state = SYNC_STATE_NONE;
    static int eachsec = 0;
    
    i2c_mutex.lock();
    M5.update();
    i2c_mutex.unlock();

    if (ppsTimestamp != 0 && ppsTimestamp != prev_pps_timestamp) 
    {
        scrn_main.led_trigger();
        prev_pps_timestamp = ppsTimestamp;
    }
    gnss_poll();

    // Serialから入ったデータをそのままSerial1に流す
    #if GNSS_BYPASS
    while( Serial.available() ) 
    {
        char c = Serial.read();
        Serial1.write(c);
    }
    #endif

    // LVGLのタスクハンドラを呼び出す
    lv_task_handler();
    scrn_manager.loop();

    // 毎秒1回の動作
    if( eachsec > 1000 ) 
    {
        eachsec = 0;
        // センサデータの更新
        sys_status.temp = bmp280.readTemperature();
        sys_status.pressure = bmp280.readPressure() / 100.0F;
        #if GNSS_BYPASS == 0
            Serial.printf("Temp: %.2f C, Pressure: %.2f hPa\r\n", sys_status.temp, sys_status.pressure);
        #endif
    }
    else 
    {
        eachsec += 10;
    }

    // 時計の同期が取れたらNMEAロガーを起動
    if( sd_is_fault() == false )
    {
        if( prev_sync_state != sys_status.sync_state && 
            prev_sync_state == SYNC_STATE_NONE )
        {
            // Serial.printf("Sync state changed: %d\r\n", sys_status.sync_state);
            prev_sync_state = sys_status.sync_state;
            nmea_logger.start();
            sensor_logger.start();
            scrn_main.set_sdcard_status(2); // SDカード記録中
        }
    }
    else 
    {
        scrn_main.set_sdcard_status(0); // SDカード利用不可
    }

    // シャットダウン要求があればシャットダウンする
    if( sys_status.shutdown_request == 1 ) 
    {
        nmea_logger.close();
        sensor_logger.stop();
        M5.Power.powerOff();
        while(1) 
        {
            delay(1000);
        }
    }

    // Bボタンが押されたらメイン画面へ，それ以外はシャットダウン画面へ
    if (M5.BtnB.wasPressed()) {
        scrn_manager.change_screen(SCREEN_ID_MAIN, SCREEN_ANIM_NONE);
    } else if (M5.BtnA.wasPressed()) {
        scrn_manager.change_screen(SCREEN_ID_SHUTDOWN, SCREEN_ANIM_NONE);
    } else if (M5.BtnC.wasPressed()) {
        scrn_manager.change_screen(SCREEN_ID_SHUTDOWN, SCREEN_ANIM_NONE);
    }

    delay(10);
}

