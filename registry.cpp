//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// registry.cpp


#include "registry.h"
#include "stringtool.h"
#include <vector>
#include <malloc.h>


// remove
bool Registry::remove(HKEY i_root, const std::wstring &i_path,
					  const std::wstring &i_name)
{
	if (i_root) {
		if (i_name.empty())
			return RegDeleteKey(i_root, i_path.c_str()) == ERROR_SUCCESS;
		HKEY hkey;
		if (ERROR_SUCCESS != RegOpenKeyEx(i_root, i_path.c_str(), 0, KEY_SET_VALUE, &hkey))
			return false;
		LONG r = RegDeleteValue(hkey, i_name.c_str());
		RegCloseKey(hkey);
		return r == ERROR_SUCCESS;
	} else {
		return false;
		if (i_name.empty())
			return false;
		return WritePrivateProfileString(L"yamy", i_name.c_str(), NULL, i_path.c_str()) == TRUE;
	}
}


// does exist the key ?
bool Registry::doesExist(HKEY i_root, const std::wstring &i_path)
{
	if (i_root) {
		HKEY hkey;
		if (ERROR_SUCCESS != RegOpenKeyEx(i_root, i_path.c_str(), 0, KEY_READ, &hkey))
			return false;
		RegCloseKey(hkey);
		return true;
	} else {
		return true;
	}
}


// read DWORD
bool Registry::read(HKEY i_root, const std::wstring &i_path,
					const std::wstring &i_name, int *o_value, int i_defaultValue)
{
	if (i_root) {
		HKEY hkey;
		if (ERROR_SUCCESS == RegOpenKeyEx(i_root, i_path.c_str(), 0, KEY_READ, &hkey)) {
			DWORD type = REG_DWORD;
			DWORD size = sizeof(*o_value);
			LONG r = RegQueryValueEx(hkey, i_name.c_str(), NULL, &type, (BYTE *)o_value, &size);
			RegCloseKey(hkey);
			if (r == ERROR_SUCCESS)
				return true;
		}
		*o_value = i_defaultValue;
		return false;
	} else {
		*o_value = GetPrivateProfileInt(L"yamy", i_name.c_str(), i_defaultValue, i_path.c_str());
		return true;
	}
}


// write DWORD
bool Registry::write(HKEY i_root, const std::wstring &i_path, const std::wstring &i_name,
					 int i_value)
{
	if (i_root) {
		HKEY hkey;
		DWORD disposition;
		if (ERROR_SUCCESS !=
			RegCreateKeyEx(i_root, i_path.c_str(), 0, nullptr,
						   REG_OPTION_NON_VOLATILE,
						   KEY_ALL_ACCESS, NULL, &hkey, &disposition))
			return false;
		LONG r = RegSetValueEx(hkey, i_name.c_str(), NULL, REG_DWORD,
							   (BYTE *)&i_value, sizeof(i_value));
		RegCloseKey(hkey);
		return r == ERROR_SUCCESS;
	} else {
		DWORD ret;
		wchar_t buf[GANA_MAX_PATH];

		_swprintf(buf, L"%d", i_value);
		ret =  WritePrivateProfileString(L"yamy", i_name.c_str(),
										 buf, i_path.c_str());
		return ret != 0;
	}
}


// read string
bool Registry::read(HKEY i_root, const std::wstring &i_path, const std::wstring &i_name,
					std::wstring *o_value, const std::wstring &i_defaultValue)
{
	if (i_root) {
		HKEY hkey;
		if (ERROR_SUCCESS ==
				RegOpenKeyEx(i_root, i_path.c_str(), 0, KEY_READ, &hkey)) {
			DWORD type = REG_SZ;
			DWORD size = 0;
			BYTE dummy;
			if (ERROR_MORE_DATA ==
					RegQueryValueEx(hkey, i_name.c_str(), NULL, &type, &dummy, &size)) {
				if (0 < size) {
					std::vector<BYTE> buf(size);
					if (ERROR_SUCCESS == RegQueryValueEx(hkey, i_name.c_str(),
														 NULL, &type, buf.data(), &size)) {
						buf.back() = 0;
						*o_value = reinterpret_cast<wchar_t *>(buf.data());
						RegCloseKey(hkey);
						return true;
					}
				}
			}
			RegCloseKey(hkey);
		}
		if (!i_defaultValue.empty())
			*o_value = i_defaultValue;
		return false;
	} else {
		wchar_t buf[GANA_MAX_PATH];
		DWORD len;
		len = GetPrivateProfileString(L"yamy", i_name.c_str(), L"",
									  buf, sizeof(buf) / sizeof(buf[0]), i_path.c_str());
		if (len > 0) {
			*o_value = buf;
			return true;
		}
		if (!i_defaultValue.empty())
			*o_value = i_defaultValue;
		return false;
	}
}


// write string
bool Registry::write(HKEY i_root, const std::wstring &i_path,
					 const std::wstring &i_name, const std::wstring &i_value)
{
	if (i_root) {
		HKEY hkey;
		DWORD disposition;
		if (ERROR_SUCCESS !=
				RegCreateKeyEx(i_root, i_path.c_str(), 0, nullptr,
							   REG_OPTION_NON_VOLATILE,
							   KEY_ALL_ACCESS, NULL, &hkey, &disposition))
			return false;
		RegSetValueEx(hkey, i_name.c_str(), NULL, REG_SZ,
					  (BYTE *)i_value.c_str(),
					  (DWORD)((i_value.size() + 1) * sizeof(std::wstring::value_type)));
		RegCloseKey(hkey);
		return true;
	} else {
		DWORD ret;

		ret =  WritePrivateProfileString(L"yamy", i_name.c_str(),
										 i_value.c_str(), i_path.c_str());
		return ret != 0;
	}
}


#ifndef USE_INI
// read list of string
bool Registry::read(HKEY i_root, const std::wstring &i_path, const std::wstring &i_name,
					tstrings *o_value, const tstrings &i_defaultValue)
{
	HKEY hkey;
	if (ERROR_SUCCESS ==
			RegOpenKeyEx(i_root, i_path.c_str(), 0, KEY_READ, &hkey)) {
		DWORD type = REG_MULTI_SZ;
		DWORD size = 0;
		BYTE dummy;
		if (ERROR_MORE_DATA ==
				RegQueryValueEx(hkey, i_name.c_str(), NULL, &type, &dummy, &size)) {
			if (0 < size) {
				std::vector<BYTE> buf(size);
				if (ERROR_SUCCESS == RegQueryValueEx(hkey, i_name.c_str(),
													 NULL, &type, buf.data(), &size)) {
					buf.back() = 0;
					o_value->clear();
					const wchar_t *p = reinterpret_cast<wchar_t *>(buf.data());
					const wchar_t *end = reinterpret_cast<wchar_t *>(buf.data() + buf.size());
					while (p < end && *p) {
						o_value->push_back(p);
						p += o_value->back().length() + 1;
					}
					RegCloseKey(hkey);
					return true;
				}
			}
		}
		RegCloseKey(hkey);
	}
	if (!i_defaultValue.empty())
		*o_value = i_defaultValue;
	return false;
}


// write list of string
bool Registry::write(HKEY i_root, const std::wstring &i_path,
					 const std::wstring &i_name, const tstrings &i_value)
{
	HKEY hkey;
	DWORD disposition;
	if (ERROR_SUCCESS !=
			RegCreateKeyEx(i_root, i_path.c_str(), 0, L"",
						   REG_OPTION_NON_VOLATILE,
						   KEY_ALL_ACCESS, NULL, &hkey, &disposition))
		return false;
	std::wstring value;
	for (tstrings::const_iterator i = i_value.begin(); i != i_value.end(); ++ i) {
		value += *i;
		value += L'\0';
	}
	RegSetValueEx(hkey, i_name.c_str(), NULL, REG_MULTI_SZ,
				  (BYTE *)value.c_str(),
				  (value.size() + 1) * sizeof(std::wstring::value_type));
	RegCloseKey(hkey);
	return true;
}
#endif //!USE_INI


// read binary
bool Registry::read(HKEY i_root, const std::wstring &i_path,
					const std::wstring &i_name, BYTE *o_value, DWORD *i_valueSize,
					const BYTE *i_defaultValue, DWORD i_defaultValueSize)
{
	if (i_root) {
		if (i_valueSize) {
			HKEY hkey;
			if (ERROR_SUCCESS ==
					RegOpenKeyEx(i_root, i_path.c_str(), 0, KEY_READ, &hkey)) {
				DWORD type = REG_BINARY;
				LONG r = RegQueryValueEx(hkey, i_name.c_str(), NULL, &type,
										 (BYTE *)o_value, i_valueSize);
				RegCloseKey(hkey);
				if (r == ERROR_SUCCESS)
					return true;
			}
		}
		if (i_defaultValue)
			CopyMemory(o_value, i_defaultValue,
					   MIN(i_defaultValueSize, *i_valueSize));
		return false;
	} else {
		return false;
	}
}


// write binary
bool Registry::write(HKEY i_root, const std::wstring &i_path, const std::wstring &i_name,
					 const BYTE *i_value, DWORD i_valueSize)
{
	if (i_root) {
		if (!i_value)
			return false;
		HKEY hkey;
		DWORD disposition;
		if (ERROR_SUCCESS !=
				RegCreateKeyEx(i_root, i_path.c_str(), 0, nullptr,
							   REG_OPTION_NON_VOLATILE,
							   KEY_ALL_ACCESS, NULL, &hkey, &disposition))
			return false;
		RegSetValueEx(hkey, i_name.c_str(), NULL, REG_BINARY, i_value, i_valueSize);
		RegCloseKey(hkey);
		return true;
	} else {
		return false;
	}
}


//
static bool string2logfont(LOGFONT *o_lf, const std::wstring &i_strlf)
{
	// -13,0,0,0,400,0,0,0,128,1,2,1,1,Terminal
	wregex_stored lf(L"^(-?\\d+),(-?\\d+),(-?\\d+),(-?\\d+),(-?\\d+),"
			  L"(-?\\d+),(-?\\d+),(-?\\d+),(-?\\d+),(-?\\d+),"
			  L"(-?\\d+),(-?\\d+),(-?\\d+),(.+)$");
	std::wsmatch what;

	if (!std::regex_match(i_strlf, what, lf))
		return false;
	o_lf->lfHeight         =       _wtoi(what.str(1).c_str());
	o_lf->lfWidth          =       _wtoi(what.str(2).c_str());
	o_lf->lfEscapement     =       _wtoi(what.str(3).c_str());
	o_lf->lfOrientation    =       _wtoi(what.str(4).c_str());
	o_lf->lfWeight         =       _wtoi(what.str(5).c_str());
	o_lf->lfItalic         = (BYTE)_wtoi(what.str(6).c_str());
	o_lf->lfUnderline      = (BYTE)_wtoi(what.str(7).c_str());
	o_lf->lfStrikeOut      = (BYTE)_wtoi(what.str(8).c_str());
	o_lf->lfCharSet        = (BYTE)_wtoi(what.str(9).c_str());
	o_lf->lfOutPrecision   = (BYTE)_wtoi(what.str(10).c_str());
	o_lf->lfClipPrecision  = (BYTE)_wtoi(what.str(11).c_str());
	o_lf->lfQuality        = (BYTE)_wtoi(what.str(12).c_str());
	o_lf->lfPitchAndFamily = (BYTE)_wtoi(what.str(13).c_str());
	tcslcpy(o_lf->lfFaceName, what.str(14).c_str(), NUMBER_OF(o_lf->lfFaceName));
	return true;
}


// read LOGFONT
bool Registry::read(HKEY i_root, const std::wstring &i_path, const std::wstring &i_name,
					LOGFONT *o_value, const std::wstring &i_defaultStringValue)
{
	std::wstring buf;
	if (!read(i_root, i_path, i_name, &buf) || !string2logfont(o_value, buf)) {
		if (!i_defaultStringValue.empty())
			string2logfont(o_value, i_defaultStringValue);
		return false;
	}
	return true;
}


// write LOGFONT
bool Registry::write(HKEY i_root, const std::wstring &i_path, const std::wstring &i_name,
					 const LOGFONT &i_value)
{
	wchar_t buf[1024];
	_snwprintf(buf, NUMBER_OF(buf),
			   L"%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%s",
			   i_value.lfHeight, i_value.lfWidth, i_value.lfEscapement,
			   i_value.lfOrientation, i_value.lfWeight, i_value.lfItalic,
			   i_value.lfUnderline, i_value.lfStrikeOut, i_value.lfCharSet,
			   i_value.lfOutPrecision, i_value.lfClipPrecision,
			   i_value.lfQuality,
			   i_value.lfPitchAndFamily, i_value.lfFaceName);
	return Registry::write(i_root, i_path, i_name, buf);
}
