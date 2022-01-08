#include <Windows.h>
#include <CommCtrl.h>
#include <Shlwapi.h>
#include <cstdint>
#include <string>
#include <vector>
#include <thread>
#include <map>
#include "resource.h"
#include "parser.h"

const char *MAIN_CLASS = "S57ToolMainWnd";
const char *SPLASH_CLASS = "S57ToolSplashScreen";
const int COL_FILENAME = 0;
const int COL_VOLUME = 1;
const int COL_IMPL = 2;
const int COL_NORTH = 3;
const int COL_SOUTH = 4;
const int COL_WEST = 5;
const int COL_EAST = 6;
const int COL_CRC = 7;
const int COL_PROPNAME = 0;
const int COL_PROPVALUE = 1;

const int COL_RCID = 0;
const int COL_CLASS = 1;
const int COL_GEOMETRY = 2;

const int COL_NODE_ID = 0;
const int COL_NODE_TYPE = 1;
const int COL_NODE_LAT = 2;
const int COL_NODE_LON = 3;
const int COL_NODE_DEPTH = 4;


enum TABS {
    RECORDS = 0,
    PROPERTIES,
    OBJECTS,
    NODES,
    EDGES,
};

struct Ctx {
    HINSTANCE instance;
    HWND mainWnd, catalogCtl, recordTree, propsList, splashScreen, tabCtl, objectTree, nodeList, edgeTree;
    HMENU mainMenu;
    bool keepRunning, loaded;
    std::vector<CatalogItem> catalog;
    std::string basePath;
    std::string splashText;
    ObjectDictionary objectDictionary;
    AttrDictionary attrDictionary;
    Dai dai;

    Ctx (HINSTANCE _instance, HMENU _menu): instance (_instance), mainMenu (_menu), keepRunning (true), loaded (false) {}

    virtual ~Ctx () {
        DestroyMenu (mainMenu);
    }
};

bool queryExit (HWND wnd) {
    return MessageBox (wnd, "Do you want to quit the application?", "Confirmation", MB_YESNO | MB_ICONQUESTION) == IDYES;
}

void initSplashScreen (HWND wnd, void *data) {
    Ctx *ctx = (Ctx *) data;
    RECT client;

    GetClientRect (wnd, & client);
    SetWindowLongPtr (wnd, GWLP_USERDATA, (LONG_PTR) data);
    SetTimer (wnd, 100, 100, 0);
    CreateWindow ("STATIC", "", SS_ETCHEDFRAME | WS_VISIBLE | WS_CHILD, 0, 0, client.right - 1, client.bottom - 1, wnd, (HMENU) IDC_STATIC, ctx->instance, 0);
    CreateWindow ("STATIC", "", SS_LEFT | WS_VISIBLE | WS_CHILD, 4, 4, client.right - 9, client.bottom - 9, wnd, (HMENU) IDC_LOADINFO, ctx->instance, 0);
}

void initWindow (HWND wnd, void *data) {
    Ctx *ctx = (Ctx *) data;
    RECT client;

    GetClientRect (wnd, & client);

    auto createControl = [&ctx] (
        HWND parent,
        const char *className,
        const char *text,
        uint32_t style,
        bool visible,
        uint64_t id,
        int x,
        int y,
        int width,
        int height
    ) {
        RECT client;

        GetClientRect (parent, & client);

        style |= WS_CHILD;

        if (visible) style |= WS_VISIBLE;
        
        return CreateWindow (className, text, style, x, y, width, height, parent, (HMENU) id, ctx->instance, 0);
    };
    auto createPopup = [&wnd, &ctx] (const char *className, const char *text, uint32_t style, bool visible, int x, int y, int width, int height) {
        style |= WS_POPUP;

        if (visible) style |= WS_VISIBLE;
        
        return CreateWindow (className, text, style, x, y, width, height, wnd, 0, ctx->instance, 0);
    };

    SetWindowLongPtr (wnd, GWLP_USERDATA, (LONG_PTR) data);

    int halfWidth = client.right / 2;
    int halfHeight = client.bottom / 2;

    ctx->catalogCtl = createControl (wnd, WC_LISTVIEW, "", LVS_REPORT | WS_VSCROLL | WS_HSCROLL, true, IDC_CATALOG, 0, 0, client.right + 1, halfHeight);
    ctx->tabCtl = createControl (wnd, WC_TABCONTROL, "", TCS_TABS | TCS_MULTILINE, true, IDC_TAB, 0, halfHeight, client.right + 1, halfHeight);

    auto addTab = [ctx] (int index, char *text) {
        TCITEM item;

        memset (& item, 0, sizeof (item));

        item.mask = TCIF_TEXT;
        item.pszText = text;

        SendMessage (ctx->tabCtl, TCM_INSERTITEM, index, (LPARAM) & item);
    };

    auto addColumn = [ctx] (HWND ctl, int columnIndex, char *caption, int width) {
        LVCOLUMN column;

        memset (& column, 0, sizeof (column));

        column.cx = width;
        column.fmt = LVCFMT_CENTER;
        column.pszText = caption;
        column.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT;

        SendMessage (ctl, LVM_INSERTCOLUMN, columnIndex, (LPARAM) & column);
    };

    addTab (TABS::RECORDS, "Records");
    addTab (TABS::PROPERTIES, "Properties");
    addTab (TABS::OBJECTS, "Objects");
    addTab (TABS::NODES, "Nodes");
    addTab (TABS::EDGES, "Edges");

    GetClientRect (ctx->tabCtl, & client);
    
    auto createTabChildControl = [&ctx, &client, &createControl] (const char *wndClass, uint32_t style, uint32_t id, bool visible) {
        return createControl (ctx->tabCtl, wndClass, "", style, visible, id, 3, 30, client.right - 6, client.bottom - 33);
    };

    ctx->recordTree = createTabChildControl (WC_TREEVIEW, TVS_HASBUTTONS | TVS_HASLINES | TVS_LINESATROOT | WS_VSCROLL, IDC_RECORDS, true);
    ctx->propsList = createTabChildControl (WC_LISTVIEW, LVS_REPORT | WS_VSCROLL | WS_HSCROLL, IDC_CATALOG, false);
    ctx->objectTree = createTabChildControl (WC_TREEVIEW, TVS_HASBUTTONS | TVS_HASLINES | TVS_LINESATROOT | WS_VSCROLL, IDC_OBJECTS, false);
    ctx->nodeList = createTabChildControl (WC_LISTVIEW, LVS_REPORT | WS_VSCROLL | WS_HSCROLL, IDC_NODES, false);
    ctx->edgeTree = createTabChildControl (WC_TREEVIEW, TVS_HASBUTTONS | TVS_HASLINES | TVS_LINESATROOT | WS_VSCROLL, IDC_EDGES, false);

    SendMessage (ctx->catalogCtl, LVM_SETEXTENDEDLISTVIEWSTYLE, LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);
    SendMessage (ctx->propsList, LVM_SETEXTENDEDLISTVIEWSTYLE, LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);
    SendMessage (ctx->nodeList, LVM_SETEXTENDEDLISTVIEWSTYLE, LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);

    addColumn (ctx->catalogCtl, COL_FILENAME, "Filename", 170);
    addColumn (ctx->catalogCtl, COL_VOLUME, "Volume", 60);
    addColumn (ctx->catalogCtl, COL_IMPL, "Type", 40);
    addColumn (ctx->catalogCtl, COL_NORTH, "North", 100);
    addColumn (ctx->catalogCtl, COL_SOUTH, "South", 100);
    addColumn (ctx->catalogCtl, COL_WEST, "West", 100);
    addColumn (ctx->catalogCtl, COL_EAST, "East", 100);
    addColumn (ctx->catalogCtl, COL_CRC, "CRC", 100);

    addColumn (ctx->propsList, COL_PROPNAME, "Name", 200);
    addColumn (ctx->propsList, COL_PROPVALUE, "Value", 170);

    addColumn (ctx->nodeList, COL_NODE_ID, "ID", 60);
    addColumn (ctx->nodeList, COL_NODE_TYPE, "Type", 150);
    addColumn (ctx->nodeList, COL_NODE_LAT, "Latitude", 100);
    addColumn (ctx->nodeList, COL_NODE_LON, "Longitude", 100);
    addColumn (ctx->nodeList, COL_NODE_DEPTH, "Depth", 100);
}

void loadCatalog (Ctx *ctx) {
    OPENFILENAME ofn;
    char path [MAX_PATH];

    static char *CAT_FILTER = "S57 Catalogs (catalog.*)\0catalog.*\0All files\0*.*\0\0";

    memset (path, 0, sizeof (path));
    memset (& ofn, 0, sizeof (ofn));

    ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
    ofn.hInstance = ctx->instance;
    ofn.lpstrFile = path;
    ofn.lpstrTitle = "Open Catalog";
    ofn.lStructSize = sizeof (ofn);
    ofn.nMaxFile = sizeof (path);
    ofn.hwndOwner = ctx->mainWnd;
    ofn.lpstrFilter = CAT_FILTER;

    if (GetOpenFileName (& ofn)) {
        std::string msg (path);
        
        SendMessage (ctx->catalogCtl, LVM_DELETEALLITEMS, 0, 0);

        if (parseCatalog (path, ctx->catalog)) {
            char basePath [MAX_PATH];

            strcpy (basePath, path);
            PathRemoveFileSpec (basePath);
            ctx->basePath = basePath;
            
            std::string msg (path);

            for (size_t i = 0; i < ctx->catalog.size (); ++ i) {
                LVITEMA item;
                std::string northern = ctx->catalog [i].northern.has_value () ? formatLat (ctx->catalog [i].northern.value ()).c_str () : "N/A";
                std::string southern = ctx->catalog [i].southern.has_value () ? formatLat (ctx->catalog [i].southern.value ()).c_str () : "N/A";
                std::string western = ctx->catalog [i].western.has_value () ? formatLon (ctx->catalog [i].western.value ()).c_str () : "N/A";
                std::string eastern = ctx->catalog [i].eastern.has_value () ? formatLon (ctx->catalog [i].eastern.value ()).c_str () : "N/A";

                memset (& item, 0, sizeof (item));

                item.iItem = i;
                item.mask = LVIF_TEXT | LVIF_PARAM;
                item.pszText = (char *) ctx->catalog [i].fileName.c_str ();
                item.lParam = (LPARAM) & ctx->catalog [i];

                SendMessage (ctx->catalogCtl, LVM_INSERTITEMA, 0, (LPARAM) & item);

                item.iSubItem = COL_VOLUME;
                item.pszText = (char *) ctx->catalog [i].volume.c_str ();
                SendMessage (ctx->catalogCtl, LVM_SETITEMTEXT, i, (LPARAM) & item);

                item.iSubItem = COL_IMPL;
                item.pszText = ctx->catalog [i].binary ? "bin" : "asc";
                SendMessage (ctx->catalogCtl, LVM_SETITEMTEXT, i, (LPARAM) & item);

                item.iSubItem = COL_CRC;
                item.pszText = (char *) (ctx->catalog [i].crc.has_value () ? ctx->catalog [i].crc.value ().c_str () : "N/A");
                SendMessage (ctx->catalogCtl, LVM_SETITEMTEXT, i, (LPARAM) & item);

                item.iSubItem = COL_NORTH;
                item.pszText = (char *) northern.c_str ();
                SendMessage (ctx->catalogCtl, LVM_SETITEMTEXT, i, (LPARAM) & item);

                item.iSubItem = COL_SOUTH;
                item.pszText = (char *) southern.c_str ();
                SendMessage (ctx->catalogCtl, LVM_SETITEMTEXT, i, (LPARAM) & item);

                item.iSubItem = COL_WEST;
                item.pszText = (char *) western.c_str ();
                SendMessage (ctx->catalogCtl, LVM_SETITEMTEXT, i, (LPARAM) & item);

                item.iSubItem = COL_EAST;
                item.pszText = (char *) eastern.c_str ();
                SendMessage (ctx->catalogCtl, LVM_SETITEMTEXT, i, (LPARAM) & item);
            }
        } else {
            MessageBox (ctx->mainWnd, msg.insert (0, " has been parsed").c_str (), "Problem", MB_ICONEXCLAMATION);
        }
    }
}

const char *coordUnitName (COUN unit) {
    switch (unit) {
        case COUN::EastingNorthing: return "Easting/Northing";
        case COUN::LatLon: return "Lat/Lon";
        case COUN::UnitsOnChart: return "Units on chart";
        default: return "Unknown";
    }
}

const char *depthMeasurementName (DUNI unit) {
    switch (unit) {
        case DUNI::DepthFathomsFeet: return "Fathoms/Feet";
        case DUNI::DepthFathomsFractions: return "Fathoms/Fractions";
        case DUNI::DepthFeet: return "Feet";
        case DUNI::DepthMeters: return "Meters";
        default: return "Unknown";
    }
}

const char *posMeasurementName (PUNI unit) {
    switch (unit) {
        case PUNI::PosCables: return "Cables";
        case PUNI::PosDegreesOfArc: return "Degrees of arc";
        case PUNI::PosFeet: return "Feet";
        case PUNI::PosMeters: return "Meters";
        case PUNI::PosMillimeters: return "Millimeters";
        default: return "Unknown";
    }
}

void openFile (Ctx *ctx, CatalogItem *item) {
    char path [MAX_PATH];

    PathCombine (path, ctx->basePath.c_str (), item->fileName.c_str ());

    std::vector<std::vector<FieldInstance>> records;
    std::vector<FeatureDesc> objects;
    Nodes points;
    Edges edges;
    Features features;
    DatasetParams datasetParams;

    loadParseS57File (path, records);
    extractDatasetParameters (records, datasetParams);
    //extractFeatureObjects (records, objects);
    extractNodes (records, points, datasetParams);
    extractEdges (records, edges, points, datasetParams);
    extractFeatureObjects (records, features);
    deformatAttrValues (ctx->attrDictionary, objects);
    
    SendMessage (ctx->recordTree, TVM_DELETEITEM, (WPARAM) TVI_ROOT, 0);
    SendMessage (ctx->propsList, LVM_DELETEALLITEMS, 0, 0);
    SendMessage (ctx->nodeList, LVM_DELETEALLITEMS, 0, 0);

    auto addListItem = [] (HWND list, char *text, LVITEM& item) {
        memset (& item, 0, sizeof (item));

        item.pszText = text;
        item.iItem = 0x7FFFFFFF;
        item.mask = LVIF_TEXT;
        
        return (int) SendMessage (list, LVM_INSERTITEM, 0, (LPARAM) & item);
    };

    auto setListItemText = [] (HWND list, int index, int column, char *text, LVITEM& item) {
        item.pszText = text;
        item.iItem = index;
        item.iSubItem = column;
        item.mask = LVIF_TEXT;
        
        SendMessage (list, LVM_SETITEMTEXT, index, (LPARAM) & item);
    };

    auto addParam = [ctx, addListItem, setListItemText] (char *name, char *value) {
        LVITEM item;

        int itemIndex = addListItem (ctx->propsList, name, item);
        setListItemText (ctx->propsList, itemIndex, 1, value, item);
    };

    auto getNodeType = [] (uint8_t nodeFlags) {
        std::string result { (nodeFlags & NodeFlags::CONNECTED) ? "Connected" : "Isolated" };

        if (nodeFlags & NodeFlags::SOUNDING_ARRAY) result += "; Soundings";

        return result;
    };

    auto addNode = [ctx, getNodeType, addListItem, setListItemText] (size_t index, struct Nodes& nodes) {
        if (nodes [index].points.size () == 0) return;

        LVITEM item;
        auto& node = nodes [index];

        std::string id { std::to_string (node.id) };
        std::string lat { formatLat (node.points.front ().lat) };
        std::string lon { formatLon (node.points.front ().lon) };
        std::string type { getNodeType (node.flags) };
        std::string depth { (node.flags & NodeFlags::SOUNDING_ARRAY) ? std::to_string (node.points.front ().depth) : "" };

        int itemIndex = addListItem (ctx->nodeList, id.data (), item);
        setListItemText (ctx->nodeList, itemIndex, COL_NODE_TYPE, type.data (), item);
        setListItemText (ctx->nodeList, itemIndex, COL_NODE_LAT, lat.data (), item);
        setListItemText (ctx->nodeList, itemIndex, COL_NODE_LON, lon.data (), item);
        setListItemText (ctx->nodeList, itemIndex, COL_NODE_DEPTH, depth.data (), item);

        for (size_t i = 1; i < node.points.size (); ++ i) {
            std::string lat { formatLat (node.points [i].lat) };
            std::string lon { formatLon (node.points [i].lon) };
            std::string depth { (node.flags & NodeFlags::SOUNDING_ARRAY) ? std::to_string (node.points [i].depth) : "" };

            itemIndex = addListItem (ctx->nodeList, "", item);
            setListItemText (ctx->nodeList, itemIndex, COL_NODE_TYPE, "", item);
            setListItemText (ctx->nodeList, itemIndex, COL_NODE_LAT, lat.data (), item);
            setListItemText (ctx->nodeList, itemIndex, COL_NODE_LON, lon.data (), item);
            setListItemText (ctx->nodeList, itemIndex, COL_NODE_DEPTH, depth.data (), item);
        }
    };

    addParam ("Comment", datasetParams.comment.has_value () ? datasetParams.comment.value ().c_str () : "N/A");
    addParam ("Coord multiplier", datasetParams.coordMultiplier.has_value () ? std::to_string (datasetParams.coordMultiplier.value ()).c_str () : "N/A");
    addParam ("Coord units", datasetParams.coordMultiplier.has_value () ? coordUnitName (datasetParams.coordUnit.value ()) : "N/A");
    addParam ("Compilation scale", datasetParams.compilationScale.has_value () ? std::to_string (datasetParams.compilationScale.value ()).c_str () : "N/A");
    addParam ("Depth measurement", datasetParams.depthMeasurement.has_value () ? depthMeasurementName (datasetParams.depthMeasurement.value ()) : "N/A");
    addParam ("Pos measurement", datasetParams.posMeasurement.has_value () ? posMeasurementName (datasetParams.posMeasurement.value ()) : "N/A");
    addParam ("Sounding datum", datasetParams.soundingDatum.has_value () ? std::to_string (datasetParams.soundingDatum.value ()).c_str () : "N/A");
    addParam ("Sounding multiplier", datasetParams.soundingMultiplier.has_value () ? std::to_string (datasetParams.soundingMultiplier.value ()).c_str () : "N/A");
    addParam ("Horizontal datum", datasetParams.horDatum.has_value () ? std::to_string (datasetParams.horDatum.value ()).c_str () : "N/A");
    addParam ("Vertical datum", datasetParams.verDatum.has_value () ? std::to_string (datasetParams.verDatum.value ()).c_str () : "N/A");

    for (size_t i = 0; i < points.size (); ++ i) {
        addNode (i, points);
    }

    TV_INSERTSTRUCTA data;

    memset (& data, 0, sizeof (data));

    data.hParent = TVI_ROOT;
    data.hInsertAfter = TVI_LAST;
    data.item.mask = TVIF_TEXT;
    data.item.pszText = "Points";

    HTREEITEM objectGroupItem [5];

    objectGroupItem [0] = (HTREEITEM) SendMessage (ctx->objectTree, TVM_INSERTITEM, 0, (LPARAM) & data);

    data.item.pszText = "Lines";

    objectGroupItem [1] = (HTREEITEM) SendMessage (ctx->objectTree, TVM_INSERTITEM, 0, (LPARAM) & data);

    data.item.pszText = "Areas";

    objectGroupItem [2] = (HTREEITEM) SendMessage (ctx->objectTree, TVM_INSERTITEM, 0, (LPARAM) & data);

    data.item.pszText = "3D Points";

    objectGroupItem [3] = (HTREEITEM) SendMessage (ctx->objectTree, TVM_INSERTITEM, 0, (LPARAM) & data);

    data.item.pszText = "Non-spatial objects";

    objectGroupItem [4] = (HTREEITEM) SendMessage (ctx->objectTree, TVM_INSERTITEM, 0, (LPARAM) & data);

    for (auto& object: objects) {
        std::string featureID ("Feature id: " + std::to_string (object.featureID));
        std::string id ("ID: " + std::to_string (object.id));
        std::string classInfo (std::to_string (object.classCode));
        static char *geometries [] { "Point", "Line", "Area", "3D points" };

        auto objectDesc = ctx->objectDictionary.findByCode (object.classCode);

        classInfo +=  " [";
        classInfo += objectDesc ? objectDesc->name : "N/A";
        classInfo += "]";

        data.hParent = (object.geometry > 0 && object.geometry < 5) ? objectGroupItem [object.geometry-1] : objectGroupItem [4];
        data.hInsertAfter = TVI_LAST;
        data.item.mask = TVIF_TEXT;
        data.item.pszText = (char *) featureID.c_str ();

        HTREEITEM objectItem = (HTREEITEM) SendMessage (ctx->objectTree, TVM_INSERTITEM, 0, (LPARAM) & data);

        data.hParent = objectItem;
        data.hInsertAfter = TVI_LAST;
        data.item.mask = TVIF_TEXT;
        data.item.pszText = (char *) id.c_str ();

        SendMessage (ctx->objectTree, TVM_INSERTITEM, 0, (LPARAM) & data);

        data.item.pszText = (char *) classInfo.c_str ();

        SendMessage (ctx->objectTree, TVM_INSERTITEM, 0, (LPARAM) & data);

        data.item.pszText = "Attributes";

        HTREEITEM attrsItem = (HTREEITEM) SendMessage (ctx->objectTree, TVM_INSERTITEM, 0, (LPARAM) & data);

        for (auto& attr: object.attributes) {
            std::string attrInfo (std::to_string (attr.classCode));

            AttrDesc *attrDesc = (AttrDesc *) ctx->attrDictionary.findByCode (attr.classCode);

            attrInfo +=  " [";
            attrInfo += attrDesc ? attrDesc->name : "N/A";
            attrInfo += "]";

            data.hParent = attrsItem;
            data.item.pszText = (char *) attrInfo.c_str ();

            HTREEITEM attrItem = (HTREEITEM) SendMessage (ctx->objectTree, TVM_INSERTITEM, 0, (LPARAM) & data);

            std::string attrType { "Type: " };

            switch (attrDesc->domain) {
                case 'E': attrType += "Enumeration"; break;
                case 'I': attrType += "Integer"; break;
                case 'F': attrType += "Float"; break;
                case 'L': attrType += "List"; break;
                case 'A': attrType += "ANSI string"; break;
                case 'S': attrType += "String"; break;
                default: attrType += "Unknown";
            }

            data.hParent = attrItem;
            data.item.pszText = (char *) attrType.c_str ();

            SendMessage (ctx->objectTree, TVM_INSERTITEM, 0, (LPARAM) & data);

            std::string attrValue { "Value: " };

            if (attr.noValue) {
                attrValue += "<No value>";
            } else {
                switch (attrDesc->domain) {
                    case 'E': {
                        attrValue += attrDesc->listValue (attr.intValue); break;
                    }
                    case 'I': {
                        attrValue += std::to_string (attr.intValue); break;
                    }
                    case 'F': {
                        attrValue += std::to_string (attr.floatValue); break;
                    }
                    case 'L': {
                        for (uint8_t value: attr.listValue) {
                            attrValue += attrDesc->listValue (value);
                            attrValue += ";";
                        }
                        break;
                    }
                    case 'A': {
                        attrValue += attr.strValue; break;
                    }
                    case 'S': {
                        attrValue += attr.strValue; break;
                    }
                    default: {
                        attrType += "Unknown";
                    }
                }
            }

            data.item.pszText = (char *) attrValue.c_str ();

            SendMessage (ctx->objectTree, TVM_INSERTITEM, 0, (LPARAM) & data);
        }
    }

    for (auto& edge: edges) {
        TV_INSERTSTRUCT data;
        char buffer [200];

        memset (& data, 0, sizeof (data));

        std::string edgeID = "ID " + std::to_string (edge.id);

        data.hParent = TVI_ROOT;
        data.hInsertAfter = TVI_LAST;
        data.item.mask = TVIF_TEXT;
        data.item.pszText = edgeID.data ();

        HTREEITEM edgeItem = (HTREEITEM) SendMessage (ctx->edgeTree, TVM_INSERTITEM, 0, (LPARAM) & data);

        auto addEdgeNodeItem = [&data, ctx, edgeItem] (EdgeNode& node, char *label) {
            data.hParent = edgeItem;
            data.item.pszText = label;

            HTREEITEM nodeItem = (HTREEITEM) SendMessage (ctx->edgeTree, TVM_INSERTITEM, 0, (LPARAM) & data);

            std::string lat = "Lat " + formatLat (node.lat);
            std::string lon = "Lon " + formatLon (node.lon);
            std::string mode { "Mode: "};

            if (node.hidden) mode += "<Masked>";
            if (node.hole) mode += "<Hole>";

            data.hParent = nodeItem;
            data.item.pszText = lat.data ();

            SendMessage (ctx->edgeTree, TVM_INSERTITEM, 0, (LPARAM) & data);

            data.item.pszText = lon.data ();

            SendMessage (ctx->edgeTree, TVM_INSERTITEM, 0, (LPARAM) & data);

            data.item.pszText = mode.data ();

            SendMessage (ctx->edgeTree, TVM_INSERTITEM, 0, (LPARAM) & data);
        };

        addEdgeNodeItem (edge.begin, "Begin");

        if (edge.internalNodes.size () > 0) {
            size_t count = 0;

            std::string label = std::to_string (edge.internalNodes.size ()) + " internal";
            data.hParent = edgeItem;
            data.item.pszText = label.data ();

            HTREEITEM internalNodesItem = (HTREEITEM) SendMessage (ctx->edgeTree, TVM_INSERTITEM, 0, (LPARAM) & data);

            for (auto& node: edge.internalNodes) {
                label = std::to_string (count++);
                
                data.hParent = internalNodesItem;
                data.item.pszText = label.data ();

                HTREEITEM nodeItem = (HTREEITEM) SendMessage (ctx->edgeTree, TVM_INSERTITEM, 0, (LPARAM) & data);

                std::string lat = "Lat " + formatLat (node.lat);
                std::string lon = "Lon " + formatLon (node.lon);

                data.hParent = nodeItem;
                data.item.pszText = lat.data ();

                SendMessage (ctx->edgeTree, TVM_INSERTITEM, 0, (LPARAM) & data);

                data.item.pszText = lon.data ();

                SendMessage (ctx->edgeTree, TVM_INSERTITEM, 0, (LPARAM) & data);
            }
        }

        addEdgeNodeItem (edge.end, "End");
    }

    for (auto& rec: records) {
        TV_INSERTSTRUCT data;
        char rcidText [50];

        auto rcidField = rec [0].instanceValues.front ().begin ();

        if (rcidField->second.intValue.has_value ()) {
            sprintf (rcidText, "RCID %d", rcidField->second.intValue.value ());
        } else {
            strcpy (rcidText, "N/A");
        }
        
        memset (& data, 0, sizeof (data));

        data.hParent = TVI_ROOT;
        data.hInsertAfter = TVI_LAST;
        data.item.mask = TVIF_TEXT;
        data.item.pszText = rcidText;

        HTREEITEM recItem = (HTREEITEM) SendMessage (ctx->recordTree, TVM_INSERTITEM, 0, (LPARAM) & data);

        for (auto& field: rec) {
            memset (& data, 0, sizeof (data));
            char label [200];

            sprintf (label, "[%s] %s", field.tag.c_str (), field.name.c_str ());

            data.hParent = recItem;
            data.hInsertAfter = TVI_LAST;
            data.item.mask = TVIF_TEXT;
            data.item.pszText = label;

            HTREEITEM fieldItem = (HTREEITEM) SendMessage (ctx->recordTree, TVM_INSERTITEM, 0, (LPARAM) & data);

            if (field.instanceValues.size () == 1) {
                for (auto& subField: field.instanceValues.front ()) {
                    memset (& data, 0, sizeof (data));

                    switch (subField.second.type) {
                        case 'B': sprintf (label, "%s: %s", subField.first.c_str (), subField.second.binaryValue.size () > 0 ? subField.second.getBinaryValueString ().c_str () : "<no value>"); break;
                        case 'A': sprintf (label, "%s: %s", subField.first.c_str (), subField.second.stringValue.has_value () ? subField.second.stringValue.value ().c_str () : "<no value>"); break;
                        case 'b': case 'I': sprintf (label, "%s: %s", subField.first.c_str (), subField.second.intValue.has_value () ? std::to_string (subField.second.intValue.value ()).c_str () : "<no value>"); break;
                        case 'R': sprintf (label, "%s: %s", subField.first.c_str (), subField.second.floatValue.has_value () ? std::to_string (subField.second.floatValue.value ()).c_str () : "<no value>"); break;
                        default: sprintf (label, "%s: ?", subField.first.c_str ());
                    }

                    data.hParent = fieldItem;
                    data.hInsertAfter = TVI_LAST;
                    data.item.mask = TVIF_TEXT;
                    data.item.pszText = label;

                    HTREEITEM subFieldItem = (HTREEITEM) SendMessage (ctx->recordTree, TVM_INSERTITEM, 0, (LPARAM) & data);
                }
            } else {
                for (size_t valueIndex = 0; valueIndex < field.instanceValues.size (); ++ valueIndex) {
                    data.hParent = fieldItem;
                    data.hInsertAfter = TVI_LAST;
                    data.item.mask = TVIF_TEXT;
                    data.item.pszText = label;
                    
                    strcpy (label, std::to_string (valueIndex).c_str ());

                    HTREEITEM valueItem = (HTREEITEM) SendMessage (ctx->recordTree, TVM_INSERTITEM, 0, (LPARAM) & data);

                    for (auto& subField: field.instanceValues [valueIndex]) {
                        memset (& data, 0, sizeof (data));

                        switch (subField.second.type) {
                            case 'B': sprintf (label, "%s: %s", subField.first.c_str (), subField.second.binaryValue.size () > 0 ? subField.second.getBinaryValueString ().c_str () : "<no value>"); break;
                            case 'A': sprintf (label, "%s: %s", subField.first.c_str (), subField.second.stringValue.has_value () ? subField.second.stringValue.value ().c_str () : "<no value>"); break;
                            case 'b': case 'I': sprintf (label, "%s: %s", subField.first.c_str (), subField.second.intValue.has_value () ? std::to_string (subField.second.intValue.value ()).c_str () : "<no value>"); break;
                            case 'R': sprintf (label, "%s: %s", subField.first.c_str (), subField.second.floatValue.has_value () ? std::to_string (subField.second.floatValue.value ()).c_str () : "<no value>"); break;
                            default: sprintf (label, "%s: ?", subField.first.c_str ());
                        }

                        data.hParent = valueItem;
                        data.hInsertAfter = TVI_LAST;
                        data.item.mask = TVIF_TEXT;
                        data.item.pszText = (char *) label;

                        HTREEITEM subFieldItem = (HTREEITEM) SendMessage (ctx->recordTree, TVM_INSERTITEM, 0, (LPARAM) & data);
                    }
                }
            }
        }
    }
}

void openFileByIndex (Ctx *ctx, int itemIndex) {
    LVITEM item;

    memset (& item, 0, sizeof (item));

    item.iItem = itemIndex;
    item.mask = LVIF_PARAM;

    SendMessage (ctx->catalogCtl, LVM_GETITEM, 0, (LPARAM) & item);

    openFile (ctx, (CatalogItem *) item.lParam);
}

void doCommand (HWND wnd, uint16_t command, uint16_t notification) {
    Ctx *ctx = (Ctx *) GetWindowLongPtr (wnd, GWLP_USERDATA);

    switch (command) {
        case ID_OPEN_CATALOG: {
            loadCatalog (ctx); break;
        }
        case ID_EXIT: {
            if (queryExit (wnd)) DestroyWindow (wnd);
            break;
        }
        case ID_OPEN_FILE: {
            int selection = (int) SendMessage (ctx->catalogCtl, LVM_GETNEXTITEM, -1, LVNI_SELECTED);

            if (selection >= 0) openFileByIndex (ctx, selection);

            break;
        }
    }
}

void onSize (HWND wnd, int width, int height) {
    Ctx *ctx = (Ctx *) GetWindowLongPtr (wnd, GWLP_USERDATA);

    int halfWidth = width / 2;
    int halfHeight = height / 2;
    RECT client;
    
    MoveWindow (ctx->catalogCtl, 0, 0, width, halfHeight, true);
    MoveWindow (ctx->tabCtl, 0, halfHeight, width, halfHeight, true);
    GetClientRect (ctx->tabCtl, & client);

    MoveWindow (ctx->recordTree, 3, 30, client.right - 6, client.bottom - 33 , true);
    MoveWindow (ctx->propsList, 3, 30, client.right - 6, client.bottom - 33 , true);
    MoveWindow (ctx->nodeList, 3, 30, client.right - 6, client.bottom - 33 , true);
}

void onNotify (HWND wnd, NMHDR *hdr) {
    Ctx *ctx = (Ctx *) GetWindowLongPtr (wnd, GWLP_USERDATA);

    switch (hdr->code) {
        case TCN_SELCHANGE: {
            switch (SendMessage (ctx->tabCtl, TCM_GETCURSEL, 0, 0)) {
                case TABS::PROPERTIES: {
                    ShowWindow (ctx->propsList, SW_SHOW);
                    ShowWindow (ctx->recordTree, SW_HIDE);
                    ShowWindow (ctx->objectTree, SW_HIDE);
                    ShowWindow (ctx->nodeList, SW_HIDE);
                    ShowWindow (ctx->edgeTree, SW_HIDE);
                    break;
                }
                case TABS::RECORDS: {
                    ShowWindow (ctx->propsList, SW_HIDE);
                    ShowWindow (ctx->recordTree, SW_SHOW);
                    ShowWindow (ctx->objectTree, SW_HIDE);
                    ShowWindow (ctx->nodeList, SW_HIDE);
                    ShowWindow (ctx->edgeTree, SW_HIDE);
                    break;
                }
                case TABS::OBJECTS: {
                    ShowWindow (ctx->propsList, SW_HIDE);
                    ShowWindow (ctx->recordTree, SW_HIDE);
                    ShowWindow (ctx->objectTree, SW_SHOW);
                    ShowWindow (ctx->nodeList, SW_HIDE);
                    ShowWindow (ctx->edgeTree, SW_HIDE);
                    break;
                }
                case TABS::NODES: {
                    ShowWindow (ctx->propsList, SW_HIDE);
                    ShowWindow (ctx->recordTree, SW_HIDE);
                    ShowWindow (ctx->objectTree, SW_HIDE);
                    ShowWindow (ctx->nodeList, SW_SHOW);
                    ShowWindow (ctx->edgeTree, SW_HIDE);
                    break;
                }
                case TABS::EDGES: {
                    ShowWindow (ctx->propsList, SW_HIDE);
                    ShowWindow (ctx->recordTree, SW_HIDE);
                    ShowWindow (ctx->objectTree, SW_HIDE);
                    ShowWindow (ctx->nodeList, SW_HIDE);
                    ShowWindow (ctx->edgeTree, SW_SHOW);
                    break;
                }
            }
            break;
        }
        case NM_DBLCLK: {
            if (hdr->idFrom == IDC_CATALOG) {
                NMITEMACTIVATE *item = (NMITEMACTIVATE *) hdr;
                
                if (item->iItem >= 0) {
                    openFileByIndex (ctx, item->iItem);
                }
            }
            break;
        }
    }
}

LRESULT wndSplashScreenProc (HWND wnd, UINT msg, WPARAM param1, LPARAM param2) {
    LRESULT result = 0;
    Ctx *ctx = (Ctx *) GetWindowLongPtr (wnd, GWLP_USERDATA);

    switch (msg) {
        case WM_TIMER:
            if (ctx->loaded) {
                DestroyWindow (wnd);
            } else {
                SetDlgItemText (wnd, IDC_LOADINFO, ctx->splashText.c_str ());
            }
            break;
        case WM_CREATE:
            initSplashScreen (wnd, ((CREATESTRUCT *) param2)->lpCreateParams); break;
        default:
            result = DefWindowProc (wnd, msg, param1, param2);
    }
    
    return result;
}


LRESULT wndProc (HWND wnd, UINT msg, WPARAM param1, LPARAM param2) {
    LRESULT result = 0;

    switch (msg) {
        case WM_NOTIFY: {
            onNotify (wnd, (NMHDR *) param2); break;
        }
        case WM_COMMAND:
            doCommand (wnd, LOWORD (param1), HIWORD (param1)); break;
        case WM_SIZE:
            onSize (wnd, LOWORD (param2), HIWORD (param2)); break;
        case WM_CREATE:
            initWindow (wnd, ((CREATESTRUCT *) param2)->lpCreateParams); break;
        case WM_DESTROY:
            PostQuitMessage (0); break;
        case WM_SYSCOMMAND:
            if (param1 == SC_CLOSE && queryExit (wnd)) DestroyWindow (wnd);
        default:
            result = DefWindowProc (wnd, msg, param1, param2);
    }
    
    return result;
}

void registerClasses (Ctx& ctx) {
    WNDCLASS classInfo;

    memset (& classInfo, 0, sizeof (classInfo));

    classInfo.hbrBackground = (HBRUSH) GetStockObject (WHITE_BRUSH);
    classInfo.hCursor = (HCURSOR) LoadCursor (0, IDC_ARROW);
    classInfo.hIcon = (HICON) LoadIcon (0, IDI_APPLICATION);
    classInfo.hInstance = ctx.instance;
    classInfo.lpfnWndProc = wndProc;
    classInfo.lpszClassName = MAIN_CLASS;
    classInfo.style = CS_HREDRAW | CS_VREDRAW;

    RegisterClass (& classInfo);

    memset (& classInfo, 0, sizeof (classInfo));

    classInfo.hbrBackground = (HBRUSH) GetStockObject (GRAY_BRUSH);
    classInfo.hCursor = (HCURSOR) LoadCursor (0, IDC_ARROW);
    classInfo.hIcon = (HICON) LoadIcon (0, IDI_APPLICATION);
    classInfo.hInstance = ctx.instance;
    classInfo.lpfnWndProc = wndSplashScreenProc;
    classInfo.lpszClassName = SPLASH_CLASS;
    classInfo.style = CS_HREDRAW | CS_VREDRAW;

    RegisterClass (& classInfo);
}

void loadProc (Ctx *ctx) {
    char path [MAX_PATH];

    ctx->splashText += "\nLoading object classes...";
    GetModuleFileName (0, path, sizeof (path));
    PathRemoveFileSpec (path);
    PathAppend (path, "objclass.dic");
    loadObjectDictionary (path, ctx->objectDictionary);
    ctx->splashText += "\nLoading object attributes...";
    PathRemoveFileSpec (path);
    PathAppend (path, "attributes.dic");
    loadAttrDictionary (path, ctx->attrDictionary);
    ctx->splashText += "\nLoading DAI...";
    PathRemoveFileSpec (path);
    PathAppend (path, "PresLib_e4.0.3.dai");
    loadDai (path, ctx->dai);
    ctx->splashText += "\nDone.";
    ctx->loaded = true;

    ShowWindow (ctx->mainWnd, SW_SHOW);
    UpdateWindow (ctx->mainWnd);
    BringWindowToTop (ctx->mainWnd);
}

int WINAPI WinMain (HINSTANCE instance, HINSTANCE prevInstance, char *cmd, int showCmd) {
    Ctx ctx (instance, LoadMenu (instance, MAKEINTRESOURCE (IDR_MAINMENU)));
    INITCOMMONCONTROLSEX comCtlData;
    
    memset (& comCtlData, 0, sizeof (comCtlData));

    comCtlData.dwSize = sizeof (comCtlData);
    comCtlData.dwICC = ICC_LISTVIEW_CLASSES | ICC_STANDARD_CLASSES | ICC_TAB_CLASSES | ICC_TREEVIEW_CLASSES;
    
    InitCommonControlsEx (& comCtlData);
    registerClasses (ctx);

    ctx.splashScreen = CreateWindow (SPLASH_CLASS, "", WS_POPUP | WS_VISIBLE, 200, 200, 500, 200, 0, 0, instance, & ctx);
    ctx.mainWnd = CreateWindow (MAIN_CLASS, "S57 Tool", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 800, 500, 0, ctx.mainMenu, instance, & ctx);
    ctx.keepRunning = true;
    ctx.splashText = "Initializing...";

    ShowWindow (ctx.splashScreen, SW_SHOW);
    UpdateWindow (ctx.splashScreen);

    std::thread loader (loadProc, & ctx);

    loader.detach ();

    MSG msg;

    while (GetMessage (& msg, 0, 0, 0)) {
        TranslateMessage (& msg);
        DispatchMessage (& msg);
    }

}