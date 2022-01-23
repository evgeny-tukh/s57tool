#pragma once

#include <vector>
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
        int _penIndex,
        int _penStyle,
        int _penWidth,
        double _north,
        double _west,
        double _lat,
        double _lon,
        double _rangeMm,
        int _zoom
    ): penIndex (_penIndex), penStyle (_penStyle), penWidth (_penWidth), north (_north), west (_west), lat (_lat), lon (_lon), zoom (_zoom), rangeMm (_rangeMm) {}

    virtual void run (HDC paintDC, PaletteIndex paletteIndex, Palette& palette) {}
};

struct LineDrawer: Drawer {
    double brg;

    LineDrawer (
        int _penIndex,
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

    virtual void run (HDC paintDC, PaletteIndex paletteIndex, Palette& palette);
};

struct ArcDrawer: Drawer {
    double start, end;

    ArcDrawer (
        int _penIndex,
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

    virtual void run (HDC paintDC, PaletteIndex paletteIndex, Palette& palette);
};

struct DrawQueue {
    std::vector<Drawer *> container;
    double north, west;
    HDC paintDC;
    int zoom;
    PaletteIndex paletteIndex;
    Dai& dai;

    DrawQueue (
        HDC _paintDC,
        PaletteIndex _paletteIndex,
        Dai& _dai,
        double _north,
        double _west,
        int _zoom
    ): paintDC (_paintDC), paletteIndex (_paletteIndex), dai (_dai), north (_north), west (_west), zoom (_zoom) {}

    virtual ~DrawQueue () {
        clear ();
    }
    void clear () {
        for (auto drawer: container) delete drawer;
        container.clear ();
    }
    void run () {
        for (auto drawer: container) drawer->run (paintDC, paletteIndex, dai.palette);
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
};
