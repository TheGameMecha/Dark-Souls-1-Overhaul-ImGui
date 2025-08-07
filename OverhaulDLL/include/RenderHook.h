/*
    DARK SOULS OVERHAUL

    Contributors to this file:
        TheGameMecha    -  C++
*/

#pragma once

#ifndef RENDER_HOOK_H
#define RENDER_HOOK_H

class RenderHook {
public:
    static bool Initialize();

    static void Shutdown();
};
#endif // RENDER_HOOK_H
