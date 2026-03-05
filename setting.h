//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// setting.h


#ifndef _SETTING_H
#  define _SETTING_H


#  include "keymap.h"
#  include <memory>
#  include <set>


/// this class contains all of loaded settings
class Setting
{
public:
	using Symbols = std::set<wstringi>;		///
	using Modifiers = std::list<Modifier>;	///

public:
	Keyboard m_keyboard;				///
	Keymaps m_keymaps;				///
	KeySeqs m_keySeqs;				///
	Symbols m_symbols;				///
	bool m_correctKanaLockHandling;		///
	bool m_mouseEvent;				///
	LONG m_dragThreshold;			///
	unsigned int m_oneShotRepeatableDelay;	///

public:
	Setting()
			: m_correctKanaLockHandling(false),
			m_mouseEvent(false),
			m_dragThreshold(0),
			m_oneShotRepeatableDelay(0) { }
};


///
namespace Event
{
///
inline Key prefixed(L"prefixed");
///
inline Key before_key_down(L"before-key-down");
///
inline Key after_key_up(L"after-key-up");
///
inline Key *events[] = { &prefixed, &before_key_down, &after_key_up, NULL };
}


#endif // !_SETTING_H
