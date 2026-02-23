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
inline Key prefixed(_T("prefixed"));
///
inline Key before_key_down(_T("before-key-down"));
///
inline Key after_key_up(_T("after-key-up"));
///
inline Key *events[] = { &prefixed, &before_key_down, &after_key_up, NULL };
}


/// builds a Setting object through a structured interface
class SettingBuilder
{
public:
	///
	using Symbols = std::set<tstringi>;

public:
	SettingBuilder() : m_setting(std::make_unique<Setting>()) { }

	// Keyboard
	void addKey(Key key) { m_setting->m_keyboard.addKey(key); }
	Key *searchKey(const tstringi &name) { return m_setting->m_keyboard.searchKey(name); }
	Key *searchKeyByNonAliasName(const tstringi &name) { return m_setting->m_keyboard.searchKeyByNonAliasName(name); }
	void addModifier(Modifier::Type mt, Key *key) { m_setting->m_keyboard.addModifier(mt, key); }
	Key *getSyncKey() { return m_setting->m_keyboard.getSyncKey(); }
	void addAlias(const tstringi &name, Key *key) { m_setting->m_keyboard.addAlias(name, key); }
	void addSubstitute(const ModifiedKey &src, const ModifiedKey &dst) { m_setting->m_keyboard.addSubstitute(src, dst); }

	// Keymaps
	Keymap *addKeymap(const Keymap &km) { return m_setting->m_keymaps.add(km); }
	Keymap *searchKeymapByName(const tstringi &name) { return m_setting->m_keymaps.searchByName(name); }
	void adjustModifiers() { m_setting->m_keymaps.adjustModifier(m_setting->m_keyboard); }

	// KeySeqs
	KeySeq *addKeySeq(const KeySeq &ks) { return m_setting->m_keySeqs.add(ks); }
	KeySeq *searchKeySeqByName(const tstringi &name) { return m_setting->m_keySeqs.searchByName(name); }

	// Symbols
	void addSymbol(const tstringi &symbol) { m_setting->m_symbols.insert(symbol); }
	bool hasSymbol(const tstringi &symbol) const { return m_setting->m_symbols.find(symbol) != m_setting->m_symbols.end(); }

	// Options (return pointers for load_ARGUMENT compatibility)
	bool *correctKanaLockHandling() { return &m_setting->m_correctKanaLockHandling; }
	unsigned int *oneShotRepeatableDelay() { return &m_setting->m_oneShotRepeatableDelay; }
	bool *sts4mayu() { return &m_setting->m_sts4mayu; }
	bool *cts4mayu() { return &m_setting->m_cts4mayu; }
	bool *mouseEvent() { return &m_setting->m_mouseEvent; }
	LONG *dragThreshold() { return &m_setting->m_dragThreshold; }

	/// build the completed Setting
	std::unique_ptr<Setting> build() { return std::move(m_setting); }

private:
	std::unique_ptr<Setting> m_setting;
};


#endif // !_SETTING_H
