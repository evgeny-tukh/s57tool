#include <cstdint>
#include <math.h>
#include <Windows.h>
#include "geo.h"

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
