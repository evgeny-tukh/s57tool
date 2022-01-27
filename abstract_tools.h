#pragma once

#include <vector>
#include <tuple>
#include <Windows.h>
#include "geo.h"

struct PenTool {
    typedef std::vector<std::vector<POINT>> PolyPolygon;

    bool polygonMode;
    View curView;
    PolyPolygon *curPolyPolygon;

    PenTool (): curView (0.0, 0.0, 0), polygonMode (false), curPolyPolygon (0) {}

    void stylize (int style, PolyPolygon& polyPolygon, PolyPolygon& temp);
    void composeLine (int style, double lat, double lon, double brg, double lengthInMm, View& view, PolyPolygon& polyPolygon, bool appendMode);
    void composeLeg (int style, double lat, double lon, double destLat, double destLon, View& view, PolyPolygon& polyPolygon, bool appendMode);
    void appendToLastLeg (double lat, double lon, View& view, PolyPolygon& polyPolygon);

    void composeArc (int style, double centerLat, double centerLon, double start, double end, double radiusInMm, View& view, PolyPolygon& vertices);
    void beginPolyPolygon (View& view, PolyPolygon& polyPolygon);
    void addVertexToPolygon (double lat, double lon);

    void composeSection (int x1, int y1, int x2, int y2, double lengthInPix, double strokeLengthPix, double gapLengthPix, PolyPolygon& polyPolygon);

    void geo2screen (double lat, double lon, View& view, int& x, int& y);
    static std::tuple<bool, double, double> getStrokeProps (int style);

    template<typename TYPE>
    void translatePolyPolygon (PolyPolygon& polyPolygon, std::vector<POINT>& vertices, std::vector<TYPE>& sizes) {
        vertices.clear ();
        sizes.clear ();
        for (auto& contour: polyPolygon) {
            if (contour.size () > 0) {
                TYPE size = 0;
                for (size_t i = 0; i < contour.size (); ++ i) {
                    auto& vertex = contour [i];
                    if (vertices.empty () || i == 0 || i == (contour.size () - 1)) {
                        auto& pt = vertices.emplace_back ();
                        pt.x = vertex.x;
                        pt.y = vertex.y;
                        ++ size;
                    } else {
                        int dx = vertices.back ().x - vertex.x;
                        int dy = vertices.back ().y - vertex.y;
                        int rngSquare = dx * dx + dy * dy;
                        if (rngSquare >= GEN_SQUARE) {
                            auto& pt = vertices.emplace_back ();
                            pt.x = vertex.x;
                            pt.y = vertex.y;
                            ++ size;
                        }
                    }
                }
                if (size > 0) sizes.emplace_back (size);
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
