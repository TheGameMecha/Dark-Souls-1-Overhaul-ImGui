#include "ImGui.h"
#include "DarkSoulsOverhaulMod.h"
#include <thread>
#include <chrono>

ImGuiImpl::ImGuiImpl() {}

bool ImGuiImpl::mIsSetup = false;

// Raw Input
GetRawInputData_t ImGuiImpl::OriginalGetRawInputData = nullptr;
GetAsyncKeyState_t ImGuiImpl::OriginalGetAsyncKeyState = nullptr;
GetCursorPos_t ImGuiImpl::OriginalGetCursorPos = nullptr;

// D3D
ID3D11DeviceContext* ImGuiImpl::context = nullptr;
ID3D11RenderTargetView* ImGuiImpl::rtv = nullptr;
PresentFn ImGuiImpl::oPresent = nullptr;

// Windows
HWND ImGuiImpl::g_hwnd = nullptr;
WNDPROC ImGuiImpl::oWndProc = nullptr;

// Input
GetDeviceState_t ImGuiImpl::OriginalGetDeviceStateKeyboard = nullptr;
GetDeviceState_t ImGuiImpl::OriginalGetDeviceStateMouse = nullptr;
DirectInput8Create_t ImGuiImpl::OriginalDirectInput8Create = nullptr;
CreateDevice_t ImGuiImpl::OriginalCreateDevice = nullptr;
IDirectInputDevice8* ImGuiImpl::g_keyboardDevice = nullptr;
IDirectInputDevice8* ImGuiImpl::g_mouseDevice = nullptr;
GetDeviceData_t ImGuiImpl::OriginalGetDeviceDataKeyboard = nullptr;
GetDeviceData_t ImGuiImpl::OriginalGetDeviceDataMouse = nullptr;

// XInput
XInputGetState_t ImGuiImpl::OriginalXInputGetState = nullptr;

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

/////////////////////////////////
// IMGUI HOOKING
/////////////////////////////////

LRESULT CALLBACK ImGuiImpl::WndProcHook(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam);
    ImGuiIO& io = ImGui::GetIO();

    const bool isMouseMsg = msg >= WM_MOUSEFIRST && msg <= WM_MOUSELAST;
    const bool isKbMsg = msg >= WM_KEYFIRST && msg <= WM_KEYLAST;

    if (io.WantCaptureMouse && isMouseMsg)
    {
        return 0; // Swallow mouse input fully
    }

    if (io.WantCaptureKeyboard && isKbMsg)
    {
        return 0; // Swallow keyboard input
    }

    return CallWindowProc(oWndProc, hwnd, msg, wParam, lParam);
}

/////////////////////////////////
// RENDER HOOKS
/////////////////////////////////

HRESULT __stdcall ImGuiImpl::HookPresent(IDXGISwapChain* swap, UINT syncInterval, UINT flags)
{
    static bool initialized = false;
    if (!initialized) {
        initialized = true;
        ConsoleWrite("RenderHook: Present Hook Initialized Successfully");

        ID3D11Device* device = nullptr;
        HRESULT hr = swap->GetDevice(__uuidof(ID3D11Device), (void**)&device);
        ImGuiImpl::SetupImGui(swap, device);
    }

    ImGuiImpl::Update();
    return oPresent(swap, syncInterval, flags);
}

IDXGISwapChain* ImGuiImpl::CreateDummySwapChain(HWND hwnd, ID3D11Device** outDevice, ID3D11DeviceContext** outContext)
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

    HRESULT hr = D3D11CreateDeviceAndSwapChain_t(d3d11::functions[d3d11::D3D11CreateDeviceAndSwapChain_i])
    (
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

    if (SUCCEEDED(hr))
    {
        if (outDevice)
        {
            *outDevice = device;
        }
        else
        {
            device->Release();
        }

        if (outContext)
        {
            *outContext = context;
        }
        else
        {
            context->Release();
        }

        return swap;
    }

    return nullptr;
}

DWORD ImGuiImpl::HandleRenderHook()
{
    ConsoleWrite("RenderHook: Initializing Render Hook");

    WNDCLASS wc = { 0 };
    wc.lpfnWndProc = DefWindowProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = "DummyWindowClass";
    RegisterClass(&wc);
    HWND dummyHwnd = CreateWindow(wc.lpszClassName, "", WS_OVERLAPPEDWINDOW, 0, 0, 100, 100, nullptr, nullptr, wc.hInstance, nullptr);

    IDXGISwapChain* dummySwap = CreateDummySwapChain(dummyHwnd);
    if (!dummySwap)
    {
        ConsoleWrite("RenderHook: Failed to create dummy swapchain");
        return 1;
    }

    void** vtable = *(void***)(dummySwap);
    void* targetFunc = vtable[VTABLE_IDX::PRESENT];

    if (MH_CreateHook(targetFunc, &HookPresent, reinterpret_cast<void**>(&oPresent)) != MH_OK || MH_EnableHook(targetFunc) != MH_OK)
    {
        ConsoleWrite("RenderHook: MinHook setup failed");
        dummySwap->Release();
        return 1;
    }

    ConsoleWrite("RenderHook: Present hook set up successfully");
    dummySwap->Release();

    return 0;
}

/////////////////////////////////
// INPUT HOOKS
/////////////////////////////////

/// <summary>
/// GetDeviceState Hook
/// </summary>
HRESULT STDMETHODCALLTYPE ImGuiImpl::HookedGetDeviceState(IDirectInputDevice8* pThis, DWORD cbData, LPVOID lpvData)
{
    if (ImGuiImpl::WantCaptureInput())
    {
        ZeroMemory(lpvData, cbData);
        return DI_OK;
    }

    // Determine if this is the keyboard or mouse device
    if (pThis == g_keyboardDevice && OriginalGetDeviceStateKeyboard)
    {
        return OriginalGetDeviceStateKeyboard(pThis, cbData, lpvData);
    }

    if (pThis == g_mouseDevice && OriginalGetDeviceStateMouse)
    {
        return OriginalGetDeviceStateMouse(pThis, cbData, lpvData);
    }

    return DI_OK; // fallback
}

/// <summary>
/// GetDeviceData Hook
/// </summary>
HRESULT STDMETHODCALLTYPE ImGuiImpl::HookedGetDeviceData(IDirectInputDevice8* pThis, DWORD cbObjectData, LPDIDEVICEOBJECTDATA rgdod, LPDWORD pdwInOut, DWORD dwFlags)
{
    if (ImGuiImpl::WantCaptureInput())
    {
        if (pdwInOut)
        {
            *pdwInOut = 0;
        }
        return DI_OK;
    }

    if (pThis == g_keyboardDevice && OriginalGetDeviceDataKeyboard)
    {
        return OriginalGetDeviceDataKeyboard(pThis, cbObjectData, rgdod, pdwInOut, dwFlags);
    }

    if (pThis == g_mouseDevice && OriginalGetDeviceDataMouse)
    {
        return OriginalGetDeviceDataMouse(pThis, cbObjectData, rgdod, pdwInOut, dwFlags);
    }

    return DI_OK;
}

/// <summary>
/// CreateDevice Hook
/// </summary>
HRESULT STDMETHODCALLTYPE ImGuiImpl::HookedCreateDevice(IDirectInput8* pThis, REFGUID rguid, IDirectInputDevice8** ppvOut, LPUNKNOWN pUnkOuter)
{
    HRESULT hr = OriginalCreateDevice(pThis, rguid, ppvOut, pUnkOuter);

    if (SUCCEEDED(hr))
    {
        if (rguid == GUID_SysKeyboard)
        {
            g_keyboardDevice = (IDirectInputDevice8*)*ppvOut;

            void** vtable = *(void***)g_keyboardDevice;

            void* getDeviceStateAddr = vtable[VTABLE_IDX::GET_DEVICE_STATE];
            MH_CreateHook(getDeviceStateAddr, &HookedGetDeviceState, reinterpret_cast<void**>(&OriginalGetDeviceStateKeyboard));
            MH_EnableHook(getDeviceStateAddr);

            void* getDeviceDataAddr = vtable[VTABLE_IDX::GET_DEVICE_DATA];
            MH_CreateHook(getDeviceDataAddr, &HookedGetDeviceData, reinterpret_cast<void**>(&OriginalGetDeviceDataKeyboard));
            MH_EnableHook(getDeviceDataAddr);
        }

        if (rguid == GUID_SysMouse)
        {
            g_mouseDevice = (IDirectInputDevice8*)*ppvOut;
            void** vtable = *(void***)g_mouseDevice;

            void* getDeviceStateAddr = vtable[VTABLE_IDX::GET_DEVICE_STATE];
            MH_CreateHook(getDeviceStateAddr, &HookedGetDeviceState, reinterpret_cast<void**>(&OriginalGetDeviceStateMouse));
            MH_EnableHook(getDeviceStateAddr);

            void* getDeviceDataAddr = vtable[VTABLE_IDX::GET_DEVICE_DATA];
            MH_CreateHook(getDeviceDataAddr, &HookedGetDeviceData, reinterpret_cast<void**>(&OriginalGetDeviceDataMouse));
            MH_EnableHook(getDeviceDataAddr);
        }
    }

    return hr;
}

/// <summary>
/// DirectInput8Create Hook
/// </summary>
HRESULT WINAPI ImGuiImpl::HookedDirectInput8Create(HINSTANCE hinst, DWORD dwVersion, REFIID riidltf, LPVOID* ppvOut, LPUNKNOWN punkOuter)
{
    HRESULT hr = OriginalDirectInput8Create(hinst, dwVersion, riidltf, ppvOut, punkOuter);
    if (SUCCEEDED(hr) && riidltf == IID_IDirectInput8)
    {
        IDirectInput8* pDI8 = reinterpret_cast<IDirectInput8*>(*ppvOut);

        void** vtable = *(void***)pDI8;
        void* createDeviceAddr = vtable[VTABLE_IDX::CREATE_DEVICE]; 

        MH_CreateHook(createDeviceAddr, &HookedCreateDevice, reinterpret_cast<void**>(&OriginalCreateDevice));
        MH_EnableHook(createDeviceAddr);
    }
    return hr;
}

/// <summary>
/// AsyncKeyState Hook
/// </summary>
SHORT WINAPI ImGuiImpl::HookedGetAsyncKeyState(int vKey)
{
    if (ImGuiImpl::WantCaptureInput())
    {
        return 0; // Pretend key is not pressed
    }

    return OriginalGetAsyncKeyState(vKey);
}

/// <summary>
/// GetCursorPos Hook
/// </summary>
BOOL WINAPI ImGuiImpl::HookedGetCursorPos(LPPOINT lpPoint)
{
    if (ImGuiImpl::WantCaptureInput())
    {
        // Return a fake mouse position off-screen or frozen
        lpPoint->x = -10000;
        lpPoint->y = -10000;
        return TRUE;
    }

    return OriginalGetCursorPos(lpPoint);
}

/// <summary>
/// GetRawInputData Hook
/// </summary>
UINT WINAPI ImGuiImpl::HookedGetRawInputData(HRAWINPUT hRawInput, UINT uiCommand, LPVOID pData, PUINT pcbSize, UINT cbSizeHeader)
{
    if (ImGuiImpl::WantCaptureInput() && uiCommand == RID_INPUT && pData)
    {
        RAWINPUT* raw = (RAWINPUT*)pData;
        if (raw->header.dwType == RIM_TYPEMOUSE || raw->header.dwType == RIM_TYPEKEYBOARD)
        {
            *pcbSize = 0;
            return 0;
        }
    }

    return OriginalGetRawInputData(hRawInput, uiCommand, pData, pcbSize, cbSizeHeader);
}

/// <summary>
/// XInputGetState Hook
/// </summary>
DWORD WINAPI ImGuiImpl::HookedXInputGetState(DWORD dwUserIndex, XINPUT_STATE* pState)
{
    if (ImGuiImpl::WantCaptureInput())
    {
        ZeroMemory(pState, sizeof(XINPUT_STATE));
        return ERROR_SUCCESS;
    }

    return OriginalXInputGetState(dwUserIndex, pState);
}

DWORD ImGuiImpl::HandleInputHook()
{
    ConsoleWrite("InputHook: Initializing Input Hook");

    DWORD errorCode = 0;

    // Get address of DirectInput8Create
    HMODULE dinput8 = GetModuleHandleA("dinput8.dll");
    if (!dinput8)
    {
        // dinput8.dll not loaded yet, wait for it
        while (!(dinput8 = GetModuleHandleA("dinput8.dll")))
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    void* directInput8CreateAddr = GetProcAddress(dinput8, "DirectInput8Create");
    if (directInput8CreateAddr)
    {
        MH_CreateHook(directInput8CreateAddr, &HookedDirectInput8Create, reinterpret_cast<void**>(&OriginalDirectInput8Create));
        MH_EnableHook(directInput8CreateAddr);
    }
    else
    {
        errorCode = 1;
    }

    HMODULE user32 = GetModuleHandleA("user32.dll");
    if (user32)
    {
        void* getRawInputDataAddr = GetProcAddress(user32, "GetRawInputData");
        if (getRawInputDataAddr)
        {
            MH_CreateHook(getRawInputDataAddr, &HookedGetRawInputData, reinterpret_cast<void**>(&OriginalGetRawInputData));
            MH_EnableHook(getRawInputDataAddr);
            ConsoleWrite("InputHook: Successfully hooked GetRawInputData");
        }
        else
        {
            ConsoleWrite("InputHook: Failed to hook GetRawInputData");
            errorCode = 1;
        }

        // Hook GetAsyncKeyState
        void* getAsyncKeyStateAddr = GetProcAddress(user32, "GetAsyncKeyState");
        if (getAsyncKeyStateAddr)
        {
            MH_CreateHook(getAsyncKeyStateAddr, &HookedGetAsyncKeyState, reinterpret_cast<void**>(&OriginalGetAsyncKeyState));
            MH_EnableHook(getAsyncKeyStateAddr);
            ConsoleWrite("InputHook: Successfully hooked GetAsyncKeyState");
        }
        else
        {
            ConsoleWrite("InputHook: Failed to hook GetAsyncKeyState");
            errorCode = 1;
        }

        void* getCursorPosAddr = GetProcAddress(user32, "GetCursorPos");
        if (getCursorPosAddr)
        {
            MH_CreateHook(getCursorPosAddr, &HookedGetCursorPos, reinterpret_cast<void**>(&OriginalGetCursorPos));
            MH_EnableHook(getCursorPosAddr);
            ConsoleWrite("InputHook: Successfully hooked GetCursorPos");
        }
        else
        {
            ConsoleWrite("InputHook: Failed to hook GetAsyncKeyState");
            errorCode = 1;
        }
    }

    HMODULE xinput = GetModuleHandleA("xinput1_3.dll");
    if (xinput)
    {
        void* xinputAddr = GetProcAddress(xinput, "XInputGetState");
        if (xinputAddr)
        {
            MH_CreateHook(xinputAddr, &HookedXInputGetState, reinterpret_cast<void**>(&OriginalXInputGetState));
            MH_EnableHook(xinputAddr);
            ConsoleWrite("InputHook: Successfully hooked XInputGetState");
        }
        else
        {
            ConsoleWrite("InputHook: Failed to hook XInputGetState");
            errorCode = 1;
        }
    }

    return errorCode;
}

/////////////////////////////////
// MAIN SETUP
/////////////////////////////////

DWORD WINAPI ImGuiImpl::InitThread(LPVOID)
{
    if (MH_Initialize() != MH_OK)
    {
        ConsoleWrite("ImGui: MinHook setup failed");
        return 1;
    }

    if (HandleRenderHook() != 0)
    {
        ConsoleWrite("ImGui: RenderHook setup failed");
        return 1;
    }

    if (HandleInputHook() != 0)
    {
        ConsoleWrite("ImGui: InputHook setup failed");
        return 1;
    }

    ConsoleWrite("ImGui: Successfully completed Setup");
    return 0;
}

void ImGuiImpl::Initialize()
{
    ConsoleWrite("Initializing ImGui");

    CreateThread(nullptr, 0, InitThread, nullptr, 0, nullptr);
}

void ImGuiImpl::SetupImGui(IDXGISwapChain* swapChain, ID3D11Device* device)
{
    ConsoleWrite("Performing ImGui Setup");

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags &= ~ImGuiConfigFlags_NoMouseCursorChange;

    device->GetImmediateContext(&context);

    DXGI_SWAP_CHAIN_DESC desc;
    swapChain->GetDesc(&desc);
    g_hwnd = desc.OutputWindow;

    ConsoleWrite("Installing WndProc hook...");
    if (!oWndProc)
    {
        oWndProc = (WNDPROC)SetWindowLongPtr(g_hwnd, GWLP_WNDPROC, (LONG_PTR)WndProcHook);
    }

    ID3D11Texture2D* backBuffer = nullptr;
    swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBuffer);
    if (backBuffer)
    {
        device->CreateRenderTargetView(backBuffer, nullptr, &rtv);
        backBuffer->Release();
    }

    ImGui_ImplWin32_Init(g_hwnd);
    ImGui_ImplDX11_Init(device, context);

    mIsSetup = true;
}

void ImGuiImpl::Update()
{
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    // @TODO: add some sort of processing for handling new imgui windows
    ImGui::ShowDemoWindow();

    if (ImGuiImpl::WantCaptureInput()) // not sure this really works - I think Dark Souls is also setting this constantly
    {
        while (ShowCursor(TRUE) < 0);
    }

    ImGui::Render();
    context->OMSetRenderTargets(1, &rtv, nullptr);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

void ImGuiImpl::Shutdown()
{
    if (mIsSetup)
    {
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        mIsSetup = false;
    }
}

bool ImGuiImpl::WantCaptureInput()
{
    if (mIsSetup)
    {
        ImGuiIO& io = ImGui::GetIO();
        return io.WantCaptureMouse || io.WantCaptureKeyboard || io.WantTextInput;
    }

    return false;
}
