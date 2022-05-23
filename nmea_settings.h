#pragma once

#include "serial_defs.h"

struct NmeaSettings {
    PortSettings gps, gyro;
    bool checkCRC;
};
