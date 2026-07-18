//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// keyboard.h


#ifndef _KEYBOARD_H
#  define _KEYBOARD_H

#  include "misc.h"
#  include "stringtool.h"
#  include <vector>
#  include <list>
#  include <map>


/// derived from w2kddk/inc/ntddkbd.h
class KEYBOARD_INPUT_DATA
{
public:
	///
	enum {
		/// key release flag
		BREAK = 1,
		/// extended key flag
		E0 = 2,
		/// extended key flag
		E1 = 4,
		/// extended key flag (E0 | E1)
		E0E1 = 6,
		///
		TERMSRV_SET_LED = 8,
		/// Define the keyboard overrun MakeCode.
		KEYBOARD_OVERRUN_MAKE_CODE_ = 0xFF,
	};

public:
	/** Unit number.  E.g., for \Device\KeyboardPort0 the unit is '0', for
	    \Device\KeyboardPort1 the unit is '1', and so on. */
	USHORT UnitId;

	/** The "make" scan code (key depression). */
	USHORT MakeCode;

	/** The flags field indicates a "break" (key release) and other miscellaneous
	    scan code information defined above. */
	USHORT Flags;

	///
	USHORT Reserved;

	/** Device-specific additional information for the event. */
	ULONG ExtraInformation;
};


/// a scan code with flags
class ScanCode
{
public:
	///
	enum {
		BREAK = KEYBOARD_INPUT_DATA::BREAK,		/// key release flag
		E0    = KEYBOARD_INPUT_DATA::E0,		/// extended key flag
		E1    = KEYBOARD_INPUT_DATA::E1,		/// extended key flag
		E0E1  = KEYBOARD_INPUT_DATA::E0E1,		/// extended key flag
	};

public:
	USHORT m_scan;				///
	USHORT m_flags;				///

public:
	///
	ScanCode() : m_scan(0), m_flags(0) { }
	///
	ScanCode(USHORT i_scan, USHORT i_flags)
			: m_scan(i_scan), m_flags(i_flags) { }
	///
	bool operator==(const ScanCode &i_sc) const {
		return (m_scan == i_sc.m_scan &&
				(E0E1 & m_flags) == (E0E1 & i_sc.m_flags));
	}
	///
	bool operator!=(const ScanCode &i_sc) const {
		return !(*this == i_sc);
	}
};


/// a key
class Key
{
public:
	enum {
		///
		MAX_SCAN_CODES_SIZE = 4,
	};

private:
	///
	using Names = std::vector<wstringi>;

public:
	/// if this key pressed physically
	bool m_isPressed;
	/// if this key pressed on Win32
	bool m_isPressedOnWin32;
	/// if this key pressed by assign
	bool m_isPressedByAssign;

private:
	/// key name
	Names m_names;
	/// key scan code length
	size_t m_scanCodesSize;
	/// key scan code
	ScanCode m_scanCodes[MAX_SCAN_CODES_SIZE];

public:
	///
	Key()
			: m_isPressed(false),
			m_isPressedOnWin32(false),
			m_isPressedByAssign(false),
			m_scanCodesSize(0) { }

	/// for Event::* only
	Key(const wstringi &i_name)
			: m_isPressed(false),
			m_isPressedOnWin32(false),
			m_isPressedByAssign(false),
			m_scanCodesSize(0) {
		addName(i_name);
		addScanCode(ScanCode());
	}

	/// get key name (first name)
	const wstringi &getName() const {
		return m_names.front();
	}

	/// get scan codes
	const ScanCode *getScanCodes() const {
		return m_scanCodes;
	}
	///
	size_t getScanCodesSize() const {
		return m_scanCodesSize;
	}

	/// add a name of key
	void addName(const wstringi &i_name);

	/// add a scan code
	void addScanCode(const ScanCode &i_sc);

	/// initializer
	Key &initialize();

	/// equation by name
	bool operator==(const wstringi &i_name) const;
	///
	bool operator!=(const wstringi &i_name) const {
		return !(*this == i_name);
	}

	/// is the scan code of this key ?
	bool isSameScanCode(const Key &i_key) const;

	/// is the i_key's scan code the prefix of this key's scan code ?
	bool isPrefixScanCode(const Key &i_key) const;

	/// stream output
	friend std::wostream &operator<<(std::wostream &i_ost, const Key &i_key);

	/// <
	bool operator<(const Key &i_key) const {
		return getName() < i_key.getName();
	}
};


///
class Modifier
{
	///
	using MODIFIERS = u_int64;
	///
	MODIFIERS m_modifiers;
	///
	MODIFIERS m_dontcares;

public:
	///
	enum Type {
		Type_begin = 0,				///

		Type_Shift = Type_begin,			/// &lt;BASIC_MODIFIER&gt;
		Type_Alt,					/// &lt;BASIC_MODIFIER&gt;
		Type_Control,				/// &lt;BASIC_MODIFIER&gt;
		Type_Windows,				/// &lt;BASIC_MODIFIER&gt;
		Type_BASIC,					///

		Type_Up = Type_BASIC,			/// &lt;KEYSEQ_MODIFIER&gt;
		Type_Down,					/// &lt;KEYSEQ_MODIFIER&gt;
		Type_KEYSEQ,				///

		Type_Repeat = Type_KEYSEQ,			/// &lt;ASSIGN_MODIFIER&gt;
		Type_ImeLock,				/// &lt;ASSIGN_MODIFIER&gt;
		Type_ImeComp,				/// &lt;ASSIGN_MODIFIER&gt;
		Type_NumLock,				/// &lt;ASSIGN_MODIFIER&gt;
		Type_CapsLock,				/// &lt;ASSIGN_MODIFIER&gt;
		Type_ScrollLock,				/// &lt;ASSIGN_MODIFIER&gt;
		Type_KanaLock,				/// &lt;ASSIGN_MODIFIER&gt;
		Type_Maximized,				/// &lt;ASSIGN_MODIFIER&gt;
		Type_Minimized,				/// &lt;ASSIGN_MODIFIER&gt;
		Type_MdiMaximized,				/// &lt;ASSIGN_MODIFIER&gt;
		Type_MdiMinimized,				/// &lt;ASSIGN_MODIFIER&gt;
		Type_Mod0,					/// &lt;ASSIGN_MODIFIER&gt;
		Type_Mod1,					/// &lt;ASSIGN_MODIFIER&gt;
		Type_Mod2,					/// &lt;ASSIGN_MODIFIER&gt;
		Type_Mod3,					/// &lt;ASSIGN_MODIFIER&gt;
		Type_Mod4,					/// &lt;ASSIGN_MODIFIER&gt;
		Type_Mod5,					/// &lt;ASSIGN_MODIFIER&gt;
		Type_Mod6,					/// &lt;ASSIGN_MODIFIER&gt;
		Type_Mod7,					/// &lt;ASSIGN_MODIFIER&gt;
		Type_Mod8,					/// &lt;ASSIGN_MODIFIER&gt;
		Type_Mod9,					/// &lt;ASSIGN_MODIFIER&gt;
		Type_Lock0,					/// &lt;ASSIGN_MODIFIER&gt;
		Type_Lock1,					/// &lt;ASSIGN_MODIFIER&gt;
		Type_Lock2,					/// &lt;ASSIGN_MODIFIER&gt;
		Type_Lock3,					/// &lt;ASSIGN_MODIFIER&gt;
		Type_Lock4,					/// &lt;ASSIGN_MODIFIER&gt;
		Type_Lock5,					/// &lt;ASSIGN_MODIFIER&gt;
		Type_Lock6,					/// &lt;ASSIGN_MODIFIER&gt;
		Type_Lock7,					/// &lt;ASSIGN_MODIFIER&gt;
		Type_Lock8,					/// &lt;ASSIGN_MODIFIER&gt;
		Type_Lock9,					/// &lt;ASSIGN_MODIFIER&gt;
		Type_ASSIGN,				///

		Type_end = Type_ASSIGN			///
	};

public:
	///
	Modifier();
	///
	Modifier &on(Type i_type) {
		return press(i_type);
	}
	///
	Modifier &off(Type i_type) {
		return release(i_type);
	}
	///
	Modifier &press(Type i_type) {
		m_modifiers |= ((MODIFIERS(1)) << i_type);
		return care(i_type);
	}
	///
	Modifier &release(Type i_type) {
		m_modifiers &= ~((MODIFIERS(1)) << i_type);
		return care(i_type);
	}
	///
	Modifier &care(Type i_type) {
		m_dontcares &= ~((MODIFIERS(1)) << i_type);
		return *this;
	}
	///
	Modifier &dontcare(Type i_type) {
		m_dontcares |= ((MODIFIERS(1)) << i_type);
		return *this;
	}
	/// set all modifiers to dontcare
	Modifier &dontcare() {
		m_dontcares = ~MODIFIERS(0);
		return *this;
	}

	///
	Modifier &on(Type i_type, bool i_isOn) {
		return press(i_type, i_isOn);
	}
	///
	Modifier &press(Type i_type, bool i_isPressed) {
		return i_isPressed ? press(i_type) : release(i_type);
	}
	///
	Modifier &care(Type i_type, bool i_doCare) {
		return i_doCare ? care(i_type) : dontcare(i_type);
	}

	///
	bool operator==(const Modifier &i_m) const {
		return m_modifiers == i_m.m_modifiers && m_dontcares == i_m.m_dontcares;
	}

	/// add m's modifiers where this dontcare
	void add(const Modifier &i_m);
	//Modifier &operator+=(const Modifier &i_m);

	/** does match. (except dontcare modifiers) (is the m included in the *this
	    set ?) */
	bool doesMatch(const Modifier &i_m) const {
		return ((m_modifiers | m_dontcares) == (i_m.m_modifiers | m_dontcares));
	}

	///
	bool isOn(Type i_type) const {
		return isPressed(i_type);
	}
	///
	bool isPressed(Type i_type) const {
		return !!(m_modifiers & ((MODIFIERS(1)) << i_type));
	}
	///
	bool isDontcare(Type i_type) const {
		return !!(m_dontcares & ((MODIFIERS(1)) << i_type));
	}

	/// stream output
	friend std::wostream &operator<<(std::wostream &i_ost, const Modifier &i_m);

	/// <
	bool operator<(const Modifier &i_m) const {
		return m_modifiers < i_m.m_modifiers ||
			   (m_modifiers == i_m.m_modifiers && m_dontcares < i_m.m_dontcares);
	}
};


/// stream output
std::wostream &operator<<(std::wostream &i_ost, Modifier::Type i_type);


///
class ModifiedKey
{
public:
	Modifier m_modifier;	///
	Key *m_key;		///

public:
	///
	ModifiedKey() : m_key(NULL) { }
	///
	ModifiedKey(Key *i_key) : m_key(i_key) { }
	///
	ModifiedKey(const Modifier &i_modifier, Key *i_key)
			: m_modifier(i_modifier), m_key(i_key) { }
	///
	bool operator==(const ModifiedKey &i_mk) const {
		return m_modifier == i_mk.m_modifier && m_key == i_mk.m_key;
	}
	///
	bool operator!=(const ModifiedKey &i_mk) const {
		return !operator==(i_mk);
	}

	/// stream output
	friend std::wostream &operator<<(std::wostream &i_ost, const ModifiedKey &i_mk);

	/// <
	bool operator<(const ModifiedKey &i_mk) const {
		return *m_key < *i_mk.m_key ||
			   (!(*i_mk.m_key < *m_key) && m_modifier < i_mk.m_modifier);
	}
};


///
class Keyboard
{
public:
	/// keyboard modifiers (pointer into Keys)
	using Mods = std::list<Key *>;

private:
	/** keyboard keys (hashed by first scan code).
	    Keys must be *list* of Key.
	    Because *pointers* into Keys exist anywhere in this program, the address
	    of Key's elements must be fixed.  */
	enum {
		HASHED_KEYS_SIZE = 128,			///
	};
	using Keys = std::list<Key>;			///
	using Aliases = std::map<wstringi, Key *>;	/// key name aliases
	///
	class Substitute
	{
	public:
		ModifiedKey m_mkeyFrom;
		ModifiedKey m_mkeyTo;
	public:
		Substitute(const ModifiedKey &i_mkeyFrom,
				   const ModifiedKey &i_mkeyTo)
				: m_mkeyFrom(i_mkeyFrom), m_mkeyTo(i_mkeyTo) {
		}
	};
	using Substitutes = std::list<Substitute>;	/// substitutes

private:
	Keys m_hashedKeys[HASHED_KEYS_SIZE];		///
	Aliases m_aliases;				///
	Substitutes m_substitutes;			///
	Key m_syncKey;				/// key used to synchronize

private:
	///
	Mods m_mods[Modifier::Type_BASIC];

public:
	///
	class KeyIterator
	{
		///
		Keys *m_hashedKeys;
		///
		size_t m_hashedKeysSize;
		///
		Keys::iterator m_i;

		///
		void next();

	public:
		///
		KeyIterator(Keys *i_hashedKeys, size_t i_hashedKeysSize);
		///
		Key *operator *();
		///
		void operator++() {
			next();
		}
	};

private:
	///
	Keys &getKeys(const Key &i_key);

public:
	/// add a key
	void addKey(const Key &i_key);

	/// add a key name alias
	void addAlias(const wstringi &i_aliasName, Key *i_key);

	/// add substitute
	void addSubstitute(const ModifiedKey &i_mkeyFrom,
					   const ModifiedKey &i_mkeyTo);

	/// get a sync key
	Key *getSyncKey() {
		return &m_syncKey;
	}

	/// add a modifier key
	void addModifier(Modifier::Type i_mt, Key * i_key);

	/// search a key
	Key *searchKey(const Key &i_key);

	/// search a key (of which the key's scan code is the prefix)
	Key *searchPrefixKey(const Key &i_key);

	/// search a key by name
	Key *searchKey(const wstringi &i_name);

	/// search a key by non-alias name
	Key *searchKeyByNonAliasName(const wstringi &i_name);

	/// search a substitute
	ModifiedKey searchSubstitute(const ModifiedKey &i_mkey);

	/// get modifiers
	Mods &getModifiers(Modifier::Type i_mt) {
		return m_mods[i_mt];
	}

	/// get key iterator
	KeyIterator getKeyIterator() {
		return KeyIterator(&m_hashedKeys[0], HASHED_KEYS_SIZE);
	}

	/// get aliases (for enumeration)
	const Aliases &getAliases() const { return m_aliases; }

	/// get substitutes (for enumeration)
	const Substitutes &getSubstitutes() const { return m_substitutes; }
};


#endif // !_KEYBOARD_H
