#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <array>

/**
 * Normalized game data structures.
 * These structures are platform-independent and safe to serialize.
 */

constexpr size_t MAX_PLAYERS = 64;
constexpr size_t MAX_PLAYER_NAME = 128;

/**
 * 3D Vector
 */
struct Vector3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    Vector3() = default;
    Vector3(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}
};

/**
 * Player information
 */
struct PlayerInfo {
    uint32_t entityId = 0;
    uint64_t steamId = 0;
    char name[MAX_PLAYER_NAME] = {0};
    
    Vector3 position;
    Vector3 viewAngles;
    
    int32_t health = 0;
    int32_t armor = 0;
    int32_t team = 0;           // 2 = T, 3 = CT
    
    bool isAlive = false;
    bool hasDefuser = false;
    bool isLocalPlayer = false;
    
    uint32_t weaponId = 0;
    // Item definition indexes for weapons and equipment carried by this player.
    std::vector<uint32_t> inventory;
    
    // Screen position (calculated from world position)
    float screenX = 0.0f;
    float screenY = 0.0f;
    bool isOnScreen = false;
};

/**
 * Game state information
 */
struct GameState {
    enum class Phase {
        UNKNOWN = 0,
        WARMUP = 1,
        LIVE = 2,
        HALFTIME = 3,
        GAME_OVER = 4
    };

    Phase gamePhase = Phase::UNKNOWN;
    int32_t roundNumber = 0;
    int32_t scoreT = 0;
    int32_t scoreCT = 0;
    
    bool isFreezetime = false;
    bool isBombPlanted = false;
    int32_t bombSite = -1; // 0 = A, 1 = B
    
    Vector3 bombPosition;
    float bombTimeRemaining = 0.0f;
    Vector3 minimapMins;
    Vector3 minimapMaxs;
    
    char mapName[64] = {0};
};

/**
 * View matrix for world-to-screen projection
 */
struct ViewMatrix {
    float matrix[16] = {0};

    /**
     * Project 3D world position to 2D screen coordinates.
     * @param worldPos 3D position in world space
     * @param screenX Output X coordinate (0-1, where 0.5 is center)
     * @param screenY Output Y coordinate (0-1, where 0.5 is center)
     * @return true if position is in front of camera, false if behind
     */
    bool WorldToScreen(const Vector3& worldPos, float& screenX, float& screenY) const;
};

/**
 * Complete game snapshot
 */
struct GameSnapshot {
    uint64_t timestamp = 0;     // Timestamp in milliseconds
    uint32_t frameNumber = 0;
    
    GameState gameState;
    ViewMatrix viewMatrix;
    
    std::vector<PlayerInfo> players;
    
    uint32_t localPlayerIndex = 0;
    bool isValid = false;
    
    /**
     * Clear all data
     */
    void Clear() {
        timestamp = 0;
        frameNumber = 0;
        players.clear();
        localPlayerIndex = 0;
        isValid = false;
    }
};
