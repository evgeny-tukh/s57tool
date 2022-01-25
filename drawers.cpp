#include <string>
#include <vector>
#include "drawers.h"
#include "painter.h"
#include "abstract_tools.h"

PenTool Drawer::penTool;

void LineDrawer::run (RECT& client, HDC paintDC, PaletteIndex paletteIndex, Palette& palette) {
    paintLine (client, paintDC, penStyle, penWidth, penIndex, lat, lon, brg, rangeMm, north, west, zoom, paletteIndex, palette);
}

void ArcDrawer::run (RECT& client, HDC paintDC, PaletteIndex paletteIndex, Palette& palette) {
    paintArc (client, paintDC, penStyle, penWidth, penIndex, lat, lon, start, end, rangeMm, north, west, zoom, paletteIndex, palette);
}

void PolyPolylineDrawer::run (RECT& client, HDC paintDC, PaletteIndex paletteIndex, Palette& palette) {
    paintPolyPolyline (client, paintDC, penStyle, penWidth, penIndex, polyPolyline, north, west, zoom, paletteIndex, palette);
}

void PolyPolylineDrawer::addNode (size_t nodeIndex) {
    auto& pos = nodes [nodeIndex].points [0];
    addVertex (pos.lat, pos.lon);
}

void PolyPolylineDrawer::addEdge (EdgeRef& edgeRef) {
    if (edgeRef.hidden) return;

    auto& edge = edges.container [edgeRef.index];

    addContour ();
    if (edgeRef.unclockwise) {
        addNode (edge.endIndex);
        for (auto pos = edge.internalNodes.rbegin (); pos != edge.internalNodes.rend (); ++ pos) {
            addVertex (pos->lat, pos->lon);
        }
        addNode (edge.beginIndex);
    } else {
        addNode (edge.beginIndex);
        for (auto pos: edge.internalNodes) {
            addVertex (pos.lat, pos.lon);
        }
        addNode (edge.endIndex);
    }
}

void PolyPolygonDrawer::run (RECT& client, HDC paintDC, PaletteIndex paletteIndex, Palette& palette) {
    paintPolyPolygon (client, paintDC, fillBrushIndex, patternBrushIndex, polyPolyline, north, west, zoom, paletteIndex, palette);
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

void DrawQueue::addEdgeChain (int penIndex, int penStyle, int penWidth, Nodes& nodes, Edges& edges) {
    container.push_back (new PolyPolylineDrawer (penIndex, penStyle, penWidth, north, west, zoom, nodes, edges));
}

void DrawQueue::addEdge (EdgeRef& edgeRef) {
    if (container.size () > 0) {
        PolyPolylineDrawer *drawer = (PolyPolylineDrawer *) container.back ();

        drawer->addEdge (edgeRef);
    }
}

