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
            // RESCSP03 call here
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
            // 1. SAFCON02 (locValdco.value ())
            // 2. Draw Selected Symbols from 'SAFCON02'
        }
    }
}

void depcnt03 (LookupTableItem *item, FeatureObject *object, Environment& environment, Chart& chart, View& view, DrawQueue& drawQueue) {
    Settings& settings = environment.settings;
    Dai& dai = environment.dai;
    // TO BE DONE!!!!
    item->edgePenIndex = dai.getBasePenIndex ("DEPCN");
    item->edgePenStyle = PS_SOLID;
    item->edgePenWidth = 1;
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

bool udwhaz05 (FeatureObject *object, double depth, Chart& chart, Environment& environment) {
    return true;
}

bool quapnt02 (FeatureObject *object, Chart& chart, Environment& environment) {
    return true;
}

void wrecks05 (LookupTableItem *item, FeatureObject *object, Environment& environment, Chart& chart, View& view, DrawQueue& drawQueue) {
    auto valsou = object->getAttr (ATTRS::VALSOU);
    double depth;
    int viewingGroup;
    std::vector<std::string> symbols;

    if (valsou && !valsou->noValue) {
        depth = valsou->floatValue;
        viewingGroup = 34051;

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
    bool isolatedDanger = udwhaz05 (object, depth, chart, environment);
    bool lowAccuracy = quapnt02 (object, chart, environment);

    if (object->primitive == 1) {
        auto& pos = chart.nodes [object->nodeIndex].points.front ();
        if (isolatedDanger) {
            drawQueue.addSymbol (pos.lat, pos.lon, environment.dai.getSymbolIndex ("ISODGR01"), 0.0, environment.dai);
            if (lowAccuracy) {
                drawQueue.addSymbol (pos.lat, pos.lon, environment.dai.getSymbolIndex ("LOWACC01"), 0.0, environment.dai);
            }
        } else {
            // cont A
        }
    } else {
        // cont B
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
}
