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

int absCoordToScreen (int absCoord) {
    static const double PIXEL_SIZE_IN_MM = 0.264583333;
    return (int) ((double) absCoord / PIXEL_SIZE_IN_MM * 0.01);
}

void completeDrawProc (
    HDC paintDC,
    DrawProcedure& drawProc,
    int startX,
    int startY,
    PaletteIndex paletteIndex,
    Dai& dai,
    int pivotPtCol,
    int pivotPtRow,
    bool patternMode = false
) {
    bool polygonMode = false;
    std::vector<std::vector<POINT>> polyPolygon;
    int curX = 0, curY = 0;
    int penColorIndex = -1;
    int penWidth = 1;
    HPEN savedPen = 0;
    HBRUSH savedBrush = 0;
    auto absPosToScreen = [startX, startY, &pivotPtCol, &pivotPtRow, patternMode] (int absX, int absY, int& screenX, int& screenY) {
        if (!patternMode) {
            absX -= pivotPtCol;
            absY -= pivotPtRow;
        }
        screenX = absCoordToScreen (absX) + startX;
        screenY = absCoordToScreen (absY) + startY;
    };
    auto selectPen = [&savedPen, &paintDC, &penWidth, &penColorIndex, &paletteIndex, &dai, patternMode] () {
        if (patternMode) {
            savedPen = (HPEN) SelectObject (paintDC, (HPEN) GetStockObject (BLACK_PEN));
        } else {
            auto& [pensExist, pens] = penColorIndex >= 0 ? dai.palette.basePens [penColorIndex].get (paletteIndex) : std::tuple<bool, Pens&> (false, Pens ());
            if (pensExist && penWidth > 0) {
                savedPen = (HPEN) SelectObject (paintDC, (HPEN) pens [penWidth-1]);
            }
        }
    };
    auto selectBrush = [&savedBrush, &paintDC, &penColorIndex, &paletteIndex, &dai, patternMode] () {
        if (patternMode) {
            savedBrush = (HBRUSH) SelectObject (paintDC, (HBRUSH) GetStockObject (WHITE_BRUSH));
        } else {
            auto& [brushExists, brush] = penColorIndex >= 0 ? dai.palette.brushes [penColorIndex].get (paletteIndex) : std::tuple<bool, HBRUSH> (false, 0);
            if (brushExists) {
                savedBrush = (HBRUSH) SelectObject (paintDC, (HBRUSH) brush);
            }
        }
    };
    auto restorePen = [&paintDC, &savedPen] () {
        if (savedPen) SelectObject (paintDC, savedPen);
    };
    auto restoreBrush = [&paintDC, &savedBrush] () {
        if (savedBrush) SelectObject (paintDC, savedBrush);
    };
    for (auto& instr: drawProc.instructions) {
        switch (instr.oper) {
            case DrawOperCode::NEW_POLYGON: {
                polygonMode = true;
                polyPolygon.clear ();
                polyPolygon.emplace_back ();
                polyPolygon.back ().emplace_back ();
                polyPolygon.back ().back ().x = curX;
                polyPolygon.back ().back ().y = curY;
                break;
            }
            case DrawOperCode::NEW_SHAPE: {
                if (polygonMode) {
                    polyPolygon.emplace_back ();
                }
                break;
            }
            case DrawOperCode::END_POLYGON: {
                polygonMode = false; break;
            }
            case DrawOperCode::FILL_POLYGON:
            case DrawOperCode::EXEC_POLYGON: {
                std::vector<POINT> vertices;
                std::vector<uint32_t> sizes;
                for (auto& contour: polyPolygon) {
                    sizes.emplace_back ((int) contour.size ());
                    vertices.insert (vertices.begin (), contour.begin (), contour.end ());
                }
                selectPen ();
                if (instr.oper == DrawOperCode::EXEC_POLYGON) {
                    PolyPolyline (paintDC, vertices.data (), (DWORD *) sizes.data (), (int) sizes.size ());
                } else {
                    selectBrush ();
                    PolyPolygon (paintDC, vertices.data (), (int *) sizes.data (), (int) sizes.size ());
                    restoreBrush ();
                }
                restorePen ();
                if (!polygonMode) polyPolygon.clear ();
                break;
            }
            case DrawOperCode::SELECT_PEN: {
                penColorIndex = instr.args [0];
                break;
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
                if (!polygonMode) selectPen ();
                for (size_t i = 0; i < instr.args.size (); i += 2) {
                    absPosToScreen (instr.args [i], instr.args [i+1], curX, curY);
                    if (polygonMode) {
                        auto& vertex = polyPolygon.back ().emplace_back ();
                        vertex.x = curX;
                        vertex.y = curY;                      
                    } else {
                        LineTo (paintDC, curX, curY);
                    }
                }
                break;
            }
            case DrawOperCode::CIRCLE: {
                int radius = absCoordToScreen (instr.args [0]);
                if (polygonMode) {
                    if (radius < 2) {
                        polyPolygon.back ().resize (5);
                        polyPolygon.back () [0].x = curX - 1;
                        polyPolygon.back () [0].y = curY;
                        polyPolygon.back () [1].x = curX;
                        polyPolygon.back () [1].y = curY - 1;
                        polyPolygon.back () [2].x = curX + 1;
                        polyPolygon.back () [2].y = curY;
                        polyPolygon.back () [3].x = curX;
                        polyPolygon.back () [3].y = curY + 1;
                    } else {
                        polyPolygon.back ().resize (37);
                        double rad = (double) radius;
                        double centerX = (double) curX;
                        double centerY = (double) curY;
                        for (int i = 0; i < 36; ++ i) {
                            double brg = (double) i * 10.0 * RAD_IN_DEG;
                            double x = centerX + rad * sin (brg);
                            double y = centerY - rad * cos (brg);
                            polyPolygon.back () [i].x = x;
                            polyPolygon.back () [i].y = y;
                        }
                    }
                    polyPolygon.back ().back ().x = polyPolygon.back ().front ().x;
                    polyPolygon.back ().back ().y = polyPolygon.back ().front ().y;
                } else {
                    selectPen ();
                    savedBrush = (HBRUSH) SelectObject (paintDC, (HBRUSH) GetStockObject (NULL_BRUSH));
                    Ellipse (paintDC, curX - radius, curY - radius, curX + radius, curY + radius);
                    restorePen ();
                    restoreBrush ();
                }
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
    symbolX -= westX;
    symbolY -= northY;
    if (symbolX >= 0 && symbolX <= client.right && symbolY >= 0 && symbolY <= client.bottom) {
        completeDrawProc (paintDC, symbol.drawProc, symbolX, symbolY, paletteIndex, dai, symbol.pivotPtCol, symbol.pivotPtRow);
    }
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
        auto tableSet = (feature.primitive == 1 || feature.primitive == 4) ? pointObjTableSet : spatialObjTableSet;
        auto lookupTable = dai.findLookupTable (feature.classCode, displayCat, tableSet, objectTypes [feature.primitive-1]);

        lookupTables.emplace_back (lookupTable);
    }

    for (int prty = 1; prty < 10; ++ prty) {
        for (size_t i = 0; i < features.size (); ++ i) {
            auto& feature = features [i];
if(feature.fidn==29143965){
int iii=0;
++iii;
--iii;
}
            if (feature.classCode >= 300 && feature.classCode <= 402) continue;
            if (feature.primitive != 2 && feature.primitive != 3) continue;
            if (!lookupTables [i]) continue;
            auto lookupTableItem = feature.findBestItem (displayCat, spatialObjTableSet, dai);
            if (!lookupTableItem || lookupTableItem->displayPriority != prty) continue;

            if (feature.primitive == 3) {
                HBRUSH brush = 0;
                HPEN pen = 0;
                if (lookupTableItem->brushIndex != LookupTableItem::NOT_EXIST) {
                    auto& [brushExists, solidBrush] = dai.palette.brushes [lookupTableItem->brushIndex].get (paletteIndex);
                    if (brushExists) brush = solidBrush;
                } else if (lookupTableItem->patternBrushIndex != LookupTableItem::NOT_EXIST) {
                    auto& [brushExists, patternBrush] = dai.palette.patternBrushes [lookupTableItem->patternBrushIndex].get (paletteIndex);
                    if (brushExists) brush = patternBrush;
                }
                if (brush) {
                    paintArea (client, paintDC, nodes, edges, feature.edgeRefs, dai, north, west, zoom, brush);
                }               
            }

            if (feature.primitive == 2 || feature.primitive == 3) {
                HPEN pen = 0;
                if (lookupTableItem->penIndex != LookupTableItem::NOT_EXIST) {
                    auto& [penExists, penHandle] = dai.palette.pens [lookupTableItem->penIndex].get (paletteIndex);
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

HBRUSH createPatternBrush (PatternDesc& pattern, PaletteIndex paletteIndex, Dai& dai) {
    HDC dc = GetDC (HWND_DESKTOP);
    int fullWidth = absCoordToScreen (pattern.bBoxWidth + pattern.bBoxCol);
    int fullHeight = absCoordToScreen (pattern.bBoxHeight + pattern.bBoxRow);
    HBITMAP bmp = CreateBitmap (fullWidth, fullHeight, 1, 1, 0);
    HDC tempDC = CreateCompatibleDC (dc);
    RECT brushRect;

    brushRect.left = 0;
    brushRect.top = 0;
    brushRect.bottom = fullHeight;
    brushRect.right = fullWidth;
 
    SelectObject (tempDC, bmp);
    FillRect (tempDC, & brushRect, (HBRUSH) GetStockObject (WHITE_BRUSH));
    completeDrawProc (
        tempDC,
        pattern.drawProc,
        0,//absCoordToScreen (pattern.bBoxCol),
        0,//absCoordToScreen (pattern.bBoxRow),
        paletteIndex,
        dai,
        pattern.pivotPtCol,
        pattern.pivotPtRow,
        true
    );
    SelectObject (tempDC, (HBITMAP) 0);
    ReleaseDC (HWND_DESKTOP, dc);
    DeleteDC (tempDC);

    HBRUSH brush = CreatePatternBrush (bmp);
    
    //DeleteObject (bmp);

    return brush;
}