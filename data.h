#pragma once

#include <stdlib.h>
#include <cstdint>
#include <vector>
#include <map>
#include <string>
#include <tuple>
#include "s57defs.h"
#include "geo.h"
#include "settings.h"

enum NodeFlags {
    CONNECTED = 1,
    SOUNDING_ARRAY = 2,
};

struct TopologyObject {
    uint32_t id;
    uint8_t recordName;
    uint8_t flags;
    uint32_t updateInstruction;
    uint32_t version;

    TopologyObject (): id (0), recordName (0), flags (0), updateInstruction (0), version (0) {}
};

struct GeoNode: TopologyObject {
    std::vector<Position> points;

    GeoNode (): TopologyObject () {}
};

enum Orient {
    FWD = 1,
    REV = 2,
    UNKNOWN = 255,
};

struct EdgeNode: Position {
    bool hidden;
    bool hole;

    EdgeNode (): Position (), hidden (false), hole (false) {}
};

struct Attr {
    uint16_t classCode;
    std::string acronym;
    bool noValue;
    uint32_t intValue;
    double floatValue;
    std::string strValue;
    std::vector<uint8_t> listValue;

    Attr (): classCode (0), noValue (true), intValue (0), floatValue (0.0f) {}

    bool listIncludes (uint8_t value) {
        for (uint8_t byte: listValue) {
            if (byte == value) return true;
        }
        return false;
    }
    bool listIncludes (uint8_t *values) {
        for (uint8_t *value = values; *value; ++ value) {
            for (uint8_t byte: listValue) {
                if (byte == *value) return true;
            }
        }
        return false;
    }
};

struct GeoEdge: TopologyObject {
    Orient orientation;
    size_t beginIndex;
    size_t endIndex;
    std::vector<Position> internalNodes;
    bool hidden;
    bool hole;
    std::vector<Attr> attributes;

    GeoEdge (): TopologyObject (), orientation (Orient::UNKNOWN), beginIndex (-1), endIndex (-1), hidden (false), hole (false) {}
};

struct EdgeRef {
    size_t index;
    bool hidden;
    bool hole;
    bool unclockwise;
    bool customPres;
    size_t penIndex;
    int penStyle;
    int penWidth;
    int viewingGroup;
    int displayPriority;
    DisplayCat dispCat;

    EdgeRef ():
        customPres (false),
        hidden (false),
        hole (false),
        unclockwise (false),
        penIndex (-1),
        penStyle (0),
        penWidth (0),
        viewingGroup (0),
        dispCat (DisplayCat::DISPLAY_BASE),
        displayPriority (0) {
    }
};

struct FeatureObject: TopologyObject {
    uint8_t primitive;
    uint8_t group;
    uint16_t classCode;
    uint16_t agency;
    uint32_t fidn;
    uint16_t subDiv;
    std::vector<Attr> attributes;
    std::vector<EdgeRef> edgeRefs;    // Lines and areas
    size_t nodeIndex;                 // Point and sounding array

    FeatureObject (): TopologyObject (), primitive (PRIM::None), group (1), classCode (0), agency (0), fidn (0), subDiv (0), nodeIndex (-1) {}

    Attr *getAttr (const char *acronym, struct AttrDictionary& dic);
    Attr *getAttr (size_t classCode) {
        for (auto& attr: attributes) {
            if (attr.classCode == classCode) return &attr;
        }
        return 0;
    }

    Attr *getEdgeAttr (EdgeRef& edgeRef, const char *acronym, struct AttrDictionary& dic, struct Edges& edges);
    Attr *getEdgeAttr (EdgeRef& edgeRef, size_t classCode, struct Edges& edges);

    LookupTableItem *findBestItem (DisplayCat displayCat, TableSet tableSet, Dai& dai, int priority = -1) {
        char objectType;
        
        if (primitive == PRIM::Area) {
            objectType = 'A';
        } else if (primitive == PRIM::Line) {
            objectType = 'L';
        } else if (primitive == PRIM::Point) {
            objectType = 'P';
        } else {
            return 0;
        }
        
        LookupTable *table = dai.findLookupTable (classCode, displayCat, tableSet, objectType);
        
        if (!table) return 0;
        
        LookupTableItem *result = & table->at (0);

        for (size_t i = 1; i < table->size (); ++ i) {
            LookupTableItem *item = & table->at (i);
            bool itemOk = true;
            for (size_t j = 0; j < item->attrCombination.size (); ++ j) {
                auto& attrRequired = item->attrCombination [j];
                bool attrFound = true;

                if (attrRequired.noValue && !attrRequired.strValue.empty ()) {
                    attrRequired.noValue = false;
                }

                if (attrRequired.missing) {
                    for (auto& attr: attributes) {
                        if (attr.classCode == attrRequired.classCode) {
                            // The attribute should be missing but it exists
                            // Skip this item
                            attrFound = false; break;
                        }
                    }
                } else {
                    for (auto& attr: attributes) {
                        if (attr.classCode == attrRequired.classCode) {
                            // The attribute found, just check value (if it is needed)
                            if (attrRequired.noValue) {
                                attrFound = true;
                            } else if (attr.noValue) {
                                attrFound = false;
                            } else {
                                switch (attrRequired.domain) {
                                    case 'I':
                                    case 'E':
                                        attrFound = attr.intValue == attrRequired.intValue; break;
                                    case 'F':
                                        attrFound = attr.floatValue == attrRequired.floatValue; break;
                                    case 'L':
                                        attrFound = false;
                                        for (uint8_t byte: attr.listValue) {
                                            if (byte == std::atoi (attrRequired.strValue.c_str ())) {
                                                attrFound = true; break;
                                            }
                                        }
                                        break;
                                    default:
                                        attrFound = true; break; // TO BE DONE!!!!!
                                }
                            }
                            break;
                        }
                    }
                }
                if (!attrFound) {
                    itemOk = false; break;
                }
            }
            if (itemOk) {
                return (priority > 0 && result->displayPriority != priority) ? 0 : new LookupTableItem (item);
            }
        }
        return (priority > 0 && result->displayPriority != priority) ? 0 : new LookupTableItem (result);
    }
};

inline uint64_t constructForeignKey (uint8_t recName, uint32_t rcid) {
    return (((uint64_t) recName) << 32) + rcid;
}

inline uint64_t constructForeignKey (uint8_t *byteArray) {
    uint64_t key = ((uint64_t) byteArray [0]) << 32;

    key += byteArray [1];
    key += byteArray [2] << 8;
    key += byteArray [3] << 16;
    key += byteArray [4] << 24;

    return key;
}

struct TopologyCollection {
    std::map<uint64_t, size_t> index;
};

template<typename ObjType>
struct GeoCollection: TopologyCollection {
    std::vector<ObjType> container;

    void buildIndex () {
        for (size_t i = 0; i < container.size (); ++ i) {
            auto& node = container [i];
            uint64_t key = constructForeignKey (node.recordName, node.id);
            index.emplace (std::pair<uint64_t, size_t> (key, i));
        }
    }

    size_t size () { return container.size (); }
    void clear () {
        container.clear ();
        index.clear ();
    }
    ObjType& emplace_back () { return container.emplace_back (); }
    ObjType& back () { return container.back (); }
    ObjType& front () { return container.front (); }
    auto begin () { return container.begin (); }
    auto end () { return container.end (); }
    ObjType& operator[] (const size_t index) { return container [index]; }

    ObjType *getByForgeignKey (uint8_t recName, uint32_t rcid) {
        return getByForgeignKey (constructForeignKey (recName, rcid));
    }
    ObjType *getByForgeignKey (uint64_t key) {
        auto pos = index.find (key);

        return (pos == index.end ()) ? 0 : & container [pos->second];
    }
    size_t getIndexByForgeignKey (uint64_t key) {
        auto pos = index.find (key);

        return (pos == index.end ()) ? -1 : pos->second;
    }
};

#if 1
struct Nodes: GeoCollection<GeoNode> {};
struct Edges: GeoCollection<GeoEdge> {};
struct Features: GeoCollection<FeatureObject> {};
struct Chart {
    Nodes nodes;
    Edges edges;
    Features features;
    UnderlyingObjectsList objectsUnderPoints, objectsUnderSpatials;
    AreaTopologyMap areaTopologyMap;

    SpatialsUnderObject *getListOfSpatialsUnderPoint (FeatureObject& point) {
        auto& pos = objectsUnderPoints.find (point.fidn);

        return pos == objectsUnderPoints.end () ? 0 : & pos->second;
    }
    SpatialsUnderObject *getListOfSpatialsUnderSpatial (FeatureObject& spatialObj) {
        auto& pos = objectsUnderSpatials.find (spatialObj.fidn);

        return pos == objectsUnderSpatials.end () ? 0 : & pos->second;
    }
};
struct Environment {
    ObjectDictionary objectDictionary;
    AttrDictionary attrDictionary;
    Dai dai;
    Settings settings;

    Environment () {
        initCSPs (*this);
    }

    void runCSP (LookupTableItem *item, struct FeatureObject *object, Chart& chart, View& view, struct DrawQueue& drawQueue) {
        if (item->procIndex >= 0 && item->procIndex < dai.procedures.size ()) {
            dai.procedures [item->procIndex] (item, object, *this, chart, view, drawQueue);
        }
    }
};

#else
struct Nodes: TopologyCollection {
    std::vector<GeoNode> container;

    void buildIndex () {
        for (size_t i = 0; i < container.size (); ++ i) {
            auto& node = container [i];
            uint64_t key = constructForeignKey (node.recordName, node.id);
            index.emplace (std::pair<uint64_t, size_t> (key, i));
        }
    }

    size_t size () { return container.size (); }
    void clear () {
        container.clear ();
        index.clear ();
    }
    GeoNode& emplace_back () { return container.emplace_back (); }
    GeoNode& back () { return container.back (); }
    GeoNode& front () { return container.front (); }
    auto begin () { return container.begin (); }
    auto end () { return container.end (); }
    GeoNode& operator[] (const size_t index) { return container [index]; }

    GeoNode *getByForgeignKey (uint8_t recName, uint32_t rcid) {
        return getByForgeignKey (constructForeignKey (recName, rcid));
    }
    GeoNode *getByForgeignKey (uint64_t key) {
        auto pos = index.find (key);

        return (pos == index.end ()) ? 0 : & container [pos->second];
    }
};

struct Edges: TopologyCollection {
    std::vector<GeoEdge> container;

    void buildIndex () {
        for (size_t i = 0; i < container.size (); ++ i) {
            auto& edge = container [i];
            uint64_t key = constructForeignKey (edge.recordName, edge.id);
            index.emplace (std::pair<uint64_t, size_t> (key, i));
        }
    }

    size_t size () { return container.size (); }
    void clear () {
        container.clear ();
        index.clear ();
    }
    GeoEdge& emplace_back () { return container.emplace_back (); }
    GeoEdge& back () { return container.back (); }
    GeoEdge& front () { return container.front (); }
    auto begin () { return container.begin (); }
    auto end () { return container.end (); }
    GeoEdge& operator[] (const size_t index) { return container [index]; }

    GeoEdge *getByForgeignKey (uint8_t recName, uint32_t rcid) {
        return getByForgeignKey (constructForeignKey (recName, rcid));
    }
    GeoEdge *getByForgeignKey (uint64_t key) {
        auto pos = index.find (key);

        return (pos == index.end ()) ? 0 : & container [pos->second];
    }
};

struct Features: TopologyCollection {
    std::vector<FeatureObject> container;

    void buildIndex () {
        for (size_t i = 0; i < container.size (); ++ i) {
            auto& feature = container [i];
            uint64_t key = constructForeignKey (feature.recordName, edge.id);
            index.emplace (std::pair<uint64_t, size_t> (key, i));
        }
    }

    size_t size () { return container.size (); }
    void clear () {
        container.clear ();
        index.clear ();
    }
    FeatureObject& emplace_back () { return container.emplace_back (); }
    FeatureObject& back () { return container.back (); }
    FeatureObject& front () { return container.front (); }
    auto begin () { return container.begin (); }
    auto end () { return container.end (); }
    FeatureObject& operator[] (const size_t index) { return container [index]; }

    FeatureObject *getByForgeignKey (uint8_t recName, uint32_t rcid) {
        return getByForgeignKey (constructForeignKey (recName, rcid));
    }
    FeatureObject *getByForgeignKey (uint64_t key) {
        auto pos = index.find (key);

        return (pos == index.end ()) ? 0 : & container [pos->second];
    }
};
#endif