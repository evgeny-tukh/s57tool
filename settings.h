#pragma once

struct Settings {
    bool fullSectorLength, safetyContourLabels, twoShades, shallowPattern, showIsolatedDanger, showLowAccuracy, symbolizedBoundaries, displayContourLabels;
    bool showLightDescriptions;
    double safetyContour, shallowContour, deepContour, safetyDepth;

    Settings ();
};




