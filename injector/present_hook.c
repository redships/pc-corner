/* present_hook.c
   Simple DLL that hooks IDXGISwapChain::Present by vtable patching to count frames.
   Exposes a named file mapping "Local\\PCCorner_FPS" containing a 64-bit counter.

   Build (MSVC): cl /LD /O2 present_hook.c /link d3d11.lib
*/

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <stdio.h>

#pragma comment(lib, "d3d11.lib")

typedef HRESULT (__stdcall *Present_t)(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags);

static Present_t g_origPresent = NULL;
static void *g_origPresentAddr = NULL;
static HANDLE g_hMap = NULL;
static volatile LONG64 *g_frameCounter = NULL;

HRESULT __stdcall myPresent(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags) {
    if (g_frameCounter) InterlockedIncrement64((volatile LONG64*)g_frameCounter);
    return g_origPresent(pSwapChain, SyncInterval, Flags);
}

static int hook_dxgi_present() {
    IDXGISwapChain *swap = NULL;
    ID3D11Device *device = NULL;
    ID3D11DeviceContext *ctx = NULL;

    DXGI_SWAP_CHAIN_DESC sd = {0};
    sd.BufferCount = 1;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = GetDesktopWindow();
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;

    HMODULE hD3D11 = LoadLibraryA("d3d11.dll");
    if (!hD3D11) return 0;

    HRESULT hr = D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, NULL, 0,
        D3D11_SDK_VERSION, &sd, &swap, &device, NULL, &ctx);
    if (FAILED(hr) || !swap) {
        if (device) device->lpVtbl->Release(device);
        if (ctx) ctx->lpVtbl->Release(ctx);
        return 0;
    }

    void **vtable = *(void***)swap; /* swap->lpVtbl */
    Present_t orig = (Present_t)vtable[8]; /* Present usually at index 8 */
    g_origPresent = orig;
    g_origPresentAddr = vtable + 8;

    DWORD old;
    SIZE_T patchSize = sizeof(void*);
    if (!VirtualProtect(vtable + 8, patchSize, PAGE_EXECUTE_READWRITE, &old)) {
        swap->lpVtbl->Release(swap);
        device->lpVtbl->Release(device);
        ctx->lpVtbl->Release(ctx);
        return 0;
    }
    vtable[8] = (void*)myPresent;
    VirtualProtect(vtable + 8, patchSize, old, &old);

    /* cleanup our local objects; vtable patch stays because vtable is shared in module memory */
    swap->lpVtbl->Release(swap);
    device->lpVtbl->Release(device);
    ctx->lpVtbl->Release(ctx);
    return 1;
}

static void create_shared_counter() {
    g_hMap = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(LONG64), "Local\\PCCorner_FPS");
    if (!g_hMap) return;
    g_frameCounter = (volatile LONG64*)MapViewOfFile(g_hMap, FILE_MAP_WRITE, 0, 0, sizeof(LONG64));
    if (g_frameCounter) *g_frameCounter = 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
        CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)hook_dxgi_present, NULL, 0, NULL);
        create_shared_counter();
        break;
    case DLL_PROCESS_DETACH:
        if (g_hMap) {
            if (g_frameCounter) UnmapViewOfFile(g_frameCounter);
            CloseHandle(g_hMap);
        }
        break;
    }
    return TRUE;
}
