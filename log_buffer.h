//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// log_buffer.h


#ifndef _LOG_BUFFER_H
#  define _LOG_BUFFER_H

#  include <string>
#  include <vector>


/** The text the log holds, with the edit control as a view onto it.

    <p>Before this, the edit control <em>was</em> the log: how much scrollback
    there was and what every appended line cost were the same number, which is
    why the limit had to be kept small.  Here the control only shows what this
    holds.</p>

    <p><b>One buffer, allocated once.</b>  A single character ring of exactly
    logMaxSize wide characters, taken at startup and never grown, moved or
    reallocated.  nyamy sits resident for weeks at a time, so the log - which
    is a diagnostic aid rather than the point of the program - has no business
    handing the allocator a stream of small blocks to fragment the heap with.
    Holding lines as separate strings would do exactly that: one allocation
    per line, each recycled slot's capacity creeping up to the longest line it
    ever held.  A ring of characters has none of that.  It is full from the
    first moment it fills and stays that way.</p>

    <p>Positions are character sequence numbers that keep counting up as text
    is evicted, never indices into the ring.  A reader that has been away can
    therefore tell whether what it last saw is still here, and no position it
    holds can quietly come to mean somewhere else.</p>

    <p>Eviction is by whole lines: the oldest line goes in one piece, so the
    first line kept is never a fragment.  There is no per-line size limit and
    none is needed - one absurdly long line simply occupies the ring and
    pushes the rest out, which is the same bound as everything else.</p>

    <p>Not synchronised.  Everything that touches this - the drain, the view
    refresh, clearing - runs on the UI thread.  Moving the drain off that
    thread means giving this a lock.</p>
*/
class LogBuffer
{
	std::vector<wchar_t> m_chars;		/// the ring; size fixed at construction
	unsigned long long m_firstSeq;		/// sequence number of the oldest char
	unsigned long long m_endSeq;		/// one past the newest
	/** Newlines among the characters that have been evicted.  A viewer needs
	    this to work out how much of what it shows has gone: the control holds
	    CRLF where this holds LF, so the two lengths differ by exactly the
	    number of newlines between them. */
	unsigned long long m_droppedNewlines;

private:
	LogBuffer(const LogBuffer &) = delete;		///
	LogBuffer &operator=(const LogBuffer &) = delete; ///

public:
	/// i_maxChars of 0 is treated as 1: the arithmetic below needs a slot
	explicit LogBuffer(size_t i_maxChars);

	/** Take a chunk of drained text.

	    The chunk may begin and end anywhere, including in the middle of a
	    line: one line is often written by several separate
	    acquire()/release() pairs.
	*/
	void add(const wchar_t *i_text, size_t i_length);

	///
	void add(const std::wstring &i_text) {
		add(i_text.c_str(), i_text.size());
	}

	/// drop everything, keeping the buffer itself
	void clear();

	/// sequence number of the oldest character held
	unsigned long long firstSeq() const {
		return m_firstSeq;
	}

	/// one past the newest, so [firstSeq(), endSeq()) is what is held
	unsigned long long endSeq() const {
		return m_endSeq;
	}

	/// newlines among the characters evicted so far
	unsigned long long droppedNewlines() const {
		return m_droppedNewlines;
	}

	/// append the characters in [i_from, i_to) to o_out, clamped to what is held
	void copyRange(unsigned long long i_from, unsigned long long i_to,
				   std::wstring *o_out) const;

	/// characters held
	size_t size() const {
		return static_cast<size_t>(m_endSeq - m_firstSeq);
	}

	/// characters that can be held
	size_t capacity() const {
		return m_chars.size();
	}

private:
	///
	size_t freeChars() const {
		return m_chars.size() - size();
	}

	/// evict whole lines until i_needed characters will fit
	void makeRoom(size_t i_needed);

	/// evict up to and including the next newline
	void dropOneLine();
};


#endif // !_LOG_BUFFER_H
