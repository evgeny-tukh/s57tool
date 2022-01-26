#pragma once

struct Settings {
    bool fullSectorLength, safetyContourLabels, twoShades, shallowPattern;
    double safetyContour, shallowContour, deepContour, safetyDepth;

    Settings ();
};
