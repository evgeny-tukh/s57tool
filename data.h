#pragma once

#include <stdlib.h>
#include <cstdint>
#include <vector>
#include <map>
#include <string>
#include <tuple>
#include "s57defs.h"

enum NodeFlags {
    CONNECTED = 1,
    SOUNDING_ARRAY = 2,
};

struct TopologyObject {
    uint32_t id;
    uint32_t recordName;
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

struct GeoEdge: TopologyObject {
    Orient orientation;
    EdgeNode begin;
    EdgeNode end;
    std::vector<Position> internalNodes;
    bool hidden;

    GeoEdge (): TopologyObject (), orientation (Orient::UNKNOWN), begin (), end () {}
};

inline uint64_t constructForeignKey (uint8_t recName, uint32_t rcid) {
    return ((uint64_t) recName << 32) + (uint64_t) rcid;
}

struct TopologyCollection {
    std::map<uint64_t, size_t> index;
};

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
