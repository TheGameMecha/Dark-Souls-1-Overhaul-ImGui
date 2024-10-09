#include "FileReloading.h"
#include "GameData.h"
#include "Files.h"
#include "SP/memory/injection/asm/x64.h"
#include "MainLoop.h"

#include <map>
#include <mutex>

const std::map<IndividualParams, const wchar_t*> IndividualParams_To_String
{
    {LIGHT_BANK, L"LIGHT_BANK"},
    {FOG_BANK, L"FOG_BANK"},
    {LIGHT_SCATTERING_BANK, L"LIGHT_SCATTERING_BANK"},
    {POINT_LIGHT_BANK, L"POINT_LIGHT_BANK"},
    {LENS_FLARE_BANK, L"LENS_FLARE_BANK"},
    {TONE_MAP_BANK, L"TONE_MAP_BANK"},
    {TONE_CORRECT_BANK, L"TONE_CORRECT_BANK"},
    {SHADOW_BANK, L"SHADOW_BANK"},
    {LENS_FLARE_EX_BANK, L"LENS_FLARE_EX_BANK"},
    {ENV_LIGHT_TEX_BANK, L"ENV_LIGHT_TEX_BANK"},
    {ENEMY_STANDARD_INFO_BANK, L"ENEMY_STANDARD_INFO_BANK"},
    {AI_STANDARD_INFO_BANK, L"AI_STANDARD_INFO_BANK"},
    {THROW_INFO_BANK, L"THROW_INFO_BANK"},
    {MISSILE_PARAM_ST, L"MISSILE_PARAM_ST"},
    {SoloParamMan_Loaded, L"SoloParamMan_Loaded"},
    {SoloParamMan, L"SoloParamMan"}
};

const std::map<IndividualSoloParams, const wchar_t*> IndividualSoloParams_To_String
{
    {EquipParamWeapon, L"EquipParamWeapon"},
    {EquipParamProtector, L"EquipParamProtector"},
    {EquipParamAccessory, L"EquipParamAccessory"},
    {EquipParamGoods, L"EquipParamGoods"},
    {ReinforceParamWeapon, L"ReinforceParamWeapon"},
    {ReinforceParamProtector, L"ReinforceParamProtector"},
    {NpcParam, L"NpcParam"},
    {AtkParam_Npc, L"AtkParam_Npc"},
    {AtkParam_Pc, L"AtkParam_Pc"},
    {NpcThinkParam, L"NpcThinkParam"},
    {ObjectParam, L"ObjectParam"},
    {Bullet, L"Bullet"},
    {BehaviorParam, L"BehaviorParam"},
    {BehaviorParam_PC, L"BehaviorParam_PC"},
    {Magic, L"Magic"},
    {SpEffectParam, L"SpEffectParam"},
    {SpEffectVfxParam, L"SpEffectVfxParam"},
    {TalkParam, L"TalkParam"},
    {MenuColorTableParam, L"MenuColorTableParam"},
    {ItemLotParam, L"ItemLotParam"},
    {MoveParam, L"MoveParam"},
    {CharaInitParam, L"CharaInitParam"},
    {EquipMtrlSetParam, L"EquipMtrlSetParam"},
    {FaceGenParam, L"FaceGenParam"},
    {RagdollParam, L"RagdollParam"},
    {ShopLineupParam, L"ShopLineupParam"},
    {QwcChange, L"QwcChange"},
    {QwcJudge, L"QwcJudge"},
    {GameAreaParam, L"GameAreaParam"},
    {SkeletonParam, L"SkeletonParam"},
    {CalcCorrectGraph, L"CalcCorrectGraph"},
    {LockCamParam, L"LockCamParam"},
    {ObjActParam, L"ObjActParam"},
    {HitMtrlParam, L"HitMtrlParam"},
    {KnockBackParam, L"KnockBackParam"},
    {LevelSyncParam, L"LevelSyncParam"},
    {CoolTimeParam, L"CoolTimeParam"},
    {WhiteCoolTimeParam, L"WhiteCoolTimeParam"},
    {All_Other_Params, L"All_Other_Params"}
};

const std::map<ParamBNDs, const wchar_t*> ParamBNDs_To_String
{
    {GameParam, L"param:/GameParam/GameParam.parambnd"},
    {a10_DrawParam, L"param:/DrawParam/a10_DrawParam.parambnd"},
    {a11_DrawParam, L"param:/DrawParam/a11_DrawParam.parambnd"},
    {a12_DrawParam, L"param:/DrawParam/a12_DrawParam.parambnd"},
    {a13_DrawParam, L"param:/DrawParam/a13_DrawParam.parambnd"},
    {a14_DrawParam, L"param:/DrawParam/a14_DrawParam.parambnd"},
    {a15_DrawParam, L"param:/DrawParam/a15_DrawParam.parambnd"},
    {a16_DrawParam, L"param:/DrawParam/a16_DrawParam.parambnd"},
    {a17_DrawParam, L"param:/DrawParam/a17_DrawParam.parambnd"},
    {a18_DrawParam, L"param:/DrawParam/a18_DrawParam.parambnd"},
    {a99_DrawParam, L"param:/DrawParam/a99_DrawParam.parambnd"},
    {default_DrawParam, L"param:/DrawParam/default_DrawParam.parambnd"},
};


/* --------------------------------------------------------------------- */

//need to add this due to a race condition with DSR accessing some PC data before the reload is finished
static std::mutex ReloadPlayer_mtx;
static std::mutex ReloadPlayer_locks_remaining_mtx;
static int ReloadPlayer_locks_remaining = -1;

void FileReloading::ReloadPlayer(ModMode currentmode, ModMode newmode)
{
    //avoid a player reload if not needed
    if (
        (currentmode == ModMode::Overhaul && newmode == ModMode::Legacy) ||
        (currentmode == ModMode::Overhaul && newmode == ModMode::Compatability) ||
        (currentmode == ModMode::Legacy && newmode == ModMode::Overhaul) ||
        (currentmode == ModMode::Compatability && newmode == ModMode::Overhaul)
        )
    {
        //we need to wait on unlocking this mutex until the player is fully reloaded
        ReloadPlayer_mtx.lock();

        ReloadPlayer_locks_remaining_mtx.lock();
        ReloadPlayer_locks_remaining = 2;
        ReloadPlayer_locks_remaining_mtx.unlock();

        // Set bPlayerReload flag to true
        *((uint8_t*)0x141c77e6b) = 1;
        // Call Force_PlayerReload
        Force_PlayerReload(*(void**)Game::world_chr_man_imp, L"c0000");
    }
}

/* --------------------------------------------------------------------- */

bool FileReloading::GameParamsLoaded = false;
bool CheckIfGameParamsLoaded(void* unused);

//we only call this on game start, so that if there's a race condition with accessing the param data while it's unloaded
//it can only happen once, on boot
//all other times we instantly switch between loaded params, so race conditions are eliminated
void FileReloading::LoadGameParam()
{
    Files::UseOverhaulFiles = true;

    UnloadIndividualParams({}, {
                                EquipParamWeapon,
                                EquipParamProtector,
                                EquipParamAccessory,
                                EquipParamGoods,
                                ReinforceParamWeapon,
                                ReinforceParamProtector,
                                NpcParam,
                                AtkParam_Npc,
                                AtkParam_Pc,
                                NpcThinkParam,
                                ObjectParam,
                                Bullet,
                                BehaviorParam,
                                BehaviorParam_PC,
                                Magic,
                                SpEffectParam,
                                SpEffectVfxParam,
                                TalkParam,
                                MenuColorTableParam,
                                ItemLotParam,
                                MoveParam,
                                CharaInitParam,
                                EquipMtrlSetParam,
                                FaceGenParam,
                                RagdollParam,
                                ShopLineupParam,
                                QwcChange,
                                QwcJudge,
                                GameAreaParam,
                                SkeletonParam,
                                CalcCorrectGraph,
                                LockCamParam,
                                ObjActParam,
                                HitMtrlParam,
                                KnockBackParam,
                                LevelSyncParam,
                                CoolTimeParam,
                                WhiteCoolTimeParam
        });

    ReloadParamFile(GameParam);

    MainLoop::setup_mainloop_callback(CheckIfGameParamsLoaded, NULL, "CheckIfGameParamsLoaded");
}

void UnloadIndividualParam(void* ParamMan_resCapArray, const wchar_t* param)
{
    void* param_res_cap = Find_ResCap(ParamMan_resCapArray, param);
    if (param_res_cap != NULL)
    {
        Unload_ResCap(ParamMan_resCapArray, param_res_cap);
    }
    else
    {
        std::string out;
        Files::string_wide_to_mb((wchar_t*)param, out);
        ConsoleWrite("Unable to find %s",out.c_str());
    }
}

/*
 * Given a list of IndividualParams or IndividualSoloParams, unload them
 */
void FileReloading::UnloadIndividualParams(std::vector<IndividualParams> i_params, std::vector<IndividualSoloParams> is_params)
{
    void* ParamMan_resCapArray = (void*)((*(uint64_t*)Game::param_man) + 8);

    for (auto param : i_params)
    {
        UnloadIndividualParam(ParamMan_resCapArray, IndividualParams_To_String.at(param));
    }
    for (auto param : is_params)
    {
        UnloadIndividualParam(ParamMan_resCapArray, IndividualSoloParams_To_String.at(param));
    }
}

/*
 * Given a paramBnd file, unload it from FileMan and reload it with the function call
 */
void FileReloading::ReloadParamFile(ParamBNDs paramfile)
{
    void* fileMan_FileCapArray_8 = (void*)((*(uint64_t*)Game::file_man) + 8);

    void* param_file_found = Find_ResCap(fileMan_FileCapArray_8, ParamBNDs_To_String.at(paramfile));
    if (param_file_found != NULL)
    {
        Unload_ResCap(fileMan_FileCapArray_8, param_file_found);
    }
    else
    {
        std::string out;
        Files::string_wide_to_mb((wchar_t*)ParamBNDs_To_String.at(paramfile), out);
        ConsoleWrite("Unable to find %s", out.c_str());
    }

    void* unused_arg = calloc(3, 8);
    ParambndFileCap_Load(ParamBNDs_To_String.at(paramfile), unused_arg, NULL, NULL);
}

// Set a flag once the game finishes loading the overhaul game params
bool CheckIfGameParamsLoaded(void* unused)
{
    int32_t containingRes_count = *(uint32_t*)((*(uint64_t*)Game::solo_param_man) + 0x10 + EquipParamWeapon * 72);
    if (containingRes_count == 1)
    {
        return true;
    }

    Files::UseOverhaulFiles = false;
    FileReloading::GameParamsLoaded = true;
    return false;
}

/* --------------------------------------------------------------------- */

bool UpdatePlayerStats(void* startTime)
{
    //check if we've waited long enough for the params to have loaded
    if (Game::get_frame_count() <= *(uint32_t*)startTime + 10)
    {
        return true;
    }

    //get the player stats and levels
    auto playerins_o = Game::get_PlayerIns();
    auto playergamedata_o = Game::get_host_player_gamedata();
    if (!playerins_o.has_value() || !playergamedata_o.has_value())
    {
        return true;
    }

    uint64_t playerins = (uint64_t)playerins_o.value();
    uint64_t playergamedata = (uint64_t)playergamedata_o.value();

    //update the player stats
    uint32_t vit = *(uint32_t*)(playergamedata + 0x40);
    uint32_t* baseMaxHp = (uint32_t*)(playergamedata + 0x1C);
    *baseMaxHp = Calculate_MaxHP_From_Vit(vit);

    uint32_t att = *(uint32_t*)(playergamedata + 0x48);
    uint32_t* baseMaxMp = (uint32_t*)(playergamedata + 0x28);
    *baseMaxMp = Calculate_MaxMP_From_Att(att);

    uint32_t end = *(uint32_t*)(playergamedata + 0x50);
    uint32_t* baseMaxSp = (uint32_t*)(playergamedata + 0x38);
    *baseMaxSp = Calculate_MaxSP_From_End(end);

    //TODO do i also need endurance, attunement, etc?

    //update the hp bar so it is scaled correctly
    Game::set_gui_hpbar_max();

    free(startTime);
    return false;
}

void FileReloading::RefreshPlayerStats()
{
    //update the character stats based off recalculated values from their level
    // (required since CalcCorrectGraph is refreshed)
    //need to wait 1 frame for the files to be reloaded
    uint32_t* startTime = (uint32_t*)malloc(4);
    *startTime = Game::get_frame_count();
    MainLoop::setup_mainloop_callback(UpdatePlayerStats, startTime, "UpdatePlayerStats");
}

/* --------------------------------------------------------------------- */

extern "C" {
    uint64_t CalcCorrectGraph_injection_return;
    void CalcCorrectGraph_injection();

    uint64_t get_ParamResCap_from_ParamMan_injection_return;
    void get_ParamResCap_from_ParamMan_injection();
    uint32_t SoloParamRes_curindex;

    uint64_t call_SetHostPlayerIns_offset_injection_return;
    void call_SetHostPlayerIns_offset_injection();
    void call_SetHostPlayerIns_offset_helper(bool lock);

    uint64_t reload_chrctrl_location1_injection_return;
    void reload_chrctrl_location1_injection();
    uint64_t reload_chrctrl_location2_injection_return;
    void reload_chrctrl_location2_injection();
    void reload_chrctrl_injection_helper();
}

void FileReloading::start()
{
    ConsoleWrite("Enabling File Reloading...");

    //injection to prevent the CalcCorrectGraph from crashing if it can't find the param (due to reloading, for example)
    uint8_t *write_address = (uint8_t*)(FileReloading::CalcCorrectGraph_injection_offset + Game::ds1_base);
    sp::mem::code::x64::inject_jmp_14b(write_address, &CalcCorrectGraph_injection_return, 4, &CalcCorrectGraph_injection);

    //injection to change what paramRes the game reads from. We'll have 2 loaded, and can just change which one we refer to
    write_address = (uint8_t*)(FileReloading::get_ParamResCap_from_ParamMan_injection_offset + Game::ds1_base);
    sp::mem::code::x64::inject_jmp_14b(write_address, &get_ParamResCap_from_ParamMan_injection_return, 2, &get_ParamResCap_from_ParamMan_injection);
    SoloParamRes_curindex = 0;

    //injection to check if we're currently reloading the character before the game accesses it (fixes race condition)
    write_address = (uint8_t*)(FileReloading::call_SetHostPlayerIns_offset + Game::ds1_base);
    sp::mem::code::x64::inject_jmp_14b(write_address, &call_SetHostPlayerIns_offset_injection_return, 6, &call_SetHostPlayerIns_offset_injection);

    //injections to determine when the character is actually done reloading, so the lock is held for the correct amount of time
    write_address = (uint8_t*)(FileReloading::reload_chrctrl_location1_offset + Game::ds1_base);
    sp::mem::code::x64::inject_jmp_14b(write_address, &reload_chrctrl_location1_injection_return, 0, &reload_chrctrl_location1_injection);
    write_address = (uint8_t*)(FileReloading::reload_chrctrl_location2_offset + Game::ds1_base);
    sp::mem::code::x64::inject_jmp_14b(write_address, &reload_chrctrl_location2_injection_return, 0, &reload_chrctrl_location2_injection);
}

void call_SetHostPlayerIns_offset_helper(bool lock)
{
    if (lock)
    {
        ReloadPlayer_mtx.lock();
    }
    else
    {
        ReloadPlayer_mtx.unlock();
    }
}

void reload_chrctrl_injection_helper()
{
    ReloadPlayer_locks_remaining_mtx.lock();
    if (ReloadPlayer_locks_remaining == 0)
    {
        ReloadPlayer_mtx.unlock();
        ReloadPlayer_locks_remaining = -1;
    }
    else if (ReloadPlayer_locks_remaining > 0)
    {
        ReloadPlayer_locks_remaining -= 1;
    }
    ReloadPlayer_locks_remaining_mtx.unlock();
}


void FileReloading::SetParamsToUse(bool legacy)
{
    if (legacy)
    {
        //the original files are always loaded first
        SoloParamRes_curindex = 0;
    }
    else
    {
        //the overhaul files are loaded after boot
        SoloParamRes_curindex = 1;
    }
}
