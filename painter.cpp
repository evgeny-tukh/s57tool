#include "painter.h"
#include "geo.h"
#include "abstract_tools.h"
#include "drawers.h"

HBRUSH createPatternBrush (PatternDesc& pattern, PaletteIndex paletteIndex, Dai& dai);
void paintLine (RECT& client, HDC paintDC, Dai& dai, double north, double west, uint8_t zoom, LineDraw& line, PaletteIndex paletteIndex);

std::vector<DrawToolItem <PatternTool>> patternTools;

HPEN getBasePen (size_t colorIndex, int width, PaletteIndex paletteIndex, Palette& palette) {
    if (width > 0 && width <= 6) {
        auto [penExists, pens] = palette.basePens [colorIndex].get (paletteIndex);

        return penExists ? pens [width-1] : 0;
    } else {
        return 0;
    }
}

void createPatternTools (Dai& dai) {
    for (auto& pattern: dai.patterns) {
        patternTools.emplace_back (
            PatternTool (pattern, PaletteIndex::Day, dai),
            PatternTool (pattern, PaletteIndex::Dusk, dai),
            PatternTool (pattern, PaletteIndex::Night, dai)
        );
    }
}

void deletePatternTools () {
    for (auto& patternTool: patternTools) {
        patternTool.day.deleteMasks ();
        patternTool.dusk.deleteMasks ();
        patternTool.night.deleteMasks ();
    }

    patternTools.clear ();
}

inline int absCoordToScreen (int absCoord) {
    return (int) ((double) absCoord / PIXEL_SIZE_IN_MM * 0.01);
}

PatternTool::PatternTool (PatternDesc& pattern, PaletteIndex paletteIndex, Dai& dai): andMask (0), orMask (0) {
    HDC dc = GetDC (HWND_DESKTOP);
    HDC tempDC = CreateCompatibleDC (dc);
    width = GetDeviceCaps (dc, HORZRES);
    height = GetDeviceCaps (dc, VERTRES);
    HBRUSH brush = createPatternBrush (pattern, paletteIndex, dai);
    RECT bmpRect;
    COLORREF color;

    ColorItem& colorItem = dai.colorTable.container [pattern.drawProc.penColors.begin ()->second];
    ColorDef *colorDef = colorItem.getColorDef (paletteIndex);

    color = RGB (colorDef->red, colorDef->green, colorDef->blue);

    bmpRect.left = bmpRect.top = 0;
    bmpRect.bottom = height - 1;
    bmpRect.right = width - 1;

    andMask = CreateCompatibleBitmap (dc, width, height);
    orMask = CreateCompatibleBitmap (dc, width, height);
    
    SelectObject (tempDC, andMask);
    FillRect (tempDC, & bmpRect, (HBRUSH) GetStockObject (WHITE_BRUSH));
    SetTextColor (tempDC, 0);
    SetBkColor (tempDC, RGB (255, 255, 255));
    FillRect (tempDC, & bmpRect, brush);

    SelectObject (tempDC, orMask);
    FillRect (tempDC, & bmpRect, (HBRUSH) GetStockObject (BLACK_BRUSH));
    SetTextColor (tempDC, color);
    SetBkColor (tempDC, 0);
    FillRect (tempDC, & bmpRect, brush);

    DeleteDC (tempDC);
    DeleteObject (brush);
    ReleaseDC (HWND_DESKTOP, dc);
}

void PatternTool::deleteMasks () {
    DeleteObject (andMask);
    DeleteObject (orMask);
}

void PatternTool::paint (HDC dc, std::vector<std::vector<POINT>>& polyPolygon) {
    RECT client;
    GetClientRect (WindowFromDC (dc), & client);

    std::vector<POINT> vertices;
    std::vector<int> sizes;
    for (auto& contour: polyPolygon) {
        sizes.emplace_back ((int) contour.size ());
        vertices.insert (vertices.begin (), contour.begin (), contour.end ());
    }
    int minX, minY, maxX, maxY;
    minX = maxX = vertices.front ().x;
    minY = maxY = vertices.front ().y;

    for (auto& pt: vertices) {
        if (pt.x < minX) minX = pt.x;
        if (pt.x > maxX) maxX = pt.x;
        if (pt.y < minY) minY = pt.y;
        if (pt.y > maxY) maxY = pt.y;
    }

    if (maxX < client.left || minX > client.right || maxY < client.top || minY > client.bottom) return;

    HDC andDC = CreateCompatibleDC (dc);
    HDC orDC = CreateCompatibleDC (dc);
    HRGN region = CreatePolyPolygonRgn (vertices.data (), sizes.data (), sizes.size (), WINDING);
    SelectClipRgn (dc, region);
    SelectObject (andDC, andMask);
    SelectObject (orDC, orMask);

    int actualWidth = min (width, maxX - minX + 1);
    int actualHeight = min (height, maxY - minY + 1);

    for (int x = minX; x <= maxX; x += width) {
        for (int y = minY; y <= maxY; y += height) {
            BitBlt (dc, x, y, actualWidth, actualHeight, andDC, 0, 0, SRCAND);
            BitBlt (dc, x, y, actualWidth, actualHeight, orDC, 0, 0, SRCPAINT);
        }
    }

    SelectClipRgn (dc, 0);
    DeleteObject (region);
    DeleteDC (andDC);
    DeleteDC (orDC);
}

void closeContour (std::vector<POINT>& contour) {
    if (contour.front ().x != contour.back ().x || contour.front ().y != contour.back ().y) {
        contour.push_back (contour.front ());
    }
}

void reverseContour (std::vector<POINT>& contour) {
    size_t mirror = contour.size () - 1;
    for (size_t i = 0; i < mirror; ++ i, -- mirror) {
        contour [i].x = contour [mirror].x;
        contour [i].y = contour [mirror].y;
    }
}

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
    HBRUSH brush,
    //bool patternMode
    PatternTool *patternTool
) {
    std::vector<POINT> vertices;
    std::vector<std::vector<POINT>> polyPolygon;

    int westX, northY;
    geoToXY (north, west, zoom, westX, northY);

    auto addVertex = [&vertices, zoom, westX, northY, &polyPolygon] (double lat, double lon) {
        int x, y;
        geoToXY (lat, lon, zoom, x, y);
        x -= westX;
        y -= northY;
        if (vertices.size () == 0 || vertices.back ().x != x || vertices.back ().y != y) {
            auto& point = vertices.emplace_back ();
            point.x = x;
            point.y = y;
            auto& vertex = polyPolygon.back ().emplace_back ();
            vertex.x = x;
            vertex.y = y;
        }
    };
    auto addNode = [addVertex, &nodes] (size_t nodeIndex) {
        auto& pos = nodes [nodeIndex].points [0];
        addVertex (pos.lat, pos.lon);
    };

    polyPolygon.emplace_back ();
    bool hole = false;
    for (auto& edgeRef: edgeRefs) {
        auto& edge = edges [edgeRef.index];
        if (edge.hidden) continue;
        if (edgeRef.hole != hole) {
            closeContour (polyPolygon.back ());
            if (hole) reverseContour (polyPolygon.back ());
            hole = edgeRef.hole;
            polyPolygon.emplace_back ();
        }
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
    closeContour (polyPolygon.back ());
    if (hole) reverseContour (polyPolygon.back ());

    if (vertices.front ().x != vertices.back ().x || vertices.front ().y != vertices.back ().y) {
        vertices.push_back (vertices.front ());
    }

    for (size_t i = 1; i < vertices.size () - 1; ++ i) {
        auto& vertex = vertices [i];
        if (vertex.x == vertices.front ().x && vertex.y == vertices.front ().y) {
            int iii = 0;
            ++ iii;
            -- iii;
        }
    }

    #if 1
    if (patternTool) {
        patternTool->paint (paintDC, polyPolygon);
    } else {
        SelectObject (paintDC, (HPEN) GetStockObject (NULL_PEN));
        SelectObject (paintDC, brush);
        std::vector<POINT> area;
        std::vector<int> sizes;
        for (auto& contour: polyPolygon) {
            sizes.emplace_back ((int) contour.size ());
            area.insert (area.end (), contour.begin (), contour.end ());
        }
        PolyPolygon (paintDC, area.data (), sizes.data (), (int) sizes.size ());
    }
    #else
    if (patternTool) {
        std::vector<std::vector<POINT>> polyPolygon;
        polyPolygon.emplace_back ();
        polyPolygon.back ().insert (polyPolygon.back ().begin (), vertices.begin (), vertices.end ());
        //PatternTool patternTool (dai.patterns [0], PaletteIndex::Day, dai, "LANDF");
        patternTool->paint (paintDC, polyPolygon);
    } else {
        SelectObject (paintDC, (HPEN) GetStockObject (NULL_PEN));
        SelectObject (paintDC, brush);
        Polygon (paintDC, vertices.data (), (int) vertices.size ());
    }
    #endif
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
    int bBoxCol,
    int bBoxRow,
    double rotAngleDeg,
    bool patternMode = false
) {
    bool polygonMode = false;
    std::vector<std::vector<POINT>> polyPolygon;
    int curX = 0, curY = 0;
    int penColorIndex = -1;
    int penWidth = 1;
    int pivotPtX, pivotPtY;
    HPEN savedPen = 0;
    HBRUSH savedBrush = 0;
    auto absPosToScreen = [startX, startY, &pivotPtCol, &pivotPtRow, patternMode, bBoxCol, bBoxRow] (int absX, int absY, int& screenX, int& screenY) {
        if (patternMode) {
            absX -= bBoxCol;
            absY -= bBoxRow;
        } else {
            absX -= pivotPtCol;
            absY -= pivotPtRow;
        }
        screenX = absCoordToScreen (absX /*- bBoxCol*/) + startX;
        screenY = absCoordToScreen (absY /*- bBoxRow*/) + startY;
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
    auto transformXY = [&pivotPtX, &pivotPtY, rotAngleDeg] (int& x, int& y) {
        if (x != pivotPtX || y != pivotPtY) {
            double dx = (double) (x - pivotPtX);
            double dy = (double) (pivotPtY - y);
            double brg = atan (fabs (dx / dy));
            if (dx < 0) {
                if (dy < 0) {
                    brg += PI;
                } else {
                    brg = PI + PI - brg;
                }
            } else if (dy < 0) {
                brg = PI - brg;
            }
            double radius = sqrt (dx * dx + dy * dy);
            brg += rotAngleDeg * RAD_IN_DEG;
            x = pivotPtX + radius * sin (brg);
            y = pivotPtY - radius * cos (brg);
        }
    };
    auto transformPoint = [transformXY] (POINT& pt) {
        int x = pt.x;
        int y = pt.y;
        transformXY (x, y);
        pt.x = x;
        pt.y = y;
    };
    absPosToScreen (pivotPtCol, pivotPtRow, pivotPtX, pivotPtY);
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
                if (rotAngleDeg != 0.0) transformXY (curX, curY);
                MoveToEx (paintDC, curX, curY, 0);
                break;
            }
            case DrawOperCode::PEN_DOWN: {
                if (!polygonMode) selectPen ();
                for (size_t i = 0; i < instr.args.size (); i += 2) {
                    absPosToScreen (instr.args [i], instr.args [i+1], curX, curY);
                    if (rotAngleDeg != 0.0) transformXY (curX, curY);
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
    SymbolDraw& symbolDraw,
    PaletteIndex paletteIndex
) {
    int symbolX, symbolY;
    auto& symbol = dai.symbols [symbolDraw.symbolIndex];
    int westX, northY;
    geoToXY (north, west, zoom, westX, northY);
    geoToXY (node.points.front ().lat, node.points.front ().lon, zoom, symbolX, symbolY);
    symbolX -= westX;
    symbolY -= northY;
    if (symbolX >= 0 && symbolX <= client.right && symbolY >= 0 && symbolY <= client.bottom) {
        completeDrawProc (paintDC, symbol.drawProc, symbolX, symbolY, paletteIndex, dai, symbol.pivotPtCol, symbol.pivotPtRow, symbol.bBoxCol, symbol.bBoxRow, symbolDraw.rotAngle);
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

    auto getTableSet = [pointObjTableSet, spatialObjTableSet] (FeatureObject& feature) {
        switch (feature.primitive) {
            case 1: case 4: return pointObjTableSet;
            case 2: return TableSet::LINES;
            case 3: return spatialObjTableSet;
            default: return TableSet::UNKNOWN_DATASET;
        }
    };

    for (auto& feature: features) {
        TableSet tableSet = getTableSet (feature);
        auto lookupTable = dai.findLookupTable (feature.classCode, displayCat, tableSet, objectTypes [feature.primitive-1]);
if(feature.fidn==29143526){
int iii=0;
++iii;
--iii;
}
if(!lookupTable){
int iii=0;
++iii;
--iii;
}
        lookupTables.emplace_back (lookupTable);
    }

    DrawQueue drawQueue (client, paintDC, paletteIndex, dai, north, west, zoom);

    for (int prty = 1; prty < 10; ++ prty) {
        drawQueue.clear ();
        
        for (size_t i = 0; i < features.size (); ++ i) {
            auto& feature = features [i];
            //if (feature.classCode >= 300 && feature.classCode <= 402) continue;

            if (feature.primitive != 2 && feature.primitive != 3) continue;
            if (!lookupTables [i]) continue;

            auto lookupTableItem = feature.findBestItem (displayCat, getTableSet (feature), dai, prty);
            if (!lookupTableItem) continue;
if(feature.fidn==29142158){
int iii=0;
++iii;
--iii;
}
            if (lookupTableItem->procIndex != LookupTableItem::NOT_EXIST) {
                dai.runCSP (lookupTableItem, & feature, nodes, edges, features, zoom, drawQueue);
            }

            if (feature.primitive == 3) {
                HBRUSH brush = 0;
                HPEN pen = 0;
                //bool patternMode = false;
                if (lookupTableItem->brushIndex != LookupTableItem::NOT_EXIST) {
                    auto& [brushExists, solidBrush] = dai.palette.brushes [lookupTableItem->brushIndex].get (paletteIndex);
                    if (brushExists) {
                        paintArea (client, paintDC, nodes, edges, feature.edgeRefs, dai, north, west, zoom, solidBrush, 0);
                    }
                } else if (lookupTableItem->patternBrushIndex != LookupTableItem::NOT_EXIST) {
                    //auto& [brushExists, patternBrush] = dai.palette.patternBrushes [lookupTableItem->patternBrushIndex].get (paletteIndex);
                    //if (brushExists) brush = patternBrush;
                    //patternMode = true;
                    auto& [toolExists, tool] = patternTools [lookupTableItem->patternBrushIndex].get (paletteIndex);
                    if (toolExists) {
                        paintArea (client, paintDC, nodes, edges, feature.edgeRefs, dai, north, west, zoom, 0, & tool);
                    }
                }
            }

            if ((feature.primitive == 2 || feature.primitive == 3) && lookupTableItem->edgePenIndex != LookupTableItem::NOT_EXIST) {
                HPEN pen = 0;
                if (lookupTableItem->penIndex != LookupTableItem::NOT_EXIST) {
                    auto& [penExists, penHandle] = dai.palette.pens [lookupTableItem->penIndex].get (paletteIndex);
                    pen = penExists ? penHandle : 0;
                } else {
                    pen = (HPEN) GetStockObject (BLACK_PEN);
                }
                drawQueue.addEdgeChain (lookupTableItem->edgePenIndex, lookupTableItem->edgePenStyle, lookupTableItem->edgePenWidth, nodes);
                for (auto& edgeRef: feature.edgeRefs) {
                    if (edgeRef.hidden) continue;
                    //paintEdge (client, paintDC, nodes, edges [edgeRef.index], dai, north, west, zoom, pen);
                    drawQueue.addEdge (edges [edgeRef.index]);
                }
            }

            delete lookupTableItem;
        }

        drawQueue.run ();
    }

    for (size_t i = 0; i < features.size (); ++ i) {
        auto& feature = features [i];
        if (feature.primitive != 1) continue;
        if (!lookupTables [i]) continue;
        auto lookupTableItem = feature.findBestItem (displayCat, pointObjTableSet, dai);
        if (!lookupTableItem) continue;

        drawQueue.clear ();

        if (lookupTableItem->procIndex != LookupTableItem::NOT_EXIST) {
            dai.runCSP (lookupTableItem, & feature, nodes, edges, features, zoom, drawQueue);
        }

        int offset = 0;
        for (auto& symbolDraw: lookupTableItem->symbols) {
            paintSymbol (client, paintDC, nodes [feature.nodeIndex], dai, north, west, zoom, offset, symbolDraw, paletteIndex);
        }

        /*if (lookupTableItem->drawArc) {
            auto& node = nodes [feature.nodeIndex];
            paintCompoundArc (client, paintDC, lookupTableItem->arcDef, dai, north, west, zoom, node.points [0].lat, node.points [0].lon, paletteIndex);
        }*/

        for (auto& line: lookupTableItem->lines) {
            paintLine (client, paintDC, dai, north, west, zoom, line, paletteIndex);
        }

        drawQueue.run ();

        delete lookupTableItem;
    }
}

HBRUSH createPatternBrush (PatternDesc& pattern, PaletteIndex paletteIndex, Dai& dai) {
    HDC dc = GetDC (HWND_DESKTOP);
    int baseWidth = absCoordToScreen (pattern.bBoxWidth + pattern.minDistance);//absCoordToScreen (pattern.bBoxWidth + pattern.bBoxCol) + 2;
    int baseHeight = absCoordToScreen (pattern.bBoxHeight + pattern.minDistance);//absCoordToScreen (pattern.bBoxHeight + pattern.bBoxRow) + 2;
    int minOffset = absCoordToScreen (pattern.minDistance);
    int fullWidth = baseWidth * 3;
    int fullHeight = baseHeight * 2;
    HBITMAP bmp = CreateBitmap (fullWidth, fullHeight, 1, 1, 0);
    HDC tempDC = CreateCompatibleDC (dc);
    RECT brushRect;

    brushRect.left = 0;
    brushRect.top = 0;
    brushRect.bottom = fullHeight;
    brushRect.right = fullWidth;
 
    SelectObject (tempDC, bmp);
    FillRect (tempDC, & brushRect, (HBRUSH) GetStockObject (WHITE_BRUSH));

    std::vector<POINT> drawPos;

    for (int i = 0; i < 2; ++ i) {
        int vertOffset = i * baseHeight;
        if (pattern.fillType == FillType::LINEAR || i != 1) {
            for (int j = 0; j < 3; ++ j) {
                auto& pos = drawPos.emplace_back ();
                pos.x = j * baseWidth;
                pos.y = vertOffset;
            }
        } else {
            for (int j = 0; j < 2; ++ j) {
                auto& pos = drawPos.emplace_back ();
                pos.x = baseWidth >> 1;
                if (j > 0) pos.x += baseWidth;
                pos.y = vertOffset;
            }
        }
    }

    for (auto& pt: drawPos) {
        completeDrawProc (
            tempDC,
            pattern.drawProc,
            pt.x,
            pt.y,
            paletteIndex,
            dai,
            pattern.pivotPtCol,
            pattern.pivotPtRow,
            pattern.bBoxCol,
            pattern.bBoxRow,
            0.0,
            true
        );
    }
    SelectObject (tempDC, (HBITMAP) 0);
    ReleaseDC (HWND_DESKTOP, dc);
    DeleteDC (tempDC);

    HBRUSH brush = CreatePatternBrush (bmp);
    
    DeleteObject (bmp);

    return brush;
}

void paintArc (
    RECT& client,
    HDC paintDC,
    size_t penIndex,
    Dai& dai,
    double north,
    double west,
    uint8_t zoom,
    double centerLat,
    double centerLon,
    double radiusMm,
    double start,
    double end,
    PaletteIndex paletteIndex
) {
    int centerX, centerY, westX, northY;
    double radius = radiusMm / PIXEL_SIZE_IN_MM;

    auto [penExists, pen] = dai.palette.pens [penIndex].get (paletteIndex);

    geoToXY (north, west, zoom, westX, northY);
    geoToXY (centerLat, centerLon, zoom, centerX, centerY);
    
    centerX -= westX;
    centerY -= northY;

    std::vector<POINT> curve;
    bool inScreen = false;

    auto addSector = [centerX, centerY, radius, &client, &inScreen, &curve] (double brg) {
        double brgRad = (brg + 180.0) * RAD_IN_DEG;
        double x = centerX + radius * sin (brgRad);
        double y = centerY - radius * cos (brgRad);

        if (!inScreen && x >= 0 && x <= client.right && y >= 0 && y <= client.bottom) inScreen = true;

        curve.emplace_back ();
        curve.back ().x = (int) x;
        curve.back ().y = (int) y;
    };

    double brg;
    for (brg = start; brg <= end; brg += 5) {
        addSector (brg);
    }
    if (brg < end) addSector (end);

    if (inScreen) {
        HPEN lastPen = (HPEN) SelectObject (paintDC, pen);
        Polyline (paintDC, curve.data (), curve.size ());
        SelectObject (paintDC, lastPen);
    }
}

void paintLine (RECT& client, HDC paintDC, Dai& dai, double north, double west, uint8_t zoom, LineDraw& line, PaletteIndex paletteIndex) {
    if (line.mode == LineDrawMode::ARC) {
        paintArc (client, paintDC, line.penIndex, dai, north, west, zoom, line.lat1, line.lon1, line.lengthMm, line.brg, line.endBrg, paletteIndex);
    } else {
        int x1, y1, x2, y2, westX, northY;

        geoToXY (north, west, zoom, westX, northY);
        geoToXY (line.lat1, line.lon1, zoom, x1, y1);

        x1 -= westX;
        y1 -= northY;

        if (line.mode == LineDrawMode::BETWEEN_TWO_POINTS) {
            geoToXY (line.lat1, line.lon1, zoom, x2, y2);

            x2 -= westX;
            y2 -= northY;
        } else {
            double brgRad = line.brg * RAD_IN_DEG;
            x2 = x1 + (int) (line.lengthMm / PIXEL_SIZE_IN_MM * sin (brgRad));
            y2 = y1 - (int) (line.lengthMm / PIXEL_SIZE_IN_MM * cos (brgRad));
        }

        auto [penExists, pen] = dai.palette.pens [line.penIndex].get (paletteIndex);

        HPEN lastPen = (HPEN) SelectObject (paintDC, pen);
        MoveToEx (paintDC, x1, y1, 0);
        LineTo (paintDC, x2, y2);
        SelectObject (paintDC, lastPen);
    }
}

void paintCompoundArc (RECT& client, HDC paintDC, ArcDef& arcDef, Dai& dai, double north, double west, uint8_t zoom, double lat, double lon, PaletteIndex paletteIndex) {
    static size_t outlineColorIndex = LookupTableItem::NOT_EXIST;

    if (outlineColorIndex == LookupTableItem::NOT_EXIST) {
        outlineColorIndex = dai.colorTable.getColorIndex ("OUTLW");
    }

    int centerX, centerY, westX, northY;

    auto [outlinePenExists, outlinePens] = dai.palette.basePens [outlineColorIndex].get (paletteIndex);

    geoToXY (north, west, zoom, westX, northY);
    geoToXY (lat, lon, zoom, centerX, centerY);
    
    centerX -= westX;
    centerY -= northY;

    auto& [penExists, pens] = dai.palette.basePens [arcDef.internalColor].get (paletteIndex);
    
    if ((centerX + (int) arcDef.radius) < 0 || (centerY + (int) arcDef.radius) < 0) return;
    if ((centerX - (int) arcDef.radius) > client.right || (centerY - (int) arcDef.radius) > client.bottom) return;

    auto drawArc = [&paintDC, centerX, centerY] (HPEN pen, int radius, double start, double end) {
        int startX = centerX + (int) (radius * sin (start * RAD_IN_DEG));
        int startY = centerY - (int) (radius * cos (start * RAD_IN_DEG));

        SelectObject (paintDC, pen);
        MoveToEx (paintDC, startX, startY, 0);
        AngleArc (paintDC, centerX, centerY, (DWORD) radius, (float) start + 90.0f, (float) end);
    };

    drawArc (outlinePens [1], arcDef.radius + 1, arcDef.start, arcDef.end);
    drawArc (pens [3], arcDef.radius + 4, arcDef.start, arcDef.end);
    drawArc (outlinePens [1], arcDef.radius + 6, arcDef.start, arcDef.end);
}

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
){
    auto pen = getBasePen (colorIndex, width, paletteIndex, palette);

    if (pen) {
        PenTool::PolyPolygon polyPolygon;
        PenTool tool;

        tool.composeLine (style, lat, lon, brg, lengthInMm, north, west, zoom, polyPolygon, true);

        HPEN lastPen = (HPEN) SelectObject (paintDC, pen);

        if (style == PS_SOLID) {
            Polyline (paintDC, polyPolygon.front ().data (), polyPolygon.front ().size ());
        } else {
            std::vector<POINT> vertices;
            std::vector<DWORD> sizes;

            tool.removeOutOfScreenContours (client, polyPolygon);

            if (polyPolygon.size () > 0) {
                tool.translatePolyPolygon<DWORD> (polyPolygon, vertices, sizes);

                PolyPolyline (paintDC, vertices.data (), sizes.data (), sizes.size ());
            }
        }
        SelectObject (paintDC, lastPen);
    }
}

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
) {
    auto pen = getBasePen (colorIndex, width, paletteIndex, palette);

    if (pen) {
        PenTool::PolyPolygon polyPolygon;
        PenTool tool;

        tool.composeArc (style, centerLat, centerLon, start, end, radiusInMm, north, west, zoom, polyPolygon);

        HPEN lastPen = (HPEN) SelectObject (paintDC, pen);

        tool.removeOutOfScreenContours (client, polyPolygon);

        if (polyPolygon.size () > 0) {
            if (style == PS_SOLID) {
                Polyline (paintDC, polyPolygon.front ().data (), polyPolygon.front ().size ());
            } else {
                std::vector<POINT> vertices;
                std::vector<DWORD> sizes;

                tool.translatePolyPolygon<DWORD> (polyPolygon, vertices, sizes);

                PolyPolyline (paintDC, vertices.data (), sizes.data (), sizes.size ());
            }
        }

        SelectObject (paintDC, lastPen);
    }
}

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
){
    auto pen = getBasePen (colorIndex, width, paletteIndex, palette);

    if (pen) {
        PenTool::PolyPolygon polyPolygon;
        PenTool tool;

        for (auto& contour: contours) {
            polyPolygon.emplace_back ();

            if (style == PS_SOLID) {
                for (auto& vertex: contour) {
                    tool.appendToLastLeg (vertex.lat, vertex.lon, north, west, zoom, polyPolygon);
                }
            } else {
                for (size_t i = 1; i < contour.size (); ++ i) {
                    auto& origin = contour [i-1];
                    auto& dest = contour [i];

                    tool.composeLeg (style, origin.lat, origin.lon, dest.lat, dest.lon, north, west, zoom, polyPolygon, true);
                }
            }
        }

        HPEN lastPen = (HPEN) SelectObject (paintDC, pen);

        //tool.removeOutOfScreenContours (client, polyPolygon);

        if (polyPolygon.size () > 0) {
            std::vector<POINT> vertices;
            std::vector<DWORD> sizes;

            tool.translatePolyPolygon<DWORD> (polyPolygon, vertices, sizes);

            PolyPolyline (paintDC, vertices.data (), sizes.data (), sizes.size ());
        }

        SelectObject (paintDC, lastPen);
    }
}
