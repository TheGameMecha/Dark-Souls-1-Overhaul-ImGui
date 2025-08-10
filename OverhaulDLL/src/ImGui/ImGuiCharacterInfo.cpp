#include "ImGuiCharacterInfo.h"

#include "PlayerInsStruct.h"
#include "ImGuiTypes.h"

#include <unordered_map>

PlayerIns* ImGuiCharacterInfo::mCurrentPlayerIns = nullptr;
bool ImGuiCharacterInfo::mIsOpen = false;

ChrManipulator_ActionInputted CurFrameActionInputs;
static std::unordered_map<std::string, InputHistory> g_InputHistory;
static int g_FrameCounter = 0;


static void BoolTextWithHistory(const char* label, bool value)
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
static void UpdateInputHistory(const char* name, bool isDown)
{
    auto& hist = g_InputHistory[name];
    if (isDown && !hist.wasDownLastFrame)
    {
        hist.lastPressedFrame = g_FrameCounter; // record moment it was pressed
    }
    hist.wasDownLastFrame = isDown;
}
#define UPDATE_INPUT(name) UpdateInputHistory(#name, CurFrameActionInputs.name)

void ImGuiCharacterInfo::UpdateAllInputHistory()
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

void ImGuiCharacterInfo::DrawInputDebugUI()
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

void ImGuiCharacterInfo::DrawWindow()
{
    if(mIsOpen)
    {
        if (ImGui::Begin("Action Input Debug##CharacterInfo", &mIsOpen))
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
}

void ImGuiCharacterInfo::ToggleWindow()
{
    mIsOpen = !mIsOpen;
}
