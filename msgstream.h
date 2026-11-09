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
#  include <vector>


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
    <code>hh:mm:ss.SSS|L| </code>, and a <code>[YYYY-MM-DD] ----</code> line is
    inserted whenever the date changes.  The prefix is produced here rather
    than at the call sites because one pair of acquire()/release() often
    writes several lines.  A line with nothing on it is not written at all.</p>

    <p><b>Blocks.</b>  beginBlock() opens a block: the next line written is
    its header, the lines after it are its body, indented here rather than by
    the writer.  endBlock() closes it.</p>

    <pre>
    12:34:56.790|D| IN D-A (0x1e) {
    12:34:56.790|D|   OUT D-A (0x1e)
    12:34:56.790|D| }
    </pre>

    <p>A block is handed to the reader whole, when it closes, and until then
    only the thread that opened it writes into it.  Another thread's line is
    published at once and so lands before the block rather than in the middle
    of it.  Holding the lock for the length of the block would keep them in
    the order they were written, but the engine runs functions inside a
    keystroke's block, and a function that calls into the UI thread while that
    thread waits for this lock is a deadlock.</p>

    <p>Every line of a block carries the time the block closed, which is when
    it reaches the reader: the log reads in time order, the other thread's
    line included.  What a block's lines do not tell is when each was written;
    something that waits for long inside a block says so on a line of its
    own.</p>

    <p>The header's level decides.  Filtered out, it opens nothing, and what
    is written meanwhile comes out as ordinary lines at no indent.  A body
    that ends up empty - lighter lines filtered, say - leaves the header on
    its own with no braces.  A block opened inside a block of the same thread
    gets no braces either: its body is simply indented one step further, so
    that one top level block stays one unit for the reader.</p>
*/

template<class T, size_t SIZE = 1024,
class TR = std::char_traits<T>, class A = std::allocator<T> >
class basic_msgbuf : public std::basic_streambuf<T, TR>, public SyncObject
{
public:
	using String = std::basic_string<T, TR, A>;	///
	using Super = std::basic_streambuf<T, TR>;	///

	/// characters in "hh:mm:ss.SSS|L| "
	static const size_t PREFIX_LENGTH = 16;

private:
	/** Unit tests move the date on, which they have no other way to do.  A
	    friend declaration does not change what is generated for the shipped
	    build. */
	friend class MsgbufTestAccess;

	/// characters in "hh:mm:ss.SSS|", the part a block line gets as it is
	/// published
	static const size_t TIME_LENGTH = 13;
	/// characters in "L| ", the part a block line is written with
	static const size_t LEVEL_LENGTH = PREFIX_LENGTH - TIME_LENGTH;

	/** The block one thread has open.  A slot is reused once it is closed, and
	    its text keeps its capacity.

	    Each line is held from the level part of its prefix on: "L| ", the
	    indent, the text and the newline.  The time is left out until the block
	    is published, when every line gets the same one. */
	struct Block {
		DWORD m_owner;			/// thread that opened it; 0 when free
		int m_depth;			/// beginBlock() calls not yet closed
		LogLevel m_level;		/// level of the top level header
		String m_text;			/// lines so far, without their time
		size_t m_lines;			/// complete lines in m_text
		bool m_atLineStart;		/// next kept character starts a new line
		bool m_isHeaderNext;	/// the line about to start is a header

		///
		Block()
				: m_owner(0),
				m_depth(0),
				m_level(LogLevel::Info),
				m_lines(0),
				m_atLineStart(true),
				m_isHeaderNext(false) {
		}
	};

	/// dashes after the date on a date line
	static const size_t DATE_RULE_LENGTH = 31;

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

	/** One slot per thread with a block open, which is two or three at the
	    very most.  Grown when none is free, and never shrunk. */
	std::vector<Block> m_blocks;

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

	/** Open a block at i_level; the next line this thread writes is its header.

	    Takes the lock itself, so it may be called inside or outside an
	    acquire()/release() pair.  A block can span any number of them.

	    @return whether one was opened.  Call endBlock() only if it was.
	*/
	bool beginBlock(LogLevel i_level) {
		if (!wouldLog(i_level))
			return false;
		std::lock_guard<std::recursive_mutex> lock(m_mutex);
		sync();					// what is written so far is not in the block
		const DWORD self = GetCurrentThreadId();
		Block *b = findBlock(self);
		if (!b) {
			b = findBlock(0);
			if (!b) {
				m_blocks.emplace_back();
				b = &m_blocks.back();
			}
			b->m_owner = self;
			b->m_level = i_level;
		}
		++ b->m_depth;
		b->m_isHeaderNext = true;
		return true;
	}

	/// close the innermost block this thread has open; see beginBlock()
	void endBlock() {
		std::lock_guard<std::recursive_mutex> lock(m_mutex);
		sync();
		Block *b = findBlock(GetCurrentThreadId());
		if (!b)
			return;
		b->m_isHeaderNext = false;
		if (0 < -- b->m_depth)
			return;
		publishBlock(b);
		notify();
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
		// Text left in the put area would otherwise be attributed to whoever
		// syncs next: their level, and their block.
		sync();
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

	/// the block i_owner has open, or a free slot when i_owner is 0
	Block *findBlock(DWORD i_owner) {
		for (Block &b : m_blocks)
			if (b.m_owner == i_owner)
				return &b;
		return NULL;
	}

	/** Append text, starting each line with its prefix.  A line of a block gets
	    only the level part for now; see Block. */
	void appendChunk(const T *i_p, size_t i_length) {
		Block *b = findBlock(GetCurrentThreadId());
		String &out = b ? b->m_text : m_str;
		bool &atLineStart = b ? b->m_atLineStart : m_atLineStart;
		const T *end = i_p + i_length;
		const T nl = static_cast<T>('\n');
		while (i_p < end) {
			if (atLineStart) {
				if (*i_p == nl) {
					++ i_p;		// a line with nothing on it is not written
					continue;
				}
				if (b) {
					appendLevel(&out, m_msgLevel);
					appendIndent(b);
				} else {
					appendPrefix(&out, m_msgLevel);
				}
				atLineStart = false;
			}
			const T *p = std::find(i_p, end, nl);
			if (p == end) {
				out.append(i_p, static_cast<size_t>(end - i_p));
				i_p = end;
			} else {
				out.append(i_p, static_cast<size_t>(p - i_p) + 1);
				i_p = p + 1;
				atLineStart = true;
				if (b)
					++ b->m_lines;
			}
		}
	}

	/// two spaces per enclosing block; a header belongs to the one around it
	void appendIndent(Block *io_b) {
		const int depth = io_b->m_depth - (io_b->m_isHeaderNext ? 1 : 0);
		io_b->m_isHeaderNext = false;
		io_b->m_text.append(static_cast<size_t>(depth) * 2,
							static_cast<T>(' '));
	}

	/** Hand a closed block to the reader and free its slot.

	    Each line goes out as the time and then the line as it is held, which
	    already starts at the level part.  The braces are added on the way: the
	    header's " {" before its newline, and a line of its own for "}". */
	void publishBlock(Block *io_b) {
		const T nl = static_cast<T>('\n');
		if (!io_b->m_atLineStart) {
			io_b->m_text += nl;
			++ io_b->m_lines;
		}
		if (0 < io_b->m_lines) {
			// Formatted before any of the block reaches m_str: a date line it
			// may produce has to land before the block, not inside its braces.
			T time[TIME_LENGTH];
			formatTime(time);
			static const T opening[] = {
				static_cast<T>(' '), static_cast<T>('{'), static_cast<T>('\n') };
			static const T closing[] = { static_cast<T>('}'), static_cast<T>('\n') };
			const bool hasBraces = 1 < io_b->m_lines;

			const T *p = io_b->m_text.data();
			const T *const end = p + io_b->m_text.size();
			const T *const header = p;
			while (p < end) {
				const T *eol = std::find(p, end, nl);	// every line has one
				const T *next = (eol == end) ? end : eol + 1;
				m_str.append(time, TIME_LENGTH);
				if (p == header && hasBraces) {
					m_str.append(p, static_cast<size_t>(eol - p));
					m_str.append(opening, NUMBER_OF(opening));
				} else {
					m_str.append(p, static_cast<size_t>(next - p));
				}
				p = next;
			}
			if (hasBraces) {
				m_str.append(time, TIME_LENGTH);
				appendLevel(&m_str, io_b->m_level);
				m_str.append(closing, NUMBER_OF(closing));
			}
		}

		io_b->m_owner = 0;
		io_b->m_text.clear();
		io_b->m_lines = 0;
		io_b->m_atLineStart = true;
	}

	/// write two decimal digits
	static void put2(T *&io_p, unsigned i_value) {
		*io_p ++ = static_cast<T>('0' + (i_value / 10) % 10);
		*io_p ++ = static_cast<T>('0' + i_value % 10);
	}

	/// append the whole prefix, "hh:mm:ss.SSS|L| ", to io_out; see formatTime()
	void appendPrefix(String *io_out, LogLevel i_level) {
		T time[TIME_LENGTH];
		formatTime(time);
		io_out->append(time, TIME_LENGTH);
		appendLevel(io_out, i_level);
	}

	/// append the level part of the prefix, "L| ", to io_out
	static void appendLevel(String *io_out, LogLevel i_level) {
		const T level[LEVEL_LENGTH] = {
			static_cast<T>(logLevelChar(i_level)),
			static_cast<T>('|'),
			static_cast<T>(' '),
		};
		io_out->append(level, LEVEL_LENGTH);
	}

	/** Write the time part of the prefix, "hh:mm:ss.SSS|", to o_time:
	    TIME_LENGTH characters with no terminator.  A date line, when the date
	    has changed, goes straight to m_str, ahead of whatever the time is put
	    in front of. */
	void formatTime(T *o_time) {
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

		T *p = o_time;
		for (size_t i = 0; i < NUMBER_OF(m_cachedHMS); ++ i)
			*p ++ = m_cachedHMS[i];
		*p ++ = static_cast<T>('.');
		unsigned ms = static_cast<unsigned>(millisec % 1000);
		*p ++ = static_cast<T>('0' + ms / 100);
		put2(p, ms);
		*p ++ = static_cast<T>('|');
		ASSERT(p == o_time + TIME_LENGTH);
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

		// The date line deliberately breaks the column layout of the normal
		// lines: that is what makes a date change visible at a glance.  Plain
		// dashes because the user picks the control's font, and not every font
		// has box drawing characters that join up.
		T date[13 + DATE_RULE_LENGTH + 1];	// "[YYYY-MM-DD] ", rule, newline
		T *d = date;
		*d ++ = static_cast<T>('[');
		put2(d, st.wYear / 100);
		put2(d, st.wYear);
		*d ++ = static_cast<T>('-');
		put2(d, st.wMonth);
		*d ++ = static_cast<T>('-');
		put2(d, st.wDay);
		*d ++ = static_cast<T>(']');
		*d ++ = static_cast<T>(' ');
		d = std::fill_n(d, DATE_RULE_LENGTH, static_cast<T>('-'));
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

	/// open a block; see basic_msgbuf::beginBlock()
	bool beginBlock(LogLevel i_level) {
		return m_streamBuf.beginBlock(i_level);
	}

	/// close a block; see basic_msgbuf::endBlock()
	void endBlock() {
		m_streamBuf.endBlock();
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


/** One log block for the length of a scope, or until close().

    Declare it before the Acquire that writes the header.  It is not itself
    an acquire()/release() pair and neither needs nor disturbs one. */
class LogBlock
{
	womsgstream *m_log;		///
	bool m_isOpen;			/// does an endBlock() remain to be made ?

private:
	LogBlock(const LogBlock &) = delete;			///
	LogBlock &operator=(const LogBlock &) = delete;	///

public:
	/// not open yet; see open()
	explicit LogBlock(womsgstream &i_log) : m_log(&i_log), m_isOpen(false) {
	}

	/// open at once
	LogBlock(womsgstream &i_log, LogLevel i_level)
			: m_log(&i_log), m_isOpen(i_log.beginBlock(i_level)) {
	}

	///
	~LogBlock() {
		close();
	}

	/// open it, unless it already is
	void open(LogLevel i_level) {
		if (!m_isOpen)
			m_isOpen = m_log->beginBlock(i_level);
	}

	/// close it, if it is open
	void close() {
		if (m_isOpen) {
			m_isOpen = false;
			m_log->endBlock();
		}
	}
};

#endif // !_MSGSTREAM_H
