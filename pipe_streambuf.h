//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// pipe_streambuf.h
//
// std::streambuf / std::wstreambuf wrappers for Win32 pipe HANDLEs.

#ifndef _PIPE_STREAMBUF_H
#  define _PIPE_STREAMBUF_H

#  include <streambuf>
#  include <windows.h>


//=============================================================================
// PipeWriteStreambuf - wraps a Win32 write pipe HANDLE as a std::streambuf
//=============================================================================

class PipeWriteStreambuf : public std::streambuf
{
public:
	explicit PipeWriteStreambuf(HANDLE h) : m_h(h) {}

protected:
	int_type overflow(int_type c) override {
		if (c == EOF) return EOF;
		char ch = static_cast<char>(c);
		DWORD written;
		return WriteFile(m_h, &ch, 1, &written, NULL) ? c : EOF;
	}

	std::streamsize xsputn(const char *s, std::streamsize n) override {
		DWORD written;
		if (!WriteFile(m_h, s, static_cast<DWORD>(n), &written, NULL))
			return 0;
		return static_cast<std::streamsize>(written);
	}

private:
	HANDLE m_h;
};


//=============================================================================
// PipeReadStreambuf - wraps a Win32 read pipe HANDLE as a std::streambuf
//=============================================================================

class PipeReadStreambuf : public std::streambuf
{
public:
	explicit PipeReadStreambuf(HANDLE h) : m_h(h) {}

protected:
	int_type underflow() override {
		DWORD got = 0;
		if (!ReadFile(m_h, &m_ch, 1, &got, NULL) || got == 0)
			return traits_type::eof();
		setg(&m_ch, &m_ch, &m_ch + 1);
		return traits_type::to_int_type(m_ch);
	}

private:
	HANDLE m_h;
	char m_ch = 0;
};


//=============================================================================
// PipeReadWStreambuf - wraps a Win32 read pipe HANDLE as a std::wstreambuf
// Reads UTF-16 wchar_t units from a pipe whose write end uses _O_U16TEXT mode
//=============================================================================

class PipeReadWStreambuf : public std::wstreambuf
{
public:
	explicit PipeReadWStreambuf(HANDLE h) : m_h(h) {}

protected:
	int_type underflow() override {
		DWORD got = 0;
		if (!ReadFile(m_h, &m_ch, sizeof(wchar_t), &got, NULL) || got < sizeof(wchar_t))
			return traits_type::eof();
		setg(&m_ch, &m_ch, &m_ch + 1);
		return traits_type::to_int_type(m_ch);
	}

private:
	HANDLE m_h;
	wchar_t m_ch = 0;
};


#endif // !_PIPE_STREAMBUF_H
