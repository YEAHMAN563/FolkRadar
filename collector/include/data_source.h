#pragma once

#include <memory>
#include <string>
#include <vector>
#include "snapshot.h"

/**
 * Abstract interface for game data sources.
 * Implementations can read from live CS2 process or demo files.
 */
class IDataSource {
public:
    virtual ~IDataSource() = default;

    /**
     * Initialize the data source.
     * @return true if initialization successful, false otherwise
     */
    virtual bool Initialize() = 0;

    /**
     * Check if the data source is currently available/valid.
     * @return true if data can be read, false otherwise
     */
    virtual bool IsValid() const = 0;

    /**
     * Capture current game state snapshot.
     * @param outSnapshot Output snapshot structure to populate
     * @return true if snapshot captured successfully, false otherwise
     */
    virtual bool CaptureSnapshot(GameSnapshot& outSnapshot) = 0;

    /**
     * Get human-readable status string for debugging.
     */
    virtual std::string GetStatusString() const = 0;

    /**
     * Cleanup and shutdown the data source.
     */
    virtual void Shutdown() = 0;

protected:
    bool m_initialized = false;
};

/**
 * CS2 Process Memory Data Source (External)
 * Reads game data from CS2 process memory using ReadProcessMemory.
 * This is an EXTERNAL reader - no injection, no hooks, no modifications.
 */
class CS2ProcessDataSource : public IDataSource {
public:
    CS2ProcessDataSource();
    ~CS2ProcessDataSource() override;

    bool Initialize() override;
    bool IsValid() const override;
    bool CaptureSnapshot(GameSnapshot& outSnapshot) override;
    std::string GetStatusString() const override;
    void Shutdown() override;

private:
    // Process information
    unsigned long m_processId = 0;
    void* m_processHandle = nullptr;
    uintptr_t m_clientBase = 0;
    uintptr_t m_engine2Base = 0;

    // Cached addresses
    uintptr_t m_entityListAddress = 0;
    uintptr_t m_localPlayerAddress = 0;
    uintptr_t m_localPlayerControllerAddress = 0;
    uintptr_t m_viewMatrixAddress = 0;
    uintptr_t m_gameRulesAddress = 0;
    uintptr_t m_plantedC4Address = 0;
    uintptr_t m_globalVarsAddress = 0;
    uintptr_t m_entitySystemAddress = 0;
    std::string m_detectedMapName;
    uintptr_t m_currentControllerAddress = 0;

    // Helper methods
    bool FindCS2Process();
    bool GetModuleBase(const wchar_t* moduleName, uintptr_t& outBase);
    bool ResolveOffsets();
    bool ValidateGameVersion();
    
    // Memory reading
    template<typename T>
    bool ReadMemory(uintptr_t address, T& out) const;
    bool ReadMemoryBuffer(uintptr_t address, void* buffer, size_t size) const;
    bool ReadEntityByIndex(uintptr_t entitySystem, uint32_t entityIndex, uintptr_t& outEntity) const;

    // Entity reading
    bool ReadEntityList(std::vector<PlayerInfo>& players);
    bool ReadPlayerController(uintptr_t entitySystem, uintptr_t controllerAddress, PlayerInfo& player);
    bool ReadPlayerPawn(uintptr_t pawnAddress, PlayerInfo& player);
    bool ReadViewMatrix(float matrix[16]);
    bool ReadGameState(GameState& state);
};

/**
 * Factory for creating appropriate data source based on configuration.
 */
class DataSourceFactory {
public:
    enum class SourceType {
        CS2_PROCESS,    // Live CS2 process memory
        DEMO_FILE       // Demo file playback (not implemented)
    };

    static std::unique_ptr<IDataSource> CreateDataSource(SourceType type);
};
