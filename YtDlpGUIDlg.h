#pragma once
#include "resource.h"

#define WM_APP_LOG   (WM_APP + 1)   // wParam: CString* line, lParam: 1=search 0=download
#define WM_APP_DONE  (WM_APP + 2)   // wParam: exit code,     lParam: 1=search 0=download

class CYtDlpGUIDlg : public CDialogEx
{
public:
	CYtDlpGUIDlg(CWnd* pParent = nullptr);
	enum { IDD = IDD_YTDLPGUI_DIALOG };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);
	virtual BOOL OnInitDialog();
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	virtual void OnOK() {}
	virtual void OnCancel();

	afx_msg void OnBnClickedSearch();
	afx_msg void OnBnClickedBrowseSave();
	afx_msg void OnBnClickedStart();
	afx_msg void OnBnClickedStop();
	afx_msg void OnBnClickedOpenDir();
	afx_msg void OnBnClickedCheckAll();
	afx_msg void OnNMDblclkResults(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg LRESULT OnAppLog(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnAppDone(WPARAM wParam, LPARAM lParam);
	DECLARE_MESSAGE_MAP()

private:
	void StartProcess(bool isSearch, const CString& exe, const CString& args);
	void UpdateButtons();
	void AppendLog(const CString& text);
	void ParseProgress(const CString& line);
	void SaveSettings();
	CString CommonArgs();
	bool CheckYtDlp(CString& exe);

	CListCtrl     m_list;
	CComboBox     m_cmbFormat;
	CComboBox     m_cmbCookies;
	CComboBox     m_cmbCount;
	CProgressCtrl m_progress;
	CEdit         m_editLog;

	HANDLE      m_hJob = nullptr;
	CWinThread* m_pThread = nullptr;
	bool        m_running = false;
	bool        m_stopRequested = false;
	bool        m_isSearch = false;
};
