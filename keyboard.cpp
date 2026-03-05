//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// setting.cpp


#include "keyboard.h"

#include <algorithm>


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Key


// add a name or an alias of key
void Key::addName(const wstringi &i_name)
{
	m_names.push_back(i_name);
}


// add a scan code
void Key::addScanCode(const ScanCode &i_sc)
{
	ASSERT(m_scanCodesSize < MAX_SCAN_CODES_SIZE);
	m_scanCodes[m_scanCodesSize ++] = i_sc;
}


// initializer
Key &Key::initialize()
{
	m_names.clear();
	m_isPressed = false;
	m_isPressedOnWin32 = false;
	m_isPressedByAssign = false;
	m_scanCodesSize = 0;
	return *this;
}


// equation by name
bool Key::operator==(const wstringi &i_name) const
{
	return std::find(m_names.begin(), m_names.end(), i_name) != m_names.end();
}


// is the scan code of this key ?
bool Key::isSameScanCode(const Key &i_key) const
{
	if (m_scanCodesSize != i_key.m_scanCodesSize)
		return false;
	return isPrefixScanCode(i_key);
}


// is the key's scan code the prefix of this key's scan code ?
bool Key::isPrefixScanCode(const Key &i_key) const
{
	for (size_t i = 0; i < i_key.m_scanCodesSize; ++ i)
		if (m_scanCodes[i] != i_key.m_scanCodes[i])
			return false;
	return true;
}


// stream output
std::wostream &operator<<(std::wostream &i_ost, const Key &i_mk)
{
	return i_ost << i_mk.getName();
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Modifier


Modifier::Modifier()
		: m_modifiers(0),
		m_dontcares(0)
{
	ASSERT(Type_end <= (sizeof(MODIFIERS) * 8));
	static const Type defaultDontCare[] = {
		Type_Up, Type_Down, Type_Repeat,
		Type_ImeLock, Type_ImeComp, Type_NumLock, Type_CapsLock, Type_ScrollLock,
		Type_KanaLock,
		Type_Maximized, Type_Minimized, Type_MdiMaximized, Type_MdiMinimized,
		Type_Lock0, Type_Lock1, Type_Lock2, Type_Lock3, Type_Lock4,
		Type_Lock5, Type_Lock6, Type_Lock7, Type_Lock8, Type_Lock9,
	};
	for (size_t i = 0; i < NUMBER_OF(defaultDontCare); ++ i)
		dontcare(defaultDontCare[i]);
}


// add m's modifiers where this dontcare
void Modifier::add(const Modifier &i_m)
{
	for (int i = 0; i < Type_end; ++ i) {
		if (isDontcare(static_cast<Modifier::Type>(i)))
			if (!i_m.isDontcare(static_cast<Modifier::Type>(i)))
				if (i_m.isPressed(static_cast<Modifier::Type>(i)))
					press(static_cast<Modifier::Type>(i));
				else
					release(static_cast<Modifier::Type>(i));
	}
}

// stream output
std::wostream &operator<<(std::wostream &i_ost, const Modifier &i_m)
{
	struct Mods {
		Modifier::Type m_mt;
		const wchar_t *m_symbol;
	};

	const static Mods mods[] = {
		{ Modifier::Type_Up, L"U-" }, { Modifier::Type_Down, L"D-" },
		{ Modifier::Type_Shift, L"S-" }, { Modifier::Type_Alt, L"A-" },
		{ Modifier::Type_Control, L"C-" }, { Modifier::Type_Windows, L"W-" },
		{ Modifier::Type_Repeat, L"R-" },
		{ Modifier::Type_ImeLock, L"IL-" },
		{ Modifier::Type_ImeComp, L"IC-" },
		{ Modifier::Type_ImeComp, L"I-" },
		{ Modifier::Type_NumLock, L"NL-" },
		{ Modifier::Type_CapsLock, L"CL-" },
		{ Modifier::Type_ScrollLock, L"SL-" },
		{ Modifier::Type_KanaLock, L"KL-" },
		{ Modifier::Type_Maximized, L"MAX-" },
		{ Modifier::Type_Minimized, L"MIN-" },
		{ Modifier::Type_MdiMaximized, L"MMAX-" },
		{ Modifier::Type_MdiMinimized, L"MMIN-" },
		{ Modifier::Type_Mod0, L"M0-" }, { Modifier::Type_Mod1, L"M1-" },
		{ Modifier::Type_Mod2, L"M2-" }, { Modifier::Type_Mod3, L"M3-" },
		{ Modifier::Type_Mod4, L"M4-" }, { Modifier::Type_Mod5, L"M5-" },
		{ Modifier::Type_Mod6, L"M6-" }, { Modifier::Type_Mod7, L"M7-" },
		{ Modifier::Type_Mod8, L"M8-" }, { Modifier::Type_Mod9, L"M9-" },
		{ Modifier::Type_Lock0, L"L0-" }, { Modifier::Type_Lock1, L"L1-" },
		{ Modifier::Type_Lock2, L"L2-" }, { Modifier::Type_Lock3, L"L3-" },
		{ Modifier::Type_Lock4, L"L4-" }, { Modifier::Type_Lock5, L"L5-" },
		{ Modifier::Type_Lock6, L"L6-" }, { Modifier::Type_Lock7, L"L7-" },
		{ Modifier::Type_Lock8, L"L8-" }, { Modifier::Type_Lock9, L"L9-" },
	};

	for (size_t i = 0; i < NUMBER_OF(mods); ++ i)
		if (!i_m.isDontcare(mods[i].m_mt) && i_m.isPressed(mods[i].m_mt))
			i_ost << mods[i].m_symbol;
#if 0
		else if (!i_m.isDontcare(mods[i].m_mt) && i_m.isPressed(mods[i].m_mt))
			i_ost << L"~" << mods[i].m_symbol;
		else
			i_ost << L"*" << mods[i].m_symbol;
#endif

	return i_ost;
}


/// stream output
std::wostream &operator<<(std::wostream &i_ost, Modifier::Type i_type)
{
	const wchar_t *modNames[] = {
		L"Shift",
		L"Alt",
		L"Control",
		L"Windows",
		L"Up",
		L"Down",
		L"Repeat",
		L"ImeLock",
		L"ImeComp",
		L"NumLock",
		L"CapsLock",
		L"ScrollLock",
		L"KanaLock",
		L"Maximized",
		L"Minimized",
		L"MdiMaximized",
		L"MdiMinimized",
		L"Touchpad",
		L"TouchpadSticky",
		L"Mod0",
		L"Mod1",
		L"Mod2",
		L"Mod3",
		L"Mod4",
		L"Mod5",
		L"Mod6",
		L"Mod7",
		L"Mod8",
		L"Mod9",
		L"Lock0",
		L"Lock1",
		L"Lock2",
		L"Lock3",
		L"Lock4",
		L"Lock5",
		L"Lock6",
		L"Lock7",
		L"Lock8",
		L"Lock9",
	};

	int i = static_cast<int>(i_type);
	if (0 <= i && i < NUMBER_OF(modNames))
		i_ost << modNames[i];

	return i_ost;
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ModifiedKey


// stream output
std::wostream &operator<<(std::wostream &i_ost, const ModifiedKey &i_mk)
{
	if (i_mk.m_key)
		i_ost << i_mk.m_modifier << *i_mk.m_key;
	return i_ost;
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Keyboard::KeyIterator


Keyboard::KeyIterator::KeyIterator(Keys *i_hashedKeys, size_t i_hashedKeysSize)
		: m_hashedKeys(i_hashedKeys),
		m_hashedKeysSize(i_hashedKeysSize),
		m_i((*m_hashedKeys).begin())
{
	if ((*m_hashedKeys).empty()) {
		do {
			-- m_hashedKeysSize;
			++ m_hashedKeys;
		} while (0 < m_hashedKeysSize && (*m_hashedKeys).empty());
		if (0 < m_hashedKeysSize)
			m_i = (*m_hashedKeys).begin();
	}
}


void Keyboard::KeyIterator::next()
{
	if (m_hashedKeysSize == 0)
		return;
	++ m_i;
	if (m_i == (*m_hashedKeys).end()) {
		do {
			-- m_hashedKeysSize;
			++ m_hashedKeys;
		} while (0 < m_hashedKeysSize && (*m_hashedKeys).empty());
		if (0 < m_hashedKeysSize)
			m_i = (*m_hashedKeys).begin();
	}
}


Key *Keyboard::KeyIterator::operator *()
{
	if (m_hashedKeysSize == 0)
		return NULL;
	return &*m_i;
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Keyboard


Keyboard::Keys &Keyboard::getKeys(const Key &i_key)
{
	ASSERT(1 <= i_key.getScanCodesSize());
	return m_hashedKeys[i_key.getScanCodes()->m_scan % HASHED_KEYS_SIZE];
}


// add a key
void Keyboard::addKey(const Key &i_key)
{
	getKeys(i_key).push_front(i_key);
}


// add a key name alias
void Keyboard::addAlias(const wstringi &i_aliasName, Key *i_key)
{
	m_aliases.insert(Aliases::value_type(i_aliasName, i_key));
}

// add substitute
void Keyboard::addSubstitute(const ModifiedKey &i_mkeyFrom,
							 const ModifiedKey &i_mkeyTo)
{
	m_substitutes.push_front(Substitute(i_mkeyFrom, i_mkeyTo));
}


// add a modifier key
void Keyboard::addModifier(Modifier::Type i_mt, Key *i_key)
{
	ASSERT((int)i_mt < (int)Modifier::Type_BASIC);
	if (std::find(m_mods[i_mt].begin(), m_mods[i_mt].end(), i_key)
			!= m_mods[i_mt].end())
		return; // already added
	m_mods[i_mt].push_back(i_key);
}


// search a key
Key *Keyboard::searchKey(const Key &i_key)
{
	Keys &keys = getKeys(i_key);
	for (Keys::iterator i = keys.begin(); i != keys.end(); ++ i)
		if ((*i).isSameScanCode(i_key))
			return &*i;
	return NULL;
}


// search a key (of which the key's scan code is the prefix)
Key *Keyboard::searchPrefixKey(const Key &i_key)
{
	Keys &keys = getKeys(i_key);
	for (Keys::iterator i = keys.begin(); i != keys.end(); ++ i)
		if ((*i).isPrefixScanCode(i_key))
			return &*i;
	return NULL;
}


// search a key by name
Key *Keyboard::searchKey(const wstringi &i_name)
{
	Aliases::iterator i = m_aliases.find(i_name);
	if (i != m_aliases.end())
		return (*i).second;
	return searchKeyByNonAliasName(i_name);
}


// search a key by non-alias name
Key *Keyboard::searchKeyByNonAliasName(const wstringi &i_name)
{
	for (int j = 0; j < HASHED_KEYS_SIZE; ++ j) {
		Keys &keys = m_hashedKeys[j];
		Keys::iterator i = std::find(keys.begin(), keys.end(), i_name);
		if (i != keys.end())
			return &*i;
	}
	return NULL;
}

/// search a substitute
ModifiedKey Keyboard::searchSubstitute(const ModifiedKey &i_mkey)
{
	for (Substitutes::const_iterator
			i = m_substitutes.begin(); i != m_substitutes.end(); ++ i)
		if (i->m_mkeyFrom.m_key == i_mkey.m_key &&
				i->m_mkeyFrom.m_modifier.doesMatch(i_mkey.m_modifier))
			return i->m_mkeyTo;
	return ModifiedKey();				// not found (.m_mkey is NULL)
}
