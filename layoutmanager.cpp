//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// layoutmanager.cpp


#include "layoutmanager.h"
#include "windowstool.h"

#include <windowsx.h>


//
LayoutManager::LayoutManager(HWND i_hwnd)
		: m_hwnd(i_hwnd),
		m_smallestRestriction(RESTRICT_NONE),
		m_largestRestriction(RESTRICT_NONE),
		m_dpi(GetDpiForWindow(i_hwnd)),
		m_isDpiChanging(false)
{
}


//
LayoutManager::~LayoutManager()
{
}

// restrict the smallest size of the window to the current size of it or
// specified by i_size
void LayoutManager::restrictSmallestSize(Restrict i_restrict, SIZE *i_size)
{
	m_smallestRestriction = i_restrict;
	if (i_size)
		m_smallestSize = *i_size;
	else {
		RECT rc;
		GetWindowRect(m_hwnd, &rc);
		m_smallestSize.cx = rc.right - rc.left;
		m_smallestSize.cy = rc.bottom - rc.top;
	}
}


// restrict the largest size of the window to the current size of it or
// specified by i_size
void LayoutManager::restrictLargestSize(Restrict i_restrict, SIZE *i_size)
{
	m_largestRestriction = i_restrict;
	if (i_size)
		m_largestSize = *i_size;
	else {
		RECT rc;
		GetWindowRect(m_hwnd, &rc);
		m_largestSize.cx = rc.right - rc.left;
		m_largestSize.cy = rc.bottom - rc.top;
	}
}

//
bool LayoutManager::addItem(HWND i_hwnd, Origin i_originLeft,
							Origin i_originTop,
							Origin i_originRight, Origin i_originBottom)
{
	Item item;
	if (!i_hwnd)
		return false;
	item.m_hwnd = i_hwnd;
	if (!(GetWindowLongPtr(i_hwnd, GWL_STYLE) & WS_CHILD))
		return false;
	item.m_hwndParent = GetParent(i_hwnd);
	if (!item.m_hwndParent)
		return false;
	getChildWindowRect(item.m_hwnd, &item.m_rc);
	GetWindowRect(item.m_hwndParent, &item.m_rcParent);
	item.m_origin[0] = i_originLeft;
	item.m_origin[1] = i_originTop;
	item.m_origin[2] = i_originRight;
	item.m_origin[3] = i_originBottom;

	m_items.push_back(item);
	return true;
}

//
bool LayoutManager::removeItem(HWND i_hwnd)
{
	for (Items::iterator i = m_items.begin(); i != m_items.end(); ++ i)
		if (i->m_hwnd == i_hwnd) {
			m_items.erase(i);
			return true;
		}
	return false;
}

//
void LayoutManager::adjust() const
{
	for (Items::const_iterator i = m_items.begin(); i != m_items.end(); ++ i) {
		RECT rc;
		GetWindowRect(i->m_hwndParent, &rc);

		struct {
			int m_width, m_pos;
			int m_curWidth;
			LONG *m_out;
		}
		pos[4] = {
			{ rcWidth(&i->m_rcParent), i->m_rc.left, rcWidth(&rc), &rc.left },
			{ rcHeight(&i->m_rcParent), i->m_rc.top, rcHeight(&rc), &rc.top },
			{ rcWidth(&i->m_rcParent), i->m_rc.right, rcWidth(&rc), &rc.right },
			{ rcHeight(&i->m_rcParent), i->m_rc.bottom, rcHeight(&rc), &rc.bottom }
		};
		for (int j = 0; j < 4; ++ j) {
			switch (i->m_origin[j]) {
			case ORIGIN_LEFT_EDGE:
				*pos[j].m_out = pos[j].m_pos;
				break;
			case ORIGIN_CENTER:
				*pos[j].m_out = pos[j].m_curWidth / 2
								- (pos[j].m_width / 2 - pos[j].m_pos);
				break;
			case ORIGIN_RIGHT_EDGE:
				*pos[j].m_out = pos[j].m_curWidth
								- (pos[j].m_width - pos[j].m_pos);
				break;
			}
		}
		MoveWindow(i->m_hwnd, rc.left, rc.top,
				   rcWidth(&rc), rcHeight(&rc), FALSE);
	}
}


// client rect of the size box, for the window's current DPI
RECT LayoutManager::sizeGripRect() const
{
	RECT rc;
	GetClientRect(m_hwnd, &rc);
	UINT dpi = GetDpiForWindow(m_hwnd);
	rc.left = rc.right - GetSystemMetricsForDpi(SM_CXHTHUMB, dpi);
	rc.top = rc.bottom - GetSystemMetricsForDpi(SM_CYVTHUMB, dpi);
	return rc;
}


// draw size box
BOOL LayoutManager::wmPaint()
{
	PAINTSTRUCT ps;
	HDC hdc = BeginPaint(m_hwnd, &ps);
	RECT rc = sizeGripRect();
	DrawFrameControl(hdc, &rc, DFC_SCROLL, DFCS_SCROLLSIZEGRIP);
	EndPaint(m_hwnd, &ps);
	return TRUE;
}


// size restriction
BOOL LayoutManager::wmSizing(int i_edge, RECT *io_rc)
{
	switch (i_edge) {
	case WMSZ_TOPLEFT:
	case WMSZ_LEFT:
	case WMSZ_BOTTOMLEFT:
		if (m_smallestRestriction & RESTRICT_HORIZONTALLY)
			if (io_rc->right - io_rc->left < m_smallestSize.cx)
				io_rc->left = io_rc->right - m_smallestSize.cx;
		if (m_largestRestriction & RESTRICT_HORIZONTALLY)
			if (m_largestSize.cx < io_rc->right - io_rc->left)
				io_rc->left = io_rc->right - m_largestSize.cx;
		break;
	}
	switch (i_edge) {
	case WMSZ_TOPRIGHT:
	case WMSZ_RIGHT:
	case WMSZ_BOTTOMRIGHT:
		if (m_smallestRestriction & RESTRICT_HORIZONTALLY)
			if (io_rc->right - io_rc->left < m_smallestSize.cx)
				io_rc->right = io_rc->left + m_smallestSize.cx;
		if (m_largestRestriction & RESTRICT_HORIZONTALLY)
			if (m_largestSize.cx < io_rc->right - io_rc->left)
				io_rc->right = io_rc->left + m_largestSize.cx;
		break;
	}
	switch (i_edge) {
	case WMSZ_TOP:
	case WMSZ_TOPLEFT:
	case WMSZ_TOPRIGHT:
		if (m_smallestRestriction & RESTRICT_VERTICALLY)
			if (io_rc->bottom - io_rc->top < m_smallestSize.cy)
				io_rc->top = io_rc->bottom - m_smallestSize.cy;
		if (m_largestRestriction & RESTRICT_VERTICALLY)
			if (m_largestSize.cy < io_rc->bottom - io_rc->top)
				io_rc->top = io_rc->bottom - m_largestSize.cy;
		break;
	}
	switch (i_edge) {
	case WMSZ_BOTTOM:
	case WMSZ_BOTTOMLEFT:
	case WMSZ_BOTTOMRIGHT:
		if (m_smallestRestriction & RESTRICT_VERTICALLY)
			if (io_rc->bottom - io_rc->top < m_smallestSize.cy)
				io_rc->bottom = io_rc->top + m_smallestSize.cy;
		if (m_largestRestriction & RESTRICT_VERTICALLY)
			if (m_largestSize.cy < io_rc->bottom - io_rc->top)
				io_rc->bottom = io_rc->top + m_largestSize.cy;
		break;
	}
	return TRUE;
}


// hittest for size box
BOOL LayoutManager::wmNcHitTest(int i_x, int i_y)
{
	POINT p = { i_x, i_y };
	ScreenToClient(m_hwnd, &p);
	RECT rc = sizeGripRect();
	if (rc.left <= p.x && rc.top <= p.y) {
		SetWindowLongPtr(m_hwnd, DWLP_MSGRESULT, HTBOTTOMRIGHT);
		return TRUE;
	}
	return FALSE;
}


// WM_SIZE
BOOL LayoutManager::wmSize(DWORD /* i_fwSizeType */, short /* i_nWidth */,
						   short /* i_nHeight */)
{
	// mid DPI change the dialog manager owns the controls; see wmDpiChanged
	if (m_isDpiChanging)
		return TRUE;
	adjust();
	RedrawWindow(m_hwnd, NULL, NULL,
				 RDW_ERASE | RDW_INVALIDATE | RDW_FRAME | RDW_ALLCHILDREN);
	return TRUE;
}


// rescale a remembered pixel rectangle between two DPIs
static void rescale(RECT *io_rc, UINT i_to, UINT i_from)
{
	io_rc->left   = MulDiv(io_rc->left,   i_to, i_from);
	io_rc->top    = MulDiv(io_rc->top,    i_to, i_from);
	io_rc->right  = MulDiv(io_rc->right,  i_to, i_from);
	io_rc->bottom = MulDiv(io_rc->bottom, i_to, i_from);
}


// note the start of a DPI change exactly once
void LayoutManager::beginDpiChange()
{
	if (m_isDpiChanging)
		return;
	m_isDpiChanging = true;
	onDpiChangeBegin();
}


// WM_DPICHANGED
BOOL LayoutManager::wmDpiChanged(UINT i_dpi, const RECT *i_suggested)
{
	// normally WM_GETDPISCALEDSIZE opened the change; this covers the case
	// where it did not arrive
	beginDpiChange();

	UINT dpiOld = m_dpi;
	if (i_dpi == 0 || dpiOld == 0 || i_dpi == dpiOld) {
		m_isDpiChanging = false;
		return FALSE;
	}
	m_dpi = i_dpi;

	// Both halves of the baseline are converted.  m_rcParent is not how big the
	// parent is now: it is how big it was when m_rc was recorded, and adjust()
	// anchors a control by the difference between the two.  Converting both
	// keeps that difference - the offset from the edge - and scales it, which
	// is all adjust() needs; the window does not have to come out exactly ratio
	// times its old size for the result to be right.
	for (Items::iterator i = m_items.begin(); i != m_items.end(); ++ i) {
		rescale(&i->m_rc, i_dpi, dpiOld);
		rescale(&i->m_rcParent, i_dpi, dpiOld);
	}

	// a smallest size taken at 125% would leave the window unshrinkable on a
	// 100% monitor, and a largest one would cap it too low
	m_smallestSize.cx = MulDiv(m_smallestSize.cx, i_dpi, dpiOld);
	m_smallestSize.cy = MulDiv(m_smallestSize.cy, i_dpi, dpiOld);
	m_largestSize.cx = MulDiv(m_largestSize.cx, i_dpi, dpiOld);
	m_largestSize.cy = MulDiv(m_largestSize.cy, i_dpi, dpiOld);

	if (i_suggested)
		SetWindowPos(m_hwnd, NULL,
					 i_suggested->left, i_suggested->top,
					 rcWidth(i_suggested), rcHeight(i_suggested),
					 SWP_NOZORDER | SWP_NOACTIVATE);

	m_isDpiChanging = false;
	adjust();
	RedrawWindow(m_hwnd, NULL, NULL,
				 RDW_ERASE | RDW_INVALIDATE | RDW_FRAME | RDW_ALLCHILDREN);
	return TRUE;
}


// forward message
BOOL LayoutManager::defaultWMHandler(UINT i_message,
									 WPARAM i_wParam, LPARAM i_lParam)
{
	switch (i_message) {
	case WM_GETDPISCALEDSIZE:
		// Sent just before WM_DPICHANGED, and before the dialog manager has
		// touched anything.  Windows resizes the window before delivering that
		// message, and the WM_SIZE it raises would otherwise reach adjust()
		// while the baselines still describe the old DPI, throwing the controls
		// across the dialog until WM_DPICHANGED arrives to put them back.
		// Reported as unhandled so the default size still applies.
		beginDpiChange();
		return FALSE;
	case WM_DPICHANGED:
		return wmDpiChanged(LOWORD(i_wParam),
							reinterpret_cast<const RECT *>(i_lParam));
	case WM_SIZE:
		return wmSize((DWORD)i_wParam, LOWORD(i_lParam), HIWORD(i_lParam));
	case WM_PAINT:
		return wmPaint();
	case WM_SIZING:
		return wmSizing((int)i_wParam, reinterpret_cast<RECT *>(i_lParam));
	case WM_NCHITTEST:
		return wmNcHitTest(GET_X_LPARAM(i_lParam), GET_Y_LPARAM(i_lParam));
	}
	return FALSE;
}
