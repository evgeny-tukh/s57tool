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

struct Ctx {
    HINSTANCE instance;
    HWND mainWnd, catalogCtl, recordTree, propsList, splashScreen;
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

    auto createControl = [&wnd, &ctx] (const char *className, const char *text, uint32_t style, bool visible, int x, int y, int width, int height, uint64_t id) {
        style |= WS_CHILD;

        if (visible) style |= WS_VISIBLE;
        
        return CreateWindow (className, text, style, x, y, width, height, wnd, (HMENU) id, ctx->instance, 0);
    };
    auto createPopup = [&wnd, &ctx] (const char *className, const char *text, uint32_t style, bool visible, int x, int y, int width, int height) {
        style |= WS_POPUP;

        if (visible) style |= WS_VISIBLE;
        
        return CreateWindow (className, text, style, x, y, width, height, wnd, 0, ctx->instance, 0);
    };

    SetWindowLongPtr (wnd, GWLP_USERDATA, (LONG_PTR) data);

    int halfWidth = client.right / 2;
    int halfHeight = client.bottom / 2;

    ctx->catalogCtl = createControl (WC_LISTVIEW, "", LVS_REPORT | WS_VSCROLL | WS_HSCROLL, true, 0, 0, client.right, halfHeight, IDC_CATALOG);
    ctx->recordTree = createControl (WC_TREEVIEW, "", TVS_HASBUTTONS | TVS_HASLINES | TVS_LINESATROOT | WS_VSCROLL, true, 0, halfHeight, halfWidth, halfHeight, IDC_RECORDS);
    ctx->propsList = createControl (WC_LISTVIEW, "", LVS_REPORT | WS_VSCROLL | WS_HSCROLL, true, halfWidth, halfHeight, halfWidth, halfHeight, IDC_CATALOG);

    SendMessage (ctx->catalogCtl, LVM_SETEXTENDEDLISTVIEWSTYLE, LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);
    SendMessage (ctx->propsList, LVM_SETEXTENDEDLISTVIEWSTYLE, LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);

    auto addColumn = [ctx] (HWND ctl, int columnIndex, char *caption, int width) {
        LVCOLUMN column;

        memset (& column, 0, sizeof (column));

        column.cx = width;
        column.fmt = LVCFMT_CENTER;
        column.pszText = caption;
        column.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT;

        SendMessage (ctl, LVM_INSERTCOLUMN, columnIndex, (LPARAM) & column);
    };

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
    std::map<uint32_t, FeatureDesc> featureObjects;
    DatasetParams datasetParams;

    loadParseS57File (path, records);
    extractDatasetParameters (records, datasetParams);
    extractFeatureObjects (records, featureObjects);
    
    SendMessage (ctx->recordTree, TVM_DELETEITEM, (WPARAM) TVI_ROOT, 0);
    SendMessage (ctx->propsList, LVM_DELETEALLITEMS, 0, 0);

    auto addParam = [ctx] (int index, char *name, char *value) {
        LVITEM item;

        memset (& item, 0, sizeof (item));

        item.pszText = name;
        item.iItem = index;
        item.mask = LVIF_TEXT;
        
        SendMessage (ctx->propsList, LVM_INSERTITEM, 0, (LPARAM) & item);

        item.pszText = value;
        item.iSubItem = 1;
        item.mask = LVIF_TEXT;
        
        SendMessage (ctx->propsList, LVM_SETITEMTEXT, index, (LPARAM) & item);
    };

    addParam (0, "Comment", datasetParams.comment.has_value () ? datasetParams.comment.value ().c_str () : "N/A");
    addParam (1, "Coord multiplier", datasetParams.coordMultiplier.has_value () ? std::to_string (datasetParams.coordMultiplier.value ()).c_str () : "N/A");
    addParam (2, "Coord units", datasetParams.coordMultiplier.has_value () ? coordUnitName (datasetParams.coordUnit.value ()) : "N/A");
    addParam (3, "Compilation scale", datasetParams.compilationScale.has_value () ? std::to_string (datasetParams.compilationScale.value ()).c_str () : "N/A");
    addParam (4, "Depth measurement", datasetParams.depthMeasurement.has_value () ? depthMeasurementName (datasetParams.depthMeasurement.value ()) : "N/A");
    addParam (5, "Pos measurement", datasetParams.posMeasurement.has_value () ? posMeasurementName (datasetParams.posMeasurement.value ()) : "N/A");
    addParam (6, "Sounding datum", datasetParams.soundingDatum.has_value () ? std::to_string (datasetParams.soundingDatum.value ()).c_str () : "N/A");
    addParam (7, "Sounding multiplier", datasetParams.soundingMultiplier.has_value () ? std::to_string (datasetParams.soundingMultiplier.value ()).c_str () : "N/A");
    addParam (8, "Horizontal datum", datasetParams.horDatum.has_value () ? std::to_string (datasetParams.horDatum.value ()).c_str () : "N/A");
    addParam (9, "Vertical datum", datasetParams.verDatum.has_value () ? std::to_string (datasetParams.verDatum.value ()).c_str () : "N/A");

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
if(subField.first.compare("*NAME")==0){
int iii=0;
++iii;
--iii;
}
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

    MoveWindow (ctx->catalogCtl, 0, 0, width, halfHeight, true);
    MoveWindow (ctx->recordTree, 0, halfHeight, halfWidth, halfHeight, true);
    MoveWindow (ctx->propsList, halfWidth, halfHeight, halfWidth, halfHeight, true);
}

void onNotify (HWND wnd, NMHDR *hdr) {
    Ctx *ctx = (Ctx *) GetWindowLongPtr (wnd, GWLP_USERDATA);

    switch (hdr->code) {
        case NM_DBLCLK: {
            NMITEMACTIVATE *item = (NMITEMACTIVATE *) hdr;
            
            if (item->iItem >= 0) {
                openFileByIndex (ctx, item->iItem);
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