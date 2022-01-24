#pragma once

struct Settings {
    bool fullSectorLength;
    double safetyContour = 30.0;
};

extern Settings settings;