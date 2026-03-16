//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// setting_builder.h


#ifndef _SETTING_BUILDER_H
#  define _SETTING_BUILDER_H


#  include "setting.h"
#  include "function.h"
#  include "adhoc_keyseq.h"
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
	using Symbols = std::set<wstringi>;

public:
	explicit SettingBuilder(Setting &s) : m_setting(s), m_currentKeymap(nullptr) {}

	// Keyboard
	void addKey(Key key) { m_setting.m_keyboard.addKey(key); }
	Key *searchKey(const wstringi &name) { return m_setting.m_keyboard.searchKey(name); }
	Key *searchKeyByNonAliasName(const wstringi &name) { return m_setting.m_keyboard.searchKeyByNonAliasName(name); }
	void addModifier(Modifier::Type mt, Key *key) { m_setting.m_keyboard.addModifier(mt, key); }
	Key *getSyncKey() { return m_setting.m_keyboard.getSyncKey(); }
	void addAlias(const wstringi &name, Key *key) { m_setting.m_keyboard.addAlias(name, key); }
	void addSubstitute(const ModifiedKey &src, const ModifiedKey &dst) { m_setting.m_keyboard.addSubstitute(src, dst); }

	// Keymaps
	Keymap *addKeymap(const Keymap &km) { return m_setting.m_keymaps.add(km); }
	Keymap *searchKeymapByName(const wstringi &name) { return m_setting.m_keymaps.searchByName(name); }
	void adjustModifiers() { m_setting.m_keymaps.adjustModifier(m_setting.m_keyboard); }

	// KeySeqs
	KeySeq *addKeySeq(const KeySeq &ks) { return m_setting.m_keySeqs.add(ks); }
	KeySeq *searchKeySeqByName(const wstringi &name) { return m_setting.m_keySeqs.searchByName(name); }

	// Symbols
	void addSymbol(const wstringi &symbol) { m_setting.m_symbols.insert(symbol); }
	bool hasSymbol(const wstringi &symbol) const { return m_setting.m_symbols.find(symbol) != m_setting.m_symbols.end(); }

	// Options (return pointers for load_ARGUMENT compatibility)
	bool *correctKanaLockHandling() { return &m_setting.m_correctKanaLockHandling; }
	unsigned int *oneShotRepeatableDelay() { return &m_setting.m_oneShotRepeatableDelay; }
	bool *mouseEvent() { return &m_setting.m_mouseEvent; }
	LONG *dragThreshold() { return &m_setting.m_dragThreshold; }

	// CmdLoadContext interface (used during command-stream interpretation)
	const Keymap *resolveKeymap(const wstringi &name) override {
		return searchKeymapByName(name);
	}
	const KeySeq *resolveKeySeq(uint32_t index) override {
		if (index < m_keySeqs.size()) return m_keySeqs[index];
		return nullptr;
	}
	Modifier resolveModifier(const CmdModifier &bm) override {
		return modifierFromCmd(bm);
	}

	// KeySeq index registry (populated as RegKeySeq commands are processed)
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

	/// Materialize a CmdArgsRegKeySeq into a KeySeq* owned by this builder
	KeySeq *materializeKeySeq(const CmdArgsRegKeySeq &cmdKs);

private:
	Setting &m_setting;                  ///< reference to the Setting being built
	std::vector<KeySeq *> m_keySeqs;     ///< index registry built during stream interpretation
	Keymap *m_currentKeymap;             ///< current keymap context during stream interpretation
};


/// Materializes ad-hoc key sequences from CmdAction lists without modifying a Setting
class AdHocMaterializer : public CmdLoadContext
{
public:
	explicit AdHocMaterializer(Setting &s) : m_setting(s) {}

	// CmdLoadContext
	const Keymap *resolveKeymap(const wstringi &name) override;
	const KeySeq *resolveKeySeq(uint32_t /*index*/) override { return nullptr; }
	Modifier resolveModifier(const CmdModifier &bm) override { return modifierFromCmd(bm); }

	/// Materialize actions into an AdHocKeySeq (not stored in Setting)
	AdHocKeySeq materialize(const std::vector<CmdAction> &actions,
	                        const TriggerInfo &ctx);

private:
	Setting &m_setting;
};


#endif // !_SETTING_BUILDER_H
