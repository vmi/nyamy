//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// layoutmanager.h


#ifndef _LAYOUTMANAGER_H
#  define _LAYOUTMANAGER_H

#  include "misc.h"
#  include <list>


///
class LayoutManager
{
public:
	///
	enum Origin {
		ORIGIN_LEFT_EDGE,				///
		ORIGIN_TOP_EDGE = ORIGIN_LEFT_EDGE,		///
		ORIGIN_CENTER,				///
		ORIGIN_RIGHT_EDGE,				///
		ORIGIN_BOTTOM_EDGE = ORIGIN_RIGHT_EDGE,	///
	};

	///
	enum Restrict {
		RESTRICT_NONE = 0,				///
		RESTRICT_HORIZONTALLY = 1,			///
		RESTRICT_VERTICALLY = 2,			///
		RESTRICT_BOTH = RESTRICT_HORIZONTALLY | RESTRICT_VERTICALLY, ///
	};

private:
	///
	class Item
	{
	public:
		HWND m_hwnd;				///
		HWND m_hwndParent;				///
		RECT m_rc;					///
		RECT m_rcParent;				///
		Origin m_origin[4];				///
	};

	///
	class SmallestSize
	{
	public:
		HWND m_hwnd;				///
		SIZE m_size;				///

	public:
		///
		SmallestSize() : m_hwnd(NULL) { }
	};

	using Items = std::list<Item>;		///



protected:
	HWND m_hwnd;					///

private:
	Items m_items;				///
	Restrict m_smallestRestriction;		///
	SIZE m_smallestSize;				///
	Restrict m_largestRestriction;		///
	SIZE m_largestSize;				///
	/// DPI the size limits were captured at
	UINT m_dpi;
	/** A DPI change is in flight.

	    Windows resizes the window and rescales its controls before it delivers
	    WM_DPICHANGED, and adjust() must not run on the WM_SIZE that raises: it
	    would put every control back at the size and offset recorded for the
	    old DPI, undoing the rescaling as fast as the dialog manager applies it.
	    That is what made an earlier round look as though Per-Monitor v2 did not
	    scale dialogs at all. */
	bool m_isDpiChanging;

public:
	///
	LayoutManager(HWND i_hwnd);
	///
	virtual ~LayoutManager();

protected:
	/** A DPI change is starting, and nothing has been rescaled yet.

	    The one point at which a derived class can still read the dialog as the
	    user last saw it.  By the time WM_DPICHANGED arrives the dialog manager
	    has already resized the controls and rescaled their fonts, so anything
	    that has to survive the change - a scroll position, say - has to be
	    noted here rather than there. */
	virtual void onDpiChangeBegin() {}

	/// note the start of a DPI change exactly once, whichever message opens it
	void beginDpiChange();

public:

	/** restrict the smallest size of the window to the current size of it or
	    specified by i_size */
	void restrictSmallestSize(Restrict i_restrict = RESTRICT_BOTH,
							  SIZE *i_size = NULL);

	/** restrict the largest size of the window to the current size of it or
	    specified by i_size */
	void restrictLargestSize(Restrict i_restrict = RESTRICT_BOTH,
							 SIZE *i_size = NULL);

	///
	bool addItem(HWND i_hwnd,
				 Origin i_originLeft = ORIGIN_LEFT_EDGE,
				 Origin i_originTop = ORIGIN_TOP_EDGE,
				 Origin i_originRight = ORIGIN_LEFT_EDGE,
				 Origin i_originBottom = ORIGIN_TOP_EDGE);
	/** Forget an item.  Needed by anyone who destroys and recreates a child
	    window, since the item would otherwise keep a dangling HWND. */
	bool removeItem(HWND i_hwnd);
	///
	void adjust() const;

private:
	/** Client rect of the size box, for the window's current DPI.

	    Shared by the drawing and the hit test so the two cannot disagree:
	    a grip drawn at one size and grabbed at another is invisible as a bug
	    until someone tries to resize by a corner that no longer answers. */
	RECT sizeGripRect() const;

public:
	/// draw size box
	virtual BOOL wmPaint();

	/// size restriction
	virtual BOOL wmSizing(int i_edge, RECT *io_rc);

	/// hittest for size box
	virtual BOOL wmNcHitTest(int i_x, int i_y);

	/// WM_SIZE
	virtual BOOL wmSize(DWORD /* i_fwSizeType */, short /* i_nWidth */,
						short /* i_nHeight */);

	/** WM_DPICHANGED: convert the layout baseline, then re-apply it.

	    Per-Monitor v2 has already scaled the controls and their fonts by the
	    time this arrives, so the fonts are left alone - scaling them here as
	    well applied the factor twice and left the text visibly small after a
	    round trip.  What the dialog manager cannot know is this class' baseline,
	    and that has to be converted synchronously: reading the geometry back
	    from a posted message instead looks equivalent, but the restore that
	    runs during WM_INITDIALOG changes DPI and size in one step, and the
	    adjust() at the end of it would then bake a layout computed from the old
	    DPI into the baseline for good. */
	virtual BOOL wmDpiChanged(UINT i_dpi, const RECT *i_suggested);

	/// forward message
	virtual BOOL defaultWMHandler(UINT i_message, WPARAM i_wParam,
								  LPARAM i_lParam);
};


#endif // !_LAYOUTMANAGER_H
