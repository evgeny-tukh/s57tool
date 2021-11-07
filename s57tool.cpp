#include <Windows.h>
#include <cstdint>
#include <string>
#include "resource.h"
#include "parser.h"

const char *MAIN_CLASS = "S57ToolMainWnd";

struct Ctx {
    HINSTANCE instance;
    HWND mainWnd;
    HMENU mainMenu;
    bool keepRunning;

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
        if (parseCatalog (path)) {
            std::string msg (path);

            MessageBox (ctx->mainWnd, msg.append (" has been parsed").c_str (), "Information", MB_ICONINFORMATION);
        } else {
            MessageBox (ctx->mainWnd, msg.insert (0, " has been parsed").c_str (), "Problem", MB_ICONEXCLAMATION);
        }
    }
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

    ctx.mainWnd = CreateWindow (MAIN_CLASS, "S57 Tool", WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, 300, 300, 0, ctx.mainMenu, instance, & ctx);

    ShowWindow (ctx.mainWnd, SW_SHOW);
    UpdateWindow (ctx.mainWnd);

    ctx.keepRunning = true;

    MSG msg;

    while (GetMessage (& msg, 0, 0, 0)) {
        TranslateMessage (& msg);
        DispatchMessage (& msg);
    }

}