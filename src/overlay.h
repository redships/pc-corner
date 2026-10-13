#pragma once
#include <windows.h>

typedef struct {
    int enabled;
    unsigned int update_ms;
    int show_ram;
    int show_vram;
    int show_temps;
    HFONT font;
    int font_size;
    COLORREF color;
    int pos_x;
    int pos_y;
} APP_SETTINGS;

HWND create_overlay_window(HINSTANCE hInstance, APP_SETTINGS *settings);
void update_overlay_settings(APP_SETTINGS *settings);
void show_overlay(int show);
