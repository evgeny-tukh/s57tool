#include "abstract_tools.h"
#include "geo.h"

void PenTool::geo2screen (double lat, double lon, View& view, int& x, int& y) {
    int westX, northY;
    geoToXY (lat, lon, view.zoom, x, y);
    geoToXY (view.north, view.west, view.zoom, westX, northY);
    x -= westX;
    y -= northY;
};

std::tuple<bool, double, double> PenTool::getStrokeProps (int style) {
    double strokeLengthPix = 0.0, gapLengthPix = 0.0;
    bool ok = style == PS_DASH || style == PS_DOT;
    if (ok) {
        double strokeLength = style == PS_DASH ? 3.6 : 0.6;
        double gapLength = style == PS_DASH ? 1.8 : 1.2;
        strokeLengthPix = strokeLength / PIXEL_SIZE_IN_MM;
        gapLengthPix = gapLength / PIXEL_SIZE_IN_MM;
    }
    return std::tuple<bool, double, double> (ok, strokeLengthPix, gapLengthPix);
}

void PenTool::composeSection (int x1, int y1, int x2, int y2, double lengthInPix, double strokeLengthPix, double gapLengthPix, PolyPolygon& polyPolygon) {
    for (double offset = 0.0; offset <= lengthInPix; offset = min (lengthInPix, offset + (strokeLengthPix + gapLengthPix))) {
        double coef1 = offset / lengthInPix;
        double endOfStroke = min (lengthInPix, offset + strokeLengthPix);
        double coef2 = endOfStroke / lengthInPix;
        double beginX = x1 + (x2 - x1) * coef1;
        double beginY = y1 + (y2 - y1) * coef1;
        double endX = x1 + (x2 - x1) * coef2;
        double endY = y1 + (y2 - y1) * coef2;

        polyPolygon.emplace_back ();

        auto& contour = polyPolygon.back ();

        contour.emplace_back ();
        contour.back ().x = (int) beginX;
        contour.back ().y = (int) beginY;
        contour.emplace_back ();
        contour.back ().x = (int) endX;
        contour.back ().y = (int) endY;

        if (offset == lengthInPix) break;
    }
}

void PenTool::appendToLastLeg (double lat, double lon, View& view, PolyPolygon& polyPolygon) {
    int x, y;

    geo2screen (lat, lon, view, x, y);

    polyPolygon.back ().emplace_back ();
    polyPolygon.back ().back ().x = x;
    polyPolygon.back ().back ().y = y;
}

void PenTool::stylize (int style, PolyPolygon& from, PolyPolygon& to) {
    auto [ok, strokeLengthPix, gapLengthPix] = getStrokeProps (style);

    to.clear ();

    for (auto& fromContour: from) {
        POINT lastPassedPt = fromContour.front ();
        bool onStroke = true;
        double usedLength = 0.0;

        auto addStroke = [&fromContour, &to] (int startX, int startY, size_t startIndex, double startOffset, double strokeLen) {
            auto& toContour = to.emplace_back ();
            POINT passedPt;
            size_t passedIndex = startIndex - 1;
            double passedDist = startOffset;
            
            passedPt.x = startX;
            passedPt.y = startY;

            toContour.push_back (passedPt);

            for (size_t i = startIndex; i < fromContour.size (); ++ i) {
                auto& pt = fromContour [i];
                double d1 = pt.x - passedPt.x;
                double d2 = pt.y - passedPt.y;
                double len = sqrt (d1 * d1 + d2 * d2);

                if ((passedDist + len) <= strokeLen) {
                    toContour.push_back (pt);
                    passedPt = pt;
                    passedIndex = i;
                    passedDist += len;
                } else {
                    double overrun = passedDist + len - strokeLen;
                    double coef = (len - overrun) / len;
                    double x = passedPt.x + coef * d1;
                    double y = passedPt.y + coef * d2;
                    toContour.emplace_back ();
                    toContour.back ().x = (int) x;
                    toContour.back ().y = (int) y;

                    return std::tuple<bool, double, size_t, int, int> (false, overrun, passedIndex, (int) x, (int) y);
                }
            }
            return std::tuple<bool, double, size_t, int, int> (true, 0.0, fromContour.size (), toContour.back ().x, toContour.back ().y);
        };

        auto addGap = [&fromContour] (int startX, int startY, size_t startIndex, double startOffset, double gapLen) {
            POINT passedPt;
            size_t passedIndex = startIndex - 1;
            double passedDist = startOffset;
            
            passedPt.x = startX;
            passedPt.y = startY;

            for (size_t i = startIndex; i < fromContour.size (); ++ i) {
                auto& pt = fromContour [i];
                double d1 = pt.x - passedPt.x;
                double d2 = pt.y - passedPt.y;
                double len = sqrt (d1 * d1 + d2 * d2);

                if ((passedDist + len) <= gapLen) {
                    passedPt = pt;
                    passedIndex = i;
                    passedDist += len;
                } else {
                    double overrun = passedDist + len - gapLen;
                    double coef = (len - overrun) / len;
                    double x = passedPt.x + coef * d1;
                    double y = passedPt.y + coef * d2;

                    return std::tuple<bool, double, size_t, int, int> (false, overrun, passedIndex, (int) x, (int) y);
                }
            }
            return std::tuple<bool, double, size_t, int, int> (true, 0.0, fromContour.size (), fromContour.back ().x, fromContour.back ().y);
        };

        double startOffset = 0.0;
        size_t startIndex = 1;
        int startX = fromContour.front ().x;
        int startY = fromContour.front ().y;
        while (true) {
            auto [finished, overrun, passedIndex, endX, endY] = addStroke (startX, startY, startIndex, startOffset, strokeLengthPix);

            if (!finished) {
                startIndex = passedIndex + 1;
                startOffset = 0.0; //overrun;
                startX = endX;
                startY = endY;

                auto [finished, overrun, passedIndex, endX, endY] = addGap (startX, startY, startIndex, startOffset, gapLengthPix);

                if (finished) {
                    break;
                } else {
                    startIndex = passedIndex + 1;
                    startOffset = 0.0; //overrun;
                    startX = endX;
                    startY = endY;
                }
            }
        }
    }
}

void PenTool::composeLeg (
    int style,
    double lat,
    double lon,
    double destLat,
    double destLon,
    View& view,
    PolyPolygon& polyPolygon,
    bool appendMode
) {
    int x1, y1, x2, y2;

    geo2screen (lat, lon, view, x1, y1);
    geo2screen (destLat, destLon, view, x2, y2);

    int cath1 = x2 - x1;
    int cath2 = y2 - y1;
    double lengthInPix = sqrt ((double) (cath1 * cath1 + cath2 * cath2));

    if (!appendMode) polyPolygon.clear ();

    if (style == PS_SOLID) {
        polyPolygon.emplace_back ();

        auto& contour = polyPolygon.back ();

        contour.resize (2);
        contour [0].x = x1;
        contour [0].y = y1;
        contour [1].x = x2;
        contour [1].y = y2;
    } else {
        auto [ok, strokeLengthPix, gapLengthPix] = getStrokeProps (style);

        composeSection (x1, y1, x2, y2, lengthInPix, strokeLengthPix, gapLengthPix, polyPolygon);
    }
}

void PenTool::composeLine (
    int style,
    double lat,
    double lon,
    double brg,
    double lengthInMm,
    View& view,
    PolyPolygon& polyPolygon,
    bool appendMode
) {
    double destLat, destLon;
    calcSphericalPos (lat, lon, brg, mmToMiles (lengthInMm, view.zoom), destLat, destLon);

    composeLeg (style, lat, lon, destLat, destLon, view, polyPolygon, appendMode);
}

void PenTool::composeArc (int style, double centerLat, double centerLon, double start, double end, double radiusInMm, View& view, PolyPolygon& polyPolygon) {
    double startRad = start * RAD_IN_DEG;
    double endRad = end * RAD_IN_DEG;
    double arcLengthRad = endRad - startRad;
    double radiusInNm = mmToMiles (radiusInMm, view.zoom);
    
    if (arcLengthRad < 0.0) {
        startRad -= TWO_PI;
        arcLengthRad += TWO_PI;
    }

    double arcLengthMm = arcLengthRad * radiusInMm;

    polyPolygon.clear ();
    polyPolygon.emplace_back ();

    if (style == PS_SOLID) {
        for (double brg = start; brg <= end; brg = min (end, brg + 2.0)) {
            double lat, lon;
            int x, y;
            calcSphericalPos (centerLat, centerLon, brg, radiusInNm, lat, lon);
            geo2screen (lat, lon, view, x, y);
            polyPolygon.back ().emplace_back ();
            polyPolygon.back ().back ().x = x;
            polyPolygon.back ().back ().y = y;
            if (brg == end) break;
        }
    } else {
        double strokeLength = style == PS_DASH ? 3.6 : 1.8;
        double gapLength = style == PS_DASH ? 0.6 : 1.2;
        double strokeLengthPix = strokeLength / PIXEL_SIZE_IN_MM;
        double gapLengthPix = gapLength / PIXEL_SIZE_IN_MM;
        double arcLengthPix = arcLengthMm / PIXEL_SIZE_IN_MM;
        double numOfSteps = arcLengthMm / (strokeLength + gapLength);
        double strokeArcBrg = strokeLength / radiusInMm;
        double gapArcBrg = gapLength / radiusInMm;
        double strokeArcBrgDeg = strokeArcBrg / RAD_IN_DEG;
        double gapArcBrgDeg = gapArcBrg / RAD_IN_DEG;
        int numOfStepsInt = (int) numOfSteps;

        for (double brg = start; brg <= end; brg = min (end, brg + strokeArcBrgDeg + gapArcBrgDeg)) {
            double lat, lon;
            int x, y;
            polyPolygon.emplace_back ();
            calcSphericalPos (centerLat, centerLon, brg, radiusInNm, lat, lon);
            geo2screen (lat, lon, view, x, y);
            polyPolygon.back ().emplace_back ();
            polyPolygon.back ().back ().x = x;
            polyPolygon.back ().back ().y = y;
            calcSphericalPos (centerLat, centerLon, brg + strokeArcBrgDeg, radiusInNm, lat, lon);
            geo2screen (lat, lon, view, x, y);
            polyPolygon.back ().emplace_back ();
            polyPolygon.back ().back ().x = x;
            polyPolygon.back ().back ().y = y;
            if (brg == end) break;
        }
    }
}

void PenTool::beginPolyPolygon (View& view, PolyPolygon& polyPolygon) {
    polygonMode = true;
    curView.north = view.north;
    curView.west = view.west;
    curView.zoom = view.zoom;
    polyPolygon.emplace_back ();
}

void PenTool::addVertexToPolygon (double lat, double lon) {
    int x, y;

    geo2screen (lat, lon, curView, x, y);

    curPolyPolygon->back ().emplace_back ();

    curPolyPolygon->back ().back ().x = x;
    curPolyPolygon->back ().back ().y = y;
}
