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
    Nodes& nodes,
    Edges& edges,
    Features& features,
    Dai& dai,
    double north,
    double west,
    uint8_t zoom,
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
    double north,
    double west,
    int zoom,
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
    double north,
    double west,
    int zoom,
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
    double north,
    double west,
    int zoom,
    PaletteIndex paletteIndex,
    Palette& palette
);
