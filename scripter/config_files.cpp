//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// config_files.cpp


#include "../misc.h"

#include "config_files.h"
#include "../mayu.h"
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


// get home directory path
void ConfigFiles::getHomeDirectories(HomeDirectories *o_pathes) const
{
	wchar_t buf[GANA_MAX_PATH];

	if (GetEnvironmentVariable(L"USERPROFILE", buf, NUMBER_OF(buf)))
		o_pathes->push_back(wstringi(std::wstring(buf) + L"\\.config\\nyamy"));
	if (GetEnvironmentVariable(L"LOCALAPPDATA", buf, NUMBER_OF(buf)))
		o_pathes->push_back(wstringi(std::wstring(buf) + L"\\NYamy\\Config"));
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
		const char *bytes = reinterpret_cast<const char *>(buf.data());
		int size = static_cast<int>(sbuf.st_size);
		// skip UTF-8 BOM (EF BB BF)
		if (size >= 3 &&
		    (unsigned char)bytes[0] == 0xef &&
		    (unsigned char)bytes[1] == 0xbb &&
		    (unsigned char)bytes[2] == 0xbf) {
			bytes += 3;
			size  -= 3;
		}
		// MB_ERR_INVALID_CHARS: fail (return 0) on invalid UTF-8 sequences
		int wlen = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
		                               bytes, size, NULL, 0);
		if (wlen > 0) {
			o_data->resize(wlen);
			MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
			                    bytes, size, &(*o_data)[0], wlen);
			fclose(fp);
			return true;
		}
		// fall through to multibyte / ASCII
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
							  RetryCallback i_retry,
							  int i_debugLevel) const
{
	// the default filename is ".mayu"
	const wstringi &name = i_name.empty() ? wstringi(L".mayu") : i_name;

	bool isFirstTime = true;

	while (true) {
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
