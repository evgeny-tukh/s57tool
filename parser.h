#pragma once

#include <vector>
#include "s57defs.h"

bool parseCatalog (const char *catPath, std::vector<CatalogItem>& catalog);
bool loadParseS57File (char *path, std::vector<std::vector<FieldInstance>>& records);
size_t parseRecord (const char *start);
std::string formatLat (double lat);
std::string formatLon (double lon);