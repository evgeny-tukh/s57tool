#include <string>
#include <vector>
#include <string.h>
#include "drawers.h"
#include "painter.h"
#include "abstract_tools.h"

PenTool Drawer::penTool;

void LineDrawer::run (RECT& client, HDC paintDC, PaletteIndex paletteIndex, Palette& palette) {
    paintLine (client, paintDC, penStyle, penWidth, penIndex, lat, lon, brg, rangeMm, north, west, zoom, paletteIndex, palette);
}

void ArcDrawer::run (RECT& client, HDC paintDC, PaletteIndex paletteIndex, Palette& palette) {
    paintArc (client, paintDC, penStyle, penWidth, penIndex, lat, lon, start, end, rangeMm, north, west, zoom, paletteIndex, palette);
}

void PolyPolylineDrawer::run (RECT& client, HDC paintDC, PaletteIndex paletteIndex, Palette& palette) {
    paintPolyPolyline (client, paintDC, penStyle, penWidth, penIndex, polyPolyline, north, west, zoom, paletteIndex, palette);
}

void PolyPolylineDrawer::addNode (size_t nodeIndex) {
    auto& pos = nodes [nodeIndex].points [0];
    addVertex (pos.lat, pos.lon);
}

void PolyPolylineDrawer::addEdge (EdgeRef& edgeRef) {
    if (edgeRef.hidden) return;

    auto& edge = edges.container [edgeRef.index];

    addContour ();
    if (edgeRef.unclockwise) {
        addNode (edge.endIndex);
        for (auto pos = edge.internalNodes.rbegin (); pos != edge.internalNodes.rend (); ++ pos) {
            addVertex (pos->lat, pos->lon);
        }
        addNode (edge.beginIndex);
    } else {
        addNode (edge.beginIndex);
        for (auto pos: edge.internalNodes) {
            addVertex (pos.lat, pos.lon);
        }
        addNode (edge.endIndex);
    }
}

void PolyPolygonDrawer::run (RECT& client, HDC paintDC, PaletteIndex paletteIndex, Palette& palette) {
    paintPolyPolygon (client, paintDC, fillBrushIndex, patternBrushIndex, polyPolyline, north, west, zoom, paletteIndex, palette);
}

void PolyPolygonDrawer::addEdge (EdgeRef& edgeRef) {
    if (edgeRef.hidden) return;

    auto& edge = edges.container [edgeRef.index];

    if (polyPolyline.empty ()) polyPolyline.emplace_back ();

    if (edgeRef.hole != hole) {
        hole = edgeRef.hole;
        polyPolyline.emplace_back ();
    } else if (hole && isLastContourClosed ()) {
        polyPolyline.emplace_back ();
    }
    if (edgeRef.unclockwise) {
        addNode (edge.endIndex);
        for (auto pos = edge.internalNodes.rbegin (); pos != edge.internalNodes.rend (); ++ pos) {
            addVertex (pos->lat, pos->lon);
        }
        addNode (edge.beginIndex);
    } else {
        addNode (edge.beginIndex);
        for (auto pos: edge.internalNodes) {
            addVertex (pos.lat, pos.lon);
        }
        addNode (edge.endIndex);
    }
}

TextDrawer::TextDrawer (
    double _north,
    double _west,
    double _lat,
    double _lon,
    int _zoom,
    const char *instr,
    FeatureObject *object,
    Dai& _dai,
    AttrDictionary& attrDic
) : Drawer (LookupTableItem::NOT_EXIST, PS_SOLID, 1, _north, _west, _lat, _lon, 0.0, _zoom), dai (_dai) {
    if (instr [0] == 'T') {
        if (instr [1] == 'E') {
            parseExtendedInstr (instr, object, dai, attrDic);
        } else {
            parseRegularInstr (instr, object, dai, attrDic);
        }
    }
}

bool TextDrawer::parseInstr (const char *instr, std::vector<std::string>& parts) {
    char *leftBracket = strchr ((char *) instr, '(');
    char *rightBracket = leftBracket ? strchr (leftBracket + 1, ')') : 0;
    bool result = false;

    if (rightBracket) {
        splitString (std::string (leftBracket + 1, rightBracket - leftBracket - 1), parts, ',');
        result = !parts.empty ();
    }

    return result;
}

std::string s57format (const char *format, std::vector<std::string>& args, FeatureObject *object, Dai& dai, AttrDictionary& attrDic) {
    std::string result;
    std::string argFmt;
    size_t argNum = 0;
    enum ArgType {
        Integral = 'd',
        Float = 'f',
        String = 's',
        Unknown = '\0',
    } argType;

    for (const char *chr = format; *chr; ++ chr) {
        if (*chr == '%') {
            argFmt.clear ();
            for (const char *fmtChr = chr; *fmtChr; ++ fmtChr) {
                argFmt += *fmtChr;
                switch (*fmtChr) {
                    case 'd': argType = ArgType::Integral; break;
                    case 'f': argType = ArgType::Float; break;
                    case 's': argType = ArgType::String; break;
                    default: argType = ArgType::Unknown;
                }
                if (argType != ArgType::Unknown) {
                    char value [500];
                    auto attr = object->getAttr (args [argNum++].c_str (), attrDic);
                    if (attr && !attr->noValue) {
                        switch (argType) {
                            case ArgType::Integral: sprintf (value, argFmt.c_str (), attr->intValue); break;
                            case ArgType::Float: sprintf (value, argFmt.c_str (), attr->floatValue); break;
                            case ArgType::String: sprintf (value, argFmt.c_str (), attr->strValue); break;
                            default: value [0] = '\0';
                        }
                    } else {
                        *value = '\0';
                    }
                    result += value;
                    chr = fmtChr;
                    break;
                }
            }
        } else {
            result += *chr;
        }
    }
    return result;
}

inline std::string unquote (std::string& source) {
    if (source.empty ()) return std::string ();
    std::string result { source.c_str () };
    if (result.front () == '\'' && result.back () == '\'') {
        result = result.substr (1, result.length () - 2);
    }
    return result;
}

void TextDrawer::parseInstrCommon (std::vector<std::string>& parts, FeatureObject *object, Dai& dai, AttrDictionary& attrDic, int start) {
    int fld = start;
    horJust = (HorJust) (parts.size () > fld ? parts [fld].front () - '0' : 3); ++ fld;
    verJust = (VerJust) (parts.size () > fld ? parts [fld].front () - '0' : 1); ++ fld;
    spacing = (Spacing) (parts.size () > fld ? parts [fld].front () - '0' : 2); ++ fld;
    auto chars = parts.size () > fld ? unquote (parts [fld]) : "3121510"; ++ fld;
    fontType = (FontType) (chars [0] - '0');
    fontWeight = (FontWeight) (chars [1] - '0');
    fontStyle = (FontStyle) (chars [2] - '0');
    fontSize = (double) ((chars [3] - '0') * 10 + chars [4] - '0') / PIXEL_SIZE_IN_MM;
    horOffset = parts.size () > fld ? std::atoi (parts [fld].c_str ()) : 0; ++ fld;
    verOffset = parts.size () > fld ? std::atoi (parts [fld].c_str ()) : 0; ++ fld;
    penIndex = dai.getBasePenIndex (parts.size () > fld ? parts [fld].c_str () : "CHBLK"); ++ fld;
    if (penIndex == LookupTableItem::NOT_EXIST) penIndex = dai.getBasePenIndex ("CHBLK"); ++ fld;
}

bool TextDrawer::parseExtendedInstr (const char *instr, FeatureObject *object, Dai& dai, AttrDictionary& attrDic) {
    std::vector<std::string> parts;
    bool result = false;

    auto getArgValue = [object, &attrDic] (const char *arg) {
        auto attr = object->getAttr (arg, attrDic);
        return (attr && !attr->noValue) ? attr->strValue : "";
    };

    if (parseInstr (instr, parts)) {
        auto format = unquote (parts [0]);
        auto argsStr = unquote (parts [1]);
        std::vector<std::string> args;
        splitString (argsStr, args, ',');
        text = s57format (format.c_str (), args, object, dai, attrDic);
        parseInstrCommon (parts, object, dai, attrDic, 2);
        result = true;
    }
    return result;
}

bool TextDrawer::parseRegularInstr (const char *instr, FeatureObject *object, Dai& dai, AttrDictionary& attrDic) {
    std::vector<std::string> parts;
    bool result = false;

    if (parseInstr (instr, parts)) {
        auto attrName = unquote (parts [0]);
        auto attr = object->getAttr (attrName.c_str (), attrDic);
        text = (attr && !attr->noValue) ? attr->strValue.c_str () : "";
        parseInstrCommon (parts, object, dai, attrDic, 1);
        result = true;
    }
    return result;
}

void TextDrawer::run (RECT& client, HDC paintDC, PaletteIndex paletteIndex, Palette& palette) {
    if (text.empty ()) return;

    unsigned int format = 0;

    switch (horJust) {
        case HorJust::CENTER: format |= DT_CENTER; break;
        case HorJust::LEFT: format |= DT_LEFT; break;
        case HorJust::RIGHT: format |= DT_RIGHT; break;
    }
    switch (verJust) {
        case VerJust::CENT: format |= DT_VCENTER; break;
        case VerJust::BOTTOM: format |= DT_BOTTOM; break;
        case VerJust::TOP: format |= DT_TOP; break;
    }
    paintText (client, paintDC, text.data (), format, lat, lon, horOffset, verOffset, penIndex, north, west, zoom, paletteIndex, dai);
};

void DrawQueue::addCompoundLightArc (
    int penIndex,
    int penStyle,
    int penWidth,
    double centerLat,
    double centerLon,
    double radiusMm,
    double start,
    double end) {
    addArc (dai.getBasePenIndex ("OUTLW"), penStyle, 2, centerLat, centerLon, radiusMm + 2.0 * PIXEL_SIZE_IN_MM, start, end);
    addArc (dai.getBasePenIndex ("OUTLW"), penStyle, 2, centerLat, centerLon, radiusMm - 2.0 * PIXEL_SIZE_IN_MM, start, end);
    addArc (penIndex, PS_SOLID, 4, centerLat, centerLon, radiusMm, start, end);
}

void DrawQueue::addEdgeChain (int penIndex, int penStyle, int penWidth, Nodes& nodes, Edges& edges) {
    container.push_back (new PolyPolylineDrawer (penIndex, penStyle, penWidth, north, west, zoom, nodes, edges));
}

void DrawQueue::addArea (size_t fillBrushIndex, size_t patternBrushIndex, Nodes& nodes, Edges& edges) {
    container.push_back (new PolyPolygonDrawer (fillBrushIndex, patternBrushIndex, north, west, zoom, nodes, edges));
}

void DrawQueue::addEdge (EdgeRef& edgeRef) {
    if (container.size () > 0) {
        PolyPolylineDrawer *drawer = (PolyPolylineDrawer *) container.back ();

        drawer->addEdge (edgeRef);
    }
}

