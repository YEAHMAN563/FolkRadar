#include "../include/ipc_client.h"
#include <Windows.h>
#include <winhttp.h>
#include <iostream>

#pragma comment(lib, "winhttp.lib")

struct IpcClient::Impl {
    HINTERNET hSession = nullptr;
    HINTERNET hConnect = nullptr;
    std::wstring wideHost;
};

IpcClient::IpcClient() : m_impl(std::make_unique<Impl>()) {
}

IpcClient::~IpcClient() {
    Shutdown();
}

bool IpcClient::Initialize(const std::string& host, int port) {
    if (m_initialized) {
        return true;
    }

    m_host = host;
    m_port = port;

    // Convert host to wide string
    int wideSize = MultiByteToWideChar(CP_UTF8, 0, host.c_str(), -1, nullptr, 0);
    m_impl->wideHost.resize(wideSize);
    MultiByteToWideChar(CP_UTF8, 0, host.c_str(), -1, &m_impl->wideHost[0], wideSize);

    // Initialize WinHTTP session
    m_impl->hSession = WinHttpOpen(
        L"FolkValleyRadar/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0
    );

    if (!m_impl->hSession) {
        m_lastError = "Failed to initialize WinHTTP session";
        return false;
    }

    // Set timeouts (in milliseconds)
    int timeout = 5000; // 5 seconds
    WinHttpSetTimeouts(m_impl->hSession, timeout, timeout, timeout, timeout);

    // Connect to backend
    m_impl->hConnect = WinHttpConnect(
        m_impl->hSession,
        m_impl->wideHost.c_str(),
        static_cast<INTERNET_PORT>(port),
        0
    );

    if (!m_impl->hConnect) {
        m_lastError = "Failed to connect to backend";
        Shutdown();
        return false;
    }

    m_initialized = true;
    std::cout << "[IpcClient] Connected to " << host << ":" << port << std::endl;
    return true;
}

bool IpcClient::SendData(const std::string& jsonData) {
    if (!m_initialized) {
        m_lastError = "Not initialized";
        return false;
    }

    // Open HTTP request
    HINTERNET hRequest = WinHttpOpenRequest(
        m_impl->hConnect,
        L"POST",
        L"/api/radar",
        nullptr,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        0
    );

    if (!hRequest) {
        m_lastError = "Failed to create HTTP request";
        return false;
    }

    // Set headers
    std::wstring headers = L"Content-Type: application/json\r\n";
    
    // Send request
    bool success = WinHttpSendRequest(
        hRequest,
        headers.c_str(),
        static_cast<DWORD>(headers.length()),
        const_cast<char*>(jsonData.c_str()),
        static_cast<DWORD>(jsonData.length()),
        static_cast<DWORD>(jsonData.length()),
        0
    );

    if (!success) {
        m_lastError = "Failed to send HTTP request";
        WinHttpCloseHandle(hRequest);
        return false;
    }

    // Receive response
    success = WinHttpReceiveResponse(hRequest, nullptr);
    if (!success) {
        m_lastError = "Failed to receive HTTP response";
        WinHttpCloseHandle(hRequest);
        return false;
    }

    // Preserve the backend's validation message so a rejected snapshot is
    // actionable instead of being reported only as a numeric status code.
    std::string responseBody;
    DWORD available = 0;
    while (WinHttpQueryDataAvailable(hRequest, &available) && available > 0) {
        std::string chunk(available, '\0');
        DWORD received = 0;
        if (!WinHttpReadData(hRequest, chunk.data(), available, &received) || received == 0) {
            break;
        }
        responseBody.append(chunk.data(), received);
    }

    // Check status code
    DWORD statusCode = 0;
    DWORD statusCodeSize = sizeof(statusCode);
    WinHttpQueryHeaders(
        hRequest,
        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX,
        &statusCode,
        &statusCodeSize,
        WINHTTP_NO_HEADER_INDEX
    );

    WinHttpCloseHandle(hRequest);

    if (statusCode != 200) {
        m_lastError = "HTTP error: " + std::to_string(statusCode);
        if (!responseBody.empty()) {
            m_lastError += ": " + responseBody;
        }
        return false;
    }

    return true;
}

bool IpcClient::IsConnected() const {
    return m_initialized && m_impl->hConnect != nullptr;
}

std::string IpcClient::GetLastError() const {
    return m_lastError;
}

void IpcClient::Shutdown() {
    if (m_impl->hConnect) {
        WinHttpCloseHandle(m_impl->hConnect);
        m_impl->hConnect = nullptr;
    }

    if (m_impl->hSession) {
        WinHttpCloseHandle(m_impl->hSession);
        m_impl->hSession = nullptr;
    }

    m_initialized = false;
    std::cout << "[IpcClient] Shutdown complete" << std::endl;
}
