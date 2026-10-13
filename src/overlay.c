#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "overlay.h"
#include "system_info.h"

static HWND g_overlay = NULL;
static APP_SETTINGS *g_settings = NULL;
static int fps_value = 0;
static LARGE_INTEGER perf_freq;

static int count_overlay_lines(const APP_SETTINGS *settings) {
    int lines = 1; /* FPS */
    if (settings->show_ram) lines++;
    if (settings->show_vram) lines++;
    if (settings->show_temps) lines += 2;
    return lines;
}

static void calc_overlay_dimensions(const APP_SETTINGS *settings, int *width, int *height) {
    int line_count = count_overlay_lines(settings);
    int line_height = settings->font_size + 6;
    if (line_height < 14) line_height = 14;
    *width = 220 + (settings->font_size > 8 ? (settings->font_size - 8) * 8 : 0);
    if (*width < 220) *width = 220;
    *height = 12 + (line_count * line_height);
}

static void draw_overlay(HWND hwnd, HDC hdc) {
    RECT r;
    GetClientRect(hwnd, &r);

    /* clear background (classic 95 style gray) */
    HBRUSH bg = CreateSolidBrush(RGB(192,192,192));
    FillRect(hdc, &r, bg);
    DeleteObject(bg);

    SetBkMode(hdc, TRANSPARENT);
    HFONT oldf = (HFONT)SelectObject(hdc, g_settings->font ? g_settings->font : GetStockObject(DEFAULT_GUI_FONT));
    SetTextColor(hdc, g_settings->color);

    TEXTMETRICA tm;
    int line_height = 16;
    if (GetTextMetricsA(hdc, &tm)) {
        line_height = tm.tmHeight + 2;
        if (line_height < 14) line_height = 14;
    }

    int y = 6;
    char buf[256];

    snprintf(buf, sizeof(buf), "FPS: %d", fps_value);
    TextOutA(hdc, 6, y, buf, (int)strlen(buf)); y += line_height;

    if (g_settings->show_ram) {
        RAM_INFO ri; query_ram(&ri);
        double used = (ri.totalPhys - ri.availPhys) / 1024.0 / 1024.0;
        double total = ri.totalPhys / 1024.0 / 1024.0;
        snprintf(buf, sizeof(buf), "RAM: %.0f MB / %.0f MB", used, total);
        TextOutA(hdc, 6, y, buf, (int)strlen(buf)); y += line_height;
    }
    if (g_settings->show_vram) {
        VRAM_INFO vi; query_vram(&vi);
        if (vi.available) {
            double total_gb = vi.vramTotalBytes / 1024.0 / 1024.0 / 1024.0;
            if (vi.vramUsedBytes)
                snprintf(buf, sizeof(buf), "VRAM: %.2f GB used / %.2f GB", vi.vramUsedBytes/1024.0/1024.0/1024.0, total_gb);
            else
                snprintf(buf, sizeof(buf), "VRAM: total %.2f GB", total_gb);
        } else {
            snprintf(buf, sizeof(buf), "VRAM: n/a");
        }
        TextOutA(hdc, 6, y, buf, (int)strlen(buf)); y += line_height;
    }
    if (g_settings->show_temps) {
        TEMP_INFO ti; query_temps(&ti);
        if (ti.cpuTempC >= 0)
            snprintf(buf, sizeof(buf), "CPU temp: %d C", ti.cpuTempC);
        else
            snprintf(buf, sizeof(buf), "CPU temp: n/a");
        TextOutA(hdc, 6, y, buf, (int)strlen(buf)); y += line_height;

        if (ti.gpuTempC >= 0)
            snprintf(buf, sizeof(buf), "GPU temp: %d C", ti.gpuTempC);
        else
            snprintf(buf, sizeof(buf), "GPU temp: n/a");
        TextOutA(hdc, 6, y, buf, (int)strlen(buf)); y += line_height;
    }

    SelectObject(hdc, oldf);
}

static LRESULT CALLBACK OverlayProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CREATE:
        QueryPerformanceFrequency(&perf_freq);
        SetTimer(hwnd, 1, g_settings->update_ms, NULL);
        return 0;
    case WM_TIMER: {
        LARGE_INTEGER now; QueryPerformanceCounter(&now);
        static LARGE_INTEGER last = {0};
        if (last.QuadPart != 0) {
            double delta = (double)(now.QuadPart - last.QuadPart) / (double)perf_freq.QuadPart;
            if (delta > 0.0) fps_value = (int)(1.0 / delta + 0.5);
        }
        last = now;

        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        draw_overlay(hwnd, hdc);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_DESTROY:
        KillTimer(hwnd, 1);
        g_overlay = NULL;
        return 0;
    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
}

HWND create_overlay_window(HINSTANCE hInstance, APP_SETTINGS *settings) {
    g_settings = settings;
    WNDCLASSA wc = {0};
    wc.lpfnWndProc = OverlayProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = "PCCornerOverlayClass";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassA(&wc);

    int width, height;
    calc_overlay_dimensions(settings, &width, &height);
    DWORD style = WS_POPUP | WS_VISIBLE | WS_BORDER;
    HWND hwnd = CreateWindowExA(WS_EX_TOPMOST | WS_EX_TOOLWINDOW, wc.lpszClassName, "", style,
        settings->pos_x, settings->pos_y, width, height, NULL, NULL, hInstance, NULL);
    if (!hwnd) return NULL;

    g_overlay = hwnd;
    return hwnd;
}

void update_overlay_settings(APP_SETTINGS *settings) {
    g_settings = settings;
    if (g_overlay) {
        int width, height;
        calc_overlay_dimensions(g_settings, &width, &height);
        SetWindowPos(g_overlay, HWND_TOPMOST, g_settings->pos_x, g_settings->pos_y, width, height,
                     SWP_NOACTIVATE | SWP_SHOWWINDOW);
        SetTimer(g_overlay, 1, g_settings->update_ms, NULL);
        InvalidateRect(g_overlay, NULL, FALSE);
    }
}

void show_overlay(int show) {
    if (!g_overlay) return;
    ShowWindow(g_overlay, show ? SW_SHOW : SW_HIDE);
}
