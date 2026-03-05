//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// msgstream.h


#ifndef _MSGSTREAM_H
#  define _MSGSTREAM_H

#  include "misc.h"
#  include "stringtool.h"
#  include "multithread.h"
#  include <mutex>


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// msgstream

/** msgstream.

    <p>Before writing to omsgstream, you must acquire lock by calling
    <code>acquire()</code>.  Then after completion of writing, you
    must call <code>release()</code>.</p>

    <p>Omsgbuf calls <code>PostMessage(hwnd, messageid, 0,
    (LPARAM)omsgbuf)</code> to notify that string is ready to get.
    When the window (<code>hwnd</code>) get the message, you can get
    the string containd in the omsgbuf by calling
    <code>acquireString()</code>.  After calling
    <code>acquireString()</code>, you must / call releaseString().</p>

*/

template<class T, size_t SIZE = 1024,
class TR = std::char_traits<T>, class A = std::allocator<T> >
class basic_msgbuf : public std::basic_streambuf<T, TR>, public SyncObject
{
public:
	using String = std::basic_string<T, TR, A>;	///
	using Super = std::basic_streambuf<T, TR>;	///

private:
	HWND m_hwnd;					/** window handle for
						    notification */
	UINT m_messageId;				/// messageid for notification
	A m_allocator;				/// allocator
	T *m_buf;					/// for streambuf
	String m_str;					/// for notification
	std::recursive_mutex m_mutex;			/// lock

	/** debug level.
	    if ( m_msgDebugLevel &lt;= m_debugLevel ), message is displayed
	*/
	int m_debugLevel;
	int m_msgDebugLevel;				///

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
			m_debugLevel(0),
			m_msgDebugLevel(0) {
		ASSERT(m_buf);
		setp(m_buf, m_buf + SIZE);
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
		if (!m_str.empty())
			PostMessage(m_hwnd, m_messageId, 0, (LPARAM)this);
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

	/// acquire string and release the string
	const String &acquireString() {
		m_mutex.lock();
		return m_str;
	}

	///
	void releaseString() {
		m_str.resize(0);
		m_mutex.unlock();
	}

	/// set debug level
	void setDebugLevel(int i_debugLevel) {
		m_debugLevel = i_debugLevel;
	}

	///
	int getDebugLevel() const {
		return m_debugLevel;
	}

	// for stream
	typename Super::int_type overflow(typename Super::int_type i_c = TR::eof()) {
		if (sync() == TR::eof()) // sync before new buffer created below
			return TR::eof();

		if (i_c != TR::eof()) {
			*pptr() = TR::to_char_type(i_c);
			pbump(1);
			sync();
		}
		return TR::not_eof(i_c); // return something other than EOF if successful
	}

	// for stream
	int sync() {
		if (m_msgDebugLevel <= m_debugLevel)
			m_str += String(pbase(), static_cast<size_t>(pptr() - pbase()));
		setp(m_buf, m_buf + SIZE);
		return TR::not_eof(0);
	}

	// sync object

	/// begin writing
	virtual void acquire() {
		m_mutex.lock();
	}

	/// begin writing
	virtual void acquire(int i_msgDebugLevel) {
		m_mutex.lock();
		m_msgDebugLevel = i_msgDebugLevel;
	}

	/// end writing
	virtual void release() {
		if (!m_str.empty())
			PostMessage(m_hwnd, m_messageId, 0, reinterpret_cast<LPARAM>(this));
		m_msgDebugLevel = m_debugLevel;
		m_mutex.unlock();
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

	/// set debug level
	void setDebugLevel(int i_debugLevel) {
		m_streamBuf.setDebugLevel(i_debugLevel);
	}

	///
	int getDebugLevel() const {
		return m_streamBuf.getDebugLevel();
	}

	/// acquire string and release the string
	const String &acquireString() {
		return m_streamBuf.acquireString();
	}

	///
	void releaseString() {
		m_streamBuf->releaseString();
	}

	// sync object

	/// begin writing
	virtual void acquire() {
		m_streamBuf.acquire();
	}

	/// begin writing
	virtual void acquire(int i_msgDebugLevel) {
		m_streamBuf.acquire(i_msgDebugLevel);
	}

	/// end writing
	virtual void release() {
		m_streamBuf.release();
	}
};

///
using womsgstream = basic_omsgstream<wchar_t>;

#endif // !_MSGSTREAM_H
