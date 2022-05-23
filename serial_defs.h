#pragma once

#include <cstdint>
#include <vector>

struct PortSettings {
    uint8_t index;
    uint16_t baud;
};

void populatePortList (std::vector<uint8_t>& ports);
