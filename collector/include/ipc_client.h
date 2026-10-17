#pragma once

#include <string>
#include <memory>

/**
 * IPC Client for sending data to Node.js backend
 * Uses HTTP POST for simplicity and cross-platform compatibility
 */
class IpcClient {
public:
    IpcClient();
    ~IpcClient();

    /**
     * Initialize connection to backend
     * @param host Backend host (e.g., "localhost" or "127.0.0.1")
     * @param port Backend port (e.g., 3000)
     * @return true if initialization successful
     */
    bool Initialize(const std::string& host, int port);

    /**
     * Send JSON data to backend
     * @param jsonData JSON string to send
     * @return true if send successful
     */
    bool SendData(const std::string& jsonData);

    /**
     * Check if connection is healthy
     */
    bool IsConnected() const;

    /**
     * Get last error message
     */
    std::string GetLastError() const;

    /**
     * Shutdown and cleanup
     */
    void Shutdown();

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
    
    std::string m_host;
    int m_port = 0;
    bool m_initialized = false;
    std::string m_lastError;
};
