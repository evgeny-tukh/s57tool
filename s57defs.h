#pragma once

#include <cstdint>

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

inline char FT = '\xe';
inline char UT = '\xf';

union EntryMap {
                                // Meaning or mandatory content
    char fieldLengthSize;       // Field length field size (1..9)
    char fieldPosSize;          // Field position field size (1..9)
    char reserved;              // 0
    char fieldTagSize;          // 4
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

#pragma pack()