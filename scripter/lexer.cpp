//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// lexer.cpp


#include "../misc.h"

#include "../errormessage.h"
#include "lexer.h"
#include <cassert>


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Token


Token::Token(const Token &i_token)
		: m_type(i_token.m_type),
		m_isValueQuoted(i_token.m_isValueQuoted),
		m_numericValue(i_token.m_numericValue),
		m_stringValue(i_token.m_stringValue),
		m_data(i_token.m_data)
{
}

Token::Token(int i_value, const wstringi &i_display)
		: m_type(Type::Number),
		m_isValueQuoted(false),
		m_numericValue(i_value),
		m_stringValue(i_display),
		m_data(NULL)
{
}

Token::Token(const wstringi &i_value, bool i_isValueQuoted, bool i_isRegexp)
		: m_type(i_isRegexp ? Type::Regexp : Type::String),
		m_isValueQuoted(i_isValueQuoted),
		m_numericValue(0),
		m_stringValue(i_value),
		m_data(NULL)
{
}

Token::Token(Type i_m_type)
		: m_type(i_m_type),
		m_isValueQuoted(false),
		m_numericValue(0),
		m_stringValue(L""),
		m_data(NULL)
{
	ASSERT(m_type == Type::OpenParen || m_type == Type::CloseParen ||
		   m_type == Type::Comma);
}

// get numeric value
int Token::getNumber() const
{
	if (m_type == Type::Number)
		return m_numericValue;
	if (m_stringValue.empty())
		return 0;
	else
		throw ErrorMessage() << L"`" << *this << L"' is not a number.";
}

// get string value
wstringi Token::getString() const
{
	if (m_type == Type::String)
		return m_stringValue;
	throw ErrorMessage() << L"`" << *this << L"' is not a string.";
}

// get regexp value
wstringi Token::getRegexp() const
{
	if (m_type == Type::Regexp)
		return m_stringValue;
	throw ErrorMessage() << L"`" << *this << L"' is not a regexp.";
}

// case insensitive equal
bool Token::operator==(const wchar_t *i_str) const
{
	if (m_type == Type::String)
		return m_stringValue == i_str;
	return false;
}

// paren equal
bool Token::operator==(const wchar_t i_c) const
{
	if (i_c == L'(') return m_type == Type::OpenParen;
	if (i_c == L')') return m_type == Type::CloseParen;
	return false;
}

// add string
void Token::add(const wstringi &i_str)
{
	m_stringValue += i_str;
}

// stream output
std::wostream &operator<<(std::wostream &i_ost, const Token &i_token)
{
	switch (i_token.m_type) {
	case Token::Type::String:
		i_ost << i_token.m_stringValue;
		break;
	case Token::Type::Number:
		i_ost << i_token.m_stringValue;
		break;
	case Token::Type::Regexp:
		i_ost << i_token.m_stringValue;
		break;
	case Token::Type::OpenParen:
		i_ost << L"(";
		break;
	case Token::Type::CloseParen:
		i_ost << L")";
		break;
	case Token::Type::Comma:
		i_ost << L", ";
		break;
	}
	return i_ost;
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Lexer


Lexer::Lexer(const wchar_t *i_str, size_t i_length)
		: m_lineNumber(1),
		m_prefixes(NULL),
		m_internalLineNumber(1),
		m_ptr(i_str),
		m_end(i_str + i_length)
{
}

// set string that may be prefix of a token.
// prefix_ is not copied, so it must be preserved after setPrefix()
void Lexer::setPrefixes(const Prefixes *i_prefixes)
{
	m_prefixes = i_prefixes;
}

// get a line
bool Lexer::getLine(wstringi *o_line)
{
	o_line->resize(0);

	if (m_ptr == m_end)
		return false;

	const wchar_t *begin = m_ptr;
	const wchar_t *end = m_end;

	// lines are separated by: "\r\n", "\n", "\x2028" (Unicode Line Separator)
	while (m_ptr != m_end)
		switch (*m_ptr) {
		case L'\n':
		case 0x2028:
			//case L'\x2028':	//  (U+2028)
			end = m_ptr;
			++ m_ptr;
			goto got_line_end;
		case L'\r':
			if (m_ptr + 1 != m_end && m_ptr[1] == L'\n') {
				end = m_ptr;
				m_ptr += 2;
				goto got_line_end;
			}
			[[fallthrough]];
		default:
			++ m_ptr;
			break;
		}
got_line_end:
	++ m_internalLineNumber;
	// o_line->assign(begin, end);		// why bcc cannot link this ?
	o_line->assign(begin, end - begin);		// workarond for bcc
	return true;
}

// symbol test
static bool isSymbolChar(wchar_t i_c)
{
	if (i_c == L'\0')
		return false;
	if (iswalpha(i_c) || iswdigit(i_c))
		return true;

	if (0x80 <= i_c && iswgraph(i_c))
		return true;

	if (iswpunct(i_c))
		return !!wcschr(L"-+/?_\\", i_c);

	// check arrows
	if (wcschr(L"\u2190\u2191\u2192\u2193", i_c)) {
		return true;
	}
	return iswgraph(i_c);
}


// get a parsed line.
// if no more lines exist, returns false
bool Lexer::getLine(std::vector<Token> *o_tokens)
{
	o_tokens->clear();
	m_lineNumber = m_internalLineNumber;

	wstringi line;
	bool isTokenExist = false;
continue_getLineLoop:
	while (getLine(&line)) {
		const wchar_t *t = line.c_str();

continue_getTokenLoop:
		while (true) {
			// skip white space
			while (*t != L'\0' && iswspace(*t))
				t ++;
			if (*t == L'\0' || *t == L'#')
				goto break_getTokenLoop; // no more tokens exist
			if (*t == L'\\' && *(t + 1) == L'\0')
				goto continue_getLineLoop; // continue to next line

			const wchar_t *tokenStart = t;

			// comma or empty token
			if (*t == L',') {
				if (!isTokenExist)
					o_tokens->push_back(Token(L"", false));
				isTokenExist = false;
				o_tokens->push_back(Token(Token::Type::Comma));
				t ++;
				goto continue_getTokenLoop;
			}

			// paren
			if (*t == L'(') {
				o_tokens->push_back(Token(Token::Type::OpenParen));
				isTokenExist = false;
				t ++;
				goto continue_getTokenLoop;
			}
			if (*t == L')') {
				if (!isTokenExist)
					o_tokens->push_back(Token(L"", false));
				isTokenExist = true;
				o_tokens->push_back(Token(Token::Type::CloseParen));
				t ++;
				goto continue_getTokenLoop;
			}

			isTokenExist = true;

			// prefix
			if (m_prefixes)
				for (size_t i = 0; i < m_prefixes->size(); i ++)
					if (_wcsnicmp(tokenStart, m_prefixes->at(i).c_str(),
								  m_prefixes->at(i).size()) == 0) {
						o_tokens->push_back(Token(m_prefixes->at(i), false));
						t += m_prefixes->at(i).size();
						goto continue_getTokenLoop;
					}

			// quoted or regexp
			if (*t == L'"' || *t == L'\'' ||
					*t == L'/' || (*t == L'\\' && *(t + 1) == L'm' &&
									  *(t + 2) != L'\0')) {
				bool isRegexp = !(*t == L'"' || *t == L'\'');
				wchar_t q[2] = { *t++, L'\0' }; // quote character
				if (q[0] == L'\\') {
					t++;
					q[0] = *t++;
				}
				tokenStart = t;

				while (*t != L'\0' && *t != q[0]) {
					if (*t == L'\\' && *(t + 1))
						t ++;
					t ++;
				}

				std::wstring str =
					interpretMetaCharacters(tokenStart, t - tokenStart, q, isRegexp);
				// concatinate continuous string
				if (!isRegexp &&
						0 < o_tokens->size() && o_tokens->back().isString() &&
						o_tokens->back().isQuoted())
					o_tokens->back().add(str);
				else
					o_tokens->push_back(Token(str, true, isRegexp));
				if (*t != L'\0')
					t ++;
				goto continue_getTokenLoop;
			}

			// not quoted
			{
				while (isSymbolChar(*t)) {
					if (*t == L'\\')
						if (*(t + 1))
							t ++;
						else
							break;
					t ++;
				}
				if (t == tokenStart) {
					ErrorMessage e;
					e << L"invalid character ";
					e << L"U+";
					e << std::hex; // << std::setw(4) << std::setfill(L'0');
					e << (int)(wchar_t)*t;
					e << std::dec;
					if (iswprint(*t))
						e << L"(" << *t << L")";
					throw e;
				}

				wchar_t *numEnd = NULL;
				long value = wcstol(tokenStart, &numEnd, 0);
				if (tokenStart == numEnd) {
					std::wstring str = interpretMetaCharacters(tokenStart, t - tokenStart);
					o_tokens->push_back(Token(str, false));
				} else {
					o_tokens->push_back(
						Token(value, wstringi(tokenStart, numEnd - tokenStart)));
					t = numEnd;
				}
				goto continue_getTokenLoop;
			}
		}
break_getTokenLoop:
		if (0 < o_tokens->size())
			break;
		m_lineNumber = m_internalLineNumber;
		isTokenExist = false;
	}

	return 0 < o_tokens->size();
}
