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