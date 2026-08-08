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

	/** Scan codes of NLS keys: keys whose break event never reaches the hook
	    because the keyboard layout driver consumes it first (see
	    README-yamy.txt 3.2).  E0/E1 prefixed codes are held as 0xE0nn / 0xE1nn,
	    the encoding the scripter's sc() helper produces. */
	std::set<USHORT> m_nlsKeys;

public:
	Setting()
			: m_correctKanaLockHandling(false),
			m_mouseEvent(false),
			m_dragThreshold(0),
			m_oneShotRepeatableDelay(0) { }

	/// does this scan code need a synthesized break ?
	bool isNlsKey(USHORT i_scan, USHORT i_flags) const {
		if (m_nlsKeys.empty())
			return false;
		USHORT code = static_cast<USHORT>(i_scan & 0xff);
		if (i_flags & KEYBOARD_INPUT_DATA::E0)
			code |= 0xe000;
		else if (i_flags & KEYBOARD_INPUT_DATA::E1)
			code |= 0xe100;
		return m_nlsKeys.find(code) != m_nlsKeys.end();
	}
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
