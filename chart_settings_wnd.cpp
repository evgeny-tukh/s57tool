#include <Windows.h>
#include <cstdint>
#include <stdio.h>
#include "chart_settings.h"
#include "nmea_settings.h"
#include "resource.h"
#include "serial.h"

void initSettingsWnd (HWND wnd, void *data) {
    ChartSettings *settings = (ChartSettings *) data;
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

void saveSettings (HWND wnd) {
    ChartSettings *settings = (ChartSettings *) GetWindowLongPtr (wnd, GWLP_USERDATA);
    auto getBtnStatus = [&wnd] (uint16_t id, bool& flag) {
        flag = IsDlgButtonChecked (wnd, id) == BST_CHECKED;
    };
    auto getFloatValue = [&wnd] (uint16_t id, double& value) {
        char buffer [256];
        GetDlgItemText (wnd, id, buffer, sizeof (buffer));
        value = atof (buffer);
    };

    getBtnStatus (IDC_FULL_SECTOR_LENGTH, settings->fullSectorLength);
    getBtnStatus (IDC_SAFETY_CONTOUR_LABELS, settings->safetyContourLabels);
    getBtnStatus (IDC_TWO_SHADES, settings->twoShades);
    getBtnStatus (IDC_SHALLOW_PATTERN, settings->shallowPattern);
    getBtnStatus (IDC_SHOW_ISOLATED_DANGERS, settings->showIsolatedDanger);
    getBtnStatus (IDC_SHOW_LOW_ACCURACY, settings->showLowAccuracy);
    getBtnStatus (IDC_SYMBOLIZED_BOUNDARIES, settings->symbolizedBoundaries);
    getBtnStatus (IDC_DISPLAY_CONTOUR_LABELS, settings->displayContourLabels);
    getBtnStatus (IDC_SHOW_LIGHT_DESCRIPTIONS, settings->showLightDescriptions);
    getFloatValue (IDC_SAFETY_CONTOUR, settings->safetyContour);
    getFloatValue (IDC_SHALLOW_CONTOUR, settings->shallowContour);
    getFloatValue (IDC_DEEP_CONTOUR, settings->deepContour);
    getFloatValue (IDC_SAFETY_DEPTH, settings->safetyDepth);
}

void doChartSettingsWndCommand (HWND wnd, uint16_t cmd) {
    switch (cmd) {
        case IDOK: saveSettings (wnd);
        case IDCANCEL: EndDialog (wnd, cmd);
    }
}

void showNmeaSettings (HWND wnd) {
    NmeaSettings *settings = (NmeaSettings *) GetWindowLongPtr (wnd, GWLP_USERDATA);
    auto checkBtn = [&wnd] (uint16_t id, bool flag) {
        CheckDlgButton (wnd, id, flag ? BST_CHECKED : BST_UNCHECKED);
    };
    auto setFloatValue = [&wnd] (uint16_t id, double value, const char *format = "%.1f") {
        char buffer [100];
        sprintf (buffer, format, value);
        SetDlgItemText (wnd, id, buffer);
    };
    
    std::vector<std::string> ports;
    getSerialPortsList (ports);

    for (auto& port: ports) {
        SendDlgItemMessage (wnd, ID_GPS_PORT, CB_ADDSTRING, 0, (LPARAM) port.c_str ());
        SendDlgItemMessage (wnd, ID_GYRO_PORT, CB_ADDSTRING, 0, (LPARAM) port.c_str ());
    }

    std::vector<int> bauds { 4800, 9600, 38400, 115200 };
    for (auto baud: bauds) {
        auto item = SendDlgItemMessage (wnd, ID_GPS_BAUD, CB_ADDSTRING, 0, (LPARAM) std::to_string (baud).c_str ());
        SendDlgItemMessage (wnd, ID_GPS_BAUD, CB_SETITEMDATA, item, baud);
        item = SendDlgItemMessage (wnd, ID_GYRO_BAUD, CB_ADDSTRING, 0, (LPARAM) std::to_string (baud).c_str ());
        SendDlgItemMessage (wnd, ID_GYRO_BAUD, CB_SETITEMDATA, item, baud);
    }
}

void showChartSettings (HWND wnd) {
    ChartSettings *settings = (ChartSettings *) GetWindowLongPtr (wnd, GWLP_USERDATA);
    auto checkBtn = [&wnd] (uint16_t id, bool flag) {
        CheckDlgButton (wnd, id, flag ? BST_CHECKED : BST_UNCHECKED);
    };
    auto setFloatValue = [&wnd] (uint16_t id, double value, const char *format = "%.1f") {
        char buffer [100];
        sprintf (buffer, format, value);
        SetDlgItemText (wnd, id, buffer);
    };

    checkBtn (IDC_FULL_SECTOR_LENGTH, settings->fullSectorLength);
    checkBtn (IDC_SAFETY_CONTOUR_LABELS, settings->safetyContourLabels);
    checkBtn (IDC_TWO_SHADES, settings->twoShades);
    checkBtn (IDC_SHALLOW_PATTERN, settings->shallowPattern);
    checkBtn (IDC_SHOW_ISOLATED_DANGERS, settings->showIsolatedDanger);
    checkBtn (IDC_SHOW_LOW_ACCURACY, settings->showLowAccuracy);
    checkBtn (IDC_SYMBOLIZED_BOUNDARIES, settings->symbolizedBoundaries);
    checkBtn (IDC_DISPLAY_CONTOUR_LABELS, settings->displayContourLabels);
    checkBtn (IDC_SHOW_LIGHT_DESCRIPTIONS, settings->showLightDescriptions);
    setFloatValue (IDC_SAFETY_CONTOUR, settings->safetyContour);
    setFloatValue (IDC_SHALLOW_CONTOUR, settings->shallowContour);
    setFloatValue (IDC_DEEP_CONTOUR, settings->deepContour);
    setFloatValue (IDC_SAFETY_DEPTH, settings->safetyDepth);
}

INT_PTR CALLBACK chartSettingsDlgProc (HWND wnd, UINT msg, WPARAM param1, LPARAM param2) {
    switch (msg) {
        case WM_COMMAND:
            doChartSettingsWndCommand (wnd, LOWORD (param1)); break;
        case WM_INITDIALOG: {
            SetWindowLongPtr (wnd, GWLP_USERDATA, param2);
            showChartSettings (wnd);
            return true;
        }
    }
    
    return false;
}

INT_PTR CALLBACK nmeaSettingsDlgProc (HWND wnd, UINT msg, WPARAM param1, LPARAM param2) {
    switch (msg) {
        case WM_COMMAND:
            doChartSettingsWndCommand (wnd, LOWORD (param1)); break;
        case WM_INITDIALOG: {
            SetWindowLongPtr (wnd, GWLP_USERDATA, param2);
            showNmeaSettings (wnd);
            return true;
        }
    }
    
    return false;
}

bool editChartSettings (HINSTANCE instance, HWND parent, struct ChartSettings *settings) {
    return DialogBoxParam (instance, MAKEINTRESOURCE (IDD_CHART_SETTINGS), parent, chartSettingsDlgProc, (LPARAM) settings) == IDOK;
}

bool editNmeaSettings (HINSTANCE instance, HWND parent, struct NmeaSettings *settings) {
    return DialogBoxParam (instance, MAKEINTRESOURCE (IDD_NMEA_SETTINGS), parent, nmeaSettingsDlgProc, (LPARAM) settings) == IDOK;
}