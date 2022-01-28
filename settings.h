#pragma once

struct Settings {
    bool fullSectorLength, safetyContourLabels, twoShades, shallowPattern, showIsolatedDanger, showLowAccuracy, symbolizedBoundaries, displayContourLabels;
    double safetyContour, shallowContour, deepContour, safetyDepth;

    Settings ();
};
