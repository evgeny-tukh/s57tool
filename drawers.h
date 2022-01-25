#pragma once

#include <vector>
#include <string>
#include <Windows.h>
#include "abstract_tools.h"
#include "s57defs.h"

struct Drawer {
    size_t penIndex;
    double north, west;
    double lat, lon;
    double rangeMm;
    int zoom, penStyle, penWidth;
    static PenTool penTool;

    Drawer (
        size_t _penIndex,
        int _penStyle,
        int _penWidth,
        double _north,
        double _west,
        double _lat,
        double _lon,
        double _rangeMm,
        int _zoom
    ): penIndex (_penIndex), penStyle (_penStyle), penWidth (_penWidth), north (_north), west (_west), lat (_lat), lon (_lon), zoom (_zoom), rangeMm (_rangeMm) {}

    virtual void run (RECT& client, HDC paintDC, PaletteIndex paletteIndex, Palette& palette) {}
};

struct TextDrawer: Drawer {
    enum HorJust {
        CENTER = 1,
        RIGHT = 2,
        LEFT = 3,
    };
    enum VerJust {
        BOTTOM = 1,
        CENT = 2,
        TOP = 3,
    };
    enum Spacing {
        FIT = 1,
        STD = 2,
        WORD_WRAP = 3,
    };
    enum FontType {
        SERIF = 1,
    };
    enum FontWeight {
        LIGHT = 4,
        MEDIUM = 5,
        BOLD = 6,
    };
    enum FontStyle {
        REGULAR = 1,
    };

    std::string text;
    HorJust horJust;
    VerJust verJust;
    Spacing spacing;
    FontType fontType;
    FontWeight fontWeight;
    FontStyle fontStyle;
    int fontSize;
    int horOffset;
    int verOffset;

    TextDrawer (
        double _north,
        double _west,
        double _lat,
        double _lon,
        int _zoom,
        const char *instr,
        FeatureObject *object,
        Dai& dai,
        AttrDictionary& attrDic
    );

    virtual void run (RECT& client, HDC paintDC, PaletteIndex paletteIndex, Palette& palette);
    bool parseExtendedInstr (const char *instr, FeatureObject *object, Dai& dai, AttrDictionary& attrDic);
    bool parseRegularInstr (const char *instr, Dai& dai);
    bool parseInstr (const char *instr, std::vector<std::string>& parts);
};

struct LineDrawer: Drawer {
    double brg;

    LineDrawer (
        size_t _penIndex,
        int _penStyle,
        int _penWidth,
        double _north,
        double _west,
        double _lat,
        double _lon,
        double _brg,
        double _rangeMm,
        int _zoom
    ): Drawer (_penIndex, _penStyle, _penWidth, _north, _west, _lat, _lon, _rangeMm, _zoom), brg (_brg) {}

    virtual void run (RECT& client, HDC paintDC, PaletteIndex paletteIndex, Palette& palette);
};

struct ArcDrawer: Drawer {
    double start, end;

    ArcDrawer (
        size_t _penIndex,
        int _penStyle,
        int _penWidth,
        double _north,
        double _west,
        double _centerLat,
        double _centerLon,
        double _radiusMm,
        double _start,
        double _end,
        int _zoom
    ): Drawer (_penIndex, _penStyle, _penWidth, _north, _west, _centerLat, _centerLon, _radiusMm, _zoom), start (_start), end (_end) {}

    virtual void run (RECT& client, HDC paintDC, PaletteIndex paletteIndex, Palette& palette);
};

struct PolyPolylineDrawer: Drawer {
    Contours polyPolyline;
    Nodes& nodes;
    Edges& edges;

    PolyPolylineDrawer (
        size_t _penIndex,
        int _penStyle,
        int _penWidth,
        double _north,
        double _west,
        int _zoom,
        Nodes& _nodes,
        Edges& _edges
    ): Drawer (_penIndex, _penStyle, _penWidth, _north, _west, 0.0, 0.0, 0.0, _zoom), nodes (_nodes), edges (_edges) {
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
        double _north,
        double _west,
        int _zoom,
        Nodes& _nodes,
        Edges& _edges
    ): PolyPolylineDrawer (-1, PS_SOLID, 0, _north, _west, _zoom, _nodes, _edges), fillBrushIndex (_fillBrushIndex), patternBrushIndex (_patternBrushIndex), hole (false) {
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
    double north, west;
    HDC paintDC;
    int zoom;
    PaletteIndex paletteIndex;
    Dai& dai;
    RECT& client;

    DrawQueue (
        RECT& _client, 
        HDC _paintDC,
        PaletteIndex _paletteIndex,
        Dai& _dai,
        double _north,
        double _west,
        int _zoom
    ): paintDC (_paintDC), paletteIndex (_paletteIndex), dai (_dai), north (_north), west (_west), zoom (_zoom), client (_client) {}

    virtual ~DrawQueue () {
        clear ();
    }
    void clear () {
        for (auto drawer: container) delete drawer;
        container.clear ();
    }
    void run () {
        for (auto drawer: container) drawer->run (client, paintDC, paletteIndex, dai.palette);
    }
    void addLine (int penIndex, int penStyle, int penWidth, double lat, double lon, double brg, double rangeMm) {
        container.push_back (new LineDrawer (penIndex, penStyle, penWidth, north, west, lat, lon, brg, rangeMm, zoom));
    }
    void addArc (int penIndex, int penStyle, int penWidth, double centerLat, double centerLon, double radiusMm, double start, double end) {
        container.push_back (new ArcDrawer (penIndex, penStyle, penWidth, north, west, centerLat, centerLon, radiusMm, start, end, zoom));
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
    void addEdgeChain (int penIndex, int penStyle, int penWidth, Nodes& nodes, Edges& edges);
    void addEdge (struct EdgeRef& edgeRef);
    void addArea (size_t fillBrushIndex, size_t patternBrushIndex, Nodes& nodes, Edges& edges);
};
