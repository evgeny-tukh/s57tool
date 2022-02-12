#include <Windows.h>
#include <cstdint>
#include "settings.h"

void initSettingsWnd (HWND wnd, void *data) {
    Settings *settings = (Settings *) data;
    RECT client;

    GetClientRect (wnd, & client);
    SetWindowLongPtr (wnd, GWLP_USERDATA, (LONG_PTR) data);
}

void onSettingsWndCommand (HWND wnd, uint16_t command, uint16_t notification) {
}

LRESULT settingsWndProc (HWND wnd, UINT msg, WPARAM param1, LPARAM param2) {
    LRESULT result = 0;

    switch (msg) {
        case WM_COMMAND:
            onSettingsWndCommand (wnd, LOWORD (param1), HIWORD (param1)); break;
        case WM_CREATE:
            initSettingsWnd (wnd, ((CREATESTRUCT *) param2)->lpCreateParams); break;
        default:
            result = DefWindowProc (wnd, msg, param1, param2);
    }
    
    return result;
}