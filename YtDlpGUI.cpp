#include "pch.h"
#include "YtDlpGUI.h"
#include "YtDlpGUIDlg.h"

CYtDlpGUIApp theApp;

BOOL CYtDlpGUIApp::InitInstance()
{
	INITCOMMONCONTROLSEX icc = {};
	icc.dwSize = sizeof(icc);
	icc.dwICC = ICC_WIN95_CLASSES;
	InitCommonControlsEx(&icc);

	CWinApp::InitInstance();
	SetRegistryKey(_T("YtDlpGUI"));

	CYtDlpGUIDlg dlg;
	m_pMainWnd = &dlg;
	dlg.DoModal();
	return FALSE;
}
