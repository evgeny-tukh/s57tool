#pragma once

struct Settings {
    bool fullSectorLength, safetyContourLabels, twoShades, shallowPattern, showIsolatedDanger, showLowAccuracy, symbolizedBoundaries;
    double safetyContour, shallowContour, deepContour, safetyDepth;

    Settings ();
};
