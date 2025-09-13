#ifndef SIMPLE_MUTEX_H
#define SIMPLE_MUTEX_H

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

class SimpleMutex 
{
private:
    SemaphoreHandle_t mutex;

public:
    SimpleMutex() 
    {
        mutex = xSemaphoreCreateMutex();
    }

    ~SimpleMutex() 
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

#endif // SIMPLE_MUTEX_H
