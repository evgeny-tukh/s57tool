#pragma once

#include <cstdint>
#include <optional>
#include <vector>
#include <map>
#include <tuple>
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

enum DUNI {
    DepthMeters = 1,
    DepthFathomsFeet = 2,
    DepthFeet = 3,
    DepthFathomsFractions = 4,
};

enum HUNI {
    HorMeters = 1,
    HorFeet = 2,
};

enum PUNI {
    PosMeters = 1,
    PosDegreesOfArc = 2,
    PosMillimeters = 3,
    PosFeet = 4,
    PosCables = 5,
};

enum HDAT {
    WGS84 = 2,
};

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

struct SubFieldInstance {
    char type;
    bool signedValue;
    std::optional<uint32_t> intValue;
    std::optional<double> floatValue;
    std::optional<std::string> stringValue;
};

struct FieldInstance {
    std::string tag;
    std::string name;
    // Field instance value might be single or repeating (for example for 2D and 3D points)
    std::vector<std::map<std::string, SubFieldInstance>> instanceValues;
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

struct DatasetParams {
    std::optional<uint32_t> coordMultiplier;    // COMF
    std::optional<uint32_t> soundingMultiplier; // SOMF
    std::optional<std::string> comment;         // COMT
    std::optional<COUN> coordUnit;              // COUN
    std::optional<uint32_t> compilationScale;   // CSCL
    std::optional<DUNI> depthMeasurement;       // DUNI
    std::optional<HUNI> heightMeasurement;      // HUNI
    std::optional<PUNI> posMeasurement;         // PUNI
    std::optional<HDAT> horDatum;               // HDAT
    std::optional<uint32_t> soundingDatum;      // VERDAT values
    std::optional<uint32_t> verDatum;           // VERDAT values
};

struct FeatureDesc {
    std::optional<uint8_t> group;               // GRUP
    std::optional<uint8_t> geometry;            // PRIM
    std::optional<uint16_t> classCode;          // OBJL
    std::optional<uint32_t> id;                 // RCID
    std::optional<uint16_t> recordName;         // RCNM
    std::optional<uint8_t> updateInstruction;   // RUIN
    std::optional<uint8_t> version;             // RVER
    std::optional<uint32_t> featureID;          // FIDN
    std::optional<uint16_t> featureSubdiv;     // FIDS
    std::optional<uint16_t> agency;             // AGEN
};

struct ObjectDesc {
    uint32_t code;
    std::string acronym;
    std::string name;
};

struct AttrDesc: ObjectDesc {
    char domain;
    std::string format;
    std::map<uint16_t, std::string> list;
};

struct GenericDictionary {
    std::map<uint16_t, size_t> indexByCode;
    std::map<std::string, size_t> indexByAcronym;

    virtual void clearItems () {}

    virtual ObjectDesc *itemAt (size_t index) { return 0; }

    void clear () {
        clearItems ();
        indexByAcronym.clear ();
        indexByCode.clear ();
    }

    ObjectDesc *findByCode (uint16_t code) {
        auto item = indexByCode.find (code);

        return (item == indexByCode.end ()) ? 0 : itemAt (item->second);
    }

    ObjectDesc *findByAcronym (const char *acronym) {
        auto item = indexByAcronym.find (acronym);

        return (item == indexByAcronym.end ()) ? 0 : itemAt (item->second);
    }
};

struct ObjectDictionary: GenericDictionary {
    std::vector<ObjectDesc> items;

    virtual void clearItems () {
        items.clear ();
    }

    void checkAddObject (uint16_t code, const char *acronym, const char *name) {
        auto codePos = indexByCode.find (code);
        auto acronymPos = indexByAcronym.find (acronym);
        size_t entryIndex;

        if (codePos == indexByCode.end () && acronymPos == indexByAcronym.end ()) {
            entryIndex = items.size ();
            auto& item = items.emplace_back ();
            item.code = code;
            item.acronym = acronym;
            item.name = name;
            indexByCode.emplace (code, entryIndex);
            indexByAcronym.emplace (acronym, entryIndex);
        } else if (codePos == indexByCode.end ()) {
            indexByCode.emplace (code, acronymPos->second).first;
        } else if (acronymPos == indexByAcronym.end ()) {
            indexByAcronym.emplace (acronym, codePos->second).first;
        }
    }
};

struct AttrDictionary: GenericDictionary {
    std::vector<AttrDesc> items;

    AttrDesc& lastItem () {
        return items.back ();
    }

    virtual void clearItems () {
        items.clear ();
    }

    void checkAddAttr (uint16_t code, char domain, const char *acronym, const char *name) {
        auto codePos = indexByCode.find (code);
        auto acronymPos = indexByAcronym.find (acronym);
        size_t entryIndex;

        if (codePos == indexByCode.end () && acronymPos == indexByAcronym.end ()) {
            entryIndex = items.size ();
            auto& item = items.emplace_back ();
            item.code = code;
            item.acronym = acronym;
            item.domain = domain;
            item.name = name;
            indexByCode.emplace (code, entryIndex);
            indexByAcronym.emplace (acronym, entryIndex);
        } else if (codePos == indexByCode.end ()) {
            indexByCode.emplace (code, acronymPos->second).first;
        } else if (acronymPos == indexByAcronym.end ()) {
            indexByAcronym.emplace (acronym, codePos->second).first;
        }
    }
};

#if 0
struct ObjectDictionary {
    std::vector<ObjectDesc> items;
    std::map<uint16_t, size_t> indexByCode;
    std::map<std::string, size_t> indexByAcronym;

    void clear () {
        items.clear ();
        indexByAcronym.clear ();
        indexByCode.clear ();
    }

    ObjectDesc *findByCode (uint16_t code) {
        auto item = indexByCode.find (code);

        return (item == indexByCode.end ()) ? 0 : & items [item->second];
    }

    ObjectDesc *findByAcronym (const char *acronym) {
        auto item = indexByAcronym.find (acronym);

        return (item == indexByAcronym.end ()) ? 0 : & items [item->second];
    }

    void checkAddObject (uint16_t code, const char *acronym, const char *name) {
        auto codePos = indexByCode.find (code);
        auto acronymPos = indexByAcronym.find (acronym);
        size_t entryIndex;

        if (codePos == indexByCode.end () && acronymPos == indexByAcronym.end ()) {
            entryIndex = items.size ();
            auto& item = items.emplace_back ();
            item.code = code;
            item.acronym = acronym;
            item.name = name;
            indexByCode.emplace (code, entryIndex);
            indexByAcronym.emplace (acronym, entryIndex);
        } else if (codePos == indexByCode.end ()) {
            indexByCode.emplace (code, acronymPos->second).first;
        } else if (acronymPos == indexByAcronym.end ()) {
            indexByAcronym.emplace (acronym, codePos->second).first;
        }
    }
};
#endif

// DAI defs
struct LibraryIdentification {
    std::string moduleName;
    uint32_t rcid;
    std::string exchangePurpose;
    std::string productType;
    std::string exchangeSetSerialNo;
    std::string editionNo;
    std::string compilationDate;
    std::string compilationTime;
    std::string libraryProfileVersionsDate;
    std::string libaryAppProfile;
    std::string objectCatVersionDate;
    std::string comment;
};

struct ColorItem {
    std::string name;
    double x, y, luminance;
    double red, green, blue;

    ColorItem (): name (), red (0), green (0), blue (0) {}
    ColorItem (const char *_name, double _x, double _y, double _luminance):
        name (_name),
        x (_x),
        y (_y),
        luminance (_luminance) {
        double X = _x * _luminance / _y;
        double Y = _luminance;
        double Z = luminance * (1.0 - _x - _y) / _y;
        //red = 0.41847 * X - 0.15866 * Y - 0.082835 * Z;
        //green = - 0.091169 * X + 0.25243 * Y + 0.015708 * Z;
        //blue = 0.00092090 * X  - 0.0025498 * Y  + 0.17860 * Z;
        red =  3.2404542*X - 1.5371385*Y - 0.4985314*Z;
        green = -0.9692660*X + 1.8760108*Y + 0.0415560*Z;
        blue =  0.0556434*X - 0.2040259*Y + 1.0572252*Z;      
    }
};

enum TableSet {
    PLAIN_BOUNDARIES = 1,
    SYMBOLIZED_BOUNDARIES,
    SIMPLIFIED,
    PAPER_CHARTS,
    LINES,
};

struct AttrInstance {
    char acronym [6];
    uint16_t classCode;
    double flatVal;
    uint32_t intVal;
    std::string strVal;
};

enum DisplayCat {
    STANDARD = 1,
    DISPLAY_BASE = 2,
    CUSTOM = 3,
};

struct LookupTableItem {
    char objectType, radarPriority;
    char acronym [6];
    uint16_t classCode;
    uint32_t displayPriority;
    TableSet tableSet;
    std::vector<AttrInstance> attrCombination;
    std::string instruction;
    DisplayCat displayCat;
    std::string comment;
};

struct Dai {
    LibraryIdentification libraryId;
    std::map<std::string, ColorItem> dayColorTable, duskColorTable, nightColorTable;
    std::map<std::string, std::vector<LookupTableItem>> lookupTables;
};

#pragma pack()