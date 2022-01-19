#include <string>
#include <vector>
#include <map>
#include <tuple>
#include "s57defs.h"

Palette::~Palette () {
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
    for (auto& brushItem: patternBrushes) {
        DeleteObject (brushItem.day);
        DeleteObject (brushItem.dusk);
        DeleteObject (brushItem.night);
    }
}

size_t Palette::checkPen (char *instr, ColorTable& colorTable) {
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

size_t Palette::checkSolidBrush (const char *colorName, ColorTable& colorTable) {
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

size_t Palette::getPatternBrushIndex (const char *patternName) {
    auto pos = patternBrushIndex.find (patternName);

    return pos == patternBrushIndex.end () ? LookupTableItem::NOT_EXIST : pos->second;
}

size_t Palette::getSolidBrushIndex (const char *colorName) {
    auto pos = brushIndex.find (colorName);

    return pos == brushIndex.end () ? LookupTableItem::NOT_EXIST : pos->second;
}

size_t Palette::checkPatternBrush (PatternDesc& pattern, struct Dai& dai) {
    auto brushPos = patternBrushIndex.find (pattern.name);
    size_t index;

    if (brushPos == patternBrushIndex.end ()) {
        index = patternBrushes.size ();

        patternBrushIndex.emplace (pattern.name, index);

        patternBrushes.emplace_back (
            createPatternBrush (pattern, PaletteIndex::Day, dai),
            createPatternBrush (pattern, PaletteIndex::Dusk, dai),
            createPatternBrush (pattern, PaletteIndex::Night, dai)
        );

        return index;
    } else {
        return brushPos->second;
    }
}

void DrawProcedure::defineOperation (const char *operDesc, Dai& dai) {
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
        size_t colorIndex = getPenColorIndex (operDesc [2]);

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

void ObjectDictionary::checkAddObject (uint16_t code, const char *acronym, const char *name) {
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

void AttrDictionary::checkAddAttr (uint16_t code, char domain, const char *acronym, const char *name) {
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

void Dai::composePatternBrushes () {
    for (auto& pattern: patterns) {
        palette.checkPatternBrush (pattern, *this);
    }
}
