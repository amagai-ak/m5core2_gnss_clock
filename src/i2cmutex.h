#ifndef I2C_MUTEX_H
#define I2C_MUTEX_H

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

class I2CMutex 
{
private:
    SemaphoreHandle_t mutex;

public:
    I2CMutex() 
    {
        mutex = xSemaphoreCreateMutex();
    }

    ~I2CMutex() 
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

};

extern I2CMutex i2c_mutex;
#endif // I2C_MUTEX_H
