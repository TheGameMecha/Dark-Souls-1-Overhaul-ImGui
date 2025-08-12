/*
    DARK SOULS OVERHAUL

    Contributors to this file:
        TheGameMecha    -  C++
*/

#pragma once

#ifndef LUA_HOOK_H
#define LUA_HOOK_H

#include "d3d11/main.h"

#include "lstate.h"
#include "llimits.h"
#include "lobject.h"
#include "lua.h"
#include <cstdint>

class LuaHook
{
public:
    LuaHook() {};

    static bool Initialize();

protected:
    static DWORD WINAPI InitThread(LPVOID);

private:

};

#endif // LUA_HOOK_H
