#pragma once

#include <vector>

const double PI = 3.1415926535897932384626433832795;
const double TWO_PI = PI + PI;
const double RAD_IN_DEG = PI / 180.0;
static const double PIXEL_SIZE_IN_MM = 0.264583333;
static const double EARTH_RADIUS = 6366707.0194937074958298109629434;

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
    if (a < b) {
        return angle >= a && angle <= b;
    } else {
        return isAngleBetween (angle, a, 360.0) || isAngleBetween (angle, 0, b);
    }
}

double calcSphericalRng (const double lat1, const double lon1, const double lat2, const double lon2);
double calcSphericalRngNm (const double lat1, const double lon1, const double lat2, const double lon2);
double calcSphericalBrg (const double lat1, const double lon1, const double lat2, const double lon2);
void calcSphericalPos (double lat, double lon, double bearing, double rangeNm, double& destLat, double& destLon);
