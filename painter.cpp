#include "painter.h"
#include "geo.h"

void paintArea (
    RECT& client,
    HDC paintDC,
    Nodes& nodes,
    Edges& edges,
    std::vector<EdgeRef>& edgeRefs,
    Dai& dai,
    double north,
    double west,
    uint8_t zoom,
    HBRUSH brush
) {
    std::vector<POINT> vertices;

    int westX, northY;
    geoToXY (north, west, zoom, westX, northY);

    auto addVertex = [&vertices, zoom, westX, northY] (double lat, double lon) {
        int x, y;
        geoToXY (lat, lon, zoom, x, y);
        auto& point = vertices.emplace_back ();
        point.x = x - westX;
        point.y = y - northY;
    };
    auto addNode = [addVertex, &nodes] (size_t nodeIndex) {
        auto& pos = nodes [nodeIndex].points [0];
        addVertex (pos.lat, pos.lon);
    };

    for (auto& edgeRef: edgeRefs) {
        auto& edge = edges [edgeRef.index];
        if (edgeRef.hole || edge.hidden) continue;
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

    vertices.push_back (vertices [0]);

    SelectObject (paintDC, (HPEN) GetStockObject (NULL_PEN));
    SelectObject (paintDC, brush);
    Polygon (paintDC, vertices.data (), (int) vertices.size ());
}

void paintEdge (
    RECT& client,
    HDC paintDC,
    Nodes& nodes,
    GeoEdge& edge,
    Dai& dai,
    double north,
    double west,
    uint8_t zoom,
    HPEN pen
) {
    std::vector<POINT> vertices;

    int westX, northY;
    geoToXY (north, west, zoom, westX, northY);

    auto addVertex = [&vertices, zoom, westX, northY] (double lat, double lon) {
        int x, y;
        geoToXY (lat, lon, zoom, x, y);
        auto& point = vertices.emplace_back ();
        point.x = x - westX;
        point.y = y - northY;
    };
    auto addNode = [addVertex, &nodes] (size_t nodeIndex) {
        auto& pos = nodes [nodeIndex].points [0];
        addVertex (pos.lat, pos.lon);
    };

    addNode (edge.beginIndex);
    for (auto pos: edge.internalNodes) {
        addVertex (pos.lat, pos.lon);
    }
    addNode (edge.endIndex);

    SelectObject (paintDC, pen);
    Polyline (paintDC, vertices.data (), (int) vertices.size ());
}

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
    TableSet tableSet
) {
    static char *objectTypes { "PLA" };
    std::vector<LookupTable *> lookupTables;
    Palette *palette = dai.getPalette (paletteIndex);
    
    if (!palette) return;

    for (auto& feature: features) {
        auto lookupTable = dai.findLookupTable (feature.classCode, displayCat, tableSet, objectTypes [feature.primitive-1]);

        lookupTables.emplace_back (lookupTable);
    }

    for (int prty = 1; prty < 10; ++ prty) {
        for (size_t i = 0; i < features.size (); ++ i) {
            auto& feature = features [i];
            if (!lookupTables [i]) continue;
            auto lookupTableItem = feature.findBestItem (displayCat, tableSet, dai);
            if (!lookupTableItem || lookupTableItem->displayPriority != prty) continue;

            if (feature.primitive == 3) {
                size_t brushIndex = lookupTableItem->getBrushIndex (paletteIndex);
                if (brushIndex != LookupTableItem::NOT_EXIST) {
                    paintArea (
                        client,
                        paintDC,
                        nodes,
                        edges,
                        feature.edgeRefs,
                        dai,
                        north,
                        west,
                        zoom,
                        palette->brushes [brushIndex]
                    );
                }
            }

            if (feature.primitive == 2 || feature.primitive == 3) {
                size_t penIndex = lookupTableItem->getPenIndex (paletteIndex);
                HPEN pen;
                if (penIndex != LookupTableItem::NOT_EXIST) {
                    pen = palette->pens [penIndex];
                } else {
                    pen = (HPEN) GetStockObject (BLACK_PEN);
                }
                for (auto& edgeRef: feature.edgeRefs) {
                    if (edgeRef.hidden) continue;
                    paintEdge (client, paintDC, nodes, edges [edgeRef.index], dai, north, west, zoom, pen);
                }
            }
        }
    }
}