#pragma once

#include <vector>
#include "s57defs.h"

bool parseCatalog (const char *catPath, std::vector<CatalogItem>& catalog);
size_t parseRecord (const char *start);
