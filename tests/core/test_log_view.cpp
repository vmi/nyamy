//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// test_log_view.cpp
//
// The invariant LogView exists to maintain: applying what it reports, in
// order, leaves the control holding exactly what the buffer holds with each
// LF turned into CRLF - however many updates were skipped in between.
//
// LogView makes no Win32 call, so the edit control is a std::wstring here and
// this exercises the shipped code rather than a copy of it.


#include "core_test.h"
#include "log_buffer.h"
#include "log_view.h"

#include <string>


namespace {

/// stands in for the edit control
class FakeEdit
{
	std::wstring m_text;

public:
	///
	void apply(const LogViewUpdate &i_update) {
		if (i_update.m_isRebuild) {
			m_text = i_update.m_append;
			return;
		}
		if (i_update.isEmpty())
			return;
		// what the control does; a drop longer than the text would be a bug
		// here rather than something to paper over
		CORE_CHECK(i_update.m_dropChars <= m_text.size(),
				   "the drop fits within what is shown");
		m_text.erase(0, i_update.m_dropChars);
		m_text += i_update.m_append;
	}

	///
	const std::wstring &text() const {
		return m_text;
	}
};


/// what the control should hold, worked out the long way
std::wstring expected(const LogBuffer &i_b)
{
	std::wstring raw;
	i_b.copyRange(i_b.firstSeq(), i_b.endSeq(), &raw);
	std::wstring s;
	for (size_t i = 0; i < raw.size(); ++ i) {
		if (raw[i] == L'\n')
			s += L'\r';
		s += raw[i];
	}
	return s;
}


/// deterministic, so that a failure can be reproduced
unsigned g_seed = 12345;
unsigned nextRandom()
{
	g_seed = g_seed * 1103515245u + 12345u;
	return (g_seed >> 16) & 0x7fff;
}


void refresh(LogView *io_view, FakeEdit *io_edit, const LogBuffer &i_b)
{
	LogViewUpdate update;
	io_view->update(i_b, &update);
	io_edit->apply(update);
}

} // namespace


void runLogViewTests()
{
	// the first update has nothing to go on, so it replaces everything
	{
		LogBuffer b(32);
		LogView v;
		b.add(L"a\nb\n");
		LogViewUpdate u;
		v.update(b, &u);
		CORE_CHECK(u.m_isRebuild, "the first update rebuilds");
		CORE_CHECK(u.m_append == L"a\r\nb\r\n", "LF became CRLF");
	}

	// ordinary following: append only
	{
		LogBuffer b(32);
		LogView v;
		FakeEdit e;
		b.add(L"a\n");
		refresh(&v, &e, b);
		LogViewUpdate u;
		b.add(L"b\n");
		v.update(b, &u);
		CORE_CHECK(!u.m_isRebuild, "following, not rebuilding");
		CORE_CHECK(u.m_dropChars == 0, "nothing dropped");
		CORE_CHECK(u.m_append == L"b\r\n", "only the new text is appended");
	}

	// text shown while still unterminated, then continued.  Getting this
	// wrong repeats its head.
	{
		LogBuffer b(32);
		LogView v;
		FakeEdit e;
		b.add(L"par");
		refresh(&v, &e, b);
		CORE_CHECK(e.text() == L"par", "unterminated text is shown");
		b.add(L"tial\n");
		refresh(&v, &e, b);
		CORE_CHECK(e.text() == L"partial\r\n", "continued without repeating");
	}

	// The drop is counted in the control's characters, not the buffer's.
	// Evicting "ab\n" is 3 characters there and 4 here.
	{
		LogBuffer b(10);
		LogView v;
		FakeEdit e;
		b.add(L"ab\ncd\n");
		refresh(&v, &e, b);
		CORE_CHECK(e.text() == L"ab\r\ncd\r\n", "shown with CRLF");
		LogViewUpdate u;
		b.add(L"efghij\n");
		v.update(b, &u);
		CORE_CHECK(u.m_dropChars == 4, "dropped \"ab\\r\\n\", not \"ab\\n\"");
		e.apply(u);
		CORE_CHECK(e.text() == expected(b), "matches the buffer");
	}

	// nothing to do when nothing arrived
	{
		LogBuffer b(32);
		LogView v;
		FakeEdit e;
		b.add(L"a\n");
		refresh(&v, &e, b);
		LogViewUpdate u;
		v.update(b, &u);
		CORE_CHECK(u.isEmpty(), "an idle refresh does nothing");
	}

	// hidden long enough that everything shown has been evicted
	{
		LogBuffer b(8);
		LogView v;
		FakeEdit e;
		b.add(L"aa\n");
		refresh(&v, &e, b);
		for (int i = 0; i < 20; ++ i)
			b.add(L"zz\n");
		LogViewUpdate u;
		v.update(b, &u);
		CORE_CHECK(u.m_isRebuild, "rebuilds when it cannot follow");
		e.apply(u);
		CORE_CHECK(e.text() == expected(b), "recovered after a long gap");
	}

	// Clearing is followed like any other eviction rather than rebuilt: it
	// is reported as everything having gone.
	{
		LogBuffer b(32);
		LogView v;
		FakeEdit e;
		b.add(L"a\nb\npart");
		refresh(&v, &e, b);
		LogViewUpdate u;
		b.clear();
		v.update(b, &u);
		CORE_CHECK(!u.m_isRebuild, "a clear does not need a rebuild");
		CORE_CHECK(u.m_dropChars == 10, "the whole of what was shown goes");
		e.apply(u);
		CORE_CHECK(e.text().empty(), "cleared");
		b.add(L"c\n");
		refresh(&v, &e, b);
		CORE_CHECK(e.text() == L"c\r\n", "usable after a clear");
	}

	// reset() is what the dialog calls when the control being described has
	// gone, so the next update has to replace everything
	{
		LogBuffer b(32);
		LogView v;
		FakeEdit e;
		b.add(L"a\nb\n");
		refresh(&v, &e, b);
		v.reset();
		LogViewUpdate u;
		v.update(b, &u);
		CORE_CHECK(u.m_isRebuild, "reset forces a rebuild");
		CORE_CHECK(u.m_append == L"a\r\nb\r\n", "the rebuild carries it all");
	}

	// The invariant, over randomised chunk shapes and randomly skipped
	// refreshes.  Refreshes are on a timer in the dialog, so any number of
	// chunks can land between two of them.
	{
		bool ok = true;
		int trials = 0;
		for (int trial = 0; trial < 200 && ok; ++ trial) {
			size_t cap = 1 + (nextRandom() % 40);
			LogBuffer b(cap);
			LogView v;
			FakeEdit e;

			for (int step = 0; step < 120; ++ step) {
				std::wstring chunk;
				int pieces = 1 + static_cast<int>(nextRandom() % 4);
				for (int i = 0; i < pieces; ++ i) {
					switch (nextRandom() % 4) {
					case 0:
						chunk += L"\n";			// blank line or terminator
						break;
					case 1:
						chunk += L"x";			// leaves it unterminated
						break;
					case 2:
						chunk += L"abc\n";
						break;
					default: {
						wchar_t t[32];
						swprintf_s(t, L"line%u\n", nextRandom() % 1000);
						chunk += t;
						break;
					}
					}
				}
				b.add(chunk.c_str(), chunk.size());

				if ((nextRandom() % 3) != 0)
					continue;		// this refresh was skipped
				refresh(&v, &e, b);

				if (e.text() != expected(b)) {
					printf("  trial %d step %d (capacity %zu)\n",
						   trial, step, cap);
					printf("  got      : [%ls]\n", e.text().c_str());
					printf("  expected : [%ls]\n", expected(b).c_str());
					ok = false;
					break;
				}
			}
			refresh(&v, &e, b);		// and after catching up
			if (ok && e.text() != expected(b)) {
				printf("  trial %d final (capacity %zu)\n", trial, cap);
				ok = false;
			}
			++ trials;
		}
		CORE_CHECK(ok, "the view matches the buffer over randomised runs");
		CORE_CHECK(trials == 200, "all randomised runs completed");
	}

	// the same, with clears thrown in
	{
		bool ok = true;
		for (int trial = 0; trial < 50 && ok; ++ trial) {
			LogBuffer b(1 + (nextRandom() % 24));
			LogView v;
			FakeEdit e;
			for (int step = 0; step < 80 && ok; ++ step) {
				if ((nextRandom() % 17) == 0)
					b.clear();
				else
					b.add(L"abc\ndef\n");
				if ((nextRandom() % 2) != 0)
					continue;		// this refresh was skipped
				refresh(&v, &e, b);
				if (e.text() != expected(b)) {
					printf("  clear trial %d step %d\n", trial, step);
					printf("  got      : [%ls]\n", e.text().c_str());
					printf("  expected : [%ls]\n", expected(b).c_str());
					ok = false;
				}
			}
			refresh(&v, &e, b);
			if (e.text() != expected(b)) {
				printf("  clear trial %d\n", trial);
				printf("  got      : [%ls]\n", e.text().c_str());
				printf("  expected : [%ls]\n", expected(b).c_str());
				ok = false;
			}
		}
		CORE_CHECK(ok, "the view survives clears at arbitrary points");
	}
}
