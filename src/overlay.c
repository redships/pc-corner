#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include "overlay.h"
#include "system_info.h"

static HWND g_overlay = NULL;
static APP_SETTINGS *g_settings = NULL;
static int fps_value = 0;
static LARGE_INTEGER perf_freq;
static HANDLE g_fpsMap = NULL;
static volatile LONG64 *g_sharedFrameCounter = NULL;

static void release_fps_mapping(void) {
    if (g_sharedFrameCounter) {
        UnmapViewOfFile((LPCVOID)g_sharedFrameCounter);
        g_sharedFrameCounter = NULL;
    }
    if (g_fpsMap) {
        CloseHandle(g_fpsMap);
        g_fpsMap = NULL;
    }
}

static int ensure_fps_mapping(void) {
    if (g_sharedFrameCounter) return 1;
    if (!g_fpsMap) {
        g_fpsMap = OpenFileMappingA(FILE_MAP_READ, FALSE, "Local\\PCCorner_FPS");
        if (!g_fpsMap) return 0;
    }
    g_sharedFrameCounter = (volatile LONG64*)MapViewOfFile(g_fpsMap, FILE_MAP_READ, 0, 0, sizeof(LONG64));
    if (!g_sharedFrameCounter) {
        CloseHandle(g_fpsMap);
        g_fpsMap = NULL;
        return 0;
    }
    return 1;
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

    int y = 6;
    char buf[256];

    snprintf(buf, sizeof(buf), "FPS: %d", fps_value);
    TextOutA(hdc, 6, y, buf, (int)strlen(buf)); y += 16;

    if (g_settings->show_ram) {
        RAM_INFO ri; query_ram(&ri);
        double used = (ri.totalPhys - ri.availPhys) / 1024.0 / 1024.0;
        double total = ri.totalPhys / 1024.0 / 1024.0;
        snprintf(buf, sizeof(buf), "RAM: %.0f MB / %.0f MB", used, total);
        TextOutA(hdc, 6, y, buf, (int)strlen(buf)); y += 16;
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
        TextOutA(hdc, 6, y, buf, (int)strlen(buf)); y += 16;
    }
    if (g_settings->show_temps) {
        TEMP_INFO ti; query_temps(&ti);
        if (ti.cpuTempC >= 0)
            snprintf(buf, sizeof(buf), "CPU temp: %d C", ti.cpuTempC);
        else
            snprintf(buf, sizeof(buf), "CPU temp: n/a");
        TextOutA(hdc, 6, y, buf, (int)strlen(buf)); y += 16;

        if (ti.gpuTempC >= 0)
            snprintf(buf, sizeof(buf), "GPU temp: %d C", ti.gpuTempC);
        else
            snprintf(buf, sizeof(buf), "GPU temp: n/a");
        TextOutA(hdc, 6, y, buf, (int)strlen(buf)); y += 16;
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
        static LONG64 lastSharedCount = -1;
        if (last.QuadPart != 0) {
            double delta = (double)(now.QuadPart - last.QuadPart) / (double)perf_freq.QuadPart;
            if (delta > 0.0) {
                int fallbackFps = (int)(1.0 / delta + 0.5);
                int hookedFps = -1;
                if (ensure_fps_mapping()) {
                    LONG64 currentCount = *g_sharedFrameCounter;
                    if (lastSharedCount >= 0 && currentCount >= lastSharedCount) {
                        hookedFps = (int)(((double)(currentCount - lastSharedCount) / delta) + 0.5);
                    }
                    lastSharedCount = currentCount;
                } else {
                    lastSharedCount = -1;
                }
                fps_value = (hookedFps >= 0) ? hookedFps : fallbackFps;
            }
        }
        last = now;

        InvalidateRect(hwnd, NULL, TRUE);
        UpdateWindow(hwnd);
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
        release_fps_mapping();
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

    DWORD style = WS_POPUP | WS_VISIBLE | WS_BORDER;
    HWND hwnd = CreateWindowExA(WS_EX_TOPMOST | WS_EX_TOOLWINDOW, wc.lpszClassName, "", style,
        settings->pos_x, settings->pos_y, 220, 120, NULL, NULL, hInstance, NULL);
    if (!hwnd) return NULL;
    g_overlay = hwnd;
    return hwnd;
}

void update_overlay_settings(APP_SETTINGS *settings) {
    g_settings = settings;
    if (g_overlay) {
        SetTimer(g_overlay, 1, g_settings->update_ms, NULL);
        InvalidateRect(g_overlay, NULL, TRUE);
    }
}

void show_overlay(int show) {
    if (!g_overlay) return;
    ShowWindow(g_overlay, show ? SW_SHOW : SW_HIDE);
}
