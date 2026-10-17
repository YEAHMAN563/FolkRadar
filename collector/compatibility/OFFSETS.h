#pragma once

// CS2 Memory Offsets
// Build 14184 - Generated from cs2-sdk.com
// Last Updated: 2026-10-17
// 
// These offsets are used to locate game data structures in memory.
// They MUST be updated after CS2 patches.
// Source: E:\webradar\offsets.hpp

#include <cstddef>
#include <cstdint>

namespace CS2Offsets {
    // Module: client.dll (Base addresses)
    namespace ClientDll {
        constexpr std::ptrdiff_t dwEntityList = 0x271A908;
        constexpr std::ptrdiff_t dwGameEntitySystem_highestEntityIndex = 0x2120;
        constexpr std::ptrdiff_t dwLocalPlayerController = 0x253CB78;
        constexpr std::ptrdiff_t dwLocalPlayerPawn = 0x255E658;
        constexpr std::ptrdiff_t dwViewMatrix = 0x256B020;
        constexpr std::ptrdiff_t dwViewAngles = 0x25741C8;
        constexpr std::ptrdiff_t dwGameRules = 0x25651D0;
        constexpr std::ptrdiff_t dwGlobalVars = 0x2229F88;
        constexpr std::ptrdiff_t dwPlantedC4 = 0x24C7380;
    }

    // Module: engine2.dll
    namespace Engine2Dll {
        constexpr std::ptrdiff_t dwBuildNumber = 0x61CFEC;      // 6410220 decimal
        constexpr std::ptrdiff_t dwNetworkGameClient = 0x91AFC0; // 9547712 decimal
    }

    // C_BaseEntity offsets (from client_dll.json)
    namespace Entity {
        constexpr std::ptrdiff_t m_pGameSceneNode = 0x330;      // 816
        constexpr std::ptrdiff_t m_iHealth = 0x34C;             // 844
        constexpr std::ptrdiff_t m_iTeamNum = 0x3E7;            // 999
        constexpr std::ptrdiff_t m_vecAbsVelocity = 0x3F8;      // 1016
        constexpr std::ptrdiff_t m_hOwnerEntity = 0x520;        // 1312
        constexpr std::ptrdiff_t m_iPawnArmor = 0x93C;          // 2364 (from CCSPlayerController)
        constexpr std::ptrdiff_t m_bPawnHasDefuser = 0x940;     // 2368 (from CCSPlayerController)
    }

    // CGameSceneNode offsets
    namespace SceneNode {
        constexpr std::ptrdiff_t m_vecAbsOrigin = 0xC8;         // 200
        constexpr std::ptrdiff_t m_angAbsRotation = 0xD4;       // 212
        constexpr std::ptrdiff_t m_vecOrigin = 0x80;            // 128
    }

    // CCSPlayerController offsets
    namespace PlayerController {
        constexpr std::ptrdiff_t m_hPlayerPawn = 0x92C;         // 2348
        constexpr std::ptrdiff_t m_iszPlayerName = 0x6FC;       // 1788 (from CBasePlayerController - m_iszPlayerName)
        constexpr std::ptrdiff_t m_bPawnIsAlive = 0x934;        // 2356
        constexpr std::ptrdiff_t m_iPawnHealth = 0x938;         // 2360
        constexpr std::ptrdiff_t m_iPawnArmor = 0x93C;          // 2364
        constexpr std::ptrdiff_t m_bPawnHasDefuser = 0x940;     // 2368
        constexpr std::ptrdiff_t m_iTeamNum = 0x3E7;            // 999 (from C_BaseEntity)
    }

    // C_CSPlayerPawn offsets
    namespace PlayerPawn {
        constexpr std::ptrdiff_t m_ArmorValue = 0x1ECC;         // 7884
        constexpr std::ptrdiff_t m_pWeaponServices = 0x12F0;    // 4848
        constexpr std::ptrdiff_t m_hActiveMinimapVolume = 0x1DA4;
        constexpr std::ptrdiff_t m_angEyeAngles = 0x35D0;
        constexpr std::ptrdiff_t m_iHealth = 0x34C;             // 844 (from C_BaseEntity)
        constexpr std::ptrdiff_t m_bIsScoped = 0x1E98;          // 7832
        constexpr std::ptrdiff_t m_bIsDefusing = 0x1E9A;        // 7834
        constexpr std::ptrdiff_t m_bIsGrabbingHostage = 0x1E9B; // 7835
    }

    namespace MinimapVolume {
        constexpr std::ptrdiff_t m_strMinimapName = 0x1180;
    }

    namespace WeaponServices {
        constexpr std::ptrdiff_t m_hMyWeapons = 0x48;
        constexpr std::ptrdiff_t m_hActiveWeapon = 0x60;
    }

    namespace Weapon {
        constexpr std::ptrdiff_t m_iItemDefinitionIndex = 0x149A; // m_AttributeManager.m_Item
    }

    // C_CSGameRules offsets
    namespace GameInfo {
        constexpr std::ptrdiff_t m_gamePhase = 0x84;            // 132
        constexpr std::ptrdiff_t m_totalRoundsPlayed = 0x88;    // 136
        constexpr std::ptrdiff_t m_bBombPlanted = 0x8C7;        // 2247
        constexpr std::ptrdiff_t m_vMinimapMins = 0xC20;
        constexpr std::ptrdiff_t m_vMinimapMaxs = 0xC2C;
        constexpr std::ptrdiff_t m_bFreezePeriod = 0x40;        // 64
        constexpr std::ptrdiff_t m_iRoundTime = 0x68;           // 104
        constexpr std::ptrdiff_t m_szMapName = 0x4B8;           // 1208 (estimated from m_szMatchStatTxt)
    }

    namespace PlantedC4 {
        constexpr std::ptrdiff_t m_nBombSite = 0x128C;
        constexpr std::ptrdiff_t m_flC4Blow = 0x12B8;
        constexpr std::ptrdiff_t m_flTimerLength = 0x12C0;
    }

    namespace GlobalVars {
        constexpr std::ptrdiff_t m_curtime = 0x30;
        constexpr std::ptrdiff_t m_mapName = 0x180;
    }

    // View matrix
    constexpr size_t VIEW_MATRIX_SIZE = 16; // 4x4 matrix (16 floats)
}
