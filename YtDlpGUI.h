#pragma once
#include "resource.h"

class CYtDlpGUIApp : public CWinApp
{
public:
	CYtDlpGUIApp() {}
	virtual BOOL InitInstance();
};

extern CYtDlpGUIApp theApp;
