#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include "system_info.h"

void query_ram(RAM_INFO *out) {
    if (!out) return;
    MEMORYSTATUSEX ms = {0};
    ms.dwLength = sizeof(ms);
    if (GlobalMemoryStatusEx(&ms)) {
        out->totalPhys = ms.ullTotalPhys;
        out->availPhys = ms.ullAvailPhys;
        out->totalPageFile = ms.ullTotalPageFile;
        out->availPageFile = ms.ullAvailPageFile;
        out->totalVirtual = ms.ullTotalVirtual;
        out->availVirtual = ms.ullAvailVirtual;
    } else {
        out->totalPhys = out->availPhys = 0;
        out->totalPageFile = out->availPageFile = 0;
        out->totalVirtual = out->availVirtual = 0;
    }
}

typedef HRESULT (WINAPI *PFN_CreateDXGIFactory1)(REFIID, void **);

static int dxgi_vram_probe(unsigned long long *totalOut) {
    *totalOut = 0;
    HMODULE h = LoadLibraryA("dxgi.dll");
    if (!h) return 0;
    PFN_CreateDXGIFactory1 pCreate = (PFN_CreateDXGIFactory1)GetProcAddress(h, "CreateDXGIFactory1");
    if (!pCreate) { FreeLibrary(h); return 0; }

    void *factory = NULL;
    HRESULT hr = pCreate(&IID_IDXGIFactory1, &factory);
    if (FAILED(hr) || !factory) { FreeLibrary(h); return 0; }

    IDXGIFactory1 *f = (IDXGIFactory1*)factory;
    IDXGIAdapter *adapter = NULL;
    if (SUCCEEDED(f->lpVtbl->EnumAdapters(f, 0, &adapter)) && adapter) {
        DXGI_ADAPTER_DESC desc;
        if (SUCCEEDED(adapter->lpVtbl->GetDesc(adapter, &desc))) {
            *totalOut = (unsigned long long)desc.DedicatedVideoMemory;
            adapter->lpVtbl->Release(adapter);
            f->lpVtbl->Release(f);
            FreeLibrary(h);
            return 1;
        }
        adapter->lpVtbl->Release(adapter);
    }
    f->lpVtbl->Release(f);
    FreeLibrary(h);
    return 0;
}

void query_vram(VRAM_INFO *out) {
    out->vramTotalBytes = 0;
    out->vramUsedBytes = 0;
    out->available = 0;
    unsigned long long total = 0;
    if (dxgi_vram_probe(&total)) {
        out->vramTotalBytes = total;
        out->vramUsedBytes = 0;
        out->available = 1;
    } else {
        out->available = 0;
    }
}

void query_temps(TEMP_INFO *out) {
    out->cpuTempC = -1;
    out->gpuTempC = -1;
}
