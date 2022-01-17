#pragma once

#include <cstdint>
#include <optional>
#include <vector>
#include <map>
#include <tuple>
#include <optional>
#include <Windows.h>
#include <stdlib.h>
#include <string.h>

#pragma pack(1)

size_t splitString (std::string source, std::vector<std::string>& parts, char separator);

enum PaletteIndex {
    Day = 1,
    Dusk,
    Night,
};

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
    std::vector<uint8_t> binaryValue;
    
    std::string getBinaryValueString () {
        std::string result;
        for (uint8_t chr: binaryValue) {
            uint8_t low = chr & 15;
            uint8_t high = chr >> 4;
            result += high < 10 ? (high + '0') : (high - 10 + 'A');
            result += low < 10 ? (low + '0') : (low - 10 + 'A');
        }
        return result;
    }
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

struct Position {
    double lat;
    double lon;
    double depth;
};

/*struct GeoLine {
    uint32_t id;
    std::vector<Position> vertices;

    GeoLine (): id (0) {}
};

struct GeoArea {
    uint32_t id;
    std::vector<Node> extContour;
    std::vector<std::vector<Node>> intContours;

    GeoArea (): id (0) {}
};*/

struct AttrInstance {
    char domain;
    uint16_t classCode;
    std::string acronym;
    bool noValue, missing;
    uint32_t intValue;
    double floatValue;
    std::string strValue;
    std::vector<uint8_t> listValue;

    AttrInstance (): classCode (0), noValue (true), intValue (0), floatValue (0.0f), missing (false) {}
};

struct FeatureDesc {
    uint8_t group;                              // GRUP
    uint8_t geometry;                           // PRIM
    uint16_t classCode;                         // OBJL
    uint32_t id;                                // RCID
    uint16_t recordName;                        // RCNM
    std::optional<uint8_t> updateInstruction;   // RUIN
    std::optional<uint8_t> version;             // RVER
    uint32_t featureID;                         // FIDN
    std::optional<uint16_t> featureSubdiv;      // FIDS
    std::optional<uint16_t> agency;             // AGEN
    std::vector<AttrInstance> attributes;       // *ATTА

    FeatureDesc (): group (0), geometry (0), classCode (0), id (0), recordName (0), updateInstruction (0), version (0), featureID (0), featureSubdiv (0), agency (0) {}
};

struct ObjectDesc {
    uint32_t code;
    std::string acronym;
    std::string name;

    ObjectDesc (): code (0) {}
};

struct AttrDesc: ObjectDesc {
    char domain;
    std::string format;
    std::map<uint16_t, std::string> list;

    std::string listValue (uint16_t code) {
        if (domain != 'L' && domain != 'E') return std::string ("N/A");

        auto pos = list.find (code);

        return pos == list.end () ? std::string ("Unknown") : pos->second;
    }

    AttrDesc (): ObjectDesc (), domain ('\0') {}
};

typedef std::map<std::string, size_t> StringIndex;

struct GenericDictionary {
    std::map<uint16_t, size_t> indexByCode;
    StringIndex indexByAcronym;

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

    virtual ObjectDesc *itemAt (size_t index) {
        return (index >= 0 && index < items.size ()) ? & items [index] : 0;
    }

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

    virtual ObjectDesc *itemAt (size_t index) {
        return (index >= 0 && index < items.size ()) ? & items [index] : 0;
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
    StringIndex indexByAcronym;

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

struct ColorDef {
    std::string name;
    double x, y, luminance;
    double red, green, blue;

    ColorDef (): name (), red (0), green (0), blue (0) {}
    ColorDef (const char *_name, double _x, double _y, double _luminance):
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

struct ColorItem {
    ColorDef day, dusk, night;
    ColorDef *getColorDef (PaletteIndex index) {
        switch (index) {
            case PaletteIndex::Day: return & day;
            case PaletteIndex::Dusk: return & dusk;
            case PaletteIndex::Night: return & night;
            default: return 0;
        }
    }
};

struct ColorTable {
    std::vector<ColorItem> container;
    StringIndex index;

    size_t getColorIndex (const char *colorName) {
        auto pos = index.find (colorName);

        return (pos == index.end ()) ? (size_t) -1 : pos->second;
    }

    ColorItem *getItem (const char *colorName) {
        auto pos = index.find (colorName);

        return (pos == index.end ()) ? 0 : & container [pos->second];
    }

    size_t size () { return container.size (); }

    std::vector<ColorItem>::iterator begin () { return container.begin (); }
    std::vector<ColorItem>::iterator end () { return container.end (); }
    StringIndex::iterator colorBegin () { return index.begin (); }
    StringIndex::iterator colorEnd () { return index.end (); }
    
    ColorItem *getItemByPos (StringIndex::iterator pos) {
        return (pos == index.end ()) ? 0 : & container [pos->second];
    }

    ColorItem* checkAddColor (const char *colorName) {
        auto pos = index.find (colorName);

        if (pos == index.end ()) {
            index.emplace (colorName, container.size ());
            return & container.emplace_back ();
        } else {
            return & container [pos->second];
        }
    }
};

enum TableSet {
    PLAIN_BOUNDARIES = 1,
    SYMBOLIZED_BOUNDARIES,
    SIMPLIFIED,
    PAPER_CHARTS,
    LINES,
};

enum DisplayCat {
    STANDARD = 1,
    DISPLAY_BASE = 2,
    CUSTOM = 3,
};

enum DrawFlags {
    SET_PEN = 1,
    SET_BRUSH = 2,
    SET_CENTRAL_SYMBOL = 4,
    SET_CSP = 8,
};

struct LookupTableItem {
    char objectType, radarPriority;
    char acronym [6];
    uint8_t drawFlags;
    uint16_t classCode;
    uint32_t displayPriority;
    TableSet tableSet;
    std::vector<AttrInstance> attrCombination;
    std::string instruction;
    DisplayCat displayCat;
    std::string comment;
    size_t penIndex;
    size_t brushIndex;
    size_t centralSymbolIndex;
    size_t symbProcIndex;
    std::vector<size_t> symbols;

    static const size_t NOT_EXIST = 0xFFFFFFFFFFFFFFFF;

    void reset () {
        memset (acronym, 0, sizeof (acronym));
        symbols.clear ();
        comment.clear ();
        attrCombination.clear ();
        instruction.clear ();
        objectType = '\0';
        radarPriority = '\0';
        classCode = 0;
        displayPriority = 0;
        tableSet = TableSet::PLAIN_BOUNDARIES;
        displayCat = DisplayCat::STANDARD;
        penIndex = brushIndex = centralSymbolIndex = symbProcIndex = NOT_EXIST;
    }

    // Lookup table index key compose rule:
    // 2 bytes - object class code
    // 1 byte - ((<display category (base/standard)>) << 4) + <charset (plain/symboolized boundaries)>
    // 1 byte - object type (A/L/P)
    static uint32_t composeKey (uint16_t classCode, DisplayCat displayCat, TableSet tableSet, char objectType) {
        return ((uint32_t) classCode << 16) + ((((uint32_t) displayCat) & 15) << 12) + ((((uint32_t) tableSet) & 15) << 8) + (uint8_t) objectType;
    }

    uint32_t composeKey () {
        return composeKey (classCode, displayCat, tableSet, objectType);
    }

    void copyFrom (LookupTableItem& source) {
        memcpy (acronym, source.acronym, sizeof (source.acronym));
        attrCombination.insert (attrCombination.begin (), source.attrCombination.begin (), source.attrCombination.end ());
        instruction.insert (instruction.begin (), source.instruction.begin (), source.instruction.end ());
        symbols.insert (symbols.begin (), source.symbols.begin (), source.symbols.end ());
        comment = source.comment.c_str ();
        classCode = source.classCode;
        comment = source.comment;
        displayCat = source.displayCat;
        displayPriority = source.displayPriority;
        instruction = source.instruction;
        objectType = source.objectType;
        radarPriority = source.radarPriority;
        tableSet = source.tableSet;
        brushIndex = source.brushIndex;
        penIndex = source.penIndex;
        symbProcIndex = source.symbProcIndex;
        centralSymbolIndex = source.centralSymbolIndex;
    }
};

enum DrawOperCode {
    // Symbols
    NONE = 0,
    SELECT_PEN,
    SELECT_PEN_WIDTH,
    SELECT_TRANSP,
    PEN_UP,
    PEN_DOWN,
    CIRCLE,
    NEW_POLYGON,
    NEW_SHAPE,
    END_POLYGON,
    EXEC_POLYGON,
    FILL_POLYGON,
    SYMBOL_CALL,
};

struct Pens {
    HPEN container [5];
    HPEN *handles () { return container; }
    HPEN& operator[] (const size_t index) { return container [index]; }
    Pens () { memset (container, 0, sizeof (container)); }
    Pens (int): Pens () {}
    virtual ~Pens () {
        for (size_t i = 0; i < 5; ++ i) {
            if (container [i]) DeleteObject (container [i]);
        }
    }
};

typedef std::vector<LookupTableItem> LookupTable;

template<typename TYPE>
struct DrawToolItem {
    TYPE day, dusk, night;

    std::tuple<bool, TYPE&> get (PaletteIndex index) {
        switch (index) {
            case PaletteIndex::Day: return std::tuple<bool, TYPE&> (true, day);
            case PaletteIndex::Dusk: return std::tuple<bool, TYPE&> (true, dusk);
            case PaletteIndex::Night: return std::tuple<bool, TYPE&> (true, night);
            default: return std::tuple<bool, TYPE&> (false, *((TYPE *) 0));
        }
    }

    DrawToolItem () {}
    DrawToolItem (TYPE _day, TYPE _dusk, TYPE _night): day (_day), dusk (_dusk), night (_night) {}
};

struct Palette {
    std::vector<DrawToolItem <HPEN>> pens;
    std::vector<DrawToolItem <HBRUSH>> brushes;
    std::vector<DrawToolItem <Pens>> basePens;
    StringIndex colorIndex;
    StringIndex penIndex;
    StringIndex brushIndex;

    virtual ~Palette () {
        for (auto& penItem: pens) {
            DeleteObject (penItem.day);
            DeleteObject (penItem.dusk);
            DeleteObject (penItem.night);
        }
        for (auto& brushItem: brushes) {
            DeleteObject (brushItem.day);
            DeleteObject (brushItem.dusk);
            DeleteObject (brushItem.night);
        }
        for (auto& basePenItem: basePens) {
            for (size_t i = 0; i < 5; ++ i) {
                DeleteObject (basePenItem.day [i]);
                DeleteObject (basePenItem.dusk [i]);
                DeleteObject (basePenItem.night [i]);
            }
        }
    }

    size_t getColorIndex (const char *colorName) {
        auto pos = colorIndex.find (colorName);

        return pos == colorIndex.end () ? LookupTableItem::NOT_EXIST : pos->second;
    }

    void composeBasePens (ColorTable& colorTable) {
        colorIndex.clear ();
        basePens.clear ();
        for (auto pos = colorTable.colorBegin (); pos != colorTable.colorEnd (); ++ pos) {
            colorIndex.emplace (pos->first, pos->second);
            auto colorDesc = colorTable.getItemByPos (pos);
            auto& item = basePens.emplace_back ();
            for (int i = 0; i < 5; ++ i) {
                int penWidth = i + 1;
                item.day [i] = CreatePen (PS_SOLID, penWidth, RGB (colorDesc->day.red, colorDesc->day.green, colorDesc->day.blue));
                item.dusk [i] = CreatePen (PS_SOLID, penWidth, RGB (colorDesc->dusk.red, colorDesc->dusk.green, colorDesc->dusk.blue));
                item.night [i] = CreatePen (PS_SOLID, penWidth, RGB (colorDesc->night.red, colorDesc->night.green, colorDesc->night.blue));
            }
        }
    }

    HPEN *getBasePens (const char *colorName, PaletteIndex paletteIndex) {
        auto pos = colorIndex.find (colorName);

        if (pos == colorIndex.end ()) return 0;

        auto [exists, pens] = basePens [pos->second].get (paletteIndex);

        return exists ? pens.handles () : 0;
    }

    size_t checkPen (char *instr, ColorTable& colorTable) {
        auto pos = penIndex.find (instr);

        if (pos == penIndex.end ()) {
            char *leftBracket = strchr (instr, '(');
            char *rightBracket = leftBracket ? strchr (leftBracket + 1, ')') : 0;
            
            if (rightBracket - leftBracket > 1) {
                std::string lineType (leftBracket + 1, rightBracket - leftBracket - 1);
                std::vector<std::string> parts;

                splitString (lineType, parts, ',');

                if (parts.size () > 2) {
                    int penStyle, penWidth;
                    if (parts [0].compare ("SOLD") == 0) {
                        penStyle = PS_SOLID;
                    } else if (parts [0].compare ("DASH") == 0) {
                        penStyle = PS_DASH;
                    } else if (parts [0].compare ("DOT") == 0) {
                        penStyle = PS_DOT;
                    } else {
                        return -1;
                    }

                    penWidth = std::atoi (parts [1].c_str ());

                    if (penWidth == 0) return -1;

                    auto colorDesc = colorTable.getItem (parts [2].c_str ());

                    if (!colorDesc) return LookupTableItem::NOT_EXIST;

                    size_t index = pens.size ();
                    pens.emplace_back (
                        CreatePen (penStyle, penWidth, RGB (colorDesc->day.red, colorDesc->day.green, colorDesc->day.blue)),
                        CreatePen (penStyle, penWidth, RGB (colorDesc->dusk.red, colorDesc->dusk.green, colorDesc->dusk.blue)),
                        CreatePen (penStyle, penWidth, RGB (colorDesc->night.red, colorDesc->night.green, colorDesc->night.blue))
                    );
                    penIndex.emplace (instr, index);
                    return index;
                }
            }
        }
        
        return pos->second;
    }

    size_t checkSolidBrush (const char *colorName, ColorTable& colorTable) {
        auto pos = brushIndex.find (colorName);

        if (pos == brushIndex.end ()) {
            auto colorDesc = colorTable.getItem (colorName);

            if (!colorDesc) return LookupTableItem::NOT_EXIST;

            size_t index = brushes.size ();
            brushes.emplace_back (
                CreateSolidBrush (RGB (colorDesc->day.red, colorDesc->day.green, colorDesc->day.blue)),
                CreateSolidBrush (RGB (colorDesc->dusk.red, colorDesc->dusk.green, colorDesc->dusk.blue)),
                CreateSolidBrush (RGB (colorDesc->night.red, colorDesc->night.green, colorDesc->night.blue))
            );
            brushIndex.emplace (/*instr*/colorName, index);
            return index;
        }
        
        return pos->second;
    }
};

struct SymbolDesc;
struct PatternDesc;
struct LineDesc;

struct Dai {
    LibraryIdentification libraryId;
    ColorTable colorTable;
    Palette palette;
    StringIndex penIndex;
    StringIndex basePenIndex;
    StringIndex brushIndex;
    std::vector<LookupTable> lookupTables;
    std::map<uint32_t, size_t> lookupTableIndex;
    std::map<std::string, PatternDesc> patterns;
    std::map<std::string, SymbolDesc> symbols;
    std::map<std::string, LineDesc> lines;

    LookupTable *findLookupTable (uint16_t code, DisplayCat displayCat, TableSet tableSet, char objectType) {
        auto pos = lookupTableIndex.find (LookupTableItem::composeKey (code, displayCat, tableSet, objectType));

        return pos == lookupTableIndex.end () ? 0 : & lookupTables [pos->second];
    }
};

struct DrawOper {
    DrawOperCode oper;
    std::vector<uint64_t> args;
    Pens *dayPens, *duskPens, *nightPens;

    DrawOper (): oper (DrawOperCode::NONE), dayPens (0), duskPens (0), nightPens (0) {}
};

struct DrawProcedure {
    std::vector<DrawOper> instructions;
    std::map<char, std::string> penColors;

    const char *getPenColorName (char code) {
        auto pos = penColors.find (code);
        return pos == penColors.end () ? 0 : pos->second.c_str ();
    }

    void definePen (char code, const char *colorName) {
        if (penColors.find (code) == penColors.end ()) {
            penColors.emplace (code, colorName);
        }
    }

    void defineOperation (const char *operDesc, Dai& dai) {
        static const char *SP = "SP";
        static const char *ST = "ST";
        static const char *SW = "SW";
        static const char *PU = "PU";
        static const char *PD = "PD";
        static const char *CI = "CI";
        static const char *EP = "EP";
        static const char *FP = "FP";
        static const char *SC = "SC";
        static const char *PM = "PM";

        auto isOperCode = [&operDesc] (const char *code) {
            return *((uint16_t *) code) == *((uint16_t *) operDesc);
        };

        if (isOperCode (SP)) {
            // Pen selection
            const char *colorName = getPenColorName (operDesc [2]);
            size_t colorIndex = dai.colorTable.getColorIndex (colorName);

            if (colorIndex != LookupTableItem::NOT_EXIST) {
                auto& oper = instructions.emplace_back ();
                oper.oper = DrawOperCode::SELECT_PEN;
                oper.args.emplace_back (colorIndex);
            }
        } else if (isOperCode (SW)) {
            // Pen width selection, arg is one digit-int
            if (isdigit (operDesc [2])) {
                auto& oper = instructions.emplace_back ();
                oper.oper = DrawOperCode::SELECT_PEN_WIDTH;
                oper.args.emplace_back (operDesc [2] - '0');
            }
        } else if (isOperCode (CI)) {
            // Draw a circle, arg is a radius
            if (isdigit (operDesc [2])) {
                auto& oper = instructions.emplace_back ();
                oper.oper = DrawOperCode::CIRCLE;
                oper.args.emplace_back (std::atoi (operDesc + 2));
            }
        } else if (isOperCode (ST)) {
            // Pen transparency selection, arg is a number of quarters, so 0 is 0%, 1 is 25% etc.
            if (isdigit (operDesc [2])) {
                auto& oper = instructions.emplace_back ();
                oper.oper = DrawOperCode::SELECT_TRANSP;
                oper.args.emplace_back ((operDesc [2] - '0') * 25);
            }
        } else if (isOperCode (PM)) {
            // Polygon control command, arg is command id (0/1/2 - enter, new shape, end).
            DrawOperCode operCode;
            switch (operDesc [2]) {
                case '0': operCode = DrawOperCode::NEW_POLYGON; break;
                case '1': operCode = DrawOperCode::NEW_SHAPE; break;
                case '2': operCode = DrawOperCode::END_POLYGON; break;
                default: return;
            }

            auto& oper = instructions.emplace_back ();
            oper.oper = operCode;
        } else if (isOperCode (PU)) {
            // Pen up command, args are x and y
            if (isdigit (operDesc [2])) {
                char *comma = strchr ((char *) operDesc + 2, ',');

                if (comma && isdigit (comma [1])) {
                    auto& oper = instructions.emplace_back ();
                    oper.oper = DrawOperCode::PEN_UP;
                    oper.args.emplace_back (std::atoi (operDesc + 2));
                    oper.args.emplace_back (std::atoi (comma + 1));
                }
            }
        } else if (isOperCode (SC)) {
            // Symbol call, args are symbol name and orientation 0/1/2
            auto& oper = instructions.emplace_back ();
            oper.oper = DrawOperCode::SYMBOL_CALL;
            for (size_t i = 0; i < 8; oper.args.emplace_back (operDesc [(i++)+2]));
            oper.args.emplace_back (operDesc [11] - '0');
        } else if (isOperCode (FP)) {
            // Fill polygon, no args
            auto& oper = instructions.emplace_back ();
            oper.oper = DrawOperCode::FILL_POLYGON;
        } else if (isOperCode (EP)) {
            // Exec polygon, no args
            auto& oper = instructions.emplace_back ();
            oper.oper = DrawOperCode::EXEC_POLYGON;
        } else if (isOperCode (PD)) {
            // Pen down command, args are x and y pairs (one or more)
            if (isdigit (operDesc [2])) {
                auto extractPoint = [] (char *instr) {
                    if (isdigit (instr [0])) {
                        char *comma = strchr (instr + 1, ',');

                        if (comma && isdigit (comma [1])) {
                            char *nextComma = strchr (comma + 1, ',');

                            int x = std::atoi (instr);
                            int y = std::atoi (comma + 1);
                            bool morePoints = nextComma && isdigit (nextComma [1]);
                            char *nextChar = morePoints ? nextComma + 1 : 0;

                            return std::tuple (true, x, y, morePoints, nextChar);
                        }
                    }

                    return std::tuple (false, 0, 0, false, (char *) 0);
                };
                char *source = (char *) operDesc + 2;
                while (true) {
                    auto [isPoint, x, y, morePoints, nextChar] = extractPoint (source);

                    if (isPoint) {
                        auto& oper = instructions.emplace_back ();
                        oper.oper = DrawOperCode::PEN_DOWN;
                        oper.args.emplace_back (x);
                        oper.args.emplace_back (y);
                    }

                    if (!morePoints) break;

                    source = nextChar;
                }
            }
        }
    }
};

enum FillType {
    STRAGGERED = 1,
    LINEAR,
};

enum Spacing {
    CONSTANT = 1,
    SCALE_DEPENDENT,
};

struct PatternDesc {
    char name [8];
    char type;  // V/R
    FillType fillType;
    Spacing spacing;
    uint32_t minDistance;
    uint32_t maxDistance;
    uint32_t pivotPtCol;
    uint32_t pivotPtRow;
    uint32_t bBoxWidth;
    uint32_t bBoxHeight;
    uint32_t bBoxCol;
    uint32_t bBoxRow;
    std::string exposition;
    std::string color;
    std::string bitmap;
    std::vector<std::vector<std::string>> svgs;
};

struct LineDesc {
    char name [8];
    uint32_t pivotPtCol;
    uint32_t pivotPtRow;
    uint32_t bBoxWidth;
    uint32_t bBoxHeight;
    uint32_t bBoxCol;
    uint32_t bBoxRow;
    std::string exposition;
    std::string color;
    std::vector<std::vector<std::string>> svgs;
};

struct SymbolDesc {
    char name [8];
    char type;  // V/R
    uint32_t pivotPtCol;
    uint32_t pivotPtRow;
    uint32_t bBoxWidth;
    uint32_t bBoxHeight;
    uint32_t bBoxCol;
    uint32_t bBoxRow;
    std::string exposition;
    std::string color;
    std::string bitmap;
    std::vector<std::vector<std::string>> svgs;
    DrawProcedure drawProc;
};


#pragma pack()