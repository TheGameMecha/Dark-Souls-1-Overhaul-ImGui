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

class ImGuiImpl
{
public:
    ImGuiImpl();

    static void Initialize(IDXGISwapChain* swapChain, ID3D11Device* device);
    static void Update();
    static void Shutdown();

    static bool WantCaptureInput();

};

#endif // IMGUI_H
