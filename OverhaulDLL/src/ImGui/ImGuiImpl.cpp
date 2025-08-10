#include "ImGuiImpl.h"
#include "DarkSoulsOverhaulMod.h"

#include "ImGuiCharacterInfo.h"

#include "PlayerInsStruct.h"
#include <thread>
#include <chrono>

ImGuiImpl::ImGuiImpl() {}

bool ImGuiImpl::mIsSetup = false;
bool ImGuiImpl::mIsVisible = false;

// D3D
ID3D11DeviceContext* ImGuiImpl::context = nullptr;
ID3D11RenderTargetView* ImGuiImpl::rtv = nullptr;
PresentFn ImGuiImpl::oPresent = nullptr;

// Windows
HWND ImGuiImpl::g_hwnd = nullptr;
WNDPROC ImGuiImpl::oWndProc = nullptr;

// Game Specific
PlayerIns* ImGuiImpl::mCurrentPlayerIns = nullptr;


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

    ImGuiImpl::DrawWindow();
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

static Step_FrpgMenuSys_FUNC* oStep_FrpgMenuSys = nullptr;
static float FrpgMenuSysFrameTime = 0.0f;

static void HookedStep_FrpgMenuSys(void* frpgmenusys, float frametime)
{
    FrpgMenuSysFrameTime = frametime;
    oStep_FrpgMenuSys(frpgmenusys, frametime);
}

static Step_InGameMenus_FUNC* oStep_InGameMenus = nullptr;
static float InGameMenusFrameTime = 0.0f;

static void HookedStep_InGameMenus(void* InGameMenuStep, float frametime, void* TaskItem)
{
    InGameMenusFrameTime = frametime;
    oStep_InGameMenus(InGameMenuStep, frametime, TaskItem);
}

static Step_MenuMan_And_MouseMan_FUNC* oStep_MenuMan_And_MouseMan = nullptr;
static float MenuMan_And_MouseManFrameTime = 0.0f;

static void HookedStep_MenuMan_And_MouseMan(float frametime)
{
    MenuMan_And_MouseManFrameTime = frametime;
    oStep_MenuMan_And_MouseMan(frametime);
}

DWORD ImGuiImpl::HandleGameHooks()
{
    std::vector<HookInfo> hooks =
    {
        { reinterpret_cast<void*>(Step_FrpgMenuSys), HookedStep_FrpgMenuSys, reinterpret_cast<void**>(&oStep_FrpgMenuSys), "Step_FrpgMenuSys"},
        { reinterpret_cast<void*>(Step_InGameMenus), HookedStep_InGameMenus, reinterpret_cast<void**>(&oStep_InGameMenus), "Step_InGameMenus"},
        { reinterpret_cast<void*>(Step_MenuMan_And_MouseMan), HookedStep_MenuMan_And_MouseMan, reinterpret_cast<void**>(&oStep_MenuMan_And_MouseMan), "Step_MenuMan_And_MouseMan"},
    };

    if (!HookFunctions(hooks)) {
        return 1;
    }
    return 0;
}

bool ImGuiImpl::HookFunctions(const std::vector<HookInfo>& hooks)
{
    for (auto& h : hooks) {
        if (MH_CreateHook(h.target, h.detour, h.original) != MH_OK)
        {
            ConsoleWrite("Failed to create hook: %s", h.name);
            return false;
        }
        if (MH_EnableHook(h.target) != MH_OK) {
            ConsoleWrite("Failed to enable hook: %s", h.name);
            return false;
        }
        ConsoleWrite("Hook installed: %s", h.name);
    }
    return true;
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

    if (HandleGameHooks() != 0)
    {
        ConsoleWrite("ImGui: GameHook setup failed");
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
    ConsoleWrite("ImGui: Performing Setup");

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

void ImGuiImpl::DrawWindow()
{
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    mIsVisible = true;

    if (mIsVisible)
    {
        if (ImGui::BeginMainMenuBar())
        {
            static bool frameTimeOpen = false;

            if (ImGui::BeginMenu("Debug##MainMenu"))
            {
                if (ImGui::MenuItem("Frame Time"))
                {
                    frameTimeOpen = !frameTimeOpen;
                }

                if (ImGui::MenuItem("Character Info"))
                {
                    ImGuiCharacterInfo::ToggleWindow();
                }

                ImGui::EndMenu();
            }

            ImGuiCharacterInfo::DrawWindow();

            if(frameTimeOpen)
            {
                if (ImGui::Begin("Frame Time##FrameTime", &frameTimeOpen))
                {
                    ImGui::Text("MenuSys:               FrameTime: %f ms", FrpgMenuSysFrameTime);
                    ImGui::Text("InGameMenus:           FrameTime: %f ms", InGameMenusFrameTime);
                    ImGui::Text("MenuMan_And_MouseMan:  FrameTime: %f ms", MenuMan_And_MouseManFrameTime);
                }

                FrpgMenuSysFrameTime = 0.0f;
                InGameMenusFrameTime = 0.0f;
                MenuMan_And_MouseManFrameTime = 0.0f;

                ImGui::End();
            }

            ImGui::EndMainMenuBar();
        }
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
    if (mIsSetup && mIsVisible)
    {
        ImGuiIO& io = ImGui::GetIO();
        return io.WantCaptureMouse || io.WantCaptureKeyboard || io.WantTextInput;
    }

    return false;
}

void ImGuiImpl::ToggleVisibility()
{
    mIsVisible = !mIsVisible;
}

void ImGuiImpl::SetVisibility(bool newVisibility)
{
    mIsVisible = newVisibility;
}
