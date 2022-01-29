
#include "painter.h"
#include "geo.h"
#include "abstract_tools.h"
#include "drawers.h"

HBRUSH createPatternBrush (PatternDesc& pattern, PaletteIndex paletteIndex, Dai& dai);
void paintLine (RECT& client, HDC paintDC, Dai& dai, View& view, LineDraw& line, PaletteIndex paletteIndex);

std::vector<DrawToolItem <PatternTool>> patternTools;

bool isPolyPolylineOverlappingScreen (std::vector<POINT>& polyPolyline, RECT& client) {
    for (auto& pt: polyPolyline) {
        if (pt.x >= 0 & pt.x <= client.right && pt.y >= 0 && pt.y <= client.bottom) return true;
    }
    return false;
}

HPEN getBasePen (size_t colorIndex, int width, PaletteIndex paletteIndex, Palette& palette) {
    if (width > 0 && width <= 6) {
        auto [penExists, pens] = palette.basePens [colorIndex].get (paletteIndex);

        return penExists ? pens [width-1] : 0;
    } else {
        return 0;
    }
}

HPEN getDashedPen (size_t colorIndex, int width, PaletteIndex paletteIndex, Palette& palette) {
    if (width > 0 && width <= 6) {
        auto [penExists, pens] = palette.dashedPens [colorIndex].get (paletteIndex);

        return penExists ? pens [width-1] : 0;
    } else {
        return 0;
    }
}

HPEN getDottedPen (size_t colorIndex, int width, PaletteIndex paletteIndex, Palette& palette) {
    if (width > 0 && width <= 6) {
        auto [penExists, pens] = palette.dottedPens [colorIndex].get (paletteIndex);

        return penExists ? pens [width-1] : 0;
    } else {
        return 0;
    }
}

HPEN getGenericPen (int style, size_t colorIndex, int width, PaletteIndex paletteIndex, Palette& palette) {
    switch (style) {
        case PS_SOLID: return getBasePen (colorIndex, width, paletteIndex, palette);
        case PS_DASH: return getDashedPen (colorIndex, width, paletteIndex, palette);
        case PS_DOT: return getDottedPen (colorIndex, width, paletteIndex, palette);
        default: return 0;
    }
}

HBRUSH getFillBrush (size_t colorIndex, PaletteIndex paletteIndex, Palette& palette) {
    auto [brushExists, brush] = palette.brushes [colorIndex].get (paletteIndex);

    return brushExists ? brush : 0;
}

HBRUSH getPatternBrush (size_t patternIndex, PaletteIndex paletteIndex, Palette& palette) {
    auto [brushExists, brush] = palette.patternBrushes [patternIndex].get (paletteIndex);

    return brushExists ? brush : 0;
}

PatternTool *getPatternTool (size_t patternBrushIndex, PaletteIndex paletteIndex, Palette& palette) {
    auto& [toolExists, tool] = patternTools [patternBrushIndex].get (paletteIndex);

    return toolExists ? & tool : 0;
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
    View& view,
    HBRUSH brush,
    //bool patternMode
    PatternTool *patternTool
) {
    std::vector<POINT> vertices;
    std::vector<std::vector<POINT>> polyPolygon;

    int westX, northY;
    geoToXY (view.north, view.west, view.zoom, westX, northY);

    auto addVertex = [&vertices, &view, westX, northY, &polyPolygon] (double lat, double lon) {
        int x, y;
        geoToXY (lat, lon, view.zoom, x, y);
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
    auto isContourClosed = [&polyPolygon] () {
        auto& lastContour = polyPolygon.back ();
        if (lastContour.size () > 1) {
            return lastContour.front ().x == lastContour.back ().x && lastContour.front ().y == lastContour.back ().y;
        } else {
            return false;
        }
    };
    for (auto& edgeRef: edgeRefs) {
        auto& edge = edges [edgeRef.index];
        if (edge.hidden) continue;
        if (edgeRef.hole != hole) {
            //closeContour (polyPolygon.back ());
            //if (hole) reverseContour (polyPolygon.back ());
            hole = edgeRef.hole;
            polyPolygon.emplace_back ();
        } else if (hole && isContourClosed ()) {
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
    //closeContour (polyPolygon.back ());
    //if (hole) reverseContour (polyPolygon.back ());

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
        if (isPolyPolylineOverlappingScreen (area, client)) {
            int lastMode = SetPolyFillMode (paintDC, ALTERNATE);
            PolyPolygon (paintDC, area.data (), sizes.data (), (int) sizes.size ());
            SetPolyFillMode (paintDC, lastMode);
        }
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
    View& view,
    int& offset,
    SymbolDraw& symbolDraw,
    PaletteIndex paletteIndex
) {
    int symbolX, symbolY;
    auto& symbol = dai.symbols [symbolDraw.symbolIndex];
    int westX, northY;
    geoToXY (view.north, view.west, view.zoom, westX, northY);

    for (size_t i = 0; i < node.points.size (); ++ i) {
        geoToXY (node.points [i].lat, node.points [i].lon, view.zoom, symbolX, symbolY);
        symbolX -= westX;
        symbolY -= northY;
        if (symbolX >= 0 && symbolX <= client.right && symbolY >= 0 && symbolY <= client.bottom) {
            completeDrawProc (paintDC, symbol.drawProc, symbolX, symbolY, paletteIndex, dai, symbol.pivotPtCol, symbol.pivotPtRow, symbol.bBoxCol, symbol.bBoxRow, symbolDraw.rotAngle);
        }
    }
}

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
) {
    int symbolX, symbolY;
    auto& symbol = dai.symbols [symbolIndex];
    int westX, northY;
    geoToXY (view.north, view.west, view.zoom, westX, northY);
    geoToXY (lat, lon, view.zoom, symbolX, symbolY);
    symbolX -= westX;
    symbolY -= northY;
    if (symbolX >= 0 && symbolX <= client.right && symbolY >= 0 && symbolY <= client.bottom) {
        completeDrawProc (paintDC, symbol.drawProc, symbolX, symbolY, paletteIndex, dai, symbol.pivotPtCol, symbol.pivotPtRow, symbol.bBoxCol, symbol.bBoxRow, rotAngle);
    }
}

void paintSymbol (
    RECT& client,
    HDC paintDC,
    int x,
    int y,
    size_t symbolIndex,
    double rotAngle,
    Dai& dai,
    PaletteIndex paletteIndex
) {
    auto& symbol = dai.symbols [symbolIndex];
    if (x >= 0 && x <= client.right && y >= 0 && y <= client.bottom) {
        completeDrawProc (paintDC, symbol.drawProc, x, y, paletteIndex, dai, symbol.pivotPtCol, symbol.pivotPtRow, symbol.bBoxCol, symbol.bBoxRow, rotAngle);
    }
}

void paintEdge (
    RECT& client,
    HDC paintDC,
    Nodes& nodes,
    GeoEdge& edge,
    Dai& dai,
    View& view,
    HPEN pen
) {
    std::vector<POINT> vertices;

    int westX, northY;
    geoToXY (view.north, view.west, view.zoom, westX, northY);

    auto addVertex = [&vertices, &view, westX, northY] (double lat, double lon) {
        int x, y;
        geoToXY (lat, lon, view.zoom, x, y);
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

void addAllTextDraws (FeatureObject& feature, LookupTableItem *lookupTableItem, DrawQueue& drawQueue, Chart& chart) {
    for (auto& desc: lookupTableItem->textDescriptions) {
        double lat, lon;
        getCenterPos (feature, chart, lat, lon);
        drawQueue.addText (lat, lon, desc, & feature);
    }
}

void paintChart (
    RECT& client,
    HDC paintDC,
    Chart& chart,
    Environment& environment,
    View& view,
    PaletteIndex paletteIndex,
    DisplayCat displayCat,
    TableSet spatialObjTableSet,
    TableSet pointObjTableSet
) {
    static char *objectTypes { "PLA" };
    std::vector<LookupTable *> lookupTables;
    std::vector<EdgeRef> delayedEdges [10];
    Nodes& nodes = chart.nodes;
    Edges& edges = chart.edges;
    Features& features = chart.features;
    Dai& dai = environment.dai;
    AttrDictionary& attrDic = environment.attrDictionary;

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
        lookupTables.emplace_back (lookupTable);
    }

    DrawQueue drawQueue (client, paintDC, paletteIndex, dai, attrDic, view);
    DrawQueue textDrawQueue (client, paintDC, paletteIndex, dai, attrDic, view);

    for (int prty = 1; prty < 10; ++ prty) {
        drawQueue.clear ();
        
        for (size_t i = 0; i < features.size (); ++ i) {
            auto& feature = features [i];
            if (feature.primitive != 2 && feature.primitive != 3) continue;
            if (!lookupTables [i]) continue;

            auto lookupTableItem = feature.findBestItem (displayCat, getTableSet (feature), dai, prty);
            if (!lookupTableItem) continue;

            if (lookupTableItem->procIndex != LookupTableItem::NOT_EXIST) {
                environment.runCSP (lookupTableItem, & feature, chart, view, drawQueue);
            }

            if (feature.primitive == 3) {
                drawQueue.addArea (lookupTableItem->brushIndex, lookupTableItem->patternBrushIndex, chart);
                for (auto& edgeRef: feature.edgeRefs) {
                    if (edgeRef.hidden) continue;
                    drawQueue.addEdge (edgeRef);
                }
            }

            if ((feature.primitive == 2 || feature.primitive == 3) && lookupTableItem->edgePenIndex != LookupTableItem::NOT_EXIST) {
                if (lookupTableItem->customEdgePres) {
                    for (auto& edgeRef: feature.edgeRefs) {
                        if (edgeRef.hidden) continue;
                        if (edgeRef.displayPriority > prty) {
                            auto& ref = delayedEdges [edgeRef.displayPriority].emplace_back (edgeRef);
                            if (ref.penIndex == LookupTableItem::NOT_EXIST) ref.penIndex = lookupTableItem->edgePenIndex;
                            if (ref.secondPen && ref.secondPenIndex == LookupTableItem::NOT_EXIST) ref.secondPen = false;
                        } else {
                            drawQueue.addEdgeChain (
                                edgeRef.customPres ? edgeRef.penIndex : lookupTableItem->edgePenIndex,
                                edgeRef.customPres ? edgeRef.penStyle : lookupTableItem->edgePenStyle,
                                edgeRef.customPres ? edgeRef.penWidth : lookupTableItem->edgePenWidth,
                                chart
                            );
                            drawQueue.addEdge (edgeRef);
                            if (edgeRef.customPres && edgeRef.secondPen) {
                                drawQueue.addEdgeChain (edgeRef.secondPenIndex, edgeRef.secondPenStyle, edgeRef.secondPenWidth, chart);
                                drawQueue.addEdge (edgeRef);
                            }
                        }
                    }
                } else {
                    drawQueue.addEdgeChain (lookupTableItem->edgePenIndex, lookupTableItem->edgePenStyle, lookupTableItem->edgePenWidth, chart);
                    for (auto& edgeRef: feature.edgeRefs) {
                        if (edgeRef.hidden) continue;
                        drawQueue.addEdge (edgeRef);
                    }
                }
            }

            if (feature.primitive == 2 || feature.primitive == 3) {
                for (auto& edgeRef: feature.edgeRefs) {
                    if (edgeRef.hidden || edgeRef.displayPriority && edgeRef.displayPriority != prty) continue;
                    if (!edgeRef.symbols.empty ()) {
                        for (size_t symbolIndex: edgeRef.symbols) {
                            drawQueue.addCentralEdgeSymbol (chart, symbolIndex, edgeRef.index, dai);
                        }
                    }
                }
            }

            addAllTextDraws (feature, lookupTableItem, textDrawQueue, chart);

            delete lookupTableItem;
        }

        // Draw delayed edges
        for (auto& edgeRef: delayedEdges [prty]) {
            drawQueue.addEdgeChain (edgeRef.penIndex, edgeRef.penStyle, edgeRef.penWidth, chart );
            drawQueue.addEdge (edgeRef);
            if (edgeRef.customPres && edgeRef.secondPen) {
                drawQueue.addEdgeChain (edgeRef.secondPenIndex, edgeRef.secondPenStyle, edgeRef.secondPenWidth, chart);
                drawQueue.addEdge (edgeRef);
            }
            for (size_t symbolIndex: edgeRef.symbols) {
                drawQueue.addCentralEdgeSymbol (chart, symbolIndex, edgeRef.index, dai);
            }
        }
        
        drawQueue.run ();
    }

    for (size_t i = 0; i < features.size (); ++ i) {
        auto& feature = features [i];
        if (feature.primitive != 1 && feature.primitive != 4) continue;
        if (!lookupTables [i]) continue;
        auto lookupTableItem = feature.findBestItem (displayCat, pointObjTableSet, dai);
        if (!lookupTableItem) continue;

        drawQueue.clear ();

        if (lookupTableItem->procIndex != LookupTableItem::NOT_EXIST) {
            environment.runCSP (lookupTableItem, & feature, chart, view, drawQueue);
        }

        int offset = 0;

        for (auto& symbolDraw: lookupTableItem->symbols) {
            auto& pos = nodes [feature.nodeIndex].points.front ();
            drawQueue.addSymbol (pos.lat, pos.lon, symbolDraw.symbolIndex, symbolDraw.rotAngle, dai);
        }
        drawQueue.run ();

        addAllTextDraws (feature, lookupTableItem, textDrawQueue, chart);

        delete lookupTableItem;
    }

    textDrawQueue.run ();
}

void paintChartSmart (
    RECT& client,
    HDC paintDC,
    Chart& chart,
    Environment& environment,
    View& view,
    PaletteIndex paletteIndex,
    DisplayCat displayCat,
    TableSet spatialObjTableSet,
    TableSet pointObjTableSet
) {
    HDC tempDC = CreateCompatibleDC (paintDC);
    HBITMAP tempBmp = CreateCompatibleBitmap (paintDC, client.right + 1, client.bottom + 1);

    SelectObject (tempDC, tempBmp);
    FillRect (tempDC, & client, (HBRUSH) GetStockObject (WHITE_BRUSH));
    paintChart (client, tempDC, chart, environment, view, paletteIndex, displayCat, spatialObjTableSet, pointObjTableSet);
    BitBlt (paintDC, 0, 0, client.right + 1, client.bottom + 1, tempDC, 0, 0, SRCCOPY);
    DeleteObject (tempBmp);
    DeleteDC (tempDC);
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
    View& view,
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

    geoToXY (view.north, view.west, view.zoom, westX, northY);
    geoToXY (centerLat, centerLon, view.zoom, centerX, centerY);
    
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

void paintLine (RECT& client, HDC paintDC, Dai& dai, View& view, LineDraw& line, PaletteIndex paletteIndex) {
    if (line.mode == LineDrawMode::ARC) {
        paintArc (client, paintDC, line.penIndex, dai, view, line.lat1, line.lon1, line.lengthMm, line.brg, line.endBrg, paletteIndex);
    } else {
        int x1, y1, x2, y2, westX, northY;

        geoToXY (view.north, view.west, view.zoom, westX, northY);
        geoToXY (line.lat1, line.lon1, view.zoom, x1, y1);

        x1 -= westX;
        y1 -= northY;

        if (line.mode == LineDrawMode::BETWEEN_TWO_POINTS) {
            geoToXY (line.lat1, line.lon1, view.zoom, x2, y2);

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

void paintCompoundArc (RECT& client, HDC paintDC, ArcDef& arcDef, Dai& dai, View& view, double lat, double lon, PaletteIndex paletteIndex) {
    static size_t outlineColorIndex = LookupTableItem::NOT_EXIST;

    if (outlineColorIndex == LookupTableItem::NOT_EXIST) {
        outlineColorIndex = dai.colorTable.getColorIndex ("OUTLW");
    }

    int centerX, centerY, westX, northY;

    auto [outlinePenExists, outlinePens] = dai.palette.basePens [outlineColorIndex].get (paletteIndex);

    geoToXY (view.north, view.west, view.zoom, westX, northY);
    geoToXY (lat, lon, view.zoom, centerX, centerY);
    
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
    View& view,
    PaletteIndex paletteIndex,
    Palette& palette
){
    auto pen = getGenericPen (style, colorIndex, width, paletteIndex, palette);

    if (pen) {
        PenTool::PolyPolygon polyPolygon;
        PenTool tool;

        tool.composeLine (style, lat, lon, brg, lengthInMm, view, polyPolygon, true);

        HPEN lastPen = (HPEN) SelectObject (paintDC, pen);

        if (style == PS_SOLID) {
            Polyline (paintDC, polyPolygon.front ().data (), polyPolygon.front ().size ());
        } else {
            std::vector<POINT> vertices;
            std::vector<DWORD> sizes;

            //tool.removeOutOfScreenContours (client, polyPolygon);

            int oldBkMode = SetBkMode (paintDC, TRANSPARENT);
            if (polyPolygon.size () > 0) {
                tool.translatePolyPolygon<DWORD> (polyPolygon, vertices, sizes);

                PolyPolyline (paintDC, vertices.data (), sizes.data (), sizes.size ());
                SetBkMode (paintDC, oldBkMode);
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
    View& view,
    PaletteIndex paletteIndex,
    Palette& palette
) {
    auto pen = getGenericPen (style, colorIndex, width, paletteIndex, palette);

    if (pen) {
        PenTool::PolyPolygon polyPolygon;
        PenTool tool;

        tool.composeArc (style, centerLat, centerLon, start, end, radiusInMm, view, polyPolygon);

        HPEN lastPen = (HPEN) SelectObject (paintDC, pen);

        //tool.removeOutOfScreenContours (client, polyPolygon);

        if (polyPolygon.size () > 0) {
            if (style == PS_SOLID) {
                Polyline (paintDC, polyPolygon.front ().data (), polyPolygon.front ().size ());
            } else {
                std::vector<POINT> vertices;
                std::vector<DWORD> sizes;

                tool.translatePolyPolygon<DWORD> (polyPolygon, vertices, sizes);

                int lastBkMode = SetBkMode (paintDC, TRANSPARENT);
                PolyPolyline (paintDC, vertices.data (), sizes.data (), sizes.size ());
                SetBkMode (paintDC, lastBkMode);
            }
        }

        SelectObject (paintDC, lastPen);
    }
}

void paintPolyPolygon (
    RECT& client,
    HDC paintDC,
    size_t fillBrushIndex,
    size_t patternBrushIndex,
    Contours& contours,
    View& view,
    PaletteIndex paletteIndex,
    Palette& palette
){
    auto fillBrush = fillBrushIndex == LookupTableItem::NOT_EXIST ? 0 : getFillBrush (fillBrushIndex, paletteIndex, palette);
    auto patternTool = patternBrushIndex ==  LookupTableItem::NOT_EXIST ? 0 : getPatternTool (patternBrushIndex, paletteIndex, palette);

    if (fillBrush || patternTool) {
        PenTool::PolyPolygon polyPolygon;
        PenTool tool;

        for (auto& contour: contours) {
            polyPolygon.emplace_back ();

            for (auto& vertex: contour) {
                tool.appendToLastLeg (vertex.lat, vertex.lon, view, polyPolygon);
            }
        }

        //tool.removeOutOfScreenContours (client, polyPolygon);
        
        if (polyPolygon.size () > 0) {
            std::vector<POINT> vertices;
            std::vector<INT> sizes;
            tool.translatePolyPolygon<INT> (polyPolygon, vertices, sizes);

            if (isPolyPolylineOverlappingScreen (vertices, client)) {
                HPEN lastPen = (HPEN) SelectObject (paintDC, GetStockObject (NULL_BRUSH));

                if (fillBrush) {
                    HBRUSH lastBrush = (HBRUSH) SelectObject (paintDC, fillBrush);
                    PolyPolygon (paintDC, vertices.data (), sizes.data (), sizes.size ());
                    SelectObject (paintDC, lastBrush);
                }
                if (patternTool) {
                    patternTool->paint (paintDC, polyPolygon);
                }

                SelectObject (paintDC, lastPen);
            }
        }
    }
}

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
){
    auto pen = getGenericPen (style, colorIndex, width, paletteIndex, palette);

    if (pen) {
        PenTool::PolyPolygon polyPolygon;
        PenTool tool;

        for (auto& contour: contours) {
            polyPolygon.emplace_back ();

            for (auto& vertex: contour) {
                tool.appendToLastLeg (vertex.lat, vertex.lon, view, polyPolygon);
            }
            /*if (style != PS_SOLID) {
                PenTool::PolyPolygon temp;
                tool.stylize (style, polyPolygon, temp);
                polyPolygon.swap (temp);
            }*/
        }
        HPEN lastPen = (HPEN) SelectObject (paintDC, pen);

        //tool.removeOutOfScreenContours (client, polyPolygon);

        if (polyPolygon.size () > 0) {
            std::vector<POINT> vertices;
            std::vector<DWORD> sizes;

            tool.translatePolyPolygon<DWORD> (polyPolygon, vertices, sizes);

            if (isPolyPolylineOverlappingScreen (vertices, client)) {
                int lastBkMode = SetBkMode (paintDC, TRANSPARENT);
                PolyPolyline (paintDC, vertices.data (), sizes.data (), sizes.size ());
                SetBkMode (paintDC, lastBkMode);
            }
        }

        SelectObject (paintDC, lastPen);
    }
}

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
){
    int x, y, westX, northY;

    geoToXY (view.north, view.west, view.zoom, westX, northY);
    geoToXY (lat, lon, view.zoom, x, y);
    
    x += xOffset - westX;
    y += yOffset - northY;

    if (x > client.right || y > client.bottom) return;

    RECT textRect;
    textRect.left = x;
    textRect.top = y;

    DrawText (paintDC, text, -1, & textRect, format | DT_CALCRECT);

    if (textRect.right < 0 || textRect.top < 0) return;

    auto lastBkMode = SetBkMode (paintDC, TRANSPARENT);
    DrawText (paintDC, text, -1, & textRect, format);

    SetBkMode (paintDC, lastBkMode);

    auto colorDef = dai.colorTable.container [colorIndex].getColorDef (paletteIndex);
}

std::tuple<bool, int, int> getCenterPos (size_t edgeIndex, RECT& client, Chart& chart, View& view) {
    auto& edge = chart.edges [edgeIndex];
    std::vector<POINT> vertices;

    auto addPoint = [&vertices] (int x, int y) {
        vertices.emplace_back ();
        vertices.back ().x = x;
        vertices.back ().y = y;
    };
    auto isBetween = [] (int value, int a, int b) {
        return value >= min (a, b) && value <= max (a, b);
    };
    auto crossTwoSections = [&isBetween] (int x11, int y11, int x12, int y12, int x21, int y21, int x22, int y22) {
        double c1 = (double) (y12 - y11) / (double) (x12 - x11);
        double c2 = (double) (y22 - y21) / (double) (x22 - x21);
        int x = (int) (((double) y21 - (double) y11 + (double) x11 * c1 - (double) x21 * c2) / (double) (c1 - c2));
        int y = (int) ((double) y11 + (double) (x - x11) * c1);
        bool crossed = isBetween (x, x11, x12) && isBetween (x, x21, x22) && isBetween (y, y11, y12) && isBetween (y, y21, y22);
        return std::tuple<bool, int, int>(crossed, x, y);
    };
    auto intersectSectionAndScreen = [&client, &crossTwoSections, &isBetween] (int x1, int y1, int x2, int y2) {
        bool inside1 = isBetween (x1, 0, client.right) && isBetween (y1, 0, client.bottom);
        bool inside2 = isBetween (x2, 0, client.right) && isBetween (y2, 0, client.bottom);

        if (inside1 && inside2) {
            return std::tuple<bool, int, int, int, int> (true, x1, y1, x2, y2);
        }

        auto [cross1, cx1, cy1] = crossTwoSections (x1, y1, x2, y2, 0, 0, client.right, 0);
        auto [cross2, cx2, cy2] = crossTwoSections (x1, y1, x2, y2, client.right, 0, client.right, client.bottom);
        auto [cross3, cx3, cy3] = crossTwoSections (x1, y1, x2, y2, 0, client.bottom, client.right, client.bottom);
        auto [cross4, cx4, cy4] = crossTwoSections (x1, y1, x2, y2, 0, 0, 0, client.bottom);

        if (inside1) {
            if (cross1) return std::tuple<bool, int, int, int, int> (true, x1, y1, cx1, cy1);
            if (cross2) return std::tuple<bool, int, int, int, int> (true, x1, y1, cx2, cy2);
            if (cross3) return std::tuple<bool, int, int, int, int> (true, x1, y1, cx3, cy3);
            if (cross4) return std::tuple<bool, int, int, int, int> (true, x1, y1, cx4, cy4);
        }
        if (inside2) {
            if (cross1) return std::tuple<bool, int, int, int, int> (true, cx1, cy1, x2, y2);
            if (cross2) return std::tuple<bool, int, int, int, int> (true, cx2, cy2, x2, y2);
            if (cross3) return std::tuple<bool, int, int, int, int> (true, cx3, cy3, x2, y2);
            if (cross4) return std::tuple<bool, int, int, int, int> (true, cx4, cy4, x2, y2);
        }
        return std::tuple<bool, int, int, int, int> (false, 0, 0, 0, 0);
    };
    auto checkAddLeg = [&vertices, &view, &addPoint, &intersectSectionAndScreen] (double lat1, double lon1, double lat2, double lon2) {
        int x1, y1, x2, y2;

        PenTool::geo2screen (lat1, lon1, view, x1, y1);
        PenTool::geo2screen (lat2, lon2, view, x2, y2);

        auto [intersects, cx1, cy1, cx2, cy2] = intersectSectionAndScreen (x1, y1, x2, y2);

        if (intersects) {
            addPoint (cx1, cy1);
            addPoint (cx2, cy2);
        }
    };

    auto checkAddLegByTwoPos = [&checkAddLeg] (Position& pos1, Position& pos2) {
        checkAddLeg (pos1.lat, pos1.lon, pos2.lat, pos2.lon);
    };
    if (edge.internalNodes.empty ()) {
        checkAddLegByTwoPos (
            chart.nodes [edge.beginIndex].points.front (),
            chart.nodes [edge.endIndex].points.front ()
        );
    } else {
        checkAddLegByTwoPos (chart.nodes [edge.beginIndex].points.front (), edge.internalNodes.front ());
        for (size_t i = 1, j = edge.internalNodes.size () - 1; i < j; ++ i) {
            checkAddLegByTwoPos (edge.internalNodes [i-1], edge.internalNodes [i]);
        }
        checkAddLegByTwoPos (edge.internalNodes.back (), chart.nodes [edge.endIndex].points.front ());
    }
    if (vertices.empty ()) return std::tuple<bool, int, int> (false, 0, 0);

    // Walk along the contour and calculate the length
    double len = 0;
    for (size_t i = 1, j = vertices.size (); i < j; ++ i) {
        auto& pt1 = vertices [i-1];
        auto& pt2 = vertices [i];

        if (pt1.x != pt2.x || pt1.y != pt2.y) {
            int dx = pt2.x - pt1.x;
            int dy = pt2.y - pt1.y;
            len += sqrt (dx * dx + dy * dy);
        }
    }

    len *= 0.5;

    // Walk along again until we found a middle point
    double passed = 0;
    for (size_t i = 1, j = vertices.size (); i < j; ++ i) {
        auto& pt1 = vertices [i-1];
        auto& pt2 = vertices [i];

        if (pt1.x != pt2.x || pt1.y != pt2.y) {
            int dx = pt2.x - pt1.x;
            int dy = pt2.y - pt1.y;
            double legLen = sqrt (dx * dx + dy * dy);

            if ((passed + legLen) >= len) {
                double coef = (len - passed) / legLen;
                int x = pt1.x + (int) (coef * (double) dx);
                int y = pt1.y + (int) (coef * (double) dy);

                return std::tuple<bool, int, int> (true, x, y);
            } else {
                passed += legLen;
            }
        }
    }
    return std::tuple<bool, int, int> (false, 0, 0);
}

