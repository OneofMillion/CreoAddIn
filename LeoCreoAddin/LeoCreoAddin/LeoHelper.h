#pragma once

#include "LeoWebClient.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

class LeoHelper
{
public:
	LeoHelper();	
	~LeoHelper();
	void GetFaceDataAction();
	int GetCurrentFaceSelection(MeasurementData* measureData);
	int IsFaceSelected(MeasurementData* measureDate);
	void ExtractHoleInfo(ProFeature* holeFeature, HoleInfo& holeInfo);
	void ExtractHoleInfoFromSurface(ProGeomitem* selectedItem, HoleInfo& holeInfo);
};

