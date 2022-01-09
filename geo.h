#pragma once

const double PI = 3.1415926535897932384626433832795;
const double RAD_IN_DEG = PI / 180.0;

struct ClientPos {
    int tileLeft;
    int tileTop;
    int tileXOffs;
    int tileYOffs; 
};

void geoToXY (double lat, double lon, int zoom, int& x, int& y);
void xyToGeo (int x, int y, int zoom, double& lat, double& lon);

void xyToClient (int x, int y, ClientPos& clientPos);
void clientToXY (ClientPos& clientPos, int& x, int& y);

void geoToClient (double lat, double lon, int zoom, ClientPos& clientPos);
void clientToGeo (ClientPos& clientPos, int zoom, double& lat, double& lon);