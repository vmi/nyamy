//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// target.cpp


#include "misc.h"

#include "mayurc.h"
#include "target.h"
#include "windowstool.h"

#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")


/// window class of the highlight overlay, registered by Register_target()
static const wchar_t c_highlightClassName[] = L"mayuTargetHighlight";


/** Bounds of the frame the user actually sees.

    GetWindowRect() includes the invisible resize border - 8px at the left,
    right and bottom of a standard frame - so two windows placed side by
    side overlap along that seam, and whichever is higher in the z order
    claims both.  The DWM reports the drawn frame instead.

    This is only usable because nyamy is DPI aware: the DWM attributes have
    always been in physical pixels, and USER32 now is too.  See
    doc/dpi-aware.md.

    Child windows have no invisible border, and the DWM does not answer for
    them, so they keep GetWindowRect().  Windows the DWM has nothing to say
    about fall back to it as well. */
static bool getVisibleWindowRect(HWND i_hwnd, RECT *o_rc)
{
	if (GetAncestor(i_hwnd, GA_ROOT) == i_hwnd &&
			SUCCEEDED(DwmGetWindowAttribute(i_hwnd,
											DWMWA_EXTENDED_FRAME_BOUNDS,
											o_rc, sizeof(*o_rc))))
		return true;
	return !!GetWindowRect(i_hwnd, o_rc);
}


/** The frame drawn over the window the cursor is on.

    This used to invert the target's own pixels with an XOR pen through
    GetWindowDC().  Under the DWM that fails in three ways: the bits fetched
    for the XOR are not necessarily the ones on screen, so the result is not
    the inverse of what the user sees; the frame is erased whenever the target
    repaints; and the erase pass - a second XOR over the same rectangle - then
    leaves a fresh frame behind.  Drawing into a window of our own has none of
    that, and lets the colour be chosen rather than derived.

    It is not derived on purpose.  Complementing the target's colour is nearly
    the same operation as inverting it and fails on the same input: mid grey
    complements to mid grey.  Three bands are drawn instead - white outside,
    black inside, an accent between - because a white edge and a black edge
    cannot both lose contrast against one background.  For the same reason the
    frame is opaque: alpha blending moves it towards the colour behind it,
    which is the opposite of what it is for. */
class Highlight
{
	HWND m_hwnd;					/// overlay window, NULL while hidden
	UINT m_dpi;					/// DPI of the monitor the target is on

	/// interior colour, keyed out by SetLayeredWindowAttributes.  All it has
	/// to be is none of the three band colours.
	static constexpr COLORREF c_transparent = RGB(0, 255, 0);
	/// outermost band; what carries the frame against a dark background
	static constexpr COLORREF c_outer = RGB(255, 255, 255);
	/// innermost band; what carries it against a light background
	static constexpr COLORREF c_inner = RGB(0, 0, 0);
	/// between the two, so the frame reads as a tool and not as a UI element
	static constexpr COLORREF c_accent = RGB(255, 0, 128);

	/// band widths at 96 dpi, outermost first
	static constexpr int c_outerPx = 1;
	static constexpr int c_accentPx = 2;
	static constexpr int c_innerPx = 1;

	/// fill a band i_width wide along the edge of io_rc, then inset io_rc
	/// past it.  A rectangle too small to inset is simply covered.
	static void drawBand(HDC i_hdc, RECT *io_rc, COLORREF i_color,
						 int i_width) {
		if (i_width <= 0 || rcWidth(io_rc) <= 0 || rcHeight(io_rc) <= 0)
			return;
		HBRUSH brush = CreateSolidBrush(i_color);
		ASSERT(brush);
		RECT edge = *io_rc;
		edge.bottom = edge.top + i_width;
		FillRect(i_hdc, &edge, brush);
		edge = *io_rc;
		edge.top = edge.bottom - i_width;
		FillRect(i_hdc, &edge, brush);
		RECT sides = *io_rc;
		sides.top += i_width;
		sides.bottom -= i_width;
		edge = sides;
		edge.right = edge.left + i_width;
		FillRect(i_hdc, &edge, brush);
		edge = sides;
		edge.left = edge.right - i_width;
		FillRect(i_hdc, &edge, brush);
		CHECK_TRUE( DeleteObject(brush) );
		InflateRect(io_rc, -i_width, -i_width);
	}

	/// WM_PAINT
	int wmPaint(HWND i_hwnd) {
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(i_hwnd, &ps);
		ASSERT(hdc);
		RECT rc;
		CHECK_TRUE( GetClientRect(i_hwnd, &rc) );
		drawBand(hdc, &rc, c_outer, scaleFromLogical(c_outerPx, m_dpi));
		drawBand(hdc, &rc, c_accent, scaleFromLogical(c_accentPx, m_dpi));
		drawBand(hdc, &rc, c_inner, scaleFromLogical(c_innerPx, m_dpi));
		HBRUSH brush = CreateSolidBrush(c_transparent);
		ASSERT(brush);
		FillRect(hdc, &rc, brush);
		CHECK_TRUE( DeleteObject(brush) );
		EndPaint(i_hwnd, &ps);
		return 0;
	}

public:
	///
	Highlight() : m_hwnd(NULL), m_dpi(USER_DEFAULT_SCREEN_DPI) {
	}

	///
	~Highlight() {
		hide();
	}

	/// the overlay itself, so that the search can leave it out
	HWND hwnd() const {
		return m_hwnd;
	}

	/// put the frame around i_target, creating the overlay if need be
	void show(HWND i_target) {
		if (!m_hwnd) {
			// WS_EX_LAYERED so that the DWM composites the overlay instead of
			// making everything under it repaint as it moves;
			// WS_EX_TRANSPARENT and WS_EX_NOACTIVATE so that it never takes
			// input even if the picker loses its mouse capture.
			m_hwnd = CreateWindowEx(WS_EX_LAYERED | WS_EX_TRANSPARENT |
									WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW |
									WS_EX_TOPMOST,
									c_highlightClassName, NULL, WS_POPUP,
									0, 0, 0, 0, NULL, NULL, g_hInst, NULL);
			if (!m_hwnd)
				return;
			setUserData(m_hwnd, this);
			// A colour key rather than SetWindowRgn: the interior is left
			// untouched either way, and a key needs nothing rebuilt when the
			// overlay is resized onto the next target.
			CHECK_TRUE( SetLayeredWindowAttributes(m_hwnd, c_transparent, 0,
												   LWA_COLORKEY) );
		}

		RECT rc;
		if (!getVisibleWindowRect(i_target, &rc))
			return;
		// the target's monitor, not ours: the two differ while the cursor is
		// still on the monitor the picker dialog is on
		m_dpi = dpiForWindowMonitor(i_target);
		// An elevated window that is also topmost stays above this and hides
		// the frame completely, the bands being drawn along the target's own
		// edges.  Neither HWND_TOPMOST nor reasserting the z order on a timer
		// wins it back, so nothing tries to; such a window cannot be
		// investigated at all either, UIPI blocking the notification that
		// would report it.  Documented as a limitation in the manual.
		CHECK_TRUE( SetWindowPos(m_hwnd, HWND_TOPMOST, rc.left, rc.top,
								 rcWidth(&rc), rcHeight(&rc),
								 SWP_NOACTIVATE | SWP_NOOWNERZORDER |
								 SWP_SHOWWINDOW) );
		// the band widths follow m_dpi, so a move alone is not enough
		CHECK_TRUE( InvalidateRect(m_hwnd, NULL, TRUE) );
		CHECK_TRUE( UpdateWindow(m_hwnd) );
	}

	/// take the frame away
	void hide() {
		if (m_hwnd) {
			CHECK_TRUE( DestroyWindow(m_hwnd) );
			m_hwnd = NULL;
		}
	}

	///
	static LRESULT CALLBACK WndProc(HWND i_hwnd, UINT i_message,
									WPARAM i_wParam, LPARAM i_lParam) {
		Highlight *wc;
		// NULL until show() has stored it, which is after CreateWindowEx()
		// returns; the window is created hidden, so nothing is painted before
		if (getUserData(i_hwnd, &wc) && i_message == WM_PAINT)
			return wc->wmPaint(i_hwnd);
		return DefWindowProc(i_hwnd, i_message, i_wParam, i_lParam);
	}
};


///
class Target
{
	HWND m_hwnd;					///
	HWND m_preHwnd;				///
	HICON m_hCursor;				///
	Highlight m_highlight;			///

	///
	Target(HWND i_hwnd)
			: m_hwnd(i_hwnd),
			m_preHwnd(NULL),
			m_hCursor(NULL) {
	}

	/// WM_CREATE
	int wmCreate(CREATESTRUCT * /* i_cs */) {
		CHECK_TRUE( m_hCursor =
						LoadCursor(g_hInst, MAKEINTRESOURCE(IDC_CURSOR_target)) );
		return 0;
	}

	/// WM_PAINT
	int wmPaint() {
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(m_hwnd, &ps);
		ASSERT(hdc);

		if (GetCapture() != m_hwnd) {
			RECT rc;
			CHECK_TRUE( GetClientRect(m_hwnd, &rc) );
			// DrawIconEx rather than DrawIcon: the cursor resource is a fixed
			// 32x32, and the control it is centred in grows with the DPI, so
			// the icon has to be stretched to the size the DPI calls for
			UINT dpi = GetDpiForWindow(m_hwnd);
			int cx = GetSystemMetricsForDpi(SM_CXICON, dpi);
			int cy = GetSystemMetricsForDpi(SM_CYICON, dpi);
			CHECK_TRUE(
				DrawIconEx(hdc, (rcWidth(&rc) - cx) / 2,
						   (rcHeight(&rc) - cy) / 2,
						   m_hCursor, cx, cy, 0, NULL, DI_NORMAL) );
		}

		EndPaint(m_hwnd, &ps);
		return 0;
	}

	///
	struct PointWindow {
		POINT m_p;					///
		HWND m_hwnd;				///
		RECT m_rc;					///
		DWORD m_processId;			/// owner of the toplevel window
		HWND m_exclude;				/// the highlight overlay, or NULL
	};

	/// a cloaked window is still visible to IsWindowVisible() but is not drawn
	/// (suspended store apps, windows on another virtual desktop)
	static bool isCloaked(HWND i_hwnd) {
		DWORD cloaked = 0;
		return SUCCEEDED(DwmGetWindowAttribute(i_hwnd, DWMWA_CLOAKED, &cloaked,
											   sizeof(cloaked))) &&
			   cloaked != 0;
	}

	///
	static bool isOwnedBy(HWND i_hwnd, DWORD i_processId) {
		DWORD processId = 0;
		GetWindowThreadProcessId(i_hwnd, &processId);
		return processId == i_processId;
	}

	/// Chromium based applications (Edge, VSCode, WebView2) parent an
	/// "Intermediate D3D Window" owned by their sandboxed GPU process into the
	/// application window, covering the whole client area.  The hook DLL cannot
	/// be loaded into that process, so a target notification posted there is
	/// never answered and the log stays silent.  Descend only into windows the
	/// toplevel window's own process owns.
	static BOOL CALLBACK childWindowFromPoint(HWND i_hwnd, LPARAM i_lParam) {
		PointWindow &pw = *(PointWindow *)i_lParam;
		if (IsWindowVisible(i_hwnd) && isOwnedBy(i_hwnd, pw.m_processId)) {
			RECT rc;
			CHECK_TRUE( GetWindowRect(i_hwnd, &rc) );
			if (PtInRect(&rc, pw.m_p))
				if (isRectInRect(&rc, &pw.m_rc)) {
					pw.m_hwnd = i_hwnd;
					pw.m_rc = rc;
				}
		}
		return TRUE;
	}

	///
	static BOOL CALLBACK windowFromPoint(HWND i_hwnd, LPARAM i_lParam) {
		PointWindow &pw = *(PointWindow *)i_lParam;
		// The highlight sits on top of the very window it is pointing out, so
		// it would be picked in its place.  WS_EX_TRANSPARENT keeps it out of
		// hit testing but not out of EnumWindows().
		if (i_hwnd != pw.m_exclude &&
				IsWindowVisible(i_hwnd) && !isCloaked(i_hwnd)) {
			RECT rcVisible;
			CHECK_TRUE( getVisibleWindowRect(i_hwnd, &rcVisible) );
			if (PtInRect(&rcVisible, pw.m_p)) {
				pw.m_hwnd = i_hwnd;
				// The descent below compares child windows against this with
				// GetWindowRect(), so that is what it has to hold: a child
				// filling the window would not be contained in the smaller
				// visible frame, and the search would stop at the toplevel.
				CHECK_TRUE( GetWindowRect(i_hwnd, &pw.m_rc) );
				return FALSE;
			}
		}
		return TRUE;
	}

	/// WM_MOUSEMOVE
	int wmMouseMove(WORD /* i_keys */, int /* i_x */, int /* i_y */) {
		if (GetCapture() == m_hwnd) {
			PointWindow pw;
			CHECK_TRUE( GetCursorPos(&pw.m_p) );
			pw.m_hwnd = NULL;
			pw.m_processId = 0;
			pw.m_exclude = m_highlight.hwnd();
			CHECK_TRUE( GetWindowRect(GetDesktopWindow(), &pw.m_rc) );
			EnumWindows(windowFromPoint, (LPARAM)&pw);
			// without a toplevel window there is nothing to descend into.
			// EnumChildWindows(NULL) would enumerate every toplevel window
			// instead, which the rect test is not written for.
			if (pw.m_hwnd) {
				CHECK_TRUE( GetWindowThreadProcessId(pw.m_hwnd,
													 &pw.m_processId) );
				while (1) {
					HWND hwndParent = pw.m_hwnd;
					EnumChildWindows(pw.m_hwnd, childWindowFromPoint,
									 (LPARAM)&pw);
					if (hwndParent == pw.m_hwnd)
						break;
				}
			}
			if (pw.m_hwnd != m_preHwnd) {
				m_preHwnd = pw.m_hwnd;
				if (m_preHwnd) {
					m_highlight.show(m_preHwnd);
					SendMessage(GetParent(m_hwnd), WM_APP_targetNotify, 0,
								(LPARAM)m_preHwnd);
				} else {
					m_highlight.hide();
				}
			}
			SetCursor(m_hCursor);
		}
		return 0;
	}

	/// WM_LBUTTONDOWN
	int wmLButtonDown(WORD /* i_keys */, int /* i_x */, int /* i_y */) {
		SetCapture(m_hwnd);
		SetCursor(m_hCursor);
		CHECK_TRUE( InvalidateRect(m_hwnd, NULL, TRUE) );
		CHECK_TRUE( UpdateWindow(m_hwnd) );
		return 0;
	}

	/// WM_LBUTTONUP
	int wmLButtonUp(WORD /* i_keys */, int /* i_x */, int /* i_y */) {
		m_highlight.hide();
		m_preHwnd = NULL;
		ReleaseCapture();
		CHECK_TRUE( InvalidateRect(m_hwnd, NULL, TRUE) );
		CHECK_TRUE( UpdateWindow(m_hwnd) );
		return 0;
	}

public:
	///
	static LRESULT CALLBACK WndProc(HWND i_hwnd, UINT i_message,
									WPARAM i_wParam, LPARAM i_lParam) {
		Target *wc;
		getUserData(i_hwnd, &wc);
		if (!wc)
			switch (i_message) {
			case WM_CREATE:
				wc = setUserData(i_hwnd, new Target(i_hwnd));
				return wc->wmCreate((CREATESTRUCT *)i_lParam);
			}
		else
			switch (i_message) {
			case WM_PAINT:
				return wc->wmPaint();
			case WM_LBUTTONDOWN:
				return wc->wmLButtonDown((WORD)i_wParam, (short)LOWORD(i_lParam),
										 (short)HIWORD(i_lParam));
			case WM_LBUTTONUP:
				return wc->wmLButtonUp((WORD)i_wParam, (short)LOWORD(i_lParam),
									   (short)HIWORD(i_lParam));
			case WM_MOUSEMOVE:
				return wc->wmMouseMove((WORD)i_wParam, (short)LOWORD(i_lParam),
									   (short)HIWORD(i_lParam));
			case WM_NCDESTROY:
				delete wc;
				return 0;
			}
		return DefWindowProc(i_hwnd, i_message, i_wParam, i_lParam);
	}
};


//
ATOM Register_target()
{
	WNDCLASS wc;
	wc.style         = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc   = Highlight::WndProc;
	wc.cbClsExtra    = 0;
	wc.cbWndExtra    = 0;
	wc.hInstance     = g_hInst;
	wc.hIcon         = NULL;
	wc.hCursor       = NULL;
	// the overlay paints every pixel it owns, and an erase pass would show
	// through the colour key as a flash of the background
	wc.hbrBackground = NULL;
	wc.lpszMenuName  = NULL;
	wc.lpszClassName = c_highlightClassName;
	if (!RegisterClass(&wc))
		return 0;

	wc.style         = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc   = Target::WndProc;
	wc.cbClsExtra    = 0;
	wc.cbWndExtra    = 0;
	wc.hInstance     = g_hInst;
	wc.hIcon         = NULL;
	wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wc.lpszMenuName  = NULL;
	wc.lpszClassName = L"mayuTarget";
	return RegisterClass(&wc);
}
