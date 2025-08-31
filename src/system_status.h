#ifndef SYSTEM_STATUS_H
#define SYSTEM_STATUS_H

#include "nmea_parser.h"

typedef struct {
    unsigned int update_count; // 更新回数
    int gps_status; // GPS status
    int gps_satellites; // Number of satellites used
    nmea_gsv_data_all_t gsv_data;    // NMEA GSV data
    nmea_rmc_data_t rmc_data;    // NMEA RMC data
    nmea_gga_data_t gga_data;    // NMEA GGA data
    float temp;
    float pressure;
} system_status_t;

extern system_status_t sys_status;

#endif // SYSTEM_STATUS_H
