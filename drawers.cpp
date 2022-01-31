#include <string>
#include <vector>
#include <string.h>
#include "drawers.h"
#include "painter.h"
#include "abstract_tools.h"

PenTool Drawer::penTool;

void LineDrawer::run (RECT& client, HDC paintDC, PaletteIndex paletteIndex, Palette& palette) {
    if (penIndex != LookupTableItem::NOT_EXIST) {
        paintLine (client, paintDC, penStyle, penWidth, penIndex, lat, lon, brg, rangeMm, view, paletteIndex, palette);
    }
}

void ArcDrawer::run (RECT& client, HDC paintDC, PaletteIndex paletteIndex, Palette& palette) {
    if (penIndex != LookupTableItem::NOT_EXIST) {
        paintArc (client, paintDC, penStyle, penWidth, penIndex, lat, lon, start, end, rangeMm, view, paletteIndex, palette);
    }
}

void PolyPolylineDrawer::run (RECT& client, HDC paintDC, PaletteIndex paletteIndex, Palette& palette) {
    if (penIndex != LookupTableItem::NOT_EXIST) {
        paintPolyPolyline (client, paintDC, penStyle, penWidth, penIndex, polyPolyline, view, paletteIndex, palette);
    }
}

void PolyPolylineDrawer::addNode (size_t nodeIndex) {
    auto& pos = chart.nodes [nodeIndex].points [0];
    addVertex (pos.lat, pos.lon);
}

void PolyPolylineDrawer::addEdge (EdgeRef& edgeRef) {
    if (edgeRef.hidden) return;

    auto& edge = chart.edges.container [edgeRef.index];

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
    paintPolyPolygon (client, paintDC, fillBrushIndex, patternBrushIndex, polyPolyline, view, paletteIndex, palette);
}

void PolyPolygonDrawer::addEdge (EdgeRef& edgeRef) {
    if (edgeRef.hidden) return;

    auto& edge = chart.edges.container [edgeRef.index];

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
    View& _view,
    double _lat,
    double _lon,
    TextDesc& _desc,
    FeatureObject *_object,
    Dai& _dai
) : Drawer (LookupTableItem::NOT_EXIST, PS_SOLID, 1, _view, _lat, _lon, 0.0), desc (_desc), dai (_dai) {
    if (_desc.paramDescs.size () == 1) {
        if (!_desc.paramDescs.front ().plainText.empty ()) {
            text = _desc.paramDescs.front ().plainText;
        } else {
            auto attr = _object->getAttr (_desc.paramDescs.front ().classCode);

            if (attr && !attr->noValue) text = attr->strValue;
        }
    } else {
        text = _desc.plainTextParts.front ();
        for (size_t i = 0; i < _desc.paramDescs.size (); ++ i) {
            auto& paramDesc = _desc.paramDescs [i];
            auto attr = _object->getAttr (paramDesc.classCode);
            if (attr && !attr->noValue) {
                char value [500];
                switch (paramDesc.type) {
                    case TextDesc::ParamType::INT_VAL: sprintf (value, paramDesc.format.c_str (), attr->intValue); break;
                    case TextDesc::ParamType::FLOAT_VAL: sprintf (value, paramDesc.format.c_str (), attr->floatValue); break;
                    case TextDesc::ParamType::STRING_VAL: strcpy (value, attr->strValue.c_str ()); break;
                    default: *value = '\0';
                }
                text += value;
            }
            text += _desc.plainTextParts [i+1];
        }
    }
}

bool parseInstr (const char *instr, std::vector<std::string>& parts) {
    char *leftBracket = strchr ((char *) instr, '(');
    char *rightBracket = 0;
    
    if (leftBracket) {
        bool inQuote = false;
        for (char *chr = (char *) instr; *chr && !rightBracket; ++ chr) {
            switch (*chr) {
                case '\'':
                    inQuote = !inQuote; break;
                case ')':
                    if (!inQuote) rightBracket = chr;
                    break;
            }
        }
    }

    bool result = false;

    if (rightBracket) {
        splitString (std::string (leftBracket + 1, rightBracket - leftBracket - 1), parts, ',');
        result = !parts.empty ();
    }

    return result;
}

void parseFormatAndArgs (const char *format, std::vector<std::string>& args, AttrDictionary& attrDic, TextDesc& desc) {
    desc.paramDescs.clear ();
    desc.plainTextParts.clear ();

    for (auto& arg: args) {
        auto attrDesc = attrDic.findByAcronym (arg.c_str ());

        auto& paramDesc = desc.paramDescs.emplace_back ();
        paramDesc.classCode = attrDesc ? attrDesc->code : 0;
    }

    desc.plainTextParts.emplace_back ();
    size_t paramNo = 0;

    for (const char *chr = format; *chr; ++ chr) {
        if (*chr == '%') {
            for (const char *fmtChr = chr; *fmtChr; ++ fmtChr) {
                auto& paramDesc = desc.paramDescs [paramNo];
                paramDesc.format += *fmtChr;
                switch (*fmtChr) {
                    case 'd': paramDesc.type = TextDesc::ParamType::INT_VAL; break;
                    case 'f': paramDesc.type = TextDesc::ParamType::FLOAT_VAL; break;
                    case 's': paramDesc.type = TextDesc::ParamType::STRING_VAL; break;
                    default: paramDesc.type = TextDesc::ParamType::UNKNOWN_TYPE;
                }
                if (paramDesc.type != TextDesc::ParamType::UNKNOWN_TYPE) {
                    chr = fmtChr - 1;
                    desc.plainTextParts.emplace_back ();
                    break;
                }
            }
        } else {
            desc.plainTextParts.back () += *chr;
        }
    }
}

void prepareSingleArg (const char *acronym, AttrDictionary& attrDic, TextDesc& desc) {
    desc.paramDescs.clear ();
    desc.plainTextParts.clear ();

    auto& paramDesc = desc.paramDescs.emplace_back ();
    paramDesc.type = TextDesc::ParamType::STRING_VAL;

    if (*acronym == '*') {
        paramDesc.plainText = acronym + 1;
    } else {
        auto attrDesc = attrDic.findByAcronym (acronym);
        paramDesc.classCode = attrDesc ? attrDesc->code : 0;
    }
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

void parseInstrCommon (std::vector<std::string>& parts, Dai& dai, int start, TextDesc& desc) {
    int fld = start;
    desc.horJust = (TextDesc::HorJust) (parts.size () > fld ? parts [fld].front () - '0' : 3); ++ fld;
    desc.verJust = (TextDesc::VerJust) (parts.size () > fld ? parts [fld].front () - '0' : 1); ++ fld;
    desc.spacing = (TextDesc::Spacing) (parts.size () > fld ? parts [fld].front () - '0' : 2); ++ fld;
    auto chars = parts.size () > fld ? unquote (parts [fld]) : "3121510"; ++ fld;
    desc.fontType = (TextDesc::FontType) (chars [0] - '0');
    desc.fontWeight = (TextDesc::FontWeight) (chars [1] - '0');
    desc.fontStyle = (TextDesc::FontStyle) (chars [2] - '0');
    desc.fontSize = (double) ((chars [3] - '0') * 10 + chars [4] - '0') / PIXEL_SIZE_IN_MM;
    desc.horOffset = parts.size () > fld ? std::atoi (parts [fld].c_str ()) : 0; ++ fld;
    desc.verOffset = parts.size () > fld ? std::atoi (parts [fld].c_str ()) : 0; ++ fld;
    desc.colorIndex = dai.getBasePenIndex (parts.size () > fld ? parts [fld].c_str () : "CHBLK");
    if (desc.colorIndex == LookupTableItem::NOT_EXIST) desc.colorIndex = dai.getBasePenIndex ("CHBLK");
}

bool parseExtendedInstr (const char *instr, Dai& dai, AttrDictionary& attrDic, TextDesc& desc) {
    std::vector<std::string> parts;
    bool result = false;

    if (parseInstr (instr, parts)) {
        auto format = unquote (parts [0]);
        auto argsStr = unquote (parts [1]);
        std::vector<std::string> args;
        splitString (argsStr, args, ',');
        parseFormatAndArgs (format.c_str (), args, attrDic, desc);
        parseInstrCommon (parts, dai, 2, desc);
        result = true;
    }
    return result;
}

bool parseRegularInstr (const char *instr, Dai& dai, AttrDictionary& attrDic, TextDesc& desc) {
    std::vector<std::string> parts;
    bool result = false;

    if (parseInstr (instr, parts)) {
        auto attrName = unquote (parts [0]);
        prepareSingleArg (attrName.c_str (), attrDic, desc);
        parseInstrCommon (parts, dai, 1, desc);
        result = true;
    }
    return result;
}

void parseTextInstruction (const char *instr, Dai& dai, AttrDictionary& attrDic, TextDesc& desc) {
    if (instr [0] == 'T') {
        if (instr [1] == 'E') {
            parseExtendedInstr (instr, dai, attrDic, desc);
        } else {
            parseRegularInstr (instr, dai, attrDic, desc);
        }
    }
}

void TextDrawer::run (RECT& client, HDC paintDC, PaletteIndex paletteIndex, Palette& palette) {
    if (text.empty ()) return;

    unsigned int format = 0;

    switch (desc.horJust) {
        case TextDesc::HorJust::CENTER: format |= DT_CENTER; break;
        case TextDesc::HorJust::LEFT: format |= DT_LEFT; break;
        case TextDesc::HorJust::RIGHT: format |= DT_RIGHT; break;
    }
    switch (desc.verJust) {
        case TextDesc::VerJust::CENT: format |= DT_VCENTER; break;
        case TextDesc::VerJust::BOTTOM: format |= DT_BOTTOM; break;
        case TextDesc::VerJust::TOP: format |= DT_TOP; break;
    }
    paintText (client, paintDC, text.data (), format, lat, lon, desc.horOffset, desc.verOffset, desc.colorIndex, view, paletteIndex, dai);
};

void SymbolDrawer::run (RECT& client, HDC paintDC, PaletteIndex paletteIndex, Palette& palette) {
    if (symbolIndex != LookupTableItem::NOT_EXIST) {
        paintSymbol (client, paintDC, lat, lon, symbolIndex, rotAngle, dai, view, paletteIndex);
    }
}

void CentralEdgeSymbolDrawer::run (RECT& client, HDC paintDC, PaletteIndex paletteIndex, Palette& palette) {
    if (symbolIndex != LookupTableItem::NOT_EXIST) {
        auto [exists, x, y] = getCenterPos (edgeIndex, client, chart, view);
        if (exists) {
            paintSymbol (client, paintDC, x, y, symbolIndex, 0.0, dai, paletteIndex);
        }
    }
}

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

void DrawQueue::addEdgeChain (int penIndex, int penStyle, int penWidth, Chart& chart) {
    container.push_back (new PolyPolylineDrawer (penIndex, penStyle, penWidth, view, chart));
}

void DrawQueue::addArea (size_t fillBrushIndex, size_t patternBrushIndex, Chart& chart) {
    container.push_back (new PolyPolygonDrawer (fillBrushIndex, patternBrushIndex, view, chart));
}

void DrawQueue::addEdge (EdgeRef& edgeRef) {
    if (container.size () > 0) {
        PolyPolylineDrawer *drawer = (PolyPolylineDrawer *) container.back ();

        drawer->addEdge (edgeRef);
    }
}

