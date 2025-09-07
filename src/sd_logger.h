#ifndef SD_LOGGER_H
#define SD_LOGGER_H

#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <M5Unified.h>

#include <time.h>

#define SD_STATUS_ERROR 0
#define SD_STATUS_READY 1

class SDLogger 
{
private:
    int sd_status;
    File logFile;
    char prefix[64];
    char filename[96];
    uint8_t *log_buffer;
    size_t buffer_pos;
    const size_t buffer_size = 4096; // バッファサイズ

public:
    SDLogger();
    int set_prefix(const char* pre);
    int start();
    int restart();
    int close();
    int write_data(const uint8_t* data, size_t length);
    int flush();
    ~SDLogger();
    int get_status(){ return sd_status; }

};

extern int sd_init();

#endif // SD_LOGGER_H

