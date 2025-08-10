/*
    DARK SOULS OVERHAUL

    Contributors to this file:
        TheGameMecha    -  C++
*/

#pragma once

#ifndef IMGUI_CHARACTER_INFO_H
#define IMGUI_CHARACTER_INFO_H

struct PlayerIns;

class ImGuiCharacterInfo
{
public:
    ImGuiCharacterInfo(){};

    static void DrawWindow();
    static void ToggleWindow();

private:
    static PlayerIns* mCurrentPlayerIns;
    static bool mIsOpen;
};

#endif // IMGUI_CHARACTER_INFO_H
