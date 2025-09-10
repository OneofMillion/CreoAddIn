#include "stdafx.h"
#include "LeoWebClient.h"
#include <winhttp.h>
#include <shellapi.h>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <regex>

#pragma comment(lib, "winhttp.lib")

// Initialize static constants
const CString LeoWebClient::DEFAULT_HOST = L"localhost";


LeoWebClient::LeoWebClient()
    : m_host(DEFAULT_HOST)
    , m_port(DEFAULT_PORT)
    , m_timeoutMs(DEFAULT_TIMEOUT_MS)
    , m_isConnected(false)
    , m_loggingEnabled(true)
    , m_defaultPort(DEFAULT_PORT)
    , m_defaultTimeout(DEFAULT_TIMEOUT_MS)
{
}

LeoWebClient::~LeoWebClient()
{
    // Cleanup if needed
}

void LeoWebClient::SetPort(int port)
{
    m_port = port;
    CString str;
    str.Format(_T("%d"), port);
    LogMessage(_T("Port set to: ") + str);
}

void LeoWebClient::SetHost(const CString& host)
{
    m_host = host;
    LogMessage(L"Host set to: " + host);
}

void LeoWebClient::SetTimeout(int timeoutMs)
{
    m_timeoutMs = timeoutMs;
    CString str;
    str.Format(_T("%d"), timeoutMs);
    LogMessage(_T("Timeout set to: ") + str + _T("ms"));
}

bool LeoWebClient::IsLeoAppRunning()
{
    // Try to connect to the Leo app to check if it's running
    try {
        HttpResponse response;
        bool success = SendHttpRequest(L"/", L"{}", 
            [&response](const HttpResponse& resp) { response = resp; },
            [](const CString& error) { /* ignore errors */ });
        
        m_isConnected = success && response.StatusCode == 200;
    } catch (...) {
        m_isConnected = false;
    }

    return m_isConnected;
}

int LeoWebClient::LaunchLeoDesktopApp(const CString& message)
{
    try {
        // Check if Leo app is already running
        if (IsLeoAppRunning()) {
            LogMessage(L"Leo app is already running");
            return -1;
        }
        
        // Launch the Leo desktop app
        SHELLEXECUTEINFO sei = {0};
        sei.cbSize = sizeof(SHELLEXECUTEINFO);
        sei.lpVerb = L"open";
        sei.lpFile = LEO_DESKTOP_EXE;
        sei.nShow = SW_SHOWNORMAL;
        
        if (ShellExecuteEx(&sei)) {
            LogMessage(L"Leo desktop app launched successfully");
            
            // Wait a bit for the app to start
            Sleep(2000);
            
            // Check if it's now running
            //m_isConnected = IsLeoAppRunning();
            return 1;
        } else {
            m_lastError = L"Failed to launch Leo app.";
            LogMessage(m_lastError);
            return 0;
        }
    } catch (const std::exception& e) {
        m_lastError = L"Exception launching Leo app: " + CString(e.what());
        LogMessage(m_lastError);
        return 0;
    }
}

bool LeoWebClient::SendFaceMeasurementData(const MeasurementData& data, 
                                          SuccessCallback successCallback,
                                          ErrorCallback errorCallback)
{
    try {
        CString jsonData = SerializeMeasurementData(data);
        LogMessage(L"Sending face measurement data: " + jsonData);
        
        return SendHttpRequest(L"/receive-data", jsonData, successCallback, errorCallback);
    } catch (const std::exception& e) {
        CString error = L"Exception sending face measurement data: " + CString(e.what());
        m_lastError = error;
        LogMessage(error);
        if (errorCallback) errorCallback(error);
        return false;
    }
}

bool LeoWebClient::SendAssemblyData(const AssemblyData& data,
                                   SuccessCallback successCallback,
                                   ErrorCallback errorCallback)
{
    try {
        CString jsonData = SerializeAssemblyData(data);
        LogMessage(L"Sending assembly data: " + jsonData);
        
        return SendHttpRequest(L"/v2/receive-data", jsonData, successCallback, errorCallback);
    } catch (const std::exception& e) {
        CString error = L"Exception sending assembly data: " + CString(e.what());
        m_lastError = error;
        LogMessage(error);
        if (errorCallback) errorCallback(error);
        return false;
    }
}

bool LeoWebClient::BringLeoAppToForeground(SuccessCallback successCallback,
                                          ErrorCallback errorCallback)
{
    try {
        LogMessage(L"Bringing Leo app to foreground");
        return SendHttpRequest(L"/unminized", L"{}", successCallback, errorCallback);
    } catch (const std::exception& e) {
        CString error = L"Exception bringing Leo app to foreground: " + CString(e.what());
        m_lastError = error;
        LogMessage(error);
        if (errorCallback) errorCallback(error);
        return false;
    }
}

CString LeoWebClient::GetLastError() const
{
    return m_lastError;
}

bool LeoWebClient::IsConnected() const
{
    return m_isConnected;
}

void LeoWebClient::SetLoggingEnabled(bool enabled)
{
    m_loggingEnabled = enabled;
}

bool LeoWebClient::SendHttpRequest(const CString& endpoint, 
                                  const CString& jsonData,
                                  SuccessCallback successCallback,
                                  ErrorCallback errorCallback)
{
    HINTERNET hSession = NULL;
    HINTERNET hConnect = NULL;
    HINTERNET hRequest = NULL;
    
    try {
        // Initialize WinHTTP session
        hSession = WinHttpOpen(L"LeoCreoAddin/1.0",
                              WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                              WINHTTP_NO_PROXY_NAME,
                              WINHTTP_NO_PROXY_BYPASS, 0);
        
        if (!hSession) {
            CString error = GetLastError();
            CString errorMsg;
            errorMsg.Format(L"Failed to initialize WinHTTP session. Error: %s", (LPCTSTR)error);
            throw std::runtime_error(CT2A(errorMsg));
        }
        
        // Set timeouts
        DWORD timeout = static_cast<DWORD>(m_timeoutMs);
        WinHttpSetOption(hSession, WINHTTP_OPTION_CONNECT_TIMEOUT, &timeout, sizeof(timeout));
        WinHttpSetOption(hSession, WINHTTP_OPTION_RECEIVE_TIMEOUT, &timeout, sizeof(timeout));
        WinHttpSetOption(hSession, WINHTTP_OPTION_SEND_TIMEOUT, &timeout, sizeof(timeout));
        
        // Connect to the server using member variables
        hConnect = WinHttpConnect(hSession, m_host, static_cast<INTERNET_PORT>(m_port), 0);
        
        if (!hConnect) {
            CString error = GetLastError();
            CString errorMsg;
            errorMsg.Format(L"Failed to connect to %s:%d. Error: %s", (LPCTSTR)m_host, m_port, (LPCTSTR)error);
            throw std::runtime_error(CT2A(errorMsg));
        }
        
        // Create HTTP request (use 0 for HTTP, not WINHTTP_FLAG_SECURE)
        hRequest = WinHttpOpenRequest(hConnect, L"POST", endpoint,
                                    NULL, WINHTTP_NO_REFERER,
                                    WINHTTP_DEFAULT_ACCEPT_TYPES,
                                    0); // 0 for HTTP, WINHTTP_FLAG_SECURE for HTTPS
        
        if (!hRequest) {
            CString error = GetLastError();
            CString errorMsg;
            errorMsg.Format(L"Failed to create HTTP request. Error: %s", (LPCTSTR)error);
            throw std::runtime_error(CT2A(errorMsg));
        }
        
        // Set Content-Type header
        if (!WinHttpAddRequestHeaders(hRequest,
                                    L"Content-Type: application/json",
                                    -1,
                                    WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE)) {
            CString error = GetLastError();
            CString errorMsg;
            errorMsg.Format(L"Failed to add request headers. Error: %s", (LPCTSTR)error);
            throw std::runtime_error(CT2A(errorMsg));
        }

        // Convert JSON data to UTF-8
        CT2CA utf8Json(jsonData, CP_UTF8);
        DWORD jsonLength = static_cast<DWORD>(strlen(utf8Json));

        // Send the request
        BOOL sendResult = WinHttpSendRequest(hRequest,
                                           WINHTTP_NO_ADDITIONAL_HEADERS,
                                           0,
                                           (LPVOID)(const char*)utf8Json,
                                           jsonLength,
                                           jsonLength,
                                           0);

        if (!sendResult) {
            CString error = GetLastError();
            CString errorMsg;
            errorMsg.Format(L"Failed to send HTTP request. Error: %s", (LPCTSTR)error);
            throw std::runtime_error(CT2A(errorMsg));
        }
        
        // Receive the response
        if (!WinHttpReceiveResponse(hRequest, NULL)) {
            CString error = GetLastError();
            CString errorMsg;
            errorMsg.Format(L"Failed to receive HTTP response. Error: %s", (LPCTSTR)error);
            throw std::runtime_error(CT2A(errorMsg));
        }
        
        // Get response status code
        DWORD statusCode = 0;
        DWORD statusCodeSize = sizeof(statusCode);
        if (!WinHttpQueryHeaders(hRequest, 
                               WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                               WINHTTP_HEADER_NAME_BY_INDEX, 
                               &statusCode, 
                               &statusCodeSize, 
                               WINHTTP_NO_HEADER_INDEX)) {
            statusCode = 0; // Default if we can't get status code
        }
        
        // Read response body
        CString responseBody;
        DWORD bytesAvailable = 0;
        
        do {
            bytesAvailable = 0;
            if (WinHttpQueryDataAvailable(hRequest, &bytesAvailable) && bytesAvailable > 0) {
                // Allocate buffer for this chunk
                std::vector<char> buffer(bytesAvailable + 1, 0);
                DWORD bytesRead = 0;
                
                if (WinHttpReadData(hRequest, buffer.data(), bytesAvailable, &bytesRead) && bytesRead > 0) {
                    // Convert from UTF-8 to CString
                    CA2T wideBuffer(buffer.data(), CP_UTF8);
                    responseBody += CString(wideBuffer);
                }
            }
        } while (bytesAvailable > 0);
        
        // Cleanup handles
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        hRequest = hConnect = hSession = NULL;
        
        // Create response object
        HttpResponse response;
        response.StatusCode = static_cast<int>(statusCode);
        response.Body = responseBody;
        response.Success = (statusCode >= 200 && statusCode < 300);
        
        // Log and handle response
        CString statusStr;
        statusStr.Format(L"%d", statusCode);
        
        if (response.Success) {
            LogMessage(L"HTTP request successful. Status: " + statusStr + L", Endpoint: " + endpoint);
            if (successCallback) {
                successCallback(response);
            }
        } else {
            CString error = L"HTTP request failed. Status: " + statusStr + L", Endpoint: " + endpoint;
            if (!responseBody.IsEmpty()) {
                error += L", Response: " + responseBody;
            }
            m_lastError = error;
            LogMessage(error);
            if (errorCallback) {
                errorCallback(error);
            }
        }
        
        return response.Success;
        
    } catch (const std::exception& e) {
        // Cleanup handles in case of exception
        if (hRequest) WinHttpCloseHandle(hRequest);
        if (hConnect) WinHttpCloseHandle(hConnect);
        if (hSession) WinHttpCloseHandle(hSession);
        
        CString error = L"HTTP request exception: " + CString(e.what());
        m_lastError = error;
        LogMessage(error);
        
        if (errorCallback) {
            errorCallback(error);
        }
        return false;
    }
}

CString LeoWebClient::SerializeMeasurementData(const MeasurementData& data)
{
    std::ostringstream json;
    json << "{";
    json << "\"Area\":\"" << (const char*)CT2A(data.Area) << "\", ";
    json << "\"Perimeter\":\"" << (const char*)CT2A(data.Perimeter) << "\", ";
    json << "\"Radius\":\"" << (const char*)CT2A(data.Radius) << "\", ";
    json << "\"Diameter\":\"" << (const char*)CT2A(data.Diameter) << "\", ";
    json << "\"CenterPoint\": { \"x\": \"" << (const char*)CT2A(data.CenterPoint.X) << "\", \"y\": \"" << (const char*)CT2A(data.CenterPoint.Y) << "\", \"z\": \"" << (const char*)CT2A(data.CenterPoint.Z) << "\" }, ";
    json << "\"Normal\":\"" << (const char*)CT2A(data.Normal) << "\", ";
    json << "\"IsHole\":" << (data.IsHole ? "true" : "false") << ", ";
    json << "\"SurfaceType\":\"" << (const char*)CT2A(data.SurfaceType) << "\", ";
    
    if (data.IsHole) {
        json << "\"HoleInfo\":";
        json << "{";
        json << "\"ThreadSize\":\"" << (const char*)CT2A(data.HoleInfo.ThreadSize) << "\",";
        json << "\"HoleDiameter\":\"" << (const char*)CT2A(data.HoleInfo.HoleDiameter) << "\",";
        json << "\"HoleDepth\":\"" << (const char*)CT2A(data.HoleInfo.HoleDepth) << "\",";
        json << "\"Standard\":\"" << (const char*)CT2A(data.HoleInfo.Standard) << "\",";
        json << "\"ThreadClass\":\"" << (const char*)CT2A(data.HoleInfo.ThreadClass) << "\",";
        json << "\"HoleType\":\"" << (const char*)CT2A(data.HoleInfo.HoleType) << "\"";
        json << "}, ";
    } else {
        json << "\"HoleInfo\": null, ";
    }
    
    json << "\"ClickLocation\": { \"x\": \"" << (const char*)CT2A(data.ClickLocation.X) << "\", \"y\": \"" << (const char*)CT2A(data.ClickLocation.Y) << "\", \"z\": \"" << (const char*)CT2A(data.ClickLocation.Z) << "\" }";
    json << "}";

    LogMessage(CString(json.str().c_str()));

    return CString(json.str().c_str());
}

CString LeoWebClient::SerializeAssemblyData(const AssemblyData& data)
{
    std::ostringstream json;
    json << "{";
    json << "\"AssemblyRoot\":\"" << data.AssemblyRoot << "\",";
    json << "\"UserInstruction\":\"" << data.UserInstruction << "\",";
    json << "\"ChildrenList\":[";
    
    for (size_t i = 0; i < data.ChildrenList.size(); ++i) {
        const auto& child = data.ChildrenList[i];
        if (i > 0) json << ",";
        
        json << "{";
        json << "\"Name\":\"" << child.Name << "\",";
        json << "\"LocalPath\":\"" << child.LocalPath << "\",";
        json << "\"Locations\":[";
        
        for (size_t j = 0; j < child.Locations.size(); ++j) {
            const auto& location = child.Locations[j];
            if (j > 0) json << ",";
            
            json << "{";
            json << "\"Loc\":" << SerializeLocation(location.Loc) << ",";
            json << "\"Orientation\":" << SerializeOrientationMatrix(location.Orientation);
            json << "}";
        }
        
        json << "]";
        json << "}";
    }
    
    json << "]";
    json << "}";

    return CString(json.str().c_str());
}

CString LeoWebClient::SerializePoint3D(const Point3D& point)
{
    std::ostringstream json;
    json << "{\"x\":\"" << point.X << "\",\"y\":\"" << point.Y << "\",\"z\":\"" << point.Z << "\"}";
    return CString(json.str().c_str());
}

CString LeoWebClient::SerializeHoleInfo(const HoleInfo& holeInfo)
{
    std::ostringstream json;
    json << "{";
    json << "\"ThreadSize\":\"" << (const char*)CT2A(holeInfo.ThreadSize) << "\",";
    json << "\"HoleDiameter\":\"" << (const char*)CT2A(holeInfo.HoleDiameter) << "\",";
    json << "\"HoleDepth\":\"" << (const char*)CT2A(holeInfo.HoleDepth) << "\",";
    json << "\"Standard\":\"" << (const char*)CT2A(holeInfo.Standard) << "\",";
    json << "\"ThreadClass\":\"" << (const char*)CT2A(holeInfo.ThreadClass) << "\",";
    json << "\"HoleType\":\"" << (const char*)CT2A(holeInfo.HoleType) << "\"";
    json << "}";
    return CString(json.str().c_str());
}

CString LeoWebClient::SerializeLocation(const Location& location)
{
    std::ostringstream json;
    json << "{\"x\":" << location.X << ",\"y\":" << location.Y << ",\"z\":" << location.Z << "}";
    return CString(json.str().c_str());
}

CString LeoWebClient::SerializeOrientationMatrix(const std::vector<std::vector<double>>& matrix)
{
    std::ostringstream json;
    json << "[";
    
    for (size_t i = 0; i < matrix.size(); ++i) {
        if (i > 0) json << ",";
        json << "[";
        
        for (size_t j = 0; j < matrix[i].size(); ++j) {
            if (j > 0) json << ",";
            json << matrix[i][j];
        }
        
        json << "]";
    }
    
    json << "]";
    return CString(json.str().c_str());
}

bool LeoWebClient::ParseHttpResponse(const CString& response, HttpResponse& httpResponse)
{
    // Simple response parsing - in a real implementation, you might want to use a JSON parser
    try {
        // For now, just check if response contains success indicators

        CString lowerResponse = response;
        for (int i = 0; i < lowerResponse.GetLength(); ++i) {
            lowerResponse.SetAt(i, tolower(lowerResponse[i]));
        }
        
        httpResponse.Body = response;
        httpResponse.Success = (lowerResponse.Find(_T("success")) != -1 ||
                               lowerResponse.Find(_T("ok")) != -1 ||
                               response.GetLength() > 0);
        
        return true;
    } catch (...) {
        return false;
    }
}

void LeoWebClient::LogMessage(const CString& message)
{
    if (!m_loggingEnabled) return;

    try {
        // Convert CString (wide) to std::wstring, then to std::string (UTF-8 or ANSI as needed)
        // But since WriteLog expects const char*, use CT2A for conversion
        CT2A logMsgA(_T("LeoWebClient: ") + message);
        LogFileWriter::WriteLog((const char*)logMsgA);
    }
    catch (...) {
        // Silently fail if logging fails
    }
}