/*
    DARK SOULS OVERHAUL

    Contributors to this file:
        TheGameMecha    -  C++
*/

#pragma once

#ifndef IMGUI_H
#define IMGUI_H

#include "d3d11/main.h"
#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_win32.h"
#include "imgui/backends/imgui_impl_dx11.h"
#include "MinHook.h"
#include "Xinput_1_3.h"

#include <dinput.h>

/////////////////////////////////
// ENUMS
/////////////////////////////////

enum VTABLE_IDX : UINT8
{
    CREATE_DEVICE = 3, // CreateDevice is usually 3rd method
    PRESENT = 8,
    GET_DEVICE_STATE = 9,
    GET_DEVICE_DATA = 10
};

/////////////////////////////////
// TYPEDEFS
/////////////////////////////////
typedef UINT(WINAPI* GetRawInputData_t)(HRAWINPUT hRawInput, UINT uiCommand, LPVOID pData, PUINT pcbSize, UINT cbSizeHeader);

typedef HRESULT(WINAPI* D3D11CreateDeviceAndSwapChain_t)(_In_opt_ IDXGIAdapter*, D3D_DRIVER_TYPE, HMODULE, UINT, _In_opt_ const D3D_FEATURE_LEVEL*, UINT, UINT,
    _In_opt_ const DXGI_SWAP_CHAIN_DESC*, _Out_opt_ IDXGISwapChain**, _Out_opt_ ID3D11Device**, _Out_opt_ D3D_FEATURE_LEVEL*, _Out_opt_ ID3D11DeviceContext**);

typedef HRESULT(__stdcall* PresentFn)(IDXGISwapChain* thisPtr, UINT syncInterval, UINT flags);
typedef SHORT(WINAPI* GetAsyncKeyState_t)(int vKey);
typedef BOOL(WINAPI* GetCursorPos_t)(LPPOINT lpPoint);
typedef HRESULT(STDMETHODCALLTYPE* GetDeviceState_t)(IDirectInputDevice8* pThis, DWORD cbData, LPVOID lpvData);
typedef HRESULT(WINAPI* DirectInput8Create_t)(HINSTANCE, DWORD, REFIID, LPVOID*, LPUNKNOWN);
typedef HRESULT(STDMETHODCALLTYPE* CreateDevice_t)(IDirectInput8* pThis, REFGUID rguid, IDirectInputDevice8** ppvOut, LPUNKNOWN pUnkOuter);
typedef HRESULT(STDMETHODCALLTYPE* GetDeviceData_t)(IDirectInputDevice8* pThis, DWORD cbObjectData, LPDIDEVICEOBJECTDATA rgdod, LPDWORD pdwInOut, DWORD dwFlags);
typedef DWORD(WINAPI* XInputGetState_t)(DWORD dwUserIndex, XINPUT_STATE* pState);

class ImGuiImpl
{
public:
    ImGuiImpl();

    static void Initialize();
    static void SetupImGui(IDXGISwapChain* swapChain, ID3D11Device* device);
    static void Update();
    static void Shutdown();

    static bool WantCaptureInput();

protected:
    static DWORD HandleRenderHook();
    static DWORD HandleInputHook();
    static DWORD WINAPI InitThread(LPVOID);

    static LRESULT CALLBACK WndProcHook(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    static HRESULT __stdcall HookPresent(IDXGISwapChain* swap, UINT syncInterval, UINT flags);
    static IDXGISwapChain* CreateDummySwapChain(HWND hwnd, ID3D11Device** outDevice = nullptr, ID3D11DeviceContext** outContext = nullptr);
    static HRESULT STDMETHODCALLTYPE HookedGetDeviceState(IDirectInputDevice8* pThis, DWORD cbData, LPVOID lpvData);
    static HRESULT STDMETHODCALLTYPE HookedGetDeviceData(IDirectInputDevice8* pThis, DWORD cbObjectData, LPDIDEVICEOBJECTDATA rgdod, LPDWORD pdwInOut, DWORD dwFlags);
    static HRESULT STDMETHODCALLTYPE HookedCreateDevice(IDirectInput8* pThis, REFGUID rguid, IDirectInputDevice8** ppvOut, LPUNKNOWN pUnkOuter);
    static HRESULT WINAPI HookedDirectInput8Create(HINSTANCE hinst, DWORD dwVersion, REFIID riidltf, LPVOID* ppvOut, LPUNKNOWN punkOuter);
    static SHORT WINAPI HookedGetAsyncKeyState(int vKey);
    static BOOL WINAPI HookedGetCursorPos(LPPOINT lpPoint);
    static UINT WINAPI HookedGetRawInputData(HRAWINPUT hRawInput, UINT uiCommand, LPVOID pData, PUINT pcbSize, UINT cbSizeHeader);
    static DWORD WINAPI HookedXInputGetState(DWORD dwUserIndex, XINPUT_STATE* pState);

private:

    static bool mIsSetup;

    // Raw Input
    static GetRawInputData_t OriginalGetRawInputData;
    static GetAsyncKeyState_t OriginalGetAsyncKeyState;
    static GetCursorPos_t OriginalGetCursorPos;

    // D3D
    static ID3D11DeviceContext* context;
    static ID3D11RenderTargetView* rtv;

    static PresentFn oPresent;

    // Windows
    static HWND g_hwnd;
    static WNDPROC oWndProc;

    // Input
    static GetDeviceState_t OriginalGetDeviceStateKeyboard;
    static GetDeviceState_t OriginalGetDeviceStateMouse;
    static DirectInput8Create_t OriginalDirectInput8Create;
    static CreateDevice_t OriginalCreateDevice;
    static IDirectInputDevice8* g_keyboardDevice;
    static IDirectInputDevice8* g_mouseDevice;
    static GetDeviceData_t OriginalGetDeviceDataKeyboard;
    static GetDeviceData_t OriginalGetDeviceDataMouse;

    // XInput
    static XInputGetState_t OriginalXInputGetState;
};

#endif // IMGUI_H
