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
//
// The drop is all or nothing: a PIPE_NOWAIT write to a byte-mode pipe either
// takes the whole message or writes nothing at all - including when the message
// is larger than the entire buffer, which is therefore always dropped.  A
// partial write would leave a truncated message in the stream and desynchronise
// the reader's framing for good, so the reporting below rests on this.
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
		// Emit the whole buffered message in one non-blocking write.  Never
		// fail the stream: a drop is reported through wasBlocked() so callers
		// stay usable.
		DWORD written = 0;
		if (!WriteFile(m_h, m_buf.data(), static_cast<DWORD>(m_buf.size()),
		               &written, NULL) || written != m_buf.size())
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
//
// With a stop event, reads are issued overlapped and signalling that event ends
// them: end of stream is reported to the caller and the thread parked here gets
// out.  The handle must then have been created with FILE_FLAG_OVERLAPPED.
//
// Without one (the default) reads are plain synchronous ReadFile calls, which
// is what the scripter side and the tests use - they own no such handle, and
// their end of the stream is the peer closing it.
//=============================================================================

class PipeReadStreambuf : public std::streambuf
{
public:
	explicit PipeReadStreambuf(HANDLE h, HANDLE hStop = NULL)
		: m_h(h), m_hStop(hStop) {
		if (m_hStop)
			m_ol.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	}

	~PipeReadStreambuf() {
		if (m_ol.hEvent) {
			cancelPendingRead();
			CloseHandle(m_ol.hEvent);
		}
	}

protected:
	int_type underflow() override {
		DWORD got = 0;
		// ReadFile on a pipe returns as soon as any data is available, so
		// asking for the whole buffer never waits for it to fill: this only
		// saves system calls, it does not add latency.
		if (m_hStop) {
			if (!overlappedRead(&got))
				return traits_type::eof();
		} else if (!ReadFile(m_h, m_buf, sizeof(m_buf), &got, NULL)) {
			return traits_type::eof();
		}
		// A named pipe fails the read with ERROR_BROKEN_PIPE once the write end
		// closes, where an anonymous one reports zero bytes instead.  Both
		// shapes still occur here, and both mean end of stream.
		if (got == 0)
			return traits_type::eof();
		setg(m_buf, m_buf, m_buf + got);
		return traits_type::to_int_type(m_buf[0]);
	}

private:
	/// false on end of stream, which includes being stopped
	bool overlappedRead(DWORD *o_got) {
		ResetEvent(m_ol.hEvent);
		if (ReadFile(m_h, m_buf, sizeof(m_buf), o_got, &m_ol))
			return true;			// completed inline
		if (GetLastError() != ERROR_IO_PENDING)
			return false;

		m_isReadPending = true;
		HANDLE handles[2] = { m_ol.hEvent, m_hStop };
		if (WaitForMultipleObjects(2, handles, FALSE, INFINITE)
				!= WAIT_OBJECT_0) {
			cancelPendingRead();
			return false;
		}
		m_isReadPending = false;
		return GetOverlappedResult(m_h, &m_ol, o_got, FALSE) != FALSE;
	}

	/// Wait out the read the kernel still owns.  It writes into m_buf and
	/// stores through m_ol, so neither may be released before it is over.
	void cancelPendingRead() {
		if (!m_isReadPending)
			return;
		CancelIoEx(m_h, &m_ol);
		DWORD got = 0;
		GetOverlappedResult(m_h, &m_ol, &got, TRUE);
		m_isReadPending = false;
	}

	HANDLE m_h;
	HANDLE m_hStop;				///< NULL: read synchronously
	OVERLAPPED m_ol = {};		///< hEvent is NULL unless m_hStop is set
	bool m_isReadPending = false;
	char m_buf[4096];
};


#endif // !_PIPE_STREAMBUF_H
