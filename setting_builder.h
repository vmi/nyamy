//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// setting_builder.h


#ifndef _SETTING_BUILDER_H
#  define _SETTING_BUILDER_H


#  include "setting.h"
#  include "function.h"
#  include <vector>


/// Convert a CmdModifier bitmask to a Modifier object
inline Modifier modifierFromCmd(const CmdModifier &bm)
{
	Modifier mod;
	for (int i = Modifier::Type_begin; i != Modifier::Type_end; ++i) {
		uint64_t bit = uint64_t(1) << i;
		Modifier::Type mt = static_cast<Modifier::Type>(i);
		if (bm.dontcares & bit) {
			mod.dontcare(mt);
		} else {
			mod.care(mt);
			if (bm.modifiers & bit)
				mod.press(mt);
			else
				mod.release(mt);
		}
	}
	return mod;
}


/// builds a Setting object through a structured interface;
/// also implements CmdLoadContext for use during command-stream interpretation
class SettingBuilder : public CmdLoadContext
{
public:
	///
	using Symbols = std::set<tstringi>;

public:
	SettingBuilder() : m_setting(std::make_unique<Setting>()), m_currentKeymap(nullptr) { }

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

	// CmdLoadContext interface (used during command-stream interpretation)
	const Keymap *resolveKeymap(const tstringi &name) override {
		return searchKeymapByName(name);
	}
	const KeySeq *resolveKeySeq(uint32_t index) override {
		if (index < m_keySeqs.size()) return m_keySeqs[index];
		return nullptr;
	}
	Modifier resolveModifier(const CmdModifier &bm) override {
		return modifierFromCmd(bm);
	}

	// KeySeq index registry (populated as DefKeySeq commands are processed)
	void pushKeySeqRef(KeySeq *ks) { m_keySeqs.push_back(ks); }
	KeySeq *getKeySeqRef(uint32_t index) {
		if (index < m_keySeqs.size()) return m_keySeqs[index];
		return nullptr;
	}

	// Current keymap (tracks state during command-stream interpretation)
	Keymap *currentKeymap() const { return m_currentKeymap; }
	void setCurrentKeymap(Keymap *km) { m_currentKeymap = km; }

	// Convert a CmdModifiedKey to a ModifiedKey
	ModifiedKey toModifiedKey(const CmdModifiedKey &bmk) {
		ModifiedKey mkey;
		mkey.m_modifier = modifierFromCmd(bmk.modifier);
		mkey.m_key = searchKey(bmk.keyName);
		return mkey;
	}

	/// Materialize a CmdKeySequence into a KeySeq* owned by this builder
	KeySeq *materializeKeySeq(const CmdKeySequence &cmdKs);

	/// build the completed Setting
	std::unique_ptr<Setting> build() { return std::move(m_setting); }

private:
	std::unique_ptr<Setting> m_setting;
	std::vector<KeySeq *> m_keySeqs;	///< index registry built during stream interpretation
	Keymap *m_currentKeymap;			///< current keymap context during stream interpretation
};


#endif // !_SETTING_BUILDER_H
