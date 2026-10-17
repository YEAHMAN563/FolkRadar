#include "../include/data_source.h"
#include "../include/normalizer.h"
#include "../include/ipc_client.h"
#include "../include/source2_overhead.h"
#include "../include/embedded_runtime.h"
#include <winsock2.h>
#include <Windows.h>
#include <iphlpapi.h>
#include <iostream>
#include <thread>
#include <chrono>
#include <csignal>
#include <atomic>
#include <vector>
#include <filesystem>
#include <io.h>
#include <cstdlib>

std::atomic<bool> g_running(true);
PROCESS_INFORMATION g_backendProcess = {};
HANDLE g_backendJob = nullptr;
std::filesystem::path g_runtimeRoot;
int g_savedStdout = -1, g_savedStderr = -1;

std::string GetLanAddress() {
    ULONG size = 0;
    GetAdaptersAddresses(AF_INET, GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER, nullptr, nullptr, &size);
    std::vector<unsigned char> buffer(size);
    auto* adapters = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data());
    if (GetAdaptersAddresses(AF_INET, GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER, nullptr, adapters, &size) != NO_ERROR) return {};
    for (auto* a = adapters; a; a = a->Next) if (a->OperStatus == IfOperStatusUp)
        for (auto* u = a->FirstUnicastAddress; u; u = u->Next) {
            auto* sin = reinterpret_cast<sockaddr_in*>(u->Address.lpSockaddr);
            const char* text = inet_ntoa(sin->sin_addr);
            if (text && std::string(text).rfind("127.", 0) != 0 && std::string(text).rfind("169.254.", 0) != 0) return text;
        }
    return {};
}

bool EnableLanAccess() {
    const char* add = "netsh advfirewall firewall add rule name=\"Folk Valley Radar Web 8787\" dir=in action=allow protocol=TCP localport=8787 profile=private,domain >nul 2>&1";
    const char* enable = "netsh advfirewall firewall set rule name=\"Folk Valley Radar Web 8787\" new enable=yes >nul 2>&1";
    return std::system(add) == 0 || std::system(enable) == 0;
}

void PrintStartupBanner() {
    std::cout << "\x1b[0;37m\n";
    std::cout << "\x1b[0;34m  ███████╗ ██████╗ ██╗     ██╗  ██╗\x1b[0m  \x1b[0;35m██╗   ██╗ █████╗ ██╗     ██╗     ███████╗██╗   ██╗\x1b[0m\n";
    std::cout << "\x1b[0;34m  ██╔════╝██╔═══██╗██║     ██║ ██╔╝\x1b[0m  \x1b[0;35m██║   ██║██╔══██╗██║     ██║     ██╔════╝╚██╗ ██╔╝\x1b[0m\n";
    std::cout << "\x1b[0;34m  █████╗  ██║   ██║██║     █████╔╝ \x1b[0m  \x1b[0;35m██║   ██║███████║██║     ██║     █████╗   ╚████╔╝ \x1b[0m\n";
    std::cout << "\x1b[0;34m  ██╔══╝  ██║   ██║██║     ██╔═██╗ \x1b[0m  \x1b[0;35m╚██╗ ██╔╝██╔══██║██║     ██║     ██╔══╝    ╚██╔╝  \x1b[0m\n";
    std::cout << "\x1b[0;34m  ██║     ╚██████╔╝███████╗██║  ██╗\x1b[0m  \x1b[0;35m ╚████╔╝ ██║  ██║███████╗███████╗███████╗   ██║   \x1b[0m\n";
    std::cout << "\x1b[0;34m  ╚═╝      ╚═════╝ ╚══════╝╚═╝  ╚═╝\x1b[0m  \x1b[0;35m  ╚═══╝  ╚═╝  ╚═╝╚══════╝╚══════╝╚══════╝   ╚═╝   \x1b[0m\n\n";
}

void SilenceReleaseLogs() {
#ifdef FOLK_VALLEY_RELEASE
    if (g_savedStdout < 0) g_savedStdout = _dup(_fileno(stdout));
    if (g_savedStderr < 0) g_savedStderr = _dup(_fileno(stderr));
    FILE* sink = nullptr;
    freopen_s(&sink, "NUL", "w", stdout);
    freopen_s(&sink, "NUL", "w", stderr);
#endif
}
void RestoreReleaseLogs() {
#ifdef FOLK_VALLEY_RELEASE
    fflush(stdout); fflush(stderr);
    if (g_savedStdout >= 0) _dup2(g_savedStdout, _fileno(stdout));
    if (g_savedStderr >= 0) _dup2(g_savedStderr, _fileno(stderr));
#endif
}
void SignalHandler(int) { g_running = false; }
std::string GetBackendDirectory() { return (g_runtimeRoot / "backend").string(); }
void OpenBrowserToRadar() { ShellExecuteA(nullptr, "open", "http://localhost:8787", nullptr, nullptr, SW_SHOWNORMAL); }

bool StartBackendServer() {
    char nodePath[MAX_PATH] = {};
    if (!SearchPathA(nullptr, "node.exe", nullptr, MAX_PATH, nodePath, nullptr)) {
        RestoreReleaseLogs();
        std::cerr << "Node.js is required. Download it from: https://nodejs.org/en/download\n";
        return false;
    }
    std::string dir = GetBackendDirectory();
    std::string command = "cmd.exe /c cd /d \"" + dir + "\" && \"" + nodePath + "\" src\\server.js";
    STARTUPINFOA si = {}; si.cb = sizeof(si); si.dwFlags = STARTF_USESHOWWINDOW; si.wShowWindow = SW_HIDE;
    g_backendJob = CreateJobObjectA(nullptr, nullptr);
    if (g_backendJob) { JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits = {}; limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE; SetInformationJobObject(g_backendJob, JobObjectExtendedLimitInformation, &limits, sizeof(limits)); }
    if (!CreateProcessA(nullptr, const_cast<char*>(command.c_str()), nullptr, nullptr, FALSE, CREATE_NEW_CONSOLE, nullptr, nullptr, &si, &g_backendProcess)) {
        RestoreReleaseLogs(); std::cerr << "Failed to start embedded web service.\n"; return false;
    }
    if (g_backendJob) AssignProcessToJobObject(g_backendJob, g_backendProcess.hProcess);
    std::this_thread::sleep_for(std::chrono::seconds(3));
    return true;
}

void StopBackendServer() {
    if (g_backendProcess.hProcess) {
        std::system(("taskkill /PID " + std::to_string(g_backendProcess.dwProcessId) + " /T /F >nul 2>&1").c_str());
        CloseHandle(g_backendProcess.hProcess); CloseHandle(g_backendProcess.hThread); g_backendProcess = {};
    }
    if (g_backendJob) { CloseHandle(g_backendJob); g_backendJob = nullptr; }
}

int main(int argc, char* argv[]) {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    EnableLanAccess();
#ifdef FOLK_VALLEY_RELEASE
    PrintStartupBanner();
    const auto ip = GetLanAddress();
    std::cout << "  SHARE THIS URL WITH FRIENDS: http://" << (ip.empty() ? "localhost" : ip) << ":8787\n";
    std::cout << "  [Main] Waiting for backend to initialize...\n";
    SilenceReleaseLogs();
#else
    PrintStartupBanner();
#endif
    g_runtimeRoot = PrepareEmbeddedRuntime();
    if (!StartBackendServer()) return 1;

    std::string host = "127.0.0.1"; int port = 3000;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--host" && i + 1 < argc) host = argv[++i];
        else if (arg == "--port" && i + 1 < argc) port = std::atoi(argv[++i]);
        else if (arg == "--help" || arg == "-h") { RestoreReleaseLogs(); std::cout << "Usage: Folk Valley Radar [--host <host>] [--port <port>]\n"; StopBackendServer(); return 0; }
    }
    auto source = std::make_unique<CS2ProcessDataSource>();
    if (!source->Initialize()) { RestoreReleaseLogs(); std::cerr << "CS2 was not found or could not be read. Run as Administrator with CS2 open.\n"; StopBackendServer(); return 1; }
    IpcClient ipc;
    if (!ipc.Initialize(host, port)) { RestoreReleaseLogs(); std::cerr << "Could not connect to the embedded web service.\n"; StopBackendServer(); return 1; }
#ifdef FOLK_VALLEY_RELEASE
    RestoreReleaseLogs();
    std::cout << "  [Main] Fully initialized, have fun <3\n";
    std::cout << "  Press Ctrl+C to close Folk Valley Radar.\n";
    SilenceReleaseLogs();
#endif
    std::signal(SIGINT, SignalHandler); std::signal(SIGTERM, SignalHandler);
    std::thread([] { std::this_thread::sleep_for(std::chrono::seconds(2)); OpenBrowserToRadar(); }).detach();
    uint32_t errors = 0; std::string lastMap;
    while (g_running) {
        if (!source->IsValid()) { if (++errors >= 10) break; std::this_thread::sleep_for(std::chrono::seconds(1)); continue; }
        GameSnapshot snapshot;
        if (!source->CaptureSnapshot(snapshot)) { if (++errors >= 10) break; std::this_thread::sleep_for(std::chrono::milliseconds(100)); continue; }
        errors = 0;
        for (auto& player : snapshot.players) DataNormalizer::NormalizePlayer(player);
        DataNormalizer::NormalizeGameState(snapshot.gameState);
        if (snapshot.gameState.mapName[0] && lastMap != snapshot.gameState.mapName) { EnsureSource2OverheadMap(snapshot.gameState.mapName, (g_runtimeRoot / "web").string()); lastMap = snapshot.gameState.mapName; }
        DataNormalizer::CalculateScreenPositions(snapshot);
        if (DataNormalizer::ValidateSnapshot(snapshot)) ipc.SendData(JsonSerializer::SerializeSnapshot(snapshot));
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
    source->Shutdown(); ipc.Shutdown(); StopBackendServer(); return 0;
}
