/*
    DARK SOULS OVERHAUL

    Contributors to this file:
        Ainsley Harriott  -  Reverse engineering, Anti-Cheat Identification
        metal-crow  -  Reverse engineering, Anti-Cheat Identification
*/
#pragma once

#ifndef ANTI_ANTI_CHEAT_H
#define ANTI_ANTI_CHEAT_H

#include <cstdint>
#include <tuple>

class AntiAntiCheat
{
public:
    static void start();

private:
    static const uint64_t game_hash_compare_checks[];
    static const uint64_t game_hash_compare_checks_alt[];
    static const std::tuple<uint64_t, uint8_t> game_runtime_hash_checks[];
    static const uint64_t game_write_playerdata_to_flatbuffer_injection_offset = 0xbd6ae6;
    static const uint64_t construct_flatbuffer_from_PlayerStatus_MemberFlags_injection_offset = 0xbd4c7c;
    static const uint64_t finish_construct_flatbuffer_from_PlayerStatus_MemberFlags_injection_offset = 0xbd8209;
    static const uint64_t set_MemberFlags_bitflag_offset = 0xbae8fe;
    static const uint64_t SendBuild_RequestUpdatePlayerStatus_GeneralRequestTask_alt2_injection_offset = 0xbde3ef;
};

#endif
