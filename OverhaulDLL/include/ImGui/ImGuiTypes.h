/*
    DARK SOULS OVERHAUL

    Contributors to this file:
        TheGameMecha    -  C++
*/

#pragma once

#ifndef IMGUI_TYPES_H
#define IMGUI_TYPES_H

#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_win32.h"
#include "imgui/backends/imgui_impl_dx11.h"
#include "MinHook.h"

#include <string>

struct InputHistory
{
    int lastPressedFrame = -1;   // Frame number when last pressed
    bool wasDownLastFrame = false;
};

#endif // IMGUI_TYPES_H
