#include <Windows.h>
#include <CommCtrl.h>
#include <Shlwapi.h>
#include <cstdint>
#include <string>
#include <vector>
#include "resource.h"
#include "parser.h"

const char *MAIN_CLASS = "S57ToolMainWnd";
const int COL_FILENAME = 0;
const int COL_VOLUME = 1;
const int COL_IMPL = 2;
const int COL_NORTH = 3;
const int COL_SOUTH = 4;
const int COL_WEST = 5;
const int COL_EAST = 6;
const int COL_CRC = 7;

struct Ctx {
    HINSTANCE instance;
    HWND mainWnd, catalogCtl;
    HMENU mainMenu;
    bool keepRunning;
    std::vector<CatalogItem> catalog;
    std::string basePath;

    Ctx (HINSTANCE _instance, HMENU _menu): instance (_instance), mainMenu (_menu), keepRunning (true) {}

    virtual ~Ctx () {
        DestroyMenu (mainMenu);
    }
};

bool queryExit (HWND wnd) {
    return MessageBox (wnd, "Do you want to quit the application?", "Confirmation", MB_YESNO | MB_ICONQUESTION) == IDYES;
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

    ctx->catalogCtl = createControl (WC_LISTVIEW, "", LVS_REPORT, true, 0, 9, client.right, client.bottom, IDC_CATALOG);
    SendMessage (ctx->catalogCtl, LVM_SETEXTENDEDLISTVIEWSTYLE, LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);

    auto addColumn = [ctx] (int columnIndex, char *caption, int width) {
        LVCOLUMN column;

        memset (& column, 0, sizeof (column));

        column.cx = width;
        column.fmt = LVCFMT_CENTER;
        column.pszText = caption;
        column.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT;

        SendMessage (ctx->catalogCtl, LVM_INSERTCOLUMN, columnIndex, (LPARAM) & column);
    };

    addColumn (COL_FILENAME, "Filename", 170);
    addColumn (COL_VOLUME, "Volume", 60);
    addColumn (COL_IMPL, "Type", 40);
    addColumn (COL_NORTH, "North", 100);
    addColumn (COL_SOUTH, "South", 100);
    addColumn (COL_WEST, "West", 100);
    addColumn (COL_EAST, "East", 100);
    addColumn (COL_CRC, "CRC", 100);
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

            MessageBox (ctx->mainWnd, msg.append (" has been parsed").c_str (), "Information", MB_ICONINFORMATION);

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

void openFile (Ctx *ctx, CatalogItem *item) {
    char path [MAX_PATH];

    PathCombine (path, ctx->basePath.c_str (), item->fileName.c_str ());

    loadParseS57File (path);
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

            if (selection >= 0) {
                LVITEM item;

                memset (& item, 0, sizeof (item));

                item.iItem = selection;
                item.mask = LVIF_PARAM;

                SendMessage (ctx->catalogCtl, LVM_GETITEM, 0, (LPARAM) & item);

                openFile (ctx, (CatalogItem *) item.lParam);
                break;
            }

            break;
        }
    }
}

void onSize (HWND wnd, int width, int height) {
    Ctx *ctx = (Ctx *) GetWindowLongPtr (wnd, GWLP_USERDATA);
}

LRESULT wndProc (HWND wnd, UINT msg, WPARAM param1, LPARAM param2) {
    LRESULT result = 0;

    switch (msg) {
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
}

int WINAPI WinMain (HINSTANCE instance, HINSTANCE prevInstance, char *cmd, int showCmd) {
    Ctx ctx (instance, LoadMenu (instance, MAKEINTRESOURCE (IDR_MAINMENU)));

    registerClasses (ctx);

    ctx.mainWnd = CreateWindow (MAIN_CLASS, "S57 Tool", WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, 800, 500, 0, ctx.mainMenu, instance, & ctx);

    ShowWindow (ctx.mainWnd, SW_SHOW);
    UpdateWindow (ctx.mainWnd);

    ctx.keepRunning = true;

    MSG msg;

    while (GetMessage (& msg, 0, 0, 0)) {
        TranslateMessage (& msg);
        DispatchMessage (& msg);
    }

}