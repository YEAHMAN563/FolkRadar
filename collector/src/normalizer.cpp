#include "../include/normalizer.h"
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <cmath>

// DataNormalizer implementation

void DataNormalizer::NormalizePlayer(PlayerInfo& player) {
    // Clamp health
    player.health = Clamp(player.health, 0, MAX_HEALTH);
    
    // Clamp armor
    player.armor = Clamp(player.armor, 0, MAX_ARMOR);
    
    // Clamp team
    player.team = Clamp(player.team, 0, 3);
    
    // Clamp position coordinates
    player.position.x = Clamp(player.position.x, -MAX_COORDINATE, MAX_COORDINATE);
    player.position.y = Clamp(player.position.y, -MAX_COORDINATE, MAX_COORDINATE);
    player.position.z = Clamp(player.position.z, -MAX_COORDINATE, MAX_COORDINATE);
    
    // Sanitize name
    SanitizeString(player.name, MAX_PLAYER_NAME);
    
    // Validate alive state vs health
    if (player.health <= 0) {
        player.isAlive = false;
    }
}

void DataNormalizer::NormalizeGameState(GameState& state) {
    // Clamp round number
    state.roundNumber = Clamp(state.roundNumber, 0, 999);
    
    // Clamp scores
    state.scoreT = Clamp(state.scoreT, 0, 999);
    state.scoreCT = Clamp(state.scoreCT, 0, 999);
    
    // Clamp bomb time
    state.bombTimeRemaining = Clamp(state.bombTimeRemaining, 0.0f, 60.0f);
    
    // Sanitize map name
    SanitizeString(state.mapName, sizeof(state.mapName));
}

bool DataNormalizer::ValidateSnapshot(const GameSnapshot& snapshot) {
    // Check timestamp is reasonable
    if (snapshot.timestamp == 0) {
        return false;
    }
    
    // Check player count is reasonable
    if (snapshot.players.size() > MAX_PLAYERS) {
        return false;
    }
    
    // Validate each player
    for (const auto& player : snapshot.players) {
        // Check team is valid
        if (player.team < 0 || player.team > 3) {
            return false;
        }
        
        // Check position is not NaN or infinite
        if (std::isnan(player.position.x) || std::isnan(player.position.y) || std::isnan(player.position.z)) {
            return false;
        }
        
        if (std::isinf(player.position.x) || std::isinf(player.position.y) || std::isinf(player.position.z)) {
            return false;
        }
    }
    
    return true;
}

void DataNormalizer::CalculateScreenPositions(GameSnapshot& snapshot) {
    for (auto& player : snapshot.players) {
        player.isOnScreen = snapshot.viewMatrix.WorldToScreen(
            player.position,
            player.screenX,
            player.screenY
        );

        // WorldToScreen intentionally returns false for points outside the
        // viewport, but still leaves the projected coordinates available for
        // diagnostics. The wire format treats screen coordinates as bounded
        // UI values, so keep them inside that contract when off-screen.
        if (!player.isOnScreen ||
            !std::isfinite(player.screenX) ||
            !std::isfinite(player.screenY)) {
            player.screenX = 0.0f;
            player.screenY = 0.0f;
        } else {
            player.screenX = Clamp(player.screenX, 0.0f, 1.0f);
            player.screenY = Clamp(player.screenY, 0.0f, 1.0f);
        }
    }
}

void DataNormalizer::SanitizeString(char* str, size_t maxLen) {
    if (!str || maxLen == 0) {
        return;
    }
    
    // Ensure null termination
    str[maxLen - 1] = '\0';
    
    // Remove control characters
    for (size_t i = 0; i < maxLen && str[i] != '\0'; i++) {
        if (str[i] < 32 || str[i] == 127) {
            str[i] = ' ';
        }
    }
}

// JsonSerializer implementation

std::string JsonSerializer::SerializeSnapshot(const GameSnapshot& snapshot) {
    std::ostringstream json;
    json << "{";
    
    // Timestamp
    json << "\"timestamp\":" << snapshot.timestamp << ",";
    json << "\"frameNumber\":" << snapshot.frameNumber << ",";
    json << "\"isValid\":" << (snapshot.isValid ? "true" : "false") << ",";
    
    // Game state
    json << "\"gameState\":" << SerializeGameState(snapshot.gameState) << ",";
    
    // View matrix
    json << "\"viewMatrix\":[";
    for (int i = 0; i < 16; i++) {
        json << std::fixed << std::setprecision(6) << snapshot.viewMatrix.matrix[i];
        if (i < 15) json << ",";
    }
    json << "],";
    
    // Players
    json << "\"players\":[";
    for (size_t i = 0; i < snapshot.players.size(); i++) {
        json << SerializePlayer(snapshot.players[i]);
        if (i < snapshot.players.size() - 1) {
            json << ",";
        }
    }
    json << "],";
    
    json << "\"localPlayerIndex\":" << snapshot.localPlayerIndex;
    
    json << "}";
    
    return json.str();
}

std::string JsonSerializer::SerializePlayer(const PlayerInfo& player) {
    std::ostringstream json;
    json << "{";
    
    json << "\"entityId\":" << player.entityId << ",";
    json << "\"steamId\":" << player.steamId << ",";
    json << "\"name\":\"" << EscapeJsonString(player.name) << "\",";
    json << "\"position\":" << Vector3ToJson(player.position) << ",";
    json << "\"viewAngles\":" << Vector3ToJson(player.viewAngles) << ",";
    json << "\"health\":" << player.health << ",";
    json << "\"armor\":" << player.armor << ",";
    json << "\"team\":" << player.team << ",";
    json << "\"isAlive\":" << (player.isAlive ? "true" : "false") << ",";
    json << "\"hasDefuser\":" << (player.hasDefuser ? "true" : "false") << ",";
    json << "\"isLocalPlayer\":" << (player.isLocalPlayer ? "true" : "false") << ",";
    json << "\"weaponId\":" << player.weaponId << ",";
    json << "\"inventory\":[";
    for (size_t i = 0; i < player.inventory.size(); ++i) {
        json << player.inventory[i];
        if (i + 1 < player.inventory.size()) json << ",";
    }
    json << "],";
    json << "\"screenX\":" << std::fixed << std::setprecision(4) << player.screenX << ",";
    json << "\"screenY\":" << std::fixed << std::setprecision(4) << player.screenY << ",";
    json << "\"isOnScreen\":" << (player.isOnScreen ? "true" : "false");
    
    json << "}";
    return json.str();
}

std::string JsonSerializer::SerializeGameState(const GameState& state) {
    std::ostringstream json;
    json << "{";
    
    json << "\"gamePhase\":" << static_cast<int>(state.gamePhase) << ",";
    json << "\"roundNumber\":" << state.roundNumber << ",";
    json << "\"scoreT\":" << state.scoreT << ",";
    json << "\"scoreCT\":" << state.scoreCT << ",";
    json << "\"isFreezetime\":" << (state.isFreezetime ? "true" : "false") << ",";
    json << "\"isBombPlanted\":" << (state.isBombPlanted ? "true" : "false") << ",";
    json << "\"bombSite\":" << state.bombSite << ",";
    json << "\"bombPosition\":" << Vector3ToJson(state.bombPosition) << ",";
    json << "\"bombTimeRemaining\":" << std::fixed << std::setprecision(2) << state.bombTimeRemaining << ",";
    json << "\"minimapMins\":" << Vector3ToJson(state.minimapMins) << ",";
    json << "\"minimapMaxs\":" << Vector3ToJson(state.minimapMaxs) << ",";
    json << "\"mapName\":\"" << EscapeJsonString(state.mapName) << "\"";
    
    json << "}";
    return json.str();
}

std::string JsonSerializer::EscapeJsonString(const char* str) {
    if (!str) {
        return "";
    }
    
    std::string result;
    for (const char* p = str; *p; p++) {
        switch (*p) {
            case '\"': result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\b': result += "\\b"; break;
            case '\f': result += "\\f"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default:
                if (*p < 32) {
                    // Control character - skip or encode
                    result += ' ';
                } else {
                    result += *p;
                }
        }
    }
    return result;
}

std::string JsonSerializer::Vector3ToJson(const Vector3& vec) {
    std::ostringstream json;
    json << "{\"x\":" << std::fixed << std::setprecision(2) << vec.x
         << ",\"y\":" << vec.y
         << ",\"z\":" << vec.z << "}";
    return json.str();
}

// ViewMatrix WorldToScreen implementation
bool ViewMatrix::WorldToScreen(const Vector3& worldPos, float& screenX, float& screenY) const {
    // 4x4 view-projection matrix transform
    float w = matrix[12] * worldPos.x + matrix[13] * worldPos.y + matrix[14] * worldPos.z + matrix[15];
    
    // Check if point is behind camera
    if (w < 0.001f) {
        return false;
    }
    
    float x = matrix[0] * worldPos.x + matrix[1] * worldPos.y + matrix[2] * worldPos.z + matrix[3];
    float y = matrix[4] * worldPos.x + matrix[5] * worldPos.y + matrix[6] * worldPos.z + matrix[7];
    
    // Perspective divide
    float invW = 1.0f / w;
    x *= invW;
    y *= invW;
    
    // Convert from NDC [-1, 1] to screen [0, 1]
    screenX = (x + 1.0f) * 0.5f;
    screenY = 1.0f - (y + 1.0f) * 0.5f; // Flip Y (OpenGL to screen coords)
    
    // Check if on screen
    if (screenX < 0.0f || screenX > 1.0f || screenY < 0.0f || screenY > 1.0f) {
        return false;
    }
    
    return true;
}
