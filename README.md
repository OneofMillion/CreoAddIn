# LeoCreoAddin

A Creo Parametric add-in that integrates with LeoAI for intelligent part search and retrieval functionality.

## Overview

LeoCreoAddin is a C++ toolkit add-in for Creo 11 that enables seamless integration with the Leo AI system. It provides intelligent part search capabilities by analyzing selected faces in Creo and communicating with the Leo desktop application to find and retrieve matching components.

## Features

- **Intelligent Face Analysis**: Select faces in Creo to extract geometric measurements and characteristics
- **AI-Powered Part Search**: Send face data to Leo AI for intelligent component matching
- **Automatic Part Retrieval**: Download and open matching parts directly in Creo
- **Assembly Integration**: Add retrieved parts to existing assemblies with proper positioning
- **Web Server Integration**: Built-in HTTP server for receiving part opening requests
- **Real-time Communication**: Bidirectional communication with Leo desktop app
- **Comprehensive Logging**: Detailed logging system for debugging and monitoring

## Quick Installation

### Step 1: Install the MSI
You can use the pre-built MSI installer or build it yourself.

1. **Close Creo Parametric:** Ensure that Creo Parametric is not running.
2. **Run the installer:** Navigate to the `LeoCreoAddin-installer/` directory and run the `creo_addin_installer.msi` file.
3. Follow the on-screen instructions to complete the installation.
4. Start Creo Parametric. The add-in should be loaded automatically.

---

## System Requirements

- **Creo Parametric 11.0.2.0** (or compatible version)
- **Windows 10/11** (64-bit)
- **Visual C++ Redistributable** (latest version)
- **Leo Desktop Application** (installed and running)
- **Internet Connection** (for AI communication)

---

## For Developers: Building from Source

If you want to build the solution yourself, follow these instructions:

### Prerequisites
To build the add-in and the installer, you will need:
- **Visual Studio 2022** with the "Desktop development with C++" workload installed
- **Creo 11.0.2.0** installed with Pro/TOOLKIT
- **WiX Toolset v6.0.0** installed for Visual Studio 2022 (for building the installer)
- **Windows SDK** (latest version)

### Build Steps
1. Open `LeoCreoAddin.sln` in Visual Studio 2022.
2. Configure the project paths in `LeoCreoAddin.vcxproj` to match your Creo installation:
   - Update include directories to point to your Creo installation
   - Update library directories to point to your Creo Pro/TOOLKIT libraries
3. Build the solution in Release x64 configuration.
4. Build the `LeoCreoAddin-installer` project to create an MSI installer package.

---

## Project Structure

```
LeoCreoAddin/
├── LeoCreoAddin/                 # Main add-in project
│   ├── LeoCreoAddin.cpp         # Main add-in implementation
│   ├── LeoCreoAddin.h           # Main add-in header
│   ├── LeoHelper.cpp/.h         # Creo integration helper functions
│   ├── LeoWebClient.cpp/.h      # HTTP client for Leo AI communication
│   ├── LeoWebServer.cpp/.h      # HTTP server for receiving requests
│   ├── LogFileWriter.cpp/.h     # Logging system
│   ├── LeoConfig.h              # Configuration constants
│   └── Lib/                     # Creo Pro/TOOLKIT libraries
├── LeoCreoAddin-installer/       # MSI installer project
│   ├── Product.wxs              # WiX installer configuration
│   └── creo_addin_installer.msi # Pre-built installer
├── text/                        # Creo text resources
│   ├── LeoCreoAddin.txt         # Menu text definitions
│   ├── resource/                # Icons and images
│   └── ribbon/                  # Ribbon definitions
└── prodev.dat                   # Creo add-in registration file
```

---

## Usage

### Basic Workflow

1. **Open Creo Parametric** with an assembly or part file
2. **Select a face** on the model that you want to search for
3. **Click "Find Component"** in the Leo menu or toolbar
4. **Wait for AI analysis** - the add-in will extract face measurements
5. **Review results** in the Leo desktop application
6. **Select matching parts** from the AI suggestions
7. **Parts are automatically opened** in Creo with proper positioning

### Menu Options

- **Open Leo**: Launches the Leo desktop application
- **Find Component**: Analyzes selected face and searches for matching parts
- **Right-click Menu**: Context menu option for selected faces

### Toolbar Integration

The add-in adds a "Find Component" button to the Creo toolbar that can be customized:
1. Right-click on any toolbar in Creo
2. Select "Customize"
3. Find "LeoCreoAddin" in the Commands tab
4. Drag "Find Component" to your desired toolbar location

---

## Configuration

### Leo Desktop App Integration

The add-in communicates with the Leo desktop application through HTTP:

- **Default Port**: 4000 (Leo app) / 4100 (Creo add-in server)
- **Host**: localhost
- **Protocol**: HTTP with JSON data exchange

### Configuration Files

The add-in looks for configuration in the following locations:
1. `%APPDATA%\leo-ai\local.properties`
2. `%APPDATA%\Electron\local.properties`
3. `local.properties` (in the add-in directory)

### Example Configuration

```properties
# Leo desktop app port
port=4000

# HTTP timeout (milliseconds)
timeout=5000

# Host address
host=localhost
```

---

## API Endpoints

The add-in provides several HTTP endpoints for external communication:

- **`POST /`**: Part opening requests (JSON format)
- **`GET /health`**: Health check endpoint

### Part Opening Request Format

```json
{
  "DownloadPath": "C:\\path\\to\\part.prt",
  "LocationInfo": {
    "Loc": {
      "X": 100.0,
      "Y": 200.0,
      "Z": 300.0
    },
    "Orientation": [
      [1.0, 0.0, 0.0],
      [0.0, 1.0, 0.0],
      [0.0, 0.0, 1.0]
    ]
  }
}
```

---

## Dependencies

### Required Libraries
- **Pro/TOOLKIT** (Creo 11.0.2.0)
- **MFC** (Microsoft Foundation Classes)
- **WinHTTP** (Windows HTTP client)
- **WS2_32** (Windows Sockets)

### NuGet Packages
- None (all dependencies are system libraries)

### Required Reference DLLs
The following files from Creo Pro/TOOLKIT are required:
( These files are originally located in C:\Program Files\PTC\Creo 11.0.2.0\Common Files\protoolkit.
But I moved it to LeoCreoAddin\LeoCreoAddin\Lib so that I can build this project without installing Creo.)
- `protk_dllmd_NU.lib`
- `ucore.lib`
- `udata.lib`
- `psapi.lib`
- `mpr.lib`
- `Netapi32.lib`

---

## Troubleshooting

### Common Issues

1. **Add-in Not Loading**
   - Verify Creo 11.0.2.0 is installed
   - Check that `prodev.dat` is in the correct location
   - Ensure all required DLLs are present

2. **Leo App Communication Failed**
   - Verify Leo desktop app is running
   - Check port configuration (4000 for Leo, 4100 for add-in)
   - Test network connectivity to localhost

3. **Face Selection Not Working**
   - Ensure a face is selected before clicking "Find Component"
   - Check that the selected face is valid (not a surface or edge)
   - Verify the model is not corrupted

4. **Parts Not Opening**
   - Check file paths are valid and accessible
   - Verify Creo can open the file format
   - Check for file permission issues

### Debug Mode

Enable detailed logging by checking the log files:
- Logs are written to the Creo working directory
- Look for `LeoCreoAddin.txt` log files
- Check for error messages and status updates

### Performance Issues

- **Slow Face Analysis**: Large or complex faces may take longer to analyze
- **Network Timeouts**: Increase timeout values in configuration
- **Memory Usage**: Large assemblies may require more system memory

---

## Development

### Adding New Features

1. **New Menu Items**: Add entries to `LeoCreoAddin.txt` and implement handlers
2. **New API Endpoints**: Extend `LeoWebServer.cpp` with new route handlers
3. **New Face Analysis**: Extend `LeoHelper.cpp` with additional measurement functions
4. **New UI Elements**: Add resources to the project and update the ribbon definitions

### Code Structure

- **Main Logic**: `LeoCreoAddin.cpp` - Entry points and main functionality
- **Creo Integration**: `LeoHelper.cpp` - Pro/TOOLKIT wrapper functions
- **Network Communication**: `LeoWebClient.cpp` - HTTP client for Leo AI
- **Server Functionality**: `LeoWebServer.cpp` - HTTP server for external requests
- **Logging**: `LogFileWriter.cpp` - Centralized logging system

### Building and Testing

1. Build in Release x64 configuration
2. Test with various Creo file types (parts, assemblies, drawings)
3. Verify communication with Leo desktop app
4. Test error handling and edge cases
5. Create installer package for distribution

---

## License

This project is open source. Please refer to the license file for details.

---

## Support

For issues, questions, or contributions:

1. Check the troubleshooting section above
2. Review the log files for error details
3. Verify system requirements and dependencies
4. Test with minimal configurations to isolate issues

---

## Version History

- **v1.0.0**: Initial release with basic face analysis and part search functionality
- **v1.0.1**: Added web server integration for external part opening requests
- **v1.0.2**: Enhanced assembly integration and positioning capabilities

---

## Contributing

Contributions are welcome! Please:

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Test thoroughly with Creo 11
5. Submit a pull request

---

*This add-in is designed specifically for Creo 11 and integrates with the Leo AI system for intelligent part search and retrieval capabilities.*
