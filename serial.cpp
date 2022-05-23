#include <Windows.h>
#include <math.h>
#include <stdio.h>
#include <cstdint>
#include <vector>
#include <thread>
#include <string>

const uint8_t ASCII_BEL = 0x07;
const uint8_t ASCII_BS = 0x08;
const uint8_t ASCII_LF = 0x0A;
const uint8_t ASCII_CR = 0x0D;
const uint8_t ASCII_XON = 0x11;
const uint8_t ASCII_XOFF = 0x13;

uint8_t calcCrc (char *sentence) {
    uint8_t crc = sentence [1];

    for (int i = 2; sentence [i] != '*' && i < 81; crc ^= sentence [i++]);

    return crc;
}

void getSerialPortsList (std::vector<std::string>& ports) {
    HKEY scomKey;
    int count = 0;
    DWORD error = RegOpenKeyEx (HKEY_LOCAL_MACHINE, "Hardware\\DeviceMap\\SerialComm", 0, KEY_QUERY_VALUE, & scomKey);

    if (error == S_OK) {
        char valueName[100], valueValue[100];
        unsigned long nameSize, valueSize, valueType;
        bool valueFound;

        ports.clear ();

        do {
            nameSize = sizeof (valueName);
            valueSize = sizeof (valueValue);
            valueFound = RegEnumValue (scomKey, (unsigned long) count ++, valueName, & nameSize, 0, & valueType, (unsigned char *) valueValue, & valueSize) == S_OK;

            if (valueFound) ports.push_back (valueValue);
        } while (valueFound);

        RegCloseKey (scomKey);
    }
}
