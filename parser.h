#pragma once

#include <vector>
#include "s57defs.h"

bool parseCatalog (const char *catPath, std::vector<CatalogItem>& catalog);
bool loadParseS57File (char *path, std::vector<std::vector<FieldInstance>>& records);
void extractDatasetParameters (std::vector<std::vector<FieldInstance>>& records, DatasetParams& datasetParams);
void extractFeatureObjects (std::vector<std::vector<FieldInstance>>& records, std::map<uint32_t, FeatureDesc>& featureObjects);
void loadObjectDictionary (const char *path, ObjectDictionary& dictionary);
void loadAttrDictionary (const char *path, AttrDictionary& dictionary);
size_t parseRecord (const char *start);
std::string formatLat (double lat);
std::string formatLon (double lon);