//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// msgstream.h


#ifndef _MSGSTREAM_H
#  define _MSGSTREAM_H

#  include "misc.h"
#  include "stringtool.h"
#  include "multithread.h"
#  include "log_level.h"
#  include <algorithm>
#  include <atomic>
#  include <mutex>


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// msgstream

/** msgstream.

    <p>Before writing to omsgstream, you must acquire lock by calling
    <code>acquire()</code>.  Then after completion of writing, you
    must call <code>release()</code>.</p>

    <p>Omsgbuf calls <code>PostMessage(hwnd, messageid, 0,
    (LPARAM)omsgbuf)</code> to notify that string is ready to get.
    When the window (<code>hwnd</code>) gets the message, it takes the
    pending text with <code>takeString()</code>, which hands it over and
    releases the lock in one step.  Whatever the reader does with the text
    afterwards is therefore outside the lock, and writers do not queue behind
    it.</p>

    <p>Every line that survives the level filter is prefixed with
    <code>hh:mm:ss.SSS|L|</code>, and a bare <code>[YYYY-MM-DD]</code> line is
    inserted whenever the date changes.  The prefix is produced here rather
    than at the call sites because one line is often written by several
    separate acquire()/release() pairs, and one pair often writes several
    lines.</p>

*/

template<class T, size_t SIZE = 1024,
class TR = std::char_traits<T>, class A = std::allocator<T> >
class basic_msgbuf : public std::basic_streambuf<T, TR>, public SyncObject
{
public:
	using String = std::basic_string<T, TR, A>;	///
	using Super = std::basic_streambuf<T, TR>;	///

	/// characters in "hh:mm:ss.SSS|L|"
	static const size_t PREFIX_LENGTH = 15;

private:
	HWND m_hwnd;					/** window handle for
						    notification */
	UINT m_messageId;				/// messageid for notification
	A m_allocator;				/// allocator
	T *m_buf;					/// for streambuf
	String m_str;					/// for notification
	std::recursive_mutex m_mutex;			/// lock

	/** Message is kept when ( m_msgLevel <= m_threshold ).  m_threshold is
	    written by the UI thread and read by the writing threads, hence atomic.
	*/
	std::atomic<LogLevel> m_threshold;
	LogLevel m_msgLevel;				/// level of the message being written

	/** True when the tail of m_str is a line boundary, so the next kept
	    character starts a new line and needs a prefix.  Discarded text does
	    not move it: m_str is all the reader ever sees.
	*/
	bool m_atLineStart;

	/// true while a notification has been posted but not yet drained
	bool m_notifyPending;

	// time cache; hh:mm:ss and the date are recomputed only when the second
	// rolls over, which keeps the per-line cost to a clock read and 3 digits
	unsigned long long m_cachedSecond;		/// seconds since the epoch
	T m_cachedHMS[8];				/// "hh:mm:ss", not terminated
	WORD m_cachedYear;				///
	WORD m_cachedMonth;				///
	WORD m_cachedDay;				///

private:
	basic_msgbuf(const basic_msgbuf &) = delete;		/// disable copy constructor
	basic_msgbuf& operator=(const basic_msgbuf &) = delete;	/// disable copy assignment
	basic_msgbuf(basic_msgbuf &&) = delete;		/// disable move constructor
	basic_msgbuf& operator=(basic_msgbuf &&) = delete;	/// disable move assignment

public:
	///
	basic_msgbuf(UINT i_messageId, HWND i_hwnd = 0)
			: m_hwnd(i_hwnd),
			m_messageId(i_messageId),
			m_buf(m_allocator.allocate(SIZE)),
			m_threshold(kLogLevelNormal),
			m_msgLevel(LogLevel::Info),
			m_atLineStart(true),
			m_notifyPending(false),
			m_cachedSecond(0),
			m_cachedYear(0),
			m_cachedMonth(0),
			m_cachedDay(0) {
		ASSERT(m_buf);
		std::fill_n(m_cachedHMS, NUMBER_OF(m_cachedHMS), static_cast<T>('0'));
		this->setp(m_buf, m_buf + SIZE);
	}

	///
	~basic_msgbuf() {
		sync();
		m_allocator.deallocate(m_buf, SIZE);
	}

	/// attach/detach a window
	basic_msgbuf* attach(HWND i_hwnd) {
		std::lock_guard<std::recursive_mutex> lock(m_mutex);
		ASSERT( !m_hwnd && i_hwnd );
		m_hwnd = i_hwnd;
		notify();
		return this;
	}

	///
	basic_msgbuf* detach() {
		std::lock_guard<std::recursive_mutex> lock(m_mutex);
		sync();
		m_hwnd = 0;
		return this;
	}

	/// get window handle
	HWND getHwnd() const {
		return m_hwnd;
	}

	/// is a window attached ?
	int is_open() const {
		return !!m_hwnd;
	}

	/** Hand the pending text over, and let go of the lock.

	    <p>io_str is swapped with the buffer rather than copied out of it, so
	    the reader's buffer comes back here to be filled again: neither side
	    reallocates once both have grown to the size they need.</p>

	    <p>What the reader then does with the text - above all appending it to
	    the edit control, which costs more than the whole of the rest of this
	    path - happens outside the lock.  That is the point.  While it runs,
	    writers append to the fresh buffer rather than waiting behind the
	    control.</p>
	*/
	void takeString(String *io_str) {
		std::lock_guard<std::recursive_mutex> lock(m_mutex);
		io_str->clear();
		io_str->swap(m_str);
		// Cleared while still holding the lock.  A writer that publishes
		// after this point must post a notification of its own: this one is
		// already spoken for and its text has left.
		m_notifyPending = false;
	}

	/// set the threshold; messages above it are dropped as they are written
	void setThreshold(LogLevel i_threshold) {
		m_threshold.store(i_threshold, std::memory_order_relaxed);
	}

	///
	LogLevel getThreshold() const {
		return m_threshold.load(std::memory_order_relaxed);
	}

	/** Would a message of this level be kept ?  Call sites in the key input
	    path use this to skip building a message that would only be thrown
	    away: the ostream formatting runs even when the result is discarded.
	*/
	bool wouldLog(LogLevel i_level) const {
		return i_level <= m_threshold.load(std::memory_order_relaxed);
	}

	// for stream
	typename Super::int_type overflow(typename Super::int_type i_c = TR::eof()) {
		if (sync() == TR::eof()) // sync before new buffer created below
			return TR::eof();

		if (i_c != TR::eof()) {
			*this->pptr() = TR::to_char_type(i_c);
			this->pbump(1);
			sync();
		}
		return TR::not_eof(i_c); // return something other than EOF if successful
	}

	// for stream
	int sync() {
		if (m_msgLevel <= m_threshold.load(std::memory_order_relaxed))
			appendChunk(this->pbase(),
						static_cast<size_t>(this->pptr() - this->pbase()));
		this->setp(m_buf, m_buf + SIZE);
		return TR::not_eof(0);
	}

	// sync object

	/// begin writing
	virtual void acquire() {
		m_mutex.lock();
		m_msgLevel = LogLevel::Info;
	}

	/// begin writing
	virtual void acquire(LogLevel i_level) {
		m_mutex.lock();
		m_msgLevel = i_level;
	}

	/// end writing
	virtual void release() {
		notify();
		m_msgLevel = LogLevel::Info;
#pragma warning(suppress: 26110) // m_mutex is locked in acquire() and release() is called only after acquire()
		m_mutex.unlock();
	}

private:
	/** Post at most one outstanding notification.  Without the flag the
	    detail mode posts a message per acquire()/release() pair - about six
	    per keystroke - and all but the first find an empty string.
	*/
	void notify() {
		if (m_str.empty() || m_notifyPending || !m_hwnd)
			return;
		if (PostMessage(m_hwnd, m_messageId, 0, reinterpret_cast<LPARAM>(this)))
			m_notifyPending = true;
	}

	/// append text, starting each line with a prefix
	void appendChunk(const T *i_p, size_t i_length) {
		const T *end = i_p + i_length;
		const T nl = static_cast<T>('\n');
		while (i_p < end) {
			if (m_atLineStart) {
				if (*i_p == nl) {
					// blank line: a prefix on its own would be noise
					m_str += *i_p ++;
					continue;
				}
				appendPrefix();
				m_atLineStart = false;
			}
			const T *p = std::find(i_p, end, nl);
			if (p == end) {
				m_str.append(i_p, static_cast<size_t>(end - i_p));
				i_p = end;
			} else {
				m_str.append(i_p, static_cast<size_t>(p - i_p) + 1);
				i_p = p + 1;
				m_atLineStart = true;
			}
		}
	}

	/// write two decimal digits
	static void put2(T *&io_p, unsigned i_value) {
		*io_p ++ = static_cast<T>('0' + (i_value / 10) % 10);
		*io_p ++ = static_cast<T>('0' + i_value % 10);
	}

	/// "hh:mm:ss.SSS|L|", preceded by "[YYYY-MM-DD]\n" when the date changed
	void appendPrefix() {
		FILETIME ft;
		// GetLocalTime() and GetSystemTime() are quantised to the timer tick,
		// which is 15.625 ms by default - the milliseconds they report would
		// be fiction.  This one is both finer and cheaper.
		GetSystemTimePreciseAsFileTime(&ft);
		unsigned long long t =
			(static_cast<unsigned long long>(ft.dwHighDateTime) << 32) |
			ft.dwLowDateTime;
		unsigned long long millisec = t / 10000;
		unsigned long long second = millisec / 1000;
		if (second != m_cachedSecond) {
			m_cachedSecond = second;
			refreshTimeCache(ft);
		}

		T prefix[PREFIX_LENGTH];
		T *p = prefix;
		for (size_t i = 0; i < NUMBER_OF(m_cachedHMS); ++ i)
			*p ++ = m_cachedHMS[i];
		*p ++ = static_cast<T>('.');
		unsigned ms = static_cast<unsigned>(millisec % 1000);
		*p ++ = static_cast<T>('0' + ms / 100);
		put2(p, ms);
		*p ++ = static_cast<T>('|');
		*p ++ = static_cast<T>(logLevelChar(m_msgLevel));
		*p ++ = static_cast<T>('|');
		m_str.append(prefix, PREFIX_LENGTH);
	}

	/// recompute hh:mm:ss and the date; called at most once per second
	void refreshTimeCache(const FILETIME &i_ft) {
		FILETIME lft;
		SYSTEMTIME st;
		// FileTimeToLocalFileTime() costs 7 ns; SystemTimeToTzSpecificLocalTime()
		// costs 352 ns and is not needed for this
		if (!FileTimeToLocalFileTime(&i_ft, &lft) ||
				!FileTimeToSystemTime(&lft, &st))
			return;

		T *p = m_cachedHMS;
		put2(p, st.wHour);
		*p ++ = static_cast<T>(':');
		put2(p, st.wMinute);
		*p ++ = static_cast<T>(':');
		put2(p, st.wSecond);

		if (st.wYear == m_cachedYear && st.wMonth == m_cachedMonth &&
				st.wDay == m_cachedDay)
			return;
		m_cachedYear = st.wYear;
		m_cachedMonth = st.wMonth;
		m_cachedDay = st.wDay;

		// "[YYYY-MM-DD]" deliberately breaks the column layout of the normal
		// lines: that is what makes a date change visible at a glance
		T date[13];	// "[YYYY-MM-DD]" and the newline
		T *d = date;
		*d ++ = static_cast<T>('[');
		put2(d, st.wYear / 100);
		put2(d, st.wYear);
		*d ++ = static_cast<T>('-');
		put2(d, st.wMonth);
		*d ++ = static_cast<T>('-');
		put2(d, st.wDay);
		*d ++ = static_cast<T>(']');
		*d ++ = static_cast<T>('\n');
		m_str.append(date, NUMBER_OF(date));
	}
};


///
template<class T, size_t SIZE = 1024,
class TR = std::char_traits<T>, class A = std::allocator<T> >
class basic_omsgstream : public std::basic_ostream<T, TR>, public SyncObject
{
public:
	using Super = std::basic_ostream<T, TR>;	///
	using StreamBuf = basic_msgbuf<T, SIZE, TR, A>; ///
	using String = std::basic_string<T, TR, A>;	///

private:
	StreamBuf m_streamBuf;			///

public:
	///
	explicit basic_omsgstream(UINT i_messageId, HWND i_hwnd = 0)
			: Super(&m_streamBuf), m_streamBuf(i_messageId, i_hwnd) {
	}

	///
	virtual ~basic_omsgstream() {
	}

	///
	StreamBuf *rdbuf() const {
		return const_cast<StreamBuf *>(&m_streamBuf);
	}

	/// attach a msg control
	void attach(HWND i_hwnd) {
		m_streamBuf.attach(i_hwnd);
	}

	/// detach a msg control
	void detach() {
		m_streamBuf.detach();
	}

	/// get window handle of the msg control
	HWND getHwnd() const {
		return m_streamBuf.getHwnd();
	}

	/// is the msg control attached ?
	int is_open() const {
		return m_streamBuf.is_open();
	}

	/// set the threshold
	void setThreshold(LogLevel i_threshold) {
		m_streamBuf.setThreshold(i_threshold);
	}

	///
	LogLevel getThreshold() const {
		return m_streamBuf.getThreshold();
	}

	/// would a message of this level be kept ?
	bool wouldLog(LogLevel i_level) const {
		return m_streamBuf.wouldLog(i_level);
	}

	/// hand the pending text over; see basic_msgbuf::takeString()
	void takeString(String *io_str) {
		m_streamBuf.takeString(io_str);
	}

	// sync object

	/// begin writing
	virtual void acquire() {
		m_streamBuf.acquire();
	}

	/// begin writing
	virtual void acquire(LogLevel i_level) {
		m_streamBuf.acquire(i_level);
	}

	/// end writing
	virtual void release() {
		m_streamBuf.release();
	}
};

///
using womsgstream = basic_omsgstream<wchar_t>;

#endif // !_MSGSTREAM_H
