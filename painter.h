#pragma once

#include <cstdint>
#include <Windows.h>
#include "data.h"
#include "geo.h"

void paintCompoundArc (
    RECT& client,
    HDC paintDC,
    ArcDef& arcDef,
    Dai& dai,
    double north,
    double west,
    uint8_t zoom,
    double centerLat,
    double centerLon,
    PaletteIndex paletteIndex
);

void paintChart (
    RECT& client,
    HDC paintDC,
    Chart& chart,
    Dai& dai,
    AttrDictionary &attrDic,
    View& view,
    PaletteIndex paletteIndex,
    DisplayCat displayCat,
    TableSet spatialObjTableSet,
    TableSet pointObjTableSet
);

HBRUSH createPatternBrush (PatternDesc& pattern, PaletteIndex paletteIndex, Dai& dai);

struct PatternTool {
    HBITMAP andMask;
    HBITMAP orMask;
    int width, height;

    PatternTool (): andMask (0), orMask (0), width (0), height (0) {}
    PatternTool (PatternDesc& pattern, PaletteIndex paletteIndex, Dai& dai);
    void deleteMasks ();
    void paint (HDC dc, std::vector<std::vector<POINT>>& polyPolygon);
};

void createPatternTools (Dai& dai);
void deletePatternTools ();

void paintLine (
    RECT& client,
    HDC paintDC,
    int style,
    int width,
    size_t colorIndex,
    double lat,
    double lon,
    double brg,
    double lengthInMm,
    View& view,
    PaletteIndex paletteIndex,
    Palette& palette
);
void paintArc (
    RECT& client,
    HDC paintDC,
    int style,
    int width,
    size_t colorIndex,
    double centerLat,
    double centerLon,
    double start,
    double end,
    double radiusInMm,
    View& view,
    PaletteIndex paletteIndex,
    Palette& palette
);
void paintPolyPolyline (
    RECT& client,
    HDC paintDC,
    int style,
    int width,
    size_t colorIndex,
    Contours& contours,
    View& view,
    PaletteIndex paletteIndex,
    Palette& palette
);
void paintPolyPolygon (
    RECT& client,
    HDC paintDC,
    size_t fillBrushIndex,
    size_t patternBrushIndex,
    Contours& contours,
    View& view,
    PaletteIndex paletteIndex,
    Palette& palette
);
void paintText (
    RECT& client,
    HDC paintDC,
    char *text,
    unsigned int format,
    double lat,
    double lon,
    int xOffset,
    int yOffset,
    size_t colorIndex,
    View& view,
    PaletteIndex paletteIndex,
    Dai& dai
);
void paintSymbol (
    RECT& client,
    HDC paintDC,
    double lat,
    double lon,
    size_t symbolIndex,
    double rotAngle,
    Dai& dai,
    View& view,
    PaletteIndex paletteIndex
);
