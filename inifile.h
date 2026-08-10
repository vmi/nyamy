//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// inifile.h


#ifndef _INIFILE_H
#  define _INIFILE_H

#  include "stringtool.h"
#  include <vector>


/// UTF-8 ini file access class (nyamy.ini in NYamyPaths::config()).
/// On first use, nyamy-ja.ini or nyamy-en.ini placed next to the executable
/// (whichever matches the user's default locale) is copied there as the
/// seed; afterwards only the per-user copy is used.
/// The file is read as UTF-8 (an optional BOM is skipped) and written as
/// UTF-8 with BOM, CRLF line endings.  Values live under the [nyamy] section;
/// comments (; or #), blank lines and unknown lines are preserved on write.
class IniFile
{
	std::wstring m_path;				/// full path of nyamy.ini

public:
	///
	IniFile();

	/// read int
	bool read(const std::wstring &i_name, int *o_value,
			  int i_defaultValue = 0) const;
	/// write int
	bool write(const std::wstring &i_name, int i_value) const;

	/// read std::wstring
	bool read(const std::wstring &i_name, std::wstring *o_value,
			  const std::wstring &i_defaultValue = L"") const;
	/// write std::wstring
	bool write(const std::wstring &i_name, const std::wstring &i_value) const;

	/// read LOGFONT
	bool read(const std::wstring &i_name, LOGFONT *o_value,
			  const std::wstring &i_defaultStringValue) const;
	/// write LOGFONT
	bool write(const std::wstring &i_name, const LOGFONT &i_value) const;

	/// read WINDOWPLACEMENT; o_value is left untouched when it is absent
	bool read(const std::wstring &i_name, WINDOWPLACEMENT *o_value) const;
	/// write WINDOWPLACEMENT
	bool write(const std::wstring &i_name, const WINDOWPLACEMENT &i_value) const;

	/// remove the value; returns false if it did not exist
	bool remove(const std::wstring &i_name) const;
};


#endif // !_INIFILE_H
