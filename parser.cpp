#include <stdlib.h>
#include <stdio.h>
#include <cstdint>
#include <vector>
#include <map>
#include <string>
#include <tuple>
#include "s57defs.h"
#include "data.h"

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

size_t splitText (std::string source, std::vector<std::string>& lines) {
    lines.clear ();

    if (source.empty ()) return 0;

    lines.emplace_back ();

    for (char chr: source) {
        switch (chr) {
            case 13:
                lines.emplace_back ();
            case 10:
                break;
            default:
                lines.back () += chr;
        }
    }

    return lines.size ();
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

size_t loadFileAndConvertToAnsi (const char *path, char *& content) {
    size_t size = loadFile (path, content);

    if (size && content) {
        if (content [0] == -1 && content [1] == -2) {
            std::string ansi;

            for (wchar_t *chr = (wchar_t *) content + 2; *chr; ++ chr) {
                ansi += (char) (*chr & 255);
            }

            strcpy (content, ansi.c_str ());
        }
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
    if (!fld.modifier.has_value () && (*fieldPos == FT || *fieldPos == UT)) {
        fieldPos ++; return std::tuple (false, std::string ());
    }

    std::string value;

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

bool getBinaryValue (RecordFieldDesc& fld, const char *& fieldPos, std::vector<uint8_t>& binary) {
    if (!fld.modifier.has_value () && (*fieldPos == FT || *fieldPos == UT)) {
        fieldPos ++; return false;
    }

    binary.clear ();

    if (fld.modifier.has_value ()) {
        size_t size = fld.modifier.value () / 8;
        binary.insert (binary.end (), fieldPos, fieldPos + size);
        fieldPos += size;
    } else {
        size_t k;
        for (k = 0; isdigit (fieldPos [k]); ++ k) {
            binary.insert (binary.end (), fieldPos [k]);
        }
        fieldPos += k + 1;
    }
    
    return true;
}

std::tuple<bool, uint32_t> getIntValue (RecordFieldDesc& fld, const char *& fieldPos) {
    if (!fld.modifier.has_value () && (*fieldPos == FT || *fieldPos == UT)) {
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
    if (!fld.modifier.has_value () && (*fieldPos == FT || *fieldPos == UT)) {
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
    if (!fld.modifier.has_value () && (*fieldPos == FT || *fieldPos == UT)) {
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
        const char *beginPos = fieldPos;
        auto& fieldInstance = fieldInstances.emplace_back ();

        fieldInstance.name = ddf.name;
        fieldInstance.tag = dirEntry.tag;

        while ((fieldPos - beginPos + 1) < dirEntry.length) {
            auto& fieldValueInstance = fieldInstance.instanceValues.emplace_back ();

            for (auto j = 0; j < ddf.fields.size (); ++ j) {
                auto& fld = ddf.fields [j];
                auto& subFieldInstance = fieldValueInstance.emplace (fld.tag, SubFieldInstance ()).first;

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
                    case 'B': {
                        bool hasValue = getBinaryValue (fld, fieldPos, subFieldInstance->second.binaryValue);
                        break;
                    }
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

void deformatAttrValues (AttrDictionary& attrDictionary, Features& features) {
    for (size_t i = 0; i < features.size (); ++ i) {
        auto& feature = features [i];
        for (auto& attr: feature.attributes) {
            if (!attr.noValue && !attr.strValue.empty ()) {
                AttrDesc *attrDesc = (AttrDesc *) attrDictionary.findByCode (attr.classCode);

                switch (attrDesc->domain) {
                    case 'L': {
                        std::vector<std::string> parts;

                        attr.listValue.clear ();

                        splitString (attr.strValue, parts, ',');

                        for (std::string& part: parts) {
                            attr.listValue.push_back (std::atoi (part.c_str ()));
                        }
                        break;
                    }
                    case 'E':
                    case 'I': {
                        if (!attr.intValue) attr.intValue = std::atoi (attr.strValue.c_str ());
                        break;
                    }
                    case 'F': {
                        attr.floatValue = std::atof (attr.strValue.c_str ()); break;
                    }
                }
            }
        }
    }
}

void extractEdges (std::vector<std::vector<FieldInstance>>& records, Edges& edges, Nodes& points, DatasetParams datasetParams) {
    edges.clear ();

    for (auto& record: records) {
        bool goToNextRec = false;
        
        for (auto& field: record) {
            if (goToNextRec) break;

            auto& firstFieldValue = field.instanceValues.front ();
            uint32_t rcid = 0;
            uint8_t recName = 0;
            uint8_t updateInstr = 0;
            uint8_t version = 0;

            if (field.tag.compare ("VRID") == 0) {
                // possible new edge
                rcid = 0;
                recName = 0;
                updateInstr = 0;
                version = 0;

                for (auto& subField: firstFieldValue) {
                    if (subField.first.compare ("RCID") == 0) {
                        if (subField.second.intValue.has_value ()) {
                            rcid = subField.second.intValue.value ();
                        }
                    } else if (subField.first.compare ("RCNM") == 0) {
                        if (subField.second.intValue.has_value ()) {
                            recName = subField.second.intValue.value ();
                            // skip record if not an edge
                            if (recName != RCNM::Edge) {
                                goToNextRec = true; break;
                            }
                        }
                    } else if (subField.first.compare ("RUIN") == 0) {
                        if (subField.second.intValue.has_value ()) {
                            updateInstr = subField.second.intValue.value ();
                        }
                    } else if (subField.first.compare ("RVER") == 0) {
                        if (subField.second.intValue.has_value ()) {
                            version = subField.second.intValue.value ();
                        }
                    }
                }
                if (goToNextRec) break;
                if (rcid && recName) {
                    GeoEdge& edge = edges.emplace_back ();
                    edge.id = rcid;
                    edge.recordName = recName;
                    edge.updateInstruction = updateInstr;
                    edge.version = version;
                }
            } else if (field.tag.compare ("SG2D") == 0) {
                // New internal node
                GeoEdge& edge = edges.back ();
                for (auto& instanceValue: field.instanceValues) {
                    for (auto& subField: instanceValue) {
                        if (subField.first.compare ("*YCOO") == 0) {
                            auto& point = edge.internalNodes.emplace_back ();
                            if (subField.second.intValue.has_value () && datasetParams.coordMultiplier) {
                                point.lat = (double) (int32_t) subField.second.intValue.value () / (double) datasetParams.coordMultiplier.value ();
                            }
                        } else if (subField.first.compare ("XCOO") == 0) {
                            auto& point = edge.internalNodes.back ();
                            if (subField.second.intValue.has_value ()) {
                                point.lon = (double) (int32_t) subField.second.intValue.value () / (double) datasetParams.coordMultiplier.value ();
                            }
                        }
                    }
                }
            } else if (field.tag.compare ("VRPT") == 0) {
                for (auto& instanceValue: field.instanceValues) {
                    std::optional<uint8_t> mask;
                    std::optional<uint8_t> orient;
                    std::optional<uint8_t> topology;
                    std::optional<uint8_t> usage;
                    std::optional<uint64_t> foreignIndex;

                    for (auto& subField: instanceValue) {
                        // New begin/end node
                        if (subField.first.compare ("*NAME") == 0) {
                            if (subField.second.binaryValue.size () > 0) {
                                foreignIndex = constructForeignKey (subField.second.binaryValue.data ());
                            }
                        } else if (subField.first.compare ("MASK") == 0) {
                            if (subField.second.intValue.has_value ()) {
                                mask = (uint8_t) subField.second.intValue.value ();
                            }
                        } else if (subField.first.compare ("ORNT") == 0) {
                            if (subField.second.intValue.has_value ()) {
                                orient = (uint8_t) subField.second.intValue.value ();
                            }
                        } else if (subField.first.compare ("TOPI") == 0) {
                            if (subField.second.intValue.has_value ()) {
                                topology = (uint8_t) subField.second.intValue.value ();
                            }
                        } else if (subField.first.compare ("USAG") == 0) {
                            if (subField.second.intValue.has_value ()) {
                                usage = (uint8_t) subField.second.intValue.value ();
                            }
                        }
                    }
                    if (foreignIndex.has_value ()) {
                        size_t nodeIndex = points.getIndexByForgeignKey (foreignIndex.value ());

                        if (nodeIndex >= 0 && topology.has_value ()) {
                            GeoEdge& edge = edges.back ();
                            switch (topology.value ()) {
                                case TOPI::BeginningNode:
                                    edge.beginIndex = nodeIndex; break;
                                case TOPI::EndNode:
                                    edge.endIndex = nodeIndex; break;
                                default:
                                    continue; // unknown edge type
                            }
                        }
                    }
                }
            }
        }
    }
    edges.buildIndex ();
}

void extractNodes (std::vector<std::vector<FieldInstance>>& records, Nodes& points, DatasetParams datasetParams) {
    points.clear ();

    for (auto& record: records) {
        bool goToNextRec = false;
        
        for (auto& field: record) {
            if (goToNextRec) break;

            auto& firstFieldValue = field.instanceValues.front ();
            uint32_t rcid = 0;
            uint8_t recName = 0;
            uint8_t updateInstr = 0;
            uint8_t version = 0;

            if (field.tag.compare ("VRID") == 0) {
                // possible new node
                rcid = 0;
                recName = 0;
                updateInstr = 0;
                version = 0;
                for (auto& subField: firstFieldValue) {
                    if (subField.first.compare ("RCID") == 0) {
                        if (subField.second.intValue.has_value ()) {
                            rcid = subField.second.intValue.value ();
                        }
                    } else if (subField.first.compare ("RCNM") == 0) {
                        if (subField.second.intValue.has_value ()) {
                            recName = subField.second.intValue.value ();

                            // skip record if not an edge
                            if (recName != RCNM::ConnectedNode && recName != RCNM::IsolatedNode) {
                                goToNextRec = true; break;
                            }
                        }
                    } else if (subField.first.compare ("RUIN") == 0) {
                        if (subField.second.intValue.has_value ()) {
                            updateInstr = subField.second.intValue.value ();
                        }
                    } else if (subField.first.compare ("RVER") == 0) {
                        if (subField.second.intValue.has_value ()) {
                            version = subField.second.intValue.value ();
                        }
                    }
                }
                if (goToNextRec) break;
                if (rcid && recName) {
                    GeoNode& point = points.emplace_back ();
                    point.id = rcid;
                    point.recordName = recName;
                    point.updateInstruction = updateInstr;
                    point.version = version;
                }
            } else if (field.tag.compare ("SG2D") == 0) {
                GeoNode& curPoint = points.back ();
                for (auto& subField: firstFieldValue) {
                    if (subField.first.compare ("*YCOO") == 0) {
                        auto& point = curPoint.points.emplace_back ();
                        if (subField.second.intValue.has_value () && datasetParams.coordMultiplier) {
                            point.lat = (double) (int32_t) subField.second.intValue.value () / (double) datasetParams.coordMultiplier.value ();
                        }
                    } else if (subField.first.compare ("XCOO") == 0) {
                        auto& point = curPoint.points.back ();
                        if (subField.second.intValue.has_value ()) {
                            point.lon = (double) (int32_t) subField.second.intValue.value () / (double) datasetParams.coordMultiplier.value ();
                        }
                    }
                }
            } else if (field.tag.compare ("SG3D") == 0) {
                GeoNode& curPoint = points.back ();
                std::vector<Position> soundings;
                curPoint.flags |= NodeFlags::SOUNDING_ARRAY;
                for (auto& instanceValue: field.instanceValues) {
                    for (auto& subField: instanceValue) {
                        if (subField.first.compare ("*YCOO") == 0) {
                            auto& point = soundings.emplace_back ();
                            if (subField.second.intValue.has_value () && datasetParams.coordMultiplier) {
                                point.lat = (double) (int32_t) subField.second.intValue.value () / (double) datasetParams.coordMultiplier.value ();
                            }
                        } else if (subField.first.compare ("VE3D") == 0 && datasetParams.soundingMultiplier) {
                            auto& point = soundings.back ();
                            if (subField.second.intValue.has_value ()) {
                                point.depth = (double) (int32_t) subField.second.intValue.value () / (double) datasetParams.soundingMultiplier.value ();

                                if (datasetParams.depthMeasurement.has_value ()) {
                                    switch (datasetParams.depthMeasurement.value ()) {
                                        case DUNI::DepthFeet: {
                                            point.depth *= 0.3048; break;
                                        }
                                        case DUNI::DepthFathomsFractions: {
                                            point.depth *= 6.0 * 0.3048; break;
                                        }
                                        case DUNI::DepthFathomsFeet:
                                        {
                                            double roundedDepth = round (point.depth);
                                            double fractionalDepth = point.depth - roundedDepth;
                                            point.depth = roundedDepth * 6.0 * 0.3048 + fractionalDepth * 0.3048;
                                            break;
                                        }
                                    }
                                }
                            }
                        } else if (subField.first.compare ("XCOO") == 0 && datasetParams.coordMultiplier) {
                            auto& point = soundings.back ();
                            if (subField.second.intValue.has_value ()) {
                                point.lon = (double) (int32_t) subField.second.intValue.value () / (double) datasetParams.coordMultiplier.value ();
                            }
                        }
                    }
                }
                if (soundings.size () > 0) {
                    curPoint.points.insert (curPoint.points.begin (), soundings.begin (), soundings.end ());
                }
                /*for (auto& subField: firstFieldValue) {
                    if (subField.first.compare ("AGEN") == 0) {
                        if (subField.second.intValue.has_value ()) {
                            curFeature->agency = subField.second.intValue.value ();
                        }
                    } else if (subField.first.compare ("FIDN") == 0) {
                        if (subField.second.intValue.has_value ()) {
                            curFeature->featureID = subField.second.intValue.value ();
                        }
                    } else if (subField.first.compare ("FIDS") == 0) {
                        if (subField.second.intValue.has_value ()) {
                            curFeature->featureSubdiv = subField.second.intValue.value ();
                        }
                    }
                }*/
            } /*else if (field.tag.compare ("ATTF") == 0) {
                for (auto& subField: firstFieldValue) {
                    if (subField.first.compare ("*ATTL") == 0) {
                        if (subField.second.intValue.has_value ()) {
                            curFeature->attributes.emplace_back ();
                            curFeature->attributes.back ().classCode = subField.second.intValue.value ();
                        }
                    } else if (subField.first.compare ("ATVL") == 0) {
                        curFeature->attributes.back ().noValue = false;
                        if (subField.second.intValue.has_value ()) {
                            curFeature->attributes.back ().intValue = subField.second.intValue.value ();
                        } else if (subField.second.floatValue.has_value ()) {
                            curFeature->attributes.back ().floatValue = subField.second.floatValue.value ();
                        } else if (subField.second.stringValue.has_value ()) {
                            curFeature->attributes.back ().strValue = subField.second.stringValue.value ();
                        } else if (subField.second.binaryValue.size () > 0) {
                            curFeature->attributes.back ().listValue.insert (
                                curFeature->attributes.back ().listValue.end (),
                                subField.second.binaryValue.begin (),
                                subField.second.binaryValue.end ()
                            );
                        } else {
                            curFeature->attributes.back ().noValue = true;
                        }
                    }
                }
            }*/
        }
    }
    points.buildIndex ();
}

void extractFeatureObjects (std::vector<std::vector<FieldInstance>>& records, Features& features, Edges& edges, Nodes& nodes) {
    features.clear ();

    for (auto& record: records) {
        bool goToNextRec = false;
        
        for (auto& field: record) {
            if (goToNextRec) break;

            auto& firstFieldValue = field.instanceValues.front ();

            uint32_t rcid = 0;
            uint8_t recName = 0;
            uint8_t updateInstr = 0;
            uint8_t version = 0;
            uint16_t classCode = 0;
            uint8_t group = 0;
            uint8_t primitive = 0;

            if (field.tag.compare ("FRID") == 0) {
                rcid = 0;
                recName = 0;
                updateInstr = 0;
                version = 0;
                classCode = 0;
                group = 0;
                primitive = 0;

                //curFeature = & objects.emplace_back ();
                for (auto& subField: firstFieldValue) {
                    if (subField.first.compare ("GRUP") == 0) {
                        if (subField.second.intValue.has_value ()) {
                            group = subField.second.intValue.value ();
                        }
                    } else if (subField.first.compare ("OBJL") == 0) {
                        if (subField.second.intValue.has_value ()) {
                            classCode = subField.second.intValue.value ();
                        }
                    } else if (subField.first.compare ("PRIM") == 0) {
                        if (subField.second.intValue.has_value ()) {
                            primitive = subField.second.intValue.value ();
                        }
                    } else if (subField.first.compare ("RCID") == 0) {
                        if (subField.second.intValue.has_value ()) {
                            rcid = subField.second.intValue.value ();
                        }
                    } else if (subField.first.compare ("RCNM") == 0) {
                        if (subField.second.intValue.has_value ()) {
                            recName = subField.second.intValue.value ();

                            if (recName != RCNM::Feature) {
                                goToNextRec = true; break;
                            }
                        }
                    } else if (subField.first.compare ("RUIN") == 0) {
                        if (subField.second.intValue.has_value ()) {
                            updateInstr = subField.second.intValue.value ();
                        }
                    } else if (subField.first.compare ("RVER") == 0) {
                        if (subField.second.intValue.has_value ()) {
                            version = subField.second.intValue.value ();
                        }
                    }
                }
                if (goToNextRec) break;
                if (rcid && recName) {
                    FeatureObject& feature = features.emplace_back ();
                    feature.id = rcid;
                    feature.recordName = recName;
                    feature.updateInstruction = updateInstr;
                    feature.version = version;
                    feature.group = group;
                    feature.classCode = classCode;
                    feature.primitive = primitive;
                }
            } else if (field.tag.compare ("FOID") == 0) {
                for (auto& subField: firstFieldValue) {
                    if (subField.first.compare ("AGEN") == 0) {
                        if (subField.second.intValue.has_value ()) {
                            features.back ().agency = subField.second.intValue.value ();
                        }
                    } else if (subField.first.compare ("FIDN") == 0) {
                        if (subField.second.intValue.has_value ()) {
                            features.back ().fidn = subField.second.intValue.value ();
                        }
                    } else if (subField.first.compare ("FIDS") == 0) {
                        if (subField.second.intValue.has_value ()) {
                            features.back ().subDiv = subField.second.intValue.value ();
                        }
                    }
                }
            } else if (field.tag.compare ("FSPT") == 0) {
                std::optional<uint8_t> mask;
                std::optional<uint8_t> orient;
                std::optional<uint8_t> usage;
                std::optional<uint64_t> foreignIndex;

                auto checkSubField = [&mask, &orient, &usage, &foreignIndex] (std::pair<const std::string, SubFieldInstance> &subField) {
                    if (subField.first.compare ("*NAME") == 0) {
                        if (subField.second.binaryValue.size () > 0) {
                            foreignIndex = constructForeignKey (subField.second.binaryValue.data ());
                        }
                    } else if (subField.first.compare ("MASK") == 0) {
                        if (subField.second.intValue.has_value ()) {
                            mask = (uint8_t) subField.second.intValue.value ();
                        }
                    } else if (subField.first.compare ("ORNT") == 0) {
                        if (subField.second.intValue.has_value ()) {
                            orient = (uint8_t) subField.second.intValue.value ();
                        }
                    } else if (subField.first.compare ("USAG") == 0) {
                        if (subField.second.intValue.has_value ()) {
                            usage = (uint8_t) subField.second.intValue.value ();
                        }
                    }                        
                };

                switch (features.back ().primitive) {
                    case PRIM::Point: {
                        for (auto& subField: firstFieldValue) {
                            checkSubField (subField);
                        }

                        if (foreignIndex.has_value ()) {
                            size_t nodeIndex = nodes.getIndexByForgeignKey (foreignIndex.value ());

                            if (nodeIndex >= 0) {
                                features.back ().nodeIndex = nodeIndex;
                            }
                        }
                        break;
                    }
                    case PRIM::Line:
                    case PRIM::Area: {
                        for (auto& instanceValue: field.instanceValues) {
                            mask.reset ();
                            orient.reset ();
                            usage.reset ();
                            foreignIndex.reset ();

                            for (auto& subField: instanceValue) {
                                checkSubField (subField);
                            }

                            if (foreignIndex.has_value ()) {
                                size_t edgeIndex = edges.getIndexByForgeignKey (foreignIndex.value ());

                                if (edgeIndex >= 0) {
                                    auto& edgeRef = features.back ().edgeRefs.emplace_back ();
                                    edgeRef.index = edgeIndex;
                                    edgeRef.hidden = mask == 1;
                                    edgeRef.hole = usage == USAG::Interior;
                                    edgeRef.unclockwise = orient == ORNT::Reverse;
                                }
                            }
                        }
                        break;
                    }
                }
            } else if (field.tag.compare ("ATTF") == 0) {
                for (auto& instanceValue: field.instanceValues) {
                    for (auto& subField: instanceValue) {
                        if (subField.first.compare ("*ATTL") == 0) {
                            if (subField.second.intValue.has_value ()) {
                                features.back ().attributes.emplace_back ().classCode = subField.second.intValue.value ();
                            }
                        } else if (subField.first.compare ("ATVL") == 0) {
                            auto& attr = features.back ().attributes.back ();
                            attr.noValue = false;
                            if (subField.second.intValue.has_value ()) {
                                attr.intValue = subField.second.intValue.value ();
                            } else if (subField.second.floatValue.has_value ()) {
                                attr.floatValue = subField.second.floatValue.value ();
                            } else if (subField.second.stringValue.has_value ()) {
                                attr.strValue = subField.second.stringValue.value ();
                            } else if (subField.second.binaryValue.size () > 0) {
                                attr.listValue.insert (
                                    attr.listValue.end (),
                                    subField.second.binaryValue.begin (),
                                    subField.second.binaryValue.end ()
                                );
                            } else {
                                attr.noValue = true;
                            }
                        }
                    }
                }
            }
        }
    }
    features.buildIndex ();
}

void extractFeatureObjects (std::vector<std::vector<FieldInstance>>& records, std::vector<FeatureDesc>& objects) {
    FeatureDesc *curFeature = 0;

    objects.clear ();

    for (auto& record: records) {
        for (auto& field: record) {
            auto& firstFieldValue = field.instanceValues.front ();

            if (field.tag.compare ("FRID") == 0) {
                curFeature = & objects.emplace_back ();
                for (auto& subField: firstFieldValue) {
                    if (subField.first.compare ("GRUP") == 0) {
                        if (subField.second.intValue.has_value ()) {
                            curFeature->group = subField.second.intValue.value ();
                        }
                    } else if (subField.first.compare ("OBJL") == 0) {
                        if (subField.second.intValue.has_value ()) {
                            curFeature->classCode = subField.second.intValue.value ();
                        }
                    } else if (subField.first.compare ("PRIM") == 0) {
                        if (subField.second.intValue.has_value ()) {
                            curFeature->geometry = subField.second.intValue.value ();
                        }
                    } else if (subField.first.compare ("RCID") == 0) {
                        if (subField.second.intValue.has_value ()) {
                            curFeature->id = subField.second.intValue.value ();
                        }
                    } else if (subField.first.compare ("RCNM") == 0) {
                        if (subField.second.intValue.has_value ()) {
                            curFeature->recordName = subField.second.intValue.value ();
                        }
                    } else if (subField.first.compare ("RUIN") == 0) {
                        if (subField.second.intValue.has_value ()) {
                            curFeature->updateInstruction = subField.second.intValue.value ();
                        }
                    } else if (subField.first.compare ("RVER") == 0) {
                        if (subField.second.intValue.has_value ()) {
                            curFeature->version = subField.second.intValue.value ();
                        }
                    }
                }
            } else if (field.tag.compare ("FOID") == 0) {
                for (auto& subField: firstFieldValue) {
                    if (subField.first.compare ("AGEN") == 0) {
                        if (subField.second.intValue.has_value ()) {
                            curFeature->agency = subField.second.intValue.value ();
                        }
                    } else if (subField.first.compare ("FIDN") == 0) {
                        if (subField.second.intValue.has_value ()) {
                            curFeature->featureID = subField.second.intValue.value ();
                        }
                    } else if (subField.first.compare ("FIDS") == 0) {
                        if (subField.second.intValue.has_value ()) {
                            curFeature->featureSubdiv = subField.second.intValue.value ();
                        }
                    }
                }
            } else if (field.tag.compare ("ATTF") == 0) {
                for (auto& subField: firstFieldValue) {
                    if (subField.first.compare ("*ATTL") == 0) {
                        if (subField.second.intValue.has_value ()) {
                            curFeature->attributes.emplace_back ();
                            curFeature->attributes.back ().classCode = subField.second.intValue.value ();
                        }
                    } else if (subField.first.compare ("ATVL") == 0) {
                        curFeature->attributes.back ().noValue = false;
                        if (subField.second.intValue.has_value ()) {
                            curFeature->attributes.back ().intValue = subField.second.intValue.value ();
                        } else if (subField.second.floatValue.has_value ()) {
                            curFeature->attributes.back ().floatValue = subField.second.floatValue.value ();
                        } else if (subField.second.stringValue.has_value ()) {
                            curFeature->attributes.back ().strValue = subField.second.stringValue.value ();
                        } else if (subField.second.binaryValue.size () > 0) {
                            curFeature->attributes.back ().listValue.insert (
                                curFeature->attributes.back ().listValue.end (),
                                subField.second.binaryValue.begin (),
                                subField.second.binaryValue.end ()
                            );
                        } else {
                            curFeature->attributes.back ().noValue = true;
                        }
                    }
                }
            }
        }
    }
}

void extractDatasetParameters (std::vector<std::vector<FieldInstance>>& records, DatasetParams& datasetParams) {
    bool found = false;

    for (auto& record: records) {
        for (auto& field: record) {
            auto& firstFieldValue = field.instanceValues.front ();

            if (field.tag.compare ("DSPM") == 0) {
                for (auto& subField: firstFieldValue) {
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
            auto& firstFieldValue = fields.instanceValues.front ();

            auto& rcid = firstFieldValue.find ("RCID");
            if (rcid != firstFieldValue.end () && rcid->second.intValue.has_value ()) {
                item.rcid = rcid->second.intValue.value ();
            }
            auto& file = firstFieldValue.find ("FILE");
            if (file != firstFieldValue.end () && file->second.stringValue.has_value ()) {
                item.fileName = file->second.stringValue.value ();
            }
            auto& volume = firstFieldValue.find ("VOLM");
            if (volume != firstFieldValue.end () && volume->second.stringValue.has_value ()) {
                item.volume = volume->second.stringValue.value ();
            }
            auto& impl = firstFieldValue.find ("IMPL");
            if (impl != firstFieldValue.end () && impl->second.stringValue.has_value ()) {
                item.binary = impl->second.stringValue.value ().compare ("BIN") == 0;
            }
            auto& southern = firstFieldValue.find ("SLAT");
            if (southern != firstFieldValue.end () && southern->second.floatValue.has_value ()) {
                item.southern = southern->second.floatValue.value ();
            }
            auto& northern = firstFieldValue.find ("NLAT");
            if (northern != firstFieldValue.end () && northern->second.floatValue.has_value ()) {
                item.northern = northern->second.floatValue.value ();
            }
            auto& western = firstFieldValue.find ("WLON");
            if (western != firstFieldValue.end () && western->second.floatValue.has_value ()) {
                item.western = western->second.floatValue.value ();
            }
            auto& eastern = firstFieldValue.find ("ELON");
            if (eastern != firstFieldValue.end () && eastern->second.floatValue.has_value ()) {
                item.eastern = eastern->second.floatValue.value ();
            }
            auto& crc = firstFieldValue.find ("CRCS");
            if (crc != firstFieldValue.end () && crc->second.stringValue.has_value ()) {
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

void loadAttrDictionary (const char *path, AttrDictionary& dictionary) {
    dictionary.clear ();

    char *content = 0;
    size_t size = loadFileAndConvertToAnsi (path, content);

    if (size && content) {
        std::vector<std::string> lines;

        splitText (content, lines);
        free (content);

        for (auto& line: lines) {
            if (line [0] == '#') {
                // New attr
                std::vector<std::string> parts;

                splitString (line.substr (1), parts, ',');

                if (parts.size () > 4) {
                    std::string acronym = parts [0];
                    uint16_t code = std::atoi (parts [1].c_str ());
                    char domain = parts [3][0];
                    std::string name = parts [4];

                    dictionary.checkAddAttr (code, domain, acronym.c_str (), name.c_str ());
                }
            } else if (dictionary.items.size () > 0) {
                if (dictionary.lastItem ().domain == 'E' || dictionary.lastItem ().domain == 'L') {
                    std::vector<std::string> fields;
                    splitString (line, fields, ':');
                    if (fields.size () > 1) {
                        dictionary.lastItem ().list.emplace (std::atoi (fields [0].c_str ()), fields [1]);
                    }
                }
            }
        }
    }
}

void loadObjectDictionary (const char *path, ObjectDictionary& dictionary) {
    dictionary.clear ();

    char *content = 0;
    size_t size = loadFileAndConvertToAnsi (path, content);

    if (size && content) {
        std::vector<std::string> lines;

        splitText (content, lines);
        free (content);

        for (auto& line: lines) {
            if (line [0] == '#') {
                // New object
                std::vector<std::string> parts;

                splitString (line.substr (1), parts, ',');

                if (parts.size () > 3) {
                    std::string acronym = parts [0];
                    uint16_t code = std::atoi (parts [1].c_str ());
                    std::string name = parts [3];

                    if (parts.size () > 4) {
                        name += ',';
                        name += parts [4];
                    }

                    dictionary.checkAddObject (code, acronym.c_str (), name.c_str ());
                }
            }
        }
    }
}

std::string extractToUnitTerm (char *&source) {
    std::string result;

    while (*source && (*source != UT)) {
        result += *(source++);
    }

    source++; return result;
}

std::string extractFixedSize (char *&source, size_t size) {
    std::string result;

    result.append (source, size);

    source += size; return result;
}

void loadLibraryId (std::vector<std::string>& module, LibraryIdentification& libraryId) {
    char *source = (char *) module [1].c_str () + 9;

    libraryId.moduleName = extractFixedSize (source, 2);
    libraryId.rcid = std::atol (extractFixedSize (source, 5).c_str ());
    libraryId.exchangePurpose = extractFixedSize (source, 3);
    libraryId.productType = extractToUnitTerm (source);
    libraryId.exchangeSetSerialNo = extractToUnitTerm (source);
    libraryId.editionNo = extractToUnitTerm (source);
    libraryId.compilationDate = extractFixedSize (source, 8);
    libraryId.compilationTime = extractFixedSize (source, 6);
    libraryId.libraryProfileVersionsDate = extractFixedSize (source, 8);
    libraryId.libaryAppProfile = extractFixedSize (source, 2);
    libraryId.objectCatVersionDate = extractFixedSize (source, 8);
    libraryId.comment = extractToUnitTerm (source);
}
/*
void loadColorTable (std::vector<std::string>& module, ColorTable& colorTable) {
    return;
    colorTable.clear ();

    for (auto& line: module) {
        char *source = (char *) line.c_str ();

        if (memcmp (source, "CCIE", 4) == 0) {
            source += 9;
            std::string colorCode = extractFixedSize (source, 5);
            double x = std::atof (extractToUnitTerm (source).c_str ());
            double y = std::atof (extractToUnitTerm (source).c_str ());
            double z = std::atof (extractToUnitTerm (source).c_str ());
            std::string colorName = extractToUnitTerm (source);

            colorTable.emplace (colorCode, ColorItem (colorName.c_str (), x, y, z));
        }
    }
}
*/
void processInstructions (Dai& dai, LookupTableItem& item, std::vector<std::string>& instructions) {
    auto extractInstr = [] (std::string& instruction) {
        size_t leftBracketPos = instruction.find ('(', 2);
        size_t rightBracketPos = leftBracketPos == std::string::npos ? std::string::npos : instruction.find (')', leftBracketPos + 1);
        return std::string (rightBracketPos == std::string::npos ? "" : instruction.substr (leftBracketPos + 1, rightBracketPos - leftBracketPos - 1));
    };

    for (auto& instruction: instructions) {
        if (instruction [0] == 'L' && instruction [1] == 'S') {
            item.penIndex = dai.palette.checkPen (instruction.data (), dai.colorTable);
        } else if (instruction [0] == 'S' && instruction [1] == 'Y') {
            auto symbolName = extractInstr (instruction);

            if (!symbolName.empty ()) {
                size_t index = dai.getSymbolIndex (symbolName.c_str ());
                if (index != LookupTableItem::NOT_EXIST) item.symbols.emplace_back (index);
            }
        } else if (instruction [0] == 'A' && instruction [1] == 'C') {
            auto colorName = extractInstr (instruction);

            if (!colorName.empty ()) item.brushIndex = dai.palette.checkSolidBrush (colorName.c_str (), dai.colorTable);
        }
    }
}

void loadLookupTableItem (
    std::vector<std::string>& module,
    Dai& dai, 
    ObjectDictionary& objectDictionary,
    AttrDictionary& attrDictionary
) {
    size_t index = -1;
    LookupTableItem item;

    for (auto& line: module) {
        char *source = (char *) line.c_str ();

        if (memcmp (source, "LUPT", 4) == 0) {
            source += 9;
            std::string moduleName = extractFixedSize (source, 2);
            uint32_t rcid = std::atoi (extractFixedSize (source, 5).c_str ());
            std::string status = extractFixedSize (source, 3);
            std::string acronym = extractFixedSize (source, 6);
            ObjectDesc *objectDesc = objectDictionary.findByAcronym (acronym.c_str ());
            std::string objType = extractFixedSize (source, 1);
            uint32_t displayPriority = std::atoi (extractFixedSize (source, 5).c_str ());
            std::string radarPriority = extractFixedSize (source, 1);
            std::string tableSet = extractToUnitTerm (source);

            memcpy (item.acronym, acronym.c_str (), 6);
            item.displayPriority = displayPriority;
            item.radarPriority = radarPriority [0];
            item.objectType = objType [0];
            item.attrCombination.clear ();
            item.classCode = objectDesc ? objectDesc->code : 0;
            item.comment.clear ();
            item.instruction.clear ();
            item.brushIndex = item.penIndex = LookupTableItem::NOT_EXIST;
            
            if (tableSet.compare ("PLAIN_BOUNDARIES") == 0) {
                item.tableSet = TableSet::PLAIN_BOUNDARIES;
            } else if (tableSet.compare ("SYMBOLIZED_BOUNDARIES") == 0) {
                item.tableSet = TableSet::SYMBOLIZED_BOUNDARIES;
            } else if (tableSet.compare ("SIMPLIFIED") == 0) {
                item.tableSet = TableSet::SIMPLIFIED;
            } else if (tableSet.compare ("PAPER_CHART") == 0) {
                item.tableSet = TableSet::PAPER_CHARTS;
            } else if (tableSet.compare ("LINES") == 0) {
                item.tableSet = TableSet::LINES;
            }
        } else if (memcmp (source, "ATTC", 4) == 0) {
            if (index >= 0) {
                source += 9;

                std::string acronym;
                std::string value;

                if (*source == UT) {
                    // No-attribute instance (general default rule)
                } else {
                    while (*source && *source != UT) {
                        acronym = extractFixedSize (source, 6);
                        value = extractToUnitTerm (source);

                        if (!acronym.empty ()) {
                            auto& instance = item.attrCombination.emplace_back ();

                            AttrDesc *desc = (AttrDesc *) attrDictionary.findByAcronym (acronym.c_str ());

                            instance.acronym = acronym;
                            instance.strValue = value;
                            instance.domain = desc->domain;

                            if (value [0] == '?') instance.missing = true;

                            switch (instance.domain) {
                                case 'E':
                                case 'I':
                                    instance.intValue = std::atoi (value.c_str ()); break;
                                case 'F':
                                    instance.floatValue = std::atof (value.c_str ()); break;
                            }

                            if (desc) instance.classCode = desc->code;
                        }
                    }
                }
            }
        } else if (memcmp (source, "INST", 4) == 0) {
            source += 9;
            item.instruction = extractToUnitTerm (source);
            //std::vector<std::string> parts;
            //splitString (item.instruction, parts, ';');
            //processInstructions (dai, item, parts);
        } else if (memcmp (source, "DISC", 4) == 0) {
            source += 9;
            std::string displayCat = extractToUnitTerm (source);
            
            if (displayCat.compare ("STANDARD") == 0) {
                item.displayCat = DisplayCat::STANDARD;
            } else if (displayCat.compare ("DISPLAY_BASE") == 0) {
                item.displayCat = DisplayCat::DISPLAY_BASE;
            }
        } else if (memcmp (source, "LUCM", 4) == 0) {
            source += 9;
            item.comment = extractToUnitTerm (source);

            // Item has been completed; need to add into table set
            uint32_t key = item.composeKey ();
            auto pos = dai.lookupTableIndex.find (key);

            if (pos == dai.lookupTableIndex.end ()) {
                // Absolutely new item group, add item
                pos = dai.lookupTableIndex.emplace (key, dai.lookupTables.size ()).first;
                dai.lookupTables.emplace_back ();
            }

            dai.lookupTables [pos->second].emplace_back ().copyFrom (item);
        }
    }
}
/*
void loadLookupTableItem (std::vector<std::string>& module, std::map<std::string, std::vector<LookupTableItem>>& lookupTables) {
    std::map<std::string, std::vector<LookupTableItem>>::iterator lookupTablePos = lookupTables.end ();

    for (auto& line: module) {
        char *source = (char *) line.c_str ();

        if (memcmp (source, "LUPT", 4) == 0) {
            source += 9;
            std::string moduleName = extractFixedSize (source, 2);
            uint32_t rcid = std::atoi (extractFixedSize (source, 5).c_str ());
            std::string status = extractFixedSize (source, 3);
            std::string acronym = extractFixedSize (source, 6);
            std::string objType = extractFixedSize (source, 1);
            uint32_t displayPriority = std::atoi (extractFixedSize (source, 5).c_str ());
            std::string radarPriority = extractFixedSize (source, 1);
            std::string tableSet = extractToUnitTerm (source);

            lookupTablePos = lookupTables.find (acronym);

            if (lookupTablePos == lookupTables.end ()) {
                lookupTablePos = lookupTables.emplace (acronym, std::vector<LookupTableItem> ()).first;
            }

            auto& item = lookupTablePos->second.emplace_back ();

            memcpy (item.acronym, acronym.c_str (), 6);
            item.displayPriority = displayPriority;
            item.radarPriority = radarPriority [0];
            item.objectType = objType [0];
            
            if (tableSet.compare ("PLAIN_BOUNDARIES") == 0) {
                item.tableSet = TableSet::PLAIN_BOUNDARIES;
            } else if (tableSet.compare ("SYMBOLIZED_BOUNDARIES") == 0) {
                item.tableSet = TableSet::SYMBOLIZED_BOUNDARIES;
            } else if (tableSet.compare ("SIMPLIFIED") == 0) {
                item.tableSet = TableSet::SIMPLIFIED;
            } else if (tableSet.compare ("PAPER_CHART") == 0) {
                item.tableSet = TableSet::PAPER_CHARTS;
            } else if (tableSet.compare ("LINES") == 0) {
                item.tableSet = TableSet::LINES;
            }
        } else if (memcmp (source, "ATTC", 4) == 0) {
            if (lookupTablePos != lookupTables.end ()) {
                source += 9;

                std::string acronym;
                std::string value;

                if (*source == UT) {
                    // add no-attribute instance
                    auto& instance = lookupTablePos->second.back ().attrCombination.emplace_back ();
                } else {
                    while (*source != UT) {
                        acronym = extractFixedSize (source, 6);
                        value = extractToUnitTerm (source);

                        if (!acronym.empty ()) {
                            auto& instance = lookupTablePos->second.back ().attrCombination.emplace_back ();

                            instance.acronym = acronym;
                            instance.strValue = value;
                        }
                    }
                }
            }
        } else if (memcmp (source, "INST", 4) == 0) {
            source += 9;
            lookupTablePos->second.back ().instruction = extractToUnitTerm (source);
        } else if (memcmp (source, "DISC", 4) == 0) {
            source += 9;
            std::string displayCat = extractToUnitTerm (source);
            
            if (displayCat.compare ("STANDARD") == 0) {
                lookupTablePos->second.back ().displayCat = DisplayCat::STANDARD;
            } else if (displayCat.compare ("DISPLAY_BASE") == 0) {
                lookupTablePos->second.back ().displayCat = DisplayCat::DISPLAY_BASE;
            }
        } else if (memcmp (source, "LUCM", 4) == 0) {
            source += 9;
            lookupTablePos->second.back ().comment = extractToUnitTerm (source);
        }
    }
}
*/
void loadPattern (std::vector<std::string>& module, Dai& dai) {
    size_t patternIndex = LookupTableItem::NOT_EXIST;

    for (auto& line: module) {
        char *source = (char *) line.c_str ();

        if (memcmp (source, "PATT", 4) == 0) {
        } else if (memcmp (source, "PATD", 4) == 0) {
            source += 9;
            std::string name = extractFixedSize (source, 8);
            std::string type = extractFixedSize (source, 1);
            std::string fillType = extractFixedSize (source, 3);
            std::string spacing = extractFixedSize (source, 3);
            uint32_t minDistance = std::atol (extractFixedSize (source, 5).c_str ());
            uint32_t maxDistance = std::atol (extractFixedSize (source, 5).c_str ());
            uint32_t pivotPtCol = std::atol (extractFixedSize (source, 5).c_str ());
            uint32_t pivotPtRow = std::atol (extractFixedSize (source, 5).c_str ());
            uint32_t bBoxWidth = std::atol (extractFixedSize (source, 5).c_str ());
            uint32_t bBoxHeight = std::atol (extractFixedSize (source, 5).c_str ());
            uint32_t bBoxCol = std::atol (extractFixedSize (source, 5).c_str ());
            uint32_t bBoxRow = std::atol (extractFixedSize (source, 5).c_str ());
            
            auto pos = dai.patternIndex.find (name);

            if (pos == dai.patternIndex.end ()) {
                patternIndex = dai.patterns.size ();
                dai.patterns.emplace_back (PatternDesc ());
                dai.patternIndex.emplace (name, patternIndex);
            }

            auto& patternDesc = dai.patterns.back ();

            memcpy (patternDesc.name, name.c_str (), 8);
            patternDesc.type = type [0];
            patternDesc.bBoxCol = bBoxCol;
            patternDesc.bBoxRow = bBoxRow;
            patternDesc.bBoxWidth = bBoxWidth;
            patternDesc.bBoxHeight = bBoxHeight;
            patternDesc.maxDistance = maxDistance;
            patternDesc.minDistance = minDistance;
            patternDesc.pivotPtCol = pivotPtCol;
            patternDesc.pivotPtRow = pivotPtRow;

            if (spacing.compare ("CON") == 0) {
                patternDesc.spacing = Spacing::CONSTANT;
            } else if (spacing.compare ("SCL") == 0) {
                patternDesc.spacing = Spacing::SCALE_DEPENDENT;
            }

            if (fillType.compare ("STG")) {
                patternDesc.fillType = FillType::STRAGGERED;
            } else if (fillType.compare ("LIN")) {
                patternDesc.fillType = FillType::LINEAR;
            }
        } else if (memcmp (source, "PXPO", 4) == 0) {
            source += 9;
            dai.patterns.back ().exposition = extractToUnitTerm (source);
        } else if (memcmp (source, "PCRF", 4) == 0) {
            source += 9;
            while (*source) {
                std::string colorDef = extractFixedSize (source, 6);
                dai.patterns.back ().drawProc.definePen (colorDef [0], colorDef.substr (1).c_str (), dai.colorTable.index);
            }
        } else if (memcmp (source, "PBTM", 4) == 0) {
            source += 9;
            dai.patterns.back ().bitmap = extractToUnitTerm (source);
        } else if (memcmp (source, "PVCT", 4) == 0) {
            source += 9;
            auto& svg = dai.patterns.back ().svgs.emplace_back ();
            splitString (extractToUnitTerm (source), svg, ';');
            for (auto& instr: svg) {
                dai.patterns.back ().drawProc.defineOperation (instr.c_str (), dai);
            }
        }
    }
}

void loadSymbol (std::vector<std::string>& module, Dai& dai) {
    size_t symbolIndex = LookupTableItem::NOT_EXIST;

    for (auto& line: module) {
        char *source = (char *) line.c_str ();

        if (memcmp (source, "SYMB", 4) == 0) {
        } else if (memcmp (source, "SYMD", 4) == 0) {
            source += 9;
            std::string name = extractFixedSize (source, 8);
            std::string type = extractFixedSize (source, 1);
            uint32_t pivotPtCol = std::atol (extractFixedSize (source, 5).c_str ());
            uint32_t pivotPtRow = std::atol (extractFixedSize (source, 5).c_str ());
            uint32_t bBoxWidth = std::atol (extractFixedSize (source, 5).c_str ());
            uint32_t bBoxHeight = std::atol (extractFixedSize (source, 5).c_str ());
            uint32_t bBoxCol = std::atol (extractFixedSize (source, 5).c_str ());
            uint32_t bBoxRow = std::atol (extractFixedSize (source, 5).c_str ());

            auto pos = dai.symbolIndex.find (name);

            if (pos == dai.symbolIndex.end ()) {
                symbolIndex = dai.symbols.size ();
                dai.symbols.emplace_back (SymbolDesc ());
                dai.symbolIndex.emplace (name, symbolIndex);
            }

            auto& symbolDesc = dai.symbols.back ();
            memcpy (symbolDesc.name, name.c_str (), 8);
            symbolDesc.type = type [0];
            symbolDesc.bBoxCol = bBoxCol;
            symbolDesc.bBoxRow = bBoxRow;
            symbolDesc.bBoxWidth = bBoxWidth;
            symbolDesc.bBoxHeight = bBoxHeight;
            symbolDesc.pivotPtCol = pivotPtCol;
            symbolDesc.pivotPtRow = pivotPtRow;
        } else if (memcmp (source, "SXPO", 4) == 0) {
            source += 9;
            dai.symbols.back ().exposition = extractToUnitTerm (source);
        } else if (memcmp (source, "SCRF", 4) == 0) {
            source += 9;
            while (*source) {
                std::string colorDef = extractFixedSize (source, 6);
                dai.symbols.back ().drawProc.definePen (colorDef [0], colorDef.substr (1).c_str (), dai.colorTable.index);
            }
        } else if (memcmp (source, "SBTM", 4) == 0) {
            source += 9;
            dai.symbols.back ().bitmap = extractToUnitTerm (source);
        } else if (memcmp (source, "SVCT", 4) == 0) {
            source += 9;
            auto& svg = dai.symbols.back ().svgs.emplace_back ();
            splitString (extractToUnitTerm (source), svg, ';');
            for (auto& instr: svg) {
                dai.symbols.back ().drawProc.defineOperation (instr.c_str (), dai);
            }
        }
    }
}

void loadLine (std::vector<std::string>& module, Dai& dai) {
    size_t lineIndex = LookupTableItem::NOT_EXIST;

    for (auto& line: module) {
        char *source = (char *) line.c_str ();

        if (memcmp (source, "LNST", 4) == 0) {
        } else if (memcmp (source, "LIND", 4) == 0) {
            source += 9;
            std::string name = extractFixedSize (source, 8);
            uint32_t pivotPtCol = std::atol (extractFixedSize (source, 5).c_str ());
            uint32_t pivotPtRow = std::atol (extractFixedSize (source, 5).c_str ());
            uint32_t bBoxWidth = std::atol (extractFixedSize (source, 5).c_str ());
            uint32_t bBoxHeight = std::atol (extractFixedSize (source, 5).c_str ());
            uint32_t bBoxCol = std::atol (extractFixedSize (source, 5).c_str ());
            uint32_t bBoxRow = std::atol (extractFixedSize (source, 5).c_str ());
            
            auto pos = dai.lineIndex.find (name);

            if (pos == dai.lineIndex.end ()) {
                lineIndex = dai.symbols.size ();
                dai.lines.emplace_back (LineDesc ());
                dai.lineIndex.emplace (name, lineIndex);
            }

            auto& lineDesc = dai.lines.back ();
            memcpy (lineDesc.name, name.c_str (), 8);

            memcpy (lineDesc.name, name.c_str (), 8);
            lineDesc.bBoxCol = bBoxCol;
            lineDesc.bBoxRow = bBoxRow;
            lineDesc.bBoxWidth = bBoxWidth;
            lineDesc.bBoxHeight = bBoxHeight;
            lineDesc.pivotPtCol = pivotPtCol;
            lineDesc.pivotPtRow = pivotPtRow;
        } else if (memcmp (source, "LXPO", 4) == 0) {
            source += 9;
            dai.lines.back ().exposition = extractToUnitTerm (source);
        } else if (memcmp (source, "LCRF", 4) == 0) {
            source += 9;
            while (*source) {
                std::string colorDef = extractFixedSize (source, 6);
                dai.lines.back ().drawProc.definePen (colorDef [0], colorDef.substr (1).c_str (), dai.colorTable.index);
            }
        } else if (memcmp (source, "LVCT", 4) == 0) {
            source += 9;
            auto& svg = dai.lines.back ().svgs.emplace_back ();
            splitString (extractToUnitTerm (source), svg, ';');
            for (auto& instr: svg) {
                dai.lines.back ().drawProc.defineOperation (instr.c_str (), dai);
            }
        }
    }
}

void loadColorTable (const char *path, Dai& dai) {
    char *content = 0;
    size_t size = loadFileAndConvertToAnsi (path, content);
    std::vector<std::string> lines;
    splitText (content, lines);
    free (content);

    PaletteIndex paletteIndex = (PaletteIndex) 0;

    for (auto& line: lines) {
        if (line.compare ("Table:DAY") == 0) {
            paletteIndex = PaletteIndex::Day; continue;
        } if (line.compare ("Table:DUSK") == 0) {
            paletteIndex = PaletteIndex::Dusk; continue;
        } if (line.compare ("Table:NIGHT") == 0) {
            paletteIndex = PaletteIndex::Night; continue;
        }

        std::vector<std::string> parts;
        splitString (line, parts, ';');

        if (parts.size () > 8 && paletteIndex > 0) {
            auto colorItem = dai.colorTable.checkAddColor (parts [0].c_str ());
            auto rgb = colorItem->getColorDef (paletteIndex);
            
            rgb->red = std::atoi (parts [5].c_str ());
            rgb->green = std::atoi (parts [6].c_str ());
            rgb->blue = std::atoi (parts [7].c_str ());
        }
    }

    for (auto item: dai.colorTable.index) {
        dai.palette.checkSolidBrush (item.first.c_str (), dai.colorTable);
    }
}

void parseInstructionList (const char *source, DrawProcedure& drawProcedure) {

}

void loadDai (const char *path, Dai& dai, ObjectDictionary& objectDictionary, AttrDictionary& attrDictionary) {
    char *content = 0;
    size_t size = loadFileAndConvertToAnsi (path, content);

    std::vector<std::vector<std::string>> modules;

    modules.emplace_back ();

    std::vector<std::string> lines;

    splitText (content, lines);
    free (content);

    for (auto& line: lines) {
        if (memcmp (line.c_str (), "****", 4) == 0) {
            modules.emplace_back ();
        } else {
            modules.back ().emplace_back (line.c_str ());
        }
    }

    for (auto& module: modules) {
        if (module.size () > 1) {
            const char *moduleName = module [1].c_str ();

            if (memcmp (moduleName, "LBID", 4) == 0) {
                loadLibraryId (module, dai.libraryId);
            } else if (memcmp (moduleName, "COLS", 4) == 0) {
                if (memcmp (moduleName + 19, "DAY", 3) == 0) {
                    // It's not clear how to do - use external color table instead
                    //loadColorTable (module, dai.dayColorTable);
                } else if (memcmp (moduleName + 19, "DUSK", 4) == 0) {
                    // It's not clear how to do - use external color table instead
                    //loadColorTable (module, dai.duskColorTable);
                } else if (memcmp (moduleName + 19, "NIGHT", 5) == 0) {
                    // It's not clear how to do - use external color table instead
                    //loadColorTable (module, dai.nightColorTable);
                }
            } else if (memcmp (moduleName, "LUPT", 4) == 0) {
                //loadLookupTableItem (module, dai.lookupTables);
                loadLookupTableItem (module, dai, objectDictionary, attrDictionary);
            } else if (memcmp (moduleName, "PATT", 4) == 0) {
                loadPattern (module, dai);
            } else if (memcmp (moduleName, "SYMB", 4) == 0) {
                loadSymbol (module, dai);
            } else if (memcmp (moduleName, "LNST", 4) == 0) {
                loadLine (module, dai);
            }
        }
    }
    dai.palette.composeBasePens (dai.colorTable);

    // Translate instructions
    for (auto& lookupTable: dai.lookupTables) {
        for (auto& item: lookupTable) {
            std::vector<std::string> parts;
            splitString (item.instruction, parts, ';');
            processInstructions (dai, item, parts);
        }
    }
}
