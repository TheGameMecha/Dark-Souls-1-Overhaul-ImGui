#include "RenderHook.h"
#include "ImGui.h"
#include "MinHook.h"

#include "DarkSoulsOverhaulMod.h"

typedef HRESULT(WINAPI* D3D11CreateDeviceAndSwapChain_t)(
    _In_opt_ IDXGIAdapter*,
    D3D_DRIVER_TYPE,
    HMODULE,
    UINT,
    _In_opt_ const D3D_FEATURE_LEVEL*,
    UINT,
    UINT,
    _In_opt_ const DXGI_SWAP_CHAIN_DESC*,
    _Out_opt_ IDXGISwapChain**,
    _Out_opt_ ID3D11Device**,
    _Out_opt_ D3D_FEATURE_LEVEL*,
    _Out_opt_ ID3D11DeviceContext**
    );

// Typedef for the Present function
typedef HRESULT(__stdcall* PresentFn)(IDXGISwapChain* thisPtr, UINT syncInterval, UINT flags);
PresentFn oPresent = nullptr;

// Your Present hook
HRESULT __stdcall hkPresent(IDXGISwapChain* swap, UINT syncInterval, UINT flags)
{
    static bool initialized = false;
    if (!initialized) {
        initialized = true;
        ConsoleWrite("[+] Present Hook Called");

        ID3D11Device* device = nullptr;
        HRESULT hr = swap->GetDevice(__uuidof(ID3D11Device), (void**)&device);
        ImGuiImpl::Initialize(swap, device);
    }

    ImGuiImpl::Update();
    return oPresent(swap, syncInterval, flags);
}

// Create a dummy window (required for dummy swapchain)
HWND CreateDummyWindow()
{
    WNDCLASS wc = { 0 };
    wc.lpfnWndProc = DefWindowProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = "DummyWindowClass";
    RegisterClass(&wc);
    return CreateWindow(wc.lpszClassName, "", WS_OVERLAPPEDWINDOW, 0, 0, 100, 100, nullptr, nullptr, wc.hInstance, nullptr);
}

// Create a dummy D3D11 device and swapchain
IDXGISwapChain* CreateDummySwapChain(HWND hwnd, ID3D11Device** outDevice = nullptr, ID3D11DeviceContext** outContext = nullptr)
{
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 1;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hwnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* context = nullptr;
    IDXGISwapChain* swap = nullptr;

    HRESULT hr = D3D11CreateDeviceAndSwapChain_t(d3d11::functions[d3d11::D3D11CreateDeviceAndSwapChain_i])(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        0,
        nullptr,
        0,
        D3D11_SDK_VERSION,
        &sd,
        &swap,
        &device,
        nullptr,
        &context
    );

    if (SUCCEEDED(hr)) {
        if (outDevice) *outDevice = device;
        else device->Release();
        if (outContext) *outContext = context;
        else context->Release();
        return swap;
    }

    return nullptr;
}

// Setup hook using dummy swapchain
void HookPresentFromDummySwapChain()
{
    HWND dummyHwnd = CreateDummyWindow();
    IDXGISwapChain* dummySwap = CreateDummySwapChain(dummyHwnd);
    if (!dummySwap) {
        ConsoleWrite("[-] Failed to create dummy swapchain");
        return;
    }

    void** vtable = *(void***)(dummySwap);
    void* targetFunc = vtable[8]; // Index 8 = Present()

    if (MH_Initialize() != MH_OK ||
        MH_CreateHook(targetFunc, &hkPresent, reinterpret_cast<void**>(&oPresent)) != MH_OK ||
        MH_EnableHook(targetFunc) != MH_OK) {
        ConsoleWrite("[-] MinHook setup failed");
        dummySwap->Release();
        return;
    }

    ConsoleWrite("[+] Present hook set up successfully");
    dummySwap->Release();
}

static DWORD WINAPI InitThread(LPVOID)
{
    HookPresentFromDummySwapChain();
    return 0;
}

bool RenderHook::Initialize()
{
    ConsoleWrite("RenderHook: Initializing...");

    CreateThread(nullptr, 0, InitThread, nullptr, 0, nullptr);

    return true;
}

void RenderHook::Shutdown()
{
    ImGuiImpl::Shutdown();
}
