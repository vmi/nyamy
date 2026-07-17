//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// lexer.h


#ifndef _LEXER_H
#  define _LEXER_H

#  include "../misc.h"
#  include "../stringtool.h"
#  include <vector>


///
class Token
{
public:
	///
	enum class Type {
		String,				///
		Number,				///
		Regexp,				///
		OpenParen,				///
		CloseParen,				///
		Comma,					///
	};

private:
	Type m_type;				///
	bool m_isValueQuoted;				///
	int m_numericValue;				///
	wstringi m_stringValue;			///
	long m_data;					///

public:
	///
	Token(const Token &i_token);
	///
	Token(int i_value, const wstringi &i_display);
	///
	Token(const wstringi &i_value, bool i_isValueQuoted,
		  bool i_isRegexp = false);
	///
	Token(Type i_type);

	/// is the value quoted ?
	bool isQuoted() const {
		return m_isValueQuoted;
	}

	/// value type
	Type getType() const {
		return m_type;
	}
	///
	bool isString() const {
		return m_type == Type::String;
	}
	///
	bool isNumber() const {
		return m_type == Type::Number;
	}
	///
	bool isRegexp() const {
		return m_type == Type::Regexp;
	}
	///
	bool isOpenParen() const {
		return m_type == Type::OpenParen;
	}
	///
	bool isCloseParen() const {
		return m_type == Type::CloseParen;
	}
	///
	bool isComma() const {
		return m_type == Type::Comma;
	}

	/// get numeric value
	int getNumber() const;

	/// get string value
	wstringi getString() const;

	/// get regexp value
	wstringi getRegexp() const;

	/// get data
	long getData() const {
		return m_data;
	}
	///
	void setData(long i_data) {
		m_data = i_data;
	}

	/// case insensitive equal
	bool operator==(const wstringi &i_str) const {
		return *this == i_str.c_str();
	}
	///
	bool operator==(const wchar_t *i_str) const;
	///
	bool operator!=(const wstringi &i_str) const {
		return *this != i_str.c_str();
	}
	///
	bool operator!=(const wchar_t *i_str) const {
		return !(*this == i_str);
	}

	/** paren equal
	    @param i_c '<code>(</code>' or '<code>)</code>' */
	bool operator==(const wchar_t i_c) const;
	/** paren equal
	    @param i_c '<code>(</code>' or '<code>)</code>' */
	bool operator!=(const wchar_t i_c) const {
		return !(*this == i_c);
	}

	/// add string
	void add(const wstringi &i_str);

	/// stream output
	friend std::wostream &operator<<(std::wostream &i_ost, const Token &i_token);
};


///
class Lexer
{
public:
	///
	using Tokens = std::vector<Token>;

private:
	///
	using Prefixes = std::vector<wstringi>;

private:
	size_t m_lineNumber;				/// current line number
	const Prefixes *m_prefixes;			/** string that may be prefix
                                                    of a token */

	size_t m_internalLineNumber;			/// next line number
	const wchar_t *m_ptr;				/// read pointer
	const wchar_t *m_end;				/// end pointer

private:
	/// get a line
	bool getLine(wstringi *o_line);

public:
	///
	Lexer(const wchar_t *i_str, size_t i_length);

	/** get a parsed line.  if no more lines exist, returns false */
	bool getLine(Tokens *o_tokens);

	/// get current line number
	size_t getLineNumber() const {
		return m_lineNumber;
	}

	/** set string that may be prefix of a token.  prefix_ is not
	    copied, so it must be preserved after setPrefix() */
	void setPrefixes(const Prefixes *m_prefixes);
};


#endif // !_LEXER_H
