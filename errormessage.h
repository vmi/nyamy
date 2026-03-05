//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// errormessage.h


#ifndef _ERRORMESSAGE_H
#  define _ERRORMESSAGE_H

#  include "stringtool.h"
#  include <sstream>


///
class ErrorMessage
{
	std::wstringstream m_ost;				///

public:
	///
	ErrorMessage() { }
	///
	ErrorMessage(const ErrorMessage &i_em) {
		m_ost << i_em.getMessage();
	}

	/// get error message
	std::wstring getMessage() const {
		return m_ost.str();
	}

	/// add message
	template<class T> ErrorMessage &operator<<(const T &i_value) {
		m_ost << i_value;
		return *this;
	}

	/// ios manipulator
	ErrorMessage &operator<<(
		std::ios_base &(*i_manip)(std::ios_base&)) {
		m_ost << i_manip;
		return *this;
	}

	/// add message
	template<> ErrorMessage &operator<<(const std::string &i_value) {
		m_ost << to_wstring(i_value);
		return *this;
	}

	/// add message
	using const_char_ptr = const char *;
	template<> ErrorMessage &operator<<(const const_char_ptr &i_value) {
		m_ost << to_wstring(i_value);
		return *this;
	}

	/// stream output
	friend std::wostream &operator<<(std::wostream &i_ost, const ErrorMessage &i_em);
};


/// stream output
inline std::wostream &operator<<(std::wostream &i_ost, const ErrorMessage &i_em)
{
	return i_ost << i_em.getMessage();
}


///
class WarningMessage : public ErrorMessage
{
public:
	/// add message
	template<class T> WarningMessage &operator<<(const T &i_value) {
		ErrorMessage::operator<<(i_value);
		return *this;
	}
};


#endif // !_ERRORMESSAGE_H
