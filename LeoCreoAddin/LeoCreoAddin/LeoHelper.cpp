#include "stdafx.h"
#include "LeoHelper.h"
#include "LogFileWriter.h"

LeoHelper::LeoHelper()
{
}

LeoHelper::~LeoHelper()
{
}

void LeoHelper::GetFaceDataAction() {

}

int LeoHelper::GetCurrentFaceSelection(MeasurementData *measureData)
{
	ProError err;
	ProMdl currMdl;

	err = ProMdlCurrentGet(&currMdl);
	if (err != PRO_TK_NO_ERROR) return err;

	ProMdlType mDltype = PRO_MDL_UNUSED;
	err = ProMdlTypeGet(currMdl, &mDltype);

	// Get current selections from selection buffer without prompting user
	ProSelection *sels;
	err = ProSelbufferSelectionsGet(&sels);
	
	if (err != PRO_TK_NO_ERROR) {
		LogFileWriter::WriteLog("Failed to get current selections from buffer");
		return err;
	}

	// Count the selections
	int nSels = 0;
	ProArraySizeGet((ProArray)sels, &nSels);
	
	CString nSelsMsg;
	nSelsMsg.Format(_T("Found %d objects in current selection buffer"), nSels);
	LogFileWriter::WriteLog((const char*)CT2A(nSelsMsg));
		
	if (nSels <= 0 || sels == NULL) {
		LogFileWriter::WriteLog("No objects currently selected");
		ProSelectionarrayFree(sels);
		return PRO_TK_E_NOT_FOUND;
	}

	// Find the first face selection
	ProSelection* sel = NULL;
	for (int i = 0; i < nSels; i++) {
		ProGeomitem selectedItem;
		err = ProSelectionModelitemGet(sels[i], &selectedItem);
		if (err == PRO_TK_NO_ERROR) {
			// Check if this is a face-type item
			CString nSelsType;
			nSelsType.Format(_T("Selected Item type is %d"), selectedItem.type);
			LogFileWriter::WriteLog((const char*)CT2A(nSelsType));
			if (selectedItem.type == PRO_SURFACE || selectedItem.type == PRO_QUILT) {
				sel = &sels[i];
				break;
			}
		}
	}

	if (sel == NULL) {
		LogFileWriter::WriteLog("No face-type objects found in current selections");
		ProSelectionarrayFree(sels);
		return PRO_TK_E_NOT_FOUND;
	}

	// Process the selected face (reuse existing logic)
	ProGeomitem selectedItem;
	err = ProSelectionModelitemGet(*sel, &selectedItem);
	if (err != PRO_TK_NO_ERROR) {
		ProSelectionarrayFree(sels);
		return err;
	}

	ProUvParam paramAtselection;
	err = ProSelectionUvParamGet(*sel, paramAtselection);
	if (err != PRO_TK_NO_ERROR) {
		ProSelectionarrayFree(sels);
		return err;
	}

	ProSurface selectedSurf;
	err = ProGeomitemToSurface(&selectedItem, &selectedSurf);
	if (err != PRO_TK_NO_ERROR) {
		ProSelectionarrayFree(sels);
		return err;
	}

	ProGeomitemdata* geomData;
	ProSurfacedata* surfaceData;
	err = ProSurfaceDataGet(selectedSurf, &geomData);
	if (err != PRO_TK_NO_ERROR) {
		ProSelectionarrayFree(sels);
		return err;
	}

	ProSrftype surfType = PRO_SRF_NONE;
	ProUvParam uvMin, uvMax, Origin;
	ProSurfaceOrient surfOrient;
	ProSurfaceshapedata surfShape;
	int surfId = -1;

	HoleInfo holeInfo;

	err = ProSurfacedataGet(geomData->data.p_surface_data, &surfType, uvMin, uvMax, &surfOrient, &surfShape, &surfId);
	if (err != PRO_TK_NO_ERROR) {
		ProSelectionarrayFree(sels);
		return err;
	}

	// Check if surface type supports area calculation
	double area = 0.0;
	err = ProSurfaceAreaEval(selectedSurf, &area);

	// Final area log with enhanced information
	CString areaMsg;
	areaMsg.Format(_T("%lf"), area);
	LogFileWriter::WriteLog((const char*)CT2A(_T("Final Surface Area Result: " + areaMsg)));

	measureData->Area = areaMsg;

	// Test section: Determine if the selected surface is a hole
	bool isHole = false;
	CString holeDetectionLog;
	
	LogFileWriter::WriteLog("Starting hole detection analysis...");
	
	// Method 1: Check if surface is cylindrical and oriented inward
	if (surfType == PRO_SRF_CYL) {
		LogFileWriter::WriteLog("Surface is cylindrical - checking orientation for hole detection");
		
		// Check surface orientation - holes typically have inward-facing normals
		if (surfOrient == PRO_SURF_ORIENT_IN) {
			isHole = true;

			holeDetectionLog = _T("Detected as HOLE: Cylindrical surface with inward orientation");
			LogFileWriter::WriteLog((const char*)CT2A(holeDetectionLog));
		} else {
			holeDetectionLog = _T("Cylindrical surface but outward orientation - likely external cylinder");
			LogFileWriter::WriteLog((const char*)CT2A(holeDetectionLog));
		}
	}
	
	// Method 2: Get surface geometry information (click location, normal vector, center point)
	ProVector xyz_point, normal;
	ProVector deriv1[2], deriv2[3];
	
	// Evaluate surface normal at the selection point (click location)
	ProError normalErr = ProSurfaceXyzdataEval(selectedSurf, paramAtselection, xyz_point, deriv1, deriv2, normal);
	
	if (normalErr == PRO_TK_NO_ERROR) {
		// Store click location (Point3D where user clicked)
		CString xStr, yStr, zStr;
		xStr.Format(_T("%lf"), xyz_point[0]);
		yStr.Format(_T("%lf"), xyz_point[1]);
		zStr.Format(_T("%lf"), xyz_point[2]);
		measureData->ClickLocation = Point3D(xStr, yStr, zStr);
		
		CString clickLog;
		clickLog.Format(_T("Click location (Point3D): [%lf, %lf, %lf]"), xyz_point[0], xyz_point[1], xyz_point[2]);
		LogFileWriter::WriteLog((const char*)CT2A(clickLog));
		
		// Store normal vector (outward normal to the surface)
		CString normalMsg;
		normalMsg.Format(_T("%lf, %lf, %lf"), normal[0], normal[1], normal[2]);
		measureData->Normal = normalMsg;
		
		CString normalLog;
		normalLog.Format(_T("Surface normal vector: %s"), (LPCTSTR)normalMsg);
		LogFileWriter::WriteLog((const char*)CT2A(normalLog));
	}
	
	// Get surface center point for cylindrical surfaces
	if (surfType == PRO_SRF_CYL) {
		// For cylindrical surfaces, get the axis origin as center point
		ProVector axisOrigin = {surfShape.cylinder.origin[0], surfShape.cylinder.origin[1], surfShape.cylinder.origin[2]};
		
		// Store center point (axis origin for cylinder) as Point3D
		CString centerXStr, centerYStr, centerZStr;
		centerXStr.Format(_T("%lf"), axisOrigin[0]);
		centerYStr.Format(_T("%lf"), axisOrigin[1]);
		centerZStr.Format(_T("%lf"), axisOrigin[2]);
		measureData->CenterPoint = Point3D(centerXStr, centerYStr, centerZStr);
		
		CString centerLog;
		centerLog.Format(_T("Surface center point (Point3D): [%lf, %lf, %lf]"), axisOrigin[0], axisOrigin[1], axisOrigin[2]);
		LogFileWriter::WriteLog((const char*)CT2A(centerLog));
	} else {
		// For non-cylindrical surfaces, use the click location as center point
		if (normalErr == PRO_TK_NO_ERROR) {
			// Use the same Point3D as click location for center point
			measureData->CenterPoint = measureData->ClickLocation;
			
			CString centerLog;
			centerLog.Format(_T("Surface center point (using click location): [%lf, %lf, %lf]"), xyz_point[0], xyz_point[1], xyz_point[2]);
			LogFileWriter::WriteLog((const char*)CT2A(centerLog));
		}
	}
	
	// Method 3: Check if surface is part of a hole feature
	if (isHole) {
		ProFeature parentFeature;
		ProError featErr = ProGeomitemFeatureGet(&selectedItem, &parentFeature);
		
		// Log the feature retrieval result
		CString featErrMsg;
		featErrMsg.Format(_T("ProGeomitemFeatureGet returned: %d"), featErr);
		LogFileWriter::WriteLog((const char*)CT2A(featErrMsg));
		
		if (featErr == PRO_TK_NO_ERROR) {
			LogFileWriter::WriteLog("Successfully retrieved parent feature - checking feature type");
			
			ProFeattype featType;
			ProError typeErr = ProFeatureTypeGet(&parentFeature, &featType);
			
			if (typeErr == PRO_TK_NO_ERROR) {
				CString featTypeMsg;
				featTypeMsg.Format(_T("Parent feature type: %d"), featType); 
				LogFileWriter::WriteLog((const char*)CT2A(featTypeMsg));
				
				// Check if parent feature is a hole-type feature
				if (featType == PRO_FEAT_HOLE || featType == PRO_FEAT_SHAFT) {
					isHole = true;
					holeDetectionLog = _T("Detected as HOLE: Surface belongs to hole/shaft feature");
					LogFileWriter::WriteLog((const char*)CT2A(holeDetectionLog));
					
					// Extract detailed hole information directly from the selected surface
					ExtractHoleInfoFromSurface(&selectedItem, holeInfo);
				} else {
					CString nonHoleMsg;
					nonHoleMsg.Format(_T("Parent feature is not a hole type (type: %d)"), featType);
					LogFileWriter::WriteLog((const char*)CT2A(nonHoleMsg));
				}
			} else {
				CString typeErrMsg;
				typeErrMsg.Format(_T("Failed to get feature type, error: %d"), typeErr);
				LogFileWriter::WriteLog((const char*)CT2A(typeErrMsg));
			}
		} else if (featErr == PRO_TK_BAD_INPUTS) {
			LogFileWriter::WriteLog("ProGeomitemFeatureGet returned PRO_TK_BAD_INPUTS - surface may not have a parent feature");
		} else {
			CString featErrMsg;
			featErrMsg.Format(_T("ProGeomitemFeatureGet failed with error: %d"), featErr);
			LogFileWriter::WriteLog((const char*)CT2A(featErrMsg));
		}
	}
	CString radiusMsg = _T("0.0");
	CString diameterMsg = _T("0.0");
	CString perimeterMsg = _T("0.0");

	// Method 4: Geometric analysis for hole detection
	if (surfType == PRO_SRF_CYL) {
		// For cylindrical surfaces, check if it's enclosed (typical of holes)
		double radius = surfShape.cylinder.radius;

		if (radius > 0.0) {
			radiusMsg.Format(_T("%lf"), radius);
			diameterMsg.Format(_T("%lf"), radius * 2.0);
			perimeterMsg.Format(_T("%lf"), 2.0 * M_PI * radius);

			LogFileWriter::WriteLog((const char*)CT2A(_T("Cylindrical surface radius: " + radiusMsg)));
			LogFileWriter::WriteLog((const char*)CT2A(_T("Cylindrical surface diameter: " + diameterMsg)));
			LogFileWriter::WriteLog((const char*)CT2A(_T("Cylindrical surface perimeter: " + perimeterMsg)));

			// Additional heuristic: small cylindrical surfaces with inward orientation are likely holes
			if (radius < 50.0 && surfOrient == PRO_SURF_ORIENT_IN) { // Assuming units are mm
				isHole = true;
				holeDetectionLog = _T("Detected as HOLE: Small cylindrical surface with inward orientation");
				LogFileWriter::WriteLog((const char*)CT2A(holeDetectionLog));
			}
		}
	}

	// Calculate hole depth
	double depth = 0.0;
	ProVector startPoint, endPoint;
	ProUvParam uvStart = { uvMin[0], uvMin[1] }; // Start of cylinder along axis
	ProUvParam uvEnd = { uvMax[0], uvMax[1] };   // End of cylinder along axis

	// Evaluate points at the UV bounds
	err = ProSurfaceXyzdataEval(selectedSurf, uvStart, startPoint, deriv1, deriv2, normal);
	if (err != PRO_TK_NO_ERROR) {
		LogFileWriter::WriteLog("Failed to evaluate start point for hole depth");
	}
	else {
		err = ProSurfaceXyzdataEval(selectedSurf, uvEnd, endPoint, deriv1, deriv2, normal);
		if (err != PRO_TK_NO_ERROR) {
			LogFileWriter::WriteLog("Failed to evaluate end point for hole depth");
		}
		else {
			// Compute depth as the distance along the cylinder axis
			ProVector axisDir = { surfShape.cylinder.origin[0], surfShape.cylinder.origin[1], surfShape.cylinder.origin[2] };
			double delta[3];
			for (int i = 0; i < 3; i++) {
				delta[i] = endPoint[i] - startPoint[i];
			}
			// Project the vector onto the axis direction
			depth = fabs(delta[0] * axisDir[0] + delta[1] * axisDir[1] + delta[2] * axisDir[2]);

			CString depthMsg;
			depthMsg.Format(_T("%lf"), depth);
			holeInfo.HoleDepth = depthMsg;
			LogFileWriter::WriteLog((const char*)CT2A(_T("Hole depth: " + depthMsg)));
		}
	}

	// Store radius and diameter in measurement data
	measureData->Perimeter = perimeterMsg;
	measureData->Radius = radiusMsg;
	measureData->Diameter = diameterMsg;

	holeInfo.HoleDiameter = diameterMsg;

	// Set hole detection result in measurement data
	measureData->IsHole = isHole;
	if (isHole == true) {
		measureData->HoleInfo = holeInfo; // Only set hole info if it's a hole
	}

	// Log final hole detection result
	CString finalHoleMsg;
	finalHoleMsg.Format(_T("Final hole detection result: %s"), isHole ? _T("TRUE - This is a HOLE") : _T("FALSE - This is NOT a hole"));
	LogFileWriter::WriteLog((const char*)CT2A(finalHoleMsg));

	// Set surface type information
	CString surfaceTypeMsg;
	switch (surfType) {
	case PRO_SRF_PLANE:
		surfaceTypeMsg = _T("Plane");
		break;
	case PRO_SRF_CYL:
		surfaceTypeMsg = _T("Cylinder");
		break;
	case PRO_SRF_CONE:
		surfaceTypeMsg = _T("Cone");
		break;
	case PRO_SRF_TORUS:
		surfaceTypeMsg = _T("Torus");
		break;
	case PRO_SRF_SPL:
		surfaceTypeMsg = _T("SPLINESR");
		break;
	case PRO_SRF_FIL:
		surfaceTypeMsg = _T("FILSRF");
		break;
	case PRO_SRF_RUL:
		surfaceTypeMsg = _T("RULSRF");
		break;
	case PRO_SRF_REV:
		surfaceTypeMsg = _T("REV");
		break;
	case PRO_SRF_TABCYL:
		surfaceTypeMsg = _T("TABCYL");
		break;
	case PRO_SRF_B_SPL:
		surfaceTypeMsg = _T("B_SPL");
		break;
	case PRO_SRF_FOREIGN:
		surfaceTypeMsg = _T("FOREIGN");
		break;
	case PRO_SRF_CYL_SPL:
		surfaceTypeMsg = _T("CYL_SPL");
		break;
	case PRO_SRF_SPL2DER:
		surfaceTypeMsg = _T("SPL2DER");
		break;
	default:
		surfaceTypeMsg = _T("OTHER");
		break;
	}
	measureData->SurfaceType = surfaceTypeMsg;

	CString surfTypeLog;
	surfTypeLog.Format(_T("Surface type: %s"), (LPCTSTR)surfaceTypeMsg);
	LogFileWriter::WriteLog(CT2A(surfTypeLog));

	// Reset error status - area calculation failure shouldn't prevent model type return
	LogFileWriter::WriteLog("Resetting error status for model type determination");
	ProSelectionarrayFree(sels);

	err = PRO_TK_NO_ERROR;
	if (mDltype == PRO_PART) {
		LogFileWriter::WriteLog("The current model is a PART");
		return PRO_PART;
	}

	if (mDltype == PRO_SURFACE) {
		LogFileWriter::WriteLog("The current model is SURFACE");
		return PRO_SURFACE;
	}

	if (mDltype == PRO_ASSEMBLY) {
		LogFileWriter::WriteLog("The current model is ASSEMBLY");
		return PRO_ASSEMBLY;
	}
	// Return the model type
	return err;
}

int LeoHelper::IsFaceSelected(MeasurementData *measureData)
{
	// First try to get current face selection without prompting user
	int result = GetCurrentFaceSelection(measureData);
	if (result == PRO_TK_NO_ERROR || result == PRO_PART || result == PRO_SURFACE || result == PRO_ASSEMBLY) {
		LogFileWriter::WriteLog("Successfully processed current face selection without prompting user");
		return result;
	}

	// If no current selection, fall back to prompting user
	LogFileWriter::WriteLog("No current face selection found, prompting user to select face");
	
	ProError err;
	ProMdl currMdl;

	err = ProMdlCurrentGet(&currMdl);
	if (err != PRO_TK_NO_ERROR) return err;

	ProMdlType mDltype = PRO_MDL_UNUSED;
	err = ProMdlTypeGet(currMdl, &mDltype);

	int nSels;
	ProSelection *sels;
	err = ProSelect("datum,surface,sldface,qltface,csys", 1, NULL, NULL, NULL, NULL, &sels, &nSels);

	CString nSelsMsg;
	nSelsMsg.Format(_T("Selected %d Objects"), nSels);
	LogFileWriter::WriteLog((const char*)CT2A(nSelsMsg));
		
	if (err != PRO_TK_NO_ERROR || nSels <= 0 || sels == NULL) {
		return err; // or appropriate error code
	}

	// Now safe to use sels[0]
	ProSelection* sel = &sels[0];

	ProGeomitem selectedItem;
	err = ProSelectionModelitemGet(*sel, &selectedItem);
	if (err != PRO_TK_NO_ERROR) return err;

	ProUvParam paramAtselection;
	err = ProSelectionUvParamGet(*sel, paramAtselection);
	if (err != PRO_TK_NO_ERROR) return err;

	ProSurface selectedSurf;
	err = ProGeomitemToSurface(&selectedItem, &selectedSurf);
	if (err != PRO_TK_NO_ERROR) return err;

	ProGeomitemdata* geomData;
	ProSurfacedata* surfaceData;
	err = ProSurfaceDataGet(selectedSurf, &geomData);
	if (err != PRO_TK_NO_ERROR) return err;

	ProSrftype surfType = PRO_SRF_NONE;
	ProUvParam uvMin, uvMax, Origin;
	ProSurfaceOrient surfOrient;
	ProSurfaceshapedata surfShape;
	int surfId = -1;

	HoleInfo holeInfo;

	err = ProSurfacedataGet(geomData->data.p_surface_data, &surfType, uvMin, uvMax, &surfOrient, &surfShape, &surfId);
	if (err != PRO_TK_NO_ERROR) return err;

	// Check if surface type supports area calculation
	double area = 0.0;
	err = ProSurfaceAreaEval(selectedSurf, &area);

	// Final area log with enhanced information
	CString areaMsg;
	areaMsg.Format(_T("%lf"), area);
	LogFileWriter::WriteLog((const char*)CT2A(_T("Final Surface Area Result: " + areaMsg)));

	measureData->Area = areaMsg;

	// Test section: Determine if the selected surface is a hole
	bool isHole = false;
	CString holeDetectionLog;
	
	LogFileWriter::WriteLog("Starting hole detection analysis...");
	
	// Method 1: Check if surface is cylindrical and oriented inward
	if (surfType == PRO_SRF_CYL) {
		LogFileWriter::WriteLog("Surface is cylindrical - checking orientation for hole detection");
		
		// Check surface orientation - holes typically have inward-facing normals
		if (surfOrient == PRO_SURF_ORIENT_IN) {
			isHole = true;

			holeDetectionLog = _T("Detected as HOLE: Cylindrical surface with inward orientation");
			LogFileWriter::WriteLog((const char*)CT2A(holeDetectionLog));
		} else {
			holeDetectionLog = _T("Cylindrical surface but outward orientation - likely external cylinder");
			LogFileWriter::WriteLog((const char*)CT2A(holeDetectionLog));
		}
	}
	
	// Method 2: Get surface geometry information (click location, normal vector, center point)
	ProVector xyz_point, normal;
	ProVector deriv1[2], deriv2[3];
	
	// Evaluate surface normal at the selection point (click location)
	ProError normalErr = ProSurfaceXyzdataEval(selectedSurf, paramAtselection, xyz_point, deriv1, deriv2, normal);
	
	if (normalErr == PRO_TK_NO_ERROR) {
		// Store click location (Point3D where user clicked)
		CString xStr, yStr, zStr;
		xStr.Format(_T("%lf"), xyz_point[0]);
		yStr.Format(_T("%lf"), xyz_point[1]);
		zStr.Format(_T("%lf"), xyz_point[2]);
		measureData->ClickLocation = Point3D(xStr, yStr, zStr);
		
		CString clickLog;
		clickLog.Format(_T("Click location (Point3D): [%lf, %lf, %lf]"), xyz_point[0], xyz_point[1], xyz_point[2]);
		LogFileWriter::WriteLog((const char*)CT2A(clickLog));
		
		// Store normal vector (outward normal to the surface)
		CString normalMsg;
		normalMsg.Format(_T("%lf, %lf, %lf"), normal[0], normal[1], normal[2]);
		measureData->Normal = normalMsg;
		
		CString normalLog;
		normalLog.Format(_T("Surface normal vector: %s"), (LPCTSTR)normalMsg);
		LogFileWriter::WriteLog((const char*)CT2A(normalLog));
	}
	
	// Get surface center point for cylindrical surfaces
	if (surfType == PRO_SRF_CYL) {
		// For cylindrical surfaces, get the axis origin as center point
		ProVector axisOrigin = {surfShape.cylinder.origin[0], surfShape.cylinder.origin[1], surfShape.cylinder.origin[2]};
		
		// Store center point (axis origin for cylinder) as Point3D
		CString centerXStr, centerYStr, centerZStr;
		centerXStr.Format(_T("%lf"), axisOrigin[0]);
		centerYStr.Format(_T("%lf"), axisOrigin[1]);
		centerZStr.Format(_T("%lf"), axisOrigin[2]);
		measureData->CenterPoint = Point3D(centerXStr, centerYStr, centerZStr);
		
		CString centerLog;
		centerLog.Format(_T("Surface center point (Point3D): [%lf, %lf, %lf]"), axisOrigin[0], axisOrigin[1], axisOrigin[2]);
		LogFileWriter::WriteLog((const char*)CT2A(centerLog));
	} else {
		// For non-cylindrical surfaces, use the click location as center point
		if (normalErr == PRO_TK_NO_ERROR) {
			// Use the same Point3D as click location for center point
			measureData->CenterPoint = measureData->ClickLocation;
			
			CString centerLog;
			centerLog.Format(_T("Surface center point (using click location): [%lf, %lf, %lf]"), xyz_point[0], xyz_point[1], xyz_point[2]);
			LogFileWriter::WriteLog((const char*)CT2A(centerLog));
		}
	}
	
	// Method 3: Check if surface is part of a hole feature
	if (isHole) {
		ProFeature parentFeature;
		ProError featErr = ProGeomitemFeatureGet(&selectedItem, &parentFeature);
		
		// Log the feature retrieval result
		CString featErrMsg;
		featErrMsg.Format(_T("ProGeomitemFeatureGet returned: %d"), featErr);
		LogFileWriter::WriteLog((const char*)CT2A(featErrMsg));
		
		if (featErr == PRO_TK_NO_ERROR) {
			LogFileWriter::WriteLog("Successfully retrieved parent feature - checking feature type");
			
			ProFeattype featType;
			ProError typeErr = ProFeatureTypeGet(&parentFeature, &featType);
			
			if (typeErr == PRO_TK_NO_ERROR) {
				CString featTypeMsg;
				featTypeMsg.Format(_T("Parent feature type: %d"), featType); 
				LogFileWriter::WriteLog((const char*)CT2A(featTypeMsg));
				
				// Check if parent feature is a hole-type feature
				if (featType == PRO_FEAT_HOLE || featType == PRO_FEAT_SHAFT) {
					isHole = true;
					holeDetectionLog = _T("Detected as HOLE: Surface belongs to hole/shaft feature");
					LogFileWriter::WriteLog((const char*)CT2A(holeDetectionLog));
					
					// Extract detailed hole information directly from the selected surface
					ExtractHoleInfoFromSurface(&selectedItem, holeInfo);
				} else {
					CString nonHoleMsg;
					nonHoleMsg.Format(_T("Parent feature is not a hole type (type: %d)"), featType);
					LogFileWriter::WriteLog((const char*)CT2A(nonHoleMsg));
				}
			} else {
				CString typeErrMsg;
				typeErrMsg.Format(_T("Failed to get feature type, error: %d"), typeErr);
				LogFileWriter::WriteLog((const char*)CT2A(typeErrMsg));
			}
		} else if (featErr == PRO_TK_BAD_INPUTS) {
			LogFileWriter::WriteLog("ProGeomitemFeatureGet returned PRO_TK_BAD_INPUTS - surface may not have a parent feature");
		} else {
			CString featErrMsg;
			featErrMsg.Format(_T("ProGeomitemFeatureGet failed with error: %d"), featErr);
			LogFileWriter::WriteLog((const char*)CT2A(featErrMsg));
		}
	}
	
	CString radiusMsg = _T("0.0");
	CString diameterMsg = _T("0.0");
	CString perimeterMsg = _T("0.0");

	// Method 4: Geometric analysis for hole detection
	if (surfType == PRO_SRF_CYL) {
		// For cylindrical surfaces, check if it's enclosed (typical of holes)
		double radius = surfShape.cylinder.radius;

		if (radius > 0.0) {
			radiusMsg.Format(_T("%lf"), radius);
			diameterMsg.Format(_T("%lf"), radius * 2.0);
			perimeterMsg.Format(_T("%lf"), 2.0 * M_PI * radius);

			LogFileWriter::WriteLog((const char*)CT2A(_T("Cylindrical surface radius: " + radiusMsg)));
			LogFileWriter::WriteLog((const char*)CT2A(_T("Cylindrical surface diameter: " + diameterMsg)));
			LogFileWriter::WriteLog((const char*)CT2A(_T("Cylindrical surface perimeter: " + perimeterMsg)));
			
			// Additional heuristic: small cylindrical surfaces with inward orientation are likely holes
			if (radius < 50.0 && surfOrient == PRO_SURF_ORIENT_IN) { // Assuming units are mm
				isHole = true;
				holeDetectionLog = _T("Detected as HOLE: Small cylindrical surface with inward orientation");
				LogFileWriter::WriteLog((const char*)CT2A(holeDetectionLog));
			}
		}
	}

	// Calculate hole depth
	double depth = 0.0;
	ProVector startPoint, endPoint;
	ProUvParam uvStart = { uvMin[0], uvMin[1] }; // Start of cylinder along axis
	ProUvParam uvEnd = { uvMax[0], uvMax[1] };   // End of cylinder along axis

	// Evaluate points at the UV bounds
	err = ProSurfaceXyzdataEval(selectedSurf, uvStart, startPoint, deriv1, deriv2, normal);
	if (err != PRO_TK_NO_ERROR) {
		LogFileWriter::WriteLog("Failed to evaluate start point for hole depth");
	}
	else {
		err = ProSurfaceXyzdataEval(selectedSurf, uvEnd, endPoint, deriv1, deriv2, normal);
		if (err != PRO_TK_NO_ERROR) {
			LogFileWriter::WriteLog("Failed to evaluate end point for hole depth");
		}
		else {
			// Compute depth as the distance along the cylinder axis
			ProVector axisDir = { surfShape.cylinder.origin[0], surfShape.cylinder.origin[1], surfShape.cylinder.origin [2]};
			double delta[3];
			for (int i = 0; i < 3; i++) {
				delta[i] = endPoint[i] - startPoint[i];
			}
			// Project the vector onto the axis direction
			depth = fabs(delta[0] * axisDir[0] + delta[1] * axisDir[1] + delta[2] * axisDir[2]);

			CString depthMsg;
			depthMsg.Format(_T("%lf"), depth);
			holeInfo.HoleDepth = depthMsg;
			LogFileWriter::WriteLog((const char*)CT2A(_T("Hole depth: " + depthMsg)));
		}
	}

	// Store radius and diameter in measurement data
	measureData->Perimeter = perimeterMsg;
	measureData->Radius = radiusMsg;
	measureData->Diameter = diameterMsg;

	holeInfo.HoleDiameter = diameterMsg;
	
	// Set hole detection result in measurement data
	measureData->IsHole = isHole;
	if ( isHole == true ) {
		measureData->HoleInfo = holeInfo; // Only set hole info if it's a hole
	}
	
	// Log final hole detection result
	CString finalHoleMsg;
	finalHoleMsg.Format(_T("Final hole detection result: %s"), isHole ? _T("TRUE - This is a HOLE") : _T("FALSE - This is NOT a hole"));
	LogFileWriter::WriteLog((const char*)CT2A(finalHoleMsg));
	
	// Set surface type information
	CString surfaceTypeMsg;
	switch (surfType) {
		case PRO_SRF_PLANE:
			surfaceTypeMsg = _T("Plane");
			break;
		case PRO_SRF_CYL:
			surfaceTypeMsg = _T("Cylinder");
			break;
		case PRO_SRF_CONE:
			surfaceTypeMsg = _T("Cone");
			break;
		case PRO_SRF_TORUS:
			surfaceTypeMsg = _T("Torus");
			break;
		case PRO_SRF_SPL:
			surfaceTypeMsg = _T("SPLINESR");
			break;
		case PRO_SRF_FIL:
			surfaceTypeMsg = _T("FILSRF");
			break;
		case PRO_SRF_RUL:
			surfaceTypeMsg = _T("RULSRF");
			break;
		case PRO_SRF_REV:
			surfaceTypeMsg = _T("REV");
			break;
		case PRO_SRF_TABCYL:
			surfaceTypeMsg = _T("TABCYL");
			break;
		case PRO_SRF_B_SPL:
			surfaceTypeMsg = _T("B_SPL");
			break;
		case PRO_SRF_FOREIGN:
			surfaceTypeMsg = _T("FOREIGN");
			break;
		case PRO_SRF_CYL_SPL:
			surfaceTypeMsg = _T("CYL_SPL");
			break;
		case PRO_SRF_SPL2DER:
			surfaceTypeMsg = _T("SPL2DER");
			break;
		default:
			surfaceTypeMsg = _T("OTHER");
			break;
	}
	measureData->SurfaceType = surfaceTypeMsg;
	
	CString surfTypeLog;
	surfTypeLog.Format(_T("Surface type: %s"), (LPCTSTR)surfaceTypeMsg);
	LogFileWriter::WriteLog(CT2A(surfTypeLog));

	// Reset error status - area calculation failure shouldn't prevent model type return
	LogFileWriter::WriteLog("Resetting error status for model type determination");

	err = PRO_TK_NO_ERROR;
	if (mDltype == PRO_PART) {
		LogFileWriter::WriteLog("The current model is a PART");
		return PRO_PART;
	}

	if (mDltype == PRO_SURFACE) {
		LogFileWriter::WriteLog("The current model is SURFACE");
		return PRO_SURFACE;
	}
	
	if (mDltype == PRO_ASSEMBLY) {
		LogFileWriter::WriteLog("The current model is ASSEMBLY");
		return PRO_ASSEMBLY;
	}
	return 0;

}

void LeoHelper::ExtractHoleInfo(ProFeature* holeFeature, HoleInfo& holeInfo)
{
    ProError err;
    ProElement elemTree = NULL;
    
    // Extract the feature element tree
    err = ProFeatureElemtreeExtract(holeFeature, NULL, PRO_FEAT_EXTRACT_NO_OPTS, &elemTree);
    if (err != PRO_TK_NO_ERROR) {
        LogFileWriter::WriteLog("Failed to extract hole feature element tree");
        return;
    }
    
    // Get the model for thread series functions
    ProMdl model;
    err = ProMdlCurrentGet(&model);
    if (err != PRO_TK_NO_ERROR) {
        LogFileWriter::WriteLog("Failed to get current model for hole info extraction");
        ProFeatureElemtreeFree(holeFeature, elemTree);
        return;
    }
    
    // Extract thread series (ThreadSize)
    wchar_t* threadSeries = NULL;
    err = ProElementHoleThreadSeriesGet(elemTree, model, &threadSeries);
    if (err == PRO_TK_NO_ERROR && threadSeries != NULL) {
        holeInfo.ThreadSize = CString(threadSeries);
        ProWstringFree(threadSeries);
        LogFileWriter::WriteLog((const char*)CT2A(_T("Thread Size: " + holeInfo.ThreadSize)));
    } else {
        holeInfo.ThreadSize = _T("N/A");
        LogFileWriter::WriteLog("Thread Size: Not available");
    }
    
    // Extract screw size (ThreadSize alternative)
    wchar_t* screwSize = NULL;
    err = ProElementHoleScrewSizeGet(elemTree, model, &screwSize);
    if (err == PRO_TK_NO_ERROR && screwSize != NULL) {
        if (holeInfo.ThreadSize == _T("N/A")) {
            holeInfo.ThreadSize = CString(screwSize);
        }
        ProWstringFree(screwSize);
        LogFileWriter::WriteLog((const char*)CT2A(_T("Screw Size: " + holeInfo.ThreadSize)));
    }
    
    // Extract hole information by traversing the element tree
    ProElement* childElems = NULL;
    err = ProElementChildrenGet(elemTree, NULL, &childElems);
    if (err == PRO_TK_NO_ERROR && childElems != NULL) {
        // Get the number of children by checking if the first element is valid
        int numChildren = 0;
        ProElement currentElem = childElems[0];
        while (currentElem != NULL) {
            numChildren++;
            currentElem = childElems[numChildren];
        }
        
        CString numChildrenMsg;
        numChildrenMsg.Format(_T("Number of child elements: %d"), numChildren);
        LogFileWriter::WriteLog((const char*)CT2A(numChildrenMsg));
        
        for (int i = 0; i < numChildren; i++) {
            ProElement childElem = childElems[i];
            if (childElem != NULL) {
                ProElemId elemId;
                err = ProElementIdGet(childElem, &elemId);
                if (err == PRO_TK_NO_ERROR) {
                    CString elemIdMsg;
                    elemIdMsg.Format(_T("Element ID: %d"), elemId);
                    LogFileWriter::WriteLog((const char*)CT2A(elemIdMsg));
                    
                    // Check for feature form (hole type)
                    if (elemId == PRO_E_FEATURE_FORM) {
                        int holeType;
                        err = ProElementIntegerGet(childElem, NULL, &holeType);
                        if (err == PRO_TK_NO_ERROR) {
                            switch (holeType) {
                                case PRO_HLE_NEW_TYPE_STRAIGHT:
                                    holeInfo.HoleType = _T("Straight");
                                    break;
                                case PRO_HLE_NEW_TYPE_SKETCH:
                                    holeInfo.HoleType = _T("Sketched");
                                    break;
                                case PRO_HLE_NEW_TYPE_STANDARD:
                                    holeInfo.HoleType = _T("Standard");
                                    break;
                                case PRO_HLE_CUSTOM_TYPE:
                                    holeInfo.HoleType = _T("Custom");
                                    break;
                                default:
                                    holeInfo.HoleType = _T("Unknown");
                                    break;
                            }
                            LogFileWriter::WriteLog((const char*)CT2A(_T("Hole Type: " + holeInfo.HoleType)));
                        }
                    }
                    // Check for standard type
                    else if (elemId == PRO_E_HLE_STAN_TYPE) {
                        int standardType;
                        err = ProElementIntegerGet(childElem, NULL, &standardType);
                        if (err == PRO_TK_NO_ERROR) {
                            switch (standardType) {
                                case PRO_HLE_TAPPED_TYPE:
                                    holeInfo.Standard = _T("Tapped");
                                    break;
                                case PRO_HLE_CLEARANCE_TYPE:
                                    holeInfo.Standard = _T("Clearance");
                                    break;
                                case PRO_HLE_DRILLED_TYPE:
                                    holeInfo.Standard = _T("Drilled");
                                    break;
                                case PRO_HLE_TAPERED_TYPE:
                                    holeInfo.Standard = _T("Tapered");
                                    break;
                                default:
                                    holeInfo.Standard = _T("Unknown");
                                    break;
                            }
                            LogFileWriter::WriteLog((const char*)CT2A(_T("Standard: " + holeInfo.Standard)));
                        }
                    }
                    // Check for fit type (thread class)
                    else if (elemId == PRO_E_HLE_FITTYPE) {
                        int fitType;
                        err = ProElementIntegerGet(childElem, NULL, &fitType);
                        if (err == PRO_TK_NO_ERROR) {
                            switch (fitType) {
                                case PRO_HLE_CLOSE_FIT:
                                    holeInfo.ThreadClass = _T("Close Fit");
                                    break;
                                case PRO_HLE_FREE_FIT:
                                    holeInfo.ThreadClass = _T("Free Fit");
                                    break;
                                case PRO_HLE_MEDIUM_FIT:
                                    holeInfo.ThreadClass = _T("Medium Fit");
                                    break;
                                default:
                                    holeInfo.ThreadClass = _T("N/A");
                                    break;
                            }
                            LogFileWriter::WriteLog((const char*)CT2A(_T("Thread Class: " + holeInfo.ThreadClass)));
                        }
                    }
                }
            }
        }
    }
    
    	// Clean up
	ProFeatureElemtreeFree(holeFeature, elemTree);
}

void LeoHelper::ExtractHoleInfoFromSurface(ProGeomitem* selectedItem, HoleInfo& holeInfo)
{
	ProError err;
	
	// Convert ProGeomitem to ProSurface
	ProSurface selectedSurf;
	err = ProGeomitemToSurface(selectedItem, &selectedSurf);
	if (err != PRO_TK_NO_ERROR) {
		LogFileWriter::WriteLog("Failed to convert ProGeomitem to ProSurface");
		return;
	}
	
	// Get surface type to determine if it's cylindrical (hole-like)
	ProSrftype surfType;
	err = ProSurfaceTypeGet(selectedSurf, &surfType);
	if (err != PRO_TK_NO_ERROR) {
		LogFileWriter::WriteLog("Failed to get surface type");
		return;
	}
	
	// Check if it's a cylindrical surface (typical of holes)
	if (surfType == PRO_SRF_CYL || surfType == PRO_SRF_TABCYL || surfType == PRO_SRF_CYL_SPL) {
		// Get hole diameter using ProSurfaceDiameterEval
		ProUvParam uvPoint = {0.0, 0.0}; // Use origin point for diameter evaluation
		double diameter;
		err = ProSurfaceDiameterEval(selectedSurf, uvPoint, &diameter);
		if (err == PRO_TK_NO_ERROR) {
			// Convert double to CString for HoleDiameter
			CString diameterStr;
			diameterStr.Format(_T("%.3f"), diameter);
			holeInfo.HoleDiameter = diameterStr;
			CString diameterMsg;
			diameterMsg.Format(_T("Hole Diameter: %.3f"), diameter);
			LogFileWriter::WriteLog((const char*)CT2A(diameterMsg));
		} else {
			holeInfo.HoleDiameter = _T("0.0");
			LogFileWriter::WriteLog("Failed to get hole diameter");
		}
		
		// Get surface area to help estimate depth
		double surfaceArea;
		err = ProSurfaceAreaEval(selectedSurf, &surfaceArea);
		if (err == PRO_TK_NO_ERROR) {
					// Estimate depth from surface area and diameter
		// For a cylinder: Area = π * diameter * height
		if (diameter > 0.0) {
			double estimatedDepth = surfaceArea / (M_PI * diameter);
			// Convert double to CString for HoleDepth
			CString depthStr;
			depthStr.Format(_T("%.3f"), estimatedDepth);
			holeInfo.HoleDepth = depthStr;
			CString depthMsg;
			depthMsg.Format(_T("Estimated Hole Depth: %.3f"), estimatedDepth);
			LogFileWriter::WriteLog((const char*)CT2A(depthMsg));
		} else {
			holeInfo.HoleDepth = _T("0.0");
		}
		} else {
			holeInfo.HoleDepth = _T("0.0");
			LogFileWriter::WriteLog("Failed to get surface area for depth estimation");
		}
		
		// Set hole type based on surface type
		if (surfType == PRO_SRF_CYL) {
			holeInfo.HoleType = _T("Straight Cylindrical");
		} else if (surfType == PRO_SRF_TABCYL) {
			holeInfo.HoleType = _T("Tapered Cylindrical");
		} else if (surfType == PRO_SRF_CYL_SPL) {
			holeInfo.HoleType = _T("Spline Cylindrical");
		}
		
		// Set standard based on typical hole characteristics
		holeInfo.Standard = _T("Standard");
		
		// Set thread class based on diameter (typical engineering standards)
		if (diameter > 0.0) {
			if (diameter < 3.0) {
				holeInfo.ThreadClass = _T("Fine Thread");
			} else if (diameter < 10.0) {
				holeInfo.ThreadClass = _T("Standard Thread");
			} else {
				holeInfo.ThreadClass = _T("Coarse Thread");
			}
		} else {
			holeInfo.ThreadClass = _T("N/A");
		}
		
		// Set thread size based on diameter
		if (diameter > 0.0) {
			CString threadSizeMsg;
			threadSizeMsg.Format(_T("M%.1f"), diameter);
			holeInfo.ThreadSize = threadSizeMsg;
		} else {
			holeInfo.ThreadSize = _T("N/A");
		}
		
		LogFileWriter::WriteLog("Successfully extracted hole information from surface");
	} else {
		// Not a cylindrical surface, set default values
		holeInfo.HoleType = _T("Non-Cylindrical");
		holeInfo.Standard = _T("Unknown");
		holeInfo.ThreadClass = _T("N/A");
		holeInfo.ThreadSize = _T("N/A");
		holeInfo.HoleDiameter = _T("0.0");
		holeInfo.HoleDepth = _T("0.0");
		LogFileWriter::WriteLog("Surface is not cylindrical - cannot extract hole information");
	}
}
