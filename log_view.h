//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// log_view.h


#ifndef _LOG_VIEW_H
#  define _LOG_VIEW_H

#  include <string>

class LogBuffer;


/// what the control has to be told to bring it into step
struct LogViewUpdate {
	/** Replace everything with m_append rather than editing what is there.
	    m_dropChars is not used when this is set. */
	bool m_isRebuild;
	/// characters to remove from the front
	size_t m_dropChars;
	/// text to add at the end, or the whole text when m_isRebuild
	std::wstring m_append;

	///
	LogViewUpdate() : m_isRebuild(false), m_dropChars(0) {
	}

	/// nothing to do
	bool isEmpty() const {
		return !m_isRebuild && m_dropChars == 0 && m_append.empty();
	}
};


/** Works out what an edit control showing a LogBuffer has to be told.

    <p>This is bookkeeping rather than display: it holds no window and makes
    no Win32 call, so what it decides can be checked without one - which is
    the point, because getting the arithmetic wrong here is what would put the
    control quietly out of step with the log.</p>

    <p>The control is never rewritten wholesale for an ordinary update.  New
    text is appended and evicted text is removed from the front, so a refresh
    costs what changed rather than what is held.</p>

    <p>Both are counted in the control's characters, not the buffer's.  The
    control holds CRLF where the buffer holds LF, so the two lengths differ by
    the number of newlines between them; LogBuffer counts the newlines it
    evicts so that this stays exact without keeping a copy of what went.</p>

    <p>A rebuild is asked for when what is shown and what is held no longer
    overlap - the dialog was hidden long enough for all of it to be evicted -
    and after reset(), which the dialog calls when the control being described
    has gone.  Clearing the log needs no rebuild: LogBuffer reports it as the
    eviction of everything, which the ordinary path already handles.</p>
*/
class LogView
{
	/// sequence number of the first character shown
	unsigned long long m_shownFirstSeq;
	/// one past the last character shown
	unsigned long long m_shownEndSeq;
	/// LogBuffer::droppedNewlines() as of the last update
	unsigned long long m_shownDroppedNewlines;
	/// next update() replaces everything
	bool m_needsRebuild;
	/// reused so that a refresh does not reallocate
	std::wstring m_scratch;

public:
	///
	LogView();

	/** Work out what the control needs, and record that it was told.

	    The caller is expected to apply the result; the bookkeeping here is
	    updated as though it had.
	*/
	void update(const LogBuffer &i_buffer, LogViewUpdate *o_update);

	/// forget what is shown, so that the next update() replaces everything
	void reset();

private:
	/// can what is shown be brought forward, or does it have to be redone ?
	bool canFollow(const LogBuffer &i_buffer) const;

	/// produce the whole text and start the bookkeeping over
	void buildAll(const LogBuffer &i_buffer, LogViewUpdate *o_update);

	/// copy i_from to o_out, turning each LF into CRLF
	static void expandNewlines(const std::wstring &i_from, std::wstring *o_out);
};


/** Whether the control follows the end of the log, worked out from what the
    reader does with it.

    <p>While it follows, every refresh brings the control up to date and
    scrolls to the end.  Once the reader scrolls away from the end, the control
    is left alone: nothing is appended and nothing is dropped from the front,
    so the end stays where it is and can be scrolled back to however fast
    lines arrive.  In detail mode the wheel itself writes more of them than a
    notch scrolls, so an end that kept moving could not be reached that way.
    Getting back to the end resumes, and what arrived in the meantime comes in
    with that one refresh; LogView works out what that takes.</p>

    <p>Scrolling away is told from the position the last refresh left the
    control at, which only the reader changes in between.  Being out of sight
    of the end is not enough on its own, so that nothing else - the text
    re-wrapping, say - can stop it.  While stopped, nothing but the reader
    moves the view either, so being at the end again is all it takes to
    resume.</p>

    <p>A selection holds refreshes off as well, without stopping: appending
    moves the caret, which would lose it.</p>

    <p>Like LogView this makes no Win32 call.  The dialog reads the control and
    passes in what it read.</p>
*/
class LogFollow
{
	bool m_isFollowing;			/// keeping the control at the end
	bool m_hasLeftAt;			/// is m_leftAt known ?
	int m_leftAt;				/// scroll position the last refresh left

public:
	///
	LogFollow();

	/** A refresh is due: is the control to be brought up to date?

	    @param i_position		the control's scroll position
	    @param i_isAtEnd		whether the end of the text is in view
	    @param i_hasSelection	whether any of the text is selected
	    @return true to bring it up to date and scroll to the end, after which
	    leftAt() is to be told where that left it */
	bool shouldRefresh(int i_position, bool i_isAtEnd, bool i_hasSelection);

	/// the control has been scrolled to the end, and is at i_position
	void leftAt(int i_position);

	/// follow again, whatever the reader did
	void resume();

	///
	bool isFollowing() const {
		return m_isFollowing;
	}
};


#endif // !_LOG_VIEW_H
