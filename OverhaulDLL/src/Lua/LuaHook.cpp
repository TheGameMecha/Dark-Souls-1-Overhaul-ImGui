#include "LuaHook.h"

#include "MinHook.h"

#include "DarkSoulsOverhaulMod.h"

typedef int(*t_lua_gettop)(lua_State* L);
static t_lua_gettop* t_lua_gettop_FUNC = (t_lua_gettop*)0x140dc8330;
t_lua_gettop o_lua_gettop = nullptr;


typedef int(*t_lua_pcall)(lua_State* L, int nargs, int nresults, int errfunc);
static t_lua_pcall* t_lua_pcall_FUNC = (t_lua_pcall*)0x140dc8700;
t_lua_pcall o_lua_pcall = nullptr;


typedef const char* t_lua_tostring(lua_State* L, int idx);
static t_lua_tostring* t_lua_tostring_FUNC = (t_lua_tostring*)0x140dc9040;


static int MyLuaPCall(lua_State* L, int nargs, int nresults, int errfunc)
{
    const char* stringVal = t_lua_tostring_FUNC(L, -1);
    ConsoleWrite("LuaHook: %s", stringVal);
    return o_lua_pcall(L, nargs, nresults, errfunc);
}

DWORD WINAPI LuaHook::InitThread(LPVOID)
{
    if (MH_CreateHook(t_lua_pcall_FUNC, MyLuaPCall, (void**)&o_lua_pcall) != MH_OK || MH_EnableHook(t_lua_pcall_FUNC) != MH_OK)
    {
        ConsoleWrite("LuaHook: Failed to hook lua_pcall");
        return 1;
    }

    ConsoleWrite("LuaHook: Finished Setup without errors");

    return 0;
}

bool LuaHook::Initialize()
{
    ConsoleWrite("LuaHook: Initializing");
    CreateThread(nullptr, 0, InitThread, nullptr, 0, nullptr);
    return true;
}
