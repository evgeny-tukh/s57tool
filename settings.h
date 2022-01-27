#pragma once

struct Settings {
    bool fullSectorLength, safetyContourLabels, twoShades, shallowPattern, showIsolatedDanger;
    double safetyContour, shallowContour, deepContour, safetyDepth;

    Settings ();
};
