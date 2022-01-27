#include <cstdint>
#include <math.h>
#include <Windows.h>
#include "geo.h"
#include "data.h"
#include "classes.h"

void geoToXY (double lat, double lon, int zoom, int& x, int& y) {
    double zoomFactor = (double) (1 << zoom);
    double latRad = lat * RAD_IN_DEG;
    double lonRad = lon * RAD_IN_DEG;
    //double tileX = (lonRad + PI) * 0.5 / PI * zoomFactor;
    //double tileY = (1.0 - (log (tan (latRad) + 1.0 /cos (latRad)) / PI)) * 0.5 * zoomFactor;
    //x = (int) (tileX * 256.0);
    //y = (int) (tileY * 256.0);
    x = (uint32_t) ((128.0 / PI) * zoomFactor * (lonRad + PI));
    double A = (PI - log (tan (PI * 0.25 + latRad * 0.5)));
    y = (uint32_t) ((128.0 / PI) * zoomFactor * A);
}

void xyToClient (int x, int y, ClientPos& clientPos) {
    clientPos.tileLeft = x / 256;
    clientPos.tileTop = y / 256;
    clientPos.tileXOffs = x % 256;
    clientPos.tileYOffs = y % 256;
}

void geoToClient (double lat, double lon, int zoom, ClientPos& clientPos) {
    int x, y;
    geoToXY (lat, lon, zoom, x, y);
    xyToClient (x, y, clientPos);
}

void xyToGeo (int x, int y, int zoom, double& lat, double& lon) {
    double zoomFactor = (double) (1 << zoom);
    double lonRad = (double) x / ((128.0 / PI) * zoomFactor) - PI;
    lon = lonRad / RAD_IN_DEG;
    double logTanA = PI - (double) y / ((128.0 / PI) * zoomFactor);
    double tanA = exp (logTanA);
    double A = atan (tanA);
    double latRad = 2.0 * (A - PI * 0.25);
    lat = latRad / RAD_IN_DEG;
}

void clientToXY (ClientPos& clientPos, int& x, int& y) {
    x = clientPos.tileLeft * 256 + clientPos.tileXOffs;
    y = clientPos.tileTop * 256 + clientPos.tileYOffs;
}

void clientToGeo (ClientPos& clientPos, int zoom, double& lat, double& lon) {
    int x, y;
    clientToXY (clientPos, x, y);
    xyToGeo (x, y, zoom, lat, lon);
}

double getPixelSizeInMm () {
    static double pixelSizeInMm = 0.0;

    if (pixelSizeInMm == 0.0) {
        HDC dc = GetDC (HWND_DESKTOP);
        int horSizeMm = GetDeviceCaps (dc, HORZSIZE);
        int horSizePix = GetDeviceCaps (dc, HORZRES);

        pixelSizeInMm = (double) horSizeMm / (double) horSizePix;
    }
    
    return pixelSizeInMm;
}

double zoomToScale (int zoom) {
    static const double EQUATOR_LENGTH_MM = 360. * 60. * 1852. * 1000.;

    int numOfWorldPixels = (1 << zoom) * 256;
    double numOfWorldMm = (double) numOfWorldPixels * PIXEL_SIZE_IN_MM;

    return EQUATOR_LENGTH_MM / numOfWorldMm;
}

double mmToMiles (double distMm, int zoom) {
    double scale = zoomToScale (zoom);
    return distMm * scale / 1852000.0;
}

double calcSphericalRng (const double lat1, const double lon1, const double lat2, const double lon2) {
    double latRad1 = lat1 * RAD_IN_DEG;
    double latRad2 = lat2 * RAD_IN_DEG;
    double lonRad1 = lon1 * RAD_IN_DEG;
    double lonRad2 = lon2 * RAD_IN_DEG;

    return acos (sin (latRad1) * sin (latRad2) + cos (latRad1) * cos (latRad2) * cos (lonRad1 - lonRad2)) * EARTH_RADIUS;
}

double calcSphericalRngNm (const double lat1, const double lon1, const double lat2, const double lon2) {
    return calcSphericalRng (lat1, lon1, lat2, lon2) / 1852.0;
}

double calcSphericalBrg (const double lat1, const double lon1, const double lat2, const double lon2) {
    double latRad1 = lat1 * RAD_IN_DEG;
    double latRad2 = lat2 * RAD_IN_DEG;
    double lonRad1 = lon1 * RAD_IN_DEG;
    double lonRad2 = lon2 * RAD_IN_DEG;
    double deltaLonW = fmod (lonRad1 - lonRad2, TWO_PI);
    double deltaLonE = fmod (lonRad2 - lonRad1, TWO_PI);
    double bearing;

    if (fabs (latRad1 - latRad2) < 1.0E-8) {
        // Same latitude, bearing is 90 or 270 deg
        bearing = (deltaLonW > deltaLonE) ? PI * 1.5 : PI * 0.5;
    } else {
        double tanRatio  = tan (latRad2 * 0.5 + PI * 0.25) / tan (latRad1 * 0.5 + PI * 0.25);
        double deltaLat  = log (tanRatio);

        bearing = (deltaLonW < deltaLonE) ? fmod (atan2 (- deltaLonW, deltaLat), TWO_PI) : fmod (atan2 (deltaLonE, deltaLat), TWO_PI);
    }

    return bearing * RAD_IN_DEG;
}

void calcSphericalPos (double lat, double lon, double bearing, double range, double& destLat, double& destLon) {
    double latRad = lat * RAD_IN_DEG;
    double lonRad = lon * RAD_IN_DEG;
    double brgRad = bearing * RAD_IN_DEG;
    double arcAngle = range * 1852.0 / EARTH_RADIUS;
    double arcSin = sin (arcAngle);
    double arcCos = cos (arcAngle);
    double latSin = sin (latRad);
    double latCos = cos (latRad);

    destLat = asin (latSin * arcCos + latCos * arcSin * cos (brgRad));
    destLon = lonRad + atan2 (sin (brgRad) * arcSin * latCos, arcCos - latSin * sin (destLat));

    destLat /= RAD_IN_DEG;
    destLon /= RAD_IN_DEG;
}

void composeAreaMetrics (FeatureObject *object, Chart& chart, Contours& metrics) {
    Nodes& nodes = chart.nodes;
    Edges& edges = chart.edges;
    bool hole = false;

    auto isLastContourClosed = [&metrics] () {
        auto& lastContour = metrics.back ();
        if (lastContour.size () > 1) {
            return lastContour.front ().lat == lastContour.back ().lat && lastContour.front ().lon == lastContour.back ().lon;
        } else {
            return false;
        }
    };
    auto addVertex = [&metrics] (double lat, double lon) {
        metrics.back ().emplace_back (lat, lon);
    };
    auto addNode = [&nodes, &addVertex] (size_t nodeIndex) {
        auto& pos = nodes [nodeIndex].points [0];
        addVertex (pos.lat, pos.lon);
    };
    auto addEdge = [&edges, &metrics, &hole, &isLastContourClosed, &addNode, &addVertex] (EdgeRef& edgeRef) {
        if (edgeRef.hidden) return;

        auto& edge = edges.container [edgeRef.index];

        if (metrics.empty ()) metrics.emplace_back ();

        if (edgeRef.hole != hole) {
            hole = edgeRef.hole;
            metrics.emplace_back ();
        } else if (hole && isLastContourClosed ()) {
            metrics.emplace_back ();
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
    };

    metrics.clear ();

    if (!object || object->primitive != 3 && object->primitive != 2) return;

    for (auto& edgeRef: object->edgeRefs) {
        if (edgeRef.hidden) return;

        auto& edge = edges.container [edgeRef.index];

        if (metrics.empty ()) metrics.emplace_back ();

        if (edgeRef.hole != hole) {
            hole = edgeRef.hole;
            metrics.emplace_back ();
        } else if (hole && isLastContourClosed ()) {
            metrics.emplace_back ();
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
}

void getBoundingRect (Contour& contour, double& northmost, double& southmost, double& westmost, double& eastmost) {
    northmost = -90.0;
    southmost = 90.0;
    westmost = 180.0;
    eastmost = -180.0;
    for (auto& pt: contour) {
        northmost = max (northmost, pt.lat);
        southmost = min (southmost, pt.lat);
        westmost = min (westmost, pt.lon);
        eastmost = max (eastmost, pt.lon);
    }
}

std::tuple<double, double> crossTwoLines (double x11, double y11, double x12, double y12, double x21, double y21, double x22, double y22) {
    double c1 = (y12 - y11) / (x12 - x11);
    double c2 = (y22 - y21) / (x22 - x21);
    double x = (y21 - y11 + x11 * c1 - x21 * c2) / (c1 - c2);
    double y = y11 + (x - x11) * c1;
    return std::tuple<double, double>(x, y);
}

inline bool inside (double val, double a, double b) {
    return val >= min (a, b) && val <= max (a, b);
}

std::tuple<bool, double, double> crossTowSections (double x11, double y11, double x12, double y12, double x21, double y21, double x22, double y22) {
    auto [crossX, crossY] = crossTwoLines (x11, y11, x12, y12, x21, y21, x22, y22);
    bool crossed = inside (crossX, x11, x12) && inside (crossX, x21, x22) && inside (crossY, y11, y12) && inside (crossY, y21, y22);
    return std::tuple<bool, double, double>(crossed, crossX, crossY);
}

bool isPointInsideContour (double lat, double lon, Contour& contour) {
    int crossCount = 0;
    
    for (size_t i = 0; i < contour.size (); ++ i) {
        auto& pt1 = i > 0 ? contour [i-1] : contour.back ();
        auto& pt2 = contour [i];

        auto [crossed, crossX, crossY] = crossTowSections (pt1.lon, pt1.lat, pt2.lon, pt2.lat, lon, lat, -180.0, lat);

        if (crossed) ++ crossCount;
    }

    return crossCount & 1;
}

AreaTopology& checkAddAreaTopology (FeatureObject& area, Chart& chart) {
    auto pos = chart.areaTopologyMap.find (area.fidn);

    if (pos == chart.areaTopologyMap.end ()) {
        pos = chart.areaTopologyMap.emplace (area.fidn, AreaTopology ()).first;
        composeAreaMetrics (& area, chart, pos->second.metrics);
        if (!pos->second.metrics.empty ()) {
            getBoundingRect (pos->second.metrics.front (), pos->second.northmost, pos->second.southmost, pos->second.westmost, pos->second.eastmost);
        }
    }
    return pos->second;
}

void addSpatialsUnderPoint (FeatureObject& point, Chart& chart, SpatialsUnderObject& areasUnderPoint) {
    for (size_t i = 0; i < chart.features.container.size (); ++ i) {
        auto& object = chart.features.container [i];
        if (object.primitive != 3 && object.group != 2) continue;
        /*switch (object.classCode) {
            case OBJ_CLASSES::DEPARE:
            case OBJ_CLASSES::UNSARE:
            case OBJ_CLASSES::DRGARE:
                break;
            default:
                continue;
        }*/

        auto& areaTopology = checkAddAreaTopology (object, chart);
        auto& pos = chart.nodes [point.nodeIndex].points.front ();

        if (areaTopology.isPointInside (pos.lat, pos.lon)) {
            auto& info = areasUnderPoint.emplace_back ();
            info.areaIndex = i;
            info.fidn = object.fidn;
            info.classCode = object.classCode;
            
            auto drval1 = object.getAttr (ATTRS::DRVAL1);
            auto drval2 = object.getAttr (ATTRS::DRVAL2);

            if (drval1 && !drval1->noValue) info.depthRangeValue1 = drval1->floatValue;
            if (drval2 && !drval2->noValue) info.depthRangeValue2 = drval2->floatValue;
        }
    }
}

void addSpatialsUnderSpatial (FeatureObject& spatialObj, Chart& chart, SpatialsUnderObject& areasUnderObject) {
    for (size_t i = 0; i < chart.features.container.size (); ++ i) {
        auto& object = chart.features.container [i];
        if (object.primitive == 1 || object.primitive == 4 || object.group != 1) continue;

        auto& objectTopology = checkAddAreaTopology (spatialObj, chart);
        auto& areaTopology = checkAddAreaTopology (object, chart);

        if (areaTopology.isCrossedBy (objectTopology)) {
            auto& info = areasUnderObject.emplace_back ();
            info.areaIndex = i;
            info.fidn = object.fidn;
            info.classCode = object.classCode;
            
            auto drval1 = object.getAttr (ATTRS::DRVAL1);
            auto drval2 = object.getAttr (ATTRS::DRVAL2);

            if (drval1 && !drval1->noValue) info.depthRangeValue1 = drval1->floatValue;
            if (drval2 && !drval2->noValue) info.depthRangeValue2 = drval2->floatValue;
        }
    }
}

void buildPointLocationInfo (Chart& chart) {
    Features& features = chart.features;
    Edges& edges = chart.edges;
    Nodes& nodes = chart.nodes;
    AreaTopologyMap& areaTopologyMap = chart.areaTopologyMap;

    chart.objectsUnderPoints.clear ();
    chart.objectsUnderSpatials.clear ();

    auto isUnproperObject = [] (FeatureObject& object) {
        switch (object.classCode) {
            case OBJ_CLASSES::SOUNDG:
            case OBJ_CLASSES::UWTROC:
            case OBJ_CLASSES::WRECKS:
            case OBJ_CLASSES::OBSTRN:
                return false;
            default:
                return true;
        }
    };

    for (auto& object: features.container) {
        if (isUnproperObject (object)) continue;

        if (object.primitive == 1 || object.primitive == 4) {
            auto& item = chart.objectsUnderPoints.emplace (object.fidn, SpatialsUnderObject ()).first->second;

            addSpatialsUnderPoint (object, chart, item);
        } else if (object.primitive == 2 && object.primitive == 3) {
            auto& item = chart.objectsUnderSpatials.emplace (object.fidn, SpatialsUnderObject ()).first->second;

            addSpatialsUnderSpatial (object, chart, item);
        }
    }
}

