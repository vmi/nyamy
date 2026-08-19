//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// log_buffer.cpp


#include "log_buffer.h"

#include <algorithm>


LogBuffer::LogBuffer(size_t i_maxChars)
		: m_chars(i_maxChars ? i_maxChars : 1),
		m_firstSeq(0),
		m_endSeq(0),
		m_droppedNewlines(0)
{
}


void LogBuffer::dropOneLine()
{
	// Searched as the one or two contiguous spans it occupies, the way
	// copyRange() reads it: walking the ring would need a division per
	// character to wrap the index.
	const size_t cap = m_chars.size();
	const size_t held = size();
	const size_t pos = static_cast<size_t>(m_firstSeq % cap);
	const size_t firstPart = std::min(held, cap - pos);

	const wchar_t *begin = m_chars.data() + pos;
	const wchar_t *end = begin + firstPart;
	const wchar_t *found = std::find(begin, end, L'\n');
	if (found != end) {
		m_firstSeq += static_cast<size_t>(found - begin) + 1;
		++ m_droppedNewlines;
		return;
	}

	if (firstPart < held) {
		const wchar_t *wrapped = m_chars.data();
		const wchar_t *wrappedEnd = wrapped + (held - firstPart);
		found = std::find(wrapped, wrappedEnd, L'\n');
		if (found != wrappedEnd) {
			m_firstSeq += firstPart +
						  static_cast<size_t>(found - wrapped) + 1;
			++ m_droppedNewlines;
			return;
		}
	}

	// No newline anywhere means a single line fills the ring; there is
	// nothing to keep, and the whole of it goes.
	m_firstSeq = m_endSeq;
}


void LogBuffer::makeRoom(size_t i_needed)
{
	while (freeChars() < i_needed && m_firstSeq < m_endSeq)
		dropOneLine();
}


void LogBuffer::add(const wchar_t *i_text, size_t i_length)
{
	const size_t cap = m_chars.size();

	// A chunk longer than the whole ring can only leave its tail.  The
	// characters skipped here are never given a sequence number, so they are
	// not part of any range a viewer could ask about.
	if (cap < i_length) {
		i_text += i_length - cap;
		i_length = cap;
	}
	if (i_length == 0)
		return;

	makeRoom(i_length);

	size_t pos = static_cast<size_t>(m_endSeq % cap);
	size_t firstPart = std::min(i_length, cap - pos);
	std::copy(i_text, i_text + firstPart, m_chars.begin() + pos);
	if (firstPart < i_length)
		std::copy(i_text + firstPart, i_text + i_length, m_chars.begin());
	m_endSeq += i_length;
}


void LogBuffer::clear()
{
	// Everything held is counted as evicted rather than forgotten, so that a
	// viewer still in step can work out how much of what it shows has gone
	// and follow the clear like any other eviction.
	const size_t cap = m_chars.size();
	for (unsigned long long q = m_firstSeq; q < m_endSeq; ++ q)
		if (m_chars[static_cast<size_t>(q % cap)] == L'\n')
			++ m_droppedNewlines;

	// Sequence numbers carry on rather than restarting: one held from before
	// the clear has to read as "gone", not as somewhere else.
	m_firstSeq = m_endSeq;
}


void LogBuffer::copyRange(unsigned long long i_from, unsigned long long i_to,
						  std::wstring *o_out) const
{
	if (i_from < m_firstSeq)
		i_from = m_firstSeq;
	if (m_endSeq < i_to)
		i_to = m_endSeq;
	if (i_to <= i_from)
		return;

	const size_t cap = m_chars.size();
	size_t n = static_cast<size_t>(i_to - i_from);
	size_t pos = static_cast<size_t>(i_from % cap);
	size_t firstPart = std::min(n, cap - pos);
	o_out->append(&m_chars[pos], firstPart);
	if (firstPart < n)
		o_out->append(&m_chars[0], n - firstPart);
}
