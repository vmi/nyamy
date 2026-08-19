//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// log_view.cpp


#include "log_view.h"
#include "log_buffer.h"


LogView::LogView()
		: m_shownFirstSeq(0),
		m_shownEndSeq(0),
		m_shownDroppedNewlines(0),
		m_needsRebuild(true)
{
}


void LogView::reset()
{
	m_shownFirstSeq = 0;
	m_shownEndSeq = 0;
	m_shownDroppedNewlines = 0;
	m_needsRebuild = true;
	m_scratch.clear();
}


void LogView::expandNewlines(const std::wstring &i_from, std::wstring *o_out)
{
	o_out->reserve(o_out->size() + i_from.size());
	for (size_t i = 0; i < i_from.size(); ++ i) {
		if (i_from[i] == L'\n')
			*o_out += L'\r';
		*o_out += i_from[i];
	}
}


bool LogView::canFollow(const LogBuffer &i_buffer) const
{
	// Text was written and evicted without ever being shown, so there is
	// nothing to bring forward from.
	if (m_shownEndSeq < i_buffer.firstSeq())
		return false;
	// The log went backwards, which nothing does; a guard rather than a case.
	if (i_buffer.endSeq() < m_shownEndSeq)
		return false;
	return true;
}


void LogView::buildAll(const LogBuffer &i_buffer, LogViewUpdate *o_update)
{
	m_scratch.clear();
	i_buffer.copyRange(i_buffer.firstSeq(), i_buffer.endSeq(), &m_scratch);
	expandNewlines(m_scratch, &o_update->m_append);

	m_shownFirstSeq = i_buffer.firstSeq();
	m_shownEndSeq = i_buffer.endSeq();
	m_shownDroppedNewlines = i_buffer.droppedNewlines();

	o_update->m_isRebuild = true;
	m_needsRebuild = false;
}


void LogView::update(const LogBuffer &i_buffer, LogViewUpdate *o_update)
{
	o_update->m_isRebuild = false;
	o_update->m_dropChars = 0;
	o_update->m_append.clear();

	if (m_needsRebuild || !canFollow(i_buffer)) {
		buildAll(i_buffer, o_update);
		return;
	}

	// What has been evicted since last time, counted in the control's
	// characters: the buffer's count, plus one for each newline among them
	// because the control holds CRLF where the buffer holds LF.
	o_update->m_dropChars =
		static_cast<size_t>(i_buffer.firstSeq() - m_shownFirstSeq) +
		static_cast<size_t>(i_buffer.droppedNewlines() -
							m_shownDroppedNewlines);
	m_shownFirstSeq = i_buffer.firstSeq();
	m_shownDroppedNewlines = i_buffer.droppedNewlines();

	if (m_shownEndSeq < i_buffer.endSeq()) {
		m_scratch.clear();
		i_buffer.copyRange(m_shownEndSeq, i_buffer.endSeq(), &m_scratch);
		expandNewlines(m_scratch, &o_update->m_append);
		m_shownEndSeq = i_buffer.endSeq();
	}
}
