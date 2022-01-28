#include <string>
#include <vector>
#include <map>
#include <tuple>
#include <optional>
#include "s57defs.h"
#include "classes.h"
#include "data.h"
#include "settings.h"
#include "drawers.h"

uint8_t _7_8_14 [] { 7, 8, 14, 0 };
uint8_t _1_2 [] { 1, 2, 0 };
uint8_t _3_4_5_6_24 [] { 3, 4, 5, 6, 24, 0 };
uint8_t _13_16_17_23_25_26_27 [] { 13, 16, 17, 23, 25, 26, 27, 0 };
uint8_t _9_10_11_12_15_18_19_20_21_22 [] { 9, 10, 11, 12, 15, 18, 19, 20, 21, 22, 0 };
uint8_t _1_2_3_4_5_6_13_16_17_23_24_25_26_27 [] { 1, 2, 3, 4, 5, 6, 13, 16, 17, 23, 24, 25, 26, 27, 0 };
uint8_t _3_4_5_6_13_16_17_23_24_25_26_27 [] { 3, 4, 5, 6, 13, 16, 17, 23, 24, 25, 26, 27, 0 };
uint8_t _1_8_9_12_14_18_19_21_24_25_26 [] { 1, 8, 9, 12, 14, 18, 19, 21, 24, 25, 26, 0 };
uint8_t _13_16_17_23_24_25_26_27 [] { 13, 16, 17, 23, 24, 25, 26, 27, 0 };
uint8_t _4_5_6_7_10_20_22_23 [] { 4, 5, 6, 7, 10, 20, 22, 23, 0 };

bool isEdgeSharedWithTg1Object (FeatureObject *thisObject, size_t edgeIndex, Features& features) {
    for (size_t i = 0; i < features.size (); ++ i) {
        auto& object = features [i];
        if (object.fidn != thisObject->fidn && object.group == 1) return true;
    }
    return false;
}

std::tuple<bool, size_t> getEdgeSharedWith (
    FeatureObject *thisObject,
    size_t edgeIndex,
    Features& features,
    uint16_t classCode,
    uint16_t classCode2 = 0,
    uint16_t classCode3 = 0,
    uint16_t classCode4 = 0,
    uint16_t classCode5 = 0,
    uint16_t classCode6 = 0
) {
    for (size_t i = 0; i < features.size (); ++ i) {
        auto& object = features [i];
        if (object.fidn == thisObject->fidn) continue;
        if (
            object.classCode == classCode ||
            object.classCode == classCode2 ||
            object.classCode == classCode3 ||
            object.classCode == classCode4 ||
            object.classCode == classCode5 ||
            object.classCode == classCode6
        ) {
            for (auto& edgeRef: object.edgeRefs) {
                if (edgeRef.index == edgeIndex) return std::tuple<bool, size_t> (true, i);
            }
        }
    }
    return std::tuple<bool, size_t> (false, -1);
}

bool isEdgeSharedWith (
    FeatureObject *thisObject,
    size_t edgeIndex,
    Features& features,
    uint16_t classCode,
    uint16_t classCode2 = 0,
    uint16_t classCode3 = 0,
    uint16_t classCode4 = 0,
    uint16_t classCode5 = 0,
    uint16_t classCode6 = 0
) {
    auto [shared, index] = getEdgeSharedWith (thisObject, edgeIndex, features, classCode, classCode2, classCode3, classCode4, classCode5, classCode6);
    return shared;
}

bool isEdgeSharedWithLinerStructure (FeatureObject *thisObject, size_t edgeIndex, Features& features) {
    if (isEdgeSharedWith (thisObject, edgeIndex, features, OBJ_CLASSES::LNDARE, OBJ_CLASSES::GATCON, OBJ_CLASSES::DAMCON)) return true;

    auto [shared, index] = getEdgeSharedWith (thisObject, edgeIndex, features, OBJ_CLASSES::SLCONS, OBJ_CLASSES::CAUSWY);

    if (!shared) return false;

    auto watlev = features [index].getAttr (ATTRS::WATLEV);

    return watlev && (watlev->noValue || watlev->intValue == 1 || watlev->intValue == 2 || watlev->intValue == 6);
}

void seabed01 (double depthRangeVal1, double depthRangeVal2, LookupTableItem *item, Environment& environment) {
    Settings& settings = environment.settings;
    std::string colorName { "DEPIT" };
    bool shallow = true;

    if (settings.twoShades) {
        if (depthRangeVal1 >= 0 && depthRangeVal2 > 0) {
            colorName = "DEPVS";
        }
        if (depthRangeVal1 >= settings.safetyContour && depthRangeVal2 > settings.safetyContour) {
            colorName = "DEPDW";
            shallow = false;
        }
    } else {
        if (depthRangeVal1 >= 0 && depthRangeVal2 > 0) {
            colorName = "DEPVS";
        }
        if (depthRangeVal1 >= settings.shallowContour && depthRangeVal2 > settings.shallowContour) {
            colorName = "DEPMS";
        }
        if (depthRangeVal1 >= settings.safetyContour && depthRangeVal2 > settings.safetyContour) {
            colorName = "DEPMD";
            shallow = false;
        }
        if (depthRangeVal1 >= settings.deepContour && depthRangeVal2 > settings.deepContour) {
            colorName = "DEPDW";
            shallow = false;
        }
    }
    
    item->brushIndex = environment.dai.getBasePenIndex (colorName.c_str ());

    if (settings.shallowPattern && shallow) {
        item->patternBrushIndex = environment.dai.getPatternIndex ("DIAMOND1");
    }
}

void safcon01 (FeatureObject *object, double depth, std::vector<std::string>& symbols) {
    if (depth < 0.0 || depth > 99999.0) return;

    auto addSymbol = [&symbols] (const char *prefix, int lastDigit) {
        symbols.push_back (std::string (prefix) + (char) (lastDigit + '0'));
    };

    int integralPart = (int) depth;
    int fractionPart = (int) ((depth - (double) integralPart) * 10.0);

    if (depth < 10.0) {
        addSymbol ("SAFCON0", integralPart);

        if (fractionPart) addSymbol ("SAFCON6", fractionPart);
    } else if (depth < 31.0 && fractionPart) {
        addSymbol ("SAFCON2", integralPart / 10);
        addSymbol ("SAFCON1", integralPart % 10);
        addSymbol ("SAFCON5", fractionPart);
    } else if (depth < 100.0) {
        addSymbol ("SAFCON2", integralPart / 10);
        addSymbol ("SAFCON1", integralPart % 10);
    } else if (depth < 1000.0) {
        addSymbol ("SAFCON8", integralPart / 100);
        addSymbol ("SAFCON0", (integralPart % 10) / 10);
        addSymbol ("SAFCON9", integralPart % 10);
    } else if (depth < 10000.0) {
        addSymbol ("SAFCON3", integralPart / 1000);
        addSymbol ("SAFCON2", (integralPart % 1000) / 100);
        addSymbol ("SAFCON1", (integralPart % 100) / 10);
        addSymbol ("SAFCON7", integralPart % 10);
    } else if (depth < 100000.0) {
        addSymbol ("SAFCON4", integralPart / 10000);
        addSymbol ("SAFCON3", (integralPart % 10000) / 1000);
        addSymbol ("SAFCON2", (integralPart % 1000) / 100);
        addSymbol ("SAFCON1", (integralPart % 100) / 10);
        addSymbol ("SAFCON7", integralPart % 10);
    }
}

void rescsp03 (Attr *restrn, FeatureObject *object, Environment& environment, Chart& chart, View& view, DrawQueue& drawQueue) {
    double lat, lon;
    getCenterPos (*object, chart, lat, lon);

    if (restrn->listIncludes (_7_8_14)) {
        if (restrn->listIncludes (_1_2_3_4_5_6_13_16_17_23_24_25_26_27)) {
            drawQueue.addSymbol (lat, lon, environment.dai.getSymbolIndex ("ENTRES61"), 0.0, environment.dai);
        } else if (restrn->listIncludes (_9_10_11_12_15_18_19_20_21_22)) {
            drawQueue.addSymbol (lat, lon, environment.dai.getSymbolIndex ("ENTRES71"), 0.0, environment.dai);
        } else {
            drawQueue.addSymbol (lat, lon, environment.dai.getSymbolIndex ("ENTRES51"), 0.0, environment.dai);
        }
    } else if (restrn->listIncludes (_1_2)) {
        if (restrn->listIncludes (_3_4_5_6_13_16_17_23_24_25_26_27)) {
            drawQueue.addSymbol (lat, lon, environment.dai.getSymbolIndex ("ACHRES61"), 0.0, environment.dai);
        } else if (restrn->listIncludes (_9_10_11_12_15_18_19_20_21_22)) {
            drawQueue.addSymbol (lat, lon, environment.dai.getSymbolIndex ("ACHRES71"), 0.0, environment.dai);
        } else {
            drawQueue.addSymbol (lat, lon, environment.dai.getSymbolIndex ("ACHRES51"), 0.0, environment.dai);
        }
    } else if (restrn->listIncludes (_3_4_5_6_24)) {
        if (restrn->listIncludes (_13_16_17_23_25_26_27)) {
            drawQueue.addSymbol (lat, lon, environment.dai.getSymbolIndex ("FSHRES61"), 0.0, environment.dai);
        } else if (restrn->listIncludes (_9_10_11_12_15_18_19_20_21_22)) {
            drawQueue.addSymbol (lat, lon, environment.dai.getSymbolIndex ("FSHRES71"), 0.0, environment.dai);
        } else {
            drawQueue.addSymbol (lat, lon, environment.dai.getSymbolIndex ("FSHRES51"), 0.0, environment.dai);
        }
    } else if (restrn->listIncludes (_13_16_17_23_25_26_27)) {
        if (restrn->listIncludes (_9_10_11_12_15_18_19_20_21_22)) {
            drawQueue.addSymbol (lat, lon, environment.dai.getSymbolIndex ("CTYARE71"), 0.0, environment.dai);
        } else {
            drawQueue.addSymbol (lat, lon, environment.dai.getSymbolIndex ("CTYARE51"), 0.0, environment.dai);
        }
    } else if (restrn->listIncludes (_9_10_11_12_15_18_19_20_21_22)) {
        drawQueue.addSymbol (lat, lon, environment.dai.getSymbolIndex ("INFARE51"), 0.0, environment.dai);
    } else {
        drawQueue.addSymbol (lat, lon, environment.dai.getSymbolIndex ("RSRDEF51"), 0.0, environment.dai);
    }
}

void depare03 (LookupTableItem *item, FeatureObject *object, Environment& environment, Chart& chart, View& view, DrawQueue& drawQueue) {
    Features& features = chart.features;
    Edges& edges = chart.edges;
    Settings& settings = environment.settings;
    Dai& dai = environment.dai;
    auto drval1 = object->getAttr (ATTRS::DRVAL1);
    auto drval2 = object->getAttr (ATTRS::DRVAL2);
    double depthRangeVal1 = (drval1 && !drval1->noValue) ? drval1->floatValue : -1.0;
    double depthRangeVal2 = (drval2 && !drval2->noValue) ? drval2->floatValue : (depthRangeVal1 + 0.01);

    seabed01 (depthRangeVal1, depthRangeVal2, item, environment);

    if (object->classCode == OBJ_CLASSES::DRGARE) {
        item->edgePenIndex = dai.getBasePenIndex ("CHGRF");
        item->edgePenStyle = PS_DASH;
        item->edgePenWidth = 1;
        item->patternBrushIndex = dai.palette.getPatternBrushIndex ("DRGARE01");

        auto restrn = object->getAttr (ATTRS::RESTRN);

        if (restrn && !restrn->noValue) {
            rescsp03 (restrn, object, environment, chart, view, drawQueue);
        }
    }

    for (auto& edgeRef: object->edgeRefs) {
        bool safe = false, unsafe = false, locSafety = false;
        std::optional<double> locValdco;
        if (depthRangeVal1 < settings.safetyContour) {
            safe = true;
        } else {
            unsafe = true;
        }
        auto [sharedWithDeepContour, deepContourIndex] = getEdgeSharedWith (object, edgeRef.index, features, OBJ_CLASSES::DEPCNT);
        if (sharedWithDeepContour) {
            auto valdco = features [deepContourIndex].getAttr (ATTRS::VALDCO);

            if (valdco && !valdco->noValue) {
                locValdco = valdco->floatValue;
            } else {
                locValdco = 0.0;
            }
        }
        if (locValdco.has_value () && locValdco.value () == settings.safetyContour) {
            locSafety = true;
        } else {
            auto [sharedWithDrgOrDepArea, areaIndex] = getEdgeSharedWith (object, edgeRef.index, features, OBJ_CLASSES::DEPARE, OBJ_CLASSES::DRGARE);

            if (sharedWithDrgOrDepArea) {
                auto locDrval1 = features [areaIndex].getAttr (ATTRS::DRVAL1);
                double locDepthRangeValue = (locDrval1 && !locDrval1->noValue) ? locDrval1->floatValue : -1.0;

                if (locDepthRangeValue < settings.safetyContour) {
                    unsafe = true;
                } else {
                    safe = true;
                }
            } else {
                if (
                    isEdgeSharedWithTg1Object (object, edgeRef.index, features) &&
                    isEdgeSharedWith (object, edgeRef.index, features, OBJ_CLASSES::LNDARE, OBJ_CLASSES::UNSARE) &&
                    isEdgeSharedWith (
                        object,
                        edgeRef.index,
                        features,
                        OBJ_CLASSES::RIVERS,
                        OBJ_CLASSES::LAKARE,
                        OBJ_CLASSES::CANALS,
                        OBJ_CLASSES::LOKBSN,
                        OBJ_CLASSES::DOCARE
                    ) &&
                    !isEdgeSharedWithLinerStructure (object, edgeRef.index, features)
                ) {
                    unsafe = true;
                }

            }
        }

        if (!locSafety && (!safe || !unsafe)) continue;

        // Edge customization
        item->customEdgePres = true;

        edgeRef.customPres = true;
        edgeRef.displayPriority = 8;
        //edgeRef.radarPriority = 'O';
        edgeRef.dispCat = DisplayCat::DISPLAY_BASE;
        edgeRef.viewingGroup = 13010;
        edgeRef.penIndex = dai.getPenIndex ("DEPSC");
        edgeRef.penWidth = 2;

        auto locQuapos = object->getEdgeAttr (edgeRef, ATTRS::VALDCO, edges);

        if (!locQuapos || locQuapos->noValue || locQuapos->intValue != 1 && locQuapos->intValue != 10 && locQuapos->intValue != 11) {
            edgeRef.penStyle = PS_DASH;
        } else {
            edgeRef.penStyle = PS_SOLID;
        }

        if (settings.safetyContourLabels && locValdco.has_value ()) {
            std::vector<std::string> symbols;
            safcon01 (object, locValdco.value (), symbols);

            for (auto& symbol: symbols) {
                edgeRef.symbols.emplace_back (environment.dai.getSymbolIndex (symbol.c_str ()));
            }
        }
    }
}

void depcnt03 (LookupTableItem *item, FeatureObject *object, Environment& environment, Chart& chart, View& view, DrawQueue& drawQueue) {
    Settings& settings = environment.settings;
    Dai& dai = environment.dai;

    for (auto& edgeRef: object->edgeRefs) {
        auto quapos = object->getEdgeAttr (edgeRef, ATTRS::QUAPOS, chart.edges);

        edgeRef.customPres = true;
        edgeRef.penWidth = 1;
        edgeRef.penIndex = dai.getBasePenIndex ("DEPCN");
        edgeRef.penStyle = PS_SOLID;

        if (quapos && !quapos->noValue) {
            if (quapos->intValue != 1 && quapos->intValue != 10 && quapos->intValue != 11) {
                edgeRef.penStyle = PS_DASH;
            }
        }

        if (environment.settings.displayContourLabels) {
            auto valdco = object->getEdgeAttr (edgeRef, ATTRS::VALDCO, chart.edges);
            std::vector<std::string> symbols;

            safcon01 (object, (valdco && !valdco->noValue) ? valdco->floatValue : 0.0, symbols);

            for (auto& symbol: symbols) {
                edgeRef.symbols.emplace_back (environment.dai.getSymbolIndex (symbol.c_str ()));
            }
        }
    }
}

void sndfrm04 (FeatureObject *object, double depth, Chart& chart, Environment& environment, std::vector<std::string>& symbols) {
    Settings& settings = environment.settings;
    std::string prefix;
    if (depth <= settings.safetyDepth) {
        prefix = "SOUNDS";
    } else {
        prefix = "SOUNDG";
    }

    auto tecsou = object->getAttr (ATTRS::TECSOU);

    if (tecsou && !tecsou->noValue && (tecsou->listIncludes (4) || tecsou->listIncludes (6))) {
        symbols.emplace_back (prefix + "B1");
    }

    auto quasou = object->getAttr (ATTRS::QUASOU);
    auto status = object->getAttr (ATTRS::STATUS);

    static uint8_t values [] { 3, 4, 5, 8, 9, 0 };
    if (quasou && !quasou->noValue && (quasou->listIncludes (values) || status && !status->noValue && status->listIncludes (18))) {
        symbols.emplace_back (prefix + "C2");
    } else {
        // Check spatial object
        auto objectsUnder = object->primitive == 1 ? chart.getListOfSpatialsUnderPoint (*object) : chart.getListOfSpatialsUnderSpatial (*object);
        
        if (objectsUnder && objectsUnder->size () > 0) {
            auto& area = chart.features [objectsUnder->front ().areaIndex];
            auto quapos = area.getAttr (ATTRS::QUAPOS);

            if (quapos && !quapos->noValue && quapos->intValue != 1 && quapos->intValue != 10 && quapos->intValue != 11) {
                symbols.emplace_back (prefix + "C2");
            }
        }
    }

    if (depth < 0.0) {
        symbols.emplace_back (prefix + "A1");
    }

    std::string depthStr = std::to_string (fabs (depth));

    if (depth < 10.0) {
        // algo1
        symbols.emplace_back (prefix + "1" + depthStr [0]);

        for (size_t i = 1; i < depthStr.length (); ++ i) {
            if (depthStr [i] == '.') {
                symbols.emplace_back (prefix + "5" + depthStr [i+1]); break;
            }
        }
    } else if (depth < 31.0 && (double) (int) depth != depth) {
        // algo 2
        symbols.emplace_back (prefix + "2" + depthStr [0]);
        symbols.emplace_back (prefix + "1" + depthStr [1]);

        for (size_t i = 2; i < depthStr.length (); ++ i) {
            if (depthStr [i] == '.') {
                symbols.emplace_back (prefix + "5" + depthStr [i+1]); break;
            }
        }
    } else if (depth < 100.0) {
        // algo 3
        symbols.emplace_back (prefix + "1" + depthStr [0]);
        symbols.emplace_back (prefix + "0" + depthStr [1]);
    } else if (depth < 1000.0) {
        // algo 4
        symbols.emplace_back (prefix + "2" + depthStr [0]);
        symbols.emplace_back (prefix + "1" + depthStr [1]);
        symbols.emplace_back (prefix + "0" + depthStr [2]);
    } else if (depth < 10000.0) {
        // algo 5
        symbols.emplace_back (prefix + "2" + depthStr [0]);
        symbols.emplace_back (prefix + "1" + depthStr [1]);
        symbols.emplace_back (prefix + "0" + depthStr [2]);
        symbols.emplace_back (prefix + "4" + depthStr [3]);
    } else {
        // algo 6
        symbols.emplace_back (prefix + "3" + depthStr [0]);
        symbols.emplace_back (prefix + "2" + depthStr [1]);
        symbols.emplace_back (prefix + "1" + depthStr [2]);
        symbols.emplace_back (prefix + "0" + depthStr [3]);
        symbols.emplace_back (prefix + "4" + depthStr [4]);
    }
}

std::tuple<std::optional<double>, std::optional<double>> depval02 (
    FeatureObject *object,
    Chart& chart,
    std::optional<int> waterLevel,
    std::optional<int> soundingExposition
) {
    std::optional<double> leastDepth, seabedDepth;
    SpatialsUnderObject *list;
    if (object->primitive == 1 || object->primitive == 4) {
        list = chart.getListOfSpatialsUnderPoint (*object);
    } else {
        list = chart.getListOfSpatialsUnderSpatial (*object);
    }
    if (list) {
        for (auto& item: *list) {
            auto& spatial = chart.features [item.areaIndex];

            if (spatial.classCode == OBJ_CLASSES::UNSARE) break;

            if (item.depthRangeValue1.has_value ()) {
                if (!leastDepth.has_value ()) {
                    leastDepth = item.depthRangeValue1.value ();
                } else if (leastDepth.value () < item.depthRangeValue1.value ()) {
                    leastDepth = item.depthRangeValue1.value ();
                }
            }
        }
    }
    if (leastDepth.has_value ()) {
        if (waterLevel.has_value () && waterLevel.value () == 3 && soundingExposition.has_value () && (soundingExposition.value () == 1 || soundingExposition.value () == 3)) {
            seabedDepth = leastDepth.value ();
        } else {
            seabedDepth = leastDepth.value ();
            leastDepth.reset ();
        }
    }
    return std::tuple<std::optional<double>, std::optional<double>> (leastDepth, seabedDepth);
}

typedef std::tuple<bool, std::optional<DisplayCat>, std::optional<int>, std::optional<int>> UdwhazResult;
UdwhazResult udwhaz05 (
    FeatureObject *object,
    double depth,
    Chart& chart,
    Environment& environment
) {
    static const UdwhazResult NO_DANGER = UdwhazResult (false, std::optional<DisplayCat> (), std::optional<int> (), std::optional<int> ());
    bool danger = false;

    if (depth <= environment.settings.safetyContour) {
        SpatialsUnderObject *list = 0;
        if (object->primitive == 1 || object->primitive == 4) {
            list = chart.getListOfSpatialsUnderPoint (*object);
        } else if (object->primitive == 2 || object->primitive == 3) {
            list = chart.getListOfSpatialsUnderSpatial (*object);
        }

        if (list) {
            for (auto& item: *list) {
                if (item.classCode == OBJ_CLASSES::DEPARE || item.classCode == OBJ_CLASSES::DRGARE) {
                    if (item.depthRangeValue1.has_value () && item.depthRangeValue1.value () >= environment.settings.safetyContour) {
                        danger = true; break;
                    }
                }
            }
        }

        if (danger) {
            auto watlev = object->getAttr (ATTRS::WATLEV);

            if (watlev && !watlev->noValue && (watlev->intValue == 1 || watlev->intValue == 2)) {
                return UdwhazResult (false, DisplayCat::DISPLAY_BASE, 8, 14050);
            } else {
                return UdwhazResult (true, DisplayCat::DISPLAY_BASE, 8, 14010);
            }
        } else if (environment.settings.showIsolatedDanger) {
            SpatialsUnderObject *list;
            if (object->primitive == 1 || object->primitive == 4) {
                list = chart.getListOfSpatialsUnderPoint (*object);
            } else {
                list = chart.getListOfSpatialsUnderSpatial (*object);
            }
            for (auto& item: *list) {
                auto& spatial = chart.features [item.areaIndex];

                if (spatial.classCode == OBJ_CLASSES::DEPARE || spatial.classCode == OBJ_CLASSES::DRGARE) {
                    if (item.depthRangeValue1.has_value () && item.depthRangeValue1.value () >= 0 && item.depthRangeValue1.value () < environment.settings.safetyContour) {
                        danger = true; break;
                    }
                }
            }

            if (!danger) return NO_DANGER;

            auto watlev = object->getAttr (ATTRS::WATLEV);

            if (watlev && !watlev->noValue && (watlev->intValue == 1 || watlev->intValue == 2)) {
                return UdwhazResult (false, DisplayCat::DISPLAY_BASE, 8, 24050);
            } else {
                return UdwhazResult (true, DisplayCat::DISPLAY_BASE, 8, 24020);
            }
        } else {
            return NO_DANGER;
        }
    }

    return NO_DANGER;
}

bool quapnt02 (FeatureObject *object, Chart& chart, Environment& environment) {
    bool accurate = true;

    if (environment.settings.showLowAccuracy) {
        for (auto& edgeRef: object->edgeRefs) {
            auto quapos = object->getEdgeAttr (edgeRef, ATTRS::QUAPOS, chart.edges);

            if (quapos && !quapos->noValue && quapos->intValue >= 2 && quapos->intValue <= 9) {
                accurate = false; break;
            }
        }
    }

    return !accurate;
}

void wrecks05 (LookupTableItem *item, FeatureObject *object, Environment& environment, Chart& chart, View& view, DrawQueue& drawQueue) {
    auto valsou = object->getAttr (ATTRS::VALSOU);
    double depth;
    std::vector<std::string> symbols;

    if (valsou && !valsou->noValue) {
        depth = valsou->floatValue;
        item->viewingGroup = 34051;

        sndfrm04 (object, depth, chart, environment, symbols);
    } else {
        auto watlev = object->getAttr (ATTRS::WATLEV);
        auto expsou = object->getAttr (ATTRS::EXPSOU);
        auto catwrk = object->getAttr (ATTRS::CATWRK);
        std::optional<int> waterLevel, soundingExposition;
        if (watlev && !watlev->noValue) waterLevel = watlev->intValue;
        if (expsou && !expsou->noValue) soundingExposition = expsou->intValue;
        auto [leastDepth, seabedDepth] = depval02 (object, chart, waterLevel, soundingExposition);

        if (!leastDepth.has_value ()) {
            if (catwrk && !catwrk->noValue) {
                if (catwrk->intValue == 1) {
                    depth = 20.1;

                    if (seabedDepth.has_value ()) {
                        leastDepth = seabedDepth.value () - 66.0;
                    }
                    if (leastDepth.value () >= 20.1) {
                        depth = leastDepth.value ();
                    }
                } else {
                    depth = -15.0;
                }
            } else if (watlev && !watlev->noValue) {
                if (watlev->intValue == 3 || watlev->intValue == 5) {
                    depth = 0.0;
                } else {
                    depth = -15.0;
                }
            } else {
                depth = -15.0;
            }
        } else {
            depth = leastDepth.value ();
        }
    }
    auto [isolatedDanger, displayCat, priority, viewingGroup] = udwhaz05 (object, depth, chart, environment);
    bool lowAccuracy = quapnt02 (object, chart, environment);

    if (displayCat.has_value ()) item->displayCat = displayCat.value ();
    if (priority.has_value ()) item->displayPriority = priority.value ();
    if (viewingGroup.has_value ()) item->viewingGroup = viewingGroup.value ();

    if (object->primitive == 1) {
        auto& pos = chart.nodes [object->nodeIndex].points.front ();
        if (isolatedDanger) {
            drawQueue.addSymbol (pos.lat, pos.lon, environment.dai.getSymbolIndex ("ISODGR01"), 0.0, environment.dai);
            if (lowAccuracy) {
                drawQueue.addSymbol (pos.lat, pos.lon, environment.dai.getSymbolIndex ("LOWACC01"), 0.0, environment.dai);
            }
        } else {
            // cont A
            if (valsou && !valsou->noValue) {
                if (valsou->floatValue <= environment.settings.safetyDepth) {
                    drawQueue.addSymbol (pos.lat, pos.lon, environment.dai.getSymbolIndex ("DANGER01"), 0.0, environment.dai);
                } else {
                    drawQueue.addSymbol (pos.lat, pos.lon, environment.dai.getSymbolIndex ("DANGER02"), 0.0, environment.dai);
                }
            } else {
                auto watlev = object->getAttr (ATTRS::WATLEV);
                auto catwrk = object->getAttr (ATTRS::CATWRK);

                item->displayPriority = 4;
                item->displayCat = DisplayCat::CUSTOM;
                item->viewingGroup = 34050;
                if (catwrk && !catwrk->noValue && catwrk->intValue == 1 && watlev && !watlev->noValue && watlev->intValue == 3) {
                    drawQueue.addSymbol (pos.lat, pos.lon, environment.dai.getSymbolIndex ("WRECKS04"), 0.0, environment.dai);
                } else if (catwrk && !catwrk->noValue && catwrk->intValue == 2 && watlev && !watlev->noValue && watlev->intValue == 3) {
                    drawQueue.addSymbol (pos.lat, pos.lon, environment.dai.getSymbolIndex ("WRECKS05"), 0.0, environment.dai);
                } else if (catwrk && !catwrk->noValue && (catwrk->intValue == 4 || catwrk->intValue == 5)) {
                    drawQueue.addSymbol (pos.lat, pos.lon, environment.dai.getSymbolIndex ("WRECKS01"), 0.0, environment.dai);
                } else if (watlev && !watlev->noValue && (watlev->intValue == 1 || watlev->intValue == 2 || watlev->intValue == 5 || watlev->intValue == 4)) {
                    drawQueue.addSymbol (pos.lat, pos.lon, environment.dai.getSymbolIndex ("WRECKS05"), 0.0, environment.dai);
                } else {
                    drawQueue.addSymbol (pos.lat, pos.lon, environment.dai.getSymbolIndex ("WRECKS05"), 0.0, environment.dai);
                }
            }
            if (lowAccuracy) {
                drawQueue.addSymbol (pos.lat, pos.lon, environment.dai.getSymbolIndex ("LOWACC01"), 0.0, environment.dai);
            }
        }
    } else {
        // cont B
        auto watlev = object->getAttr (ATTRS::WATLEV);
        auto catwrk = object->getAttr (ATTRS::CATWRK);

        for (auto& edgeRef: object->edgeRefs) {
            auto edgeQuapos = object->getEdgeAttr (edgeRef, ATTRS::QUAPOS, chart.edges);

            if (edgeQuapos && !edgeQuapos->noValue) {
                if (edgeQuapos->intValue != 1 && edgeQuapos->intValue != 10 && edgeQuapos->intValue != 11) {
                    edgeRef.customPres = true;
                    edgeRef.symbols.emplace_back (environment.dai.getSymbolIndex ("LOWACC41"));
                    continue;
                }
            }

            if (isolatedDanger) {
                edgeRef.customPres = true;
                edgeRef.penIndex = environment.dai.getBasePenIndex ("CHBLK");
                edgeRef.penStyle = PS_DOT;
                edgeRef.penWidth = 2;
                continue;
            }

            auto edgeValsou = object->getEdgeAttr (edgeRef, ATTRS::VALSOU, chart.edges);

            edgeRef.customPres = true;
            edgeRef.penWidth = 2;
            edgeRef.viewingGroup = 34050;

            if (edgeValsou && !edgeValsou->noValue) {
                edgeRef.penIndex = environment.dai.getBasePenIndex ("CHBLK");
                if (edgeValsou->floatValue <= environment.settings.safetyDepth) {
                    edgeRef.penStyle = PS_DOT;
                } else {
                    edgeRef.penStyle = PS_DASH;
                }
            } else {
                edgeRef.penIndex = environment.dai.getBasePenIndex ("CSTLN");
                edgeRef.displayPriority = 4;

                if (watlev && !watlev->noValue && (watlev->intValue == 1 || watlev->intValue == 2)) {
                    edgeRef.penStyle = PS_SOLID;
                } else if (watlev && !watlev->noValue && watlev->intValue == 4) {
                    edgeRef.penStyle = PS_DASH;
                } else if (watlev && !watlev->noValue && (watlev->intValue == 5 || watlev->intValue == 3)) {
                    edgeRef.penStyle = PS_DOT;
                } else {
                    edgeRef.penStyle = PS_DOT;
                }
            }
        }

        double lat, lon;
        getCenterPos (*object, chart, lat, lon);

        if (valsou && !valsou->noValue) {
            if (isolatedDanger) {
                drawQueue.removeAllSymbols ();
                drawQueue.addSymbol (lat, lon, environment.dai.getSymbolIndex ("ISODGR01"), 0.0, environment.dai);
            }
        }

        if (lowAccuracy) {
            drawQueue.addSymbol (lat, lon, environment.dai.getSymbolIndex ("LOWACC01"), 0.0, environment.dai);
        }
    }
}

void soundg03 (LookupTableItem *item, FeatureObject *object, Environment& environment, Chart& chart, View& view, DrawQueue& drawQueue) {
    auto& node = chart.nodes.container [object->nodeIndex];

    for (auto& pos: node.points) {
        std::vector<std::string> symbols;

        sndfrm04 (object, pos.depth, chart, environment, symbols);

        for (auto& symbolName: symbols) {
            size_t symbolIndex = environment.dai.getSymbolIndex (symbolName.c_str ());

            drawQueue.addSymbol (pos.lat, pos.lon, symbolIndex, 0.0, environment.dai);
        }
    }
}

void qualin02 (LookupTableItem *item, FeatureObject *object, Environment& environment, Chart& chart, View& view, DrawQueue& drawQueue) {
    for (auto& edgeRef: object->edgeRefs) {
        auto quapos = object->getEdgeAttr (edgeRef, ATTRS::QUAPOS, chart.edges);

        if (quapos && !quapos->noValue) {
            if (quapos->intValue == 1 || quapos->intValue == 10 || quapos->intValue == 11) {
                edgeRef.customPres = true;
                edgeRef.symbols.emplace_back (environment.dai.getSymbolIndex ("LOWACC21"));
                continue;
            }
        }
        if (object->classCode == OBJ_CLASSES::COALNE) {
            auto conrad = object->getAttr (ATTRS::CONRAD);

            edgeRef.customPres = true;

            if (conrad && !conrad->noValue) {
                if (conrad->intValue = 1) {
                    edgeRef.penIndex = environment.dai.getBasePenIndex ("CHMGF");
                    edgeRef.penStyle = PS_SOLID;
                    edgeRef.penWidth = 3;
                    edgeRef.secondPen = true;
                    edgeRef.secondPenIndex = environment.dai.getBasePenIndex ("CSTLN");
                    edgeRef.secondPenStyle = PS_SOLID;
                    edgeRef.secondPenWidth = 1;
                } else {
                    edgeRef.penIndex = environment.dai.getBasePenIndex ("CSTLN");
                    edgeRef.penStyle = PS_SOLID;
                    edgeRef.penWidth = 1;
                }
            } else {
                edgeRef.penIndex = environment.dai.getBasePenIndex ("CSTLN");
                edgeRef.penStyle = PS_SOLID;
                edgeRef.penWidth = 1;
            }
        } else {
            edgeRef.penIndex = environment.dai.getBasePenIndex ("CSTLN");
            edgeRef.penStyle = PS_SOLID;
            edgeRef.penWidth = 1;
            continue;
        }
    }
}

void obstrn07 (LookupTableItem *item, FeatureObject *object, Environment& environment, Chart& chart, View& view, DrawQueue& drawQueue) {
    auto valsou = object->getAttr (ATTRS::VALSOU);
    double depth;

    if (valsou && !valsou->noValue) {
        depth = valsou->floatValue;
    } else {
        auto watlev = object->getAttr (ATTRS::WATLEV);
        auto expsou = object->getAttr (ATTRS::EXPSOU);

        std::optional<int> waterLevel, soundingExposition;

        if (watlev && !watlev->noValue) waterLevel = watlev->intValue;
        if (expsou && !expsou->noValue) soundingExposition = expsou->intValue;

        auto [leastDepth, seabedDepth] = depval02 (object, chart, waterLevel, soundingExposition);
        
        if (leastDepth.has_value ()) {
            depth = leastDepth.value ();
        } else {
            auto catobs = object->getAttr (ATTRS::CATOBS);

            if (catobs && !catobs->noValue && catobs->intValue == 6 || watlev && !watlev->noValue && watlev->intValue == 3) {
                depth = 0.01;
            } else if (watlev && !watlev->noValue && watlev->intValue == 5) {
                depth = 0.0;
            } else {
                depth = -15.0;
            }
        }

        auto [isolatedDanger, displayCat, prty, viewingGroup] = udwhaz05 (object, depth, chart, environment);

        if (object->primitive == 1 || object->primitive == 4) {
            // cont a
            bool lowAccuracy = quapnt02 (object, chart, environment);
            auto& pos = chart.nodes [object->nodeIndex].points.front ();

            if (isolatedDanger) {
                drawQueue.addSymbol (pos.lat, pos.lon, environment.dai.getSymbolIndex ("ISODGR01"), 0.0, environment.dai);

                if (lowAccuracy) drawQueue.addSymbol (pos.lat, pos.lon, environment.dai.getSymbolIndex ("LOWACC01"), 0.0, environment.dai);

                return; 
            }

            bool sounding = false;
            std::string symbolName;

            if (valsou && !valsou->noValue) {
                if (valsou->floatValue <= environment.settings.safetyDepth) {
                    if (object->classCode == OBJ_CLASSES::UWTROC) {
                        if (watlev && !watlev->noValue && (watlev->intValue == 4 || watlev->intValue == 5)) {
                            symbolName = "UWTROC04";
                            sounding = false;
                        } else {
                            symbolName = "DANGER01";
                            sounding = true;
                        }
                    } else {
                        auto catobs = object->getAttr (ATTRS::CATOBS);

                        if (catobs && !catobs->noValue && catobs->intValue == 6) {
                            symbolName = "DANGER01";
                            sounding = true;
                        } else if (watlev && !watlev->noValue && (watlev->intValue == 1 || watlev->intValue == 2)) {
                            symbolName = "OBSTRN11";
                            sounding = false;
                        } else if (watlev && !watlev->noValue && (watlev->intValue == 4 || watlev->intValue == 5)) {
                            symbolName = "DANGER03";
                            sounding = true;
                        } else {
                            symbolName = "DANGER01";
                            sounding = true;
                        }
                    }
                } else {
                    symbolName = "DANGER02";
                    sounding = true;
                }

                drawQueue.addSymbol (pos.lat, pos.lon, environment.dai.getSymbolIndex (symbolName.c_str ()), 0.0, environment.dai);

                if (sounding) {
                    std::vector<std::string> symbols;

                    sndfrm04 (object, depth, chart, environment, symbols);

                    for (auto& symbol: symbols) {
                        drawQueue.addSymbol (pos.lat, pos.lon, environment.dai.getSymbolIndex (symbol.c_str ()), 0.0, environment.dai);
                    }

                    if (lowAccuracy) drawQueue.addSymbol (pos.lat, pos.lon, environment.dai.getSymbolIndex ("LOWACC01"), 0.0, environment.dai);
                }
            }
        } else if (object->primitive == 2) {
            // cont b
            for (auto& edgeRef: object->edgeRefs) {
                auto quapos = object->getEdgeAttr (edgeRef, ATTRS::QUAPOS, chart.edges);

                edgeRef.customPres = true;

                if (quapos && !quapos->noValue && quapos->intValue >= 2 && quapos->intValue <= 9) {
                    edgeRef.symbols.emplace_back (environment.dai.getSymbolIndex (isolatedDanger ? "LOWACC41" : "LOWACC31"));
                    continue;
                }
                
                edgeRef.penWidth = 2;
                edgeRef.penIndex = environment.dai.getBasePenIndex ("CHBLK");

                if (isolatedDanger) {
                    edgeRef.penStyle = PS_DOT; continue;
                }

                if (valsou && !valsou->noValue) {
                    if (valsou->floatValue <= environment.settings.safetyDepth) {
                        edgeRef.penStyle = PS_DOT;
                    } else {
                        edgeRef.penStyle = PS_DASH;
                    }
                } else {
                    edgeRef.penStyle = PS_DOT;
                }
            }
        } else {
            // cont c
            bool lowAccuracy = quapnt02 (object, chart, environment);

            double lat, lon;
            getCenterPos (*object, chart, lat, lon);

            if (isolatedDanger) {
                item->brushIndex = environment.dai.getBasePenIndex ("DEPVS");
                item->patternBrushIndex = environment.dai.getPatternIndex ("FOULAR01");
                item->edgePenIndex = environment.dai.getBasePenIndex ("CHBLK");
                item->edgePenStyle = PS_DOT;
                item->edgePenWidth = 2;
                
                drawQueue.addSymbol (lat, lon, environment.dai.getSymbolIndex ("ISODGR01"), 0, environment.dai);

                if (lowAccuracy) {
                    drawQueue.addSymbol (lat, lon, environment.dai.getSymbolIndex ("LOWACC01"), 0, environment.dai);
                }
                return;
            }

            if (valsou && !valsou->noValue) {
                if (valsou->floatValue <= environment.settings.safetyDepth) {
                    item->edgePenIndex = environment.dai.getBasePenIndex ("CHGRD");
                    item->edgePenStyle = PS_DASH;
                    item->edgePenWidth = 2;
                } else {
                    item->edgePenIndex = environment.dai.getBasePenIndex ("CHBLK");
                    item->edgePenStyle = PS_DOT;
                    item->edgePenWidth = 2;
                }

                std::vector<std::string> symbols;
                sndfrm04 (object, depth, chart, environment, symbols);
 
                for (auto& symbol: symbols) {
                    drawQueue.addSymbol (lat, lon, environment.dai.getSymbolIndex (symbol.c_str ()), 0, environment.dai);
                }
            } else {
                auto catobs = object->getAttr (ATTRS::CATOBS);

                if (catobs && !catobs->noValue && catobs->intValue == 6) {
                    item->patternBrushIndex = environment.dai.getPatternIndex ("FOULAR01");
                    item->edgePenIndex = environment.dai.getBasePenIndex ("CHBLK");
                    item->edgePenStyle = PS_DOT;
                    item->edgePenWidth = 2;
                } else if (watlev && !watlev->noValue && (watlev->intValue == 1 || watlev->intValue == 2)) {
                    item->brushIndex = environment.dai.getBasePenIndex ("CHBRN");
                    item->edgePenIndex = environment.dai.getBasePenIndex ("CSTLN");
                    item->edgePenStyle = PS_SOLID;
                    item->edgePenWidth = 2;
                } else if (watlev && !watlev->noValue && watlev->intValue == 4) {
                    item->brushIndex = environment.dai.getBasePenIndex ("DEPIT");
                    item->edgePenIndex = environment.dai.getBasePenIndex ("CSTLN");
                    item->edgePenStyle = PS_DASH;
                    item->edgePenWidth = 2;
                } else {
                    item->brushIndex = environment.dai.getBasePenIndex ("DEPVS");
                    item->edgePenIndex = environment.dai.getBasePenIndex ("CHBLK");
                    item->edgePenStyle = PS_DOT;
                    item->edgePenWidth = 2;
                }
            }

            if (lowAccuracy) {
                drawQueue.addSymbol (lat, lon, environment.dai.getSymbolIndex ("LOWACC01"), 0, environment.dai);
            }
        }
    }
}

void quapos01 (LookupTableItem *item, FeatureObject *object, Environment& environment, Chart& chart, View& view, DrawQueue& drawQueue) {
    if (object->primitive == 2) {
        qualin02 (item, object, environment, chart, view, drawQueue);
    } else if (quapnt02 (object, chart, environment)) {
        auto& pos = chart.nodes [object->nodeIndex].points.front ();
        drawQueue.addSymbol (pos.lat, pos.lon, environment.dai.getSymbolIndex ("LOWACC01"), 0.0, environment.dai);
    }
}

void slcons04 (LookupTableItem *item, FeatureObject *object, Environment& environment, Chart& chart, View& view, DrawQueue& drawQueue) {
    if (object->primitive == 1 || object->primitive == 4) {
        if (quapnt02 (object, chart, environment)) {
            auto& pos = chart.nodes [object->nodeIndex].points.front ();
            drawQueue.addSymbol (pos.lat, pos.lon, environment.dai.getSymbolIndex ("LOWACC01"), 0.0, environment.dai);
        }
    } else {
        for (auto& edgeRef: object->edgeRefs) {
            auto quapos = object->getEdgeAttr (edgeRef, ATTRS::QUAPOS, chart.edges);

            if (quapos && !quapos->noValue && (quapos->intValue == 1 || quapos->intValue == 10 || quapos->intValue == 11)) {
                edgeRef.symbols.emplace_back (environment.dai.getSymbolIndex ("LOWACC01"));
            } else {
                auto condtn = object->getEdgeAttr (edgeRef, ATTRS::CONDTN, chart.edges);
                auto catslc = object->getEdgeAttr (edgeRef, ATTRS::CATSLC, chart.edges);
                auto watlev = object->getEdgeAttr (edgeRef, ATTRS::WATLEV, chart.edges);

                edgeRef.customPres = true;
                edgeRef.penIndex = environment.dai.getBasePenIndex ("CSTLN");

                if (condtn && !condtn->noValue && (condtn->intValue == 1 || condtn->intValue == 2)) {
                    edgeRef.penStyle = PS_DASH;
                    edgeRef.penWidth = 1;
                } else if (catslc && !catslc->noValue && (catslc->intValue == 6 || catslc->intValue == 15 || catslc->intValue == 16)) {
                    edgeRef.penStyle = PS_SOLID;
                    edgeRef.penWidth = 4;
                } else if (watlev && !watlev->noValue && (watlev->intValue == 3 || watlev->intValue == 4)) {
                    edgeRef.penStyle = PS_DASH;
                    edgeRef.penWidth = 2;
                } else if (watlev && !watlev->noValue && watlev->intValue == 2) {
                    edgeRef.penStyle = PS_SOLID;
                    edgeRef.penWidth = 2;
                } else {
                    edgeRef.penStyle = PS_SOLID;
                    edgeRef.penWidth = 2;
                }
            }
        }
    }
}

void restrn01 (LookupTableItem *item, FeatureObject *object, Environment& environment, Chart& chart, View& view, DrawQueue& drawQueue) {
    auto restrn = object->getAttr (ATTRS::RESTRN);

    if (restrn && !restrn->noValue) {
        rescsp03 (restrn, object, environment, chart, view, drawQueue);
    }
}

void resare04 (LookupTableItem *item, FeatureObject *object, Environment& environment, Chart& chart, View& view, DrawQueue& drawQueue) {
if(object->fidn==29140887){
int iii=0;
++iii;
--iii;
}
    auto restrn = object->getAttr (ATTRS::RESTRN);
    auto catrea = object->getAttr (ATTRS::CATREA);
    auto catreaIncludes = [&catrea] (uint8_t *list) {
        return catrea && !catrea->noValue && catrea->listIncludes (list);
    };

    double lat, lon;
    getCenterPos (*object, chart, lat, lon);

    if (restrn && !restrn->noValue) {
        if (restrn->listIncludes (_7_8_14)) {
            // cont A
            item->displayPriority = 6;

            if (restrn->listIncludes (_1_2_3_4_5_6_13_16_17_23_24_25_26_27)) {
                drawQueue.addSymbol (lat, lon, environment.dai.getSymbolIndex ("ENTRES61"), 0.0, environment.dai);
            } else if (catreaIncludes (_1_8_9_12_14_18_19_21_24_25_26)) {
                drawQueue.addSymbol (lat, lon, environment.dai.getSymbolIndex ("ENTRES61"), 0.0, environment.dai);
            } else if (restrn->listIncludes (_9_10_11_12_15_18_19_20_21_22)) {
                drawQueue.addSymbol (lat, lon, environment.dai.getSymbolIndex ("ENTRES71"), 0.0, environment.dai);
            } else if (catreaIncludes (_4_5_6_7_10_20_22_23)) {
                drawQueue.addSymbol (lat, lon, environment.dai.getSymbolIndex ("ENTRES71"), 0.0, environment.dai);
            } else {
                drawQueue.addSymbol (lat, lon, environment.dai.getSymbolIndex ("ENTRES51"), 0.0, environment.dai);
            }

            if (environment.settings.symbolizedBoundaries) {
                item->lineCharIndex = environment.dai.getSymbolIndex ("ENTRES51)");
            } else {
                item->edgePenIndex = environment.dai.getBasePenIndex ("CHMGD");
                item->edgePenStyle = PS_DASH;
                item->edgePenWidth = 2;
            }
        } else if (restrn->listIncludes (_1_2)) {
            // cont B
            item->displayPriority = 6;

            if (restrn->listIncludes (_3_4_5_6_13_16_17_23_24_25_26_27)) {
                drawQueue.addSymbol (lat, lon, environment.dai.getSymbolIndex ("ACHRES61"), 0.0, environment.dai);
            } else if (catreaIncludes (_1_8_9_12_14_18_19_21_24_25_26)) {
                drawQueue.addSymbol (lat, lon, environment.dai.getSymbolIndex ("ACHRES61"), 0.0, environment.dai);
            } else if (restrn->listIncludes (_9_10_11_12_15_18_19_20_21_22)) {
                drawQueue.addSymbol (lat, lon, environment.dai.getSymbolIndex ("ACHRES71"), 0.0, environment.dai);
            } else if (catreaIncludes (_4_5_6_7_10_20_22_23)) {
                drawQueue.addSymbol (lat, lon, environment.dai.getSymbolIndex ("ACHRES71"), 0.0, environment.dai);
            } else {
                drawQueue.addSymbol (lat, lon, environment.dai.getSymbolIndex ("ACHRES51"), 0.0, environment.dai);
            }

            if (environment.settings.symbolizedBoundaries) {
                item->lineCharIndex = environment.dai.getSymbolIndex ("ACHRES51)");
            } else {
                item->edgePenIndex = environment.dai.getBasePenIndex ("CHMGD");
                item->edgePenStyle = PS_DASH;
                item->edgePenWidth = 2;
            }
        } else if (restrn->listIncludes (_3_4_5_6_24)) {
            // cont C
            item->displayPriority = 6;

            if (restrn->listIncludes (_13_16_17_23_24_25_26_27)) {
                drawQueue.addSymbol (lat, lon, environment.dai.getSymbolIndex ("FSHRES61"), 0.0, environment.dai);
            } else if (catreaIncludes (_1_8_9_12_14_18_19_21_24_25_26)) {
                drawQueue.addSymbol (lat, lon, environment.dai.getSymbolIndex ("FSHRES61"), 0.0, environment.dai);
            } else if (restrn->listIncludes (_9_10_11_12_15_18_19_20_21_22)) {
                drawQueue.addSymbol (lat, lon, environment.dai.getSymbolIndex ("FSHRES71"), 0.0, environment.dai);
            } else if (catreaIncludes (_4_5_6_7_10_20_22_23)) {
                drawQueue.addSymbol (lat, lon, environment.dai.getSymbolIndex ("FSHRES71"), 0.0, environment.dai);
            } else {
                drawQueue.addSymbol (lat, lon, environment.dai.getSymbolIndex ("FSHRES51"), 0.0, environment.dai);
            }

            if (environment.settings.symbolizedBoundaries) {
                item->lineCharIndex = environment.dai.getSymbolIndex ("FSHRES51)");
            } else {
                item->edgePenIndex = environment.dai.getBasePenIndex ("CHMGD");
                item->edgePenStyle = PS_DASH;
                item->edgePenWidth = 2;
            }
        } else if (restrn->listIncludes (_13_16_17_23_25_26_27)) {
            // cont D
            if (restrn->listIncludes (_9_10_11_12_15_18_19_20_21_22)) {
                drawQueue.addSymbol (lat, lon, environment.dai.getSymbolIndex ("CTYARE71"), 0.0, environment.dai);
            } else if (catreaIncludes (_4_5_6_7_10_20_22_23)) {
                drawQueue.addSymbol (lat, lon, environment.dai.getSymbolIndex ("CTYARE71"), 0.0, environment.dai);
            } else {
                drawQueue.addSymbol (lat, lon, environment.dai.getSymbolIndex ("CTYARE51"), 0.0, environment.dai);
            }

            if (environment.settings.symbolizedBoundaries) {
                item->lineCharIndex = environment.dai.getSymbolIndex ("CTYARE51)");
            } else {
                item->edgePenIndex = environment.dai.getBasePenIndex ("CHMGD");
                item->edgePenStyle = PS_DASH;
                item->edgePenWidth = 2;
            }
        } else {
            if (restrn->listIncludes (_9_10_11_12_15_18_19_20_21_22)) {
                drawQueue.addSymbol (lat, lon, environment.dai.getSymbolIndex ("INFARE51"), 0.0, environment.dai);
            } else {
                drawQueue.addSymbol (lat, lon, environment.dai.getSymbolIndex ("RSRDEF51"), 0.0, environment.dai);
            }

            if (environment.settings.symbolizedBoundaries) {
                item->lineCharIndex = environment.dai.getSymbolIndex ("CTYARE51)");
            } else {
                item->edgePenIndex = environment.dai.getBasePenIndex ("CHMGD");
                item->edgePenStyle = PS_DASH;
                item->edgePenWidth = 2;
            }
        }
    } else {
        // cont E
        if (catrea && !catrea->noValue) {
            if (catreaIncludes (_1_8_9_12_14_18_19_21_24_25_26)) {
                if (catreaIncludes (_4_5_6_7_10_20_22_23)) {
                    drawQueue.addSymbol (lat, lon, environment.dai.getSymbolIndex ("CTYARE71"), 0.0, environment.dai);
                } else {
                    drawQueue.addSymbol (lat, lon, environment.dai.getSymbolIndex ("CTYARE51"), 0.0, environment.dai);
                }
            } else if (catreaIncludes (_4_5_6_7_10_20_22_23)) {
                drawQueue.addSymbol (lat, lon, environment.dai.getSymbolIndex ("CTYARE51"), 0.0, environment.dai);
            } else {
                drawQueue.addSymbol (lat, lon, environment.dai.getSymbolIndex ("RSRDEF51"), 0.0, environment.dai);
            }
        } else {
            drawQueue.addSymbol (lat, lon, environment.dai.getSymbolIndex ("RSRDEF51"), 0.0, environment.dai);
        }

        if (environment.settings.symbolizedBoundaries) {
            item->lineCharIndex = environment.dai.getSymbolIndex ("CTYARE51)");
        } else {
            item->edgePenIndex = environment.dai.getBasePenIndex ("CHMGD");
            item->edgePenStyle = PS_DASH;
            item->edgePenWidth = 2;
        }
    }
}

void lights06 (LookupTableItem *item, FeatureObject *object, Environment& environment, Chart& chart, View& view, DrawQueue& drawQueue) {
    Nodes& nodes = chart.nodes;
    Features& features = chart.features;
    Dai& dai = environment.dai;
    Settings& settings = environment.settings;
    auto valnmr = object->getAttr (ATTRS::VALNMR);
    Attr *orient = 0;
    auto position = nodes [object->nodeIndex].points.front ();

    double nominalRange = (valnmr && !valnmr->noValue) ? valnmr->floatValue : 9.0;

    auto catlit = object->getAttr (ATTRS::CATLIT);

    if (catlit && !catlit->noValue) {
        switch (catlit->intValue) {
            case 11:
            case 8: {
                item->symbols.clear ();
                item->symbols.emplace_back (dai.getSymbolIndex ("LIGHTS82"));
                return;
            }
            case 9: {
                item->symbols.clear ();
                item->symbols.emplace_back (dai.getSymbolIndex ("LIGHTS81"));
                return;
            }
            case 1:
            case 16: {
                orient = object->getAttr (ATTRS::ORIENT);
                if (orient && !orient->noValue) {
                    item->penIndex = dai.getPenIndex ("LS(DASH,1,CHBLK");
                }
                break;
            }
        }
    }

    auto colour = object->getAttr (ATTRS::COLOUR);
    auto litchr = object->getAttr (ATTRS::LITCHR);
    auto sectr1 = object->getAttr (ATTRS::SECTR1);
    auto sectr2 = object->getAttr (ATTRS::SECTR2);

    bool flareAt45Deg = false;

    int colorValue = (!colour || colour->noValue) ? 12 : 0;

    bool noSector = (
        !sectr1 ||
        sectr1->noValue ||
        !sectr2 ||
        sectr2->noValue ||
        sectr1->floatValue == sectr2->floatValue ||
        sectr1->floatValue == 0.0 && sectr2->floatValue == 360.0
    );
    if (noSector) {
        if (
            nominalRange >= 10.0 &&
            (catlit->noValue || !catlit->listIncludes (5) && !catlit->listIncludes (6)) &&
            (litchr->noValue || litchr->intValue != 12)
        ) {
            std::string arcColorName = "CHMGD";
            if (colour && !colour->noValue) {
                if (colour->listIncludes (1) && colour->listIncludes (3)) {
                    arcColorName = "LITRD";
                } else if (colour->listIncludes (3)) {
                    arcColorName = "LITRD";
                } else if (colour->listIncludes (1) && colour->listIncludes (4)) {
                    arcColorName = "LITGN";
                } else if (colour->listIncludes (4)) {
                    arcColorName = "LITGN";
                } else if (colour->listIncludes (11)) {
                    arcColorName = "LITYW";
                } else if (colour->listIncludes (6)) {
                    arcColorName = "LITYW";
                } else if (colour->listIncludes (5) && colour->listIncludes (6)) {
                    arcColorName = "LITYW";
                } else if (colour->listIncludes (1)) {
                    arcColorName = "LITYW";
                }
            }
            auto& pos = nodes [object->nodeIndex].points.front ();

            drawQueue.addCompoundLightArc (dai.getBasePenIndex (arcColorName.c_str ()), PS_SOLID, 2, pos.lat, pos.lon, 26, 0.0, 360.0);
            /*item->drawArc = true;
            item->arcDef.start = 0.0;
            item->arcDef.end = 359.9999999;
            item->arcDef.nodeIndex = object->nodeIndex;
            item->arcDef.internalColor = dai.getBasePenIndex (arcColorName.c_str ());
            item->arcDef.radius = 26.0 / PIXEL_SIZE_IN_MM;*/
        } else {
            flareAt45Deg = false;

            if (false/*NoSectorLightsPlus*/) {
                if (colour && !colour->noValue && (colour->listIncludes (1) || colour->listIncludes (6) || colour->listIncludes (11))) {
                    flareAt45Deg = true;
                }
            }
            std::string symbolName;
            symbolName = "LITDEF11";
            if (colour && !colour->noValue) {
                if (colour->listIncludes (1) && colour->listIncludes (3)) {
                    symbolName = "LIGHTS11";
                } else if (colour->listIncludes (3)) {
                    symbolName = "LIGHTS11";
                } else if (colour->listIncludes (1) && colour->listIncludes (4)) {
                    symbolName = "LIGHTS12";
                } else if (colour->listIncludes (4)) {
                    symbolName = "LIGHTS12";
                } else if (colour->listIncludes (11)) {
                    symbolName = "LIGHTS13";
                } else if (colour->listIncludes (6)) {
                    symbolName = "LIGHTS13";
                } else if (colour->listIncludes (5) && colour->listIncludes (6)) {
                    symbolName = "LIGHTS13";
                } else if (colour->listIncludes (1)) {
                    symbolName = "LIGHTS13";
                }
            }
            item->symbols.clear ();

            if (catlit && !catlit->noValue && (catlit->intValue == 1 || catlit->intValue == 16)) {
                if (orient && !orient->noValue) {
                    // +/- 180
                    item->symbols.emplace_back (dai.getSymbolIndex (symbolName.c_str ()), 180.0);
                } else {
                    item->symbols.push_back (dai.getSymbolIndex ("QUESMRK1"));
                }
            } else if (flareAt45Deg) {
                // 45
                item->symbols.emplace_back (dai.getSymbolIndex (symbolName.c_str ()), 45);
            } else {
                // 135
                item->symbols.emplace_back (dai.getSymbolIndex (symbolName.c_str ()), 135);
            }
        }
    } else {
        // Continuation
        double sector1Value = sectr1->floatValue;
        double sector2Value = sectr2->floatValue;
 
        if (sector2Value <= sector1Value) sector2Value += 360.0;

        double legLengthMm = settings.fullSectorLength ? nominalRange * 1852000.0 / zoomToScale (view.zoom) : 25.0;

        //size_t penIndex = dai.getPenIndex ("LS(DASH,1,CHBLK)");
        //
        //item->lines.emplace_back (penIndex, LineDrawMode::USING_BRG_AND_RNG, position.lat, position.lon, sector1Value + 180.0, legLengthMm);
        //item->lines.emplace_back (penIndex, LineDrawMode::USING_BRG_AND_RNG, position.lat, position.lon, sector2Value + 180.0, legLengthMm);
        size_t penIndex = dai.getBasePenIndex ("CHBLK");
        drawQueue.addLine (penIndex, PS_DASH, 1, position.lat, position.lon, sector1Value + 180.0, legLengthMm);
        drawQueue.addLine (penIndex, PS_DASH, 1, position.lat, position.lon, sector2Value + 180.0, legLengthMm);

        bool extendedArcRadius = false;

        // Other lights at this point
        for (auto& curObject: features) {
            if (curObject.fidn != object->fidn && curObject.classCode == OBJ_CLASSES::LIGHTS) {
                bool colocated = curObject.nodeIndex == object->nodeIndex;
                if (!colocated) {
                    auto pos1 = nodes [object->nodeIndex].points.front ();
                    auto pos2 = nodes [curObject.nodeIndex].points.front ();

                    if (pos1.lat == pos2.lat && pos1.lon == pos2.lon) colocated = true;
                }
                if (colocated) {
                    auto curSectr1 = curObject.getAttr (ATTRS::SECTR1);
                    auto curSectr2 = curObject.getAttr (ATTRS::SECTR2);
                    auto curValnmr = curObject.getAttr (ATTRS::VALNMR);

                    if (curSectr1 && curSectr2 && !curSectr1->noValue && !curSectr2->noValue) {
                        double curSector1Value = curSectr1->floatValue;
                        double curSector2Value = curSectr2->floatValue;

                        //if (curSector2Value <= curSector1Value) curSector2Value += 360.0;

                        // Are two sectors overlapping?
                        if (
                            isAngleBetween (sector1Value, curSector1Value, curSector2Value) ||
                            isAngleBetween (sector2Value, curSector1Value, curSector2Value) ||
                            isAngleBetween (curSector1Value, sector1Value, sector2Value) ||
                            isAngleBetween (curSector2Value, sector1Value, sector2Value)
                        ) {
                            double curNominalRangeValue = (curValnmr && !curValnmr->noValue) ? curValnmr->floatValue : 9.0f;
                            if (curNominalRangeValue > nominalRange) {
                                extendedArcRadius = true; break;
                            }
                        }
                    }
                }
            }
        }

        size_t arcPenIndex;
        std::string arcPenName;
        int arcRadiusMm = extendedArcRadius ? 25 : 20;

        auto litvis = object->getAttr (ATTRS::LITVIS);

        int lineStyle, lineWidth;
        std::string arcColorName;
        if (litvis && !litvis->noValue && (litvis->intValue == 3 || litvis->intValue == 7 || litvis->intValue == 8)) {
            lineStyle = PS_DASH;
            lineWidth = 1;
            arcColorName = "CHBLK";
            arcPenName = "LS(DASH,1,CHBLK)";
        } else {
            arcColorName = "CHMGD";
            if (colour && !colour->noValue) {
                if (colour->listIncludes (1) && colour->listIncludes (3)) {
                    arcColorName = "LITRD";
                } else if (colour->listIncludes (3)) {
                    arcColorName = "LITRD";
                } else if (colour->listIncludes (1) && colour->listIncludes (4)) {
                    arcColorName = "LITGN";
                } else if (colour->listIncludes (4)) {
                    arcColorName = "LITGN";
                } else if (colour->listIncludes (11)) {
                    arcColorName = "LITYW";
                } else if (colour->listIncludes (6)) {
                    arcColorName = "LITYW";
                } else if (colour->listIncludes (1)) {
                    arcColorName = "LITYW";
                }
            }
            arcPenName = "LS(SOLD,2," + arcColorName + ")";
            lineStyle = PS_SOLID;
            lineWidth = 2;
        }
        /*arcPenIndex = dai.palette.checkPen (arcPenName.data (), dai.colorTable); //dai.getPenIndex (arcPenName.c_str ());
        item->lines.emplace_back (arcPenIndex, LineDrawMode::ARC, position.lat, position.lon, sector1Value, sector2Value, arcRadiusMm);*/
        drawQueue.addCompoundLightArc (
            dai.getBasePenIndex (arcColorName.c_str ()),
            lineStyle,
            lineWidth,
            position.lat,
            position.lon,
            arcRadiusMm,
            sector1Value + 180.0,
            sector2Value + 180.0
        );
    }
}

void initCSPs (Environment& environment) {
    environment.dai.addCSP ("LIGHTS06", lights06);
    environment.dai.addCSP ("DEPCNT03", depcnt03);
    environment.dai.addCSP ("DEPARE03", depare03);
    environment.dai.addCSP ("SOUNDG03", soundg03);
    environment.dai.addCSP ("WRECKS05", wrecks05);
    environment.dai.addCSP ("RESARE04", resare04);
    environment.dai.addCSP ("RESTRN01", restrn01);
    environment.dai.addCSP ("SLCONS04", slcons04);
    environment.dai.addCSP ("QUAPOS01", quapos01);
    environment.dai.addCSP ("OBSTRN07", obstrn07);
}
