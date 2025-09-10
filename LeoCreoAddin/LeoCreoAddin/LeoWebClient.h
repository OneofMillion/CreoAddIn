#pragma once

#include <ProWstring.h>
#include <vector>
#include <memory>
#include <functional>
#include "LeoConfig.h" // Leo AI configuration  
#include "LogFileWriter.h"

// Forward declarations
struct Point3D;
struct HoleInfo;
struct MeasurementData;
struct Location;
struct LocationWrapper;
struct Child;
struct AssemblyData;
struct FileDownloadInfo;
struct LocationInfo;

// 3D Point structure for coordinates
struct Point3D {
    CString X;
    CString Y;
    CString Z;
    
    Point3D() : X("0.0"), Y("0.0"), Z("0.0") {}
    Point3D(const CString& x, const CString& y, const CString& z) 
        : X(x), Y(y), Z(z) {}
};

// Hole information structure
struct HoleInfo {
    CString ThreadSize;
    CString HoleDiameter;
    CString HoleDepth;
    CString Standard;
    CString ThreadClass;
    CString HoleType;
    
    HoleInfo() = default;
};

// Face measurement data structure
struct MeasurementData {
    CString Area;
    CString Perimeter;
    CString Radius;
    CString Diameter;
    Point3D CenterPoint;
    CString Normal;
    bool IsHole;
    CString SurfaceType;
    HoleInfo HoleInfo;
    Point3D ClickLocation;
    
    MeasurementData() : IsHole(false) {}
};

// 3D Location structure for component positioning
struct Location {
    double X;
    double Y;
    double Z;
    
    Location() : X(0.0), Y(0.0), Z(0.0) {}
    Location(double x, double y, double z) : X(x), Y(y), Z(z) {}
};

// Location wrapper with orientation matrix
struct LocationWrapper {
    Location Loc;
    std::vector<std::vector<double>> Orientation;
    
    LocationWrapper() {
        // Initialize 3x3 identity matrix
        Orientation = {{1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, {0.0, 0.0, 1.0}};
    }
};

// Child component structure
struct Child {
    CString Name;
    CString LocalPath;
    std::vector<LocationWrapper> Locations;
    
    Child() = default;
};

// Assembly data structure
struct AssemblyData {
    CString AssemblyRoot;
    CString UserInstruction;
    std::vector<Child> ChildrenList;
    
    AssemblyData() = default;
};

// Location information for file placement
struct LocationInfo {
    Location Loc;
    std::vector<std::vector<double>> Orientation;

    LocationInfo() {
        // Initialize 3x3 identity matrix
        Orientation = { {1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, {0.0, 0.0, 1.0} };
    }
};

// File download information
struct FileDownloadInfo {
    CString DownloadPath;
    LocationInfo LocationInfo;
    
    FileDownloadInfo() = default;
};


// HTTP response structure
struct HttpResponse {
    int StatusCode;
    CString Body;
    CString ErrorMessage;
    bool Success;
    
    HttpResponse() : StatusCode(0), Success(false) {}
};

// Callback function types
using SuccessCallback = std::function<void(const HttpResponse&)>;
using ErrorCallback = std::function<void(const CString&)>;

// Leo Web Client class for communicating with Leo desktop app
class LeoWebClient {
public:
    LeoWebClient();
    ~LeoWebClient();
    
    // Configuration methods
    void SetPort(int port);
    void SetHost(const CString& host);
    void SetTimeout(int timeoutMs);
    
    // Core HTTP communication methods
    bool IsLeoAppRunning();
    int LaunchLeoDesktopApp(const CString& message = L"");
    
    // Data sending methods
    bool SendFaceMeasurementData(const MeasurementData& data, 
                                SuccessCallback successCallback = nullptr,
                                ErrorCallback errorCallback = nullptr);
    
    bool SendAssemblyData(const AssemblyData& data,
                         SuccessCallback successCallback = nullptr,
                         ErrorCallback errorCallback = nullptr);
    
    bool BringLeoAppToForeground(SuccessCallback successCallback = nullptr,
                                ErrorCallback errorCallback = nullptr);
    
    // Utility methods
    CString GetLastError() const;
    bool IsConnected() const;
    void SetLoggingEnabled(bool enabled);
    
private:
    // Private helper methods
    bool SendHttpRequest(const CString& endpoint, 
                        const CString& jsonData,
                        SuccessCallback successCallback,
                        ErrorCallback errorCallback);
    
    CString SerializeMeasurementData(const MeasurementData& data);
    CString SerializeAssemblyData(const AssemblyData& data);
    CString SerializePoint3D(const Point3D& point);
    CString SerializeHoleInfo(const HoleInfo& holeInfo);
    CString SerializeLocation(const Location& location);
    CString SerializeOrientationMatrix(const std::vector<std::vector<double>>& matrix);
    
    bool ParseHttpResponse(const CString& response, HttpResponse& httpResponse);
    void LogMessage(const CString& message);
    
    // Member variables
    CString m_host;
    int m_port;
    int m_timeoutMs;
    CString m_lastError;
    bool m_isConnected;
    bool m_loggingEnabled;
    CString m_configFilePath;
    
    // Configuration values
    int m_defaultPort;
    int m_defaultTimeout;
    
    // Constants
    static const int DEFAULT_PORT = 4000;
    static const int DEFAULT_TIMEOUT_MS = 5000;
    static const int MAX_RETRY_COUNT = 3;
    static const CString DEFAULT_HOST;
};