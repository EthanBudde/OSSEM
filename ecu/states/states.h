//states.h
//
//brief: shared enums for state values
//       we build stuff with these 
//          
#pragma once

#include <Arduino.h>

enum DeviceType {
    DEVICE_NULL  = 0,
    DEVICE_ARB   = 1,
    DEVICE_ECU   = 2,
    DEVICE_GAGGI = 3
};

enum FanSpeedState {
    FAN_OFF  = 0,
    FAN_LOW  = 1,
    FAN_MID  = 2,
    FAN_HIGH = 3
};

enum SensorBits {
    SENSOR_BME = 0,
    SENSOR_SCD = 1,
    SENSOR_SGP = 2
};

union SystemState {
    uint32_t raw;
    struct {
        uint32_t deviceID            : 8;
        uint32_t deviceType          : 2;
        uint32_t timesince           : 8;

        uint32_t sensors_functional  : 3;

        uint32_t network_active      : 1;
        uint32_t time_synced         : 1;
        uint32_t data_capable        : 1;
        uint32_t logging_active      : 1;

        uint32_t fancount            : 4;
        uint32_t fan_speed_state     : 2;

        uint32_t fault               : 1;
    };

};

extern SystemState systemState;

void initializeSystemState();

String systemStateBinary();
String systemStateHex();

String deviceTypeString();