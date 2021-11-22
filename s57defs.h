#pragma once

#include <cstdint>
#include <optional>

#pragma pack(1)

enum RCNM {
    DatasetGeneralInfo = 10,
    DatasetGeoRef = 20,
    DatasetHistory = 30,
    DatasetAccuracy = 40,
    CatalogueCrossRef = 60,
    DataDictDef = 70,
    DataDictDomain = 80,
    DataDictSchema = 90,
    Feature = 100,
    IsolatedNode = 110,
    ConnectedNode = 120,
    Edge = 130,
    Face = 140,
};

namespace RCNM_ANSI {
    inline const char *DatasetGeneralInfo = "DS";
    inline const char *DatasetGeoRef = "DP";
    inline const char *DatasetHistory = "DH";
    inline const char *DatasetAccuracy = "DA";
    inline const char *CatalogueDir = "CD";
    inline const char *CatalogueCrossRef = "CR";
    inline const char *DataDictDef = "ID";
    inline const char *DataDictDomain = "IO";
    inline const char *DataDictSchema = "IS";
    inline const char *Feature = "FE";
    inline const char *IsolatedNode = "VI";
    inline const char *ConnectedNode = "VC";
    inline const char *Edge = "VE";
    inline const char *Face = "VF";
}

enum DSTR {
    CartSpagetti = 1,
    ChainNode = 2,
    PlanarGraph = 3,
    FullTopology = 4,
    DataStrNotRel = 255,
};

namespace DSTR_ANSI {
    inline const char *CartSpagetti = "CS";
    inline const char *ChainNode = "CN";
    inline const char *PlanarGraph = "PG";
    inline const char *FullTopology = "FT";
    inline const char *NotRelevant = "NO";
}

enum COUN {
    LatLon = 1,
    EastingNorthing = 2,
    UnitsOnChart = 3,
};

namespace COUN_ANSI {
    inline const char *LatLon = "LL";
    inline const char *EastingNorthing = "EN";
    inline const char *UnitsOnChart = "UC";
}

enum PRIM {
    Point = 1,
    PointAnsi = 'P',
    Line = 2,
    LineAnsi = 'L',
    Area = 3,
    AreaAnsi = 'A',
    None = 255,
    NoneAnsi = 'N',
};

enum ORNT {
    Forward = 1,
    ForwardAnsi = 'F',
    Reverse = 2,
    ReverseAnsi = 'R',
    OrntNotRel = 255,
    OrntNotRelAnsi = 'N',
};

namespace USAG {
    inline uint8_t Mask = 1;
    inline char MaskAnsi = 'M';
    inline uint8_t Show = 2;
    inline char ShowAnsi = 'S';
    inline uint8_t MaskingNotRel = 255;
    inline char MaskingNotRelAnsi = 'N';
    inline uint8_t Exterior = 1;
    inline char ExteriorAnsi = 'E';
    inline uint8_t Interior = 2;
    inline char InteriorAnsi = 'I';
    inline uint8_t ExteriorTruncated = 3;
    inline char ExteriorTruncatedAnsi = 'C';
}

enum TOPI {
    ContainingFace = 5,
    BeginningNode = 1,
    EndNode = 2,
    LeftFace = 3,
    RightFace = 4,

    ContainingFaceAnsi = 'F',
    BeginningNodeAnsi = 'B',
    EndNodeAnsi = 'E',
    LeftFaceAnsi = 'S',
    RightFaceAnsi = 'D',
};

enum RIND {
    Master = 1,
    MasterAnsi = 'M',
    Slave = 2,
    SlaveAnsi = 'S',
    Peer = 3,
    PeerAnsi = 'P',
};

inline char FT = '\x1e';
inline char UT = '\x1f';

inline uint32_t parseField (char *field, size_t size) {
    uint32_t result = 0;

    for (size_t i = 0; i < size; ++ i) {
        result *= 10;
        result += field [i] - '0';
    }

    return result;
}

struct EntryMap {
                                // Meaning or mandatory content
    char fieldLengthSize;       // Field length field size (1..9)
    char fieldPosSize;          // Field position field size (1..9)
    char reserved;              // 0
    char fieldTagSize;          // 4
};

struct DirEntry {
    std::string tag;
    uint32_t length;
    uint32_t position;

    DirEntry (char *& source, EntryMap *entryMap) {
        int fieldTagSize = entryMap->fieldTagSize - '0';
        int fieldPosSize = entryMap->fieldPosSize - '0';
        int fieldLengthSize = entryMap->fieldLengthSize - '0';

        tag.append (source, fieldTagSize);
        source += fieldTagSize;

        length = parseField (source, fieldLengthSize);
        source += fieldLengthSize;

        position = parseField (source, fieldPosSize);
        source += fieldPosSize;
    }
};

struct FieldControls {
    char dataStructureCode;     // 0 - field control field (tree), 1 - linear, 2 - multi-dim
    char dataTypeCode;          // 0 - char string, 1 - implicit point, 5 - binary form, 6 - mixed data type
    char auxControls [2];       // 00
    char printableGraphics [2]; // ;&
    char truncatedEscSeq [3];   // <space><space><space> - lex lvl 0, -A<space> - lex lvl 1, %/A - lex lvl 2
};

struct Leader {
                                // Meaning or mandatory content
                                // DDR                              DR
    char recLength [5];         // number of bytes in the record
    char interchangeLevel;      // 3                                <space>
    char leaderID;              // L                                D
    char inlineCodeExtID;       // E                                <space>
    char versionNo;             // 1                                <space>
    char appID;                 // <space>                          <space>
    char fieldCtlLength [2];    // 09                               <space><space>
    char fldAreaBaseAddr [5];   // Base address of field area (number of bytes in leader & dir)
    char extendedCharsetInd [3];// <space>!<space>                  <space><space><space>
    EntryMap entryMap;
};

struct ParsedLeader {
    uint32_t recLength;         // number of bytes in the record
    uint8_t versionNo;          // 1                                <space>
    uint8_t fieldCtlLength;     // 09                               <space><space>
    uint32_t fldAreaBaseAddr;   // Base address of field area (number of bytes in leader & dir)
    uint8_t fieldLengthSize;    // Field length field size (1..9)
    uint8_t fieldPosSize;       // Field position field size (1..9)
    uint8_t fieldTagSize;

    ParsedLeader (Leader *leader) {
        recLength = parseField (leader->recLength, sizeof (leader->recLength));
        versionNo = parseField (& leader->versionNo, sizeof (leader->versionNo));
        fieldCtlLength = parseField (leader->fieldCtlLength, sizeof (leader->fieldCtlLength));
        fldAreaBaseAddr = parseField (leader->fldAreaBaseAddr, sizeof (leader->fldAreaBaseAddr));
        fieldTagSize = leader->entryMap.fieldTagSize - '0';
        fieldPosSize = leader->entryMap.fieldPosSize - '0';
        fieldLengthSize = leader->entryMap.fieldLengthSize - '0';
    }
};

struct RecordFieldDesc {
    std::string tag;
    char format;
    std::optional<size_t> modifier;
};

struct DdfDesc {
    std::string name;
    std::vector<RecordFieldDesc> fields;
};

struct FieldInstance {
    char type;
    std::optional<uint32_t> intValue;
    std::optional<double> floatValue;
    std::optional<std::string> stringValue;
};

struct CatalogItem {
    bool binary;
    uint32_t rcid;
    std::string fileName;
    std::string volume;
    std::optional<std::string> crc;
    std::optional<double> northern;
    std::optional<double> southern;
    std::optional<double> eastern;
    std::optional<double> western;
};

#pragma pack()