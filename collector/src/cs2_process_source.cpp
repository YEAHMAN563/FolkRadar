#include "../include/data_source.h"
#include "../compatibility/OFFSETS.h"
#include "../compatibility/PATTERNS.h"
#include <Windows.h>
#include <TlHelp32.h>
#include <Psapi.h>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <sstream>
#include <vector>

using namespace CS2Offsets;

namespace {

std::vector<int> ParseSignature(const char* signature) {
    std::vector<int> bytes;
    std::istringstream stream(signature);
    std::string token;

    while (stream >> token) {
        if (token == "?" || token == "??") {
            bytes.push_back(-1);
        } else {
            bytes.push_back(static_cast<int>(std::stoul(token, nullptr, 16)));
        }
    }

    return bytes;
}

uintptr_t ResolveRipRelativeSignature(
    const std::vector<uint8_t>& moduleImage,
    uintptr_t moduleBase,
    const char* signature,
    size_t displacementOffset,
    size_t instructionLength) {
    const std::vector<int> pattern = ParseSignature(signature);
    if (pattern.empty() || moduleImage.size() < pattern.size()) {
        return 0;
    }

    for (size_t i = 0; i <= moduleImage.size() - pattern.size(); ++i) {
        bool matches = true;
        for (size_t j = 0; j < pattern.size(); ++j) {
            if (pattern[j] >= 0 && moduleImage[i + j] != static_cast<uint8_t>(pattern[j])) {
                matches = false;
                break;
            }
        }

        if (!matches || i + displacementOffset + sizeof(int32_t) > moduleImage.size()) {
            continue;
        }

        int32_t displacement = 0;
        memcpy(&displacement, moduleImage.data() + i + displacementOffset, sizeof(displacement));
        return moduleBase + i + instructionLength + displacement;
    }

    return 0;
}

} // namespace

/**
 * CS2ProcessDataSource Implementation
 * External process memory reader - NO INJECTION, NO HOOKS, NO WRITES
 */

CS2ProcessDataSource::CS2ProcessDataSource() 
    : m_processId(0), m_processHandle(nullptr), m_clientBase(0), m_engine2Base(0) {
}

CS2ProcessDataSource::~CS2ProcessDataSource() {
    Shutdown();
}

bool CS2ProcessDataSource::Initialize() {
    if (m_initialized) {
        return true;
    }

    std::cout << "[CS2ProcessDataSource] Initializing..." << std::endl;

    // Find CS2 process
    if (!FindCS2Process()) {
        std::cerr << "[CS2ProcessDataSource] Failed to find CS2 process (cs2.exe)" << std::endl;
        return false;
    }

    std::cout << "[CS2ProcessDataSource] Found CS2 process (PID: " << m_processId << ")" << std::endl;

    // Open process with read-only access
    m_processHandle = OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, FALSE, m_processId);
    if (!m_processHandle) {
        std::cerr << "[CS2ProcessDataSource] Failed to open process. Error: " << GetLastError() << std::endl;
        return false;
    }

    std::cout << "[CS2ProcessDataSource] Opened process handle" << std::endl;

    // Get module base addresses
    if (!GetModuleBase(L"client.dll", m_clientBase)) {
        std::cerr << "[CS2ProcessDataSource] Failed to find client.dll" << std::endl;
        Shutdown();
        return false;
    }

    if (!GetModuleBase(L"engine2.dll", m_engine2Base)) {
        std::cerr << "[CS2ProcessDataSource] Failed to find engine2.dll" << std::endl;
        Shutdown();
        return false;
    }

    std::cout << "[CS2ProcessDataSource] client.dll base: 0x" << std::hex << m_clientBase << std::dec << std::endl;
    std::cout << "[CS2ProcessDataSource] engine2.dll base: 0x" << std::hex << m_engine2Base << std::dec << std::endl;

    // Resolve important addresses
    if (!ResolveOffsets()) {
        std::cerr << "[CS2ProcessDataSource] Failed to resolve offsets" << std::endl;
        Shutdown();
        return false;
    }

    m_initialized = true;
    std::cout << "[CS2ProcessDataSource] Initialization complete" << std::endl;
    return true;
}

bool CS2ProcessDataSource::IsValid() const {
    if (!m_initialized || !m_processHandle) {
        return false;
    }

    // Check if process is still running
    DWORD exitCode;
    if (!GetExitCodeProcess(m_processHandle, &exitCode)) {
        return false;
    }

    return exitCode == STILL_ACTIVE;
}

bool CS2ProcessDataSource::CaptureSnapshot(GameSnapshot& outSnapshot) {
    if (!IsValid()) {
        return false;
    }

    outSnapshot.Clear();
    outSnapshot.timestamp = GetTickCount64();
    outSnapshot.frameNumber++;

    // Read view matrix
    if (!ReadViewMatrix(outSnapshot.viewMatrix.matrix)) {
        std::cerr << "[CS2ProcessDataSource] Failed to read view matrix" << std::endl;
        return false;
    }

    // Read entities
    if (!ReadEntityList(outSnapshot.players)) {
        std::cerr << "[CS2ProcessDataSource] Failed to read entity list" << std::endl;
        return false;
    }

    // Read game state after entities so the local pawn can provide the exact
    // active minimap volume name for automatic map selection.
    if (!ReadGameState(outSnapshot.gameState)) {
        std::cerr << "[CS2ProcessDataSource] Failed to read game state" << std::endl;
    }

    // If no players found, something is wrong
    if (outSnapshot.players.empty()) {
        std::cerr << "[CS2ProcessDataSource] No players found in entity list" << std::endl;
        return false;
    }

    outSnapshot.isValid = true;
    return true;
}

std::string CS2ProcessDataSource::GetStatusString() const {
    if (!m_initialized) {
        return "Not initialized";
    }
    if (!IsValid()) {
        return "Process not found or terminated";
    }
    return "Connected to CS2 (PID: " + std::to_string(m_processId) + ")";
}

void CS2ProcessDataSource::Shutdown() {
    if (m_processHandle) {
        CloseHandle(m_processHandle);
        m_processHandle = nullptr;
    }
    m_initialized = false;
    m_processId = 0;
    std::cout << "[CS2ProcessDataSource] Shutdown complete" << std::endl;
}

// Helper methods

bool CS2ProcessDataSource::FindCS2Process() {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return false;
    }

    PROCESSENTRY32W pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32W);

    if (Process32FirstW(snapshot, &pe32)) {
        do {
            if (wcscmp(pe32.szExeFile, L"cs2.exe") == 0) {
                m_processId = pe32.th32ProcessID;
                CloseHandle(snapshot);
                return true;
            }
        } while (Process32NextW(snapshot, &pe32));
    }

    CloseHandle(snapshot);
    return false;
}

bool CS2ProcessDataSource::GetModuleBase(const wchar_t* moduleName, uintptr_t& outBase) {
    HMODULE hMods[1024];
    DWORD cbNeeded;

    if (!EnumProcessModules(m_processHandle, hMods, sizeof(hMods), &cbNeeded)) {
        return false;
    }

    for (unsigned int i = 0; i < (cbNeeded / sizeof(HMODULE)); i++) {
        wchar_t szModName[MAX_PATH];
        if (GetModuleBaseNameW(m_processHandle, hMods[i], szModName, sizeof(szModName) / sizeof(wchar_t))) {
            if (wcscmp(szModName, moduleName) == 0) {
                outBase = reinterpret_cast<uintptr_t>(hMods[i]);
                return true;
            }
        }
    }

    return false;
}

bool CS2ProcessDataSource::ResolveOffsets() {
    // Start with generated RVAs as a fallback.
    m_entityListAddress = m_clientBase + ClientDll::dwEntityList;
    m_localPlayerAddress = m_clientBase + ClientDll::dwLocalPlayerPawn;
    m_localPlayerControllerAddress = m_clientBase + ClientDll::dwLocalPlayerController;
    m_viewMatrixAddress = m_clientBase + ClientDll::dwViewMatrix;
    m_gameRulesAddress = m_clientBase + ClientDll::dwGameRules;
    m_plantedC4Address = m_clientBase + ClientDll::dwPlantedC4;
    m_globalVarsAddress = m_clientBase + ClientDll::dwGlobalVars;

    // Global RVAs move frequently. Resolve them from signatures in the
    // loaded module so a game update cannot silently leave a stale address.
    MODULEINFO moduleInfo = {};
    if (GetModuleInformation(
            static_cast<HANDLE>(m_processHandle),
            reinterpret_cast<HMODULE>(m_clientBase),
            &moduleInfo,
            sizeof(moduleInfo))) {
        std::vector<uint8_t> moduleImage(moduleInfo.SizeOfImage);
        SIZE_T bytesRead = 0;
        if (ReadProcessMemory(
                static_cast<HANDLE>(m_processHandle),
                reinterpret_cast<LPCVOID>(m_clientBase),
                moduleImage.data(),
                moduleImage.size(),
                &bytesRead) &&
            bytesRead == moduleImage.size()) {
            const uintptr_t entityList = ResolveRipRelativeSignature(
                moduleImage, m_clientBase,
                "48 8B 1D ? ? ? ? 48 89 1D ? ? ? ?", 3, 7);
            const uintptr_t localPlayerController = ResolveRipRelativeSignature(
                moduleImage, m_clientBase,
                "48 8B 05 ? ? ? ? 41 89 BE", 3, 7);
            const uintptr_t viewMatrix = ResolveRipRelativeSignature(
                moduleImage, m_clientBase,
                "48 8D 0D ? ? ? ? 48 C1 E0 06", 3, 7);
            const uintptr_t gameRules = ResolveRipRelativeSignature(
                moduleImage, m_clientBase,
                "48 8B 1D ? ? ? ? 48 8D 54 24 ? 0F 28 D0 48 8D 4C 24 ?", 3, 7);
            const uintptr_t plantedC4 = ResolveRipRelativeSignature(
                moduleImage, m_clientBase,
                "48 39 2D ? ? ? ? 75 ? 49 89 AE", 3, 7);
            const uintptr_t globalVars = ResolveRipRelativeSignature(
                moduleImage, m_clientBase,
                "48 89 15 ? ? ? ? 48 89 42", 3, 7);

            if (entityList != 0) {
                m_entityListAddress = entityList;
            }
            if (localPlayerController != 0) {
                m_localPlayerControllerAddress = localPlayerController;
            }
            if (viewMatrix != 0) {
                m_viewMatrixAddress = viewMatrix;
            }
            if (gameRules != 0) {
                m_gameRulesAddress = gameRules;
            }
            if (plantedC4 != 0) {
                m_plantedC4Address = plantedC4;
            }
            if (globalVars != 0) {
                m_globalVarsAddress = globalVars;
            }
        }
    }

    std::cout << "[CS2ProcessDataSource] Resolved addresses:" << std::endl;
    std::cout << "  EntityList: 0x" << std::hex << m_entityListAddress << std::dec << std::endl;
    std::cout << "  LocalPlayer: 0x" << std::hex << m_localPlayerAddress << std::dec << std::endl;
    std::cout << "  ViewMatrix: 0x" << std::hex << m_viewMatrixAddress << std::dec << std::endl;
    std::cout << "  GameRules: 0x" << std::hex << m_gameRulesAddress << std::dec << std::endl;

    return true;
}

template<typename T>
bool CS2ProcessDataSource::ReadMemory(uintptr_t address, T& out) const {
    return ReadMemoryBuffer(address, &out, sizeof(T));
}

bool CS2ProcessDataSource::ReadMemoryBuffer(uintptr_t address, void* buffer, size_t size) const {
    if (!m_processHandle || address == 0) {
        return false;
    }

    SIZE_T bytesRead;
    return ReadProcessMemory(m_processHandle, reinterpret_cast<LPCVOID>(address), buffer, size, &bytesRead) 
           && bytesRead == size;
}

bool CS2ProcessDataSource::ReadEntityByIndex(
    uintptr_t entitySystem,
    uint32_t entityIndex,
    uintptr_t& outEntity) const {
    outEntity = 0;

    // CGameEntitySystem stores 64 pointers to chunks of 512 CEntityIdentity
    // entries. In the current build each identity is 0x70 bytes and begins
    // with its entity pointer.
    if (entitySystem == 0 || entityIndex > 0x7FFE) {
        return false;
    }

    const uint32_t chunkIndex = entityIndex >> 9;
    const uint32_t indexInChunk = entityIndex & 0x1FF;

    uintptr_t entityChunk = 0;
    const uintptr_t chunkAddress = entitySystem + 0x10 + (sizeof(uintptr_t) * chunkIndex);
    if (!ReadMemory(chunkAddress, entityChunk) || entityChunk == 0) {
        return false;
    }

    const uintptr_t identityAddress = entityChunk + (0x70 * indexInChunk);
    return ReadMemory(identityAddress, outEntity) && outEntity != 0;
}

bool CS2ProcessDataSource::ReadEntityList(std::vector<PlayerInfo>& players) {
    players.clear();

    // dwEntityList/dwGameEntitySystem is a global pointer, not the entity
    // system object itself.
    uintptr_t entitySystem = 0;
    if (!ReadMemory(m_entityListAddress, entitySystem) || entitySystem == 0) {
        std::cerr << "[CS2ProcessDataSource] Entity system is not available (join a match first)" << std::endl;
        return false;
    }
    m_entitySystemAddress = entitySystem;

    int32_t highestEntityIndex = 0;
    if (!ReadMemory(
            entitySystem + ClientDll::dwGameEntitySystem_highestEntityIndex,
            highestEntityIndex) ||
        highestEntityIndex < 1 || highestEntityIndex > 0x7FFE) {
        std::cerr << "[CS2ProcessDataSource] Invalid highest entity index" << std::endl;
        return false;
    }

    // Read local player controller for reference
    uintptr_t localPlayerController = 0;
    ReadMemory(m_localPlayerControllerAddress, localPlayerController);

    // Player controllers occupy the first 64 entity slots. Scanning other
    // entity classes as controllers creates duplicate and invalid players.
    const int32_t lastControllerIndex = min(highestEntityIndex, 64);
    for (int32_t i = 1; i <= lastControllerIndex; ++i) {
        uintptr_t entityPtr = 0;
        if (!ReadEntityByIndex(entitySystem, static_cast<uint32_t>(i), entityPtr)) {
            continue;
        }

        PlayerInfo player;
        if (ReadPlayerController(entitySystem, entityPtr, player)) {
            player.entityId = i;
            player.isLocalPlayer = (entityPtr == localPlayerController);
            players.push_back(player);
        }
    }

    if (!players.empty()) {
        std::cout << "[CS2ProcessDataSource] Read " << players.size() << " players" << std::endl;
    }
    
    return !players.empty();
}

bool CS2ProcessDataSource::ReadPlayerController(
    uintptr_t entitySystem,
    uintptr_t controllerAddress,
    PlayerInfo& player) {
    m_currentControllerAddress = controllerAddress;
    // The controller's replicated pawn armor is the authoritative value.
    // Mark it unavailable until that read succeeds so the pawn path cannot
    // overwrite a valid value with an unrelated/stale field.
    player.armor = -1;
    // Read team number
    uint8_t teamNum = 0;
    if (!ReadMemory(controllerAddress + PlayerController::m_iTeamNum, teamNum)) {
        return false;
    }

    // Only process valid teams (2 = T, 3 = CT)
    if (teamNum < 2 || teamNum > 3) {
        return false;
    }

    player.team = static_cast<int32_t>(teamNum);

    // Read player name
    char nameBuffer[MAX_PLAYER_NAME];
    if (ReadMemoryBuffer(controllerAddress + PlayerController::m_iszPlayerName, nameBuffer, sizeof(nameBuffer))) {
        nameBuffer[MAX_PLAYER_NAME - 1] = '\0';
        strncpy_s(player.name, nameBuffer, MAX_PLAYER_NAME - 1);
    }

    // Read pawn handle
    uint32_t pawnHandle;
    if (!ReadMemory(controllerAddress + PlayerController::m_hPlayerPawn, pawnHandle)) {
        return false;
    }

    // Read alive status
    bool isAlive;
    if (ReadMemory(controllerAddress + PlayerController::m_bPawnIsAlive, isAlive)) {
        player.isAlive = isAlive;
    }

    // Read health
    int32_t health;
    if (ReadMemory(controllerAddress + PlayerController::m_iPawnHealth, health)) {
        player.health = health;
    }

    int32_t controllerArmor = 0;
    if (ReadMemory(controllerAddress + PlayerController::m_iPawnArmor, controllerArmor) &&
        controllerArmor >= 0 && controllerArmor <= 100) {
        player.armor = controllerArmor;
    }

    bool hasDefuser = false;
    if (ReadMemory(controllerAddress + PlayerController::m_bPawnHasDefuser, hasDefuser)) {
        player.hasDefuser = hasDefuser;
    }

    // Resolve pawn from handle
    if (pawnHandle != 0 && pawnHandle != 0xFFFFFFFF) {
        // Handle contains entity index in lower 15 bits
        uint32_t pawnIndex = pawnHandle & 0x7FFF;
        
        if (pawnIndex > 0 && pawnIndex < 2048) {
            uintptr_t pawnAddress = 0;
            if (ReadEntityByIndex(entitySystem, pawnIndex, pawnAddress)) {
                ReadPlayerPawn(pawnAddress, player);
            }
        }
    }

    return true;
}

bool CS2ProcessDataSource::ReadPlayerPawn(uintptr_t pawnAddress, PlayerInfo& player) {
    // Read game scene node (contains position)
    uintptr_t sceneNode;
    if (!ReadMemory(pawnAddress + Entity::m_pGameSceneNode, sceneNode)) {
        return false;
    }

    if (sceneNode == 0) {
        return false;
    }

    if (player.isLocalPlayer) {
        uint32_t minimapHandle = 0;
        if (ReadMemory(pawnAddress + PlayerPawn::m_hActiveMinimapVolume, minimapHandle)) {
            const uint32_t minimapIndex = minimapHandle & 0x7fff;
            uintptr_t minimapVolume = 0;
            if (minimapIndex > 0 && minimapIndex < 2048 &&
                ReadEntityByIndex(m_entitySystemAddress, minimapIndex, minimapVolume) && minimapVolume) {
                uintptr_t mapNamePtr = 0;
                char mapName[64] = {};
                if (ReadMemory(minimapVolume + MinimapVolume::m_strMinimapName, mapNamePtr) &&
                    mapNamePtr && ReadMemoryBuffer(mapNamePtr, mapName, sizeof(mapName) - 1)) {
                    mapName[sizeof(mapName) - 1] = '\0';
                    std::string detected(mapName);
                    const size_t prefix = detected.find("de_") != std::string::npos ? detected.find("de_") :
                        (detected.find("cs_") != std::string::npos ? detected.find("cs_") : detected.find("ar_"));
                    if (prefix != std::string::npos) {
                        detected = detected.substr(prefix);
                        const size_t extension = detected.find_first_of(".");
                        if (extension != std::string::npos) detected.resize(extension);
                        const size_t suffix = detected.find("_radar");
                        if (suffix != std::string::npos) detected.resize(suffix);
                        m_detectedMapName = detected;
                    }
                }
            }
        }
    }

    // Read position from scene node
    float posBuffer[3];
    if (ReadMemoryBuffer(sceneNode + SceneNode::m_vecAbsOrigin, posBuffer, sizeof(posBuffer))) {
        player.position.x = posBuffer[0];
        player.position.y = posBuffer[1];
        player.position.z = posBuffer[2];
    }

    if (player.isLocalPlayer) {
        float eyeAngles[2] = {};
        if (ReadMemoryBuffer(pawnAddress + PlayerPawn::m_angEyeAngles, eyeAngles, sizeof(eyeAngles))) {
            player.viewAngles.x = eyeAngles[0];
            player.viewAngles.y = eyeAngles[1];
        }
    }

    // Read health (from pawn, more reliable than controller)
    int32_t health;
    if (ReadMemory(pawnAddress + Entity::m_iHealth, health)) {
        player.health = health;
    }

    // Read armor
    // m_ArmorValue is an int32 in the client schema.  Some builds expose the
    // replicated value as a packed 16-bit field while the pawn is being
    // predicted; prefer a sane byte/word value and only then use the full int.
    int32_t armor = player.armor;
    if (player.armor < 0 && ReadMemory(pawnAddress + PlayerPawn::m_ArmorValue, armor)) {
        const uint8_t low = static_cast<uint8_t>(armor & 0xff);
        const uint16_t lowWord = static_cast<uint16_t>(armor & 0xffff);
        if (low <= 100) player.armor = low;
        else if (lowWord <= 100) player.armor = static_cast<int32_t>(lowWord);
        // The current client sometimes stores armor as a doubled byte in the
        // replicated pawn field (100 armor is read as 200).
        else if (low <= 200 && (low % 2) == 0) player.armor = static_cast<int32_t>(low / 2);
        else if (armor >= 0 && armor <= 100) player.armor = armor;
    }

    // Read the weapon service vector.  Its handles are entity-list handles,
    // so this remains read-only and works for every visible player.
    uintptr_t weaponServices = 0;
    if (ReadMemory(pawnAddress + PlayerPawn::m_pWeaponServices, weaponServices) && weaponServices) {
        uint32_t activeHandle = 0;
        ReadMemory(weaponServices + WeaponServices::m_hActiveWeapon, activeHandle);
        // Resolve the active weapon independently of the vector so the UI
        // still shows the held weapon if the inventory container is empty
        // during spawn/round transitions.
        const uint32_t activeIndex = activeHandle & 0x7fff;
        if (activeIndex > 0 && activeIndex < 2048) {
            uintptr_t activeWeapon = 0;
            uint16_t activeItem = 0;
            if (ReadEntityByIndex(m_entitySystemAddress, activeIndex, activeWeapon) && activeWeapon &&
                ReadMemory(activeWeapon + Weapon::m_iItemDefinitionIndex, activeItem) && activeItem) {
                player.weaponId = activeItem;
                player.inventory.push_back(activeItem);
            }
        }
        uintptr_t vectorData = 0;
        ReadMemory(weaponServices + WeaponServices::m_hMyWeapons, vectorData);
        // CNetworkUtlVectorBase has used both 8-byte and 12-byte size
        // alignment in released client builds. Try each valid count field.
        for (const uintptr_t sizeOffset : { sizeof(uintptr_t), sizeof(uintptr_t) + 4, sizeof(uintptr_t) + 8 }) {
            int32_t vectorSize = 0;
            ReadMemory(weaponServices + WeaponServices::m_hMyWeapons + sizeOffset, vectorSize);
            if (!vectorData || vectorSize <= 0 || vectorSize > 64) continue;
            for (int32_t i = 0; i < vectorSize; ++i) {
                uint32_t handle = 0;
                if (!ReadMemory(vectorData + static_cast<uintptr_t>(i) * sizeof(uint32_t), handle)) continue;
                const uint32_t index = handle & 0x7fff;
                if (!index || index >= 2048) continue;
                uintptr_t weapon = 0;
                if (!ReadEntityByIndex(m_entitySystemAddress, index, weapon) || !weapon) continue;
                uint16_t itemDef = 0;
                if (ReadMemory(weapon + Weapon::m_iItemDefinitionIndex, itemDef) && itemDef) {
                    if (std::find(player.inventory.begin(), player.inventory.end(), itemDef) == player.inventory.end()) {
                        player.inventory.push_back(itemDef);
                    }
                    if (handle == activeHandle) player.weaponId = itemDef;
                }
            }
        }

        // Newer builds can expose m_hMyWeapons as an inline handle array
        // instead of a separately allocated CUtlVector. Read the first 64
        // handles as a second read-only path so inventory remains populated
        // during spawn and weapon-switch transitions.
        for (size_t i = 0; i < 64; ++i) {
            uint32_t handle = 0;
            if (!ReadMemory(weaponServices + WeaponServices::m_hMyWeapons + i * sizeof(uint32_t), handle)) continue;
            const uint32_t index = handle & 0x7fff;
            if (!index || index >= 2048) continue;
            uintptr_t weapon = 0;
            if (!ReadEntityByIndex(m_entitySystemAddress, index, weapon) || !weapon) continue;
            uint16_t itemDef = 0;
            if (!ReadMemory(weapon + Weapon::m_iItemDefinitionIndex, itemDef) || !itemDef || itemDef >= 500) continue;
            if (std::find(player.inventory.begin(), player.inventory.end(), itemDef) == player.inventory.end()) player.inventory.push_back(itemDef);
            if (handle == activeHandle) player.weaponId = itemDef;
        }

        // Fallback for builds where the network vector is not materialized as
        // a readable CUtlVector. Dropped/carried weapons still expose their
        // owner handle, so scan entity identities and collect matching items.
        {
            int32_t highest = 0;
            if (ReadMemory(m_entitySystemAddress + ClientDll::dwGameEntitySystem_highestEntityIndex, highest) &&
                highest > 0 && highest < 2048) {
                for (int32_t entityIndex = 1; entityIndex <= highest; ++entityIndex) {
                    uintptr_t candidate = 0;
                    if (!ReadEntityByIndex(m_entitySystemAddress, static_cast<uint32_t>(entityIndex), candidate) ||
                        candidate == pawnAddress) continue;
                    uint32_t ownerHandle = 0;
                    if (!ReadMemory(candidate + Entity::m_hOwnerEntity, ownerHandle)) continue;
                    const uint32_t ownerIndex = ownerHandle & 0x7fff;
                    if (!ownerIndex) continue;
                    uintptr_t owner = 0;
                    if (!ReadEntityByIndex(m_entitySystemAddress, ownerIndex, owner) ||
                        (owner != pawnAddress && owner != m_currentControllerAddress)) continue;
                    uint16_t itemDef = 0;
                    if (ReadMemory(candidate + Weapon::m_iItemDefinitionIndex, itemDef) && itemDef > 0 && itemDef < 128 &&
                        std::find(player.inventory.begin(), player.inventory.end(), itemDef) == player.inventory.end()) {
                        player.inventory.push_back(itemDef);
                    }
                }
            }
        }
    }

    return true;
}

bool CS2ProcessDataSource::ReadViewMatrix(float matrix[16]) {
    return ReadMemoryBuffer(m_viewMatrixAddress, matrix, sizeof(float) * 16);
}

bool CS2ProcessDataSource::ReadGameState(GameState& state) {
    m_detectedMapName.clear();
    state.mapName[0] = '\0';
    // dwGameRules is a global pointer to the game-rules object.
    uintptr_t gameRules = 0;
    if (!ReadMemory(m_gameRulesAddress, gameRules) || gameRules == 0) {
        return false;
    }
    // CGlobalVars stores the live level name as a pointer. This is the
    // authoritative map source and works for workshop maps too.
    uintptr_t mapGlobalVars = 0;
    if (ReadMemory(m_globalVarsAddress, mapGlobalVars) && mapGlobalVars) {
        uintptr_t mapNamePtr = 0;
        char mapText[64] = {};
        if (ReadMemory(mapGlobalVars + GlobalVars::m_mapName, mapNamePtr) && mapNamePtr &&
            ReadMemoryBuffer(mapNamePtr, mapText, sizeof(mapText) - 1)) {
            std::string detected(mapText);
            const size_t de = detected.find("de_");
            const size_t cs = detected.find("cs_");
            const size_t ar = detected.find("ar_");
            const size_t prefix = de != std::string::npos ? de : (cs != std::string::npos ? cs : ar);
            if (prefix != std::string::npos) {
                detected = detected.substr(prefix);
                const size_t extension = detected.find_first_of(".");
                if (extension != std::string::npos) detected.resize(extension);
                if (detected.size() < sizeof(state.mapName)) m_detectedMapName = detected;
            }
        }
    }
    // Some client builds keep the current level as a CUtlString pointer.
    // Keep this as a read-only fallback for builds where minimap volumes are
    // not populated yet during warmup or a map transition.
    uintptr_t mapNameAddress = 0;
    if (ReadMemory(gameRules + GameInfo::m_szMapName, mapNameAddress) && mapNameAddress) {
        char mapText[64] = {};
        if (ReadMemoryBuffer(mapNameAddress, mapText, sizeof(mapText) - 1)) {
            std::string detected(mapText);
            const size_t de = detected.find("de_");
            const size_t cs = detected.find("cs_");
            const size_t ar = detected.find("ar_");
            const size_t prefix = de != std::string::npos ? de : (cs != std::string::npos ? cs : ar);
            if (prefix != std::string::npos) {
                detected = detected.substr(prefix);
                const size_t extension = detected.find_first_of(".");
                if (extension != std::string::npos) detected.resize(extension);
                if (detected.size() < sizeof(state.mapName)) m_detectedMapName = detected;
            }
        }
    }

    // Read game phase
    int32_t gamePhase;
    if (ReadMemory(gameRules + GameInfo::m_gamePhase, gamePhase)) {
        state.gamePhase = static_cast<GameState::Phase>(gamePhase);
    }

    // Read round number
    int32_t roundNum;
    if (ReadMemory(gameRules + GameInfo::m_totalRoundsPlayed, roundNum)) {
        state.roundNumber = roundNum;
    }

    // Read freeze time
    bool freezeTime;
    if (ReadMemory(gameRules + GameInfo::m_bFreezePeriod, freezeTime)) {
        state.isFreezetime = freezeTime;
    }

    // Read bomb planted
    bool bombPlanted;
    if (ReadMemory(gameRules + GameInfo::m_bBombPlanted, bombPlanted)) {
        state.isBombPlanted = bombPlanted;
    }

    // Discover map names from minimap-volume entities so new maps do not
    // require a new hardcoded bounds fingerprint.
    if (m_entitySystemAddress != 0) {
        int32_t highest = 0;
        if (ReadMemory(m_entitySystemAddress + ClientDll::dwGameEntitySystem_highestEntityIndex, highest) &&
            highest > 0 && highest < 2048) {
            for (int32_t index = 1; index <= highest; ++index) {
                uintptr_t entity = 0;
                if (!ReadEntityByIndex(m_entitySystemAddress, static_cast<uint32_t>(index), entity) || !entity) continue;
                uintptr_t namePtr = 0;
                if (!ReadMemory(entity + MinimapVolume::m_strMinimapName, namePtr) || !namePtr) continue;
                char name[96] = {};
                if (!ReadMemoryBuffer(namePtr, name, sizeof(name) - 1)) continue;
                name[sizeof(name) - 1] = '\0';
                std::string detected(name);
                const size_t de = detected.find("de_");
                const size_t cs = detected.find("cs_");
                const size_t ar = detected.find("ar_");
                const size_t prefix = de != std::string::npos ? de : (cs != std::string::npos ? cs : ar);
                if (prefix == std::string::npos) continue;
                detected = detected.substr(prefix);
                const size_t extension = detected.find_first_of(".");
                if (extension != std::string::npos) detected.resize(extension);
                const size_t suffix = detected.find("_radar");
                if (suffix != std::string::npos) detected.resize(suffix);
                if (detected.size() > 3 && detected.size() < sizeof(state.mapName)) {
                    strncpy_s(state.mapName, detected.c_str(), sizeof(state.mapName) - 1);
                    m_detectedMapName = detected;
                    break;
                }
            }
        }
    }

    if (state.isBombPlanted) {
        uintptr_t plantedC4 = 0;
        float blowTime = 0.0f;
        float timerLength = 40.0f;
        uintptr_t globalVars = 0;
        float currentTime = 0.0f;

        if (ReadMemory(m_plantedC4Address, plantedC4) && plantedC4 != 0) {
            ReadMemory(plantedC4 + PlantedC4::m_nBombSite, state.bombSite);
            ReadMemory(plantedC4 + PlantedC4::m_flC4Blow, blowTime);
            ReadMemory(plantedC4 + PlantedC4::m_flTimerLength, timerLength);
        }
        if (ReadMemory(m_globalVarsAddress, globalVars) && globalVars != 0) {
            ReadMemory(globalVars + GlobalVars::m_curtime, currentTime);
        }

        if (blowTime > 0.0f && currentTime >= 0.0f) {
            state.bombTimeRemaining = (std::max)(0.0f, blowTime - currentTime);
            if (timerLength > 0.0f) {
                state.bombTimeRemaining = (std::min)(state.bombTimeRemaining, timerLength);
            }
        }
    } else {
        state.bombSite = -1;
        state.bombTimeRemaining = 0.0f;
    }

    // The client schema exposes the active minimap rectangle even though it
    // does not expose the map string. Use that runtime fingerprint for maps
    // with a known rectangle and keep the UI selector as a fallback for
    // custom/workshop maps.
    float minimapMins[3] = {};
    float minimapMaxs[3] = {};
    if (ReadMemoryBuffer(gameRules + GameInfo::m_vMinimapMins, minimapMins, sizeof(minimapMins)) &&
        ReadMemoryBuffer(gameRules + GameInfo::m_vMinimapMaxs, minimapMaxs, sizeof(minimapMaxs)) &&
        std::isfinite(minimapMins[0]) && std::isfinite(minimapMaxs[0])) {
        state.minimapMins = Vector3(minimapMins[0], minimapMins[1], minimapMins[2]);
        state.minimapMaxs = Vector3(minimapMaxs[0], minimapMaxs[1], minimapMaxs[2]);
        const bool looksLikeNuke =
            minimapMins[0] > -3200.0f && minimapMins[0] < -3000.0f &&
            minimapMaxs[0] > 3500.0f && minimapMaxs[0] < 3900.0f &&
            minimapMins[1] > -2800.0f && minimapMins[1] < -2300.0f &&
            minimapMaxs[1] > 900.0f && minimapMaxs[1] < 1400.0f;
        const bool looksLikeMirage =
            minimapMins[0] > -2900.0f && minimapMins[0] < -2700.0f &&
            minimapMaxs[0] > 1400.0f && minimapMaxs[0] < 1650.0f &&
            minimapMins[1] > -2950.0f && minimapMins[1] < -2700.0f &&
            minimapMaxs[1] > 1000.0f && minimapMaxs[1] < 1250.0f;
        const bool looksLikeVertigo =
            minimapMins[0] > -3250.0f && minimapMins[0] < -3000.0f &&
            minimapMaxs[0] > 400.0f && minimapMaxs[0] < 700.0f &&
            minimapMins[1] > -2150.0f && minimapMins[1] < -1900.0f &&
            minimapMaxs[1] > 1450.0f && minimapMaxs[1] < 1700.0f;
        if (looksLikeNuke) {
            strcpy_s(state.mapName, "de_nuke");
        } else if (looksLikeMirage) {
            strcpy_s(state.mapName, "de_mirage");
        } else if (looksLikeVertigo) {
            strcpy_s(state.mapName, "de_vertigo");
        }
    }

    if (!m_detectedMapName.empty()) {
        strncpy_s(state.mapName, m_detectedMapName.c_str(), sizeof(state.mapName) - 1);
    } else if (state.mapName[0] == '\0') {
        strcpy_s(state.mapName, "de_unknown");
    }

    return true;
}

// Factory implementation
std::unique_ptr<IDataSource> DataSourceFactory::CreateDataSource(SourceType type) {
    switch (type) {
        case SourceType::CS2_PROCESS:
            return std::make_unique<CS2ProcessDataSource>();
        case SourceType::DEMO_FILE:
            // Not implemented
            return nullptr;
        default:
            return nullptr;
    }
}
