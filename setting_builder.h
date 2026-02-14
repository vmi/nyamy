//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// setting_builder.h


#ifndef _SETTING_BUILDER_H
#  define _SETTING_BUILDER_H


#  include "keymap.h"
#  include <memory>
#  include <set>


class Setting;


/// builds a Setting object through a structured interface
class SettingBuilder
{
public:
	///
	using Symbols = std::set<tstringi>;

public:
	SettingBuilder();

	// Keyboard
	void addKey(Key key);
	Key *searchKey(const tstringi &name);
	Key *searchKeyByNonAliasName(const tstringi &name);
	void addModifier(Modifier::Type mt, Key *key);
	Key *getSyncKey();
	void addAlias(const tstringi &name, Key *key);
	void addSubstitute(const ModifiedKey &src, const ModifiedKey &dst);

	// Keymaps
	Keymap *addKeymap(const Keymap &km);
	Keymap *searchKeymapByName(const tstringi &name);
	void adjustModifiers();

	// KeySeqs
	KeySeq *addKeySeq(const KeySeq &ks);
	KeySeq *searchKeySeqByName(const tstringi &name);

	// Symbols
	void addSymbol(const tstringi &symbol);
	bool hasSymbol(const tstringi &symbol) const;

	// Options (return pointers for load_ARGUMENT compatibility)
	bool *correctKanaLockHandling();
	unsigned int *oneShotRepeatableDelay();
	bool *sts4mayu();
	bool *cts4mayu();
	bool *mouseEvent();
	LONG *dragThreshold();

	/// build the completed Setting
	std::unique_ptr<Setting> build();

private:
	std::unique_ptr<Setting> m_setting;
};


#endif // !_SETTING_BUILDER_H
