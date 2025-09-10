#pragma once

// Leo AI Configuration
#define LEO_DESKTOP_PORT 3000
#define LEO_DESKTOP_HOST L"localhost:4000"
#define LEO_SEARCH_ENDPOINT L"/receive-data"
#define LEO_OPEN_PART_ENDPOINT L"/api/open-part"
#define LEO_FACE_SEARCH_ENDPOINT L"/api/face-search"

// HTTP Request Headers
#define HTTP_CONTENT_TYPE L"Content-Type: application/json"
#define HTTP_USER_AGENT L"LeoCreoAddin/1.0"

// Timeout settings
#define HTTP_TIMEOUT_MS 5000
#define HTTP_RETRY_COUNT 3

// Assembly data settings
#define MAX_ASSEMBLY_COMPONENTS 1000
#define MAX_FACE_MEASUREMENTS 100

// File paths
#define LEO_DESKTOP_EXE L"C:\\Program Files\\Leo\\Leo.exe"
#define LEO_CONFIG_FILE L"leo_config.json"

// Error messages
#define ERROR_LEO_APP_NOT_RUNNING L"Leo Desktop App is not running. Please start it first."
#define ERROR_HTTP_REQUEST_FAILED L"HTTP request failed. Check network connection."
#define ERROR_NO_ASSEMBLY_ACTIVE L"No active assembly found. Please open an assembly first."
#define ERROR_FACE_SELECTION_FAILED L"Face selection failed. Please select a valid face." 