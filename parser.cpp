#include <stdlib.h>
#include <stdio.h>
#include <cstdint>
#include <vector>
#include <map>
#include <string>
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
    std::vector<DdfDesc>& dataDescriptiveFields,
    FieldControls *fieldControls,
    std::string& fieldName,
    std::string& arrayDesc,
    std::string& fieldValue
) {
    auto& item = dataDescriptiveFields.emplace_back ();
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

        if (format.length () > 1 && format [1] == '(') {
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
    std::string& fieldName,
    std::string& arrayDesc,
    std::string& fieldValue,
    std::vector<DirEntry>& directory,
    ParsedLeader& parsedLeader,
    std::vector<std::pair<std::string, std::string>>& fieldTree,
    std::vector<DdfDesc>& dataDescriptiveFields
) {
    bool result = false;
    
    if (memcmp (fieldControls, "0000", 4) == 0) {
        processFieldControlField (fieldName, arrayDesc, directory, parsedLeader, fieldTree); result = true;
    } else {
        processDataDesciptiveField (dataDescriptiveFields, fieldControls, fieldName, arrayDesc, fieldValue); result = true;
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

size_t parseDataRecord (
    const char *start,
    std::vector<DdfDesc>& dataDescriptiveFields
) {
    std::vector<DirEntry> directory;
    ParsedLeader parsedLeader ((Leader *) start);

    size_t headerSize = parseRecordLeaderAndDirectory (start, directory);
    const char *source = start + headerSize;

    for (size_t i = 0; i < directory.size (); ++ i) {
        auto& dirEntry = directory [i];
        auto& ddf = dataDescriptiveFields [i];
        const char *fieldPos = source + dirEntry.position;

        for (auto j = 0; j < ddf.fields.size (); ++ j) {
            auto& fld = ddf.fields [j];
            uint32_t intValue;
            std::string stringValue;
            auto getIntValue = [&fld, &fieldPos] () {
                char value [100];

                memset (value, 0, sizeof (value));

                if (fld.modifier.has_value ()) {
                    memcpy (value, fieldPos, fld.modifier.value ());
                    fieldPos += fld.modifier.value ();
                } else {
                    size_t k;
                    for (k = 0; isdigit (fieldPos [k]) && fieldPos [k] != UT; ++ k) {
                        value [k] = fieldPos [k];
                    }
                    fieldPos += k + 1;
                }
                return std::atoi (value);
            };
            auto getStrValue = [&fld, &fieldPos] () {
                std::string value;

                if (fld.modifier.has_value ()) {
                    value.append (fieldPos, fld.modifier.value ());
                    fieldPos += fld.modifier.value ();
                } else {
                    size_t k;
                    for (k = 0; fieldPos [k] != UT; ++ k) {
                        value += fieldPos [k];
                    }
                    fieldPos += k + 1;
                }
                return value;
            };
            switch (fld.format) {
                case 'I': {
                    intValue = getIntValue (); break;
                }
                case 'A': {
                    stringValue = getStrValue (); break;
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
    std::vector<DdfDesc>& dataDescriptiveFields
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

        parseDdrField (fieldControls, fieldName, arrayDesc, fieldValue, directory, parsedLeader, fieldTree, dataDescriptiveFields);

        source = curPtr + 1;
    }

    return parsedLeader.recLength;
}

bool parseCatalog (const char *catPath) {
    char *catalogContent = 0;
    size_t size = loadFile (catPath, catalogContent);
    bool result = false;

    if (size) {
        char *recStart = catalogContent;
        std::vector<std::pair<std::string, std::string>> fieldTree;
        std::vector<DdfDesc> dataDescriptiveFields;
        size_t processedSize = 0;
        
        size_t ddrSize = parseDataDescriptiveRecord (recStart, fieldTree, dataDescriptiveFields);

        recStart += ddrSize;
        processedSize += ddrSize;

        while (processedSize < size) {
            size_t drSize = parseDataRecord (recStart, dataDescriptiveFields);

            recStart += ddrSize;
            processedSize += ddrSize;
        }
        
        result = true;
        free (catalogContent);
    }

    return result;
}

