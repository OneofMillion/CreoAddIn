// LeoCreoAddin.h :  Main header file for the LeoCreoAddin DLL
//

#pragma once

#ifndef __AFXWIN_H__
#error "Include 'stdafx.h' before including this file to generate the PCH file"
#endif

#include "resource.h" // Main symbols
#include "LeoConfig.h" // Leo AI configuration


// CLeoCreoAddinApp
// For information on this class implementation, see CreoAddin.cpp

class CLeoCreoAddinApp : public CWinApp
{
public:
	CLeoCreoAddinApp();

// Override
public:
	virtual BOOL InitInstance();
	virtual int ExitInstance();

	DECLARE_MESSAGE_MAP()
};
