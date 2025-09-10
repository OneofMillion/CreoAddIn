// stdafx.h: Include file for standard system include files
// or project-specific include files that are frequently used but rarely changed
//

#pragma once

#ifndef VC_EXTRALEAN
#define VC_EXTRALEAN // Exclude rarely used material from Windows headers
#endif

#include "targetver.h"

#define _ATL_CSTRING_EXPLICIT_CONSTRUCTORS // Certain CString constructors will be explicit

#include <afxwin.h> // MFC core and standard components
#include <afxext.h> // MFC extensions

#ifndef _AFX_NO_OLE_SUPPORT
#include <afxole.h> // MFC OLE classes
#include <afxodlgs.h> // MFC OLE dialog classes
#include <afxdisp.h> // MFC automation classes
#endif // _AFX_NO_OLE_SUPPORT

#ifndef _AFX_NO_DB_SUPPORT
#include <afxdb.h> // MFC ODBC database class
#endif // _AFX_NO_DB_SUPPORT

#ifndef _AFX_NO_DAO_SUPPORT
#include <afxdao.h> // MFC DAO database class
#endif // _AFX_NO_DAO_SUPPORT

#ifndef _AFX_NO_OLE_SUPPORT
#include <afxdtctl.h> // MFC support for Internet Explorer 4 common controls
#endif
#ifndef _AFX_NO_AFXCMN_SUPPORT
#include <afxcmn.h> // MFC support for Windows common controls
#endif // _AFX_NO_AFXCMN_SUPPORT



#include <ProUICmd.h>
#include <ProUtil.h>
#include <ProMenu.h>
#include <ProUIMessage.h>
#include <ProToolkitDll.h>
#include <ProMenubar.h>
#include <ProArray.h>
#include <ProNotify.h>
#include <ProModelitem.h>
#include <ProPopupmenu.h>
#include <ProMessage.h>
#include <ProRibbon.h>
#include <ProToolkit.h>
#include <ProObjects.h>
#include <ProSelection.h>
#include <ProSelbuffer.h>
#include <ProSurface.h>
#include <ProSolid.h>
#include <ProMdl.h>
#include <ProGeomitem.h>
#include <ProMechWeld.h>
#include <ProMechItem.h>
#include <ProEdge.h>
#include <ProContour.h>
#include <ProFeature.h>
#include <ProFeatType.h>
#include <ProHole.h>
#include <ProElement.h>
#include <ProWstring.h>
#include <ProWindows.h>
#include <ProCore.h>
#include <shellapi.h> // For ShellExecuteEx
