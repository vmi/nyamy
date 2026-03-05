//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// registry.h


#ifndef _REGISTRY_H
#  define _REGISTRY_H

#  include "stringtool.h"
#  include <list>


/// registry access class
class Registry
{
	HKEY m_root;					/// registry root
	std::wstring m_path;				/// path from registry root

public:
	using tstrings = std::list<std::wstring>;

public:
	///
	Registry() : m_root(NULL) {
		setRoot(NULL, L"");
	}
	///
	Registry(HKEY i_root, const std::wstring &i_path)
			: m_root(i_root), m_path(i_path) {
		setRoot(i_root, i_path);
	}

	/// set registry root and path
	void setRoot(HKEY i_root, const std::wstring &i_path) {
		m_root = i_root;
		if (m_root) {
			m_path = i_path;
		} else {
			wchar_t exePath[GANA_MAX_PATH];
			wchar_t exeDrive[GANA_MAX_PATH];
			wchar_t exeDir[GANA_MAX_PATH];
			GetModuleFileName(NULL, exePath, GANA_MAX_PATH);
			_wsplitpath_s(exePath, exeDrive, GANA_MAX_PATH, exeDir, GANA_MAX_PATH, NULL, 0, NULL, 0);
			m_path = exeDrive;
			m_path += exeDir;
			m_path += L"yamy.ini";
		}
	}

	/// remvoe
	bool remove(const std::wstring &i_name = L"") const {
		return remove(m_root, m_path, i_name);
	}

	/// does exist the key ?
	bool doesExist() const {
		return doesExist(m_root, m_path);
	}

	/// read DWORD
	bool read(const std::wstring &i_name, int *o_value, int i_defaultValue = 0)
	const {
		return read(m_root, m_path, i_name, o_value, i_defaultValue);
	}
	/// write DWORD
	bool write(const std::wstring &i_name, int i_value) const {
		return write(m_root, m_path, i_name, i_value);
	}

	/// read std::wstring
	bool read(const std::wstring &i_name, std::wstring *o_value,
			  const std::wstring &i_defaultValue = L"") const {
		return read(m_root, m_path, i_name, o_value, i_defaultValue);
	}
	/// write std::wstring
	bool write(const std::wstring &i_name, const std::wstring &i_value) const {
		return write(m_root, m_path, i_name, i_value);
	}

#ifndef USE_INI
	/// read list of std::wstring
	bool read(const std::wstring &i_name, tstrings *o_value,
			  const tstrings &i_defaultValue = tstrings()) const {
		return read(m_root, m_path, i_name, o_value, i_defaultValue);
	}
	/// write list of std::wstring
	bool write(const std::wstring &i_name, const tstrings &i_value) const {
		return write(m_root, m_path, i_name, i_value);
	}
#endif //!USE_INI

	/// read binary data
	bool read(const std::wstring &i_name, BYTE *o_value, DWORD *i_valueSize,
			  const BYTE *i_defaultValue = NULL, DWORD i_defaultValueSize = 0)
	const {
		return read(m_root, m_path, i_name, o_value, i_valueSize, i_defaultValue,
					i_defaultValueSize);
	}
	/// write binary data
	bool write(const std::wstring &i_name, const BYTE *i_value,
			   DWORD i_valueSize) const {
		return write(m_root, m_path, i_name, i_value, i_valueSize);
	}

public:
	/// remove
	static bool remove(HKEY i_root, const std::wstring &i_path,
					   const std::wstring &i_name = L"");

	/// does exist the key ?
	static bool doesExist(HKEY i_root, const std::wstring &i_path);

	/// read DWORD
	static bool read(HKEY i_root, const std::wstring &i_path, const std::wstring &i_name,
					 int *o_value, int i_defaultValue = 0);
	/// write DWORD
	static bool write(HKEY i_root, const std::wstring &i_path, const std::wstring &i_name,
					  int i_value);

	/// read std::wstring
	static bool read(HKEY i_root, const std::wstring &i_path, const std::wstring &i_name,
					 std::wstring *o_value, const std::wstring &i_defaultValue = L"");
	/// write std::wstring
	static bool write(HKEY i_root, const std::wstring &i_path, const std::wstring &i_name,
					  const std::wstring &i_value);

#ifndef USE_INI
	/// read list of std::wstring
	static bool read(HKEY i_root, const std::wstring &i_path, const std::wstring &i_name,
					 tstrings *o_value, const tstrings &i_defaultValue = tstrings());
	/// write list of std::wstring
	static bool write(HKEY i_root, const std::wstring &i_path, const std::wstring &i_name,
					  const tstrings &i_value);
#endif //!USE_INI

	/// read binary data
	static bool read(HKEY i_root, const std::wstring &i_path, const std::wstring &i_name,
					 BYTE *o_value, DWORD *i_valueSize,
					 const BYTE *i_defaultValue = NULL,
					 DWORD i_defaultValueSize = 0);
	/// write binary data
	static bool write(HKEY i_root, const std::wstring &i_path, const std::wstring &i_name,
					  const BYTE *i_value, DWORD i_valueSize);
	/// read LOGFONT
	static bool read(HKEY i_root, const std::wstring &i_path, const std::wstring &i_name,
					 LOGFONT *o_value, const std::wstring &i_defaultStringValue);
	/// write LOGFONT
	static bool write(HKEY i_root, const std::wstring &i_path, const std::wstring &i_name,
					  const LOGFONT &i_value);
};


#endif // !_REGISTRY_H
