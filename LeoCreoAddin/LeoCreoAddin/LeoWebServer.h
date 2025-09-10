#pragma once

#include <afxwin.h>
#include <afxinet.h>
#include <afxmt.h>
#include <vector>
#include <functional>
#include <thread>
#include <atomic>
#include <map>
#include <memory>

// Forward declarations
struct FileDownloadInfo;
struct LocationInfo;
struct Location;

// HTTP request structure
struct HttpRequest {
    CString Method;
    CString Path;
    CString Body;
    std::map<CString, CString> Headers;
    
    HttpRequest() = default;
};

// Web server response structure
struct WebServerResponse {
    int StatusCode;
    CString Body;
    CString ContentType;
    
    WebServerResponse() : StatusCode(200), ContentType(_T("text/html")) {}
};

// Callback function types for file processing
using FileProcessingCallback = std::function<void(const FileDownloadInfo&)>;
using RequestHandlerCallback = std::function<WebServerResponse(const HttpRequest&)>;

// Leo Web Server class for receiving part opening requests
class LeoWebServer {
public:
    LeoWebServer();
    ~LeoWebServer();
    
    // Server management
    bool StartServer(int port = 4100);
    void StopServer();
    bool IsRunning() const;
    
    // Configuration
    void SetPort(int port);
    int GetPort() const;
    
    // File processing callback
    void SetFileProcessingCallback(FileProcessingCallback callback);
    
    // Request handling
    void SetRequestHandler(RequestHandlerCallback callback);
    
    // Utility methods
    CString GetLastError() const;
    void SetLoggingEnabled(bool enabled);
    
private:
    // Server thread function
    void ServerThread();
    
    // Request handling methods
    WebServerResponse HandleRequest(const HttpRequest& request);
    WebServerResponse HandlePartOpeningRequest(const CString& requestBody);
    WebServerResponse HandleHealthCheck();
    
    // JSON parsing methods
    bool ParseFileDownloadInfo(const CString& jsonData, FileDownloadInfo& fileInfo);
    bool ParseLocationInfo(const CString& jsonData, LocationInfo& locationInfo);
    bool ParseLocation(const CString& jsonData, Location& location);
    bool ParseOrientationMatrix(const CString& jsonData, std::vector<std::vector<double>>& matrix);
    
    // Utility methods
    CString CreateSuccessResponse();
    CString CreateErrorResponse(const CString& errorMessage);
    void LogMessage(const CString& message);
    CString ExtractJsonValue(const CString& jsonData, const CString& key);
    
    // Member variables
    int m_port;
    std::atomic<bool> m_isRunning;
    std::atomic<bool> m_shouldStop;
    std::thread m_serverThread;
    CString m_lastError;
    bool m_loggingEnabled;
    
    // Callbacks
    FileProcessingCallback m_fileProcessingCallback;
    RequestHandlerCallback m_requestHandlerCallback;
    
    // Constants
    static const int DEFAULT_PORT = 4100;
    static const int MAX_REQUEST_SIZE = 8192;
    static const CString DEFAULT_RESPONSE;
    
    // Simple HTTP server implementation
class SimpleHttpServer;
std::unique_ptr<SimpleHttpServer> m_impl;
};

// Simple HTTP Server implementation using Windows sockets
class LeoWebServer::SimpleHttpServer {
public:
    SimpleHttpServer();
    ~SimpleHttpServer();
    
    bool Start(int port);
    void Stop();
    bool IsRunning() const;
    
    // Request handling
    bool WaitForRequest(HttpRequest& request);
    bool SendResponse(const WebServerResponse& response);
    
private:
    // Server state
    bool m_isRunning;
    int m_port;
    SOCKET m_serverSocket;
    SOCKET m_lastClientSocket;
    
    // Initialize simple HTTP server
    bool InitializeServer();
    void CleanupServer();
    
    // Socket operations
    bool ReadRequest(SOCKET clientSocket, HttpRequest& request);
    bool SendHttpResponse(SOCKET clientSocket, const WebServerResponse& response);
    
    // HTTP parsing
    bool ParseHttpRequest(const CString& rawRequest, HttpRequest& request);
    CString CreateHttpResponse(const WebServerResponse& response);
};

