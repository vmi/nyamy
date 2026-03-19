//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// stringtool.cpp


#include "stringtool.h"
#include <vector>
#include <locale>
#include <malloc.h>
#include <mbstring.h>


/* ************************************************************************** *

STRLCPY(3)                OpenBSD Programmer's Manual               STRLCPY(3)

NAME
     strlcpy, strlcat - size-bounded string copying and concatenation



SYNOPSIS
     #include <string.h>

     size_t
     strlcpy(char *dst, const char *src, size_t size);

     size_t
     strlcat(char *dst, const char *src, size_t size);

DESCRIPTION
     The strlcpy() and strlcat() functions copy and concatenate strings re-
     spectively.  They are designed to be safer, more consistent, and less er-
     ror prone replacements for strncpy(3) and strncat(3).  Unlike those func-
     tions, strlcpy() and strlcat() take the full size of the buffer (not just
     the length) and guarantee to NUL-terminate the result (as long as size is
     larger than 0).  Note that you should include a byte for the NUL in size.

     The strlcpy() function copies up to size - 1 characters from the NUL-ter-
     minated string src to dst, NUL-terminating the result.

     The strlcat() function appends the NUL-terminated string src to the end
     of dst. It will append at most size - strlen(dst) - 1 bytes, NUL-termi-
     nating the result.

RETURN VALUES
     The strlcpy() and strlcat() functions return the total length of the
     string they tried to create.  For strlcpy() that means the length of src.
     For strlcat() that means the initial length of dst plus the length of
     src. While this may seem somewhat confusing it was done to make trunca-
     tion detection simple.

EXAMPLES
     The following code fragment illustrates the simple case:

           char *s, *p, buf[BUFSIZ];

           ...

           (void)strlcpy(buf, s, sizeof(buf));
           (void)strlcat(buf, p, sizeof(buf));

     To detect truncation, perhaps while building a pathname, something like
     the following might be used:

           char *dir, *file, pname[MAXPATHNAMELEN];

           ...

           if (strlcpy(pname, dir, sizeof(pname)) >= sizeof(pname))
                   goto toolong;
           if (strlcat(pname, file, sizeof(pname)) >= sizeof(pname))
                   goto toolong;

     Since we know how many characters we copied the first time, we can speed
     things up a bit by using a copy instead on an append:

           char *dir, *file, pname[MAXPATHNAMELEN];
           size_t n;

           ...

           n = strlcpy(pname, dir, sizeof(pname));
           if (n >= sizeof(pname))
                   goto toolong;
           if (strlcpy(pname + n, file, sizeof(pname) - n) >= sizeof(pname)-n)
                   goto toolong;

     However, one may question the validity of such optimizations, as they de-
     feat the whole purpose of strlcpy() and strlcat().  As a matter of fact,
     the first version of this manual page got it wrong.

SEE ALSO
     snprintf(3),  strncat(3),  strncpy(3)

OpenBSD 2.6                      June 22, 1998                               2


-------------------------------------------------------------------------------

Source: OpenBSD 2.6 man pages. Copyright: Portions are copyrighted by BERKELEY
SOFTWARE DESIGN, INC., The Regents of the University of California,
Massachusetts Institute of Technology, Free Software Foundation, FreeBSD Inc.,
and others.

* ************************************************************************** */


// copy
template <class T>
static inline size_t xstrlcpy(T *o_dest, const T *i_src, size_t i_destSize)
{
	T *d = o_dest;
	const T *s = i_src;
	size_t n = i_destSize;

	ASSERT( o_dest != NULL );
	ASSERT( i_src != NULL );

	// Copy as many bytes as will fit
	if (n != 0 && --n != 0) {
		do {
			if ((*d++ = *s++) == 0)
				break;
		} while (--n != 0);
	}

	// Not enough room in o_dest, add NUL and traverse rest of i_src
	if (n == 0) {
		if (i_destSize != 0)
			*d = T();					// NUL-terminate o_dest
		while (*s++)
			;
	}

	return (s - i_src - 1);			// count does not include NUL
}


// copy
size_t wcslcpy(wchar_t *o_dest, const wchar_t *i_src, size_t i_destSize)
{
	return xstrlcpy(o_dest, i_src, i_destSize);
}


/// stream output
std::wostream &operator<<(std::wostream &i_ost, const wstringq &i_data)
{
	i_ost << L"\"";
	for (const wchar_t *s = i_data.c_str(); *s; ++ s) {
		switch (*s) {
		case L'\a':
			i_ost << L"\\a";
			break;
		case L'\f':
			i_ost << L"\\f";
			break;
		case L'\n':
			i_ost << L"\\n";
			break;
		case L'\r':
			i_ost << L"\\r";
			break;
		case L'\t':
			i_ost << L"\\t";
			break;
		case L'\v':
			i_ost << L"\\v";
			break;
		case L'"':
			i_ost << L"\\\"";
			break;
		default:
	if (iswprint(*s)) {
				wchar_t buf[2] = { *s, 0 };
				i_ost << buf;
			} else {
				i_ost << L"\\x";
				wchar_t buf[5];
				_snwprintf(buf, NUMBER_OF(buf), L"%04x", *s);
				i_ost << buf;
			}
			break;
		}
	}
	i_ost << L"\"";
	return i_ost;
}


// interpret meta characters such as \n
std::wstring interpretMetaCharacters(const wchar_t *i_str, size_t i_len,
								const wchar_t *i_quote,
								bool i_doesUseRegexpBackReference)
{
	// interpreted string is always less than i_len
	std::vector<wchar_t> result(i_len + 1);
	// destination
	wchar_t *d = result.data();
	// end pointer
	const wchar_t *end = i_str + i_len;

	while (i_str < end && *i_str) {
		if (*i_str != L'\\') {
			*d++ = *i_str++;
		} else if (*(i_str + 1) != L'\0') {
			i_str ++;
			if (i_quote && wcschr(i_quote, *i_str))
				*d++ = *i_str++;
			else
				switch (*i_str) {
				case L'a':
					*d++ = L'\x07';
					i_str ++;
					break;
					//case L'b': *d++ = L'\b'; i_str ++; break;
				case L'e':
					*d++ = L'\x1b';
					i_str ++;
					break;
				case L'f':
					*d++ = L'\f';
					i_str ++;
					break;
				case L'n':
					*d++ = L'\n';
					i_str ++;
					break;
				case L'r':
					*d++ = L'\r';
					i_str ++;
					break;
				case L't':
					*d++ = L'\t';
					i_str ++;
					break;
				case L'v':
					*d++ = L'\v';
					i_str ++;
					break;
					//case L'?': *d++ = L'\x7f'; i_str ++; break;
					//case L'_': *d++ = L' '; i_str ++; break;
					//case L'\\': *d++ = L'\\'; i_str ++; break;
				case L'\'':
					*d++ = L'\'';
					i_str ++;
					break;
				case L'"':
					*d++ = L'"';
					i_str ++;
					break;
				case L'\\':
					*d++ = L'\\';
					i_str ++;
					break;
				case L'c': // control code, for example '\c[' is escape: '\x1b'
					i_str ++;
					if (i_str < end && *i_str) {
						static const wchar_t *ctrlchar =
							L"@ABCDEFGHIJKLMNO"
							L"PQRSTUVWXYZ[\\]^_"
							L"@abcdefghijklmno"
							L"pqrstuvwxyz@@@@?";
						static const wchar_t *ctrlcode =
							L"\00\01\02\03\04\05\06\07\10\11\12\13\14\15\16\17"
							L"\20\21\22\23\24\25\26\27\30\31\32\33\34\35\36\37"
							L"\00\01\02\03\04\05\06\07\10\11\12\13\14\15\16\17"
							L"\20\21\22\23\24\25\26\27\30\31\32\00\00\00\00\177";
						if (const wchar_t *c = wcschr(ctrlchar, *i_str))
							*d++ = ctrlcode[c - ctrlchar], i_str ++;
					}
					break;
				case L'x':
				case L'X': {
					i_str ++;
					static const wchar_t *hexchar = L"0123456789ABCDEFabcdef";
					static int hexvalue[] = { 0, 1, 2, 3, 4, 5 ,6, 7, 8, 9,
											  10, 11, 12, 13, 14, 15,
											  10, 11, 12, 13, 14, 15,
											};
					bool brace = false;
					if (i_str < end && *i_str == L'{') {
						i_str ++;
						brace = true;
					}
					int n = 0;
					for (; i_str < end && *i_str; i_str ++)
						if (const wchar_t *c = wcschr(hexchar, *i_str))
							n = n * 16 + hexvalue[c - hexchar];
						else
							break;
					if (i_str < end && *i_str == L'}' && brace)
						i_str ++;
					if (0 < n)
						*d++ = static_cast<wchar_t>(n);
					break;
				}
				case L'1':
				case L'2':
				case L'3':
				case L'4':
				case L'5':
				case L'6':
				case L'7':
					if (i_doesUseRegexpBackReference)
						goto case_default;
					// fall through
				case L'0': {
					static const wchar_t *octalchar = L"01234567";
					static int octalvalue[] = { 0, 1, 2, 3, 4, 5 ,6, 7, };
					int n = 0;
					for (; i_str < end && *i_str; i_str ++)
						if (const wchar_t *c = wcschr(octalchar, *i_str))
							n = n * 8 + octalvalue[c - octalchar];
						else
							break;
					if (0 < n)
						*d++ = static_cast<wchar_t>(n);
					break;
				}
				default:
case_default:
					*d++ = L'\\';
					*d++ = *i_str++;
					break;
				}
		}
	}
	*d =L'\0';
	return result.data();
}


// add session id to i_str
std::wstring addSessionId(const wchar_t *i_str)
{
	DWORD sessionId;
	std::wstring s(i_str);
	if (ProcessIdToSessionId(GetCurrentProcessId(), &sessionId)) {
		wchar_t buf[20];
		_snwprintf(buf, NUMBER_OF(buf), L"%u", sessionId);
		s += buf;
	}
	return s;
}


// converter
std::wstring to_wstring(const std::string &i_str)
{
	size_t size = mbstowcs(NULL, i_str.c_str(), i_str.size() + 1);
	if (size == (size_t)-1)
		return std::wstring();
	std::vector<wchar_t> result(size + 1);
	mbstowcs(result.data(), i_str.c_str(), i_str.size() + 1);
	return std::wstring(result.data());
}


// converter
std::string to_string(const std::wstring &i_str)
{
	size_t size = wcstombs(NULL, i_str.c_str(), i_str.size() + 1);
	if (size == (size_t)-1)
		return std::string();
	std::vector<char> result(size + 1);
	wcstombs(result.data(), i_str.c_str(), i_str.size() + 1);
	return std::string(result.data());
}


/// stream output
std::wostream &operator<<(std::wostream &i_ost, const wregex_stored &i_data)
{
	return i_ost << i_data.str();
}


/// get lower string
std::wstring toLower(const std::wstring &i_str)
{
	std::wstring str(i_str);
	for (size_t i = 0; i < str.size(); ++ i) {
		wchar_t c = str[i];
		if (c <= 0x007f)
			str[i] = static_cast<wchar_t>(tolower(c)); // ASCII only
	}
	return str;
}


// convert wstring to UTF-8
std::string to_UTF_8(const std::wstring &i_str)
{
	// 0xxxxxxx: 00-7F
	// 110xxxxx 10xxxxxx: 0080-07FF
	// 1110xxxx 10xxxxxx 10xxxxxx: 0800 - FFFF

	int size = 0;

	// count needed buffer size
	for (std::wstring::const_iterator i = i_str.begin(); i != i_str.end(); ++ i) {
		if (0x0000 <= *i && *i <= 0x007f)
			size += 1;
		else if (0x0080 <= *i && *i <= 0x07ff)
			size += 2;
		else if (0x0800 <= *i && *i <= 0xffff)
			size += 3;
	}

	std::vector<char> result(size);
	int ri = 0;

	// make UTF-8
	for (std::wstring::const_iterator i = i_str.begin(); i != i_str.end(); ++ i) {
		if (0x0000 <= *i && *i <= 0x007f)
			result[ri ++] = static_cast<char>(*i);
		else if (0x0080 <= *i && *i <= 0x07ff) {
			result[ri ++] = static_cast<char>(((*i & 0x0fc0) >>  6) | 0xc0);
			result[ri ++] = static_cast<char>(( *i & 0x003f       ) | 0x80);
		} else if (0x0800 <= *i && *i <= 0xffff) {
			result[ri ++] = static_cast<char>(((*i & 0xf000) >> 12) | 0xe0);
			result[ri ++] = static_cast<char>(((*i & 0x0fc0) >>  6) | 0x80);
			result[ri ++] = static_cast<char>(( *i & 0x003f       ) | 0x80);
		}
	}

	return std::string(result.begin(), result.end());
}
