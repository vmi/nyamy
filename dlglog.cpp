//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// dlglog.cpp


#include "misc.h"
#include "mayu.h"
#include "mayurc.h"
#include "inifile.h"
#include "windowstool.h"
#include "msgstream.h"
#include "layoutmanager.h"
#include "dlglog.h"
#include <windowsx.h>


///
class DlgLog : public LayoutManager
{
	HWND m_hwndEdit;				///
	HWND m_hwndTaskTray;				/// tasktray window
	LOGFONT m_lf;					///
	HFONT m_hfontOriginal;			///
	HFONT m_hfont;				///
	womsgstream *m_log;				///

	/** Style of the log edit control with the horizontal-scroll bits masked
	    off.  Word wrap is switched by recreating the control with or without
	    them: an edit control ignores a later change to ES_AUTOHSCROLL.
	    Copying the style the resource gave us keeps the recreated control
	    looking exactly like the original.
	*/
	DWORD m_editStyle;
	DWORD m_editExStyle;
	bool m_isWordWrap;				///

	/// the saved placement was maximized; apply it when the dialog is shown
	bool m_restoreMaximized;

public:
	///
	DlgLog(HWND i_hwnd)
			: LayoutManager(i_hwnd),
			m_hwndEdit(GetDlgItem(m_hwnd, IDC_EDIT_log)),
			m_hwndTaskTray(NULL),
			m_hfontOriginal(GetWindowFont(m_hwnd)),
			m_hfont(NULL),
			m_log(NULL),
			m_editStyle(0),
			m_editExStyle(0),
			m_isWordWrap(true),
			m_restoreMaximized(false) {
	}

	/// WM_INITDIALOG
	BOOL wmInitDialog(HWND /* i_focus */, LPARAM i_lParam) {
		DlgLogData *dld = reinterpret_cast<DlgLogData *>(i_lParam);
		m_log = dld->m_log;
		m_hwndTaskTray = dld->m_hwndTaskTray;

		IniFile ini;

		// set icons
		setSmallIcon(m_hwnd, IDI_ICON_mayu);
		setBigIcon(m_hwnd, IDI_ICON_mayu);

		// set font
		ini.read(L"logFont", &m_lf, loadString(IDS_logFont));
		m_hfont = CreateFontIndirect(&m_lf);
		SetWindowFont(m_hwndEdit, m_hfont, false);

		m_editStyle = static_cast<DWORD>(GetWindowLongPtr(m_hwndEdit, GWL_STYLE))
					  & ~static_cast<DWORD>(ES_AUTOHSCROLL | WS_HSCROLL);
		m_editExStyle =
			static_cast<DWORD>(GetWindowLongPtr(m_hwndEdit, GWL_EXSTYLE));
		Edit_LimitText(m_hwndEdit, 0);

		// set layout manager
		using LM = LayoutManager;
		addItem(GetDlgItem(m_hwnd, IDOK),
				LM::ORIGIN_RIGHT_EDGE, LM::ORIGIN_BOTTOM_EDGE,
				LM::ORIGIN_RIGHT_EDGE, LM::ORIGIN_BOTTOM_EDGE);
		addItem(m_hwndEdit,
				LM::ORIGIN_LEFT_EDGE, LM::ORIGIN_TOP_EDGE,
				LM::ORIGIN_RIGHT_EDGE, LM::ORIGIN_BOTTOM_EDGE);
		addItem(GetDlgItem(m_hwnd, IDC_BUTTON_clearLog),
				LM::ORIGIN_LEFT_EDGE, LM::ORIGIN_BOTTOM_EDGE,
				LM::ORIGIN_LEFT_EDGE, LM::ORIGIN_BOTTOM_EDGE);
		addItem(GetDlgItem(m_hwnd, IDC_BUTTON_changeFont),
				LM::ORIGIN_LEFT_EDGE, LM::ORIGIN_BOTTOM_EDGE,
				LM::ORIGIN_LEFT_EDGE, LM::ORIGIN_BOTTOM_EDGE);
		addItem(GetDlgItem(m_hwnd, IDC_CHECK_detail),
				LM::ORIGIN_LEFT_EDGE, LM::ORIGIN_BOTTOM_EDGE,
				LM::ORIGIN_LEFT_EDGE, LM::ORIGIN_BOTTOM_EDGE);
		addItem(GetDlgItem(m_hwnd, IDC_CHECK_wordWrap),
				LM::ORIGIN_LEFT_EDGE, LM::ORIGIN_BOTTOM_EDGE,
				LM::ORIGIN_LEFT_EDGE, LM::ORIGIN_BOTTOM_EDGE);
		restrictSmallestSize();

		// detail; restoring it means nyamy can start with detail already on,
		// which is why the initial level is part of the scripter's Start
		int isDetail = 0;
		ini.read(L"logDetail", &isDetail, 0);
		CheckDlgButton(m_hwnd, IDC_CHECK_detail,
					   isDetail ? BST_CHECKED : BST_UNCHECKED);
		applyThreshold(isDetail != 0);

		// word wrap; the resource control carries ES_AUTOHSCROLL, so wrapping
		// means recreating it
		int isWordWrap = 1;
		ini.read(L"logWordWrap", &isWordWrap, 1);
		m_isWordWrap = (isWordWrap != 0);
		CheckDlgButton(m_hwnd, IDC_CHECK_wordWrap,
					   m_isWordWrap ? BST_CHECKED : BST_UNCHECKED);
		if (m_isWordWrap)
			recreateEdit();

		restorePlacement(ini);
		adjust();
		return TRUE;
	}

	/// WM_DESTROY
	BOOL wmDestroy() {
		// Backstop for a geometry change that never reached WM_EXITSIZEMOVE,
		// e.g. a resize by the window manager or a snap.
		savePlacement();

		// unset font
		SetWindowFont(m_hwndEdit, m_hfontOriginal, false);
		DeleteObject(m_hfont);

		// unset icons
		unsetBigIcon(m_hwnd);
		unsetSmallIcon(m_hwnd);
		return TRUE;
	}

	/// WM_CLOSE
	BOOL wmClose() {
		savePlacement();
		ShowWindow(m_hwnd, SW_HIDE);
		return TRUE;
	}

	/// WM_EXITSIZEMOVE
	BOOL wmExitSizeMove() {
		savePlacement();
		return TRUE;
	}

	/// WM_SHOWWINDOW
	BOOL wmShowWindow(BOOL i_isShown, int /* i_status */) {
		if (i_isShown && m_restoreMaximized) {
			m_restoreMaximized = false;
			ShowWindow(m_hwnd, SW_SHOWMAXIMIZED);
		}
		return FALSE;
	}

	/// WM_COMMAND
	BOOL wmCommand(int /* i_notifyCode */, int i_id, HWND /* i_hwndControl */) {
		switch (i_id) {
		case IDOK: {
			savePlacement();
			ShowWindow(m_hwnd, SW_HIDE);
			return TRUE;
		}

		case IDC_BUTTON_clearLog: {
			Edit_SetSel(m_hwndEdit, 0, Edit_GetTextLength(m_hwndEdit));
			Edit_ReplaceSel(m_hwndEdit, L"");
			SendMessage(m_hwndTaskTray, WM_APP_dlglogNotify,
						DlgLogNotify_logCleared, 0);
			return TRUE;
		}

		case IDC_BUTTON_changeFont: {
			CHOOSEFONT cf;
			memset(&cf, 0, sizeof(cf));
			cf.lStructSize = sizeof(cf);
			cf.hwndOwner = m_hwnd;
			cf.lpLogFont = &m_lf;
			cf.Flags = CF_INITTOLOGFONTSTRUCT | CF_SCREENFONTS;
			if (ChooseFont(&cf)) {
				HFONT hfontNew = CreateFontIndirect(&m_lf);
				SetWindowFont(m_hwndEdit, hfontNew, true);
				DeleteObject(m_hfont);
				m_hfont = hfontNew;
				IniFile().write(L"logFont", m_lf);
			}
			return TRUE;
		}

		case IDC_CHECK_detail: {
			bool isChecked =
				(IsDlgButtonChecked(m_hwnd, IDC_CHECK_detail) == BST_CHECKED);
			applyThreshold(isChecked);
			IniFile().write(L"logDetail", isChecked ? 1 : 0);
			return TRUE;
		}

		case IDC_CHECK_wordWrap: {
			bool isChecked =
				(IsDlgButtonChecked(m_hwnd, IDC_CHECK_wordWrap) == BST_CHECKED);
			if (isChecked != m_isWordWrap) {
				m_isWordWrap = isChecked;
				recreateEdit();
				IniFile().write(L"logWordWrap", m_isWordWrap ? 1 : 0);
			}
			return TRUE;
		}
		}
		return FALSE;
	}

private:
	/// apply the "detail" state, and let the tasktray window forward it
	void applyThreshold(bool i_isDetail) {
		LogLevel level = i_isDetail ? kLogLevelDetail : kLogLevelNormal;
		if (m_log)
			m_log->setThreshold(level);
		if (m_hwndTaskTray)
			SendMessage(m_hwndTaskTray, WM_APP_dlglogNotify,
						DlgLogNotify_thresholdChanged,
						static_cast<LPARAM>(level));
	}

	/** Rebuild the log edit control with the current word wrap setting,
	    carrying the text over.  The text is capped well below the point where
	    moving it would be noticeable.
	*/
	void recreateEdit() {
		std::wstring text;
		int length = GetWindowTextLength(m_hwndEdit);
		if (0 < length) {
			text.resize(static_cast<size_t>(length));
			// &text[0] is contiguous and writable; the terminator goes into
			// the slot resize() reserved for it
			length = GetWindowText(m_hwndEdit, &text[0], length + 1);
			text.resize(static_cast<size_t>(length < 0 ? 0 : length));
		}

		RECT rc;
		GetWindowRect(m_hwndEdit, &rc);
		MapWindowPoints(NULL, m_hwnd, reinterpret_cast<POINT *>(&rc), 2);

		// Remember the sibling in front of it.  Tab order follows z-order, and
		// the dialog manager focuses the first control in tab order: putting
		// the recreated edit at the front would make this read-only control
		// take the initial focus instead of the Close button.  That also feeds
		// the whole log text into the window title nyamy reports for itself,
		// because the title is built from GetWindowText of the focus window.
		HWND prev = GetWindow(m_hwndEdit, GW_HWNDPREV);

		removeItem(m_hwndEdit);
		DestroyWindow(m_hwndEdit);

		DWORD style = m_editStyle;
		if (!m_isWordWrap)
			style |= ES_AUTOHSCROLL | WS_HSCROLL;
		m_hwndEdit = CreateWindowEx(
						 m_editExStyle, L"EDIT", NULL, style,
						 rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top,
						 m_hwnd, reinterpret_cast<HMENU>(IDC_EDIT_log), g_hInst, NULL);
		if (!m_hwndEdit)
			return;

		SetWindowFont(m_hwndEdit, m_hfont, false);
		Edit_LimitText(m_hwndEdit, 0);
		if (!text.empty()) {
			SetWindowText(m_hwndEdit, text.c_str());
			int end = Edit_GetTextLength(m_hwndEdit);
			Edit_SetSel(m_hwndEdit, end, end);
			Edit_ScrollCaret(m_hwndEdit);
		}

		// restore the original slot in the z-order, hence in the tab order
		SetWindowPos(m_hwndEdit, prev ? prev : HWND_TOP, 0, 0, 0, 0,
					 SWP_NOMOVE | SWP_NOSIZE);
		using LM = LayoutManager;
		addItem(m_hwndEdit,
				LM::ORIGIN_LEFT_EDGE, LM::ORIGIN_TOP_EDGE,
				LM::ORIGIN_RIGHT_EDGE, LM::ORIGIN_BOTTOM_EDGE);
	}

	/// restore the saved window placement, or pick a sensible first one
	void restorePlacement(const IniFile &i_ini) {
		WINDOWPLACEMENT wp;
		memset(&wp, 0, sizeof(wp));
		wp.length = sizeof(wp);
		// MONITOR_DEFAULTTONULL: a placement saved on a monitor that is gone
		// (or on a resolution that shrank) would put the dialog out of reach
		if (i_ini.read(L"logWindowPlacement", &wp) &&
				MonitorFromRect(&wp.rcNormalPosition, MONITOR_DEFAULTTONULL)) {
			m_restoreMaximized = (wp.showCmd == SW_SHOWMAXIMIZED);
			// the dialog is created hidden and stays that way until the user
			// opens it; SW_HIDE places the window without showing it
			wp.showCmd = SW_HIDE;
			wp.flags = 0;
			SetWindowPlacement(m_hwnd, &wp);
			clampToWorkArea();
			return;
		}

		// No usable saved placement.  The dialog resource is only tall enough
		// for the button row, so make room for the log itself - but centred on
		// the work area, not grown downwards off the bottom of the screen.
		RECT rc;
		GetWindowRect(m_hwnd, &rc);
		int width = rcWidth(&rc);
		int height = rcHeight(&rc) * 4;

		RECT rcWork;
		MONITORINFO mi;
		mi.cbSize = sizeof(mi);
		if (GetMonitorInfo(MonitorFromWindow(m_hwnd, MONITOR_DEFAULTTOPRIMARY),
						   &mi))
			rcWork = mi.rcWork;
		else if (!SystemParametersInfo(SPI_GETWORKAREA, 0, &rcWork, 0))
			return;

		if (rcWidth(&rcWork) < width)
			width = rcWidth(&rcWork);
		if (rcHeight(&rcWork) < height)
			height = rcHeight(&rcWork);
		MoveWindow(m_hwnd,
				   rcWork.left + (rcWidth(&rcWork) - width) / 2,
				   rcWork.top + (rcHeight(&rcWork) - height) / 2,
				   width, height, FALSE);
		clampToWorkArea();
	}

	/** Pull the window back inside the work area of the monitor it sits on.

	    Applied after restoring, because a saved rectangle can be stale: the
	    monitor arrangement may have changed, and older builds parked this
	    window below the investigate dialog and saved it from there, which put
	    its lower edge off the screen.  Works in screen coordinates on the real
	    window rather than on WINDOWPLACEMENT, whose rcNormalPosition is in
	    workspace coordinates and would need converting first.
	*/
	void clampToWorkArea() {
		RECT rc;
		if (!GetWindowRect(m_hwnd, &rc))
			return;
		MONITORINFO mi;
		mi.cbSize = sizeof(mi);
		if (!GetMonitorInfo(MonitorFromRect(&rc, MONITOR_DEFAULTTONEAREST), &mi))
			return;

		int width = rcWidth(&rc);
		int height = rcHeight(&rc);
		if (rcWidth(&mi.rcWork) < width)
			width = rcWidth(&mi.rcWork);
		if (rcHeight(&mi.rcWork) < height)
			height = rcHeight(&mi.rcWork);

		int x = rc.left;
		int y = rc.top;
		if (mi.rcWork.right < x + width)
			x = mi.rcWork.right - width;
		if (mi.rcWork.bottom < y + height)
			y = mi.rcWork.bottom - height;
		if (x < mi.rcWork.left)
			x = mi.rcWork.left;
		if (y < mi.rcWork.top)
			y = mi.rcWork.top;

		if (x == rc.left && y == rc.top &&
				width == rcWidth(&rc) && height == rcHeight(&rc))
			return;
		SetWindowPos(m_hwnd, NULL, x, y, width, height,
					 SWP_NOZORDER | SWP_NOACTIVATE);
	}

	///
	void savePlacement() {
		WINDOWPLACEMENT wp;
		memset(&wp, 0, sizeof(wp));
		wp.length = sizeof(wp);
		if (!GetWindowPlacement(m_hwnd, &wp))
			return;
		// while the dialog is closed this reports SW_HIDE, and a minimized
		// state would restore to a window the user cannot see
		wp.showCmd = (wp.showCmd == SW_SHOWMAXIMIZED) ? SW_SHOWMAXIMIZED
					 : SW_SHOWNORMAL;
		IniFile().write(L"logWindowPlacement", wp);
	}
};


//
INT_PTR CALLBACK dlgLog_dlgProc(HWND i_hwnd, UINT i_message,
								WPARAM i_wParam, LPARAM i_lParam)
{
	DlgLog *wc;
	getUserData(i_hwnd, &wc);
	if (!wc)
		switch (i_message) {
		case WM_INITDIALOG:
			wc = setUserData(i_hwnd, new DlgLog(i_hwnd));
			return wc->wmInitDialog(reinterpret_cast<HWND>(i_wParam), i_lParam);
		}
	else
		switch (i_message) {
		case WM_COMMAND:
			return wc->wmCommand(HIWORD(i_wParam), LOWORD(i_wParam),
								 reinterpret_cast<HWND>(i_lParam));
		case WM_CLOSE:
			return wc->wmClose();
		case WM_EXITSIZEMOVE:
			return wc->wmExitSizeMove();
		case WM_SHOWWINDOW:
			return wc->wmShowWindow(static_cast<BOOL>(i_wParam),
									static_cast<int>(i_lParam));
		case WM_DESTROY:
			return wc->wmDestroy();
		case WM_NCDESTROY:
			delete wc;
			return TRUE;
		default:
			return wc->defaultWMHandler(i_message, i_wParam, i_lParam);
		}
	return FALSE;
}
