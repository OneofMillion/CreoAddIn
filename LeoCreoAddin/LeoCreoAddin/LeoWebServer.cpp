#include "stdafx.h"
#include "LeoWebServer.h"
#include "LeoWebClient.h"
#include "LogFileWriter.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <sstream>
#include <algorithm>
#include <cstring>

#pragma comment(lib, "ws2_32.lib")

// Static constants
const CString LeoWebServer::DEFAULT_RESPONSE = _T("<html><body><h1>Data Received</h1></body></html>");

// LeoWebServer implementation
LeoWebServer::LeoWebServer()
    : m_port(DEFAULT_PORT)
    , m_isRunning(false)
    , m_shouldStop(false)
    , m_loggingEnabled(true)
    , m_impl(std::make_unique<SimpleHttpServer>())
{
    LogMessage(_T("LeoWebServer: Constructor called"));
}

LeoWebServer::~LeoWebServer()
{
    LogMessage(_T("LeoWebServer: Destructor called"));
    StopServer();
}

bool LeoWebServer::StartServer(int port)
{
    if (m_isRunning) {
        LogMessage(_T("LeoWebServer: Server is already running"));
        return true;
    }
    
    if (port > 0) {
        m_port = port;
    }
    
    // Fix for E2140: Use CString::Format to build the message, then pass to LogMessage
    CString msg;
    msg.Format(_T("LeoWebServer: Starting server on port %d"), m_port);
    LogMessage(msg);
    
    try {
        // Start the simple HTTP server implementation
        if (!m_impl->Start(m_port)) {
            m_lastError = _T("Failed to start HTTP server");
            LogMessage(_T("LeoWebServer: ") + m_lastError);
            return false;
        }
        
        // Start the server thread
        m_shouldStop = false;
        m_serverThread = std::thread(&LeoWebServer::ServerThread, this);
        
        m_isRunning = true;
        CString msg1;
        msg1.Format(_T("LeoWebServer: Server started successfully on port %d"), m_port);
        LogMessage(msg1);
        return true;
        
    } catch (const std::exception& e) {
        m_lastError = CString(_T("Exception starting server: ")) + CString(e.what());
        LogMessage(_T("LeoWebServer: ") + m_lastError);
        return false;
    }
}

void LeoWebServer::StopServer()
{
    if (!m_isRunning) {
        return;
    }
    
    LogMessage(_T("LeoWebServer: Stopping server"));
    
    // Signal the server thread to stop
    m_shouldStop = true;
    
    // Stop the HTTP server implementation
    m_impl->Stop();
    
    // Wait for the server thread to finish
    if (m_serverThread.joinable()) {
        m_serverThread.join();
    }
    
    m_isRunning = false;
    LogMessage(_T("LeoWebServer: Server stopped"));
}

bool LeoWebServer::IsRunning() const
{
    return m_isRunning && m_impl->IsRunning();
}

void LeoWebServer::SetPort(int port)
{
    if (port > 0 && port != m_port) {
        m_port = port;
        CString msg;
        msg.Format(_T("LeoWebServer: Port set to %d"), m_port);
        LogMessage(msg);
    }
}

int LeoWebServer::GetPort() const
{
    return m_port;
}

void LeoWebServer::SetFileProcessingCallback(FileProcessingCallback callback)
{
    m_fileProcessingCallback = callback;
    LogMessage(_T("LeoWebServer: File processing callback set"));
}

void LeoWebServer::SetRequestHandler(RequestHandlerCallback callback)
{
    m_requestHandlerCallback = callback;
    LogMessage(_T("LeoWebServer: Request handler callback set"));
}

CString LeoWebServer::GetLastError() const
{
    return m_lastError;
}

void LeoWebServer::SetLoggingEnabled(bool enabled)
{
    m_loggingEnabled = enabled;
    CString status = enabled ? _T("enabled") : _T("disabled");
    LogMessage(_T("LeoWebServer: Logging ") + status);
}

void LeoWebServer::ServerThread()
{
    LogMessage(_T("LeoWebServer: Server thread started"));
    
    while (!m_shouldStop) {
        try {
            HttpRequest request;
            
            // Wait for incoming requests with zero-latency response
            // The WaitForRequest method now uses select() with 0 timeout
            // This ensures immediate processing of incoming connections
            if (m_impl->WaitForRequest(request)) {
                LogMessage(_T("LeoWebServer: Received request: ") + request.Method + _T(" ") + request.Path);
                
                    // Handle the request
    WebServerResponse response = HandleRequest(request);
                
                // Send the response
                if (!m_impl->SendResponse(response)) {
                    LogMessage(_T("LeoWebServer: Failed to send response"));
                }
            }
            
            // Check for pending connections more frequently
            // This reduces latency while preventing excessive CPU usage
            std::this_thread::sleep_for(std::chrono::microseconds(100)); // 0.1ms delay
            
        } catch (const std::exception& e) {
            LogMessage(_T("LeoWebServer: Exception in server thread: ") + CString(e.what()));
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    
    LogMessage(_T("LeoWebServer: Server thread stopped"));
}

WebServerResponse LeoWebServer::HandleRequest(const HttpRequest& request)
{
    // Check if custom request handler is set
    if (m_requestHandlerCallback) {
        return m_requestHandlerCallback(request);
    }
    
    // Default request handling
    if (request.Method.CompareNoCase(_T("POST")) == 0) {
        if (request.Path.CompareNoCase(_T("/")) == 0) {
            return HandlePartOpeningRequest(request.Body);
        } else if (request.Path.CompareNoCase(_T("/health")) == 0) {
            return HandleHealthCheck();
        }
    } else if (request.Method.CompareNoCase(_T("GET")) == 0) {
        if (request.Path.CompareNoCase(_T("/health")) == 0) {
            return HandleHealthCheck();
        }
    }
    
    // Default response for unknown requests
    WebServerResponse response;
    response.StatusCode = 404;
    response.Body = _T("<html><body><h1>Not Found</h1></body></html>");
    response.ContentType = _T("text/html");
    
    return response;
}

WebServerResponse LeoWebServer::HandlePartOpeningRequest(const CString& requestBody)
{
    LogMessage(_T("LeoWebServer: Handling part opening request"));
    
    WebServerResponse response;
    
    if (requestBody.IsEmpty()) {
        response.StatusCode = 400;
        response.Body = CreateErrorResponse(_T("Request body is empty"));
        response.ContentType = _T("text/html");
        return response;
    }
    
    // Parse the file download information
    FileDownloadInfo fileInfo;
    if (!ParseFileDownloadInfo(requestBody, fileInfo)) {
        response.StatusCode = 400;
        response.Body = CreateErrorResponse(_T("Invalid JSON format in request body"));
        response.ContentType = _T("text/html");
        return response;
    }
    
    // Validate the file path
    if (fileInfo.DownloadPath.IsEmpty()) {
        response.StatusCode = 400;
        response.Body = CreateErrorResponse(_T("Download path is missing"));
        response.ContentType = _T("text/html");
        return response;
    }
    
    // Check if file exists
    if (GetFileAttributes(fileInfo.DownloadPath) == INVALID_FILE_ATTRIBUTES) {
        response.StatusCode = 404;
        response.Body = CreateErrorResponse(_T("File not found: ") + fileInfo.DownloadPath);
        response.ContentType = _T("text/html");
        return response;
    }
    
    // Process the file if callback is set
    if (m_fileProcessingCallback) {
        try {
            m_fileProcessingCallback(fileInfo);
            LogMessage(_T("LeoWebServer: File processing callback executed successfully"));
        } catch (const std::exception& e) {
            LogMessage(_T("LeoWebServer: Exception in file processing callback: ") + CString(e.what()));
            response.StatusCode = 500;
            response.Body = CreateErrorResponse(_T("Internal server error during file processing"));
            response.ContentType = _T("text/html");
            return response;
        }
    } else {
        LogMessage(_T("LeoWebServer: No file processing callback set"));
    }
    
    // Return success response
    response.StatusCode = 200;
    response.Body = CreateSuccessResponse();
    response.ContentType = _T("text/html");
    
    LogMessage(_T("LeoWebServer: Part opening request processed successfully"));
    return response;
}

WebServerResponse LeoWebServer::HandleHealthCheck()
{
    WebServerResponse response;
    response.StatusCode = 200;
    response.Body = _T("<html><body><h1>Leo Web Server is running</h1></body></html>");
    response.ContentType = _T("text/html");
    
    return response;
}

bool LeoWebServer::ParseFileDownloadInfo(const CString& jsonData, FileDownloadInfo& fileInfo)
{
    try {
        // Extract download path
        CString downloadPath = ExtractJsonValue(jsonData, _T("downloadPath"));
        if (!downloadPath.IsEmpty()) {
            fileInfo.DownloadPath = downloadPath;
        }
        
        // Extract location info
        CString locationInfoJson = ExtractJsonValue(jsonData, _T("locationInfo"));
        if (!locationInfoJson.IsEmpty()) {
            LocationInfo locationInfo;
            if (ParseLocationInfo(locationInfoJson, locationInfo)) {
                fileInfo.LocationInfo = locationInfo;
            }
        }
        
        return true;
        
    } catch (const std::exception& e) {
        LogMessage(_T("LeoWebServer: Exception parsing FileDownloadInfo: ") + CString(e.what()));
        return false;
    }
}

bool LeoWebServer::ParseLocationInfo(const CString& jsonData, LocationInfo& locationInfo)
{
    try {
        // Parse location
        CString locationJson = ExtractJsonValue(jsonData, _T("loc"));
        if (!locationJson.IsEmpty()) {
            Location location;
            if (ParseLocation(locationJson, location)) {
                locationInfo.Loc = location;
            }
        }
        
        // Parse orientation matrix
        CString orientationJson = ExtractJsonValue(jsonData, _T("orientation"));
        if (!orientationJson.IsEmpty()) {
            std::vector<std::vector<double>> matrix;
            if (ParseOrientationMatrix(orientationJson, matrix)) {
                locationInfo.Orientation = matrix;
            }
        }
        
        return true;
        
    } catch (const std::exception& e) {
        LogMessage(_T("LeoWebServer: Exception parsing LocationInfo: ") + CString(e.what()));
        return false;
    }
}

bool LeoWebServer::ParseLocation(const CString& jsonData, Location& location)
{
    try {
        // Extract X, Y, Z coordinates
        CString xStr = ExtractJsonValue(jsonData, _T("x"));
        CString yStr = ExtractJsonValue(jsonData, _T("y"));
        CString zStr = ExtractJsonValue(jsonData, _T("z"));
        
        if (!xStr.IsEmpty() && !yStr.IsEmpty() && !zStr.IsEmpty()) {
            location.X = _ttof(xStr);
            location.Y = _ttof(yStr);
            location.Z = _ttof(zStr);
            return true;
        }
        
        return false;
        
    } catch (const std::exception& e) {
        LogMessage(_T("LeoWebServer: Exception parsing Location: ") + CString(e.what()));
        return false;
    }
}

bool LeoWebServer::ParseOrientationMatrix(const CString& jsonData, std::vector<std::vector<double>>& matrix)
{
    try {
        // Simple parsing for 3x3 matrix
        // This is a basic implementation - you might want to use a proper JSON parser
        matrix.clear();
        matrix.resize(3);
        
        for (int i = 0; i < 3; i++) {
            matrix[i].resize(3);
            for (int j = 0; j < 3; j++) {
                matrix[i][j] = (i == j) ? 1.0 : 0.0; // Default to identity matrix
            }
        }
        
        // Try to parse the actual values if they exist
        // This is a simplified approach - in production, use a proper JSON parser
        if (jsonData.Find(_T("[")) >= 0 && jsonData.Find(_T("]")) >= 0) {
            // Basic matrix parsing (simplified)
            // For now, we'll use the default identity matrix
            LogMessage(_T("LeoWebServer: Using default identity matrix for orientation"));
        }
        
        return true;
        
    } catch (const std::exception& e) {
        LogMessage(_T("LeoWebServer: Exception parsing orientation matrix: ") + CString(e.what()));
        return false;
    }
}

CString LeoWebServer::CreateSuccessResponse()
{
    return DEFAULT_RESPONSE;
}

CString LeoWebServer::CreateErrorResponse(const CString& errorMessage)
{
    CString response;
    response.Format(_T("<html><body><h1>Error</h1><p>%s</p></body></html>"), errorMessage);
    return response;
}

void LeoWebServer::LogMessage(const CString& message)
{
    if (m_loggingEnabled) {
        LogFileWriter::WriteLog((const char*)CT2A(_T("LeoWebServer: ") + message));
    }
}

CString LeoWebServer::ExtractJsonValue(const CString& jsonData, const CString& key)
{
    // Simple JSON value extraction
    // This is a basic implementation - in production, use a proper JSON parser
    
    CString searchKey = _T("\"") + key + _T("\":");
    int keyPos = jsonData.Find(searchKey);
    if (keyPos < 0) {
        return _T("");
    }
    
    int valueStart = keyPos + searchKey.GetLength();
    
    // Skip whitespace
    while (valueStart < jsonData.GetLength() && 
           (jsonData[valueStart] == _T(' ') || jsonData[valueStart] == _T('\t'))) {
        valueStart++;
    }
    
    if (valueStart >= jsonData.GetLength()) {
        return _T("");
    }
    
    // Find the end of the value
    int valueEnd = valueStart;
    bool inQuotes = false;
    
    if (jsonData[valueStart] == _T('"')) {
        inQuotes = true;
        valueStart++; // Skip opening quote
        valueEnd = valueStart;
        
        while (valueEnd < jsonData.GetLength() && jsonData[valueEnd] != _T('"')) {
            if (jsonData[valueEnd] == _T('\\') && valueEnd + 1 < jsonData.GetLength()) {
                valueEnd += 2; // Skip escaped character
            } else {
                valueEnd++;
            }
        }
    } else {
        // Find next comma or closing brace/bracket
        while (valueEnd < jsonData.GetLength() && 
               jsonData[valueEnd] != _T(',') && 
               jsonData[valueEnd] != _T('}') && 
               jsonData[valueEnd] != _T(']')) {
            valueEnd++;
        }
    }
    
    if (valueEnd > valueStart) {
        return jsonData.Mid(valueStart, valueEnd - valueStart);
    }
    
    return _T("");
}

// SimpleHttpServer implementation
LeoWebServer::SimpleHttpServer::SimpleHttpServer()
    : m_isRunning(false)
    , m_port(4100)
    , m_serverSocket(INVALID_SOCKET)
    , m_lastClientSocket(INVALID_SOCKET)
{
}

LeoWebServer::SimpleHttpServer::~SimpleHttpServer()
{
    Stop();
}

bool LeoWebServer::SimpleHttpServer::Start(int port)
{
    if (m_isRunning) {
        return true;
    }
    
    m_port = port;
    
    if (!InitializeServer()) {
        return false;
    }
    
    m_isRunning = true;
    return true;
}

void LeoWebServer::SimpleHttpServer::Stop()
{
    if (!m_isRunning) {
        return;
    }
    
    CleanupServer();
    m_isRunning = false;
}

bool LeoWebServer::SimpleHttpServer::IsRunning() const
{
    return m_isRunning;
}

bool LeoWebServer::SimpleHttpServer::InitializeServer()
{
    // Initialize Winsock
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        return false;
    }
    
    // Create socket
    m_serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_serverSocket == INVALID_SOCKET) {
        WSACleanup();
        return false;
    }
    
    // Set socket options
    int opt = 1;
    setsockopt(m_serverSocket, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));
    
    // Bind socket
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(m_port);
    
    if (bind(m_serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        closesocket(m_serverSocket);
        WSACleanup();
        return false;
    }
    
    // Listen for connections
    if (listen(m_serverSocket, SOMAXCONN) == SOCKET_ERROR) {
        closesocket(m_serverSocket);
        WSACleanup();
        return false;
    }
    
    return true;
}

void LeoWebServer::SimpleHttpServer::CleanupServer()
{
    if (m_serverSocket != INVALID_SOCKET) {
        closesocket(m_serverSocket);
        m_serverSocket = INVALID_SOCKET;
    }
    
    if (m_lastClientSocket != INVALID_SOCKET) {
        closesocket(m_lastClientSocket);
        m_lastClientSocket = INVALID_SOCKET;
    }
    
    WSACleanup();
}

bool LeoWebServer::SimpleHttpServer::WaitForRequest(HttpRequest& request)
{
    if (!m_isRunning || m_serverSocket == INVALID_SOCKET) {
        return false;
    }
    
    // Use select with ZERO timeout for immediate response
    fd_set readSet;
    FD_ZERO(&readSet);
    FD_SET(m_serverSocket, &readSet);
    
    timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 0; // ZERO timeout - immediate response
    
    int result = select(0, &readSet, NULL, NULL, &timeout);
    if (result <= 0) {
        return false; // No connection waiting
    }
    
    // Accept connection immediately
    sockaddr_in clientAddr;
    int clientAddrLen = sizeof(clientAddr);
    SOCKET clientSocket = accept(m_serverSocket, (sockaddr*)&clientAddr, &clientAddrLen);
    
    if (clientSocket == INVALID_SOCKET) {
        return false;
    }
    
    // Read request
    bool success = ReadRequest(clientSocket, request);
    
    if (success) {
        // Store client socket for response
        m_lastClientSocket = clientSocket;
    } else {
        closesocket(clientSocket);
    }
    
    return success;
}



bool LeoWebServer::SimpleHttpServer::ReadRequest(SOCKET clientSocket, HttpRequest& request)
{
    char buffer[8192];
    int totalBytesReceived = 0;
    int bytesReceived;
    
    // Set socket to non-blocking mode for reading
    u_long mode = 1;
    ioctlsocket(clientSocket, FIONBIO, &mode);
    
    // Read data with minimal timeout
    fd_set readSet;
    FD_ZERO(&readSet);
    FD_SET(clientSocket, &readSet);
    
    timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 1000; // 1ms timeout for reading
    
    // Check if data is available
    int result = select(0, &readSet, NULL, NULL, &timeout);
    if (result <= 0) {
        return false; // No data available
    }
    
    // Read available data
    bytesReceived = recv(clientSocket, buffer + totalBytesReceived, sizeof(buffer) - totalBytesReceived - 1, 0);
    
    if (bytesReceived > 0) {
        totalBytesReceived += bytesReceived;
        buffer[totalBytesReceived] = '\0';
        
        // Convert from UTF-8 to Unicode properly
        CString rawRequest;
        int wideLen = MultiByteToWideChar(CP_UTF8, 0, buffer, totalBytesReceived, NULL, 0);
        if (wideLen > 0) {
            wchar_t* wideBuffer = new wchar_t[wideLen + 1];
            MultiByteToWideChar(CP_UTF8, 0, buffer, totalBytesReceived, wideBuffer, wideLen);
            wideBuffer[wideLen] = L'\0';
            rawRequest = wideBuffer;
            delete[] wideBuffer;
        } else {
            // Fallback to ANSI if UTF-8 conversion fails
            rawRequest = buffer;
        }
        
        return ParseHttpRequest(rawRequest, request);
    }
    
    return false;
}

bool LeoWebServer::SimpleHttpServer::ParseHttpRequest(const CString& rawRequest, HttpRequest& request)
{
    // Simple HTTP request parsing
    // Find first line (method and path)
    int lineEnd = rawRequest.Find(_T("\r\n"));
    if (lineEnd < 0) {
        lineEnd = rawRequest.Find(_T("\n"));
    }
    
    if (lineEnd < 0) {
        return false;
    }
    
    CString firstLine = rawRequest.Left(lineEnd);
    
    // Parse method and path
    int space1 = firstLine.Find(_T(" "));
    if (space1 < 0) {
        return false;
    }
    
    int space2 = firstLine.Find(_T(" "), space1 + 1);
    if (space2 < 0) {
        return false;
    }
    
    request.Method = firstLine.Left(space1);
    request.Path = firstLine.Mid(space1 + 1, space2 - space1 - 1);
    
    // Find body (after double CRLF)
    int bodyStart = rawRequest.Find(_T("\r\n\r\n"));
    if (bodyStart >= 0) {
        request.Body = rawRequest.Mid(bodyStart + 4);
    }
    
    return true;
}

bool LeoWebServer::SimpleHttpServer::SendResponse(const WebServerResponse& response)
{
    if (m_lastClientSocket == INVALID_SOCKET) {
        return false;
    }
    
    // Create HTTP response
    CString httpResponse = CreateHttpResponse(response);
    
    // Convert to ANSI for sending
    CT2A ansiResponse(httpResponse);
    
    // Send response
    int bytesSent = send(m_lastClientSocket, ansiResponse, (int)strlen(ansiResponse), 0);
    
    // Close client socket
    closesocket(m_lastClientSocket);
    m_lastClientSocket = INVALID_SOCKET;
    
    return (bytesSent > 0);
}

CString LeoWebServer::SimpleHttpServer::CreateHttpResponse(const WebServerResponse& response)
{
    CString httpResponse;
    
    // Status line
    CString statusText;
    switch (response.StatusCode) {
        case 200: statusText = _T("OK"); break;
        case 400: statusText = _T("Bad Request"); break;
        case 404: statusText = _T("Not Found"); break;
        case 500: statusText = _T("Internal Server Error"); break;
        default: statusText = _T("Unknown"); break;
    }
        
    httpResponse.Format(_T("HTTP/1.1 %d %s\r\n"), response.StatusCode, statusText);
    
    // Headers
    httpResponse += _T("Content-Type: ") + response.ContentType + _T("\r\n");
    CString contentLength;
    contentLength.Format(_T("%d"), response.Body.GetLength());
    httpResponse += _T("Content-Length: ") + contentLength + _T("\r\n");
    httpResponse += _T("Connection: close\r\n");


    
    
    // Empty line before body
    httpResponse += _T("\r\n");
    
    // Body
    httpResponse += response.Body;
    
    return httpResponse;
}
