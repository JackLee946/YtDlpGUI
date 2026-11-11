#pragma once

#include <SDKDDKVer.h>
#define VC_EXTRALEAN

#include <afxwin.h>
#include <afxext.h>
#include <afxdlgs.h>
#include <afxcmn.h>
#include <afxdialogex.h>
#include <shellapi.h>
#include <shlobj.h>

#if defined _M_X64
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='amd64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#endif
