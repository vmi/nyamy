//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// stringtool.h


#ifndef _STRINGTOOL_H
#  define _STRINGTOOL_H

#  include "misc.h"
#  include <cctype>
#  include <string>
#  include <iosfwd>
#  include <fstream>
#  include <locale>
#  include <regex>
#  include <stdio.h>				// for snprintf


/// basic_regex with stored pattern (wchar_t variant)
class wregex_stored : public std::wregex {
private:
    std::wstring m_pattern;

public:
	wregex_stored() : std::wregex(), m_pattern() {
    }

	wregex_stored(const wchar_t* i_pattern,
		std::regex::flag_type i_flags = (std::regex::ECMAScript))
		: std::wregex(i_pattern, i_flags),
		m_pattern(i_pattern) {
	}

	wregex_stored(const std::wstring &i_pattern,
		std::regex::flag_type i_flags = (std::regex::ECMAScript))
		: std::wregex(i_pattern, i_flags),
		m_pattern(i_pattern) {
	}

	wregex_stored& assign(const wchar_t* i_pattern,
		std::regex::flag_type i_flags = (std::regex::ECMAScript)) {
		std::wregex::assign(i_pattern, i_flags);
		m_pattern = i_pattern;
		return *this;
    }

	wregex_stored& assign(const std::wstring &i_pattern,
		std::regex::flag_type i_flags = (std::regex::ECMAScript)) {
		std::wregex::assign(i_pattern, i_flags);
		m_pattern = i_pattern;
		return *this;
    }

	const std::wstring &str() const {
		return m_pattern;
    }

};


/// string with custom stream output
class wstringq : public std::wstring
{
public:
	///
	wstringq() { }
	///
	wstringq(const wstringq &i_str) : std::wstring(i_str) { }
	///
	wstringq(const std::wstring &i_str) : std::wstring(i_str) { }
	///
	wstringq(const wchar_t *i_str) : std::wstring(i_str) { }
	///
	wstringq(const wchar_t *i_str, size_t i_n) : std::wstring(i_str, i_n) { }
	///
	wstringq(const wchar_t *i_str, size_t i_pos, size_t i_n)
			: std::wstring(i_str, i_pos, i_n) { }
	///
	wstringq(size_t i_n, wchar_t i_c) : std::wstring(i_n, i_c) { }
};


/// stream output
extern std::wostream &operator<<(std::wostream &i_ost, const wstringq &i_data);


/// write a pattern as an ECMAScript /regexp/ literal
extern std::wostream &outputRegexp(std::wostream &i_ost,
								   const std::wstring &i_pattern);


/// interpret meta characters such as \n
std::wstring interpretMetaCharacters(const wchar_t *i_str, size_t i_len,
								const wchar_t *i_quote = NULL,
								bool i_doesUseRegexpBackReference = false);

/// add session id to i_str
std::wstring addSessionId(const wchar_t *i_str);

/// copy
size_t wcslcpy(wchar_t *o_dest, const wchar_t *i_src, size_t i_destSize);

/// converter
std::wstring to_wstring(const std::string &i_str);
/// converter
std::string to_string(const std::wstring &i_str);
// convert wstring to UTF-8
std::string to_UTF8(const std::wstring &i_str);
// convert UTF-8 encoded string to wstring
std::wstring from_UTF8(const std::string &i_str);


/// case insensitive string
class wstringi : public std::wstring
{
public:
	///
	wstringi() { }
	///
	wstringi(const wstringi &i_str) : std::wstring(i_str) { }
	///
	wstringi(const std::wstring &i_str) : std::wstring(i_str) { }
	///
	wstringi(const wchar_t *i_str) : std::wstring(i_str) { }
	///
	wstringi(const wchar_t *i_str, size_t i_n) : std::wstring(i_str, i_n) { }
	///
	wstringi(const wchar_t *i_str, size_t i_pos, size_t i_n)
			: std::wstring(i_str, i_pos, i_n) { }
	///
	wstringi(size_t i_n, wchar_t i_c) : std::wstring(i_n, i_c) { }
	///
	int compare(const wstringi &i_str) const {
		return compare(i_str.c_str());
	}
	///
	int compare(const std::wstring &i_str) const {
		return compare(i_str.c_str());
	}
	///
	int compare(const wchar_t *i_str) const {
		return _wcsicmp(c_str(), i_str);
	}
	///
	std::wstring &getString() {
		return *this;
	}
	///
	const std::wstring &getString() const {
		return *this;
	}
};

/// case insensitive string comparison
inline bool operator<(const wstringi &i_str1, const wchar_t *i_str2)
{
	return i_str1.compare(i_str2) < 0;
}
/// case insensitive string comparison
inline bool operator<(const wchar_t *i_str1, const wstringi &i_str2)
{
	return 0 < i_str2.compare(i_str1);
}
/// case insensitive string comparison
inline bool operator<(const wstringi &i_str1, const std::wstring &i_str2)
{
	return i_str1.compare(i_str2) < 0;
}
/// case insensitive string comparison
inline bool operator<(const std::wstring &i_str1, const wstringi &i_str2)
{
	return 0 < i_str2.compare(i_str1);
}
/// case insensitive string comparison
inline bool operator<(const wstringi &i_str1, const wstringi &i_str2)
{
	return i_str1.compare(i_str2) < 0;
}

/// case insensitive string comparison
inline bool operator==(const wchar_t *i_str1, const wstringi &i_str2)
{
	return i_str2.compare(i_str1) == 0;
}
/// case insensitive string comparison
inline bool operator==(const wstringi &i_str1, const wchar_t *i_str2)
{
	return i_str1.compare(i_str2) == 0;
}
/// case insensitive string comparison
inline bool operator==(const std::wstring &i_str1, const wstringi &i_str2)
{
	return i_str2.compare(i_str1) == 0;
}
/// case insensitive string comparison
inline bool operator==(const wstringi &i_str1, const std::wstring &i_str2)
{
	return i_str1.compare(i_str2) == 0;
}
/// case insensitive string comparison
inline bool operator==(const wstringi &i_str1, const wstringi &i_str2)
{
	return i_str1.compare(i_str2) == 0;
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// workarounds for Borland C++


/// case insensitive string comparison
inline bool operator!=(const wchar_t *i_str1, const wstringi &i_str2)
{
	return i_str2.compare(i_str1) != 0;
}
/// case insensitive string comparison
inline bool operator!=(const wstringi &i_str1, const wchar_t *i_str2)
{
	return i_str1.compare(i_str2) != 0;
}
/// case insensitive string comparison
inline bool operator!=(const std::wstring &i_str1, const wstringi &i_str2)
{
	return i_str2.compare(i_str1) != 0;
}
/// case insensitive string comparison
inline bool operator!=(const wstringi &i_str1, const std::wstring &i_str2)
{
	return i_str1.compare(i_str2) != 0;
}
/// case insensitive string comparison
inline bool operator!=(const wstringi &i_str1, const wstringi &i_str2)
{
	return i_str1.compare(i_str2) != 0;
}


/// stream output
extern std::wostream &operator<<(std::wostream &i_ost, const wregex_stored &i_data);

/// get lower string
extern std::wstring toLower(const std::wstring &i_str);

/** Spell out control characters in a window title so that they survive a
    single-font edit control: TAB, LF and CR by name, anything else as
    &lt;XX&gt; (or &lt;U+XXXX&gt; above 0xFF).

    The result is not only displayed, it is what "window /class/ /title/"
    matches against, so the spelling has to be typable in a regexp.  That rules
    out backslash escapes - these strings always carry backslashes from the
    module path, so "\n" could not be told apart from a literal one - and caret
    notation, since '^' is an anchor.  '&lt;' and '&gt;' are literal in
    ECMAScript regexps and practically never occur in a title.

    Every path that produces a title has to use this, or the same window would
    match differently depending on which path saw it. */
extern std::wstring escapeControlChars(const std::wstring &i_str);


#endif // !_STRINGTOOL_H
