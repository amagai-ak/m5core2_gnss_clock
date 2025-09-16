
#ifndef SENSOR_LOGGER_H
#define SENSOR_LOGGER_H

#include "sd_logger.h"
#include "bus_mutex.h"

typedef struct {
    struct timeval timestamp; // タイムスタンプ
    uint32_t count;      // サンプル番号
    float ax, ay, az;
    float gx, gy, gz;
    int16_t mx, my, mz;
} imu_record_t;

class SensorLogger 
{
protected:

public:
    SensorLogger();
    ~SensorLogger();

    int start();
    int stop();
    int init();
};

#endif // SENSOR_LOGGER_H
