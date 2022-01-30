#include "settings.h"

Settings::Settings ():
    fullSectorLength (false),
    safetyContourLabels (false),
    displayContourLabels (true),
    showLightDescriptions (true),
    twoShades (false),
    symbolizedBoundaries (false),
    showIsolatedDanger (false),
    showLowAccuracy (true),
    shallowPattern (true),
    safetyContour (30.0),
    shallowContour (2.0),
    deepContour (30.0),
    safetyDepth (30.0) {
}