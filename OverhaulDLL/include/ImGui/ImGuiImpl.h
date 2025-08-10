/*
    DARK SOULS OVERHAUL

    Contributors to this file:
        TheGameMecha    -  C++
*/

#pragma once

#ifndef IMGUI_IMPL_H
#define IMGUI_IMPL_H

#include "ImGuiTypes.h"
#include "d3d11/main.h"

#include <vector>
#include <unordered_map>

/////////////////////////////////
// STRUCTS
/////////////////////////////////

struct PlayerIns;

struct HookInfo {
    void* target;       // Function address to hook
    void* detour;       // Hooked function
    void** original;    // Where MinHook stores the original function
    const char* name;   // Optional: for logging
};

/////////////////////////////////
// ENUMS
/////////////////////////////////

enum VTABLE_IDX : UINT8
{
    PRESENT = 8,
};

/////////////////////////////////
// TYPEDEFS
/////////////////////////////////

typedef HRESULT(WINAPI* D3D11CreateDeviceAndSwapChain_t)(_In_opt_ IDXGIAdapter*, D3D_DRIVER_TYPE, HMODULE, UINT, _In_opt_ const D3D_FEATURE_LEVEL*, UINT, UINT,
    _In_opt_ const DXGI_SWAP_CHAIN_DESC*, _Out_opt_ IDXGISwapChain**, _Out_opt_ ID3D11Device**, _Out_opt_ D3D_FEATURE_LEVEL*, _Out_opt_ ID3D11DeviceContext**);

typedef HRESULT(__stdcall* PresentFn)(IDXGISwapChain* thisPtr, UINT syncInterval, UINT flags);

class ImGuiImpl
{
public:
    ImGuiImpl();

    static void Initialize();
    static void SetupImGui(IDXGISwapChain* swapChain, ID3D11Device* device);
    static void DrawWindow();
    static void Shutdown();

    static bool WantCaptureInput();

    static void ToggleVisibility();
    static void SetVisibility(bool newVisibility);

protected:
    static DWORD HandleRenderHook();
    static DWORD HandleGameHooks();
    static DWORD WINAPI InitThread(LPVOID);

    static LRESULT CALLBACK WndProcHook(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    static HRESULT __stdcall HookPresent(IDXGISwapChain* swap, UINT syncInterval, UINT flags);
    static IDXGISwapChain* CreateDummySwapChain(HWND hwnd, ID3D11Device** outDevice = nullptr, ID3D11DeviceContext** outContext = nullptr);

    static bool HookFunctions(const std::vector<HookInfo>& hooks);
private:

    static bool mIsSetup;
    static bool mIsVisible;

    // D3D
    static ID3D11DeviceContext* context;
    static ID3D11RenderTargetView* rtv;

    static PresentFn oPresent;

    // Windows
    static HWND g_hwnd;
    static WNDPROC oWndProc;

    // Game Specific Variables
    static PlayerIns* mCurrentPlayerIns;
};

#endif // IMGUI_IMPL_H

