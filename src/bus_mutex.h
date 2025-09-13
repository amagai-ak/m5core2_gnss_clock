#ifndef BUS_MUTEX_H
#define BUS_MUTEX_H

#include "simple_mutex.h"

extern SimpleMutex i2c_mutex;
extern SimpleMutex spi_mutex;
#endif // BUS_MUTEX_H
