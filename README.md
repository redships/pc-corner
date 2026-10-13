# PC Corner — lightweight Win32 FPS/VRAM/RAM/Temp corner overlay

This repository contains a minimal, Windows 95 style, low-overhead Win32 application that shows a corner overlay with FPS, RAM, VRAM and temperature (best-effort) information. It also includes an optional injector and a DLL-based Present hook to measure per-application FPS.

What's included
- src/ : main application sources (classic Win32 GUI, simple settings dialog, overlay drawing)
- injector/ : injector.exe source and present_hook.dll source (DLL hooks IDXGISwapChain::Present and increments a shared counter)
- installer.nsi : NSIS installer script
- LICENSE

Design goals
- Use default Windows controls and GDI for drawing to keep memory/CPU use very low.
- Simple Windows 95 look: MS Sans Serif font, classic gray background, minimal chrome.
- Provide a light-weight per-application FPS measurement via an injectable DLL that hooks Present and writes a frame counter into a named memory mapping.

Build instructions (MSVC)

1) Build main app
   Open "Developer Command Prompt for VS" and run:
     cl /Zi /O2 /MD src\\main.c src\\overlay.c src\\system_info.c src\\resources.rc /link user32.lib gdi32.lib comctl32.lib dxgi.lib d3d11.lib
   This will produce PCCorner.exe

2) Build injector
     cl /O2 injector\\injector.c /link kernel32.lib
   Produces injector.exe

3) Build present hook DLL
     cl /LD /O2 injector\\present_hook.c /link d3d11.lib
   Produces present_hook.dll

Build (MinGW)
- Similar commands using gcc; link against -ld3d11 -ldxgi as available. For example:
  gcc -O2 -o PCCorner.exe src/main.c src/overlay.c src/system_info.c -lgdi32 -lcomctl32 -ldxgi -ld3d11
  gcc -O2 -o injector.exe injector/injector.c -lkernel32
  gcc -shared -o present_hook.dll injector/present_hook.c -ld3d11

Usage
- Run PCCorner.exe to show the classic UI and overlay.
- Settings allow toggling overlay, update rate, which metrics to show, overlay position, font size and color.
- To get per-application FPS (accurate):
  1) Build present_hook.dll and injector.exe.
  2) Use injector.exe to inject present_hook.dll into the target game/process: injector.exe <pid|procname> path\\to\\present_hook.dll
  3) The injected DLL creates a named mapping "Local\\PCCorner_FPS" (8 bytes). PCCorner can be extended to read this mapping and compute FPS by delta of the 64-bit counter across time.

Notes and limitations
- The overlay automatically reads the shared frame counter mapping `Local\\PCCorner_FPS` when the hook is injected and falls back to local timer FPS when no mapping is available.
- The Present hook implemented is intentionally minimal and uses a dummy D3D11 device to locate the IDXGISwapChain::Present vtable slot and patch it. This method works for many Direct3D11-based applications but is not universal.
- Kernel-level anti-cheat drivers or protected processes may prevent injection or hooking. Be careful: injecting into games with anti-cheat may lead to bans.
- GPU temperature probing now attempts NVAPI (NVIDIA) first and ADL/Overdrive (AMD) as a fallback, while retaining best-effort behavior when vendor libraries are unavailable.
- The injector uses CreateRemoteThread+LoadLibrary which requires appropriate privileges and may be blocked by some security products.

Extending configuration (what I changed after option B)
- Settings now include overlay position (Pos X, Pos Y), font size, and color (#RRGGBB). These are persisted in HKCU\\Software\\RedShips\\PCCorner.
- The settings dialog remains simple (classic controls) to preserve the 95-style look.
- To integrate per-application FPS into the overlay: read the 64-bit counter from the named mapping "Local\\PCCorner_FPS" and compute delta/frame-per-second in the overlay update loop. This keeps the overlay app decoupled and low-overhead (it only reads 8 bytes periodically).

Security & privacy
- This software reads local system metrics only. The injector and hooking functionality require caution and user consent.

License
- MIT (see LICENSE file)

CI now includes a Windows build workflow that compiles binaries, builds the NSIS installer, uploads artifacts, and auto-publishes a release when the push build succeeds.