#pragma once

#include "snapshot.h"
#include <string>

/**
 * Data Normalizer
 * Validates and normalizes raw game data into safe, serializable format.
 * Filters out invalid/corrupted data, applies sanity checks.
 */
class DataNormalizer {
public:
    /**
     * Normalize a player info structure.
     * Clamps values, validates ranges, sanitizes strings.
     */
    static void NormalizePlayer(PlayerInfo& player);

    /**
     * Normalize game state.
     * Validates enums, clamps values.
     */
    static void NormalizeGameState(GameState& state);

    /**
     * Validate entire snapshot.
     * @return true if snapshot passes validation, false otherwise
     */
    static bool ValidateSnapshot(const GameSnapshot& snapshot);

    /**
     * Calculate screen positions for all players in snapshot.
     * Uses view matrix to project world coordinates to screen space.
     */
    static void CalculateScreenPositions(GameSnapshot& snapshot);

    /**
     * Sanitize player name (remove control characters, ensure null termination)
     */
    static void SanitizeString(char* str, size_t maxLen);

    /**
     * Clamp value between min and max
     */
    template<typename T>
    static T Clamp(T value, T min, T max) {
        if (value < min) return min;
        if (value > max) return max;
        return value;
    }

private:
    // Validation limits
    static constexpr float MAX_COORDINATE = 100000.0f;
    static constexpr int32_t MAX_HEALTH = 500;
    static constexpr int32_t MAX_ARMOR = 100;
};

/**
 * JSON Serializer
 * Converts normalized snapshot to JSON format for IPC transmission.
 */
class JsonSerializer {
public:
    /**
     * Serialize snapshot to JSON string.
     * @param snapshot Normalized game snapshot
     * @return JSON string
     */
    static std::string SerializeSnapshot(const GameSnapshot& snapshot);

    /**
     * Serialize player info to JSON object string.
     */
    static std::string SerializePlayer(const PlayerInfo& player);

    /**
     * Serialize game state to JSON object string.
     */
    static std::string SerializeGameState(const GameState& state);

private:
    static std::string EscapeJsonString(const char* str);
    static std::string Vector3ToJson(const Vector3& vec);
};
