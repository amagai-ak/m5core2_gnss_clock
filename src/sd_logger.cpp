/**
 * @file sd_logger.cpp
 * @author amagai
 * @brief SDカードへのロギング機能を提供するクラス
 * @version 0.1
 * @date 2025-09-06
 * 
 * sd_init()を呼び出して，ハードウエアを最初に初期化する．
 * set_prefix()でファイル名のプリフィクスを設定し，
 * start()でロギングを開始する．ファイル名は，start()呼び出し時の
 * 時刻を元に生成される．
 * ファイルは常にcloseされた状態で，バッファからデータを書き込む時にのみ
 * 一時的にopenされ，appendされ，closeされる．
 * NMEAのデータであれば，3秒に1回程度の頻度で書き込まれる．
 */

#include "sd_logger.h"

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>


// SDカードのSPIピン設定
// M5satack Core2用
// 以下を参照
// https://docs.m5stack.com/en/arduino/m5core2/microsd
#define SD_SPI_CS_PIN   4
#define SD_SPI_SCK_PIN  18
#define SD_SPI_MISO_PIN 38
#define SD_SPI_MOSI_PIN 23

static bool sd_initialized = false;
static volatile bool sd_fault= false;


/**
 * @brief SDカードの初期化を行う
 * 
 * @return int 成功すれば0，失敗すれば-1
 * 
 * M5.begin()の後で呼び出すこと
 */
int sd_init()
{
    if( sd_initialized ) 
    {
        return 0; // すでに初期化されている場合は何もしない
    }

    SPI.begin(SD_SPI_SCK_PIN, SD_SPI_MISO_PIN, SD_SPI_MOSI_PIN, SD_SPI_CS_PIN);
    if (!SD.begin(SD_SPI_CS_PIN, SPI, 25000000)) 
    {
        sd_fault = true;
        return -1;
    }
    sd_initialized = true;
    sd_fault = false;
    return 0;
}


/**
 * @brief SDカードの状態を取得する
 * 
 * @return true SDカードに異常が発生している
 * @return false SDカードは正常
 */
bool sd_is_fault()
{
    return sd_fault;
}


SDLogger::SDLogger()
{
    log_buffer = new uint8_t[buffer_size];
    if( log_buffer == NULL ) 
    {
        sd_fault = true;
        ESP_LOGE("SDLogger", "Failed to allocate log buffer");
    }
    prefix[0] = '\0';
    filename[0] = '\0';
    buffer_pos = 0;
    sd_status = SD_STATUS_ERROR;
}


SDLogger::~SDLogger() 
{
    delete[] log_buffer;
}


/**
 * @brief 生成するファイル名のプリフィクスを設定する
 * 
 * @param pre プリフィクス文字列
 * @return int 0
 */
int SDLogger::set_prefix(const char* pre)
{
    strlcpy(prefix, pre, sizeof(prefix));
    return 0;
}


/**
 * @brief SDカードのロガーを開始する．
 * 
 * @return int 成功すれば0，失敗すれば-1
 * 
 * ファイル名は，プリフィクス_YYYYMMDD_HHMMSS.logの形式で生成される．
 */
int SDLogger::start() 
{
    time_t now = time(NULL);
    struct tm *t = localtime(&now);

    if( (!sd_initialized) || sd_fault ) 
    {
        // SDカードが初期化されていないか，障害が発生している場合は失敗
        return -1;
    }

    if( sd_status == SD_STATUS_READY ) 
    {
        // すでに開始されている場合は何もしない
        return 0;
    }
    // ファイル名を生成
    strlcpy(filename, prefix, sizeof(filename));
    int n = strlen(filename);
    strftime(filename + n, sizeof(filename) - n, "_%Y%m%d_%H%M%S.log", t);

    spi_mutex.lock();
    logFile = SD.open(filename, FILE_WRITE);
    if (!logFile) 
    {
        sd_status = SD_STATUS_ERROR;
        sd_fault = true;
        spi_mutex.unlock();
        return -1;
    }
    sd_status = SD_STATUS_READY;
    buffer_pos = 0;
    logFile.close();
    spi_mutex.unlock();
    return 0;
}


/**
 * @brief SDカードのロガーを再起動する
 * 
 * @return int 成功すれば0，失敗すれば-1
 * 
 * 既に開始されている場合は一旦閉じてから再度開始する．
 * 新しい日付でファイル名が生成される．
 */
int SDLogger::restart() 
{
    if( sd_status == SD_STATUS_READY ) 
    {
        close();
    }
    return start();
}


/**
 * @brief SDカードのロガーを閉じる
 * 
 * @return int 成功すれば0，失敗すれば-1
 */
int SDLogger::close() 
{
    if (sd_status != SD_STATUS_READY) 
    {
        return -1;
    }
    flush();
    sd_status = SD_STATUS_ERROR;
    return 0;
}


/**
 * @brief データを書き込む
 * 
 * @param data 書き込むデータ
 * @param length データの長さ
 * @return int 成功すれば0，失敗すれば-1
 */
int SDLogger::write_data(const uint8_t* data, size_t length) 
{
    if (sd_status != SD_STATUS_READY) 
    {
        return -1;
    }
    // バッファに収まる場合は，バッファに追加するだけで終了
    if (buffer_pos + length < buffer_size) 
    {
        memcpy(log_buffer + buffer_pos, data, length);
        buffer_pos += length;
        return 0;
    }
    // バッファに収まらない場合は，バッファをフラッシュしてから追加
    spi_mutex.lock();
    logFile = SD.open(filename, FILE_APPEND);
    if (!logFile) 
    {
        sd_status = SD_STATUS_ERROR;
        sd_fault = true;
        spi_mutex.unlock();
        ESP_LOGE("SDLogger", "Failed to open log file");
        return -1;
    }
    if (buffer_pos > 0) 
    {
        if( logFile.write(log_buffer, buffer_pos) != buffer_pos )
        {
            sd_fault = true;
            sd_status = SD_STATUS_ERROR;
            logFile.close();
            spi_mutex.unlock();
            ESP_LOGE("SDLogger", "Failed to write data");
            return -1;
        }
        buffer_pos = 0;
    }

    if (length > buffer_size) 
    {
        // データがバッファサイズを超える場合は直接書き込む
        if( logFile.write(data, length) != length )
        {
            sd_fault = true;
            sd_status = SD_STATUS_ERROR;
            logFile.close();
            spi_mutex.unlock();
            ESP_LOGE("SDLogger", "Failed to write data");
            return -1;
        }
        logFile.flush();
    } 
    else 
    {
        memcpy(log_buffer, data, length);
        buffer_pos = length;
    }
    logFile.close();
    spi_mutex.unlock();
    return 0;
}


/**
 * @brief 現在のバッファ内容をSDカードにフラッシュする
 * 
 * @return int 成功すれば0，失敗すれば-1
 */
int SDLogger::flush() 
{
    if (sd_status != SD_STATUS_READY) 
    {
        return -1;
    }
    spi_mutex.lock();
    logFile = SD.open(filename, FILE_APPEND);
    if (!logFile) 
    {
        sd_status = SD_STATUS_ERROR;
        sd_fault = true;
        spi_mutex.unlock();
        ESP_LOGE("SDLogger", "Failed to open log file");
        return -1;
    }
    if (buffer_pos > 0) 
    {
        if( logFile.write(log_buffer, buffer_pos) != buffer_pos )
        {
            sd_fault = true;
            sd_status = SD_STATUS_ERROR;
            logFile.close();
            spi_mutex.unlock();
            ESP_LOGE("SDLogger", "Failed to write data");
            return -1;
        }
        logFile.flush();
        buffer_pos = 0;
    }
    logFile.close();
    spi_mutex.unlock();
    return 0;
}
