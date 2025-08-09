#include "ImGui.h"
#include "DarkSoulsOverhaulMod.h"
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

ChrManipulator_ActionInputted CurFrameActionInputs;
static std::unordered_map<std::string, InputHistory> g_InputHistory;
static int g_FrameCounter = 0;

auto BoolText = [](const char* label, bool value)
    {
        ImGui::TextColored(value ? ImVec4(0.0f, 1.0f, 0.0f, 1.0f)  // Green if true
            : ImVec4(1.0f, 0.0f, 0.0f, 1.0f), // Red if false
            "%s: %s", label, value ? "true" : "false");
    };

void BoolTextWithHistory(const char* label, bool value)
{
    const auto& hist = g_InputHistory[label];
    ImVec4 col = value ? ImVec4(0, 1, 0, 1) : ImVec4(1, 0, 0, 1);

    if (hist.lastPressedFrame >= 0)
        ImGui::TextColored(col, "%s: %s (last pressed %d frames ago)",
            label, value ? "true" : "false",
            g_FrameCounter - hist.lastPressedFrame);
    else
        ImGui::TextColored(col, "%s: %s (never pressed)",
            label, value ? "true" : "false");
}

#define DRAW_INPUT(name) BoolTextWithHistory(#name, CurFrameActionInputs.name)
void UpdateInputHistory(const char* name, bool isDown)
{
    auto& hist = g_InputHistory[name];
    if (isDown && !hist.wasDownLastFrame)
    {
        hist.lastPressedFrame = g_FrameCounter; // record moment it was pressed
    }
    hist.wasDownLastFrame = isDown;
}
#define UPDATE_INPUT(name) UpdateInputHistory(#name, CurFrameActionInputs.name)

void UpdateAllInputHistory()
{
    // Attacks
    UPDATE_INPUT(r1_weapon_attack_input_1);
    UPDATE_INPUT(r1_weapon_attack_input_2);
    UPDATE_INPUT(l1_weapon_attack);
    UPDATE_INPUT(l2_weapon_attack);
    UPDATE_INPUT(lefthand_weapon_attack);
    UPDATE_INPUT(r1_magic_attack_input);
    UPDATE_INPUT(l1_magic_attack_input);
    UPDATE_INPUT(r2_input);

    // Defense / Utility
    UPDATE_INPUT(l1_input);
    UPDATE_INPUT(parry_input);
    UPDATE_INPUT(block_input);
    UPDATE_INPUT(use_ButtonPressed);

    // Movement
    UPDATE_INPUT(backstep_input);
    UPDATE_INPUT(roll_forward_input);
    UPDATE_INPUT(jump_input);

    // Emotes
    UPDATE_INPUT(beckon_emote_input);
    UPDATE_INPUT(point_forward_emote_input);
    UPDATE_INPUT(hurrah_emote_input);
    UPDATE_INPUT(bow_emote_input);
    UPDATE_INPUT(joy_emote_input);
    UPDATE_INPUT(wave_emote_input);
    UPDATE_INPUT(point_up_emote_input);
    UPDATE_INPUT(point_down_emote_input);
    UPDATE_INPUT(well_what_is_it_emote_input);
    UPDATE_INPUT(prostration_emote_input);
    UPDATE_INPUT(proper_bow_emote_input);
    UPDATE_INPUT(prayer_emote_input);
    UPDATE_INPUT(shrug_emote_input);
    UPDATE_INPUT(praise_the_sun_emote_input);
    UPDATE_INPUT(look_skyward_emote_input);

    // Unknowns
    UPDATE_INPUT(field4_0x4);
    UPDATE_INPUT(field6_0x6);
    UPDATE_INPUT(field8_0x8);
    UPDATE_INPUT(field9_0x9);
    UPDATE_INPUT(field11_0xb);
    UPDATE_INPUT(field12_0xc);
    UPDATE_INPUT(field13_0xd);
    UPDATE_INPUT(field16_0x10);
    UPDATE_INPUT(field17_0x11);
    UPDATE_INPUT(field18_0x12);
    UPDATE_INPUT(field37_0x25);
    UPDATE_INPUT(field38_0x26);
    UPDATE_INPUT(field39_0x27);
    UPDATE_INPUT(field40_0x28);
    UPDATE_INPUT(field41_0x29);
    UPDATE_INPUT(field43_0x2b);
    UPDATE_INPUT(field44_0x2c);
    UPDATE_INPUT(field45_0x2d);
    UPDATE_INPUT(field46_0x2e);
    UPDATE_INPUT(field47_0x2f);
    UPDATE_INPUT(field48_0x30);
    UPDATE_INPUT(field49_0x31);
    UPDATE_INPUT(field50_0x32);
}

void DrawInputDebugUI()
{
    // Attacks
    DRAW_INPUT(r1_weapon_attack_input_1);
    DRAW_INPUT(r1_weapon_attack_input_2);
    DRAW_INPUT(l1_weapon_attack);
    DRAW_INPUT(l2_weapon_attack);
    DRAW_INPUT(lefthand_weapon_attack);
    DRAW_INPUT(r1_magic_attack_input);
    DRAW_INPUT(l1_magic_attack_input);
    DRAW_INPUT(r2_input);

    // Defense / Utility
    DRAW_INPUT(l1_input);
    DRAW_INPUT(parry_input);
    DRAW_INPUT(block_input);
    DRAW_INPUT(use_ButtonPressed);

    // Movement
    DRAW_INPUT(backstep_input);
    DRAW_INPUT(roll_forward_input);
    DRAW_INPUT(jump_input);

    // Emotes
    DRAW_INPUT(beckon_emote_input);
    DRAW_INPUT(point_forward_emote_input);
    DRAW_INPUT(hurrah_emote_input);
    DRAW_INPUT(bow_emote_input);
    DRAW_INPUT(joy_emote_input);
    DRAW_INPUT(wave_emote_input);
    DRAW_INPUT(point_up_emote_input);
    DRAW_INPUT(point_down_emote_input);
    DRAW_INPUT(well_what_is_it_emote_input);
    DRAW_INPUT(prostration_emote_input);
    DRAW_INPUT(proper_bow_emote_input);
    DRAW_INPUT(prayer_emote_input);
    DRAW_INPUT(shrug_emote_input);
    DRAW_INPUT(praise_the_sun_emote_input);
    DRAW_INPUT(look_skyward_emote_input);

    // Unknowns
    DRAW_INPUT(field4_0x4);
    DRAW_INPUT(field6_0x6);
    DRAW_INPUT(field8_0x8);
    DRAW_INPUT(field9_0x9);
    DRAW_INPUT(field11_0xb);
    DRAW_INPUT(field12_0xc);
    DRAW_INPUT(field13_0xd);
    DRAW_INPUT(field16_0x10);
    DRAW_INPUT(field17_0x11);
    DRAW_INPUT(field18_0x12);
    DRAW_INPUT(field37_0x25);
    DRAW_INPUT(field38_0x26);
    DRAW_INPUT(field39_0x27);
    DRAW_INPUT(field40_0x28);
    DRAW_INPUT(field41_0x29);
    DRAW_INPUT(field43_0x2b);
    DRAW_INPUT(field44_0x2c);
    DRAW_INPUT(field45_0x2d);
    DRAW_INPUT(field46_0x2e);
    DRAW_INPUT(field47_0x2f);
    DRAW_INPUT(field48_0x30);
    DRAW_INPUT(field49_0x31);
    DRAW_INPUT(field50_0x32);
}

void ImGuiImpl::Update()
{
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    mIsVisible = true;
    bool okToEnterEquipmentScreen = false;

    if (Game::world_chr_man_imp)
    {
        auto playerIns_o = Game::get_PlayerIns();
        if (playerIns_o && playerIns_o != std::nullopt && playerIns_o.has_value())
        {
            mCurrentPlayerIns = (PlayerIns*)playerIns_o.value();
            if (mCurrentPlayerIns)
            {
                okToEnterEquipmentScreen = ok_to_enter_equipment_menu(mCurrentPlayerIns);
            }
        }
    }

    if (mIsVisible)
    {
        // @TODO: add some sort of processing for handling new imgui windows
        if (ImGui::Begin("DARK SOULS OVERHAUL MOD"))
        {
            ImGui::Text("MenuSys:               FrameTime: %f ms", FrpgMenuSysFrameTime);
            ImGui::Text("InGameMenus:           FrameTime: %f ms", InGameMenusFrameTime);
            ImGui::Text("MenuMan_And_MouseMan:  FrameTime: %f ms", MenuMan_And_MouseManFrameTime);
            ImGui::Text("OkToEnterEquipmentScreen: %s", okToEnterEquipmentScreen ? "true" : "false");
        }

        FrpgMenuSysFrameTime = 0.0f;
        InGameMenusFrameTime = 0.0f;
        MenuMan_And_MouseManFrameTime = 0.0f;

        ImGui::End();

        if (ImGui::Begin("Action Input Debug"))
        {
            if (!mCurrentPlayerIns)
            {
                ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Current Player Not Loaded");
                g_FrameCounter = -1;
                g_InputHistory.clear();
            }
            else
            {
                CurFrameActionInputs = mCurrentPlayerIns->chrins.padManipulator->chrManipulator.CurrentFrame_ActionInputs;

                UpdateAllInputHistory();
                DrawInputDebugUI();
                g_FrameCounter++;
            }
        }

        ImGui::End();
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
