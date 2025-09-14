/**
 * @file sensor_logger.cpp
 * @author amagai
 * @brief 
 * @version 0.1
 * @date 2025-09-13
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#include "sensor_logger.h"

static volatile bool terminate_sensor_logging = false;
static TaskHandle_t sensor_sampler_handle;
static TaskHandle_t sensor_logger_handle;

static volatile bool sensor_sampler_terminated = false;
static volatile bool sensor_logger_terminated = false;

class IMUFifo 
{
private:
    static const int FIFO_SIZE = 128; // FIFOのサイズ
    imu_record_t buffer[FIFO_SIZE];
    int head;
    int tail;
    int count;
    int overflow;
    SemaphoreHandle_t mutex;

public:
    IMUFifo() : head(0), tail(0), count(0), overflow(0) 
    {
        mutex = xSemaphoreCreateMutex();
    }
    ~IMUFifo() 
    {
        vSemaphoreDelete(mutex);
    }

    void lock() 
    {
        xSemaphoreTake(mutex, portMAX_DELAY);
    }

    void unlock() 
    {
        xSemaphoreGive(mutex);
    }

    bool push(const imu_record_t& record) 
    {
        lock();
        if (count < FIFO_SIZE) 
        {
            buffer[head] = record;
            head = (head + 1) % FIFO_SIZE;
            count++;
            unlock();
            return true;
        }
        overflow++;
        unlock();
        return false;
    }

    bool pop(imu_record_t& record) 
    {
        lock();
        if (count > 0) 
        {
            record = buffer[tail];
            tail = (tail + 1) % FIFO_SIZE;
            count--;
            unlock();
            return true;
        }
        unlock();
        return false;
    }

    int size() const 
    {
        return count;
    }

    void clear() 
    {
        lock();
        head = 0;
        tail = 0;
        count = 0;
        overflow = 0;
        unlock();
    }
};


static IMUFifo *imufifo = NULL;

/**
 * @brief センサーデータのサンプリングタスク
 * 
 * @param param 
 */
static void task_sensor_sampler(void *param)
{
    const int sample_period_ms = 100;
    portTickType xLastWakeTime;
    struct timeval tv;
    imu_record_t record;
    uint32_t samplecount = 0;


    xLastWakeTime = xTaskGetTickCount();

    while (terminate_sensor_logging == false) 
    {
        // 時刻の取得
        gettimeofday(&tv, NULL);

        // センサデータの更新
        i2c_mutex.lock();
        M5.Imu.getAccelData(&record.ax, &record.ay, &record.az);
        M5.Imu.getGyroData(&record.gx, &record.gy, &record.gz);
        i2c_mutex.unlock();

        record.timestamp = tv;
        record.count = samplecount++;
        if (!imufifo->push(record)) 
        {
            // FIFOがオーバーフローした場合の処理
            ESP_LOGW("IMUFifo", "FIFO overflow");
        }
        vTaskDelayUntil(&xLastWakeTime, sample_period_ms / portTICK_PERIOD_MS);
    }
    sensor_sampler_terminated = true;

    vTaskDelete(NULL);
}


/**
 * @brief センサーデータのロギングタスク
 * 
 * @param param 
 */
static void task_sensor_logger(void *param)
{
    imu_record_t record;
    char logline[128];
    int len;
    SDLogger *imu_logger;

    imu_logger = new SDLogger();
    if (imu_logger == NULL)
    {
        ESP_LOGE("SensorLogger", "Failed to create SDLogger");
        terminate_sensor_logging = true;
        sensor_logger_terminated = true;
        vTaskDelete(NULL);
        return;
    }

    imu_logger->set_prefix("/imu");
    imu_logger->start();

    while (terminate_sensor_logging == false) 
    {
        if (imufifo->pop(record)) 
        {
            // ログ行の生成
            len = snprintf(logline, sizeof(logline), "%ld.%06ld,%u,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f\n",
                           record.timestamp.tv_sec, record.timestamp.tv_usec,
                           record.count,
                           record.ax, record.ay, record.az,
                           record.gx, record.gy, record.gz);
            if (len > 0 && len < sizeof(logline)) 
            {
                if (imu_logger->write_data((const uint8_t *)logline, len) != 0) 
                {
                    ESP_LOGE("SensorLogger", "Failed to write data");
                    terminate_sensor_logging = true;
                }
            }
        } 
        else 
        {
            // FIFOが空の場合は少し待つ
            vTaskDelay(100 / portTICK_PERIOD_MS);
        }
    }
    imu_logger->close();
    delete imu_logger;

    sensor_logger_terminated = true;

    vTaskDelete(NULL);
}


SensorLogger::SensorLogger()
{
    sensor_sampler_handle = NULL;
    sensor_logger_handle = NULL;
    sensor_sampler_terminated = true;
    sensor_logger_terminated = true;
}


SensorLogger::~SensorLogger()
{
}


int SensorLogger::start()
{
    if( imufifo != NULL ) 
    {
        // 既に開始されている
        return -1;
    }

    sensor_sampler_terminated = true;
    sensor_logger_terminated = true;
    terminate_sensor_logging = false;

    imufifo = new IMUFifo();
    if (imufifo == NULL) 
    {
        ESP_LOGE("SensorLogger", "Failed to create IMUFifo");
        goto error_exit;
    }

    sensor_sampler_terminated = false;
    xTaskCreatePinnedToCore(task_sensor_sampler, "SensorSampler", 2048, NULL, 2, &sensor_sampler_handle, 1);
    if (sensor_sampler_handle == NULL) 
    {
        ESP_LOGE("SensorLogger", "Failed to create SensorSampler task");
        sensor_sampler_terminated = true;
        goto error_exit;
    }

    sensor_logger_terminated = false;
    xTaskCreatePinnedToCore(task_sensor_logger, "SensorLogger", 4096, NULL, 0, &sensor_logger_handle, 1);
    if (sensor_logger_handle == NULL) 
    {
        ESP_LOGE("SensorLogger", "Failed to create SensorLogger task");
        sensor_logger_terminated = true;
        goto error_exit;
    }

    return 0;

error_exit:
    if (imufifo != NULL) 
    {
        delete imufifo;
        imufifo = NULL;
    }
    terminate_sensor_logging = true;
    return -1;
}


int SensorLogger::stop()
{
    terminate_sensor_logging = true;

    // タスクが終了するまで待つ
    while (!sensor_sampler_terminated || !sensor_logger_terminated) 
    {
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }

    // FIFOを削除
    if (imufifo != NULL) 
    {
        delete imufifo;
        imufifo = NULL;
    }
    return 0;
}
