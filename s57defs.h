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
#include "csp.h"
#include "geo.h"

#pragma pack(1)

size_t splitString (std::string source, std::vector<std::string>& parts, char separator);
HBRUSH createPatternBrush (struct PatternDesc& pattern, enum PaletteIndex paletteIndex, struct Dai& dai);

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

    void checkAddObject (uint16_t code, const char *acronym, const char *name);
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

    void checkAddAttr (uint16_t code, char domain, const char *acronym, const char *name);
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

        return (pos == index.end ()) ? (size_t) 0xFFFFFFFFFFFFFFFF : pos->second;
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
    UNKNOWN_DATASET,
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

struct ArcDef {
    double radius, start, end;
    size_t nodeIndex, internalColor;

    static const int AT_OBJECT_CENTER = 0x7FFFFFFF;
};

struct SymbolDraw {
    size_t symbolIndex;
    double rotAngle;

    SymbolDraw (): symbolIndex (0xFFFFFFFFFFFFFFFF), rotAngle (0.0) {}
    SymbolDraw (size_t _symbolIndex): symbolIndex (_symbolIndex), rotAngle (0.0) {}
    SymbolDraw (size_t _symbolIndex, double _rotAngle): symbolIndex (_symbolIndex), rotAngle (_rotAngle) {}
};

enum LineDrawMode {
    BETWEEN_TWO_POINTS,
    USING_BRG_AND_RNG,
    ARC,
};

struct LineDraw {
    size_t penIndex; // Non-base pen!
    LineDrawMode mode;
    double lat1, lon1, lat2, lon2, brg, endBrg, lengthMm;

    LineDraw (
        size_t _penIndex,
        LineDrawMode _mode,
        double _centerLat,
        double _centerLon,
        double _brg1,
        double _brg2,
        double _radiusMm
    ): penIndex (_penIndex), mode (_mode), lat1 (_centerLat), lon1 (_centerLon), lat2 (0.0), lon2 (0.0), lengthMm (0.0), brg (0.0), endBrg (0.0) {
        if (mode == LineDrawMode::ARC) {
            brg = _brg1;
            endBrg = _brg2;
            lengthMm = _radiusMm;
        }
    }
    LineDraw (
        size_t _penIndex,
        LineDrawMode _mode,
        double _lat1,
        double _lon1,
        double _arg5,
        double _arg6
    ): penIndex (_penIndex), mode (_mode), lat1 (_lat1), lon1 (_lon1), lat2 (0.0), lon2 (0.0), lengthMm (0.0), brg (0.0), endBrg (0.0) {
        if (mode == LineDrawMode::BETWEEN_TWO_POINTS) {
            lat2 = _arg5;
            lon2 = _arg6;
        } else if (mode == LineDrawMode::USING_BRG_AND_RNG) {
            brg = _arg5;
            lengthMm = _arg6;
        }
    }
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
    size_t patternBrushIndex;
    size_t centralSymbolIndex;
    size_t procIndex;
    size_t edgePenIndex;
    int edgePenStyle;
    int edgePenWidth;
    int viewingGroup;
    std::vector<SymbolDraw> symbols;
    std::vector<LineDraw> lines;
    std::vector<std::string> textInstructions;
    std::vector<struct TextDesc> textDescriptions;
    bool customEdgePres;

    // Lights only, specified in LIGHTS06 procedre
    ArcDef arcDef;
    bool drawArc;

    static const size_t NOT_EXIST = 0xFFFFFFFFFFFFFFFF;

    LookupTableItem (LookupTableItem &source) {
        copyFrom (source);
    }
    LookupTableItem (LookupTableItem *source) {
        copyFrom (*source);
    }
    LookupTableItem () {
        reset ();
    }

    void addSymbol (size_t index, double rotAngle) {
        symbols.emplace_back (index, rotAngle);
    }

    void reset () {
        memset (acronym, 0, sizeof (acronym));
        symbols.clear ();
        comment.clear ();
        attrCombination.clear ();
        textInstructions.clear ();
        textDescriptions.clear ();
        instruction.clear ();
        objectType = '\0';
        radarPriority = '\0';
        classCode = 0;
        displayPriority = 0;
        tableSet = TableSet::PLAIN_BOUNDARIES;
        displayCat = DisplayCat::STANDARD;
        edgePenIndex = penIndex = brushIndex = patternBrushIndex = centralSymbolIndex = procIndex = NOT_EXIST;
        edgePenStyle = PS_SOLID;
        edgePenWidth = 1;
        drawArc = false;
        viewingGroup = 0;
        customEdgePres = false;
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
        textInstructions.insert (textInstructions.begin (), source.textInstructions.begin (), source.textInstructions.end ());
        textDescriptions.insert (textDescriptions.begin (), source.textDescriptions.begin (), source.textDescriptions.end ());
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
        patternBrushIndex = source.patternBrushIndex;
        penIndex = source.penIndex;
        procIndex = source.procIndex;
        centralSymbolIndex = source.centralSymbolIndex;
        drawArc = source.drawArc;
        arcDef = source.arcDef;
        edgePenIndex = source.edgePenIndex;
        edgePenStyle = source.edgePenStyle;
        edgePenWidth = source.edgePenWidth;
        viewingGroup = source.viewingGroup;
        customEdgePres = source.customEdgePres;
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
    HPEN container [6];
    HPEN *handles () { return container; }
    HPEN& operator[] (const size_t index) { return container [index]; }
    Pens () { memset (container, 0, sizeof (container)); }
    Pens (int): Pens () {}
    virtual ~Pens () {
        for (size_t i = 0; i <= 6; ++ i) {
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

struct SymbolDesc;
struct PatternDesc;
struct LineDesc;
struct Dai;

struct Palette {
    std::vector<DrawToolItem <HPEN>> pens;
    std::vector<DrawToolItem <HBRUSH>> brushes, patternBrushes;
    std::vector<DrawToolItem <Pens>> basePens;
    StringIndex colorIndex;
    StringIndex penIndex;
    StringIndex brushIndex;
    StringIndex patternBrushIndex;

    virtual ~Palette ();

    size_t getColorIndex (const char *colorName) {
        auto pos = colorIndex.find (colorName);

        return pos == colorIndex.end () ? LookupTableItem::NOT_EXIST : pos->second;
    }

    HPEN *getBasePens (const char *colorName, PaletteIndex paletteIndex) {
        auto pos = colorIndex.find (colorName);

        if (pos == colorIndex.end ()) return 0;

        auto& [exists, pens] = basePens [pos->second].get (paletteIndex);

        return exists ? pens.handles () : 0;
    }

    size_t checkPen (char *instr, ColorTable& colorTable);
    size_t checkSolidBrush (const char *colorName, ColorTable& colorTable);
    size_t checkPatternBrush (PatternDesc& pattern, struct Dai& dai);
    size_t getPatternBrushIndex (const char *patternName);
    size_t getSolidBrushIndex (const char *colorName);
};

typedef void (*CSP) (
    LookupTableItem *item,
    struct FeatureObject *object,
    struct Dai& dai,
    struct Nodes& nodes,
    struct Edges& edges,
    struct Features& features,
    int zoom,
    struct DrawQueue& drawQueue
);

struct Dai {
    LibraryIdentification libraryId;
    ColorTable colorTable;
    Palette palette;
    StringIndex penIndex;
    StringIndex brushIndex;
    StringIndex patternIndex;
    StringIndex symbolIndex;
    StringIndex lineIndex;
    StringIndex bitmapIndex;
    StringIndex procIndex;
    std::vector<LookupTable> lookupTables;
    std::map<uint32_t, size_t> lookupTableIndex;
    std::vector<PatternDesc> patterns;
    std::vector<SymbolDesc> symbols;
    std::vector<LineDesc> lines;
    std::vector<CSP> procedures;

    Dai () {
        initCSPs (*this);
    }

    LookupTable *findLookupTable (uint16_t code, DisplayCat displayCat, TableSet tableSet, char objectType) {
        auto pos = lookupTableIndex.find (LookupTableItem::composeKey (code, displayCat, tableSet, objectType));

        return pos == lookupTableIndex.end () ? 0 : & lookupTables [pos->second];
    }
    size_t getItemIndex  (const char *name, StringIndex& index) {
        auto pos = index.find (name);
        return pos == index.end () ? LookupTableItem::NOT_EXIST : pos->second;
    }
    size_t getSymbolIndex (const char *name) {
        return getItemIndex (name, symbolIndex);
    }
    size_t getProcIndex (const char *name) {
        return getItemIndex (name, procIndex);
    }
    size_t getLineIndex (const char *name) {
        return getItemIndex (name, lineIndex);
    }
    size_t getPenIndex (const char *name) {
        return getItemIndex (name, palette.penIndex);
    }
    size_t getBasePenIndex (const char *name) {
        return getItemIndex (name, colorTable.index);
    }
    size_t getPatternIndex (const char *name) {
        return getItemIndex (name, patternIndex);
    }
    void composePatternBrushes ();
    void addCSP (const char *name, CSP proc) {
        procIndex.emplace (name, procedures.size ());
        procedures.emplace_back (proc);
    }
    void runCSP (LookupTableItem *item, struct FeatureObject *object, Nodes& nodes, Edges& edges, Features& features, int zoom, struct DrawQueue& drawQueue) {
        if (item->procIndex >= 0 && item->procIndex < procedures.size ()) {
            procedures [item->procIndex] (item, object, *this, nodes, edges, features, zoom, drawQueue);
        }
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
    std::map<char, size_t> penColors;

    size_t getPenColorIndex (char code) {
        auto pos = penColors.find (code);
        return pos == penColors.end () ? LookupTableItem::NOT_EXIST : pos->second;
    }

    void definePen (char code, const char *colorName, StringIndex& colorIndex) {
        if (penColors.find (code) == penColors.end ()) {
            auto pos = colorIndex.find (colorName);

            if (pos != colorIndex.end ()) {
                penColors.emplace (code, pos->second);
            }
        }
    }

    void defineOperation (const char *operDesc, Dai& dai);
};

enum FillType {
    STRAGGERED = 1,
    LINEAR,
};

enum Spacing {
    CONSTANT = 1,
    SCALE_DEPENDENT,
};

struct DrawableObjDesc {
    std::string name;
    uint32_t pivotPtCol;
    uint32_t pivotPtRow;
    std::string exposition;
    std::string color;
    uint32_t bBoxWidth;
    uint32_t bBoxHeight;
    uint32_t bBoxCol;
    uint32_t bBoxRow;
    std::vector<std::vector<std::string>> svgs;
    DrawProcedure drawProc;
};

struct PatternDesc: DrawableObjDesc {
    char type;  // V/R
    FillType fillType;
    Spacing spacing;
    uint32_t minDistance;
    uint32_t maxDistance;
    std::string bitmap;
};

struct LineDesc: DrawableObjDesc {
};

struct SymbolDesc: DrawableObjDesc {
    char type;  // V/R
    std::string bitmap;
};

struct TextDesc {
    enum HorJust {
        CENTER = 1,
        RIGHT = 2,
        LEFT = 3,
    };
    enum VerJust {
        BOTTOM = 1,
        CENT = 2,
        TOP = 3,
    };
    enum Spacing {
        FIT = 1,
        STD = 2,
        WORD_WRAP = 3,
    };
    enum FontType {
        SERIF = 1,
    };
    enum FontWeight {
        LIGHT = 4,
        MEDIUM = 5,
        BOLD = 6,
    };
    enum FontStyle {
        REGULAR = 1,
    };
    enum ParamType {
        INT_VAL,
        FLOAT_VAL,
        STRING_VAL,
        UNKNOWN_TYPE,
    };
    struct ParamDesc {
        ParamType type;
        std::string format;
        size_t classCode;
    };

    HorJust horJust;
    VerJust verJust;
    Spacing spacing;
    FontType fontType;
    FontWeight fontWeight;
    FontStyle fontStyle;
    int fontSize;
    int horOffset;
    int verOffset;
    size_t colorIndex;
    std::vector<ParamDesc> paramDescs;
    std::vector<std::string> plainTextParts;
};

#pragma pack()