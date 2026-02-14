//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// setting_builder.cpp


#include "misc.h"

#include "setting_builder.h"
#include "setting.h"


// constructor
SettingBuilder::SettingBuilder()
		: m_setting(std::make_unique<Setting>())
{
}


// Keyboard

void SettingBuilder::addKey(Key key)
{
	m_setting->m_keyboard.addKey(key);
}

Key *SettingBuilder::searchKey(const tstringi &name)
{
	return m_setting->m_keyboard.searchKey(name);
}

Key *SettingBuilder::searchKeyByNonAliasName(const tstringi &name)
{
	return m_setting->m_keyboard.searchKeyByNonAliasName(name);
}

void SettingBuilder::addModifier(Modifier::Type mt, Key *key)
{
	m_setting->m_keyboard.addModifier(mt, key);
}

Key *SettingBuilder::getSyncKey()
{
	return m_setting->m_keyboard.getSyncKey();
}

void SettingBuilder::addAlias(const tstringi &name, Key *key)
{
	m_setting->m_keyboard.addAlias(name, key);
}

void SettingBuilder::addSubstitute(const ModifiedKey &src,
								   const ModifiedKey &dst)
{
	m_setting->m_keyboard.addSubstitute(src, dst);
}


// Keymaps

Keymap *SettingBuilder::addKeymap(const Keymap &km)
{
	return m_setting->m_keymaps.add(km);
}

Keymap *SettingBuilder::searchKeymapByName(const tstringi &name)
{
	return m_setting->m_keymaps.searchByName(name);
}

void SettingBuilder::adjustModifiers()
{
	m_setting->m_keymaps.adjustModifier(m_setting->m_keyboard);
}


// KeySeqs

KeySeq *SettingBuilder::addKeySeq(const KeySeq &ks)
{
	return m_setting->m_keySeqs.add(ks);
}

KeySeq *SettingBuilder::searchKeySeqByName(const tstringi &name)
{
	return m_setting->m_keySeqs.searchByName(name);
}


// Symbols

void SettingBuilder::addSymbol(const tstringi &symbol)
{
	m_setting->m_symbols.insert(symbol);
}

bool SettingBuilder::hasSymbol(const tstringi &symbol) const
{
	return m_setting->m_symbols.find(symbol) != m_setting->m_symbols.end();
}


// Options

bool *SettingBuilder::correctKanaLockHandling()
{
	return &m_setting->m_correctKanaLockHandling;
}

unsigned int *SettingBuilder::oneShotRepeatableDelay()
{
	return &m_setting->m_oneShotRepeatableDelay;
}

bool *SettingBuilder::sts4mayu()
{
	return &m_setting->m_sts4mayu;
}

bool *SettingBuilder::cts4mayu()
{
	return &m_setting->m_cts4mayu;
}

bool *SettingBuilder::mouseEvent()
{
	return &m_setting->m_mouseEvent;
}

LONG *SettingBuilder::dragThreshold()
{
	return &m_setting->m_dragThreshold;
}


// build

std::unique_ptr<Setting> SettingBuilder::build()
{
	return std::move(m_setting);
}
