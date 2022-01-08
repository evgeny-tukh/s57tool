#pragma once

#include <vector>
#include "s57defs.h"
#include "data.h"

bool parseCatalog (const char *catPath, std::vector<CatalogItem>& catalog);
bool loadParseS57File (char *path, std::vector<std::vector<FieldInstance>>& records);
void extractDatasetParameters (std::vector<std::vector<FieldInstance>>& records, DatasetParams& datasetParams);
void extractFeatureObjects (std::vector<std::vector<FieldInstance>>& records, std::vector<FeatureDesc>& objects);
void extractFeatureObjects (std::vector<std::vector<FieldInstance>>& records, Features& features);
void extractNodes (std::vector<std::vector<FieldInstance>>& records, Nodes& points, DatasetParams datasetParams);
void extractEdges (std::vector<std::vector<FieldInstance>>& records, Edges& edges, Nodes& points, DatasetParams datasetParams);
void deformatAttrValues (AttrDictionary& attrDictionary, std::vector<FeatureDesc>& objects);
void loadObjectDictionary (const char *path, ObjectDictionary& dictionary);
void loadAttrDictionary (const char *path, AttrDictionary& dictionary);
void loadDai (const char *path, Dai& dai);
size_t parseRecord (const char *start);
std::string formatLat (double lat);
std::string formatLon (double lon);