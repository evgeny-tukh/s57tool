#pragma once

#include <vector>
#include "s57defs.h"
#include "data.h"

bool parseCatalog (const char *catPath, std::vector<CatalogItem>& catalog);
bool loadParseS57File (char *path, std::vector<std::vector<FieldInstance>>& records);
void extractDatasetParameters (std::vector<std::vector<FieldInstance>>& records, DatasetParams& datasetParams);
//void extractFeatureObjects (std::vector<std::vector<FieldInstance>>& records, std::vector<FeatureDesc>& objects);
void extractFeatureObjects (std::vector<std::vector<FieldInstance>>& records, Chart& chart);
void extractNodes (std::vector<std::vector<FieldInstance>>& records, Chart& chart, DatasetParams datasetParams);
void extractEdges (std::vector<std::vector<FieldInstance>>& records, Chart& chart, DatasetParams datasetParams);
void deformatAttrValues (AttrDictionary& attrDictionary, Chart& chart);
void loadObjectDictionary (const char *path, ObjectDictionary& dictionary);
void loadAttrDictionary (const char *path, AttrDictionary& dictionary);
void loadDai (const char *path, Environment& environment);
void loadColorTable (const char *path, Dai& dai);
size_t parseRecord (const char *start);
std::string formatLat (double lat);
std::string formatLon (double lon);
void processInstructions (Dai& dai, AttrDictionary& attrDic, LookupTableItem& item, std::vector<std::string>& instructions);
void parseTextInstruction (const char *instr, Dai& dai, AttrDictionary& attrDic, TextDesc& desc);
std::string getAttrStringValue (Attr *attr, AttrDictionary& dic);
std::tuple<bool, int, double, double, double, double> getCoverageRect (Features& features, Nodes& nodes, Edges& edges);

