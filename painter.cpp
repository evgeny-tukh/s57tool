#include "painter.h"
#include "geo.h"

void paintEdge (
    RECT& client,
    HDC paintDC,
    Nodes& nodes,
    GeoEdge& edge,
    Dai& dai,
    double north,
    double west,
    uint8_t zoom
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

    SelectObject (paintDC, GetStockObject (BLACK_PEN));
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
            if (lookupTables [i]->at (0).displayPriority != prty) continue;

            for (auto& edgeRef: feature.edgeRefs) {
                if (edgeRef.hidden) continue;
                paintEdge (
                    client,
                    paintDC,
                    nodes,
                    edges [edgeRef.index],
                    dai,
                    north,
                    west,
                    zoom
                );
            }
        }
    }
}