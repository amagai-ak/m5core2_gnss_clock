/**
 * @file nmea_parser.c
 * @author amagai
 * @brief NMEA 0183の簡易パーサー
 * @version 0.1
 * @date 2025-06-28
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <sys/time.h>
#include "nmea_parser.h"

/**
 * @brief 現在時刻をミリ秒単位で取得する
 * @brief 現在時刻をミリ秒単位で取得する
 * @return uint64_t 現在時刻 (UNIXタイムスタンプ, ミリ秒単位)
 */
static uint64_t nmea_get_current_time_ms(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)(tv.tv_sec) * 1000 + (uint64_t)(tv.tv_usec) / 1000;
}


/**
 * @brief NMEAのCSVから1つのデータを抽出する
 * @param nmea_sentence NMEA文
 * @param index 抽出するデータのインデックス (0から始まる)
 * @param output 抽出したデータを格納するバッファ
 * @param output_size outputのサイズ
 * @return 0: 成功, -1: エラー
 */
static int nmea_extract_field(const char *nmea_sentence, int index, char *output, size_t output_size) 
{
    if (nmea_sentence == NULL || output == NULL || output_size == 0) 
    {
        return -1; // Error: NULL pointer or zero size
    }

    const char *start = nmea_sentence;
    const char *end = strchr(start, ',');
    int current_index = 0;

    while (end != NULL && current_index < index) 
    {
        start = end + 1; // Move to the next field
        end = strchr(start, ',');
        current_index++;
    }

    if (current_index != index || start == NULL) 
    {
        return -1; // Error: Index out of bounds
    }

    size_t length = (end != NULL) ? (size_t)(end - start) : strlen(start);
    if (length >= output_size) 
    {
        return -1; // Error: Output buffer too small
    }

    strncpy(output, start, length);
    output[length] = '\0'; // Null-terminate the string
    output[output_size - 1] = '\0'; // Ensure null termination in case of overflow
    // 最後のフィールドはチェックサムが含まれるので、チェックサムを除外する
    char *checksum_start = strchr(output, '*');
    if (checksum_start != NULL)
    {            
        *checksum_start = '\0'; // Null-terminate at the checksum
    }

    return 0; // Success
}


/**
 * @brief NMEA文のフィールド数をカウントする
 * 
 * @param nmea_sentence NMEA文
 * @return int フィールド数。エラー時は-1を返す。
 */
static int nmea_count_fields(const char *nmea_sentence)
{
    if (nmea_sentence == NULL) 
    {
        return -1; // Error: NULL pointer
    }

    int count = 0;
    const char *p = nmea_sentence;

    while (*p != '\0') 
    {
        if (*p == ',') 
        {
            count++;
        }
        p++;
    }

    return count + 1; // Add one for the last field
}


/**
 * @brief チェックサムの確認
 * 
 * @param nmea_sentence 
 * @return int チェックサムが有効な場合は1、無効な場合は0
 */
int nmea_is_valid_checksum(const char *nmea_sentence) 
{
    if (nmea_sentence == NULL || strlen(nmea_sentence) < 7) 
    {
        return 0; // Invalid sentence
    }

    const char *checksum_start = strchr(nmea_sentence, '*');
    if (checksum_start == NULL || checksum_start - nmea_sentence < 3) 
    {
        return 0; // No checksum found
    }

    int checksum_value = strtol(checksum_start + 1, NULL, 16);
    int calculated_checksum = 0;

    for (const char *p = nmea_sentence + 1; p < checksum_start; p++) 
    {
        calculated_checksum ^= *p;
    }

    return (calculated_checksum == checksum_value);
}


/**
 * @brief GSVデータの初期化
 * 
 * @param data 初期化するnmea_gsv_data_all_t構造体へのポインタ
 * @return int 成功時は0、失敗時は-1
 */
int nmea_init_gsv_data_all(nmea_gsv_data_all_t *data) 
{
    if (data == NULL) 
    {
        return -1; // Error: NULL pointer
    }

    memset(data, 0, sizeof(nmea_gsv_data_all_t));
    return 0; // Success
}


/**
 * @brief GSVデータの削除
 * 
 */
void nmea_free_gsv_data(nmea_gsv_data_t *data)
{
    if (data == NULL)
    {
        return; // Error: NULL pointer
    }
    if( data->next != NULL )
    {
        nmea_free_gsv_data(data->next); // 次のデータが存在する場合は解放
        free(data->next);
    }
    data->next = NULL; // 次のデータへのポインタをNULLに
}


/**
 * @brief 全GSVデータの削除
 * 
 */
void nmea_free_gsv_data_all(nmea_gsv_data_all_t *data)
{
    if (data == NULL)
    {
        return; // Error: NULL pointer
    }
    nmea_free_gsv_data(&data->gps);
    nmea_free_gsv_data(&data->glonass);
    nmea_free_gsv_data(&data->galileo);
    nmea_free_gsv_data(&data->beidou);
    nmea_free_gsv_data(&data->qzss);
}


/**
 * @brief GSVデータの更新
 * @param data 更新するnmea_gsv_data_all_t構造体へのポインタ
 * @param nmea_sentence NMEA GSV文
 * @return int 成功時は0、失敗時は-1
 */
int nmea_update_gsv_data_all(nmea_gsv_data_all_t *data, const char *nmea_sentence)
{
    nmea_gsv_data_t *gsv_data = NULL;
    int satnum;
    int num_fields;
    int signal_id;

    if (data == NULL || nmea_sentence == NULL)
    {
        return -1; // Error: NULL pointer
    }
    if (!nmea_is_valid_checksum(nmea_sentence)) 
    {
        return -2; // Error: Invalid checksum
    }
    if (nmea_sentence[1] != 'G' || nmea_sentence[3] != 'G' || nmea_sentence[4] != 'S' || nmea_sentence[5] != 'V' )
    {
        return -3; // Error: Not a GSV sentence
    }

    // フィールド数をカウント
    num_fields = nmea_count_fields(nmea_sentence);

    // フィールドの数が4の倍数+1の場合はシグナルIDが含まれる
    if (num_fields > 8 && (num_fields - 1) % 4 == 0) 
    {
        // 最後のフィールドがシグナルID
        char signal_id_field[10];
        if (nmea_extract_field(nmea_sentence, num_fields - 1, signal_id_field, sizeof(signal_id_field)) != 0) 
        {
            return -5; // Error: Failed to extract signal ID
        }
        // シグナルIDを整数に変換．シグナルIDは0-Fの値なので，これを整数に変換．
        if (signal_id_field[0] != '\0') 
        {
            signal_id = strtol(signal_id_field, NULL, 16); // 16進数として解釈
            if (signal_id < 0 || signal_id > 15) 
            {
                return -6; // Error: Invalid signal ID
            }
        } 
        else 
        {
            return -7; // Error: Empty signal ID field
        }
    }
    else 
    {
        signal_id = 0; // シグナルIDが無い場合は0に設定
    }

    // 衛星の種類を判別
    switch( nmea_sentence[2] ) 
    {
        case 'P': // GPS
            gsv_data = &data->gps;
            break;
        case 'L': // GLONASS
            gsv_data = &data->glonass;
            break;
        case 'A': // Galileo
            gsv_data = &data->galileo;
            break;
        case 'B': // BeiDou
            gsv_data = &data->beidou;
            break;
        case 'Q': // QZSS
            gsv_data = &data->qzss;
            break;
        default:
            return -4; // Error: Unknown satellite system
    }

    // シグナルIDが一致する構造体を探す
    nmea_gsv_data_t *current = gsv_data;
    while (current != NULL)
    {
        if (current->signal_id == signal_id) 
        {
            gsv_data = current; // 既存のデータを使用
            break;
        }
        if (current->next == NULL)
        {
            // 新しいデータを追加
            nmea_gsv_data_t *new_data = (nmea_gsv_data_t *)malloc(sizeof(nmea_gsv_data_t));
            if (new_data == NULL)
            {
                return -8; // Error: Memory allocation failed
            }
            memset(new_data, 0, sizeof(nmea_gsv_data_t));
            new_data->signal_id = signal_id;
            new_data->next = NULL;
            current->next = new_data; // 既存のリストに追加
            gsv_data = new_data; // 新しいデータを使用
            break;
        }
        current = current->next; // 次のデータへ
    }

    // センテンス番号を抽出
    char field[10];
    int sentence_number;
    int sentence_total;
    if (nmea_extract_field(nmea_sentence, 1, field, sizeof(field)) != 0) 
    {
        return -5; // Error: Failed to extract sentence total
    }
    sentence_total = atoi(field);
    if (nmea_extract_field(nmea_sentence, 2, field, sizeof(field)) != 0) 
    {
        return -5; // Error: Failed to extract sentence number
    }
    sentence_number = atoi(field);
    // センテンス番号が1の場合、データを初期化
    if (sentence_number == 1) 
    {
        gsv_data->num_sats_tmp = 0; // Reset the number of satellites
    }

    // 各衛星のデータを抽出．最大4つの衛星の情報が含まれる．
    for (satnum = 0; satnum < 4; satnum++)
    {
        int prn, elevation, azimuth, snr;
        char prn_field[10], elevation_field[10], azimuth_field[10], snr_field[10];

        if( gsv_data->num_sats_tmp >= NMEA_MAX_SATELLITES ) 
        {
            return -6; // Error: Maximum number of satellites exceeded
        }
        // 各衛星のPRN番号, 仰角，方位角, SNRを抽出
        if (nmea_extract_field(nmea_sentence, 4 + satnum * 4, prn_field, sizeof(prn_field)) != 0 ||
            nmea_extract_field(nmea_sentence, 5 + satnum * 4, elevation_field, sizeof(elevation_field)) != 0 ||
            nmea_extract_field(nmea_sentence, 6 + satnum * 4, azimuth_field, sizeof(azimuth_field)) != 0 ||
            nmea_extract_field(nmea_sentence, 7 + satnum * 4, snr_field, sizeof(snr_field)) != 0) 
        {
            break; // Error: Failed to extract fields
        }

        // 抽出したフィールドが空でないことを確認
        if (prn_field[0] == '\0' || elevation_field[0] == '\0' ||
            azimuth_field[0] == '\0' || snr_field[0] == '\0') 
        {
            continue; // 空のフィールドがあればスキップ
        }
        prn = atoi(prn_field);
        elevation = atoi(elevation_field);
        azimuth = atoi(azimuth_field);
        snr = atoi(snr_field);

        // 抽出したデータを構造体に格納
        gsv_data->satellites_tmp[gsv_data->num_sats_tmp].prn = prn;
        gsv_data->satellites_tmp[gsv_data->num_sats_tmp].elevation = elevation;
        gsv_data->satellites_tmp[gsv_data->num_sats_tmp].azimuth = azimuth;
        gsv_data->satellites_tmp[gsv_data->num_sats_tmp].snr = snr;
        gsv_data->num_sats_tmp++;

    }

    // センテンス番号が，総センテンス数に一致する場合は
    // データの受信が完了したとみなし，受信中データを確定させる．
    if (sentence_number == sentence_total) 
    {
        // 受信中データを確定させる処理
        gsv_data->num_sats = gsv_data->num_sats_tmp;
        memcpy(gsv_data->satellites, gsv_data->satellites_tmp, sizeof(gsv_data->satellites_tmp));
        gsv_data->last_update_ms = nmea_get_current_time_ms(); // 最後の更新時刻を記録
        gsv_data->num_sats_tmp = 0; // 受信中データをリセット
    }

    return 0; // Success
}


/**
 * @brief 古いGSVデータをクリアする
 * @param data GSVデータ構造体へのポインタ
 * @param age クリアする年齢 (秒単位)
 * @return int 成功時は0、失敗時は-1
 */
int nmea_clear_old_gsv_data(nmea_gsv_data_t *data, int age)
{
    if (data == NULL) 
    {
        return -1; // Error: NULL pointer
    }

    // 現在の時刻を取得
    uint64_t current_time = nmea_get_current_time_ms();
    uint64_t age_ms = (uint64_t)age * 1000; // 秒をミリ秒に変換

    if( data->last_update_ms < current_time - age_ms ) 
    {
        // 最後の更新時刻が指定された年齢より古い場合はデータをクリア
        data->num_sats = 0;
    }
    if (data->next != NULL) 
    {
        nmea_clear_old_gsv_data(data->next, age); // 次のGSVデータもクリア
    }
    return 0; // Success
}


int nmea_clear_old_gsv_data_all(nmea_gsv_data_all_t *data, int age)
{
    if (data == NULL) 
    {
        return -1; // Error: NULL pointer
    }

    nmea_clear_old_gsv_data(&data->gps, age);
    nmea_clear_old_gsv_data(&data->glonass, age);
    nmea_clear_old_gsv_data(&data->galileo, age);
    nmea_clear_old_gsv_data(&data->beidou, age);
    nmea_clear_old_gsv_data(&data->qzss, age);

    return 0; // Success
}




/**
 * @brief 衛星数を得る
 * 
 * @param data 
 * @return int 
 */
int nmea_get_gsv_satellites(nmea_gsv_data_t *data)
{
    int sat_num = 0;

    if (data == NULL) 
    {
        return -1; // Error: NULL pointer
    }

    nmea_gsv_data_t *current = data;

    // リストを走査して，最大の衛星数を探す
    while (current != NULL) 
    {
        if( current->num_sats > sat_num )
        {
            sat_num = current->num_sats; // 最大の衛星数を更新
        }
        current = current->next; // 次のデータへ
    }

    return sat_num;
}


/**
 * @brief 全ての衛星システムの総衛星数を取得
 * 
 * @param data 
 * @return int 
 */
int nmea_get_gsv_satellites_all(nmea_gsv_data_all_t *data)
{
    if (data == NULL) 
    {
        return -1; // Error: NULL pointer
    }

    int sat_num = 0;

    // 各衛星システムの衛星数を取得
    sat_num += nmea_get_gsv_satellites(&data->gps);
    sat_num += nmea_get_gsv_satellites(&data->glonass);
    sat_num += nmea_get_gsv_satellites(&data->galileo);
    sat_num += nmea_get_gsv_satellites(&data->beidou);
    sat_num += nmea_get_gsv_satellites(&data->qzss);

    return sat_num;
}


/**
 * @brief Initialize the RMC data structure
 * 
 * @param rmc_data 
 * @return int 
 */
int nmea_init_rmc(nmea_rmc_data_t *rmc_data)
{
    if (rmc_data == NULL) 
    {
        return -1; // Error: NULL pointer
    }

    memset(rmc_data, 0, sizeof(nmea_rmc_data_t));
    rmc_data->data_valid = 0; // 初期状態では無効
    rmc_data->fix_type = NMEA_FIX_TYPE_NOFIX; // 初期状態では測位なし
    rmc_data->latitude = 0.0; // 初期状態では緯度は0
    rmc_data->longitude = 0.0; // 初期状態では経度は0
    rmc_data->time_hour = 0; // 初期状態では時は0
    rmc_data->time_minute = 0; // 初期状態では分は0
    rmc_data->time_second = 0; // 初期状態では秒は0
    rmc_data->time_millisecond = 0; // 初期状態ではミリ秒は0
    return 0; // Success
}


/**
 * @brief RMCメッセージのパース
 * @param nmea_sentence NMEA RMC文
 * @param rmc_data パース結果を格納するnmea_rmc_data_t構造体へのポインタ
 * @return int 成功時は0、失敗時は負の値
 */
int nmea_parse_rmc(const char *nmea_sentence, nmea_rmc_data_t *rmc_data)
{
    char field[20];
    double latitude, longitude, speed, course;

    if (nmea_sentence == NULL || rmc_data == NULL)
    {
        return -1; // Error: NULL pointer
    }

    if (!nmea_is_valid_checksum(nmea_sentence)) 
    {
        return -2; // Error: Invalid checksum
    }

    if (nmea_sentence[3] != 'R' || nmea_sentence[4] != 'M' || nmea_sentence[5] != 'C') 
    {
        return -3; // Error: Not a RMC sentence
    }

    // データの有効性を確認
    if (nmea_extract_field(nmea_sentence, 2, field, sizeof(field)) != 0) 
    {
        return -4; // Error: Failed to extract data validity
    }
    if (field[0] == '\0') 
    {
        return -5; // Error: Empty data validity field
    }
    if( field[0] == 'A' )
    {
        rmc_data->data_valid = 1; // 'A' means valid
    }
    else
    {
        rmc_data->data_valid = 0; // 'V' means invalid
    }

    // UTC時刻を抽出 hhmmss.sss
    if (nmea_extract_field(nmea_sentence, 1, field, sizeof(field)) != 0) 
    {
        return -5; // Error: Failed to extract sentence number
    }
    if (field[0] == '\0') 
    {
        return -6; // Error: Empty UTC time field
    }
    sscanf(field, "%2d%2d%2d.%2d", &rmc_data->time_hour, &rmc_data->time_minute, &rmc_data->time_second, &rmc_data->time_millisecond);
    rmc_data->time_millisecond = rmc_data->time_millisecond * 10; // フィールドは2桁なので10倍してミリ秒に変換

    // 緯度の抽出
    if (nmea_extract_field(nmea_sentence, 3, field, sizeof(field)) != 0) 
    {
        return -7; // Error: Failed to extract latitude
    }
    if (field[0] == '\0') 
    {
        return -8; // Error: Empty latitude field
    }
    latitude = atof(field); // ddmm.mmmmmmm 形式で，mは分単位
    rmc_data->latitude = (int)(latitude / 100.0) + (latitude - (int)(latitude / 100.0) * 100.0) / 60.0;
    if (nmea_extract_field(nmea_sentence, 4, field, sizeof(field)) != 0) 
    {
        return -9; // Error: Failed to extract longitude
    }
    if (field[0] == '\0') 
    {
        return -10; // Error: Empty latitude direction field
    }
    // 緯度の方向を確認
    if (field[0] == 'S' || field[0] == 's') 
    {
        rmc_data->latitude = -rmc_data->latitude; // 南緯の場合
    }

    // 経度の抽出
    if (nmea_extract_field(nmea_sentence, 5, field, sizeof(field)) != 0) 
    {
        return -9; // Error: Failed to extract longitude
    }
    if (field[0] == '\0') 
    {
        return -10; // Error: Empty longitude field
    }
    longitude = atof(field); // dddmm.mmmmmmm 形式で，mは分単位
    rmc_data->longitude = (int)(longitude / 100.0) + (longitude - (int)(longitude / 100.0) * 100.0) / 60.0;
    if (nmea_extract_field(nmea_sentence, 6, field, sizeof(field)) != 0) 
    {
        return -11; // Error: Failed to extract latitude direction
    }
    if (field[0] == '\0') 
    {
        return -12; // Error: Empty longitude direction field
    }
    // 経度の方向を確認
    if (field[0] == 'W' || field[0] == 'w') 
    {
        rmc_data->longitude = -rmc_data->longitude; // 西経の場合
    }   

    // 日付の抽出 ddmmyy
    if (nmea_extract_field(nmea_sentence, 9, field, sizeof(field)) != 0) 
    {
        return -13; // Error: Failed to extract date
    }
    if (field[0] == '\0') 
    {
        return -14; // Error: Empty date field
    }
    sscanf(field, "%2d%2d%2d", &rmc_data->date_day, &rmc_data->date_month, &rmc_data->date_year);
    rmc_data->date_year += 2000; // 年は2000年以降と仮定

    // 測位モードの抽出
    if (nmea_extract_field(nmea_sentence, 12, field, sizeof(field)) != 0) 
    {
        return -15; // Error: Failed to extract fix type
    }
    if (field[0] == '\0') 
    {
        return -16; // Error: Empty fix type field
    }
    switch( field[0] ) 
    {
        case 'A': // Autonomous
            rmc_data->fix_type = NMEA_FIX_TYPE_AUTONOMOUS;
            break;
        case 'D': // Differential
            rmc_data->fix_type = NMEA_FIX_TYPE_DIFFERENTIAL;
            break;
        case 'E': // Estimated
            rmc_data->fix_type = NMEA_FIX_TYPE_ESTIMATED;
            break;
        case 'F': // Float RTK
            rmc_data->fix_type = NMEA_FIX_TYPE_RTK_FLOAT;
            break;
        case 'R': // Fixed RTK
            rmc_data->fix_type = NMEA_FIX_TYPE_RTK_FIXED;
            break;
        case 'P': // PPP
            rmc_data->fix_type = NMEA_FIX_TYPE_PPP;
            break;
        case 'S': // Simulator
            rmc_data->fix_type = NMEA_FIX_TYPE_SIM;
            break;
        default: // Invalid or manual mode
            rmc_data->fix_type = NMEA_FIX_TYPE_INVALID; // or NMEA_FIX_TYPE_MANUAL depending on your needs
            break;
    }

    rmc_data->last_update_ms = nmea_get_current_time_ms();

    return 0; // Success
}


/**
 * @brief GGAデータの初期化
 * 
 * @param gga_data 
 * @return int 
 */
int nmea_init_gga(nmea_gga_data_t *gga_data)
{
    if (gga_data == NULL) 
    {
        return -1; // Error: NULL pointer
    }

    memset(gga_data, 0, sizeof(nmea_gga_data_t));
    gga_data->fix_type = 0; // 初期状態では測位なし
    gga_data->latitude = 0.0; // 初期状態では緯度は0
    gga_data->longitude = 0.0; // 初期状態では経度は0
    gga_data->time_hour = 0; // 初期状態では時は0
    gga_data->time_minute = 0; // 初期状態では分は0
    gga_data->time_second = 0; // 初期状態では秒は0
    gga_data->time_millisecond = 0; // 初期状態ではミリ秒は0
    gga_data->num_sats = 0; // 初期状態では衛星数は0
    gga_data->last_update_ms = 0; // 初期状態では0
    gga_data->hdop = 0.0; // 初期状態ではHDOPは0
    gga_data->altitude = 0.0; // 初期状態では高度は0
    gga_data->geoidal_separation = 0.0; // 初期状態ではジオイド高は0
    gga_data->age_of_diff_corr = 0; // 初期状態では差分補正の年齢は0
    gga_data->diff_station_id = 0; // 初期状態では差分局IDは0
    return 0; // Success
}


/**
 * @brief GGAメッセージのパース
 * @param nmea_sentence NMEA GGA文
 * @param gga_data パース結果を格納するnmea_gga_data_t構造体へのポインタ
 * @return int 成功時は0、失敗時は負の値
 */
int nmea_parse_gga(const char *nmea_sentence, nmea_gga_data_t *gga_data)
{
    char field[20];
    double latitude, longitude;

    if (nmea_sentence == NULL || gga_data == NULL)
    {
        return -1; // Error: NULL pointer
    }

    if (!nmea_is_valid_checksum(nmea_sentence)) 
    {
        return -2; // Error: Invalid checksum
    }

    if (nmea_sentence[3] != 'G' || nmea_sentence[4] != 'G' || nmea_sentence[5] != 'A') 
    {
        return -3; // Error: Not a GGA sentence
    }

    // UTC時刻を抽出 hhmmss.ss
    if (nmea_extract_field(nmea_sentence, 1, field, sizeof(field)) != 0) 
    {
        return -4; // Error: Failed to extract UTC time
    }
    if (field[0] == '\0') 
    {
        return -5; // Error: Empty UTC time field
    }
    sscanf(field, "%2d%2d%2d.%2d", &gga_data->time_hour, &gga_data->time_minute, &gga_data->time_second, &gga_data->time_millisecond);
    gga_data->time_millisecond = gga_data->time_millisecond * 10; // フィールドは2桁なので10倍してミリ秒に変換

    // 緯度の抽出
    if (nmea_extract_field(nmea_sentence, 2, field, sizeof(field)) != 0) 
    {
        return -6; // Error: Failed to extract latitude
    }
    if (field[0] == '\0') 
    {
        return -7; // Error: Empty latitude field
    }
    latitude = atof(field); // ddmm.mmmmmmm 形式で，mは分単位
    gga_data->latitude = (int)(latitude / 100.0) + (latitude - (int)(latitude / 100.0) * 100.0) / 60.0;

    if (nmea_extract_field(nmea_sentence, 3, field, sizeof(field)) != 0) 
    {
        return -8; // Error: Failed to extract latitude direction
    }
    if (field[0] == '\0') 
    {
        return -9;
    // Error: Empty latitude direction field
    }
    // 緯度の方向を確認
    if (field[0] == 'S' || field[0] == 's') 
    {
        gga_data->latitude = -gga_data->latitude; // 南緯の場合
    }
    // 経度の抽出
    if (nmea_extract_field(nmea_sentence, 4, field, sizeof(field)) != 0) 
    {
        return -10; // Error: Failed to extract longitude
    }
    if (field[0] == '\0') 
    {
        return -11; // Error: Empty longitude field
    }
    longitude = atof(field); // dddmm.mmmmmmm 形式で，mは分単位
    gga_data->longitude = (int)(longitude / 100.0) + (longitude - (int)(longitude / 100.0) * 100.0) / 60.0;

    if (nmea_extract_field(nmea_sentence, 5, field, sizeof(field)) != 0) 
    {
        return -12; // Error: Failed to extract longitude direction
    }
    if (field[0] == '\0') 
    {
        return -13; // Error: Empty longitude direction field
    }
    // 経度の方向を確認
    if (field[0] == 'W' || field[0] == 'w') 
    {
        gga_data->longitude = -gga_data->longitude; // 西経の場合
    }

    // 測位モードの抽出
    if (nmea_extract_field(nmea_sentence, 6, field, sizeof(field)) != 0) 
    {
        return -14; // Error: Failed to extract fix type
    }
    if (field[0] == '\0') 
    {
        return -15; // Error: Empty fix type field
    }
    switch( field[0] )
    {
        case '0': // 無効な測位
            gga_data->fix_type = NMEA_FIX_TYPE_INVALID;
            break;
        case '1': // GPS測位
            gga_data->fix_type = NMEA_FIX_TYPE_AUTONOMOUS;
            break;
        case '2': // DGPS測位
            gga_data->fix_type = NMEA_FIX_TYPE_DIFFERENTIAL;
            break;
        case '3': // PPP測位
            gga_data->fix_type = NMEA_FIX_TYPE_PPP;
            break;
        case '4': // RTK測位 (Fixed)
            gga_data->fix_type = NMEA_FIX_TYPE_RTK_FIXED;
            break;
        case '5': // RTK測位 (Float)
            gga_data->fix_type = NMEA_FIX_TYPE_RTK_FLOAT;
            break;
        case '6': // Estimated (Dead Reckoning)
            gga_data->fix_type = NMEA_FIX_TYPE_ESTIMATED;
            break;
        default: // その他の測位
            gga_data->fix_type = NMEA_FIX_TYPE_INVALID; // 無効な測位
            break;
    } 
    // 衛星数の抽出
    if (nmea_extract_field(nmea_sentence, 7, field, sizeof(field)) != 0) 
    {
        return -16; // Error: Failed to extract number of satellites
    }
    if (field[0] == '\0') 
    {
        return -17; // Error: Empty number of satellites field
    }
    gga_data->num_sats = atoi(field);   // 衛星数を整数に変換
   
    // HDOPの抽出
    if (nmea_extract_field(nmea_sentence, 8, field, sizeof(field))  != 0) 
    {
        return -18; // Error: Failed to extract HDOP
    }
    if (field[0] == '\0') 
    {
        return -19; // Error: Empty HDOP field
    }
    gga_data->hdop = atof(field); // HDOPを浮動小数点数に変換

    // 高度の抽出
    if (nmea_extract_field(nmea_sentence, 9, field, sizeof(field)) != 0) 
    {        return -20; // Error: Failed to extract altitude
    }
    if (field[0] == '\0') 
    {
        return -21; // Error: Empty altitude field
    }
    gga_data->altitude = atof(field); // 高度を浮動小数点数に変換

    // 高度の単位の抽出
    if (nmea_extract_field(nmea_sentence, 10, field, sizeof(field)) != 0) 
    {        return -22; // Error: Failed to extract altitude unit
    }
    if (field[0] == '\0') 
    {
        return -23; // Error: Empty altitude unit field
    }
    if (field[0] == 'F' || field[0] == 'f') 
    {
        // フィート単位の場合はメートルに変換
        gga_data->altitude *= 0.3048; // フィートからメートルへの変換
    }

    // 高度のジオイド高の抽出
    if (nmea_extract_field(nmea_sentence, 11, field, sizeof(field)) != 0) 
    {
        return -24; // Error: Failed to extract geoid height
    }
    if (field[0] == '\0') 
    {
        return -25; // Error: Empty geoid height field
    }
    gga_data->geoidal_separation = atof(field); // ジオイド高を浮動小数点数に変換
    // ジオイド高の単位の抽出
    if (nmea_extract_field(nmea_sentence, 12, field, sizeof(field)) != 0) 
    {
        return -26; // Error: Failed to extract geoid height unit
    }
    if (field[0] == '\0') 
    {
        return -27; // Error: Empty geoid height unit field
    }
    if (field[0] == 'F' || field[0] == 'f') 
    {
        // フィート単位の場合はメートルに変換
        gga_data->geoidal_separation *= 0.3048; // フィートからメートルへの変換
    }
    // UTC時刻の更新
    gga_data->last_update_ms = nmea_get_current_time_ms();
    return 0; // Success
}
