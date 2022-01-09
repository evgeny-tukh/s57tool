#pragma once

#include <cstdint>
#include <Windows.h>
#include "data.h"

void paintChart (
    RECT& client,
    HDC paintDC,
    Nodes& nodes,
    Edges& edges,
    Features& features,
    Dai& dai,
    double north,
    double west,
    uint8_t zoom
);
