#include <string>
#include <vector>
#include "drawers.h"
#include "painter.h"

PenTool Drawer::penTool;

void LineDrawer::run (HDC paintDC, PaletteIndex paletteIndex, Palette& palette) {
    paintLine (paintDC, penStyle, penWidth, penIndex, lat, lon, brg, rangeMm, north, west, zoom, paletteIndex, palette);
}

void ArcDrawer::run (HDC paintDC, PaletteIndex paletteIndex, Palette& palette) {
    paintArc (paintDC, penStyle, penWidth, penIndex, lat, lon, start, end, rangeMm, north, west, zoom, paletteIndex, palette);
}

void DrawQueue::addCompoundLightArc (
    int penIndex,
    int penStyle,
    int penWidth,
    double centerLat,
    double centerLon,
    double radiusMm,
    double start,
    double end) {
    addArc (dai.getBasePenIndex ("OUTLW"), penStyle, 2, centerLat, centerLon, radiusMm + 2.0 * PIXEL_SIZE_IN_MM, start, end);
    addArc (dai.getBasePenIndex ("OUTLW"), penStyle, 2, centerLat, centerLon, radiusMm - 2.0 * PIXEL_SIZE_IN_MM, start, end);
    addArc (penIndex, PS_SOLID, 4, centerLat, centerLon, radiusMm, start, end);
}
