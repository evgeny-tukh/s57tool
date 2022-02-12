#pragma once

#include <vector>
#include <string>
#include <Windows.h>
#include "abstract_tools.h"
#include "s57defs.h"

struct Drawer {
    size_t penIndex;
    View& view;
    double lat, lon;
    double rangeMm;
    int penStyle, penWidth;
    static PenTool penTool;
    
    virtual bool isSymbol () { return false; }

    Drawer (
        size_t _penIndex,
        int _penStyle,
        int _penWidth,
        View& _view,
        double _lat,
        double _lon,
        double _rangeMm
    ): penIndex (_penIndex), penStyle (_penStyle), penWidth (_penWidth), view (_view), lat (_lat), lon (_lon), rangeMm (_rangeMm) {}

    virtual void run (RECT& client, HDC paintDC, PaletteIndex paletteIndex, Palette& palette) {}
};

struct SymbolDrawer: Drawer {
    size_t symbolIndex;
    double rotAngle;
    Dai& dai;

    virtual bool isSymbol () { return true; }

    SymbolDrawer (
        View& _view,
        double _lat,
        double _lon,
        size_t _symbolIndex,
        double _rotAngle,
        Dai& _dai
    ): Drawer (0, 0, 0, _view, _lat, _lon, 0.0), symbolIndex (_symbolIndex), dai (_dai), rotAngle (_rotAngle) {}

    virtual void run (RECT& client, HDC paintDC, PaletteIndex paletteIndex, Palette& palette);
};

struct CentralEdgeSymbolDrawer: Drawer {
    size_t symbolIndex;
    size_t edgeIndex;
    Dai& dai;
    Chart& chart;

    virtual bool isSymbol () { return true; }

    CentralEdgeSymbolDrawer (
        Chart& _chart,
        View& _view,
        size_t _symbolIndex,
        size_t _edgeIndex,
        Dai& _dai
    ): Drawer (0, 0, 0, _view, 0.0, 0.0, 0.0), edgeIndex (_edgeIndex), symbolIndex (_symbolIndex), dai (_dai), chart (_chart) {}

    virtual void run (RECT& client, HDC paintDC, PaletteIndex paletteIndex, Palette& palette);
};

struct TextDrawer: Drawer {
    std::string text;
    TextDesc desc;
    Dai& dai;
    AttrDictionary& dic;

    TextDrawer (
        View& _view,
        double _lat,
        double _lon,
        TextDesc& _desc,
        FeatureObject *_object,
        Dai& _dai,
        AttrDictionary& _dic
    );

    virtual void run (RECT& client, HDC paintDC, PaletteIndex paletteIndex, Palette& palette);
};

struct LineDrawer: Drawer {
    double brg;

    LineDrawer (
        size_t _penIndex,
        int _penStyle,
        int _penWidth,
        View& _view,
        double _lat,
        double _lon,
        double _brg,
        double _rangeMm
    ): Drawer (_penIndex, _penStyle, _penWidth, _view, _lat, _lon, _rangeMm), brg (_brg) {}

    virtual void run (RECT& client, HDC paintDC, PaletteIndex paletteIndex, Palette& palette);
};

struct ArcDrawer: Drawer {
    double start, end;

    ArcDrawer (
        size_t _penIndex,
        int _penStyle,
        int _penWidth,
        View& _view,
        double _centerLat,
        double _centerLon,
        double _radiusMm,
        double _start,
        double _end
    ): Drawer (_penIndex, _penStyle, _penWidth, _view, _centerLat, _centerLon, _radiusMm), start (_start), end (_end) {}

    virtual void run (RECT& client, HDC paintDC, PaletteIndex paletteIndex, Palette& palette);
};

struct PolyPolylineDrawer: Drawer {
    Contours polyPolyline;
    Chart& chart;

    PolyPolylineDrawer (
        size_t _penIndex,
        int _penStyle,
        int _penWidth,
        View& _view,
        Chart& _chart
    ): Drawer (_penIndex, _penStyle, _penWidth, _view, 0.0, 0.0, 0.0), chart (_chart) {
    }

    void addContour () {
        polyPolyline.emplace_back ();
    }
    void addVertex (double lat, double lon) {
        polyPolyline.back ().emplace_back (lat, lon);
    }
    void addNode (size_t nodeIndex);

    virtual void run (RECT& client, HDC paintDC, PaletteIndex paletteIndex, Palette& palette);
    virtual void addEdge (struct EdgeRef& edgeRef);
};

struct PolyPolygonDrawer: PolyPolylineDrawer {
    size_t fillBrushIndex;
    size_t patternBrushIndex;
    bool hole;

    PolyPolygonDrawer (
        size_t _fillBrushIndex,
        size_t _patternBrushIndex,
        View& _view,
        Chart& _chart
    ): PolyPolylineDrawer (-1, PS_SOLID, 0, _view, _chart), fillBrushIndex (_fillBrushIndex), patternBrushIndex (_patternBrushIndex), hole (false) {
    }

    virtual void run (RECT& client, HDC paintDC, PaletteIndex paletteIndex, Palette& palette);
    virtual void addEdge (struct EdgeRef& edgeRef);

    bool isLastContourClosed () {
        auto& lastContour = polyPolyline.back ();
        if (lastContour.size () > 1) {
            return lastContour.front ().lat == lastContour.back ().lat && lastContour.front ().lon == lastContour.back ().lon;
        } else {
            return false;
        }
    }
};

struct DrawQueue {
    std::vector<Drawer *> container;
    HDC paintDC;
    View& view;
    PaletteIndex paletteIndex;
    Dai& dai;
    RECT& client;
    AttrDictionary& attrDic;

    DrawQueue (
        RECT& _client, 
        HDC _paintDC,
        PaletteIndex _paletteIndex,
        Dai& _dai,
        AttrDictionary& _attrDic,
        View& _view
    ): paintDC (_paintDC), paletteIndex (_paletteIndex), dai (_dai), view (_view), client (_client), attrDic (_attrDic) {}

    virtual ~DrawQueue () {
        clear ();
    }
    void clear () {
        for (auto drawer: container) delete drawer;
        container.clear ();
    }
    void run () {
        for (auto drawer: container) {
            //POINT prevPos;
            //MoveToEx (paintDC, 0, 0, & prevPos);
            //MoveToEx (paintDC, prevPos.x, prevPos.y, 0);
            drawer->run (client, paintDC, paletteIndex, dai.palette);
            //MoveToEx (paintDC, prevPos.x, prevPos.y, 0);
        }
    }
    void addLine (int penIndex, int penStyle, int penWidth, double lat, double lon, double brg, double rangeMm) {
        container.push_back (new LineDrawer (penIndex, penStyle, penWidth, view, lat, lon, brg, rangeMm));
    }
    void addArc (int penIndex, int penStyle, int penWidth, double centerLat, double centerLon, double radiusMm, double start, double end) {
        container.push_back (new ArcDrawer (penIndex, penStyle, penWidth, view, centerLat, centerLon, radiusMm, start, end));
    }
    void addText (double lat, double lon, TextDesc& desc, FeatureObject *object) {
        container.push_back (new TextDrawer (view, lat, lon, desc, object, dai, attrDic)); 
    }
    void addSymbol (double lat, double lon, size_t symbolIndex, double rotAngle, Dai& dai) {
        container.push_back (new SymbolDrawer (view, lat, lon, symbolIndex, rotAngle, dai));
    }
    void addCentralEdgeSymbol (Chart& chart, size_t symbolIndex, size_t edgeIndex, Dai& dai) {
        container.push_back (new CentralEdgeSymbolDrawer (chart, view, symbolIndex, edgeIndex, dai));
    }
    void addCompoundLightArc (
        int penIndex,
        int penStyle,
        int penWidth,
        double centerLat,
        double centerLon,
        double radiusMm,
        double start,
        double end
    );
    void addEdgeChain (int penIndex, int penStyle, int penWidth, Chart& chart);
    void addEdge (struct EdgeRef& edgeRef);
    void addArea (size_t fillBrushIndex, size_t patternBrushIndex, Chart& chart);
    void removeAllSymbols () {
        for (int i = container.size () - 1; i >= 0; --i) {
            auto drawer = container [i];
            if (drawer->isSymbol ()) {
                container.erase (container.begin () + i);
                delete drawer;
            }
        }
    }
};
