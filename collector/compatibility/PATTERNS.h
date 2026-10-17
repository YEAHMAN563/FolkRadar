#pragma once

// CS2 Memory Patterns for Signature Scanning
// Generated from: https://github.com/a2x/cs2-dumper
// 
// These patterns are used to dynamically find offsets when they change after game updates.
// Patterns are byte sequences with wildcards (?) for variable bytes.
//
// Format: "AA BB ? ? CC DD" where ? represents wildcard bytes

#include <string>
#include <cstdint>

namespace CS2Patterns {
    // client.dll patterns
    namespace ClientDll {
        // EntityList pattern
        constexpr const char* dwEntityList = "48 8B 0D ? ? ? ? 48 89 7C 24 ? 8B FA";
        
        // LocalPlayerController pattern
        constexpr const char* dwLocalPlayerController = "48 8B 05 ? ? ? ? 48 85 C0 74 4F";
        
        // LocalPlayerPawn pattern  
        constexpr const char* dwLocalPlayerPawn = "48 8D 05 ? ? ? ? C3 CC CC CC CC CC CC CC CC 48 83 EC 28";
        
        // ViewMatrix pattern
        constexpr const char* dwViewMatrix = "48 8D 0D ? ? ? ? 48 C1 E0 06";
        
        // GameRules pattern
        constexpr const char* dwGameRules = "48 8B 0D ? ? ? ? 48 85 C9 0F 84 ? ? ? ? 8B 91";
    }

    // Pattern scanning helper structure
    struct Pattern {
        const char* name;
        const char* module;
        const char* pattern;
        std::ptrdiff_t offset;
        bool isRelative;
        int relativeOffset;
    };

    // Predefined patterns for scanning
    constexpr Pattern PATTERNS[] = {
        {"dwEntityList", "client.dll", ClientDll::dwEntityList, 0, true, 3},
        {"dwLocalPlayerController", "client.dll", ClientDll::dwLocalPlayerController, 0, true, 3},
        {"dwLocalPlayerPawn", "client.dll", ClientDll::dwLocalPlayerPawn, 0, true, 3},
        {"dwViewMatrix", "client.dll", ClientDll::dwViewMatrix, 0, true, 3},
        {"dwGameRules", "client.dll", ClientDll::dwGameRules, 0, true, 3},
    };
}

// Helper function declarations for pattern scanning
namespace PatternScanner {
    /**
     * Convert pattern string to byte array with mask
     * Example: "AA BB ? ? CC" -> bytes={0xAA, 0xBB, 0x00, 0x00, 0xCC}, mask="11001"
     */
    bool ParsePattern(const char* pattern, uint8_t* bytes, char* mask, size_t& length);
    
    /**
     * Scan memory region for pattern
     * Returns: Address of first match, or 0 if not found
     */
    uintptr_t ScanPattern(uintptr_t baseAddress, size_t regionSize, 
                          const uint8_t* pattern, const char* mask, size_t patternLength);
    
    /**
     * Resolve relative address from pattern match
     * For patterns like "48 8B 0D ? ? ? ?" where bytes at offset are RVA
     */
    uintptr_t ResolveRelativeAddress(uintptr_t matchAddress, int offset, int instructionLength);
}
