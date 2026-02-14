//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// setting.h


#ifndef _SETTING_H
#  define _SETTING_H


#  include "keymap.h"
#  include <set>


/// this class contains all of loaded settings
class Setting
{
public:
	using Symbols = std::set<tstringi>;		///
	using Modifiers = std::list<Modifier>;	///

public:
	Keyboard m_keyboard;				///
	Keymaps m_keymaps;				///
	KeySeqs m_keySeqs;				///
	Symbols m_symbols;				///
	bool m_correctKanaLockHandling;		///
	bool m_sts4mayu;				///
	bool m_cts4mayu;				///
	bool m_mouseEvent;				///
	LONG m_dragThreshold;			///
	unsigned int m_oneShotRepeatableDelay;	///

public:
	Setting()
			: m_correctKanaLockHandling(false),
			m_sts4mayu(false),
			m_cts4mayu(false),
			m_mouseEvent(false),
			m_dragThreshold(0),
			m_oneShotRepeatableDelay(0) { }
};


///
namespace Event
{
///
extern Key prefixed;
///
extern Key before_key_down;
///
extern Key after_key_up;
///
extern Key *events[];
}


#endif // !_SETTING_H
