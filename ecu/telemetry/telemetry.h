#pragma once

enum DestType { //where are we sending data to
    NETWORK = 0,
    STORAGE = 1,
    SERIAL = 2, 
    OTHER = 3
};

struct TelemetryPacket{
    uint32_t systemState;
    uint32_t timestamp;
    bmeValues bme;
    scdValues scd;
    sgpValues sgp;
    uint16_t checksum;
}
void buildTelemetryPacket();
void sendTelemetry();

uint16_t telemetryChecksum();