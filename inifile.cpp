//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// inifile.cpp


#include "misc.h"
#include "inifile.h"
#include "nyamy_paths.h"
#include <fstream>


namespace
{

const wchar_t SECTION_NAME[] = L"nyamy";


//
std::wstring trim(const std::wstring &i_str)
{
	size_t b = i_str.find_first_not_of(L" \t");
	if (b == std::wstring::npos)
		return std::wstring();
	size_t e = i_str.find_last_not_of(L" \t");
	return i_str.substr(b, e - b + 1);
}


// Windows GetPrivateProfileString() convention: a value whose first and
// last characters are a matching pair of quotes (' or ") has the quotes
// discarded.  A lone quote, or quotes appearing only at one end, are left
// as-is (matches documented GetPrivateProfileString behavior).
std::wstring stripQuotes(const std::wstring &i_value)
{
	if (i_value.size() >= 2) {
		wchar_t q = i_value.front();
		if ((q == L'"' || q == L'\'') && i_value.back() == q)
			return i_value.substr(1, i_value.size() - 2);
	}
	return i_value;
}


// is the line a section header ?  if so, extract its name
bool parseSectionHeader(const std::wstring &i_line, std::wstring *o_name)
{
	std::wstring t = trim(i_line);
	if (t.size() < 2 || t[0] != L'[')
		return false;
	size_t close = t.find(L']');
	if (close == std::wstring::npos)
		return false;
	*o_name = trim(t.substr(1, close - 1));
	return true;
}


// is the line a "key=value" line ?  comments (; #) are not
bool parseKeyValue(const std::wstring &i_line,
				   std::wstring *o_key, std::wstring *o_value)
{
	std::wstring t = trim(i_line);
	if (t.empty() || t[0] == L';' || t[0] == L'#' || t[0] == L'[')
		return false;
	size_t eq = t.find(L'=');
	if (eq == std::wstring::npos)
		return false;
	*o_key = trim(t.substr(0, eq));
	*o_value = stripQuotes(trim(t.substr(eq + 1)));
	return !o_key->empty();
}


// load the whole file as UTF-8 lines (skipping a BOM if any)
std::vector<std::wstring> loadLines(const std::wstring &i_path)
{
	std::vector<std::wstring> lines;
	std::ifstream f(i_path.c_str(), std::ios::binary);
	if (!f)
		return lines;
	std::string content((std::istreambuf_iterator<char>(f)),
						std::istreambuf_iterator<char>());
	if (content.size() >= 3 &&
			content[0] == '\xef' && content[1] == '\xbb' && content[2] == '\xbf')
		content.erase(0, 3);

	size_t begin = 0;
	while (begin <= content.size()) {
		size_t end = content.find('\n', begin);
		std::string line;
		if (end == std::string::npos) {
			if (begin == content.size())
				break;					// no trailing empty line
			line = content.substr(begin);
			begin = content.size() + 1;
		} else {
			line = content.substr(begin, end - begin);
			begin = end + 1;
		}
		if (!line.empty() && line.back() == '\r')
			line.pop_back();
		lines.push_back(from_UTF8(line));
	}
	return lines;
}


// write the lines back as UTF-8 with BOM, CRLF line endings
bool saveLines(const std::wstring &i_path, const std::vector<std::wstring> &i_lines)
{
	std::string content("\xef\xbb\xbf");
	for (const std::wstring &line : i_lines) {
		content += to_UTF8(line);
		content += "\r\n";
	}
	std::ofstream f(i_path.c_str(), std::ios::binary | std::ios::trunc);
	if (!f)
		return false;
	f.write(content.data(), content.size());
	return !f.fail();
}


// find the "key=value" line of i_name inside [nyamy].
// returns the line index or -1.  when o_sectionEnd is non-NULL it receives
// the index just after the last non-blank line of the section (insertion
// point for new keys), or -1 if the section itself is missing.
int findKeyLine(const std::vector<std::wstring> &i_lines,
				const std::wstring &i_name, int *o_sectionEnd)
{
	bool inSection = false;
	int sectionEnd = -1;
	int found = -1;
	for (size_t i = 0; i < i_lines.size(); ++ i) {
		std::wstring sectionName;
		if (parseSectionHeader(i_lines[i], &sectionName)) {
			if (inSection)
				break;
			inSection = (_wcsicmp(sectionName.c_str(), SECTION_NAME) == 0);
			if (inSection)
				sectionEnd = static_cast<int>(i) + 1;
			continue;
		}
		if (!inSection)
			continue;
		if (!trim(i_lines[i]).empty())
			sectionEnd = static_cast<int>(i) + 1;
		std::wstring key, value;
		if (parseKeyValue(i_lines[i], &key, &value) &&
				_wcsicmp(key.c_str(), i_name.c_str()) == 0) {
			found = static_cast<int>(i);
			break;
		}
	}
	if (o_sectionEnd)
		*o_sectionEnd = sectionEnd;
	return found;
}

} // anonymous namespace


//
IniFile::IniFile()
{
	// seed: nyamy-ja.ini or nyamy-en.ini next to the executable, picked by
	// the user's default locale (read on first use only).  This mirrors the
	// LANGUAGE blocks in mayu.rc, which fall back on the same locale.
	const wchar_t *seedName =
		PRIMARYLANGID(GetUserDefaultLangID()) == LANG_JAPANESE ?
			L"\\nyamy-ja.ini" : L"\\nyamy-en.ini";
	std::wstring seedPath = NYamyPaths::root() + seedName;
	m_path = NYamyPaths::config() + L"\\nyamy.ini";

	// first use: copy the locale-appropriate seed into place, if any.  The
	// seed and the per-user copy always have different file names, so this
	// also works when there is no per-user directory to use (config == root).
	if (GetFileAttributesW(m_path.c_str()) == INVALID_FILE_ATTRIBUTES &&
			GetFileAttributesW(seedPath.c_str()) != INVALID_FILE_ATTRIBUTES)
		CopyFileW(seedPath.c_str(), m_path.c_str(), TRUE);
}


// read int
bool IniFile::read(const std::wstring &i_name, int *o_value,
				   int i_defaultValue) const
{
	std::wstring buf;
	if (read(i_name, &buf)) {
		*o_value = _wtoi(buf.c_str());
		return true;
	}
	*o_value = i_defaultValue;
	return false;
}


// write int
bool IniFile::write(const std::wstring &i_name, int i_value) const
{
	wchar_t buf[32];
	_snwprintf(buf, NUMBER_OF(buf), L"%d", i_value);
	return write(i_name, std::wstring(buf));
}


// read string
bool IniFile::read(const std::wstring &i_name, std::wstring *o_value,
				   const std::wstring &i_defaultValue) const
{
	std::vector<std::wstring> lines = loadLines(m_path);
	int index = findKeyLine(lines, i_name, NULL);
	if (0 <= index) {
		std::wstring key, value;
		parseKeyValue(lines[index], &key, &value);
		if (!value.empty()) {
			*o_value = value;
			return true;
		}
	}
	if (!i_defaultValue.empty())
		*o_value = i_defaultValue;
	return false;
}


// write string
bool IniFile::write(const std::wstring &i_name, const std::wstring &i_value) const
{
	std::vector<std::wstring> lines = loadLines(m_path);
	std::wstring newLine = i_name + L"=" + i_value;
	int sectionEnd;
	int index = findKeyLine(lines, i_name, &sectionEnd);
	if (0 <= index) {
		lines[index] = newLine;
	} else if (0 <= sectionEnd) {
		lines.insert(lines.begin() + sectionEnd, newLine);
	} else {
		lines.push_back(std::wstring(L"[") + SECTION_NAME + L"]");
		lines.push_back(newLine);
	}
	return saveLines(m_path, lines);
}


// remove
bool IniFile::remove(const std::wstring &i_name) const
{
	std::vector<std::wstring> lines = loadLines(m_path);
	int index = findKeyLine(lines, i_name, NULL);
	if (index < 0)
		return false;
	lines.erase(lines.begin() + index);
	return saveLines(m_path, lines);
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
	wcslcpy(o_lf->lfFaceName, what.str(14).c_str(), NUMBER_OF(o_lf->lfFaceName));
	return true;
}


// read LOGFONT
bool IniFile::read(const std::wstring &i_name, LOGFONT *o_value,
				   const std::wstring &i_defaultStringValue) const
{
	std::wstring buf;
	if (!read(i_name, &buf) || !string2logfont(o_value, buf)) {
		if (!i_defaultStringValue.empty())
			string2logfont(o_value, i_defaultStringValue);
		return false;
	}
	return true;
}


// read WINDOWPLACEMENT
//
// Only showCmd and rcNormalPosition are stored: the rest of the structure is
// either derived (rcNormalPosition already is in workspace coordinates, so the
// taskbar needs no accounting) or meaningless once the window is gone.
bool IniFile::read(const std::wstring &i_name, WINDOWPLACEMENT *o_value) const
{
	std::wstring buf;
	if (!read(i_name, &buf))
		return false;

	wregex_stored re(L"^(-?\\d+),(-?\\d+),(-?\\d+),(-?\\d+),(-?\\d+)$");
	std::wsmatch what;
	if (!std::regex_match(buf, what, re))
		return false;

	o_value->length = sizeof(*o_value);
	o_value->flags = 0;
	o_value->showCmd = static_cast<UINT>(_wtoi(what.str(1).c_str()));
	o_value->rcNormalPosition.left   = _wtoi(what.str(2).c_str());
	o_value->rcNormalPosition.top    = _wtoi(what.str(3).c_str());
	o_value->rcNormalPosition.right  = _wtoi(what.str(4).c_str());
	o_value->rcNormalPosition.bottom = _wtoi(what.str(5).c_str());
	if (o_value->rcNormalPosition.right <= o_value->rcNormalPosition.left ||
			o_value->rcNormalPosition.bottom <= o_value->rcNormalPosition.top)
		return false;
	return true;
}


// write WINDOWPLACEMENT
bool IniFile::write(const std::wstring &i_name,
					const WINDOWPLACEMENT &i_value) const
{
	wchar_t buf[128];
	_snwprintf(buf, NUMBER_OF(buf), L"%u,%d,%d,%d,%d",
			   i_value.showCmd,
			   i_value.rcNormalPosition.left, i_value.rcNormalPosition.top,
			   i_value.rcNormalPosition.right, i_value.rcNormalPosition.bottom);
	return write(i_name, std::wstring(buf));
}


// write LOGFONT
bool IniFile::write(const std::wstring &i_name, const LOGFONT &i_value) const
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
	return write(i_name, std::wstring(buf));
}
