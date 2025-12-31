/**
 * @file nmea_parser.h
 * @author amagai
 * @brief NMEA 0183の簡易パーサー
 * @version 0.1
 * @date 2025-06-28
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#ifndef NMEA_PARSER_H
#define NMEA_PARSER_H

#include <stdint.h>

// C++ compatibility
#ifdef __cplusplus
extern "C" {
#endif

#define NMEA_MAX_SATELLITES 64

#define NMEA_SAT_GPS 1
#define NMEA_SAT_GLONASS 2
#define NMEA_SAT_GALILEO 3
#define NMEA_SAT_BEIDOU 4
#define NMEA_SAT_QZSS 5


#define NMEA_FIX_TYPE_NOFIX 0
#define NMEA_FIX_TYPE_AUTONOMOUS 1
#define NMEA_FIX_TYPE_DIFFERENTIAL 2
#define NMEA_FIX_TYPE_ESTIMATED 3
#define NMEA_FIX_TYPE_RTK_FIXED 4
#define NMEA_FIX_TYPE_RTK_FLOAT 5
#define NMEA_FIX_TYPE_PPP 6
#define NMEA_FIX_TYPE_SIM 7
#define NMEA_FIX_TYPE_INVALID 8
#define NMEA_FIX_TYPE_MANUAL 9


/**
 * @brief GGAメッセージ
 * 
 */
typedef struct {
    uint64_t last_update_ms;      // タイムスタンプ (UNIXタイムスタンプ, ミリ秒単位)
    int time_hour;          // 時 (0-23)
    int time_minute;        // 分 (0-59)
    int time_second;        // 秒 (0-59)
    int time_millisecond;   // ミリ秒 (0-999)
    double latitude;          // 緯度 (度)
    double longitude;         // 経度 (度)
    int fix_type;            // 測位タイプ
    int num_sats;            // 使用衛星数
    double hdop;             // 水平精度 (HDOP)
    double altitude;         // 海抜高度 (メートル)
    double geoidal_separation; // ジオイドと楕円体の高さ差
    double age_of_diff_corr; // 差分補正の年齢 (秒)
    int diff_station_id;     // 差分ステーションID
} nmea_gga_data_t;



/**
 * @brief RMCメッセージ
 * 
 */
typedef struct {
    uint64_t last_update_ms;      // タイムスタンプ (UNIXタイムスタンプ, ミリ秒単位)
    int data_valid;              // データの有効性 (1: 有効, 0: 無効)
    int date_year;          // 年
    int date_month;         // 月
    int date_day;           // 日
    int time_hour;          // 時 (0-23)
    int time_minute;        // 分 (0-59)
    int time_second;        // 秒 (0-59)
    int time_millisecond;   // ミリ秒 (0-999)
    double latitude;          // 緯度 (度)
    double longitude;         // 経度 (度)
    int fix_type;            // 測位タイプ
} nmea_rmc_data_t;


/**
 * @brief 衛星情報
 * 
 */
typedef struct {
    int prn;          // PRN番号
    int elevation;    // 仰角 (度)
    int azimuth;      // 方位角 (度)
    int snr;          // 信号対雑音比 (dBHz)
} nmea_satellite_t;


/**
 * @brief GSVデータ構造体
 * シグナルIDごとに衛星情報を保持する
 */
typedef struct nmea_gsv_data {
    int signal_id;                  // シグナルID．衛星と周波数で決まる識別子．0-15
    uint64_t last_update_ms;          // 最後の更新時刻 (UNIXタイムスタンプ, ミリ秒単位)
    int num_sats;                   // 衛星の数
    nmea_satellite_t satellites[NMEA_MAX_SATELLITES]; // 衛星情報の配列
    struct nmea_gsv_data *next;     // 次のGSVデータへのポインタ

    // 受信中のデータを一時的に保持するための変数．最終センテンス受信で確定する．
    int num_sats_tmp;                   // 衛星の数
    nmea_satellite_t satellites_tmp[NMEA_MAX_SATELLITES]; // 衛星情報の配列
} nmea_gsv_data_t;


/**
 * @brief 全衛星のGSVデータ構造体
 * 
 */
typedef struct {
    nmea_gsv_data_t gps;        // GPS衛星情報
    nmea_gsv_data_t glonass;     // GLONASS衛星情報
    nmea_gsv_data_t galileo;     // Galileo衛星情報
    nmea_gsv_data_t beidou;      // BeiDou衛星情報
    nmea_gsv_data_t qzss;        // QZSS衛星情報
} nmea_gsv_data_all_t;


int nmea_is_valid_checksum(const char *nmea_sentence);
int nmea_init_gsv_data_all(nmea_gsv_data_all_t *data);
void nmea_free_gsv_data_all(nmea_gsv_data_all_t *data);
int nmea_update_gsv_data_all(nmea_gsv_data_all_t *data, const char *nmea_sentence);
int nmea_parse_rmc(const char *nmea_sentence, nmea_rmc_data_t *rmc_data);
int nmea_init_rmc(nmea_rmc_data_t *rmc_data);
int nmea_clear_old_gsv_data(nmea_gsv_data_t *data, int age);
int nmea_clear_old_gsv_data_all(nmea_gsv_data_all_t *data, int age);
int nmea_get_gsv_satellites(nmea_gsv_data_t *data);
int nmea_get_gsv_satellites_all(nmea_gsv_data_all_t *data);
int nmea_init_gga(nmea_gga_data_t *gga_data);
int nmea_parse_gga(const char *nmea_sentence, nmea_gga_data_t *gga_data);


#ifdef __cplusplus
}
#endif
// End of C++ compatibility

#endif // NMEA_PARSER_H
