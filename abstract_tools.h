#pragma once

#include <vector>
#include <Windows.h>

struct PenTool {
    typedef std::vector<std::vector<POINT>> PolyPolygon;

    bool polygonMode;
    double curNorth;
    double curWest;
    int curZoom;
    PolyPolygon *curPolyPolygon;

    void composeLine (int style, double lat, double lon, double brg, double lengthInMm, double north, double west, int zoom, PolyPolygon& vertices);
    void composeArc (int style, double centerLat, double centerLon, double start, double end, double radiusInMm, double north, double west, int zoom, PolyPolygon& vertices);
    void beginPolyPolygon (double north, double west, int zoom, PolyPolygon& polyPolygon);
    void addVertexToPolygon (double lat, double lon);

    void geo2screen (double lat, double lon, double north, double west, int zoom, int& x, int& y);

    template<typename TYPE>
    void translatePolyPolygon (PolyPolygon &polyPolygon, std::vector<POINT> vertices, std::vector<TYPE>sizes) {
        vertices.clear ();
        sizes.clear ();
        for (auto& contour: polyPolygon) {
            sizes.push_back (contour.size ());
            for (auto& vertex: contour) {
                vertices.push_back (vertex);
            }
        }
    }
};
