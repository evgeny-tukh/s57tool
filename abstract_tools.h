#pragma once

#include <vector>
#include <tuple>
#include <Windows.h>

struct PenTool {
    typedef std::vector<std::vector<POINT>> PolyPolygon;

    bool polygonMode;
    double curNorth;
    double curWest;
    int curZoom;
    PolyPolygon *curPolyPolygon;

    void composeLine (int style, double lat, double lon, double brg, double lengthInMm, double north, double west, int zoom, PolyPolygon& polyPolygon, bool appendMode);
    void composeLeg (int style, double lat, double lon, double destLat, double destLon, double north, double west, int zoom, PolyPolygon& polyPolygon, bool appendMode);
    void appendToLastLeg (double lat, double lon, double north, double west, int zoom, PolyPolygon& polyPolygon);

    void composeArc (int style, double centerLat, double centerLon, double start, double end, double radiusInMm, double north, double west, int zoom, PolyPolygon& vertices);
    void beginPolyPolygon (double north, double west, int zoom, PolyPolygon& polyPolygon);
    void addVertexToPolygon (double lat, double lon);

    void composeSection (int x1, int y1, int x2, int y2, double lengthInPix, double strokeLengthPix, double gapLengthPix, PolyPolygon& polyPolygon);

    void geo2screen (double lat, double lon, double north, double west, int zoom, int& x, int& y);
    std::tuple<bool, double, double> getStrokeProps (int style);

    template<typename TYPE>
    void translatePolyPolygon (PolyPolygon& polyPolygon, std::vector<POINT>& vertices, std::vector<TYPE>& sizes) {
        vertices.clear ();
        sizes.clear ();
        for (auto& contour: polyPolygon) {
            if (contour.size () > 0) {
                sizes.push_back (contour.size ());
                for (auto& vertex: contour) {
                    vertices.push_back (vertex);
                }
            }
        }
    }
    void removeOutOfScreenContours (RECT& client, PolyPolygon& polyPolygon) {
        for (auto contour = polyPolygon.begin (); contour != polyPolygon.end ();) {
            bool inScreen = false;
            for (auto& pt: *contour) {
                if (pt.x >= 0 && pt.x <= client.right && pt.y >= 0 && pt.y <= client.bottom) {
                    inScreen = true; break;
                }
            }
            if (inScreen) {
                ++ contour;
            } else {
                contour = polyPolygon.erase (contour);
            }
        }
    }
};
