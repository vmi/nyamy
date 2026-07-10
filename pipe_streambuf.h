//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// pipe_streambuf.h
//
// std::streambuf / std::wstreambuf wrappers for Win32 pipe HANDLEs.

#ifndef _PIPE_STREAMBUF_H
#  define _PIPE_STREAMBUF_H

#  include <streambuf>
#  include <string>
#  include <windows.h>


//=============================================================================
// PipeWriteStreambuf - wraps a Win32 write pipe HANDLE as a std::streambuf
//
// Default (blocking) mode writes each byte/run to the pipe immediately, so a
// full pipe buffer blocks the caller until the reader drains it.
//
// Non-blocking mode (opt-in via setNonBlocking) buffers everything written
// between flushes and, on sync(), emits it as a single non-blocking WriteFile.
// If the whole message does not fit (the reader is stuck and the pipe buffer
// is full), the message is dropped and wasBlocked() reports true instead of
// blocking the caller.  This is meant for the ctrl pipe, whose writer runs on
// the engine's keyboard-handler thread and must never stall on a busy scripter.
//=============================================================================

class PipeWriteStreambuf : public std::streambuf
{
public:
	explicit PipeWriteStreambuf(HANDLE h) : m_h(h) {}

	/// Switch to non-blocking, drop-on-full mode.  Call once the pipe reader
	/// is running (messages written before this go out reliably).
	void setNonBlocking() {
		m_nonBlocking = true;
		DWORD mode = PIPE_NOWAIT;
		SetNamedPipeHandleState(m_h, &mode, NULL, NULL);
	}

	/// true if the most recent flush was dropped because the pipe was full
	bool wasBlocked() const { return m_blocked; }
	void clearBlocked() { m_blocked = false; }

protected:
	int_type overflow(int_type c) override {
		if (c == EOF) return EOF;
		if (m_nonBlocking) {
			m_buf.push_back(static_cast<char>(c));
			return c;
		}
		char ch = static_cast<char>(c);
		DWORD written;
		return WriteFile(m_h, &ch, 1, &written, NULL) ? c : EOF;
	}

	std::streamsize xsputn(const char *s, std::streamsize n) override {
		if (m_nonBlocking) {
			m_buf.append(s, static_cast<size_t>(n));
			return n;
		}
		DWORD written;
		if (!WriteFile(m_h, s, static_cast<DWORD>(n), &written, NULL))
			return 0;
		return static_cast<std::streamsize>(written);
	}

	int sync() override {
		if (!m_nonBlocking || m_buf.empty())
			return 0;
		// Emit the whole buffered message in one non-blocking write.  In
		// PIPE_NOWAIT mode WriteFile writes only what currently fits, so a
		// full buffer yields written < size; treat any non-complete write as
		// "dropped" and discard the rest.  Never fail the stream: drops are
		// reported through wasBlocked() so callers stay usable.
		DWORD written = 0;
		BOOL ok = WriteFile(m_h, m_buf.data(),
		                    static_cast<DWORD>(m_buf.size()), &written, NULL);
		if (!ok || written != m_buf.size())
			m_blocked = true;
		m_buf.clear();
		return 0;
	}

private:
	HANDLE m_h;
	bool m_nonBlocking = false;
	bool m_blocked = false;
	std::string m_buf;
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
