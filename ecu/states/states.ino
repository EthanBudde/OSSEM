#include "states.h"

void initializeSystemState(){
    systemState.raw = 0;
    systemState.deviceID = 0x01;
    systemState.deviceType = DEVICE_ECU;
    systemState.fancount = 2;
    systemState.fan_speed_state = FAN_OFF;
}

String systemStateBinary(){
    return String(systemState.raw, BIN);
}

String systemStateHex(){
    char buffer[11];
    sprintf(buffer, "0x%08X", systemState.raw);
    return String(systemState.raw, HEX);
}

String deviceTypeString(){
    switch(systemState.deviceType){
        case DEVICE_ARB:
            return "ARB";

        case DEVICE_ECU:
            return "ECU";

        case DEVICE_GAGGI:
            return "GAGGI";

        default:
            return "NULL";
    }
}
