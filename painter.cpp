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
    uint8_t zoom
) {
    static char *objectTypes { "PLA" };
    std::vector<LookupTable *> lookupTables;
    
    for (auto& feature: features) {
        auto lookupTable = dai.findLookupTable (feature.classCode, DisplayCat::STANDARD, TableSet::PLAIN_BOUNDARIES, objectTypes [feature.primitive-1]);

        lookupTables.emplace_back (lookupTable);
    }

    for (int prty = 1; prty < 10; ++ prty) {
        for (size_t i = 0; i < features.size (); ++ i) {
            auto& feature = features [i];

            if (!lookupTables [i]) continue;
            auto& lookupTableItem = lookupTables [i]->at (0);
            if (lookupTableItem.displayPriority != prty) continue;

            HBRUSH brush = 0;

            if (feature.primitive == 3) {
                brush = lookupTableItem.dayBrushIndex >= 0 ? dai.day.brushes [lookupTableItem.dayBrushIndex] : (HBRUSH) GetStockObject (WHITE_BRUSH);
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
                    brush
                );
            }

            for (auto& edgeRef: feature.edgeRefs) {
                if (edgeRef.hidden) continue;
                HPEN pen = 0;
                
                if (feature.primitive == 2 || feature.primitive == 3) {
                    pen = lookupTableItem.dayPenIndex >= 0 ? dai.day.pens [lookupTableItem.dayPenIndex] : (HPEN) GetStockObject (BLACK_PEN);

                    paintEdge (
                        client,
                        paintDC,
                        nodes,
                        edges [edgeRef.index],
                        dai,
                        north,
                        west,
                        zoom,
                        pen
                    );
                }
            }
        }
    }
}