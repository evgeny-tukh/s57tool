#include <stdlib.h>
#include <stdio.h>
#include <cstdint>
#include <vector>
#include <map>
#include <string>
#include <tuple>
#include "s57defs.h"

size_t splitString (std::string source, std::vector<std::string>& parts, char separator) {
    parts.clear ();

    if (source.empty ()) return 0;

    parts.emplace_back ();

    for (char chr: source) {
        if (chr == separator) {
            parts.emplace_back ();
        } else {
            parts.back () += chr;
        }
    }

    return parts.size ();
}

size_t loadFile (const char *path, char *& content) {
    size_t size = 0;
    FILE *file = fopen (path, "rb+");

    if (file) {
        fseek (file, 0, SEEK_END);
        size = ftell (file);
        fseek (file, 0, SEEK_SET);
        content = (char *) malloc (size);
        
        if (!content) {
            size = 0;
        } else if (fread (content, 1, size, file) == 0) {
            free (content);
            size = 0;
        }

        fclose (file);
    }

    return size;
}

void processFieldControlField (
    std::string& fieldControls,
    std::string& fieldTreeText,
    std::vector<DirEntry>& directory,
    ParsedLeader& parsedLeader,
    std::vector<std::pair<std::string, std::string>>& fieldTree
) {
    fieldTree.clear ();

    for (size_t i = 0; i < fieldTreeText.length (); i += parsedLeader.fieldTagSize * 2) {
        auto& treeNode = fieldTree.emplace_back ();
        treeNode.first.append (fieldTreeText.c_str () + i, parsedLeader.fieldTagSize);
        treeNode.second.append (fieldTreeText.c_str () + (i + parsedLeader.fieldTagSize), parsedLeader.fieldTagSize);
    }
}

bool processDataDesciptiveField (
    std::map<std::string, DdfDesc>& dataDescriptiveFields,
    FieldControls *fieldControls,
    std::string& fieldTag,
    std::string& fieldName,
    std::string& arrayDesc,
    std::string& fieldValue
) {
    auto& item = dataDescriptiveFields.emplace (fieldTag, DdfDesc ()).first->second;
    std::vector<std::string> tags;
    std::vector<std::string> formats;

    item.name = fieldName;

    if (fieldValue.front () == '(' && fieldValue.back () == ')') {
        splitString (fieldValue.substr (1, fieldValue.length () - 2), formats, ',');
    } else {
        // Unproper format list, should be enclosed with ()
        return false;
    }

    splitString (arrayDesc, tags, '!');

    auto addFormat = [&item] (std::string& format) {
        auto& formatDesc = item.fields.emplace_back ();
        
        formatDesc.format = format [0];

        if (formatDesc.format == 'b') {
            formatDesc.modifier = (format [1] - '0') * 10 + format [2] - '0';
        } else if (format.length () > 1 && format [1] == '(') {
            formatDesc.modifier = std::atoi (format.substr (2, format.length () - 3).c_str ());
        }
    };

    for (auto& format: formats) {
        if (isdigit (format [0])) {
            // Multiplier found
            size_t quantity = isdigit (format [1]) ? (format [0] - '0') * 10 + format [1] - '0' : format [0] - '0';

            for (size_t i = 0; i < quantity; ++ i) {
                addFormat (format.substr (1));
            }
        } else {
            addFormat (format);
        }
    }

    for (size_t i = 0; i < tags.size (); ++ i) {
        item.fields [i].tag = tags [i];
    }

    return true;
}

bool parseDdrField (
    FieldControls *fieldControls,
    std::string& fieldTag,
    std::string& fieldName,
    std::string& arrayDesc,
    std::string& fieldValue,
    std::vector<DirEntry>& directory,
    ParsedLeader& parsedLeader,
    std::vector<std::pair<std::string, std::string>>& fieldTree,
    std::map<std::string, DdfDesc>& dataDescriptiveFields
) {
    bool result = false;
    
    if (memcmp (fieldControls, "0000", 4) == 0) {
        processFieldControlField (fieldName, arrayDesc, directory, parsedLeader, fieldTree); result = true;
    } else {
        processDataDesciptiveField (dataDescriptiveFields, fieldControls, fieldTag, fieldName, arrayDesc, fieldValue); result = true;
    }

    return result;
}

size_t parseRecordLeaderAndDirectory (const char *start, std::vector<DirEntry>& directory
) {
    Leader *leader = (Leader *) start;
    ParsedLeader parsedLeader (leader);

    directory.clear ();

    char *source = (char *) (leader + 1);

    while (*source != FT) {
        directory.emplace_back (source, & leader->entryMap);
    }

    ++ source;

    return source - start;
}

std::tuple<bool, std::string> getStrValue (RecordFieldDesc& fld, const char *& fieldPos) {
    if (*fieldPos == FT || *fieldPos == UT) {
        fieldPos ++; return std::tuple (false, std::string ());
    }

    std::string value;
if(fld.tag.compare("COMT")==0){
    int iii=0;
    ++iii;
    --iii;
}
    if (fld.modifier.has_value ()) {
        value.append (fieldPos, fld.modifier.value ());
        fieldPos += fld.modifier.value ();
    } else {
        size_t k;
        for (k = 0; fieldPos [k] != UT && fieldPos [k] != FT; ++ k) {
            value += fieldPos [k];
        }
        fieldPos += k + 1;
    }
    
    return std::tuple (true, value);
}

std::tuple<bool, uint32_t> getIntValue (RecordFieldDesc& fld, const char *& fieldPos) {
    if (*fieldPos == FT || *fieldPos == UT) {
        fieldPos ++; return std::tuple (false, 0);
    }

    char value [100];

    memset (value, 0, sizeof (value));

    if (fld.modifier.has_value ()) {
        memcpy (value, fieldPos, fld.modifier.value ());
        fieldPos += fld.modifier.value ();
    } else {
        size_t k;
        for (k = 0; isdigit (fieldPos [k]); ++ k) {
            value [k] = fieldPos [k];
        }
        fieldPos += k + 1;
    }
    
    return std::tuple (true, std::atoi (value));
}

std::tuple<bool, uint32_t> getBinValue (RecordFieldDesc& fld, const char *& fieldPos) {
    if (*fieldPos == FT || *fieldPos == UT) {
        fieldPos ++; return std::tuple (false, 0);
    }

    uint32_t value = 0;

    if (fld.modifier.has_value ()) {
        size_t width = fld.modifier.value () % 10;
        memcpy (& value, fieldPos, width);
        fieldPos += width;
    } else {
        size_t k;
        uint8_t *dest = (uint8_t *) & value;
        for (k = 0; k < 4 && fieldPos [k] != FT && fieldPos [k] != UT; ++ k) {
            dest [k] = fieldPos [k];
        }
        fieldPos += k + 1;
    }
    
    return std::tuple (true, value);
}

std::tuple<bool, double> getFloatValue (RecordFieldDesc& fld, const char *& fieldPos) {
    if (*fieldPos == FT || *fieldPos == UT) {
        fieldPos ++; return std::tuple (false, 0.0);
    }

    char value [100];

    memset (value, 0, sizeof (value));

    if (fld.modifier.has_value ()) {
        memcpy (value, fieldPos, fld.modifier.value ());
        fieldPos += fld.modifier.value ();
    } else {
        size_t k;
        bool minusPassed = false;
        bool dotPassed = false;
        bool digitPassed = false;
        auto validChar = [&digitPassed, &minusPassed, &dotPassed] (char chr) {
            if (isdigit (chr)) {
                digitPassed = true; return true;
            }
            if (chr == FT || chr == UT) return false;
            if (chr == '-') {
                if (digitPassed || dotPassed || minusPassed) return false;
                minusPassed = true; return true;
            }
            if (chr == '.') {
                if (!digitPassed || dotPassed) return false;
                dotPassed = true; return true;
            }
            return false;
        };
        for (k = 0; validChar (fieldPos [k]); ++ k) {
            value [k] = fieldPos [k];
        }
        fieldPos += k + 1;
    }
    
    return std::tuple (true, std::atof (value));
}

size_t parseDataRecord (
    const char *start,
    std::map<std::string, DdfDesc>& dataDescriptiveFields,
    std::vector<FieldInstance>& fieldInstances
) {
    std::vector<DirEntry> directory;
    ParsedLeader parsedLeader ((Leader *) start);

    fieldInstances.clear ();

    size_t headerSize = parseRecordLeaderAndDirectory (start, directory);
    const char *source = start + headerSize;

    for (size_t i = 0; i < directory.size (); ++ i) {
        auto& dirEntry = directory [i];
        auto& ddf = dataDescriptiveFields [dirEntry.tag];
        const char *fieldPos = source + dirEntry.position;
        auto& fieldInstance = fieldInstances.emplace_back ();

        fieldInstance.name = ddf.name;
        fieldInstance.tag = dirEntry.tag;

        for (auto j = 0; j < ddf.fields.size (); ++ j) {
            auto& fld = ddf.fields [j];
            auto& subFieldInstance = fieldInstance.subFieldInstances.emplace (fld.tag, SubFieldInstance ()).first;

            subFieldInstance->second.type = fld.format;

            switch (fld.format) {
                case 'I': {
                    auto [hasValue, intValue] = getIntValue (fld, fieldPos);
                    if (hasValue) subFieldInstance->second.intValue = intValue;
                    break;
                }
                case 'A': {
                    auto [hasValue, stringValue] = getStrValue (fld, fieldPos);
                    if (hasValue) subFieldInstance->second.stringValue = stringValue;
                    break;
                }
                case 'R': {
                    auto [hasValue, floatValue] = getFloatValue (fld, fieldPos);
                    if (hasValue) subFieldInstance->second.floatValue = floatValue;
                    break;
                }
                case 'b': {
                    auto [hasValue, intValue] = getBinValue (fld, fieldPos);
                    if (hasValue) subFieldInstance->second.intValue = intValue;
                    break;
                }
            }
        }
    }
    /*for (auto& ddf: dataDescriptiveFields) {
        switch (ddf.)
    }*/

    /*while ((source - start) < parsedLeader.recLength) {
    }*/

    return parsedLeader.recLength;
}

size_t parseDataDescriptiveRecord (
    const char *start,
    std::vector<std::pair<std::string, std::string>>& fieldTree,
    std::map<std::string, DdfDesc>& dataDescriptiveFields
) {
    std::vector<DirEntry> directory;
    ParsedLeader parsedLeader ((Leader *) start);
    
    #if 0
    Leader *leader = (Leader *) start;
    ParsedLeader parsedLeader (leader);

    directory.clear ();

    char *source = (char *) (leader + 1);

    while (*source != FT) {
        directory.emplace_back (source, & leader->entryMap);
    }

    ++ source;
    #endif
    size_t headerSize = parseRecordLeaderAndDirectory (start, directory);
    const char *source = start + headerSize;
    size_t dirIndex = 0;

    while ((source - start) < parsedLeader.recLength) {
        FieldControls *fieldControls = (FieldControls *) source;
        std::string fieldName;
        std::string arrayDesc;
        std::string fieldValue;
        int utCount = 0;
        char *curPtr;

        for (curPtr = (char *) (fieldControls + 1); *curPtr != FT; ++ curPtr) {
            if (*curPtr == UT) {
                utCount ++;
            } else if (utCount == 0) {
                fieldName += *curPtr;
            } else if (utCount == 1) {
                arrayDesc += *curPtr;
            } else {
                fieldValue += *curPtr;
            }
        }

        parseDdrField (fieldControls, directory [dirIndex].tag, fieldName, arrayDesc, fieldValue, directory, parsedLeader, fieldTree, dataDescriptiveFields);

        source = curPtr + 1;
        ++ dirIndex;
    }

    return parsedLeader.recLength;
}

void extractDatasetParameters (std::vector<std::vector<FieldInstance>>& records, DatasetParams& datasetParams) {
    bool found = false;

    for (auto& record: records) {
        for (auto& field: record) {
            if (field.tag.compare ("DSPM") == 0) {
                for (auto& subField: field.subFieldInstances) {
                    if (subField.first.compare ("COMF") == 0) {
                        if (subField.second.intValue.has_value ()) {
                            datasetParams.coordMultiplier = subField.second.intValue.value ();
                        }
                    } else if (subField.first.compare ("COMT") == 0) {
                        if (subField.second.stringValue.has_value ()) {
                            datasetParams.comment = subField.second.stringValue.value ();
                        }
                    } else if (subField.first.compare ("COUN") == 0) {
                        if (subField.second.intValue.has_value ()) {
                            datasetParams.coordUnit = (COUN) subField.second.intValue.value ();
                        }
                    } else if (subField.first.compare ("CSCL") == 0) {
                        if (subField.second.intValue.has_value ()) {
                            datasetParams.compilationScale = subField.second.intValue.value ();
                        }
                    } else if (subField.first.compare ("DUNI") == 0) {
                        if (subField.second.intValue.has_value ()) {
                            datasetParams.depthMeasurement = (DUNI) subField.second.intValue.value ();
                        }
                    } else if (subField.first.compare ("HDAT") == 0) {
                        if (subField.second.intValue.has_value ()) {
                            datasetParams.horDatum = (HDAT) subField.second.intValue.value ();
                        }
                    } else if (subField.first.compare ("HUNI") == 0) {
                        if (subField.second.intValue.has_value ()) {
                            datasetParams.heightMeasurement = (HUNI) subField.second.intValue.value ();
                        }
                    } else if (subField.first.compare ("PUNI") == 0) {
                        if (subField.second.intValue.has_value ()) {
                            datasetParams.posMeasurement = (PUNI) subField.second.intValue.value ();
                        }
                    } else if (subField.first.compare ("SDAT") == 0) {
                        if (subField.second.intValue.has_value ()) {
                            datasetParams.soundingDatum = subField.second.intValue.value ();
                        }
                    } else if (subField.first.compare ("SOMF") == 0) {
                        if (subField.second.intValue.has_value ()) {
                            datasetParams.soundingMultiplier = subField.second.intValue.value ();
                        }
                    } else if (subField.first.compare ("VDAT") == 0) {
                        if (subField.second.intValue.has_value ()) {
                            datasetParams.verDatum = subField.second.intValue.value ();
                        }
                    }
                }

                found = true; break;
            }
        }

        if (found) break;
    }
}

bool loadParseS57File (char *path, std::vector<std::vector<FieldInstance>>& records) {
    char *content = 0;
    size_t size = loadFile (path, content);
    bool result = false;

    if (size) {
        char *recStart = content;
        std::vector<std::pair<std::string, std::string>> fieldTree;
        std::map<std::string, DdfDesc> dataDescriptiveFields;
        size_t processedSize = 0;
        
        records.clear ();

        size_t ddrSize = parseDataDescriptiveRecord (recStart, fieldTree, dataDescriptiveFields);

        recStart += ddrSize;
        processedSize += ddrSize;

        while (processedSize < size) {
            auto& fieldInstances = records.emplace_back ();
            size_t drSize = parseDataRecord (recStart, dataDescriptiveFields, fieldInstances);

            recStart += drSize;
            processedSize += drSize;
        }
        
        result = true;
        free (content);
    }

    return result;
}

bool parseCatalog (const char *catPath, std::vector<CatalogItem>& catalog) {
    char *catalogContent = 0;
    size_t size = loadFile (catPath, catalogContent);
    bool result = false;

    catalog.clear ();

    if (size) {
        char *recStart = catalogContent;
        std::vector<std::pair<std::string, std::string>> fieldTree;
        std::map<std::string, DdfDesc> dataDescriptiveFields;
        std::vector<FieldInstance> fieldInstances;
        size_t processedSize = 0;
        
        size_t ddrSize = parseDataDescriptiveRecord (recStart, fieldTree, dataDescriptiveFields);

        recStart += ddrSize;
        processedSize += ddrSize;

        while (processedSize < size) {
            size_t drSize = parseDataRecord (recStart, dataDescriptiveFields, fieldInstances);

            auto& item = catalog.emplace_back ();
            auto& fields = fieldInstances [1];

            auto& rcid = fields.subFieldInstances.find ("RCID");
            if (rcid != fields.subFieldInstances.end () && rcid->second.intValue.has_value ()) {
                item.rcid = rcid->second.intValue.value ();
            }
            auto& file = fields.subFieldInstances.find ("FILE");
            if (file != fields.subFieldInstances.end () && file->second.stringValue.has_value ()) {
                item.fileName = file->second.stringValue.value ();
            }
            auto& volume = fields.subFieldInstances.find ("VOLM");
            if (volume != fields.subFieldInstances.end () && volume->second.stringValue.has_value ()) {
                item.volume = volume->second.stringValue.value ();
            }
            auto& impl = fields.subFieldInstances.find ("IMPL");
            if (impl != fields.subFieldInstances.end () && impl->second.stringValue.has_value ()) {
                item.binary = impl->second.stringValue.value ().compare ("BIN") == 0;
            }
            auto& southern = fields.subFieldInstances.find ("SLAT");
            if (southern != fields.subFieldInstances.end () && southern->second.floatValue.has_value ()) {
                item.southern = southern->second.floatValue.value ();
            }
            auto& northern = fields.subFieldInstances.find ("NLAT");
            if (northern != fields.subFieldInstances.end () && northern->second.floatValue.has_value ()) {
                item.northern = northern->second.floatValue.value ();
            }
            auto& western = fields.subFieldInstances.find ("WLON");
            if (western != fields.subFieldInstances.end () && western->second.floatValue.has_value ()) {
                item.western = western->second.floatValue.value ();
            }
            auto& eastern = fields.subFieldInstances.find ("ELON");
            if (eastern != fields.subFieldInstances.end () && eastern->second.floatValue.has_value ()) {
                item.eastern = eastern->second.floatValue.value ();
            }
            auto& crc = fields.subFieldInstances.find ("CRCS");
            if (crc != fields.subFieldInstances.end () && crc->second.stringValue.has_value ()) {
                item.crc = crc->second.stringValue.value ();
            }

            recStart += drSize;
            processedSize += drSize;
        }
        
        result = true;
        free (catalogContent);
    }

    return result;
}

std::string formatLat (double lat) {
    double absLat = fabs (lat);
    int deg = (int) absLat;
    double min = (absLat - (double) deg) * 60.0;
    char buffer [30];
    sprintf (buffer, "%02d %06.3f%C", deg, min, lat < 0.0 ? 'S' : 'N');

    return std::string (buffer);
}

std::string formatLon (double lon) {
    double absLon = fabs (lon);
    int deg = (int) absLon;
    double min = (absLon - (double) deg) * 60.0;
    char buffer [30];
    sprintf (buffer, "%03d %06.3f%C", deg, min, lon < 0.0 ? 'W' : 'E');

    return std::string (buffer);
}
