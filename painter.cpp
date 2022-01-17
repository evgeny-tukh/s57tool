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

void completeDrawProc (HDC paintDC, DrawProcedure& drawProc, int startX, int startY, PaletteIndex paletteIndex, Dai& dai) {
    int curX = 0, curY = 0;
    int penColorIndex = -1;
    int penWidth = 0;
    auto absCoordToScreen = [] (int absCoord) {
        static const double PIXEL_SIZE_IN_MM = 0.264583333;
        return (int) ((double) absCoord / PIXEL_SIZE_IN_MM * 0.01);
    };
    auto absPosToScreen = [startX, startY, &absCoordToScreen] (int absX, int absY, int& screenX, int& screenY) {
        screenX = absCoordToScreen (absX) + startX;
        screenY = absCoordToScreen (absY) + startY;
    };
    auto selectPen = [&paintDC, &penWidth, &penColorIndex, &paletteIndex, &dai] () {
        auto [pensExist, pens] = penColorIndex >= 0 ? dai.palette.basePens [penColorIndex].get (paletteIndex) : std::tuple<bool, Pens> (false, Pens ());
        if (pensExist && penWidth > 0) {
            SelectObject (paintDC, pens [penWidth-1]);
        }
    };
    for (auto& instr: drawProc.instructions) {
        switch (instr.oper) {
            case DrawOperCode::SELECT_PEN: {
                penColorIndex = instr.args [0]; break;
            }
            case DrawOperCode::SELECT_PEN_WIDTH: {
                penWidth = instr.args [0]; break;
            }
            case DrawOperCode::PEN_UP: {
                absPosToScreen (instr.args [0], instr.args [1], curX, curY);
                MoveToEx (paintDC, curX, curY, 0);
                break;
            }
            case DrawOperCode::PEN_DOWN: {
                selectPen ();
                for (size_t i = 0; i < instr.args.size (); i += 2) {
                    absPosToScreen (instr.args [i], instr.args [i+1], curX, curY);
                    LineTo (paintDC, curX, curY);
                }
                break;
            }
            case DrawOperCode::CIRCLE: {
                int radius = absCoordToScreen (instr.args [0]);
                selectPen ();
                Ellipse (paintDC, curX - radius, curY - radius, curX + radius, curY + radius);
                break;
            }
        }
    }
}

void paintSymbol (
    RECT& client,
    HDC paintDC,
    GeoNode& node,
    Dai& dai,
    double north,
    double west,
    uint8_t zoom,
    int& offset,
    size_t symbolIndex,
    PaletteIndex paletteIndex
) {
    int symbolX, symbolY;
    auto& symbol = dai.symbols [symbolIndex];
    int westX, northY;
    geoToXY (north, west, zoom, westX, northY);
    geoToXY (node.points.front ().lat, node.points.front ().lon, zoom, symbolX, symbolY);
    completeDrawProc (paintDC, symbol.drawProc, symbolX - westX, symbolY - northY, paletteIndex, dai);
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
    TableSet spatialObjTableSet,
    TableSet pointObjTableSet
) {
    static char *objectTypes { "PLA" };
    std::vector<LookupTable *> lookupTables;

    for (auto& feature: features) {
if(feature.primitive==1)
{
int iii=0;
++iii;
--iii;
}
        auto tableSet = (feature.primitive == 1 || feature.primitive == 4) ? pointObjTableSet : spatialObjTableSet;
        auto lookupTable = dai.findLookupTable (feature.classCode, displayCat, tableSet, objectTypes [feature.primitive-1]);

        lookupTables.emplace_back (lookupTable);
    }

    for (int prty = 1; prty < 10; ++ prty) {
        for (size_t i = 0; i < features.size (); ++ i) {
            auto& feature = features [i];
            if (feature.primitive != 2 && feature.primitive != 3) continue;
            if (!lookupTables [i]) continue;
            auto lookupTableItem = feature.findBestItem (displayCat, spatialObjTableSet, dai);
            if (!lookupTableItem || lookupTableItem->displayPriority != prty) continue;

            if (feature.primitive == 3) {
                if (lookupTableItem->brushIndex != LookupTableItem::NOT_EXIST) {
                    auto [brushExists, brush] = dai.palette.brushes [lookupTableItem->brushIndex].get (paletteIndex);
                    if (brushExists) {
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
                }               
            }

            if (feature.primitive == 2 || feature.primitive == 3) {
                HPEN pen;
                if (lookupTableItem->penIndex != LookupTableItem::NOT_EXIST) {
                    auto [penExists, penHandle] = dai.palette.pens [lookupTableItem->penIndex].get (paletteIndex);
                    pen = penExists ? penHandle : 0;
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

    for (size_t i = 0; i < features.size (); ++ i) {
        auto& feature = features [i];
        if (feature.primitive != 1) continue;
        if (!lookupTables [i]) continue;
        auto lookupTableItem = feature.findBestItem (displayCat, pointObjTableSet, dai);
        if (!lookupTableItem) continue;
        int offset = 0;
        for (auto symbolIndex: lookupTableItem->symbols) {
            paintSymbol (client, paintDC, nodes [feature.nodeIndex], dai, north, west, zoom, offset, symbolIndex, paletteIndex);
        }
    }
}