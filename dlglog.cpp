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
	/** Log font, with lfHeight held at 96 dpi.

	    That is the form the ini stores, so a settings file written before
	    nyamy became DPI aware still means what it says, and one font setting
	    describes the same apparent size on every monitor.  It is scaled to the
	    window's DPI only on the way into GDI; see makeFont(). */
	LOGFONT m_lf;
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

	/** Where the log was being read when a DPI change started; see
	    onDpiChangeBegin().  m_dpiAnchor is a character index, or -1 for none. */
	bool m_dpiWasAtBottom;
	int m_dpiAnchor;

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
			m_restoreMaximized(false),
			m_dpiWasAtBottom(false),
			m_dpiAnchor(-1) {
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
		m_hfont = makeFont();
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

	/** WM_DPICHANGED

	    The base class rescales the layout snapshots; the log font is ours
	    alone, built from a 96 dpi height, so it has to be rebuilt for the DPI
	    the dialog has moved to.  The dialog manager rescales the fonts it set
	    itself, but not one handed to a control through WM_SETFONT. */
	/** Note where the log is being read, before anything is rescaled.

	    A taller font fits fewer lines in the same control, and the control
	    keeps its scroll offset in lines, so the view slides on every DPI
	    change and walks away over a few of them.

	    The anchor is a character index, not a line number.  These messages
	    count wrapped lines, not logical ones, and both the font change and the
	    resize re-wrap the text: line 40 before and line 40 after are different
	    places in the log, and with word wrap on, the top line is usually the
	    middle of a longer one.  A character index means the same thing however
	    often the text is re-wrapped in between - which is the reason this is
	    read here rather than in wmDpiChanged: by then the dialog manager has
	    already rescaled the font and the view has already moved. */
	void onDpiChangeBegin() override {
		m_dpiWasAtBottom = isAtBottom();
		m_dpiAnchor = -1;
		int wasFirst = static_cast<int>(
						   SendMessage(m_hwndEdit, EM_GETFIRSTVISIBLELINE,
									   0, 0));
		if (0 <= wasFirst)
			m_dpiAnchor = static_cast<int>(
							  SendMessage(m_hwndEdit, EM_LINEINDEX,
										  static_cast<WPARAM>(wasFirst), 0));
	}

	/** Put the log's own font back after the base class has laid the dialog out,
	    and return the view to where onDpiChangeBegin() found it.

	    The dialog manager scales the font of every control, this one included,
	    by the ratio the window happened to change by.  m_lf is exact - a 96 dpi
	    height that converts to the new DPI without drift however many monitors
	    the dialog crosses - so the scaled version is replaced rather than
	    kept. */
	BOOL wmDpiChanged(UINT i_dpi, const RECT *i_suggested) override {
		BOOL result = LayoutManager::wmDpiChanged(i_dpi, i_suggested);

		HFONT hfontNew = makeFontForDpi(i_dpi);
		if (hfontNew) {
			SetWindowFont(m_hwndEdit, hfontNew, true);
			DeleteObject(m_hfont);
			m_hfont = hfontNew;
		}

		if (m_dpiWasAtBottom) {
			// The log follows its own tail, and a taller font fits fewer
			// lines: anchoring the top line here would push the newest ones
			// out of sight until the next line arrived.  Scrolling past the
			// end clamps.
			SendMessage(m_hwndEdit, EM_LINESCROLL, 0,
						static_cast<LPARAM>(Edit_GetLineCount(m_hwndEdit)));
		} else if (0 <= m_dpiAnchor) {
			int wantLine = static_cast<int>(
							   SendMessage(m_hwndEdit, EM_LINEFROMCHAR,
										   static_cast<WPARAM>(m_dpiAnchor), 0));
			int nowFirst = static_cast<int>(
							   SendMessage(m_hwndEdit, EM_GETFIRSTVISIBLELINE,
										   0, 0));
			if (0 <= wantLine && 0 <= nowFirst && wantLine != nowFirst)
				SendMessage(m_hwndEdit, EM_LINESCROLL, 0,
							static_cast<LPARAM>(wantLine - nowFirst));
		}
		m_dpiAnchor = -1;
		return result;
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
			// The dialog works in device pixels, so hand it a scaled copy and
			// convert the answer back: m_lf stays the 96 dpi original.
			UINT dpi = GetDpiForWindow(m_hwnd);
			LOGFONT lf = m_lf;
			lf.lfHeight = scaleFromLogical(m_lf.lfHeight, dpi);

			CHOOSEFONT cf;
			memset(&cf, 0, sizeof(cf));
			cf.lStructSize = sizeof(cf);
			cf.hwndOwner = m_hwnd;
			cf.lpLogFont = &lf;
			cf.Flags = CF_INITTOLOGFONTSTRUCT | CF_SCREENFONTS;
			if (ChooseFont(&cf)) {
				m_lf = lf;
				m_lf.lfHeight = MulDiv(lf.lfHeight, USER_DEFAULT_SCREEN_DPI,
									   static_cast<int>(dpi));
				HFONT hfontNew = makeFont();
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
	/** Is the end of the log in view?

	    Asked of the scroll bar rather than worked out from the font metrics and
	    the formatting rectangle: the control already tracks this, in the same
	    wrapped lines the scrolling messages count, and reading it moves
	    nothing. */
	bool isAtBottom() const {
		SCROLLINFO si;
		memset(&si, 0, sizeof(si));
		si.cbSize = sizeof(si);
		si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
		if (!GetScrollInfo(m_hwndEdit, SB_VERT, &si))
			return false;
		if (si.nPage == 0)		// everything fits; nothing to scroll past
			return true;
		return si.nMax <= si.nPos + static_cast<int>(si.nPage) - 1;
	}

	/** Build the log font for a given DPI.

	    m_lf.lfHeight is a 96 dpi length; GDI wants device pixels.  The DPI is
	    a parameter because WM_DPICHANGED has to build the font for the DPI
	    being moved to, which GetDpiForWindow does not report yet. */
	HFONT makeFontForDpi(UINT i_dpi) const {
		LOGFONT lf = m_lf;
		lf.lfHeight = scaleFromLogical(m_lf.lfHeight, i_dpi);
		return CreateFontIndirect(&lf);
	}

	/// Build the log font for the DPI the dialog is on now.
	HFONT makeFont() const {
		return makeFontForDpi(GetDpiForWindow(m_hwnd));
	}

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
		//
		// The key is not the one nyamy used before it became DPI aware, whose
		// values were written in the virtualized 96 dpi space.  That one is
		// still read as a fallback, once: the two spaces only differ for a
		// window left on a monitor whose scaling is not the system's, and a
		// rectangle that does come out wrong is pulled back into view by
		// clampToWorkArea() below.  Carrying the position over the upgrade is
		// worth that.  savePlacement() drops the old key, so the fallback
		// stops applying as soon as the dialog has been closed once.
		bool hasSaved = i_ini.read(L"logWindowPlacementPhysical", &wp);
		if (!hasSaved)
			hasSaved = i_ini.read(L"logWindowPlacement", &wp);

		if (hasSaved &&
				MonitorFromRect(&wp.rcNormalPosition, MONITOR_DEFAULTTONULL)) {
			m_restoreMaximized = (wp.showCmd == SW_SHOWMAXIMIZED);
			// the dialog is created hidden and stays that way until the user
			// opens it; SW_HIDE places the window without showing it
			wp.showCmd = SW_HIDE;
			wp.flags = 0;
			RECT rcWanted = wp.rcNormalPosition;
			SetWindowPlacement(m_hwnd, &wp);

			// The dialog is created on whichever monitor the template lands
			// on.  If the saved placement is on one with another DPI, arriving
			// there raises WM_DPICHANGED and the dialog manager rescales the
			// window on top of the size just asked for - so the restored
			// window comes out a factor larger than it was saved, and saving
			// it again on exit compounds that every run.  The window is on the
			// target monitor by now, so putting the size back cannot cross a
			// DPI boundary a second time.
			RECT rcNow;
			if (GetWindowRect(m_hwnd, &rcNow) &&
					(rcWidth(&rcNow) != rcWidth(&rcWanted) ||
					 rcHeight(&rcNow) != rcHeight(&rcWanted)))
				SetWindowPos(m_hwnd, NULL, 0, 0,
							 rcWidth(&rcWanted), rcHeight(&rcWanted),
							 SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);

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
		IniFile ini;
		ini.write(L"logWindowPlacementPhysical", wp);
		// The value just written supersedes the one the DPI unaware builds
		// wrote, which restorePlacement() falls back to.  Drop it rather than
		// leave a stale rectangle in the file for that fallback to find.
		ini.remove(L"logWindowPlacement");
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
