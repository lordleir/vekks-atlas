///----------------------------------------------------------------------------------------------------
/// Copyright (c) Raidcore.GG - Licensed under the MIT license.
///
/// Name         :  RTAPI.hpp
/// Description  :  Definitions for public-facing real-time API.
/// Authors      :  K. Bieniek
///----------------------------------------------------------------------------------------------------

#ifndef RTAPI_H
#define RTAPI_H

#define RTAPI_SIG                     0x2501A02C
#define DL_RTAPI                      "RTAPI"
#define EV_RTAPI_GROUP_MEMBER_JOINED  "RTAPI_GROUP_MEMBER_JOINED"
#define EV_RTAPI_GROUP_MEMBER_LEFT    "RTAPI_GROUP_MEMBER_LEFT"
#define EV_RTAPI_GROUP_MEMBER_UPDATED "RTAPI_GROUP_MEMBER_UPDATED"

#include <cstdint>

namespace RTAPI
{
    enum class EGameState : uint32_t { CharacterSelection, CharacterCreation, Cinematic, LoadingScreen, Gameplay };
    enum class EGameLanguage : uint32_t { English, Korean, French, German, Spanish, Chinese };
    enum class ETimeOfDay : uint32_t { Dawn, Day, Dusk, Night };
    enum class ECharacterState : uint32_t
    {
        IsAlive = 1 << 0, IsDowned = 1 << 1, IsInCombat = 1 << 2, IsSwimming = 1 << 3,
        IsUnderwater = 1 << 4, IsGliding = 1 << 5, IsFlying = 1 << 6
    };
    enum class EMapType : uint32_t
    {
        AutoRedirect, CharacterCreation, PvP, GvG, Instance, Public, Tournament, Tutorial,
        UserTournament, WvW_EternalBattlegrounds, WvW_BlueBorderlands, WvW_GreenBorderlands,
        WvW_RedBorderlands, WVW_FortunesVale, WvW_ObsidianSanctum, WvW_EdgeOfTheMists,
        Public_Mini, BigBattle, WvW_Lounge
    };
    enum class EGroupType : uint32_t { None, Party, RaidSquad, Squad };

    struct GroupMember
    {
        char AccountName[140];
        char CharacterName[140];
        uint32_t Subgroup;
        uint32_t Profession;
        uint32_t EliteSpecialization;
        uint32_t IsSelf : 1;
        uint32_t IsInInstance : 1;
        uint32_t IsCommander : 1;
        uint32_t IsLieutenant : 1;
    };

    struct RealTimeData
    {
        uint32_t GameBuild;
        EGameState GameState;
        EGameLanguage Language;
        ETimeOfDay TimeOfDay;
        uint32_t MapID;
        EMapType MapType;
        uint8_t IPAddress[4];
        float Cursor[3];
        float SquadMarkers[8][3];
        EGroupType GroupType;
        uint32_t GroupMemberCount;
        uint32_t RESERVED2;
        uint32_t RESERVED1;
        char AccountName[140];
        char CharacterName[140];
        float CharacterPosition[3];
        float CharacterFacing[3];
        uint32_t Profession;
        uint32_t EliteSpecialization;
        uint32_t MountIndex;
        ECharacterState CharacterState;
        float CameraPosition[3];
        float CameraFacing[3];
        float CameraFOV;
        uint32_t IsActionCamera : 1;
        uint32_t CharacterLevel;
        uint32_t CharacterEffectiveLevel;
    };
}

#endif
