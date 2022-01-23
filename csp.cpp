#include <string>
#include <vector>
#include <map>
#include <tuple>
#include "s57defs.h"
#include "classes.h"
#include "data.h"
#include "settings.h"
#include "drawers.h"

void lights06 (LookupTableItem *item, FeatureObject *object, Dai& dai, Nodes& nodes, Edges& edges, Features& features, int zoom, DrawQueue& drawQueue) {
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

        double legLengthMm = settings.fullSectorLength ? nominalRange * 1852000.0 / zoomToScale (zoom) : 25.0;

        size_t penIndex = dai.getPenIndex ("LS(DASH,1,CHBLK)");

        item->lines.emplace_back (penIndex, LineDrawMode::USING_BRG_AND_RNG, position.lat, position.lon, sector1Value + 180.0, legLengthMm);
        item->lines.emplace_back (penIndex, LineDrawMode::USING_BRG_AND_RNG, position.lat, position.lon, sector2Value + 180.0, legLengthMm);

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

        if (litvis && !litvis->noValue && (litvis->intValue == 3 || litvis->intValue == 7 || litvis->intValue == 8)) {
            arcPenName = "LS(DASH,1,CHBLK)";
        } else {
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
                } else if (colour->listIncludes (1)) {
                    arcColorName = "LITYW";
                }
            }
            arcPenName = "LS(SOLD,2," + arcColorName + ")";
        }
        arcPenIndex = dai.palette.checkPen (arcPenName.data (), dai.colorTable); //dai.getPenIndex (arcPenName.c_str ());
        item->lines.emplace_back (arcPenIndex, LineDrawMode::ARC, position.lat, position.lon, sector1Value, sector2Value, arcRadiusMm);
    }
}

void initCSPs (Dai& dai) {
    dai.addCSP ("LIGHTS06", lights06);
}
