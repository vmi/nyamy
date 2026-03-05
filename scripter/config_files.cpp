//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// config_files.cpp


#include "../misc.h"

#include "config_files.h"
#include "../mayu.h"
#include "../registry.h"
#include "../windowstool.h"
#include "../multithread.h"
#include <fstream>
#include <sys/stat.h>


// constructor
ConfigFiles::ConfigFiles(SyncObject *i_soLog, std::wostream *i_log)
		: m_soLog(i_soLog),
		m_log(i_log)
{
}


// get mayu filename from registry
bool ConfigFiles::getFilenameFromRegistry(
	wstringi *o_name, wstringi *o_filename, Symbols *o_symbols) const
{
	Registry reg(MAYU_REGISTRY_ROOT);
	int index;
	reg.read(L".mayuIndex", &index, 0);
	wchar_t buf[100];
	_snwprintf(buf, NUMBER_OF(buf), L".mayu%d", index);

	wstringi entry;
	if (!reg.read(buf, &entry))
		return false;

	wregex_stored getFilename(L"^([^;]*);([^;]*);(.*)$");
	std::wsmatch getFilenameResult;
	if (!std::regex_match(entry, getFilenameResult, getFilename))
		return false;

	if (o_name)
		*o_name = getFilenameResult.str(1);
	if (o_filename)
		*o_filename = getFilenameResult.str(2);
	if (o_symbols) {
		wstringi symbols = getFilenameResult.str(3);
		wregex_stored symbol(L"-D([^;]*)(.*)$");
		std::wsmatch symbolResult;
		while (std::regex_search(symbols, symbolResult, symbol)) {
			o_symbols->insert(symbolResult.str(1));
			symbols = symbolResult.str(2);
		}
	}
	return true;
}


// get home directory path
void ConfigFiles::getHomeDirectories(HomeDirectories *o_pathes) const
{
	wstringi filename;
#ifndef USE_INI
	if (getFilenameFromRegistry(NULL, &filename, NULL) &&
			!filename.empty()) {
		wregex_stored getPath(L"^(.*[/\\\\])[^/\\\\]*$");
		std::wsmatch getPathResult;
		if (std::regex_match(filename, getPathResult, getPath))
			o_pathes->push_back(getPathResult.str(1));
	}

	const wchar_t *home = _wgetenv(L"HOME");
	if (home)
		o_pathes->push_back(home);

	const wchar_t *homedrive = _wgetenv(L"HOMEDRIVE");
	const wchar_t *homepath = _wgetenv(L"HOMEPATH");
	if (homedrive && homepath)
		o_pathes->push_back(wstringi(homedrive) + homepath);

	const wchar_t *userprofile = _wgetenv(L"USERPROFILE");
	if (userprofile)
		o_pathes->push_back(userprofile);

	wchar_t buf[GANA_MAX_PATH];
	DWORD len = GetCurrentDirectory(NUMBER_OF(buf), buf);
	if (0 < len && len < NUMBER_OF(buf))
		o_pathes->push_back(buf);
#else //USE_INI
	wchar_t buf[GANA_MAX_PATH];
#endif //USE_INI

	if (GetModuleFileName(GetModuleHandle(NULL), buf, NUMBER_OF(buf)))
		o_pathes->push_back(pathRemoveFileSpec(buf));
}


// read file contents
bool ConfigFiles::readFile(std::wstring *o_data, const wstringi &i_filename) const
{
	// get size of file
	struct _stati64 sbuf;
	if (_wstati64(i_filename.c_str(), &sbuf) < 0 || sbuf.st_size == 0)
		return false;
	// following check is needed to cast sbuf.st_size to size_t safely
	// this cast occurs because of above workaround for bcc
	if (sbuf.st_size > UINT_MAX)
		return false;

	// open
	FILE *fp = _wfopen(i_filename.c_str(), L"rb");
	if (!fp)
		return false;

	// read file
	std::vector<BYTE> buf(static_cast<size_t>(sbuf.st_size) + 1);
	if (fread(buf.data(), static_cast<size_t>(sbuf.st_size), 1, fp) != 1) {
		fclose(fp);
		return false;
	}
	buf[static_cast<size_t>(sbuf.st_size)] = 0;			// mbstowcs() requires null
	// terminated string

	//
	if (buf[0] == 0xffU && buf[1] == 0xfeU &&
			sbuf.st_size % 2 == 0)
		// UTF-16 Little Endien
	{
		size_t size = static_cast<size_t>(sbuf.st_size) / 2;
		o_data->resize(size);
		BYTE *p = buf.data();
		for (size_t i = 0; i < size; ++ i) {
			wchar_t c = static_cast<wchar_t>(*p ++);
			c |= static_cast<wchar_t>(*p ++) << 8;
			(*o_data)[i] = c;
		}
		fclose(fp);
		return true;
	}

	//
	if (buf[0] == 0xfeU && buf[1] == 0xffU &&
			sbuf.st_size % 2 == 0)
		// UTF-16 Big Endien
	{
		size_t size = static_cast<size_t>(sbuf.st_size) / 2;
		o_data->resize(size);
		BYTE *p = buf.data();
		for (size_t i = 0; i < size; ++ i) {
			wchar_t c = static_cast<wchar_t>(*p ++) << 8;
			c |= static_cast<wchar_t>(*p ++);
			(*o_data)[i] = c;
		}
		fclose(fp);
		return true;
	}

	// try UTF-8
	{
		std::vector<wchar_t> wbuf(static_cast<size_t>(sbuf.st_size));
		BYTE *f = buf.data();
		BYTE *end = buf.data() + sbuf.st_size;
		// skip UTF-8 BOM (EF BB BF)
		if (end - f >= 3 && f[0] == 0xefU && f[1] == 0xbbU && f[2] == 0xbfU)
			f += 3;
		wchar_t *d = wbuf.data();
		enum { STATE_1, STATE_2of2, STATE_2of3, STATE_3of3 } state = STATE_1;

		while (f != end) {
			switch (state) {
			case STATE_1:
				if (!(*f & 0x80))			// 0xxxxxxx: 00-7F
					*d++ = static_cast<wchar_t>(*f++);
				else if ((*f & 0xe0) == 0xc0) {	// 110xxxxx 10xxxxxx: 0080-07FF
					*d = ((static_cast<wchar_t>(*f++) & 0x1f) << 6);
					state = STATE_2of2;
				} else if ((*f & 0xf0) == 0xe0)		// 1110xxxx 10xxxxxx 10xxxxxx:
					// 0800 - FFFF
				{
					*d = ((static_cast<wchar_t>(*f++) & 0x0f) << 12);
					state = STATE_2of3;
				} else
					goto not_UTF_8;
				break;

			case STATE_2of2:
			case STATE_3of3:
				if ((*f & 0xc0) != 0x80)
					goto not_UTF_8;
				*d++ |= (static_cast<wchar_t>(*f++) & 0x3f);
				state = STATE_1;
				break;

			case STATE_2of3:
				if ((*f & 0xc0) != 0x80)
					goto not_UTF_8;
				*d |= ((static_cast<wchar_t>(*f++) & 0x3f) << 6);
				state = STATE_3of3;
				break;
			}
		}
		o_data->assign(wbuf.data(), d);
		fclose(fp);
		return true;

not_UTF_8:
		;
	}

	// try multibyte charset
	size_t wsize = mbstowcs(NULL, reinterpret_cast<char *>(buf.data()), 0);
	if (wsize != size_t(-1)) {
		std::vector<wchar_t> wbuf(wsize);
		mbstowcs(wbuf.data(), reinterpret_cast<char *>(buf.data()), wsize);
		o_data->assign(wbuf.data(), wbuf.data() + wsize);
		fclose(fp);
		return true;
	}

	// assume ascii
	o_data->resize(static_cast<size_t>(sbuf.st_size));
	for (off_t i = 0; i < sbuf.st_size; ++ i)
		(*o_data)[i] = buf[i];
	fclose(fp);
	return true;
}


// is the filename readable ?
bool ConfigFiles::isReadable(const wstringi &i_filename,
							 int i_debugLevel) const
{
	if (i_filename.empty())
		return false;
	std::wifstream ist(to_string(i_filename).c_str());
	if (ist.good()) {
		if (m_log && m_soLog) {
			Acquire a(m_soLog, 0);
			*m_log << L"  loading: " << i_filename << std::endl;
		}
		return true;
	} else {
		if (m_log && m_soLog) {
			Acquire a(m_soLog, i_debugLevel);
			*m_log << L"not found: " << i_filename << std::endl;
		}
		return false;
	}
}


// get filename
bool ConfigFiles::getFilename(const wstringi &i_name, wstringi *o_path,
							  Symbols *o_symbols,
							  RetryCallback i_retry,
							  int i_debugLevel) const
{
	// the default filename is ".mayu"
	const wstringi &name = i_name.empty() ? wstringi(L".mayu") : i_name;

	bool isFirstTime = true;

	while (true) {
		// find file from registry
		if (i_name.empty()) {			// called not from 'include'
			Symbols symbols;
			if (getFilenameFromRegistry(NULL, o_path, &symbols)) {
				if (o_path->empty())
					// find file from home directory
				{
					HomeDirectories pathes;
					getHomeDirectories(&pathes);
					for (HomeDirectories::iterator
							i = pathes.begin(); i != pathes.end(); ++ i) {
						*o_path = *i + L"\\" + name;
						if (isReadable(*o_path, i_debugLevel))
							goto add_symbols;
					}
					return false;
				} else {
					if (!isReadable(*o_path, i_debugLevel))
						return false;
				}
add_symbols:
				if (o_symbols) {
					for (Symbols::iterator
							i = symbols.begin(); i != symbols.end(); ++ i)
						o_symbols->insert(*i);
				}
				return true;
			}
		}

		if (!isFirstTime)
			return false;

		// find file from home directory
		HomeDirectories pathes;
		getHomeDirectories(&pathes);
		for (HomeDirectories::iterator i = pathes.begin(); i != pathes.end(); ++ i) {
			*o_path = *i + L"\\" + name;
			if (isReadable(*o_path, i_debugLevel))
				return true;
		}

		if (!i_name.empty())
			return false;				// called by 'include'

		if (!i_retry || !i_retry())
			return false;

		isFirstTime = false;
	}
}
