// LeoCreoAddin.cpp : efines the DLL's initialization routine.
//

#include "stdafx.h"
#include "LeoCreoAddin.h"
#include "LogFileWriter.h"
#include "LeoWebClient.h"
#include "LeoHelper.h"
#include "LeoWebServer.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#define MSGFILE L"LeoCreoAddin.txt"

// Function declarations
ProError OpenFileInCreo(const CString& filePath, const LocationInfo& locationInfo);
ProError ApplyLocationAndOrientation(ProMdl model, const LocationInfo& locationInfo);

// CLeoCreoAddinApp

BEGIN_MESSAGE_MAP(CLeoCreoAddinApp, CWinApp)
END_MESSAGE_MAP()


CLeoCreoAddinApp::CLeoCreoAddinApp()
{
	// TODO: Add construction code here.
	// Place all important initialization in InitInstance
}

// The only CLeoCreoAddinApp object

LeoHelper leoHelper;

// Global web server instance
LeoWebServer leoWebServer;

// File processing callback function for the web server
void OnFileProcessingRequest(const FileDownloadInfo& fileInfo)
{
	try {
		LogFileWriter::WriteLog("=== File Processing Request Received ===");

		CString filePathMsg;
		filePathMsg.Format(_T("File path: %s"), fileInfo.DownloadPath.GetString());
		LogFileWriter::WriteLog((const char*)CT2A(filePathMsg));
		
		// Validate file path
		if (fileInfo.DownloadPath.IsEmpty()) {
			LogFileWriter::WriteLog("ERROR: Empty file path received");
			return;
		}
		
		// Check if file exists
		if (GetFileAttributes(fileInfo.DownloadPath) == INVALID_FILE_ATTRIBUTES) {
			CString DownloadPath;
			DownloadPath.Format(_T("ERROR: File does not exist: %s"), fileInfo.DownloadPath.GetString());
			LogFileWriter::WriteLog((const char*)CT2A(DownloadPath));
			return;
		}
		
		// Log the location information
		LogFileWriter::WriteLog("Location Information:");
		CString xStr, yStr, zStr, location;
		xStr.Format(_T("%.6f"), fileInfo.LocationInfo.Loc.X);
		yStr.Format(_T("%.6f"), fileInfo.LocationInfo.Loc.Y);
		zStr.Format(_T("%.6f"), fileInfo.LocationInfo.Loc.Z);
		location.Format(_T("  X: %s, Y: %s, Z: %s"), xStr, yStr, zStr);
		
		// Log orientation matrix if available
		if (!fileInfo.LocationInfo.Orientation.empty()) {
			LogFileWriter::WriteLog("Orientation Matrix:");
			for (size_t i = 0; i < fileInfo.LocationInfo.Orientation.size(); i++) {
				CString rowMsg;
				rowMsg.Format(_T("  Row %d: [%.3f, %.3f, %.3f]"),
					(int)i,
					fileInfo.LocationInfo.Orientation[i][0],
					fileInfo.LocationInfo.Orientation[i][1],
					fileInfo.LocationInfo.Orientation[i][2]);
				LogFileWriter::WriteLog((const char*)CT2A(rowMsg));
			}
		}
		
		// Implement the logic to open the file in Creo
		ProError status = OpenFileInCreo(fileInfo.DownloadPath, fileInfo.LocationInfo);
		if (status == PRO_TK_NO_ERROR) {
			LogFileWriter::WriteLog("File opened in Creo successfully");
		} else {
			CString errorMsg;
			errorMsg.Format(_T("ERROR: Failed to open file in Creo (Error code: %d)"), status);
			LogFileWriter::WriteLog((const char*)CT2A(errorMsg));
		}
		
		LogFileWriter::WriteLog("File processing completed");
		LogFileWriter::WriteLog("=====================================");
		
	} catch (const std::exception& e) {
		LogFileWriter::WriteLog("ERROR: Exception in file processing: ");
		LogFileWriter::WriteLog((const char*)CT2A(CString(e.what())));
	} catch (...) {
		LogFileWriter::WriteLog("ERROR: Unknown exception in file processing");
	}
}


// Function to open a file in new window in Creo using Pro/ENGINEER Toolkit
ProError OpenFileInCreoNewWindow(const CString& filePath)
{
	ProError status;
	
	try {
		LogFileWriter::WriteLog("=== Opening File in Creo ===");
		
		// Validate input parameters
		if (filePath.IsEmpty()) {
			LogFileWriter::WriteLog("ERROR: Empty file path provided");
			return PRO_TK_BAD_INPUTS;
		}
		
		// Check if file exists
		if (GetFileAttributes(filePath) == INVALID_FILE_ATTRIBUTES) {
			CString errorMsg;
			errorMsg.Format(_T("ERROR: File does not exist: %s"), filePath.GetString());
			LogFileWriter::WriteLog((const char*)CT2A(errorMsg));
			return PRO_TK_E_NOT_FOUND;
		}

		// Convert CString to wchar_t* for Pro/ENGINEER Toolkit functions
		CStringW wFilePath = filePath;
		ProPath proFilePath;
		
		// Validate path length
		if (wFilePath.GetLength() >= PRO_PATH_SIZE) {
			LogFileWriter::WriteLog("ERROR: File path too long for Pro/ENGINEER Toolkit");
			return PRO_TK_BAD_INPUTS;
		}
		
		wcscpy_s(proFilePath, PRO_PATH_SIZE, wFilePath.GetString());
		
		// Log the file path being processed
		CString pathMsg;
		pathMsg.Format(_T("Processing file: %s"), filePath.GetString());
		LogFileWriter::WriteLog((const char*)CT2A(pathMsg));
		
		// Determine the file type
		ProMdlfileType fileType;
		ProMdlType modelType;
		ProMdlsubtype subType;
		
		status = ProFileSubtypeGet(proFilePath, &fileType, &modelType, &subType);
		if (status != PRO_TK_NO_ERROR) {
			CString errorMsg;
			errorMsg.Format(_T("ERROR: Failed to determine file type (Error code: %d)"), status);
			LogFileWriter::WriteLog((const char*)CT2A(errorMsg));
			
			// Provide more specific error messages
			switch (status) {
				case PRO_TK_BAD_INPUTS:
					LogFileWriter::WriteLog("  Reason: Invalid file path or file format");
					break;
				case PRO_TK_E_NOT_FOUND:
					LogFileWriter::WriteLog("  Reason: File not found");
					break;
				case PRO_TK_CANT_OPEN:
					LogFileWriter::WriteLog("  Reason: Cannot open or read file");
					break;
				default:
					LogFileWriter::WriteLog("  Reason: Unknown error");
					break;
			}
			return status;
		}
		
		// Log the determined file type
		CString typeMsg;
		typeMsg.Format(_T("File type: %d, Model type: %d, Subtype: %d"), fileType, modelType, subType);
		LogFileWriter::WriteLog((const char*)CT2A(typeMsg));
		
		// Extract the model name from the file path
		ProFamilyMdlName modelName;
		wchar_t* fileName = wcsrchr(proFilePath, L'\\');
		if (!fileName) {
			fileName = wcsrchr(proFilePath, L'/');
		}
		if (fileName) {
			fileName++; // Skip the path separator
		} else {
			fileName = proFilePath; // No path separator found, use the whole string
		}
		
		// Remove the file extension
		wcscpy_s(modelName, PRO_NAME_SIZE, fileName);
		wchar_t* extension = wcsrchr(modelName, L'.');
		if (extension) {
			*extension = L'\0';
		}
		
		// Log the extracted model name
		CString nameMsg;
		nameMsg.Format(_T("Model name: %s"), CString(modelName).GetString());
		LogFileWriter::WriteLog((const char*)CT2A(nameMsg));
		
		// Launch a new Creo session with the model file
		LogFileWriter::WriteLog("Launching new Creo session with model file...");
		
		// Prepare the command line to launch Creo with the model file
		CStringW creoPath = L"C:\\Program Files\\PTC\\Creo 11.0.2.0\\Parametric\\bin\\parametric.exe";
		CStringW commandLine;
		commandLine.Format(L"\"%s\" \"%s\"", creoPath.GetString(), filePath.GetString());
		
		// Log the command being executed
		CString cmdMsg;
		cmdMsg.Format(_T("Executing command: %s"), CString(commandLine).GetString());
		LogFileWriter::WriteLog((const char*)CT2A(cmdMsg));
		
		// Use ShellExecuteEx to launch new Creo session
		SHELLEXECUTEINFO sei = { 0 };
		sei.cbSize = sizeof(SHELLEXECUTEINFO);
		sei.fMask = SEE_MASK_NOCLOSEPROCESS;
		sei.lpVerb = L"open";
		sei.lpFile = creoPath.GetString();
		sei.lpParameters = filePath.GetString();
		sei.nShow = SW_SHOW;
		
		if (!ShellExecuteEx(&sei)) {
			DWORD error = GetLastError();
			CString errorMsg;
			errorMsg.Format(_T("ERROR: Failed to launch new Creo session (Windows error: %d)"), error);
			LogFileWriter::WriteLog((const char*)CT2A(errorMsg));
			
			// Log specific error details for process launch
			switch (error) {
				case ERROR_FILE_NOT_FOUND:
					LogFileWriter::WriteLog("  Reason: Creo executable not found at specified path");
					break;
				case ERROR_ACCESS_DENIED:
					LogFileWriter::WriteLog("  Reason: Access denied - insufficient permissions");
					break;
				case ERROR_BAD_EXE_FORMAT:
					LogFileWriter::WriteLog("  Reason: Invalid executable format");
					break;
				default:
					LogFileWriter::WriteLog("  Reason: Unknown Windows error during process launch");
					break;
			}
			return PRO_TK_GENERAL_ERROR;
		}
		
		LogFileWriter::WriteLog("New Creo session launched successfully with model file");
		
		LogFileWriter::WriteLog("File opening completed successfully");
		LogFileWriter::WriteLog("=============================");
		
		return PRO_TK_NO_ERROR;
		
	} catch (const std::exception& e) {
		LogFileWriter::WriteLog("ERROR: Exception in OpenFileInCreo: ");
		LogFileWriter::WriteLog((const char*)CT2A(CString(e.what())));
		return PRO_TK_GENERAL_ERROR;
	} catch (...) {
		LogFileWriter::WriteLog("ERROR: Unknown exception in OpenFileInCreo");
		return PRO_TK_GENERAL_ERROR;
	}
}

// Function to open a file in Creo using Pro/ENGINEER Toolkit
ProError OpenFileInCreo(const CString& filePath, const LocationInfo& locationInfo)
{
	ProError status;
	
	try {
		LogFileWriter::WriteLog("=== Opening File in Creo ===");
		
		// Validate input parameters
		if (filePath.IsEmpty()) {
			LogFileWriter::WriteLog("ERROR: Empty file path provided");
			return PRO_TK_BAD_INPUTS;
		}
		
		// Check if file exists
		if (GetFileAttributes(filePath) == INVALID_FILE_ATTRIBUTES) {
			CString errorMsg;
			errorMsg.Format(_T("ERROR: File does not exist: %s"), filePath.GetString());
			LogFileWriter::WriteLog((const char*)CT2A(errorMsg));
			return PRO_TK_E_NOT_FOUND;
		}

		// Convert CString to wchar_t* for Pro/ENGINEER Toolkit functions
		CStringW wFilePath1 = filePath;
		ProPath proFilePath;
		
		// Validate path length
		if (wFilePath1.GetLength() >= PRO_PATH_SIZE) {
			LogFileWriter::WriteLog("ERROR: File path too long for Pro/ENGINEER Toolkit");
			return PRO_TK_BAD_INPUTS;
		}
		
		wcscpy_s(proFilePath, PRO_PATH_SIZE, wFilePath1.GetString());
		
		// Log the file path being processed
		CString pathMsg;
		pathMsg.Format(_T("Processing file: %s"), filePath.GetString());
		LogFileWriter::WriteLog((const char*)CT2A(pathMsg));
		
		// Determine the file type
		ProMdlfileType fileType;
		ProMdlType modelType;
		ProMdlsubtype subType;
		
		status = ProFileSubtypeGet(proFilePath, &fileType, &modelType, &subType);
		if (status != PRO_TK_NO_ERROR) {
			CString errorMsg;
			errorMsg.Format(_T("ERROR: Failed to determine file type (Error code: %d)"), status);
			LogFileWriter::WriteLog((const char*)CT2A(errorMsg));
			
			// Provide more specific error messages
			switch (status) {
				case PRO_TK_BAD_INPUTS:
					LogFileWriter::WriteLog("  Reason: Invalid file path or file format");
					break;
				case PRO_TK_E_NOT_FOUND:
					LogFileWriter::WriteLog("  Reason: File not found");
					break;
				case PRO_TK_CANT_OPEN:
					LogFileWriter::WriteLog("  Reason: Cannot open or read file");
					break;
				default:
					LogFileWriter::WriteLog("  Reason: Unknown error");
					break;
			}
			return status;
		}
		
		// Log the determined file type
		CString typeMsg;
		typeMsg.Format(_T("File type: %d, Model type: %d, Subtype: %d"), fileType, modelType, subType);
		LogFileWriter::WriteLog((const char*)CT2A(typeMsg));
		
		// Extract the model name from the file path
		ProFamilyMdlName modelName;
		wchar_t* fileName = wcsrchr(proFilePath, L'\\');
		if (!fileName) {
			fileName = wcsrchr(proFilePath, L'/');
		}
		if (fileName) {
			fileName++; // Skip the path separator
		} else {
			fileName = proFilePath; // No path separator found, use the whole string
		}
		
		// Remove the file extension
		wcscpy_s(modelName, PRO_NAME_SIZE, fileName);
		wchar_t* extension = wcsrchr(modelName, L'.');
		if (extension) {
			*extension = L'\0';
		}
		
		// Log the extracted model name
		CString nameMsg;
		nameMsg.Format(_T("Model name: %s"), CString(modelName).GetString());
		LogFileWriter::WriteLog((const char*)CT2A(nameMsg));
		
		// Simple and reliable approach: Use working directory + ProMdlnameRetrieve
		LogFileWriter::WriteLog("Opening file in CURRENT SESSION...");
		
		// Declare model variable
		ProMdl model = NULL;
		
		// Extract directory path from file path
		CStringW wFilePath = filePath;
		int lastSlashPos = wFilePath.ReverseFind(L'\\');
		if (lastSlashPos == -1) {
			lastSlashPos = wFilePath.ReverseFind(L'/');
		}
		

		if (lastSlashPos > 0) {
			// Get directory path
			ProPath dirPath;
			CStringW dirPathStr = wFilePath.Left(lastSlashPos);
			wcscpy_s(dirPath, PRO_PATH_SIZE, dirPathStr.GetString());
			
			// Change to file's directory
			LogFileWriter::WriteLog("Changing to file directory...");
			status = ProDirectoryChange(dirPath);
			if (status == PRO_TK_NO_ERROR) {
				LogFileWriter::WriteLog("Directory changed successfully");
				
				// Load model by name (most reliable method)
				LogFileWriter::WriteLog("Loading model by name...");
				
				// First, retrieve the model handle using ProMdlnameRetrieve
				status = ProMdlnameRetrieve(modelName, fileType, &model);
				if (status == PRO_TK_NO_ERROR) {
					LogFileWriter::WriteLog("SUCCESS: ProMdlnameRetrieve - Model handle retrieved");
					
					// Get the current assembly
					ProMdl currentAssembly;
					status = ProMdlCurrentGet(&currentAssembly);
					if (status == PRO_TK_NO_ERROR) {
						ProMdlType currentModelType;
						status = ProMdlTypeGet(currentAssembly, &currentModelType);
						if (status == PRO_TK_NO_ERROR && currentModelType == PRO_MDL_ASSEMBLY) {
							LogFileWriter::WriteLog("SUCCESS: Current assembly found - Adding component to assembly");
							
							// Create transformation matrix from locationInfo data
							ProMatrix initPos;
							
							// Use the actual locationInfo data from the JSON
							double locX = locationInfo.Loc.X;
							double locY = locationInfo.Loc.Y;
							double locZ = locationInfo.Loc.Z;
							
							// Use the actual orientation matrix from the JSON
							double orient[3][3];
							if (locationInfo.Orientation.size() >= 3 && 
								locationInfo.Orientation[0].size() >= 3 &&
								locationInfo.Orientation[1].size() >= 3 &&
								locationInfo.Orientation[2].size() >= 3) {
								
								// Use the provided orientation matrix
								for (int i = 0; i < 3; i++) {
									for (int j = 0; j < 3; j++) {
										orient[i][j] = locationInfo.Orientation[i][j];
									}
								}
								LogFileWriter::WriteLog("SUCCESS: Using provided orientation matrix from JSON");
							} else {
								// Fallback to identity matrix if orientation data is invalid
								orient[0][0] = 1.0; orient[0][1] = 0.0; orient[0][2] = 0.0;
								orient[1][0] = 0.0; orient[1][1] = 1.0; orient[1][2] = 0.0;
								orient[2][0] = 0.0; orient[2][1] = 0.0; orient[2][2] = 1.0;
								LogFileWriter::WriteLog("WARNING: Invalid orientation data, using identity matrix");
							}
							
							// Build 4x4 transformation matrix
							// Set rotation part (3x3)
							for (int i = 0; i < 3; i++) {
								for (int j = 0; j < 3; j++) {
									initPos[i][j] = orient[i][j];
								}
							}
							
							// Set translation part
							initPos[0][3] = locX;
							initPos[1][3] = locY;
							initPos[2][3] = locZ;
							
							// Set homogeneous coordinate
							initPos[3][0] = 0.0;
							initPos[3][1] = 0.0;
							initPos[3][2] = 0.0;
							initPos[3][3] = 1.0;
							
							// Log the transformation matrix values
							char logMsg[512];
							sprintf_s(logMsg, "SUCCESS: Transformation matrix created - Location: [%.3f, %.3f, %.3f], Orientation: [%.3f,%.3f,%.3f; %.3f,%.3f,%.3f; %.3f,%.3f,%.3f]", 
								locX, locY, locZ,
								orient[0][0], orient[0][1], orient[0][2],
								orient[1][0], orient[1][1], orient[1][2],
								orient[2][0], orient[2][1], orient[2][2]);
							LogFileWriter::WriteLog(logMsg);
							
							// Add the component to the current assembly
							ProAsmcomp newComponent;
							status = ProAsmcompAssemble((ProAssembly)currentAssembly, (ProSolid)model, initPos, &newComponent);
							if (status == PRO_TK_NO_ERROR) {
								LogFileWriter::WriteLog("SUCCESS: ProAsmcompAssemble - Component added to assembly");
								
								// Regenerate the assembly to update the display
								status = ProAsmcompRegenerate(&newComponent, PRO_B_FALSE);
								if (status == PRO_TK_NO_ERROR) {
									LogFileWriter::WriteLog("SUCCESS: ProAsmcompRegenerate - Assembly regenerated");
								} else {
									LogFileWriter::WriteLog("WARNING: ProAsmcompRegenerate failed, but component is added");
								}
								
								// Refresh the model tree to show the new component
								status = ProWindowRepaint(PRO_VALUE_UNUSED);
								if (status == PRO_TK_NO_ERROR) {
									LogFileWriter::WriteLog("SUCCESS: ProWindowRepaint - Model tree refreshed");
								} else {
									LogFileWriter::WriteLog("WARNING: ProWindowRepaint failed, but component is added");
								}
								
								// Force a tree refresh by getting the current window and repainting
								int currentWindowId;
								status = ProWindowCurrentGet(&currentWindowId);
								if (status == PRO_TK_NO_ERROR) {
									status = ProWindowRepaint(currentWindowId);
									if (status == PRO_TK_NO_ERROR) {
										LogFileWriter::WriteLog("SUCCESS: ProWindowRepaint(currentWindow) - Model tree refreshed");
									}
								}
								
								// Refresh the model tree specifically
								status = ProTreetoolRefresh(currentAssembly);
								if (status == PRO_TK_NO_ERROR) {
									LogFileWriter::WriteLog("SUCCESS: ProTreetoolRefresh - Model tree refreshed");
								} else {
									LogFileWriter::WriteLog("WARNING: ProTreetoolRefresh failed, but component is added");
								}
								
								// Force assembly display refresh
								status = ProMdlDisplay(currentAssembly);
								if (status == PRO_TK_NO_ERROR) {
									LogFileWriter::WriteLog("SUCCESS: ProMdlDisplay - Assembly display refreshed");
								} else {
									LogFileWriter::WriteLog("WARNING: ProMdlDisplay failed, but component is added");
								}
								
								// Force window activation to bring to front
								if (currentWindowId != PRO_VALUE_UNUSED) {
									status = ProWindowActivate(currentWindowId);
									if (status == PRO_TK_NO_ERROR) {
										LogFileWriter::WriteLog("SUCCESS: ProWindowActivate - Window brought to front");
									}
								}
								
								// Additional window refresh for visibility
								status = ProWindowRepaint(currentWindowId);
								if (status == PRO_TK_NO_ERROR) {
									LogFileWriter::WriteLog("SUCCESS: ProWindowRepaint - Additional window refresh");
								}
								
								// Final comprehensive refresh
								status = ProWindowRepaint(PRO_VALUE_UNUSED);
								if (status == PRO_TK_NO_ERROR) {
									LogFileWriter::WriteLog("SUCCESS: Final ProWindowRepaint - Complete refresh done");
								}
								
								LogFileWriter::WriteLog("SUCCESS: Component added with enhanced refresh - Should be immediately visible");
							} else {
								LogFileWriter::WriteLog("ERROR: ProAsmcompAssemble failed - Could not add component to assembly");
								return status;
							}
						} else {
							LogFileWriter::WriteLog("WARNING: Current model is not an assembly - Checking model type for appropriate action");
							
							// Check if current model is a part
							if (currentModelType == PRO_MDL_PART) {
								LogFileWriter::WriteLog("INFO: Current model is a part - Creating new session for imported part");
								status = OpenFileInCreoNewWindow(filePath);
								if (status == PRO_TK_NO_ERROR) {
									LogFileWriter::WriteLog("SUCCESS: OpenFileInCreoNewWindow - New session created for imported part");
								} else {
									LogFileWriter::WriteLog("ERROR: Failed to create new session for imported part");
									return status;
								}
							} else {
								LogFileWriter::WriteLog("WARNING: Current model is not a part or assembly - Using fallback display method");
								
								// Fallback: Display in separate window
								int winid;
								ProType mdl_type = PRO_PART;
								status = ProObjectwindowMdlnameCreate(modelName, mdl_type, &winid);
								if (status == PRO_TK_NO_ERROR) {
									LogFileWriter::WriteLog("SUCCESS: ProObjectwindowMdlnameCreate - Model displayed in separate window");
									
									// Note: No locationInfo applied for fallback - only for assembly components
									LogFileWriter::WriteLog("INFO: Fallback part opened - LocationInfo not applicable for standalone parts");
								} else {
									LogFileWriter::WriteLog("ERROR: Failed to display model in separate window");
									return status;
								}
							}
						}
					} else {
						LogFileWriter::WriteLog("WARNING: No current model found - Using fallback display method");
						
						// Fallback: Display in separate window
						int winid;
						ProType mdl_type = PRO_PART;
						status = ProObjectwindowMdlnameCreate(modelName, mdl_type, &winid);
						if (status == PRO_TK_NO_ERROR) {
							LogFileWriter::WriteLog("SUCCESS: ProObjectwindowMdlnameCreate - Model displayed in separate window");
						} else {
							LogFileWriter::WriteLog("ERROR: Failed to display model in separate window");
							return status;
						}
					}
				} else {
					LogFileWriter::WriteLog("ERROR: Failed to retrieve model by name");
					return status;
				}
			} else {
				LogFileWriter::WriteLog("ERROR: Failed to change directory");
				return status;
			}
		} else {
			LogFileWriter::WriteLog("ERROR: Could not extract directory from file path");
			return PRO_TK_BAD_INPUTS;
		}
		
		
		// File is opened and displayed in CURRENT SESSION - that's it!
		LogFileWriter::WriteLog("SUCCESS: File opened and displayed in current session");
		
		LogFileWriter::WriteLog("File opening completed successfully");
		LogFileWriter::WriteLog("=============================");
		
		return PRO_TK_NO_ERROR;
		
	} catch (const std::exception& e) {
		LogFileWriter::WriteLog("ERROR: Exception in OpenFileInCreo: ");
		LogFileWriter::WriteLog((const char*)CT2A(CString(e.what())));
		return PRO_TK_GENERAL_ERROR;
	} catch (...) {
		LogFileWriter::WriteLog("ERROR: Unknown exception in OpenFileInCreo");
		return PRO_TK_GENERAL_ERROR;
	}
}


// Function to apply location and orientation to a model
ProError ApplyLocationAndOrientation(ProMdl model, const LocationInfo& locationInfo)
{
	try {
		LogFileWriter::WriteLog("=== Applying Location and Orientation ===");
		
		// Validate input parameters
		if (model == NULL) {
			LogFileWriter::WriteLog("ERROR: Invalid model handle provided");
			return PRO_TK_BAD_INPUTS;
		}
		
		// Log the location information
		CString locationMsg;
		locationMsg.Format(_T("Location: X=%.6f, Y=%.6f, Z=%.6f"), 
			locationInfo.Loc.X, locationInfo.Loc.Y, locationInfo.Loc.Z);
		LogFileWriter::WriteLog((const char*)CT2A(locationMsg));
		
		// Validate and log the orientation matrix
		LogFileWriter::WriteLog("Orientation Matrix:");
		bool validMatrix = true;
		if (locationInfo.Orientation.size() < 3) {
			LogFileWriter::WriteLog("WARNING: Orientation matrix has less than 3 rows");
			validMatrix = false;
		}
		
		for (size_t i = 0; i < locationInfo.Orientation.size() && i < 3; i++) {
			if (locationInfo.Orientation[i].size() < 3) {
				CString warningMsg;
				warningMsg.Format(_T("WARNING: Orientation matrix row %d has less than 3 columns"), (int)i);
				LogFileWriter::WriteLog((const char*)CT2A(warningMsg));
				validMatrix = false;
			} else {
				CString matrixMsg;
				matrixMsg.Format(_T("  [%.6f, %.6f, %.6f]"),
					locationInfo.Orientation[i][0],
					locationInfo.Orientation[i][1],
					locationInfo.Orientation[i][2]);
				LogFileWriter::WriteLog((const char*)CT2A(matrixMsg));
			}
		}
		
		if (!validMatrix) {
			LogFileWriter::WriteLog("WARNING: Orientation matrix is not valid 3x3 matrix");
		}
		
		// Check if this is an assembly model and we need to add it as a component
		ProMdlType modelType;
		ProError status = ProMdlTypeGet(model, &modelType);
		if (status != PRO_TK_NO_ERROR) {
			LogFileWriter::WriteLog("ERROR: Failed to get model type");
			return status;
		}
		
		// Get the current model (if it's an assembly, we'll add this as a component)
		ProMdl currentModel;
		status = ProMdlCurrentGet(&currentModel);
		if (status == PRO_TK_NO_ERROR) {
			ProMdlType currentModelType;
			status = ProMdlTypeGet(currentModel, &currentModelType);
			if (status == PRO_TK_NO_ERROR && currentModelType == PRO_MDL_ASSEMBLY) {
				LogFileWriter::WriteLog("Current model is an assembly - position could be applied as component placement");
				// Note: Component placement with specific location/orientation would require
				// more complex Pro/ENGINEER Toolkit implementation using constraint-based assembly
			}
		}
		
		// For now, just log that location/orientation information is available
		// In a full implementation, you would:
		// 1. Check if current model is an assembly
		// 2. Add the opened model as a component to the assembly using ProAsmcompMdlnameCreateCopy
		// 3. Apply the location and orientation as component placement constraints using Pro/ENGINEER placement constraints
		// 4. Use ProAsmcompConstraint functions to create precise positioning constraints
		
		LogFileWriter::WriteLog("Location and orientation information processed successfully");
		
		// Log implementation notes for future development
		LogFileWriter::WriteLog("IMPLEMENTATION NOTES:");
		LogFileWriter::WriteLog("- File opened and displayed in Creo successfully");
		LogFileWriter::WriteLog("- Location and orientation data captured and validated");
		LogFileWriter::WriteLog("- For component placement in assemblies, additional implementation needed:");
		LogFileWriter::WriteLog("  * Use ProAsmcompMdlnameCreateCopy to add as assembly component");
		LogFileWriter::WriteLog("  * Use constraint-based placement system for precise positioning");
		LogFileWriter::WriteLog("  * Apply transformation matrix from orientation data");
		LogFileWriter::WriteLog("=====================================");
		
		return PRO_TK_NO_ERROR;
		
	} catch (const std::exception& e) {
		LogFileWriter::WriteLog("ERROR: Exception in ApplyLocationAndOrientation: ");
		LogFileWriter::WriteLog((const char*)CT2A(CString(e.what())));
		return PRO_TK_GENERAL_ERROR;
	} catch (...) {
		LogFileWriter::WriteLog("ERROR: Unknown exception in ApplyLocationAndOrientation");
		return PRO_TK_GENERAL_ERROR;
	}
}

// Function to check web server status
void CheckWebServerStatus()
{
	if (leoWebServer.IsRunning()) {
		LogFileWriter::WriteLog("Leo Web Server is running and healthy");
		LogFileWriter::WriteLog("Port: 4100");
		LogFileWriter::WriteLog("Status: Active and listening for requests");
	} else {
		LogFileWriter::WriteLog("Leo Web Server is not running");
		LogFileWriter::WriteLog("Attempting to restart web server...");
		
		// Try to restart the web server
		if (leoWebServer.StartServer(4100)) {
			LogFileWriter::WriteLog("Web server restarted successfully");
		} else {
			LogFileWriter::WriteLog("Failed to restart web server");
			LogFileWriter::WriteLog((const char*)CT2A(leoWebServer.GetLastError()));
		}
	}
}

// CLeoCreoAddinApp InitInstance

BOOL CLeoCreoAddinApp::InitInstance()
{
	CWinApp::InitInstance();

	return TRUE;
}

// CLeoCreoAddinApp ExitInstance
int CLeoCreoAddinApp::ExitInstance()
{

	return CWinApp::ExitInstance();
}

static uiCmdAccessState AccessDefault(uiCmdAccessMode access_mode)
{
	return ACCESS_AVAILABLE;
}

ProError ShowDialog(wchar_t* Message)
{
	ProUIMessageButton* buttons;
	ProUIMessageButton user_choice;
	ProArrayAlloc(1, sizeof(ProUIMessageButton), 1, (ProArray*)&buttons);
	buttons[0] = PRO_UI_MESSAGE_OK;
	ProUIMessageDialogDisplay(PROUIMESSAGE_INFO, L"Information", Message, buttons, PRO_UI_MESSAGE_OK, &user_choice);
	ProArrayFree((ProArray*)&buttons);
	return PRO_TK_NO_ERROR;
}

void MainMenuAct()
{
	ShowDialog(L"Clicked the main menu item. Right-clicking a selected feature in a drawing or model will also add a right-click menu item, with the same functionality as clicking the main menu. In this example, only this dialog box is popped up.");
}


void OpenLeoAppAct()
{
	LogFileWriter::WriteLog("Open Leo APP\n");
	
	// Try to start Leo Desktop App
	SHELLEXECUTEINFO sei = { 0 };
	sei.cbSize = sizeof(SHELLEXECUTEINFO);
	sei.lpVerb = L"open";
	sei.lpFile = LEO_DESKTOP_EXE; // Adjust path as needed
	sei.nShow = SW_SHOW;

	if (!ShellExecuteEx(&sei))
	{
		LogFileWriter::WriteLog("Failed when you open the Leo app\n");
		return;
	}
	LogFileWriter::WriteLog("Leo desktop app launched successfully\n");
/*
	LeoWebClient *pWebclient = new LeoWebClient();
	int res = pWebclient->LaunchLeoDesktopApp(L"LeoCreoAddin launched from Creo");
	if (res == -1)
	{
		ShowDialog(L"Leo app is already running.");
		return;
	}
	else if (res == 0)
	{
		ShowDialog(L"Failed to launch Leo app.");
		return;
	}
	else
	{
		ShowDialog(L"Leo desktop app launched successfully");
		return;
	}
*/
}

void ProcessSelectedFace(int modelType, MeasurementData measureData)
{
	// Convert int to string before passing to WriteLog

	LeoWebClient* webClient = new LeoWebClient();

	// Optional: Configure custom settings
	//webClient->SetPort(4000);
	//webClient->SetHost(_T("localhost"));
	//webClient->SetTimeout(5000);

	CString statusMsg;

	//if (webClient->IsLeoAppRunning()) {
	//	// Leo app is already running
	//	statusMsg.Format(_T("Leo app is running"));
	//	LogFileWriter::WriteLog((const char*)CT2A(statusMsg));
	//}
	//else {
	//	// Launch Leo app if not running
	//	if (webClient->LaunchLeoDesktopApp(_T("Starting Leo for part search..."))) {
	//		statusMsg.Format(_T("Leo app launched successfully"));
	//		LogFileWriter::WriteLog((const char*)CT2A(statusMsg));
	//	}
	//	else {
	//		statusMsg.Format(_T("Failed to launch Leo app: %s"), webClient->GetLastError());
	//		LogFileWriter::WriteLog((const char*)CT2A(statusMsg));
	//	}
	//}

	webClient->SendFaceMeasurementData(measureData,
		[](const HttpResponse& response) {
			AFX_MANAGE_STATE(AfxGetStaticModuleState());
			CString responseMsg;
			responseMsg.Format(_T("Measurement data sent successfully"));
			LogFileWriter::WriteLog((const char*)CT2A(responseMsg));

			// Convert response.Body (std::string) to CString
			CString responseBody(CW2T(response.Body));
			responseMsg.Format(_T("Response: %s"), responseBody.GetString());
			LogFileWriter::WriteLog((const char*)CT2A(responseMsg));
		},
		[](const CString& error) {
			AFX_MANAGE_STATE(AfxGetStaticModuleState());
			CString errorMsg;
			errorMsg.Format(_T("Error sending measurement data: %s"), error.GetString());
			LogFileWriter::WriteLog((const char*)CT2A(errorMsg));
		}
	);
}


void FindComponentAct()
{
	MeasurementData measureData;

	LogFileWriter::WriteLog((const char*)CT2A(_T("Start!!")));
	
	int modelType = leoHelper.IsFaceSelected(&measureData);
	if (modelType == PRO_PART || modelType == PRO_SURFACE || modelType == PRO_ASSEMBLY) {
		ProcessSelectedFace(modelType, measureData);
	}else
	{
		LogFileWriter::WriteLog("User needs to select a face but No faces selected");
		ShowDialog(L"No faces selected");
	}

	// Here you would implement the logic to find a component in the assembly.
	// This is a placeholder for demonstration purposes.
	// You can use ProAssembly functions to find components based on user input or selection.
}


//static ProMenuItemName check_group_items[] = {"CheckButtonMenu"};
//static ProMenuItemLabel check_group_labels[] = {"CheckButtonMenuItem"};
//static ProMenuLineHelp check_group_help[] = {"CheckButtonMenuItemtips"};
//static ProCmdItemIcon check_group_icons[]={"Icon.png"};

typedef struct procheckbuttonstruct
{
	uiCmdCmdId command;
	ProBoolean state;
} ProCheckButton;

static ProCheckButton _checkbutton[1];

int CheckButtonActfn(uiCmdCmdId command, uiCmdValue* p_value, void* p_push_command_data)
{
	//Should loop for multiple items. Here, we test only one menu item, so it is simplified.
	if (command == _checkbutton[0].command)
	{
		if (_checkbutton[0].state == PRO_B_FALSE)
		{
			_checkbutton[0].state = PRO_B_TRUE;
			ShowDialog(L"CheckButton has been pressed. This example only pops up this dialog box.");
		}
		else
		{
			_checkbutton[0].state = PRO_B_FALSE;
			ShowDialog(L"CheckButton has been released. This example only pops up this dialog box.");
		}
	}
	return 0;
}

int CheckButtonValFn(uiCmdCmdId command, uiCmdValue* p_value)
{
	ProError status;
	ProBoolean value;

	//Should do a loop check for multiple items. Here, there is only one menu item to test, so it is simplified.
	if (_checkbutton[0].command == command)
	{
		status = ProMenubarmenuChkbuttonValueGet(p_value, &value);
		if (value == _checkbutton[0].state)
			return 0;

		status = ProMenubarmenuChkbuttonValueSet(p_value, _checkbutton[0].state);
		return status;
	}

	return 0;
}

static ProMenuItemName radio_group_items[] = { "RadioButtonMenu1", "RadioButtonMenu2","RadioButtonMenu3", "RadioButtonMenu4" };
static ProMenuItemLabel radio_group_labels[] = { "RadioButtonMenuItem1", "RadioButtonMenuItem2","RadioButtonMenuItem3","RadioButtonMenuItem4" };
static ProMenuLineHelp radio_group_help[] = { "RadioButtonMenuItem1tips", "RadioButtonMenuItem2tips","RadioButtonMenuItem3tips","RadioButtonMenuItem4tips" };
static ProCmdItemIcon radio_group_icons[] = { "Icon.png", "Icon.png","Icon.png", "Icon.png" };

int RadioButtonValFn(uiCmdCmdId command, uiCmdValue* p_value)
{
	ProError status;
	ProMenuItemName name;

	status = ProMenubarMenuRadiogrpValueGet(p_value, name);
	status = ProMenubarMenuRadiogrpValueSet(p_value, name);

	return 0;
}


static uiCmdAccessState AccessPopupmenu(uiCmdCmdId command, ProAppData appdata, ProSelection* sel_buffer)
{
	// Whether the right-click menu appears should be determined based on the selected object. It appears by default here.
	return ACCESS_AVAILABLE;
}

ProError ProPopupMenuNotification(ProMenuName name)
{
	ProError status;
	uiCmdCmdId FindComponentMenuID;
	ProPopupMenuId PopupMenuID;
	ProLine label;
	ProLine help;

	status = ProPopupmenuIdGet(name, &PopupMenuID);
	status = ProCmdCmdIdFind("FindComponent_Act", &FindComponentMenuID);
	//ProMessageDisplay(MSGFILE, "FindComponentMenuItem");
	//ProMessageDisplay(MSGFILE, "FindComponentMenuItemtips");
	status = ProPopupmenuButtonAdd(PopupMenuID, PRO_VALUE_UNUSED, "FindComponent_Act", L"Find Component", L"Find Component in Assembly", FindComponentMenuID, AccessPopupmenu, NULL);

	return PRO_TK_NO_ERROR;
}

// Function to create a custom toolbar group for the Find Component button
ProError CreateFindComponentToolbar()
{
    ProError status;
    uiCmdCmdId FindComponentMenuID;
    
    // Get the Find Component command ID
    status = ProCmdCmdIdFind("FindComponent_Act", &FindComponentMenuID);
    if (status != PRO_TK_NO_ERROR) {
        LogFileWriter::WriteLog("ERROR: Failed to find FindComponent_Act command");
        return status;
    }
    
    // Create a custom toolbar group
    // Note: This approach uses the modern Creo Parametric toolbar system
    // The button will appear in the toolbar area and can be customized by the user
    
    // Set the command to be always available for toolbar placement
    status = ProCmdAlwaysAllowValueUpdate(FindComponentMenuID, PRO_B_TRUE);
    if (status != PRO_TK_NO_ERROR) {
        LogFileWriter::WriteLog("WARNING: Failed to set command to always allow value update");
    }
    
    // Log detailed instructions for users
    LogFileWriter::WriteLog("=== Find Component Toolbar Button Setup ===");
    LogFileWriter::WriteLog("The Find Component button is now available for toolbar placement");
    LogFileWriter::WriteLog("To add it to the toolbar area (highlighted in red in your image):");
    LogFileWriter::WriteLog("1. Right-click on any toolbar in Creo Parametric");
    LogFileWriter::WriteLog("2. Select 'Customize' from the context menu");
    LogFileWriter::WriteLog("3. Go to the 'Commands' tab");
    LogFileWriter::WriteLog("4. In the 'Categories' list, find 'LeoCreoAddin'");
    LogFileWriter::WriteLog("5. In the 'Commands' list, find 'Find Component'");
    LogFileWriter::WriteLog("6. Drag and drop 'Find Component' to your desired toolbar location");
    LogFileWriter::WriteLog("7. Click 'OK' to save the toolbar configuration");
    LogFileWriter::WriteLog("8. The button will now appear as an icon-only button on your toolbar");
    LogFileWriter::WriteLog("==========================================");
    
    return PRO_TK_NO_ERROR;
}

extern "C" int user_initialize()
{
	ProError status;
	uiCmdCmdId LeoOpenMenuID;
	uiCmdCmdId FindComponentMenuID;

	status = ProMenubarMenuAdd("LeoCreoAddin", "LeoCreoAddin", "Help", PRO_B_TRUE, MSGFILE);

	// Open Leo App
	status = ProCmdActionAdd("OpenLeoApp_Act", (uiCmdCmdActFn)OpenLeoAppAct, uiProeImmediate, AccessDefault, PRO_B_TRUE, PRO_B_TRUE, &LeoOpenMenuID);
	status = ProMenubarmenuPushbuttonAdd("LeoCreoAddin", "OpenLeoMenuItem", "OpenLeoMenuItem", "OpenLeoMenuItemtips", NULL, PRO_B_TRUE, LeoOpenMenuID, MSGFILE);
	status = ProCmdIconSet(LeoOpenMenuID, "LeoApp.png");
	status = ProCmdDesignate(LeoOpenMenuID, "OpenLeoMenuItem", "OpenLeoMenuItem", "OpenLeoMenuItemtips", MSGFILE);


	// Find Component
	status = ProCmdActionAdd("FindComponent_Act", (uiCmdCmdActFn)FindComponentAct, uiProeImmediate, AccessDefault, PRO_B_TRUE, PRO_B_TRUE, &FindComponentMenuID);
	status = ProMenubarmenuPushbuttonAdd("LeoCreoAddin", "FindComponentMenuItem", "FindComponentMenuItem", "FindComponentMenuItemtips", NULL, PRO_B_TRUE, FindComponentMenuID, MSGFILE);
	status = ProCmdIconSet(FindComponentMenuID, "findComponent.png");
	status = ProCmdDesignate(FindComponentMenuID, "FindComponentMenuItem", "FindComponentMenuItem", "FindComponentMenuItemtips", MSGFILE);

	// Create the Find Component toolbar button
	status = CreateFindComponentToolbar();
	if (status != PRO_TK_NO_ERROR) {
		LogFileWriter::WriteLog("WARNING: Failed to create Find Component toolbar button");
	}

	//Register right-click menu listener event, function is the same as normal menu
	status = ProNotificationSet(PRO_POPUPMENU_CREATE_POST, (ProFunction)ProPopupMenuNotification);

	// Load custom ribbon bar
	//status = ProRibbonDefinitionfileLoad(L"LeoCreoAddin.rbn");

	// Start the web server for receiving part opening requests
	LogFileWriter::WriteLog("=== Starting Leo Web Server ===");
	LogFileWriter::WriteLog("Port: 4100");
	LogFileWriter::WriteLog("Purpose: Receive part opening requests from external applications");
	
	// Set the file processing callback
	leoWebServer.SetFileProcessingCallback(OnFileProcessingRequest);
	LogFileWriter::WriteLog("File processing callback registered");
	
	if (leoWebServer.StartServer(4100)) {
		LogFileWriter::WriteLog("Leo Web Server started successfully on port 4100");
		LogFileWriter::WriteLog("Web server is now listening for part opening requests");
		LogFileWriter::WriteLog("Available endpoints:");
		LogFileWriter::WriteLog("  - POST / : Part opening requests (JSON format)");
		LogFileWriter::WriteLog("  - GET /health : Health check endpoint");
		LogFileWriter::WriteLog("Performance optimizations:");
		LogFileWriter::WriteLog("  - Zero-latency connection acceptance");
		LogFileWriter::WriteLog("  - Minimal 0.1ms polling interval");
		LogFileWriter::WriteLog("  - Immediate request processing");
		LogFileWriter::WriteLog("Server status: ACTIVE");
		LogFileWriter::WriteLog("================================");
	} else {
		LogFileWriter::WriteLog("ERROR: Failed to start Leo Web Server");
		CString LastError;
		LastError.Format(_T("Error details: %s"), (const char*)CT2A(leoWebServer.GetLastError()));
		LogFileWriter::WriteLog((const char*)CT2A(LastError));
		LogFileWriter::WriteLog("Server status: FAILED");
		LogFileWriter::WriteLog("================================");
	}

	return PRO_TK_NO_ERROR;
}

extern "C" void user_terminate()
{
	ProError status;
	status = ProNotificationUnset(PRO_POPUPMENU_CREATE_POST);
	
	// Stop the web server
	LogFileWriter::WriteLog("=== Stopping Leo Web Server ===");
	leoWebServer.StopServer();
	LogFileWriter::WriteLog("Leo Web Server stopped successfully");
	LogFileWriter::WriteLog("Port 4100 released");
	LogFileWriter::WriteLog("================================");
}