# Folk Valley Radar

A real-time external radar overlay for Counter-Strike 2 using **external process memory reading**. 

## Architecture

This application operates **entirely outside** the CS2 process with **NO injection, NO hooks, and NO memory writes**.

```
┌─────────────────┐         ┌─────────────────┐         ┌─────────────────┐
│  C++ Collector  │ ──HTTP─>│  Node.js Backend│ <─WS──> │  Web Interface  │
│  (radar_        │  POST   │  (validates &   │         │  (browser-based │
│   collector.exe)│         │   broadcasts)   │         │   radar display)│
└─────────────────┘         └─────────────────┘         └─────────────────┘
      │                                                          │
      │ ReadProcessMemory                                       │
      │ (read-only)                                            │
      ▼                                                          ▼
┌─────────────────┐                                    ┌─────────────────┐
│   CS2 Process   │                                    │   Browser       │
│   (cs2.exe)     │                                    │   (any modern)  │
└─────────────────┘                                    └─────────────────┘
```

### Components

1. **C++ Collector** (`collector/`)
   - External Windows process reader
   - Uses `OpenProcess()` with `PROCESS_VM_READ` permission
   - Reads CS2 memory structures (entity list, player data, view matrix)
   - Normalizes and validates data
   - Sends JSON snapshots to backend via HTTP POST

2. **Node.js Backend** (`backend/`)
   - Receives data from collector (port 3000)
   - Validates incoming snapshots
   - Broadcasts to web clients via WebSocket
   - Serves static web files (port 8080)

3. **Web Interface** (`web/`)
   - Real-time radar display with CS2 map backgrounds
   - Shows player positions, health, armor, team
   - WebSocket-based live updates

## Requirements

- **Windows 10/11** (x64)
- **Visual Studio 2019+** or **Build Tools for Visual Studio** with C++ support
- **CMake 3.20+**
- **Node.js 16+**
- **CS2** (for live mode) or run in simulation mode for testing

## Quick Start

### Option 1: Automated Startup (Recommended)

```powershell
# Run with CS2 (requires CS2 running & Administrator privileges)
.\start-radar.ps1

# Run in simulation mode (no CS2 required, for testing)
.\start-radar.ps1 -Simulate

# Skip rebuild if already compiled
.\start-radar.ps1 -SkipBuild

# Custom ports
.\start-radar.ps1 -WebPort 9000 -CollectorPort 4000
```

### Option 2: Manual Steps

1. **Install backend dependencies:**
   ```powershell
   cd backend
   npm install
   ```

2. **Build C++ collector:**
   ```powershell
   cd collector
   cmake -S . -B build -A x64
   cmake --build build --config Release
   ```

3. **Start backend server:**
   ```powershell
   cd backend
   node src/server.js
   ```

4. **Start collector:**
   ```powershell
   # With CS2 running (as Administrator)
   cd collector/build/Release
   .\Folk Valley Radar.exe

   # Or in simulation mode (no CS2)
   ```

5. **Open browser:**
   ```
   http://localhost:8080
   ```

## Usage

### Command-Line Options (Collector)

```
Folk Valley Radar.exe [options]

Options:
  --host <host>       Backend host (default: 127.0.0.1)
  --port <port>       Backend port (default: 3000)
  --help, -h          Show help message
```

### Running with CS2

1. Start CS2 and join a match or local server
2. Run the collector **as Administrator** (required for process memory access)
3. The radar will display live player positions

**Important Notes:**
- Must run as Administrator on Windows 10/11 for memory reading
- Only reads data available to spectators (no wallhack/ESP)
- Does not write to CS2 memory
- Does not inject code into CS2
- Safe to use in local/offline matches

### Simulation Mode

Perfect for testing without CS2:

```powershell
.\start-radar.ps1 -Simulate
```

Generates fake player data moving in circular patterns on de_dust2.

## Project Structure

```
webradar/
├── collector/              # C++ external memory reader
│   ├── include/           # Header files
│   │   ├── data_source.h      # Abstract data source interface
│   │   ├── snapshot.h         # Game data structures
│   │   ├── normalizer.h       # Data validation & JSON serialization
│   │   └── ipc_client.h       # HTTP client for backend communication
│   ├── src/               # Implementation files
│   │   ├── main.cpp               # Entry point & capture loop
│   │   ├── cs2_process_source.cpp # CS2 memory reader
│   │   ├── normalizer.cpp         # Data normalization
│   │   └── ipc_client.cpp         # HTTP POST client
│   ├── compatibility/     # CS2 memory offsets & patterns
│   │   ├── OFFSETS.h      # Memory offsets from cs2-dumper
│   │   └── PATTERNS.h     # Signature patterns for scanning
│   └── CMakeLists.txt     # CMake build configuration
├── backend/               # Node.js validation & broadcast server
│   ├── src/
│   │   ├── server.js      # HTTP + WebSocket server
│   │   └── validate.js    # Snapshot validation
│   └── package.json
├── web/                   # Browser-based radar interface
│   ├── index.html         # Main page
│   ├── app.js             # WebSocket client & rendering
│   ├── style.css          # Radar styling
│   └── maps/              # CS2 map images
│       ├── de_dust2.png
│       ├── de_mirage.png
│       ├── de_inferno.png
│       └── ...
├── start-radar.ps1        # Automated startup script
└── README.md              # This file
```

## Technical Details

### Memory Reading

The collector uses **external process memory reading**:

- `CreateToolhelp32Snapshot()` - Find CS2 process (cs2.exe)
- `OpenProcess(PROCESS_VM_READ)` - Open process with read-only access
- `EnumProcessModules()` - Find client.dll and engine2.dll base addresses
- `ReadProcessMemory()` - Read game data structures

### Offsets & Patterns

Memory offsets are sourced from [cs2-dumper](https://github.com/a2x/cs2-dumper) and stored in:
- `collector/compatibility/OFFSETS.h` - Static offsets
- `collector/compatibility/PATTERNS.h` - Signature patterns for dynamic resolution

**Updating Offsets:**

After CS2 updates, offsets may change. Update `OFFSETS.h` with latest values from cs2-dumper:
```
https://github.com/a2x/cs2-dumper/blob/main/output/offsets.hpp
```

### Data Flow

1. **Collector** reads CS2 memory at 60 Hz
2. Extracts: player positions, health, team, view matrix, game state
3. Normalizes data (clamps values, sanitizes strings)
4. Serializes to JSON
5. Sends HTTP POST to backend `/api/radar`
6. **Backend** validates snapshot structure
7. Broadcasts to all connected WebSocket clients
8. **Web interface** receives updates and renders radar

### Coordinate Transformation

World coordinates from CS2 are converted to 2D radar coordinates:

```javascript
// Convert CS2 world position to radar percentage
function worldToRadarCoords(worldX, worldY, mapName) {
  const bounds = MAP_BOUNDS[mapName]; // Map-specific boundaries
  let x = (worldX - bounds.minX) / (bounds.maxX - bounds.minX);
  let y = (worldY - bounds.minY) / (bounds.maxY - bounds.minY);
  return { x: x * 100, y: (1 - y) * 100 }; // 0-100% range
}
```

## Security & Anti-Cheat

### What This Does
- ✅ Reads CS2 process memory externally
- ✅ Displays player positions visible to spectators
- ✅ Works like watching a GOTV demo in real-time

### What This Does NOT Do
- ❌ Inject DLLs into CS2
- ❌ Hook game functions
- ❌ Modify game memory
- ❌ Write to CS2 process
- ❌ Provide information not available to spectators
- ❌ Bypass anti-cheat directly

### Important Disclaimer

**This tool is for educational and offline use only.**

- Use only in local matches, offline games, or private servers
- Using external tools in official matchmaking may violate CS2 Terms of Service
- VAC and other anti-cheat systems may detect external memory reading
- The authors are not responsible for any bans or account actions

## Troubleshooting

### Collector fails to find CS2
- Ensure CS2 (cs2.exe) is running
- Check Task Manager for "cs2.exe" process
- Try running collector as Administrator

### "Failed to open process" error
- **Run as Administrator** (required for ReadProcessMemory)
- Check Windows Security settings
- Ensure antivirus isn't blocking the collector

### No data on radar
- Verify backend is running (http://localhost:8080)
- Check collector console for errors
- Confirm CS2 is in an active match (not main menu)

### Build errors
- Ensure Visual Studio C++ tools are installed
- Update CMake to 3.20+
- Clear build directory: `Remove-Item collector/build -Recurse -Force`

### Offsets outdated after CS2 update
- Get latest offsets from [cs2-dumper](https://github.com/a2x/cs2-dumper)
- Update `collector/compatibility/OFFSETS.h`
- Rebuild collector

## Development

### Adding New Maps

1. Add map background image to `web/maps/<mapname>.png`
2. Update `MAP_BOUNDS` in `web/app.js` with map coordinates:
   ```javascript
   de_newmap: { minX: -3000, maxX: 3000, minY: -3000, maxY: 3000 }
   ```

### Modifying Data Collection

Edit `collector/src/cs2_process_source.cpp`:
- `ReadEntityList()` - Entity iteration
- `ReadPlayerController()` - Player data reading
- `ReadPlayerPawn()` - Pawn-specific data

Add new fields to `collector/include/snapshot.h` structures.

### Extending Web Interface

Edit `web/app.js`:
- `render()` - Update display logic
- `worldToRadarCoords()` - Coordinate transformation
- Add new UI elements in `web/index.html`

## License & Credits

This project is for **educational purposes only**.

### Credits
- Memory offsets: [cs2-dumper](https://github.com/a2x/cs2-dumper) by a2x
- CS2 map radar images: Valve Corporation / Community contributors

### Disclaimer

Counter-Strike 2, CS2, Steam, and Valve are trademarks of Valve Corporation. This project is not affiliated with or endorsed by Valve Corporation.

## FAQ

**Q: Is this a cheat?**  
A: It reads memory externally but only displays information available to spectators. Use responsibly and only in approved contexts.

**Q: Will I get VAC banned?**  
A: Possibly. External memory reading can be detected. Use at your own risk, preferably offline only.

**Q: Can I use this in matchmaking?**  
A: **NO.** This violates CS2 Terms of Service. Use only for educational purposes in offline/local matches.

**Q: Does it work on Linux?**  
A: Currently Windows-only. Linux port would require different APIs (ptrace, /proc/pid/mem).

**Q: How often are offsets updated?**  
A: Offsets must be updated manually after CS2 patches. Check cs2-dumper repository.

**Q: Can I add more features?**  
A: Yes! The code is modular. See Development section above.

## Support

For issues, questions, or contributions:
- Check existing issues in the repository
- Review troubleshooting section above
- Ensure you're using latest offsets from cs2-dumper

**Remember: Use responsibly and ethically. This tool is for learning and offline use only.**

Powered by [Source 2 Viewer](https://s2v.app) ([ValveResourceFormat](https://github.com/ValveResourceFormat/ValveResourceFormat)).

