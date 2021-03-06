#pragma once

#include <vector>
#include <map>
#include <tuple>
#include <optional>

static const int GEN_SQUARE = 25;

const double PI = 3.1415926535897932384626433832795;
const double TWO_PI = PI + PI;
const double RAD_IN_DEG = PI / 180.0;
static const double PIXEL_SIZE_IN_MM = 0.264583333;
static const double EARTH_RADIUS = 6366707.0194937074958298109629434;

struct View {
    double north, west, south, east;
    int zoom;

    View (double _north, double _west, int _zoom): north (_north), west (_west), zoom (_zoom) {}
};

struct ClientPos {
    int tileLeft;
    int tileTop;
    int tileXOffs;
    int tileYOffs; 
};

struct Vertex {
    double lat, lon;
    Vertex (double _lat, double _lon): lat (_lat), lon (_lon) {}
};

typedef std::vector<Vertex> Contour;
typedef std::vector<Contour> Contours;

void geoToXY (double lat, double lon, int zoom, int& x, int& y);
void xyToGeo (int x, int y, int zoom, double& lat, double& lon);

void xyToClient (int x, int y, ClientPos& clientPos);
void clientToXY (ClientPos& clientPos, int& x, int& y);

void geoToClient (double lat, double lon, int zoom, ClientPos& clientPos);
void clientToGeo (ClientPos& clientPos, int zoom, double& lat, double& lon);

double zoomToScale (int zoom);
double mmToMiles (double distMm, int zoom);

inline double turnBackDeg (double brg) {
    return brg > 180.0 ? brg - 180.0 : brg + 180.0;
}

double getPixelSizeInMm ();

inline bool isAngleBetween (double angle, double a, double b) {
    if (a == b) {
        return angle == a;
    } else if (a < b) {
        return angle >= a && angle <= b;
    } else {
        return isAngleBetween (angle, a, 360.0) || isAngleBetween (angle, 0, b);
    }
}

bool isPointInsideContour (double lat, double lon, Contour& contour);

struct AreaTopology {
    Contours metrics;
    double northmost, southmost, westmost, eastmost;

    bool isPointInside (double lat, double lon) {
        if (lat > northmost || lat < southmost || lon < westmost || lon > eastmost) return false;
        if (!isPointInsideContour (lat, lon, metrics.front ())) return false;

        for (size_t i = 1; i < metrics.size (); ++ i) {
            if (isPointInsideContour (lat, lon, metrics [i])) return false;
        }

        return true;
    }
    bool isCrossedBy (Contour& contour) {
        for (auto& pt: contour) {
            if (!isPointInsideContour (pt.lat, pt.lon, metrics.front ())) continue;

            bool insideHole = false;

            for (size_t i = 1; i < metrics.size (); ++ i) {
                if (isPointInsideContour (pt.lat, pt.lon, metrics.front ())) {
                    insideHole = true; break;
                }
            }

            if (!insideHole) return true;
        }
        return false;
    }
    bool isCrossedBy (AreaTopology& another) {
        if (northmost < another.southmost) return false;
        if (southmost > another.northmost) return false;
        if (westmost > another.eastmost) return false;
        if (eastmost < another.westmost) return false;

        return isCrossedBy (another.metrics.front ());
    }
};
typedef std::map<uint32_t, AreaTopology> AreaTopologyMap;

struct SpatialUnderObject {
    size_t areaIndex;
    uint32_t fidn;
    uint16_t classCode;
    std::optional<double> depthRangeValue1, depthRangeValue2;
};

typedef std::vector<SpatialUnderObject> SpatialsUnderObject;

// Keyed by point/3d array object FIDN
typedef std::map<uint32_t, SpatialsUnderObject> UnderlyingObjectsList;

double calcSphericalRng (const double lat1, const double lon1, const double lat2, const double lon2);
double calcSphericalRngNm (const double lat1, const double lon1, const double lat2, const double lon2);
double calcSphericalBrg (const double lat1, const double lon1, const double lat2, const double lon2);
void calcSphericalPos (double lat, double lon, double bearing, double rangeNm, double& destLat, double& destLon);
void composeAreaMetrics (struct FeatureObject *object, struct Chart& chart, Contours& metrics);
void getBoundingRect (Contour& contour, double& northmost, double& southmost, double& westmost, double& eastmost);
void buildPointLocationInfo (struct Chart& chart);
void getCenterPos (FeatureObject& object, Chart& chart, double& lat, double& lon);
