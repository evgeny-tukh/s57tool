#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <vector>
#include <string>
#include <map>

struct RecProps {
    size_t recLength;
    size_t fieldLengthFieldSize;
    size_t fieldPosFieldSize;
    size_t fieldTagFieldSize;
    size_t baseAddrOfFieldArea;
};

struct FormatCtl {
    char specifier;
    int length;

    FormatCtl (char _specifier): specifier (_specifier), length (0) {}
    FormatCtl (char _specifier, int _length): specifier (_specifier), length (_length) {}
};

#pragma pack(1)

struct RecLeader {
    char recLength [5];
                                    // should be:   DDR                 DR
    char interchangeLevel;          //              '3'                 '<space>'
    char leaderID;                  //              'L'                 'D'
    char inlineCodeExtID;           //              'E'                 '<space>'
    char versionNo;                 //              '1'                 '<space>'
    char appID;                     //              '<space>'           '<space>'
    char fieldCtlLen [2];           //              '09'                '<space><space>'
    char baseAddrOfFieldArea [5];
    char extendedCharSetInd [3];    //              '<space>!<space>'   '<space><space><space>'
    struct entryMap {
        char fieldLengthFieldSize;
        char fieldPosFieldSize;
        char reserved;              //              '0'                 '0'
        char fieldTagFieldSize;     //              '4'                 '4'
    } map;
};

struct DirEntry {
    std::string tag;
    size_t length;
    size_t position;

    DirEntry (char *_tag, size_t _tagSize, size_t _length, size_t _position): length (_length), position (_position) {
        tag.append (_tag, _tagSize);
    }
};

struct FieldPair {
    std::string first;
    std::string second;

    FieldPair (const char *_first, const char *_second): first (_first), second (_second) {}
};

enum DataStructCode {
    LINERAR = '1',
    MULTI_DIM = '2',
};

enum DataTypeCode {
    STRING = '0',
    INTEGER = '1',
    BINARY = '5',
    MIXED = '6',
};

struct DdfFieldControls {
    char dataStructCode;        // 1 - linear struct, 2 - multidim struct
    char dataTypeCode;          // 0 - string, 1 - int, 5 - binary, 6 - mixed data
    char auxControls [2];       // 00
    char printableGraphics [2]; // ;&
    char truncatedEscSeq [3];   // '{3} ' - lex level 0; '-A ' - lex level 1; '%/A' - lex level 2
};

int char2int (char *source, int size);

struct DataDescriptiveField {
    DdfFieldControls fieldControls;
    std::string fieldName;
    std::vector<std::string> descriptors;
    std::vector<FormatCtl> formatControls;
    int code;
};

#pragma pack()

int printFail (char *msg) {
    printf (msg);
    exit (0);
}

int char2int (char *source, int size) {
    int result = 0;

    for (int i = 0; i < size; ++ i) {
        result = result * 10 + (source [i] - '0');
    }

    return result;
}

void showUsage () {
    printf ("S-57 Tool\n");
}

void showUsageAndExit () {
    showUsage ();
    exit (0);
}

void extractRecord (std::vector<uint8_t>& buffer, std::vector<uint8_t>& record) {
    size_t recLength = char2int ((char *) buffer.data (), 5);
    auto to = buffer.begin () + recLength;

    record.clear ();
    record.insert (record.begin (), buffer.begin (), to);
    buffer.erase (buffer.begin (), to);
}

void parseGenericDataRecord (
    RecProps& recProps,
    std::vector<uint8_t>& record, RecLeader& recLeader,
    std::vector<DirEntry>& directory,
    std::vector<uint8_t>& fieldArea
) {
    std::vector<uint8_t> dirData;
    char *source;

    memcpy (& recLeader, record.data (), sizeof (RecLeader));
    
    recProps.recLength = char2int (recLeader.recLength, 5);
    recProps.fieldLengthFieldSize = char2int (& recLeader.map.fieldLengthFieldSize, sizeof (recLeader.map.fieldLengthFieldSize));
    recProps.fieldPosFieldSize = char2int (& recLeader.map.fieldPosFieldSize, sizeof (recLeader.map.fieldPosFieldSize));
    recProps.fieldTagFieldSize = char2int (& recLeader.map.fieldTagFieldSize, sizeof (recLeader.map.fieldTagFieldSize));
    recProps.baseAddrOfFieldArea = char2int (recLeader.baseAddrOfFieldArea, sizeof (recLeader.baseAddrOfFieldArea));

    // Extract directory and field area data parts into separated buffers
    dirData.insert (dirData.begin (), record.data () + sizeof (RecLeader), record.data () + recProps.baseAddrOfFieldArea);
    fieldArea.clear ();
    fieldArea.insert (fieldArea.begin (), record.data () + recProps.baseAddrOfFieldArea, record.data () + recProps.recLength);

    // Process the record directory
    for (
        source = (char *) dirData.data (), directory.clear ();
        *source != '\x1e';
        source += recProps.fieldLengthFieldSize + recProps.fieldTagFieldSize + recProps.fieldPosFieldSize
    ) {
        directory.emplace_back (
            source,
            recProps.fieldTagFieldSize,
            char2int (source + recProps.fieldTagFieldSize, recProps.fieldLengthFieldSize),
            char2int (source + recProps.fieldTagFieldSize + recProps.fieldLengthFieldSize, recProps.fieldPosFieldSize)
        );
    }
}

DirEntry *findField (std::vector<DirEntry>& directory, const char *tag) {
    for (auto& entry: directory) {
        if (entry.tag.compare (tag) == 0) return & entry;
    }

    return (DirEntry *) 0;
};

void splitBinaryBuffer (std::vector<uint8_t>& buffer, std::vector<std::vector<uint8_t>>& parts, uint8_t separator) {
    parts.clear ();
    parts.emplace_back ();

    for (auto pos = buffer.begin (); pos != buffer.end (); ++ pos) {
        if (*pos == separator) {
            parts.emplace_back ();
        } else {
            parts.back ().emplace_back (*pos);
        }
    }
}

void splitFieldArea (std::vector<uint8_t>& fieldArea, std::vector<std::vector<uint8_t>>& fields) {
    splitBinaryBuffer (fieldArea, fields, '\x1e');
}

void splitFieldUnits (std::vector<uint8_t>& field, std::vector<std::vector<uint8_t>>& units) {
    splitBinaryBuffer (field, units, '\x1f');
}

void parseDataRecord (
    std::vector<uint8_t>& record,
    RecProps& recProps
) {
    RecLeader recLeader;
    std::vector<uint8_t> fieldArea;
    std::vector<std::vector<uint8_t>> fields;
    std::vector<std::vector<uint8_t>> units;

    parseGenericDataRecord (recProps, record, recLeader, directory, fieldArea);
    splitFieldArea (fieldArea, fields);
}

void parseDataDescriptiveRecord (
    std::vector<uint8_t>& record,
    RecProps& recProps,
    std::vector<DirEntry> directory,
    std::vector<FieldPair> fieldPairs,
    DataStructCode& dataStructCode,
    DataTypeCode& dataTypeCode,
    int& lexLevel,
    std::vector<DataDescriptiveField>& dataDescFields
) {
    RecLeader recLeader;
    std::vector<uint8_t> fieldArea;
    std::vector<std::vector<uint8_t>> fields;
    std::vector<std::vector<uint8_t>> units;

    parseGenericDataRecord (recProps, record, recLeader, directory, fieldArea);
    splitFieldArea (fieldArea, fields);

    // Process field area
    for (size_t i = 0; i < fields.size (); ++ i) {
        if (fields [i].size () == 0) continue;

        splitFieldUnits (fields [i], units);

        dataDescFields.emplace_back ();

        DataDescriptiveField& ddf = dataDescFields.back ();

        memcpy (& ddf.fieldControls, units [0].data (), sizeof (ddf.fieldControls));

        ddf.code = char2int ((char *) & ddf.fieldControls, 4);
        
        if (i == 0) {
            // Field control field
            if (memcmp (& ddf.fieldControls, "0000", 4) == 0) {
                for (size_t i = 0; i < units [1].size () && units [1][i] != '\x1e'; i += recProps.fieldTagFieldSize * 2) {
                    std::string first ((const char *) units [1].data () + i, recProps.fieldTagFieldSize);
                    std::string second ((const char *) units [1].data () + (i + recProps.fieldTagFieldSize), recProps.fieldTagFieldSize);

                    fieldPairs.emplace_back (first.c_str (), second.c_str ());
                }
            }
        } else {
            // Data descriptive field
            ddf.fieldName.append ((const char *) units [0].data () + sizeof (ddf.fieldControls), units [0].size () - sizeof (ddf.fieldControls));

            if (units [1].size () > 0) {
                std::vector<std::vector<uint8_t>> descriptors;

                splitBinaryBuffer (units [1], descriptors, '!');

                for (auto& descriptor: descriptors) {
                    ddf.descriptors.emplace_back ((const char *) descriptor.data (), descriptor.size ());
                }
            }

            if (units [2].size () > 0) {
                std::vector<std::vector<uint8_t>> formatControls;

                if (units [2].front () == '(' && units [2].back () == ')') {
                    units [2].erase (units [2].begin ());
                    units [2].erase (units [2].end () - 1);
                }

                splitBinaryBuffer (units [2], formatControls, ',');

                for (auto& formatControl: formatControls) {
                    size_t numOfDigits = 0;

                    while (std::isdigit (formatControl [numOfDigits])) ++ numOfDigits;

                    size_t multiplier = numOfDigits > 0 ? char2int ((char *) formatControl.data (), numOfDigits) : 1;

                    for (size_t i = 0; i < multiplier; ++ i) {
                        std::string formatControl ((const char *) formatControl.data () + numOfDigits, formatControl.size () - numOfDigits);

                        if (formatControl [1] != '(') {
                            ddf.formatControls.emplace_back (formatControl [0]);
                        } else if (formatControl [2] == ')') {
                            ddf.formatControls.emplace_back (formatControl [0], -1);
                        } else {
                            ddf.formatControls.emplace_back (formatControl [0], std::atoi (formatControl.c_str () + 2));
                        }
                    }
                }
            }
        }
    }
}

void loadFile (FILE *file, std::vector<uint8_t>& buffer) {
    auto pos = ftell (file);
    fseek (file, 0, SEEK_END);
    auto size = ftell (file);
    fseek (file, pos, SEEK_SET);
    buffer.resize (size - pos);
    fread (buffer.data (), 1, buffer.size (), file);
}

#if 0
bool readDdr (FILE *file) {
    DdrLeader ddrLeader;
    std::vector<char> buffer;

    // Read and parse the leader
    if (fread (& ddrLeader, sizeof (ddrLeader), 1, file) <= 0) return false;

    size_t recLength = char2int (ddrLeader.recLength, 5);
    size_t fieldLengthFieldSize = char2int (& ddrLeader.map.fieldLengthFieldSize, sizeof (ddrLeader.map.fieldLengthFieldSize));
    size_t fieldPosFieldSize = char2int (& ddrLeader.map.fieldPosFieldSize, sizeof (ddrLeader.map.fieldPosFieldSize));
    size_t fieldTagFieldSize = char2int (& ddrLeader.map.fieldTagFieldSize, sizeof (ddrLeader.map.fieldTagFieldSize));
    size_t baseAddrOfFieldArea = char2int (ddrLeader.baseAddrOfFieldArea, sizeof (ddrLeader.baseAddrOfFieldArea));

    // Read the rest of the record
    size_t bufSize = recLength - sizeof (DdrLeader);
    buffer.resize (bufSize);

    if (fread (buffer.data (), 1, bufSize, file) <= 0) return false;

    // Process the directory
    std::vector<DirEntry> directory;
    std::vector<FieldPair> fieldPairs;
    DataStructCode dataStructCode;
    DataTypeCode dataTypeCode;
    int lexLevel;
    char *source;

    auto findField = [&directory] (const char *tag) {
        for (auto& entry: directory) {
            if (entry.tag.compare (tag) == 0) return & entry;
        }

        return (DirEntry *) 0;
    };

    for (source = buffer.data (); *source != '\x1e'; source += fieldLengthFieldSize + fieldTagFieldSize + fieldPosFieldSize) {
        directory.emplace_back (
            source,
            fieldTagFieldSize,
            char2int (source + fieldTagFieldSize, fieldLengthFieldSize),
            char2int (source + fieldTagFieldSize + fieldLengthFieldSize, fieldPosFieldSize)
        );
    }

    // Process field area
    for (++ source; *source != '\x1e';) {
        std::string fieldTag (source, fieldTagFieldSize);

        auto fieldEntry = findField (fieldTag.c_str ());

        if (fieldEntry) {
            if (fieldTag.compare ("0000") == 0) {
                char *couples = strchr (source, '\x1f');

                if (couples) {
                    for (++ couples; *couples != '\x1e'; couples += fieldTagFieldSize * 2) {
                        std::string first (couples, fieldTagFieldSize);
                        std::string second (couples + fieldTagFieldSize, fieldTagFieldSize);

                        fieldPairs.emplace_back (first.c_str (), second.c_str ());
                    }
                }
            } else {
                DdfFieldControls *fieldCtrls = (DdfFieldControls *) (source + 1);

                dataStructCode = (DataStructCode) fieldCtrls->dataStructCode;
                dataTypeCode = (DataTypeCode) fieldCtrls->dataTypeCode;

                if (memcmp (fieldCtrls->truncatedEscSeq, "   ", 3) == 0) {
                    lexLevel = 0;
                } else if (memcmp (fieldCtrls->truncatedEscSeq, "-A ", 3) == 0) {
                    lexLevel = 1;
                } if (memcmp (fieldCtrls->truncatedEscSeq, "%/A", 3) == 0) {
                    lexLevel = 3;
                }
            }
        }
    }
    return true;
}
#endif
int main (int argCount, char *args []) {
    char path [500];

    for (int i = 1; i < argCount; ++ i) {
        char *arg = args [i];

        if (*arg != '-' && *arg != '/') {
            showUsageAndExit ();
        }

        switch (toupper (arg [1])) {
            case 'P':
                strcpy (path, arg + 3);
                if (*path == '"') {
                    for (auto i = 1; path [i]; ++ i) {
                        if (path [i] == '"') path [i] = '\0';
                        path [i-1] = path [i];
                    }
                }
                break;
            default:
                showUsageAndExit ();           
        }
    }

    FILE *catalog = 0;
    char catPath [500];

    strcpy (catPath, path);
    strcat (catPath, "\\catalog.");

    int catIndex;

    for (catIndex = 0, !catalog; catIndex < 1000; ++ catIndex) {
        char filePath [500];
        char ext [4];

        strcpy (filePath, catPath);
        sprintf (ext, "%03d", catIndex);
        strcat (filePath, ext);

        catalog = fopen (filePath, "rb+");

        if (catalog) {
            std::vector<uint8_t> buffer, ddr, dr;
            std::vector<DirEntry> directory;
            std::vector<FieldPair> fieldPairs;
            DataStructCode dataStructCode;
            DataTypeCode dataTypeCode;
            std::vector<DataDescriptiveField> dataDescFields;
            int lexLevel;
            RecProps recProps;

            loadFile (catalog, buffer);
            extractRecord (buffer, ddr);
            parseDataDescriptiveRecord (ddr, recProps, directory, fieldPairs, dataStructCode, dataTypeCode, lexLevel, dataDescFields);

            extractRecord (buffer, dr);
            parseDataRecord (dr, recProps);
            //readDdr (catalog);
            fclose (catalog);
            break;
        }
    }
}
