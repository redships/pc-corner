#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include "resource.h"
#include "overlay.h"

#pragma comment(lib, "comctl32.lib")

static HINSTANCE g_hInst;
static APP_SETTINGS g_settings = {1, 500, 1, 1, 0, NULL, 8, RGB(255,255,255), 8, 8};
static HWND g_mainWnd = NULL;
static HWND g_overlayWnd = NULL;

static void load_settings_from_registry(APP_SETTINGS *s) {
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\RedShips\\PCCorner", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD val;
        DWORD type = REG_DWORD, size = sizeof(val);
        if (RegQueryValueExA(hKey, "OverlayEnabled", NULL, &type, (LPBYTE)&val, &size) == ERROR_SUCCESS) s->enabled = val;
        size = sizeof(val);
        if (RegQueryValueExA(hKey, "UpdateMs", NULL, &type, (LPBYTE)&val, &size) == ERROR_SUCCESS) s->update_ms = val;
        size = sizeof(val);
        if (RegQueryValueExA(hKey, "ShowRAM", NULL, &type, (LPBYTE)&val, &size) == ERROR_SUCCESS) s->show_ram = val;
        size = sizeof(val);
        if (RegQueryValueExA(hKey, "ShowVRAM", NULL, &type, (LPBYTE)&val, &size) == ERROR_SUCCESS) s->show_vram = val;
        size = sizeof(val);
        if (RegQueryValueExA(hKey, "ShowTemps", NULL, &type, (LPBYTE)&val, &size) == ERROR_SUCCESS) s->show_temps = val;
        size = sizeof(val);
        if (RegQueryValueExA(hKey, "PosX", NULL, &type, (LPBYTE)&val, &size) == ERROR_SUCCESS) s->pos_x = (int)val;
        size = sizeof(val);
        if (RegQueryValueExA(hKey, "PosY", NULL, &type, (LPBYTE)&val, &size) == ERROR_SUCCESS) s->pos_y = (int)val;
        size = sizeof(val);
        if (RegQueryValueExA(hKey, "FontSize", NULL, &type, (LPBYTE)&val, &size) == ERROR_SUCCESS) s->font_size = (int)val;
        size = sizeof(val);
        if (RegQueryValueExA(hKey, "Color", NULL, &type, (LPBYTE)&val, &size) == ERROR_SUCCESS) s->color = (COLORREF)val;
        RegCloseKey(hKey);
    }
}

static void save_settings_to_registry(APP_SETTINGS *s) {
    HKEY hKey;
    RegCreateKeyExA(HKEY_CURRENT_USER, "Software\\RedShips\\PCCorner", 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL);
    RegSetValueExA(hKey, "OverlayEnabled", 0, REG_DWORD, (const BYTE*)&s->enabled, sizeof(s->enabled));
    RegSetValueExA(hKey, "UpdateMs", 0, REG_DWORD, (const BYTE*)&s->update_ms, sizeof(s->update_ms));
    RegSetValueExA(hKey, "ShowRAM", 0, REG_DWORD, (const BYTE*)&s->show_ram, sizeof(s->show_ram));
    RegSetValueExA(hKey, "ShowVRAM", 0, REG_DWORD, (const BYTE*)&s->show_vram, sizeof(s->show_vram));
    RegSetValueExA(hKey, "ShowTemps", 0, REG_DWORD, (const BYTE*)&s->show_temps, sizeof(s->show_temps));
    DWORD val;
    val = (DWORD)s->pos_x; RegSetValueExA(hKey, "PosX", 0, REG_DWORD, (const BYTE*)&val, sizeof(val));
    val = (DWORD)s->pos_y; RegSetValueExA(hKey, "PosY", 0, REG_DWORD, (const BYTE*)&val, sizeof(val));
    val = (DWORD)s->font_size; RegSetValueExA(hKey, "FontSize", 0, REG_DWORD, (const BYTE*)&val, sizeof(val));
    val = (DWORD)s->color; RegSetValueExA(hKey, "Color", 0, REG_DWORD, (const BYTE*)&val, sizeof(val));
    RegCloseKey(hKey);
}

INT_PTR CALLBACK SettingsDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_INITDIALOG:
        CheckDlgButton(hwnd, IDC_CHECK_OVERLAY, g_settings.enabled ? BST_CHECKED : BST_UNCHECKED);
        SendDlgItemMessage(hwnd, IDC_SLIDER_RATE, TBM_SETRANGE, TRUE, MAKELONG(50, 2000));
        SendDlgItemMessage(hwnd, IDC_SLIDER_RATE, TBM_SETPOS, TRUE, g_settings.update_ms);
        CheckDlgButton(hwnd, IDC_CHECK_RAM, g_settings.show_ram ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hwnd, IDC_CHECK_VRAM, g_settings.show_vram ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hwnd, IDC_CHECK_TEMP, g_settings.show_temps ? BST_CHECKED : BST_UNCHECKED);
        SetDlgItemInt(hwnd, IDC_POSX, g_settings.pos_x, TRUE);
        SetDlgItemInt(hwnd, IDC_POSY, g_settings.pos_y, TRUE);
        SetDlgItemInt(hwnd, IDC_FONTSIZE, g_settings.font_size, FALSE);
        {
            char buf[64]; snprintf(buf, sizeof(buf), "#%02X%02X%02X", GetRValue(g_settings.color), GetGValue(g_settings.color), GetBValue(g_settings.color));
            SetDlgItemTextA(hwnd, IDC_COLOR, buf);
        }
        return TRUE;
    case WM_COMMAND:
        if (LOWORD(wParam) == IDC_BTN_OK) {
            g_settings.enabled = (IsDlgButtonChecked(hwnd, IDC_CHECK_OVERLAY) == BST_CHECKED);
            g_settings.update_ms = (unsigned int)SendDlgItemMessage(hwnd, IDC_SLIDER_RATE, TBM_GETPOS, 0, 0);
            g_settings.show_ram = (IsDlgButtonChecked(hwnd, IDC_CHECK_RAM) == BST_CHECKED);
            g_settings.show_vram = (IsDlgButtonChecked(hwnd, IDC_CHECK_VRAM) == BST_CHECKED);
            g_settings.show_temps = (IsDlgButtonChecked(hwnd, IDC_CHECK_TEMP) == BST_CHECKED);
            g_settings.pos_x = GetDlgItemInt(hwnd, IDC_POSX, NULL, TRUE);
            g_settings.pos_y = GetDlgItemInt(hwnd, IDC_POSY, NULL, TRUE);
            g_settings.font_size = GetDlgItemInt(hwnd, IDC_FONTSIZE, NULL, FALSE);
            char colorbuf[64]; GetDlgItemTextA(hwnd, IDC_COLOR, colorbuf, sizeof(colorbuf));
            /* parse #RRGGBB or R,G,B */
            if (colorbuf[0] == '#') {
                unsigned int r,g,b;
                if (sscanf(colorbuf+1, "%02x%02x%02x", &r, &g, &b) == 3) {
                    g_settings.color = RGB(r,g,b);
                }
            }
            save_settings_to_registry(&g_settings);
            update_overlay_settings(&g_settings);
            if (g_overlayWnd) show_overlay(g_settings.enabled);
            EndDialog(hwnd, IDOK);
            return TRUE;
        } else if (LOWORD(wParam) == IDC_BTN_CANCEL) {
            EndDialog(hwnd, IDCANCEL);
            return TRUE;
        }
        break;
    }
    return FALSE;
}

LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        CreateWindowExA(0, "BUTTON", "Settings", WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON, 10, 10, 80, 24, hwnd, (HMENU)1, g_hInst, NULL);
        CreateWindowExA(0, "BUTTON", "Inject PID...", WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON, 100, 10, 80, 24, hwnd, (HMENU)3, g_hInst, NULL);
        CreateWindowExA(0, "BUTTON", "Exit", WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON, 10, 40, 80, 24, hwnd, (HMENU)2, g_hInst, NULL);
        return 0;
    case WM_COMMAND:
        if (LOWORD(wParam) == 1) {
            DialogBox(g_hInst, MAKEINTRESOURCE(IDD_SETTINGS), hwnd, SettingsDlgProc);
        } else if (LOWORD(wParam) == 2) {
            PostQuitMessage(0);
        } else if (LOWORD(wParam) == 3) {
            /* Launch injector helper dialog */
            DialogBox(g_hInst, MAKEINTRESOURCE(IDD_INJECT), hwnd, (DLGPROC) (LRESULT) (LPVOID) CreateDialogParam);
            /* For simplicity the user can run injector.exe from the installer; documentation in README */
            MessageBoxA(hwnd, "Injector dialog not implemented in this minimal build. See README for injector.exe usage.", "Inject", MB_OK|MB_ICONINFORMATION);
        }
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrev, LPSTR lpCmdLine, int nCmdShow) {
    g_hInst = hInstance;
    INITCOMMONCONTROLSEX ic = {sizeof(ic), ICC_BAR_CLASSES};
    InitCommonControlsEx(&ic);

    load_settings_from_registry(&g_settings);

    WNDCLASSA wc = {0};
    wc.lpfnWndProc = MainWndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = "PCCornerMain";
    wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_APPICON));
    RegisterClassA(&wc);

    HWND mainWin = CreateWindowA(wc.lpszClassName, "PC Corner (classic UI)", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT, 240, 120, NULL, NULL, hInstance, NULL);
    g_mainWnd = mainWin;

    /* create overlay */
    g_settings.font = CreateFontA(g_settings.font_size,0,0,0,FW_NORMAL,0,0,0,ANSI_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,
                                  DEFAULT_QUALITY,FF_DONTCARE,"MS Sans Serif");
    g_overlayWnd = create_overlay_window(hInstance, &g_settings);
    if (g_overlayWnd) {
        /* position overlay */
        SetWindowPos(g_overlayWnd, HWND_TOPMOST, g_settings.pos_x, g_settings.pos_y, 220, 120, SWP_SHOWWINDOW);
        show_overlay(g_settings.enabled);
    }

    ShowWindow(mainWin, SW_SHOW);
    UpdateWindow(mainWin);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}
