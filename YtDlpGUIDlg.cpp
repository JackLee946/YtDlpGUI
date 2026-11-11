#include "pch.h"
#include "YtDlpGUI.h"
#include "YtDlpGUIDlg.h"
#include <memory>
#include <string>
#include <vector>

namespace
{
	const wchar_t* kSection = L"Settings";

	struct RunJob
	{
		CString exe;
		CString args;
		bool    isSearch = false;
		HWND    hwnd = nullptr;
		HANDLE  hJob = nullptr;
	};

	CString DecodeBytes(const std::string& s)
	{
		CString out;
		if (s.empty()) return out;
		UINT cp = CP_UTF8;
		DWORD flags = MB_ERR_INVALID_CHARS;
		int n = ::MultiByteToWideChar(cp, flags, s.data(), (int)s.size(), nullptr, 0);
		if (n <= 0) { cp = CP_ACP; flags = 0; n = ::MultiByteToWideChar(cp, flags, s.data(), (int)s.size(), nullptr, 0); }
		if (n <= 0) return out;
		wchar_t* buf = out.GetBuffer(n);
		::MultiByteToWideChar(cp, flags, s.data(), (int)s.size(), buf, n);
		out.ReleaseBuffer(n);
		return out;
	}

	CString TrimPath(CString p)
	{
		p.Trim();
		p.Remove(L'"');
		while (p.GetLength() > 3 && (p.Right(1) == L"\\" || p.Right(1) == L"/"))
			p = p.Left(p.GetLength() - 1);
		return p;
	}

	bool FileExists(const CString& p)
	{
		DWORD a = ::GetFileAttributesW(p);
		return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
	}

	// 本程序 exe 所在目录 (yt-dlp.exe / ffmpeg.exe 与之放在一起)
	CString GetAppDir()
	{
		wchar_t buf[MAX_PATH];
		DWORD n = ::GetModuleFileNameW(nullptr, buf, MAX_PATH);
		CString s(buf, (int)n);
		int i = s.ReverseFind(L'\\');
		return i >= 0 ? s.Left(i) : s;
	}

	CString FormatArgs(int sel)
	{
		switch (sel)
		{
		case 1:  return L" -f \"bv*[height<=1080]+ba/b[height<=1080]\" --merge-output-format mp4";
		case 2:  return L" -f \"bv*[height<=720]+ba/b[height<=720]\" --merge-output-format mp4";
		case 3:  return L" -f \"bv*[vcodec^=avc1]+ba[ext=m4a]/b[ext=mp4]/b\" --merge-output-format mp4";
		case 4:  return L" -x --audio-format mp3";
		case 5:  return L" -x --audio-format m4a";
		default: return L" -f \"bv*+ba/b\" --merge-output-format mp4";
		}
	}

	UINT RunThreadProc(LPVOID pParam)
	{
		std::unique_ptr<RunJob> job(static_cast<RunJob*>(pParam));
		DWORD exitCode = (DWORD)-1;
		const LPARAM mode = job->isSearch ? 1 : 0;

		auto postLog = [&](const CString& s) {
			::PostMessageW(job->hwnd, WM_APP_LOG, (WPARAM)new CString(s), mode);
		};

		SECURITY_ATTRIBUTES sa = { sizeof(sa), nullptr, TRUE };
		HANDLE hRead = nullptr, hWrite = nullptr;
		if (!::CreatePipe(&hRead, &hWrite, &sa, 0))
		{
			postLog(L"创建管道失败");
			::PostMessageW(job->hwnd, WM_APP_DONE, exitCode, mode);
			return 0;
		}
		::SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);
		HANDLE hNul = ::CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, nullptr);

		STARTUPINFOW si = {};
		si.cb = sizeof(si);
		si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
		si.wShowWindow = SW_HIDE;
		si.hStdInput = hNul;
		si.hStdOutput = hWrite;
		si.hStdError = hWrite;

		CString cmd;
		cmd = L"\"" + job->exe + L"\" " + job->args;

		PROCESS_INFORMATION pi = {};
		BOOL ok = ::CreateProcessW(nullptr, cmd.GetBuffer(), nullptr, nullptr, TRUE,
			CREATE_NO_WINDOW | CREATE_SUSPENDED, nullptr, nullptr, &si, &pi);
		DWORD err = ::GetLastError();
		cmd.ReleaseBuffer();
		::CloseHandle(hWrite);
		if (hNul != INVALID_HANDLE_VALUE) ::CloseHandle(hNul);

		if (!ok)
		{
			CString m; m.Format(L"启动进程失败，错误码 %lu", err);
			postLog(m);
		}
		else
		{
			::AssignProcessToJobObject(job->hJob, pi.hProcess);
			::ResumeThread(pi.hThread);
			::CloseHandle(pi.hThread);

			std::string buf;
			char chunk[4096];
			DWORD n = 0;
			while (::ReadFile(hRead, chunk, sizeof(chunk), &n, nullptr) && n > 0)
			{
				buf.append(chunk, n);
				size_t pos;
				while ((pos = buf.find('\n')) != std::string::npos)
				{
					std::string line = buf.substr(0, pos);
					buf.erase(0, pos + 1);
					if (!line.empty() && line.back() == '\r') line.pop_back();
					postLog(DecodeBytes(line));
				}
			}
			if (!buf.empty()) postLog(DecodeBytes(buf));

			::WaitForSingleObject(pi.hProcess, INFINITE);
			::GetExitCodeProcess(pi.hProcess, &exitCode);
			::CloseHandle(pi.hProcess);
		}
		::CloseHandle(hRead);
		::PostMessageW(job->hwnd, WM_APP_DONE, exitCode, mode);
		return 0;
	}
}
CYtDlpGUIDlg::CYtDlpGUIDlg(CWnd* pParent)
	: CDialogEx(IDD_YTDLPGUI_DIALOG, pParent)
{
}

void CYtDlpGUIDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_LIST_RESULTS, m_list);
	DDX_Control(pDX, IDC_COMBO_FORMAT, m_cmbFormat);
	DDX_Control(pDX, IDC_COMBO_COOKIES, m_cmbCookies);
	DDX_Control(pDX, IDC_COMBO_COUNT, m_cmbCount);
	DDX_Control(pDX, IDC_PROGRESS, m_progress);
	DDX_Control(pDX, IDC_EDIT_LOG, m_editLog);
}

BEGIN_MESSAGE_MAP(CYtDlpGUIDlg, CDialogEx)
	ON_BN_CLICKED(IDC_BTN_SEARCH, &CYtDlpGUIDlg::OnBnClickedSearch)
	ON_BN_CLICKED(IDC_BTN_BROWSE_SAVE, &CYtDlpGUIDlg::OnBnClickedBrowseSave)
	ON_BN_CLICKED(IDC_BTN_START, &CYtDlpGUIDlg::OnBnClickedStart)
	ON_BN_CLICKED(IDC_BTN_STOP, &CYtDlpGUIDlg::OnBnClickedStop)
	ON_BN_CLICKED(IDC_BTN_OPENDIR, &CYtDlpGUIDlg::OnBnClickedOpenDir)
	ON_BN_CLICKED(IDC_CHECK_ALL, &CYtDlpGUIDlg::OnBnClickedCheckAll)
	ON_NOTIFY(NM_DBLCLK, IDC_LIST_RESULTS, &CYtDlpGUIDlg::OnNMDblclkResults)
	ON_MESSAGE(WM_APP_LOG, &CYtDlpGUIDlg::OnAppLog)
	ON_MESSAGE(WM_APP_DONE, &CYtDlpGUIDlg::OnAppDone)
END_MESSAGE_MAP()

BOOL CYtDlpGUIDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// 让 yt-dlp 以 UTF-8 输出，避免中文标题乱码
	::SetEnvironmentVariableW(L"PYTHONUTF8", L"1");
	::SetEnvironmentVariableW(L"PYTHONIOENCODING", L"utf-8");

	m_list.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_CHECKBOXES);
	CRect rc; m_list.GetClientRect(&rc);
	int w = rc.Width();
	m_list.InsertColumn(0, L"标题", LVCFMT_LEFT, w * 55 / 100);
	m_list.InsertColumn(1, L"时长", LVCFMT_LEFT, w * 12 / 100);
	m_list.InsertColumn(2, L"链接", LVCFMT_LEFT, w * 30 / 100);

	m_cmbFormat.AddString(L"最佳画质 (mp4)");
	m_cmbFormat.AddString(L"最高 1080p (mp4)");
	m_cmbFormat.AddString(L"最高 720p (mp4)");
	m_cmbFormat.AddString(L"H.264+AAC 兼容 mp4");
	m_cmbFormat.AddString(L"仅音频 (mp3)");
	m_cmbFormat.AddString(L"仅音频 (m4a)");

	m_cmbCookies.AddString(L"不使用");
	m_cmbCookies.AddString(L"chrome");
	m_cmbCookies.AddString(L"edge");
	m_cmbCookies.AddString(L"firefox");

	m_cmbCount.AddString(L"10");
	m_cmbCount.AddString(L"20");
	m_cmbCount.AddString(L"30");
	m_cmbCount.AddString(L"50");
	m_cmbCount.AddString(L"100");

	m_progress.SetRange(0, 100);
	m_editLog.SetLimitText(1000000);

	CWinApp* app = AfxGetApp();
	SetDlgItemText(IDC_EDIT_SAVEDIR, app->GetProfileString(kSection, L"SaveDir", L"D:\\Videos"));
	SetDlgItemText(IDC_EDIT_PROXY, app->GetProfileString(kSection, L"Proxy", L""));
	m_cmbFormat.SetCurSel(app->GetProfileInt(kSection, L"Format", 0));
	m_cmbCookies.SetCurSel(app->GetProfileInt(kSection, L"Cookies", 0));
	m_cmbCount.SetCurSel(app->GetProfileInt(kSection, L"Count", 1));
	CheckDlgButton(IDC_CHECK_NOPLAYLIST, app->GetProfileInt(kSection, L"NoPlaylist", 1) ? BST_CHECKED : BST_UNCHECKED);

	UpdateButtons();
	return TRUE;
}

BOOL CYtDlpGUIDlg::PreTranslateMessage(MSG* pMsg)
{
	if (pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_RETURN && GetFocus() == GetDlgItem(IDC_EDIT_SEARCH))
	{
		OnBnClickedSearch();
		return TRUE;
	}
	return CDialogEx::PreTranslateMessage(pMsg);
}

void CYtDlpGUIDlg::OnCancel()
{
	if (m_running)
	{
		if (AfxMessageBox(L"任务正在执行，确定退出吗？", MB_YESNO | MB_ICONQUESTION) != IDYES)
			return;
		m_stopRequested = true;
		if (m_hJob) ::TerminateJobObject(m_hJob, 1);
		if (m_pThread)
		{
			::WaitForSingleObject(m_pThread->m_hThread, 5000);
			delete m_pThread;
			m_pThread = nullptr;
		}
		if (m_hJob) { ::CloseHandle(m_hJob); m_hJob = nullptr; }
		m_running = false;
	}
	SaveSettings();
	CDialogEx::OnCancel();
}

void CYtDlpGUIDlg::SaveSettings()
{
	CWinApp* app = AfxGetApp();
	CString s;
	GetDlgItemText(IDC_EDIT_SAVEDIR, s); app->WriteProfileString(kSection, L"SaveDir", s);
	GetDlgItemText(IDC_EDIT_PROXY, s);   app->WriteProfileString(kSection, L"Proxy", s);
	app->WriteProfileInt(kSection, L"Format", m_cmbFormat.GetCurSel());
	app->WriteProfileInt(kSection, L"Cookies", m_cmbCookies.GetCurSel());
	app->WriteProfileInt(kSection, L"Count", m_cmbCount.GetCurSel());
	app->WriteProfileInt(kSection, L"NoPlaylist", IsDlgButtonChecked(IDC_CHECK_NOPLAYLIST) == BST_CHECKED ? 1 : 0);
}

void CYtDlpGUIDlg::UpdateButtons()
{
	GetDlgItem(IDC_BTN_START)->EnableWindow(!m_running);
	GetDlgItem(IDC_BTN_SEARCH)->EnableWindow(!m_running);
	GetDlgItem(IDC_BTN_STOP)->EnableWindow(m_running);
}

void CYtDlpGUIDlg::AppendLog(const CString& text)
{
	int len = m_editLog.GetWindowTextLength();
	if (len > 200000)
	{
		m_editLog.SetSel(0, 100000);
		m_editLog.ReplaceSel(L"");
		len = m_editLog.GetWindowTextLength();
	}
	m_editLog.SetSel(len, len);
	m_editLog.ReplaceSel(text + L"\r\n");
}

void CYtDlpGUIDlg::ParseProgress(const CString& line)
{
	if (line.Left(10) != L"[download]") return;
	int pct = line.Find(L'%');
	if (pct <= 0) return;
	int s = pct;
	while (s > 0 && (iswdigit(line[s - 1]) || line[s - 1] == L'.')) s--;
	if (s == pct) return;
	double v = _wtof(line.Mid(s, pct - s));
	m_progress.SetPos((int)v);
	CString st = line.Mid(10);
	st.Trim();
	SetDlgItemText(IDC_STATIC_STATUS, st);
}

CString CYtDlpGUIDlg::CommonArgs()
{
	CString args = L"--newline";
	wchar_t found[MAX_PATH];
	if (::SearchPathW(nullptr, L"node.exe", nullptr, MAX_PATH, found, nullptr) > 0)
		args += L" --js-runtimes node";
	CString proxy;
	GetDlgItemText(IDC_EDIT_PROXY, proxy);
	proxy.Trim(); proxy.Remove(L'"');
	if (!proxy.IsEmpty()) args += L" --proxy \"" + proxy + L"\"";
	return args;
}

bool CYtDlpGUIDlg::CheckYtDlp(CString& exe)
{
	exe = GetAppDir() + L"\\yt-dlp.exe";
	if (!FileExists(exe))
	{
		AfxMessageBox(L"找不到 yt-dlp.exe，请确认它与本程序放在同一目录下。", MB_ICONWARNING);
		return false;
	}
	return true;
}
void CYtDlpGUIDlg::OnBnClickedBrowseSave()
{
	CString cur; GetDlgItemText(IDC_EDIT_SAVEDIR, cur);
	CFolderPickerDialog dlg(cur, 0, this);
	if (dlg.DoModal() == IDOK) SetDlgItemText(IDC_EDIT_SAVEDIR, dlg.GetPathName());
}

void CYtDlpGUIDlg::OnBnClickedOpenDir()
{
	CString dir; GetDlgItemText(IDC_EDIT_SAVEDIR, dir);
	dir = TrimPath(dir);
	if (dir.IsEmpty()) return;
	::SHCreateDirectoryExW(nullptr, dir, nullptr);
	::ShellExecuteW(m_hWnd, L"open", dir, nullptr, nullptr, SW_SHOW);
}

void CYtDlpGUIDlg::OnNMDblclkResults(NMHDR* pNMHDR, LRESULT* pResult)
{
	auto* p = reinterpret_cast<LPNMITEMACTIVATE>(pNMHDR);
	*pResult = 0;
	if (p->iItem < 0) return;
	CString url = m_list.GetItemText(p->iItem, 2);
	if (url.IsEmpty()) return;
	CString cur; GetDlgItemText(IDC_EDIT_URLS, cur);
	if (!cur.IsEmpty() && cur.Right(1) != L"\n") cur += L"\r\n";
	cur += url + L"\r\n";
	SetDlgItemText(IDC_EDIT_URLS, cur);
}

void CYtDlpGUIDlg::OnBnClickedCheckAll()
{
	BOOL on = (IsDlgButtonChecked(IDC_CHECK_ALL) == BST_CHECKED);
	for (int i = 0; i < m_list.GetItemCount(); i++)
		m_list.SetCheck(i, on);
}

void CYtDlpGUIDlg::OnBnClickedSearch()
{
	if (m_running) return;
	CString q; GetDlgItemText(IDC_EDIT_SEARCH, q);
	q.Trim(); q.Remove(L'"');
	if (q.IsEmpty()) { AfxMessageBox(L"请输入搜索关键词。"); return; }
	CString exe;
	if (!CheckYtDlp(exe)) return;

	m_list.DeleteAllItems();
	CString cnt;
	if (m_cmbCount.GetCurSel() >= 0) m_cmbCount.GetLBText(m_cmbCount.GetCurSel(), cnt);
	if (cnt.IsEmpty()) cnt = L"20";
	CheckDlgButton(IDC_CHECK_ALL, BST_UNCHECKED);
	CString args = CommonArgs();
	args += L" --flat-playlist --print \"%(title)s|||%(duration_string)s|||%(url)s\" \"ytsearch" + cnt + L":" + q + L"\"";
	SetDlgItemText(IDC_STATIC_STATUS, L"搜索中...");
	StartProcess(true, exe, args);
}

void CYtDlpGUIDlg::OnBnClickedStart()
{
	if (m_running) return;

	CString text; GetDlgItemText(IDC_EDIT_URLS, text);
	std::vector<CString> urls;
	int pos = 0;
	while (pos != -1)
	{
		CString t = text.Tokenize(L"\r\n", pos);
		t.Trim(); t.Remove(L'"');
		if (!t.IsEmpty()) urls.push_back(t);
	}
	for (int i = 0; i < m_list.GetItemCount(); i++)
	{
		if (!m_list.GetCheck(i)) continue;
		CString u = m_list.GetItemText(i, 2);
		u.Trim();
		if (u.IsEmpty()) continue;
		bool dup = false;
		for (const auto& x : urls) { if (x == u) { dup = true; break; } }
		if (!dup) urls.push_back(u);
	}
	if (urls.empty()) { AfxMessageBox(L"请先输入视频链接，或在搜索结果中勾选要下载的视频。"); return; }
	{
		CString cnt;
		cnt.Format(L"共 %d 个视频待下载", (int)urls.size());
		AppendLog(cnt);
	}

	CString exe;
	if (!CheckYtDlp(exe)) return;

	CString saveDir; GetDlgItemText(IDC_EDIT_SAVEDIR, saveDir);
	saveDir = TrimPath(saveDir);
	if (saveDir.IsEmpty()) { AfxMessageBox(L"请选择保存目录。"); return; }
	::SHCreateDirectoryExW(nullptr, saveDir, nullptr);

	// ffmpeg.exe 与本程序放在同一目录；不存在时退回到系统 PATH
	CString ffmpeg = GetAppDir();
	if (!FileExists(ffmpeg + L"\\ffmpeg.exe")) ffmpeg.Empty();

	CString args = CommonArgs();
	if (!ffmpeg.IsEmpty()) args += L" --ffmpeg-location \"" + ffmpeg + L"\"";
	args += FormatArgs(m_cmbFormat.GetCurSel());
	if (IsDlgButtonChecked(IDC_CHECK_NOPLAYLIST) == BST_CHECKED) args += L" --no-playlist";

	int ck = m_cmbCookies.GetCurSel();
	if (ck > 0)
	{
		CString b; m_cmbCookies.GetLBText(ck, b);
		args += L" --cookies-from-browser " + b;
	}
	args += L" -o \"" + saveDir + L"\\%(title)s.%(ext)s\"";
	for (const auto& u : urls) args += L" \"" + u + L"\"";

	SaveSettings();
	m_progress.SetPos(0);
	SetDlgItemText(IDC_STATIC_STATUS, L"下载中...");
	StartProcess(false, exe, args);
}

void CYtDlpGUIDlg::OnBnClickedStop()
{
	if (!m_running) return;
	m_stopRequested = true;
	if (m_hJob) ::TerminateJobObject(m_hJob, 1);
	SetDlgItemText(IDC_STATIC_STATUS, L"正在停止...");
}

void CYtDlpGUIDlg::StartProcess(bool isSearch, const CString& exe, const CString& args)
{
	m_hJob = ::CreateJobObjectW(nullptr, nullptr);
	if (m_hJob)
	{
		JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli = {};
		jeli.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
		::SetInformationJobObject(m_hJob, JobObjectExtendedLimitInformation, &jeli, sizeof(jeli));
	}

	m_stopRequested = false;
	m_isSearch = isSearch;
	m_running = true;
	UpdateButtons();
	AppendLog(L"> " + exe + L" " + args);

	RunJob* job = new RunJob();
	job->exe = exe;
	job->args = args;
	job->isSearch = isSearch;
	job->hwnd = m_hWnd;
	job->hJob = m_hJob;

	m_pThread = AfxBeginThread(RunThreadProc, job, THREAD_PRIORITY_NORMAL, 0, CREATE_SUSPENDED);
	m_pThread->m_bAutoDelete = FALSE;
	m_pThread->ResumeThread();
}

LRESULT CYtDlpGUIDlg::OnAppLog(WPARAM wParam, LPARAM lParam)
{
	std::unique_ptr<CString> p(reinterpret_cast<CString*>(wParam));
	const CString& line = *p;

	if (lParam == 1)
	{
		int a = line.Find(L"|||");
		int b = (a >= 0) ? line.Find(L"|||", a + 3) : -1;
		if (a >= 0 && b >= 0)
		{
			int i = m_list.InsertItem(m_list.GetItemCount(), line.Left(a));
			m_list.SetItemText(i, 1, line.Mid(a + 3, b - a - 3));
			m_list.SetItemText(i, 2, line.Mid(b + 3));
			return 0;
		}
	}
	AppendLog(line);
	if (lParam == 0) ParseProgress(line);
	return 0;
}

LRESULT CYtDlpGUIDlg::OnAppDone(WPARAM wParam, LPARAM lParam)
{
	if (m_pThread)
	{
		::WaitForSingleObject(m_pThread->m_hThread, 5000);
		delete m_pThread;
		m_pThread = nullptr;
	}
	if (m_hJob) { ::CloseHandle(m_hJob); m_hJob = nullptr; }
	m_running = false;
	UpdateButtons();

	DWORD code = (DWORD)wParam;
	CString st;
	if (m_stopRequested)
		st = L"已停止";
	else if (code == 0)
	{
		if (lParam == 1) st.Format(L"搜索完成，共 %d 条结果", m_list.GetItemCount());
		else { st = L"下载完成"; m_progress.SetPos(100); }
	}
	else
		st.Format(L"失败（退出码 %d），请查看下方日志", (int)code);

	SetDlgItemText(IDC_STATIC_STATUS, st);
	AppendLog(L"=== " + st + L" ===");
	return 0;
}
