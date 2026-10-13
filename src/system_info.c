#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dxgi.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
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

/* -------- NVIDIA NVAPI runtime binding (optional) -------- */
#define NVAPI_MAX_THERMAL_SENSORS 3
#define NVAPI_MAX_PHYSICAL_GPUS 64
#define NVAPI_OK 0
#define NVAPI_THERMAL_TARGET_ALL 15
#define NVAPI_MAKE_VERSION(typeName, ver) (NvU32)(sizeof(typeName) | ((ver) << 16))

typedef int NvAPI_Status;
typedef unsigned int NvU32;
typedef void* NvPhysicalGpuHandle;
typedef void* (__cdecl *PFN_nvapi_QueryInterface)(unsigned int);
typedef NvAPI_Status (__cdecl *PFN_NvAPI_Initialize)(void);
typedef NvAPI_Status (__cdecl *PFN_NvAPI_EnumPhysicalGPUs)(NvPhysicalGpuHandle[NVAPI_MAX_PHYSICAL_GPUS], NvU32*);
typedef NvAPI_Status (__cdecl *PFN_NvAPI_GPU_GetThermalSettings)(NvPhysicalGpuHandle, NvU32, void*);

#define NVAPI_ID_INITIALIZE               0x0150E828
#define NVAPI_ID_ENUM_PHYSICAL_GPUS       0xE5AC921F
#define NVAPI_ID_GET_THERMAL_SETTINGS     0xE3640A56

typedef struct {
    int controller;
    int defaultMinTemp;
    int defaultMaxTemp;
    int currentTemp;
    int target;
} NV_GPU_THERMAL_SENSOR;

typedef struct {
    NvU32 version;
    NvU32 count;
    NV_GPU_THERMAL_SENSOR sensor[NVAPI_MAX_THERMAL_SENSORS];
} NV_GPU_THERMAL_SETTINGS;

static int g_nvapiInitState = 0;
static PFN_NvAPI_EnumPhysicalGPUs g_nvEnumPhysicalGPUs = NULL;
static PFN_NvAPI_GPU_GetThermalSettings g_nvGetThermalSettings = NULL;

static int init_nvapi(void) {
    if (g_nvapiInitState != 0) return g_nvapiInitState > 0;
    g_nvapiInitState = -1;

    HMODULE nvapi = LoadLibraryA("nvapi64.dll");
    if (!nvapi) nvapi = LoadLibraryA("nvapi.dll");
    if (!nvapi) return 0;

    PFN_nvapi_QueryInterface queryInterface = (PFN_nvapi_QueryInterface)GetProcAddress(nvapi, "nvapi_QueryInterface");
    if (!queryInterface) return 0;

    PFN_NvAPI_Initialize nvInit = (PFN_NvAPI_Initialize)queryInterface(NVAPI_ID_INITIALIZE);
    g_nvEnumPhysicalGPUs = (PFN_NvAPI_EnumPhysicalGPUs)queryInterface(NVAPI_ID_ENUM_PHYSICAL_GPUS);
    g_nvGetThermalSettings = (PFN_NvAPI_GPU_GetThermalSettings)queryInterface(NVAPI_ID_GET_THERMAL_SETTINGS);
    if (!nvInit || !g_nvEnumPhysicalGPUs || !g_nvGetThermalSettings) return 0;
    if (nvInit() != NVAPI_OK) return 0;

    g_nvapiInitState = 1;
    return 1;
}

static int nvapi_gpu_temp_probe(int *tempOut) {
    if (!tempOut || !init_nvapi()) return 0;

    NvPhysicalGpuHandle gpus[NVAPI_MAX_PHYSICAL_GPUS];
    NvU32 gpuCount = 0;
    if (g_nvEnumPhysicalGPUs(gpus, &gpuCount) != NVAPI_OK || gpuCount == 0) return 0;

    NV_GPU_THERMAL_SETTINGS thermal = {0};
    thermal.version = NVAPI_MAKE_VERSION(NV_GPU_THERMAL_SETTINGS, 1);
    if (g_nvGetThermalSettings(gpus[0], NVAPI_THERMAL_TARGET_ALL, &thermal) != NVAPI_OK) return 0;
    if (thermal.count == 0) return 0;

    *tempOut = thermal.sensor[0].currentTemp;
    return (*tempOut >= 0);
}

/* -------- AMD ADL runtime binding (optional) -------- */
typedef void* (__stdcall *ADL_MAIN_MALLOC_CALLBACK)(int);
typedef int (__stdcall *PFN_ADL_MAIN_CONTROL_CREATE)(ADL_MAIN_MALLOC_CALLBACK, int);
typedef int (__stdcall *PFN_ADL_MAIN_CONTROL_DESTROY)(void);
typedef int (__stdcall *PFN_ADL_ADAPTER_NUMBEROFADAPTERS_GET)(int*);
typedef int (__stdcall *PFN_ADL_ADAPTER_ADAPTERINFO_GET)(void*, int);
typedef int (__stdcall *PFN_ADL_ADAPTER_ACTIVE_GET)(int, int*);
typedef int (__stdcall *PFN_ADL_OVERDRIVE5_TEMPERATURE_GET)(int, int, void*);

#define ADL_OK 0
#define AMD_VENDOR_ID 1002

typedef struct {
    int iSize;
    int iAdapterIndex;
    char strUDID[256];
    int iBusNumber;
    int iDeviceNumber;
    int iFunctionNumber;
    int iVendorID;
    char strAdapterName[256];
    char strDisplayName[256];
    int iPresent;
    int iExist;
    char strDriverPath[256];
    char strDriverPathExt[256];
    char strPNPString[256];
    int iOSDisplayIndex;
} ADLAdapterInfo;

typedef struct {
    int iSize;
    int iTemperature; /* millidegrees Celsius */
} ADLTemperature;

static void* __stdcall adl_malloc(int size) {
    return malloc((size_t)size);
}

static int adl_gpu_temp_probe(int *tempOut) {
    if (!tempOut) return 0;

    HMODULE adl = LoadLibraryA("atiadlxx.dll");
    if (!adl) adl = LoadLibraryA("atiadlxy.dll");
    if (!adl) return 0;

    PFN_ADL_MAIN_CONTROL_CREATE adlCreate = (PFN_ADL_MAIN_CONTROL_CREATE)GetProcAddress(adl, "ADL_Main_Control_Create");
    PFN_ADL_MAIN_CONTROL_DESTROY adlDestroy = (PFN_ADL_MAIN_CONTROL_DESTROY)GetProcAddress(adl, "ADL_Main_Control_Destroy");
    PFN_ADL_ADAPTER_NUMBEROFADAPTERS_GET adlGetCount = (PFN_ADL_ADAPTER_NUMBEROFADAPTERS_GET)GetProcAddress(adl, "ADL_Adapter_NumberOfAdapters_Get");
    PFN_ADL_ADAPTER_ADAPTERINFO_GET adlGetInfo = (PFN_ADL_ADAPTER_ADAPTERINFO_GET)GetProcAddress(adl, "ADL_Adapter_AdapterInfo_Get");
    PFN_ADL_ADAPTER_ACTIVE_GET adlIsActive = (PFN_ADL_ADAPTER_ACTIVE_GET)GetProcAddress(adl, "ADL_Adapter_Active_Get");
    PFN_ADL_OVERDRIVE5_TEMPERATURE_GET adlGetTemp = (PFN_ADL_OVERDRIVE5_TEMPERATURE_GET)GetProcAddress(adl, "ADL_Overdrive5_Temperature_Get");
    if (!adlCreate || !adlDestroy || !adlGetCount || !adlGetInfo || !adlIsActive || !adlGetTemp) {
        FreeLibrary(adl);
        return 0;
    }

    if (adlCreate(adl_malloc, 1) != ADL_OK) {
        FreeLibrary(adl);
        return 0;
    }

    int adapterCount = 0;
    int success = 0;
    if (adlGetCount(&adapterCount) == ADL_OK && adapterCount > 0) {
        int infoSize = (int)(sizeof(ADLAdapterInfo) * (size_t)adapterCount);
        ADLAdapterInfo *info = (ADLAdapterInfo*)malloc((size_t)infoSize);
        if (info) {
            memset(info, 0, (size_t)infoSize);
            if (adlGetInfo(info, infoSize) == ADL_OK) {
                int i;
                for (i = 0; i < adapterCount; ++i) {
                    int active = 0;
                    if (info[i].iVendorID != AMD_VENDOR_ID) continue;
                    if (adlIsActive(info[i].iAdapterIndex, &active) != ADL_OK || !active) continue;

                    ADLTemperature temp = {0};
                    temp.iSize = sizeof(temp);
                    if (adlGetTemp(info[i].iAdapterIndex, 0, &temp) == ADL_OK && temp.iTemperature > 0) {
                        *tempOut = temp.iTemperature / 1000;
                        success = 1;
                        break;
                    }
                }
            }
            free(info);
        }
    }

    adlDestroy();
    FreeLibrary(adl);
    return success;
}

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
    if (nvapi_gpu_temp_probe(&out->gpuTempC)) return;
    adl_gpu_temp_probe(&out->gpuTempC);
}
