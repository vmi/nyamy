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

public:
	///
	LayoutManager(HWND i_hwnd);

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

	/// forward message
	virtual BOOL defaultWMHandler(UINT i_message, WPARAM i_wParam,
								  LPARAM i_lParam);
};


#endif // !_LAYOUTMANAGER_H
