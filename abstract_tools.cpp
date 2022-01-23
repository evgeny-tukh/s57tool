#include "abstract_tools.h"
#include "geo.h"

void PenTool::geo2screen (double lat, double lon, double north, double west, int zoom, int& x, int& y) {
    int westX, northY;
    geoToXY (lat, lon, zoom, x, y);
    geoToXY (north, west, zoom, westX, northY);
    x -= westX;
    y -= northY;
};

void PenTool::composeLine (
    int style,
    double lat,
    double lon,
    double brg,
    double lengthInMm,
    double north,
    double west,
    int zoom,
    PolyPolygon& polyPolygon
) {
    double destLat, destLon;
    calcSphericalPos (lat, lon, brg, mmToMiles (lengthInMm, zoom), destLat, destLon);

    int x1, y1, x2, y2;

    geo2screen (lat, lon, north, west, zoom, x1, y1);
    geo2screen (destLat, destLon, north, west, zoom, x2, y2);

    polyPolygon.clear ();

    if (style == PS_SOLID) {
        polyPolygon.emplace_back ();

        auto& contour = polyPolygon.back ();

        contour.resize (2);
        contour [0].x = x1;
        contour [0].y = y1;
        contour [1].x = x2;
        contour [1].y = y2;
    } else {
        double strokeLength = style == PS_DASH ? 3.6 : 1.8;
        double gapLength = style == PS_DASH ? 0.6 : 1.2;
        double strokeLengthPix = strokeLength / PIXEL_SIZE_IN_MM;
        double gapLengthPix = gapLength / PIXEL_SIZE_IN_MM;
        double lengthInPix = lengthInMm / PIXEL_SIZE_IN_MM;
        double numOfSteps = lengthInMm / (strokeLength + gapLength);
        int numOfStepsInt = (int) numOfSteps;

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
}

void PenTool::composeArc (int style, double centerLat, double centerLon, double start, double end, double radiusInMm, double north, double west, int zoom, PolyPolygon& polyPolygon) {
    double startRad = start * RAD_IN_DEG;
    double endRad = end * RAD_IN_DEG;
    double arcLengthRad = endRad - startRad;
    double radiusInNm = mmToMiles (radiusInMm, zoom);
    
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
            geo2screen (lat, lon, north, west, zoom, x, y);
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
            geo2screen (lat, lon, north, west, zoom, x, y);
            polyPolygon.back ().emplace_back ();
            polyPolygon.back ().back ().x = x;
            polyPolygon.back ().back ().y = y;
            calcSphericalPos (centerLat, centerLon, brg + strokeArcBrgDeg, radiusInNm, lat, lon);
            geo2screen (lat, lon, north, west, zoom, x, y);
            polyPolygon.back ().emplace_back ();
            polyPolygon.back ().back ().x = x;
            polyPolygon.back ().back ().y = y;
            if (brg == end) break;
        }
    }
}

void PenTool::beginPolyPolygon (double north, double west, int zoom, PolyPolygon& polyPolygon) {
    polygonMode = true;
    curNorth = north;
    curWest = west;
    curZoom = zoom;
    polyPolygon.emplace_back ();
}

void PenTool::addVertexToPolygon (double lat, double lon) {
    int x, y;

    geo2screen (lat, lon, curNorth, curWest, curZoom, x, y);

    curPolyPolygon->back ().emplace_back ();

    curPolyPolygon->back ().back ().x = x;
    curPolyPolygon->back ().back ().y = y;
}
