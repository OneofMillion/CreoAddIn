# LeoWebClient - C++ Web Client for Leo AI Integration

## Overview

The `LeoWebClient` class provides a comprehensive interface for communicating with the Leo desktop application from within your Creo addin. It handles HTTP communication, JSON serialization, and process management for seamless integration.

## Features

- **HTTP Communication**: Built-in HTTP client using WinHTTP for reliable communication
- **JSON Serialization**: Automatic serialization of measurement and assembly data
- **Process Management**: Launch and manage Leo desktop app lifecycle
- **Configuration Management**: Load settings from external configuration files
- **Error Handling**: Comprehensive error handling with callback support
- **Logging**: Built-in logging integration with the existing Creo addin logging system

## Data Structures

### Point3D
Represents 3D coordinates with string values for precision.
```cpp
struct Point3D {
    std::string X;
    std::string Y;
    std::string Z;
};
```

### MeasurementData
Contains face measurement information for part search.
```cpp
struct MeasurementData {
    std::string Area;
    std::string Perimeter;
    std::string Radius;
    std::string Diameter;
    Point3D CenterPoint;
    std::string Normal;
    bool IsHole;
    std::string SurfaceType;
    HoleInfo HoleInfo;
    Point3D ClickLocation;
};
```

### AssemblyData
Contains assembly structure and component information.
```cpp
struct AssemblyData {
    std::string AssemblyRoot;
    std::string UserInstruction;
    std::vector<Child> ChildrenList;
};
```

### Location and Orientation
Represents component positioning with 3x3 orientation matrix.
```cpp
struct Location {
    double X;
    double Y;
    double Z;
};

struct LocationWrapper {
    Location Loc;
    std::vector<std::vector<double>> Orientation; // 3x3 matrix
};
```

## Basic Usage

### 1. Initialize the Web Client

```cpp
#include "LeoWebClient.h"

// Create web client instance
LeoWebClient webClient;

// Optional: Configure custom settings
webClient.SetPort(4000);
webClient.SetHost("localhost");
webClient.SetTimeout(5000);
```

### 2. Check Leo App Status

```cpp
if (webClient.IsLeoAppRunning()) {
    // Leo app is already running
    std::cout << "Leo app is running" << std::endl;
} else {
    // Launch Leo app if not running
    if (webClient.LaunchLeoDesktopApp("Starting Leo for part search...")) {
        std::cout << "Leo app launched successfully" << std::endl;
    } else {
        std::cout << "Failed to launch Leo app: " << webClient.GetLastError() << std::endl;
    }
}
```

### 3. Send Face Measurement Data

```cpp
// Create measurement data
MeasurementData measurementData;
measurementData.Area = "12.57";
measurementData.Perimeter = "12.57";
measurementData.Radius = "2.0";
measurementData.Diameter = "4.0";
measurementData.CenterPoint = Point3D("10.0", "20.0", "30.0");
measurementData.Normal = "0,0,1";
measurementData.IsHole = false;
measurementData.SurfaceType = "Cylinder";
measurementData.ClickLocation = Point3D("15.5", "22.3", "30.0");

// Send data with callbacks
webClient.SendFaceMeasurementData(measurementData,
    [](const HttpResponse& response) {
        // Success callback
        std::cout << "Measurement data sent successfully" << std::endl;
        std::cout << "Response: " << response.Body << std::endl;
    },
    [](const std::string& error) {
        // Error callback
        std::cout << "Error sending measurement data: " << error << std::endl;
    }
);
```

### 4. Send Assembly Data

```cpp
// Create assembly data
AssemblyData assemblyData;
assemblyData.AssemblyRoot = "C:/path/to/assembly.sldasm";
assemblyData.UserInstruction = "";

// Add child components
Child component1;
component1.Name = "Component1";
component1.LocalPath = "C:/path/to/component1.sldprt";

LocationWrapper location1;
location1.Loc = Location(100.0, 200.0, 300.0);
// Set orientation matrix (3x3 identity matrix by default)
location1.Orientation = {{1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, {0.0, 0.0, 1.0}};

component1.Locations.push_back(location1);
assemblyData.ChildrenList.push_back(component1);

// Send assembly data
webClient.SendAssemblyData(assemblyData,
    [](const HttpResponse& response) {
        std::cout << "Assembly data sent successfully" << std::endl;
    },
    [](const std::string& error) {
        std::cout << "Error sending assembly data: " << error << std::endl;
    }
);
```

### 5. Bring Leo App to Foreground

```cpp
webClient.BringLeoAppToForeground(
    [](const HttpResponse& response) {
        std::cout << "Leo app brought to foreground" << std::endl;
    },
    [](const std::string& error) {
        std::cout << "Error bringing Leo app to foreground: " << error << std::endl;
    }
);
```

## Configuration

The web client automatically loads configuration from a `local.properties` file. The file should be placed in one of these locations:

1. **Production**: `%APPDATA%\leo-ai\local.properties`
2. **Development**: `%APPDATA%\Electron\local.properties`
3. **Local**: `local.properties` (in the same directory as the executable)

**Note**: The Leo app installation path is hardcoded to `C:\Program Files\Leo\Leo.exe` and cannot be changed via configuration.

### Configuration File Format

```properties
# Port for Leo desktop app
port=4000

# Note: Leo app installation path is hardcoded to C:\Program Files\Leo\Leo.exe

# Optional: Custom timeout for HTTP requests (in milliseconds)
timeout=5000

# Optional: Custom host for Leo desktop app
host=localhost
```

## API Endpoints

The web client communicates with the Leo desktop app using these endpoints:

- **`POST /receive-data`**: Send face measurement data
- **`POST /v2/receive-data`**: Send assembly data
- **`POST /unminized`**: Bring Leo app to foreground

## Error Handling

The web client provides comprehensive error handling:

```cpp
// Check for errors
if (!webClient.IsConnected()) {
    std::string error = webClient.GetLastError();
    std::cout << "Connection error: " << error << std::endl;
}

// Use error callbacks for async operations
webClient.SendFaceMeasurementData(measurementData,
    nullptr, // No success callback
    [](const std::string& error) {
        // Handle specific error
        if (error.find("timeout") != std::string::npos) {
            std::cout << "Request timed out, retrying..." << std::endl;
        } else if (error.find("connection") != std::string::npos) {
            std::cout << "Connection failed, check if Leo app is running" << std::endl;
        }
    }
);
```

## Logging

The web client integrates with the existing Creo addin logging system:

```cpp
// Enable/disable logging
webClient.SetLoggingEnabled(true);

// Logs are automatically written to the log file
// Format: "LeoWebClient: [message]"
```

## Thread Safety

The web client is designed for single-threaded use within the Creo addin context. For multi-threaded scenarios, consider:

- Creating separate instances for different threads
- Using proper synchronization mechanisms
- Implementing thread-safe callback handling

## Performance Considerations

- **HTTP Timeouts**: Default timeout is 5 seconds, adjust based on network conditions
- **Connection Reuse**: The client creates new connections for each request
- **Memory Management**: Large assembly data structures are serialized efficiently
- **Error Recovery**: Built-in retry logic for failed requests

## Troubleshooting

### Common Issues

1. **Leo App Not Starting**
   - Verify Leo app is installed at C:\Program Files\Leo\Leo.exe
   - Check Windows firewall settings
   - Ensure proper permissions

2. **Connection Failures**
   - Verify port configuration (default: 4000)
   - Check if Leo app is running
   - Test network connectivity

3. **Serialization Errors**
   - Validate data structure integrity
   - Check for invalid characters in strings
   - Ensure proper initialization of all fields

### Debug Mode

Enable detailed logging for troubleshooting:

```cpp
webClient.SetLoggingEnabled(true);

// Check connection status
if (webClient.IsConnected()) {
    std::cout << "Connected to Leo app" << std::endl;
} else {
    std::cout << "Not connected: " << webClient.GetLastError() << std::endl;
}
```

## Integration with Creo Addin

The web client is designed to integrate seamlessly with your Creo addin:

```cpp
// In your Creo addin command handler
void CLeoCreoAddinApp::OnSearchComponentWithFace()
{
    // Get selected face from Creo
    ProSelection faceSelection = GetSelectedFace();
    if (faceSelection != nullptr) {
        // Extract measurement data
        MeasurementData measurementData = ExtractFaceMeasurements(faceSelection);
        
        // Send to Leo app
        m_webClient.SendFaceMeasurementData(measurementData);
    }
}
```

## Dependencies

- **WinHTTP**: Windows HTTP client library (automatically linked)
- **Standard C++**: STL containers and algorithms
- **MFC**: For Windows integration (already included in Creo addin)

## Building

The web client is automatically included in your Creo addin project. Ensure that:

1. `LeoWebClient.h` and `LeoWebClient.cpp` are included in your project
2. `winhttp.lib` is linked (automatically added to project dependencies)
3. Your project includes the necessary Windows headers

## Example Project Structure

```
LeoCreoAddin/
├── LeoWebClient.h          # Header file
├── LeoWebClient.cpp        # Implementation
├── LeoWebClient_README.md  # This documentation
├── local.properties.example # Example configuration
├── LeoCreoAddin.cpp        # Main addin implementation
├── LeoCreoAddin.h          # Main addin header
└── LeoConfig.h             # Configuration constants
```

## Support

For issues or questions regarding the LeoWebClient:

1. Check the log files for detailed error information
2. Verify configuration file settings
3. Test network connectivity to localhost:4000
4. Ensure Leo desktop app is running and accessible

The web client provides a robust foundation for integrating your Creo addin with the Leo AI system, enabling seamless part search and retrieval workflows.
