#include "ImGui.h"
#include "DarkSoulsOverhaulMod.h"

#include "MinHook.h"

static bool mInitialized;

ImGuiImpl::ImGuiImpl(){}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static ID3D11Device* device = nullptr;
static ID3D11DeviceContext* context = nullptr;
static ID3D11RenderTargetView* rtv = nullptr;
static HWND g_hwnd = nullptr;

WNDPROC oWndProc = nullptr;


LRESULT CALLBACK WndProcHook(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    // Let ImGui handle the message first
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam) && ImGuiImpl::WantCaptureInput())
    {
        return TRUE;
    }

    // Pass to original window proc
    return CallWindowProc(oWndProc, hwnd, msg, wParam, lParam);
}

std::wstring GetWindowTitle(HWND hwnd)
{
    wchar_t title[256] = { 0 };
    GetWindowTextW(hwnd, title, sizeof(title) / sizeof(wchar_t));
    return std::wstring(title);
}

// Convert wide string to UTF-8 string
static std::string WideToUTF8(const std::wstring& wstr)
{
    if (wstr.empty()) return {};

    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), nullptr, 0, nullptr, nullptr);
    std::string strTo(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, nullptr, nullptr);
    return strTo;
}

void ImGuiImpl::Initialize(IDXGISwapChain* swapChain, ID3D11Device* device)
{
    ConsoleWrite("Initializing ImGui");

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls

    device->GetImmediateContext(&context);

    DXGI_SWAP_CHAIN_DESC desc;
    swapChain->GetDesc(&desc);
    g_hwnd = desc.OutputWindow;

    std::wstring wTitle = GetWindowTitle(g_hwnd);
    std::string title = WideToUTF8(wTitle);
    ConsoleWrite("Window title: %s", title.c_str());

    ConsoleWrite("Installing WndProc hook...");
    if (!oWndProc)
    {
        oWndProc = (WNDPROC)SetWindowLongPtr(g_hwnd, GWLP_WNDPROC, (LONG_PTR)WndProcHook);
    }

    ID3D11Texture2D* backBuffer = nullptr;
    swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBuffer);
    if(backBuffer)
    {
        device->CreateRenderTargetView(backBuffer, nullptr, &rtv);
        backBuffer->Release();
    }

    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(g_hwnd);
    ImGui_ImplDX11_Init(device, context);

    mInitialized = true;
}

void ImGuiImpl::Update()
{
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    // todo: add some sort of processing for handling new imgui windows
    ImGui::ShowDemoWindow();

    ImGui::Render();
    context->OMSetRenderTargets(1, &rtv, nullptr);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

void ImGuiImpl::Shutdown()
{
    if(mInitialized)
    {
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        mInitialized = false;
    }
}

bool ImGuiImpl::WantCaptureInput()
{
    if(mInitialized)
    {
        ImGuiIO& io = ImGui::GetIO();
        if (io.WantCaptureMouse)
        {
            return true;
        }
    }

    return false;
}
