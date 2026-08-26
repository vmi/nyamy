//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// test_log_buffer.cpp
//
// LogBuffer's ring arithmetic: wrapping, eviction by whole lines, and the
// evicted-newline count that a viewer's bookkeeping depends on.


#include "core_test.h"
#include "log_buffer.h"

#include <string>


namespace {

/// everything held, so that a failure can be read
std::wstring held(const LogBuffer &i_b)
{
	std::wstring s;
	i_b.copyRange(i_b.firstSeq(), i_b.endSeq(), &s);
	return s;
}


bool holds(const LogBuffer &i_b, const wchar_t *i_expect)
{
	std::wstring got = held(i_b);
	if (got == i_expect)
		return true;
	printf("  got      : [%ls]\n", got.c_str());
	printf("  expected : [%ls]\n", i_expect);
	return false;
}

} // namespace


void runLogBufferTests()
{
	// plain appends, no wrap
	{
		LogBuffer b(32);
		b.add(L"a\nb\n");
		CORE_CHECK(holds(b, L"a\nb\n"), "two lines held");
		CORE_CHECK(b.size() == 4, "size counts characters");
		CORE_CHECK(b.firstSeq() == 0 && b.endSeq() == 4, "sequence 0..4");
		CORE_CHECK(b.droppedNewlines() == 0, "nothing evicted yet");
	}

	// a chunk that stops mid-line is held as it is; one line is often
	// written by several acquire()/release() pairs
	{
		LogBuffer b(32);
		b.add(L"he");
		CORE_CHECK(holds(b, L"he"), "unterminated text is held");
		b.add(L"llo\n");
		CORE_CHECK(holds(b, L"hello\n"), "continued and terminated");
	}

	// eviction takes whole lines, so the oldest line kept is never a fragment
	{
		LogBuffer b(10);
		b.add(L"ab\ncd\n");			// 6 chars
		b.add(L"efghij\n");			// needs 7, only 4 free
		CORE_CHECK(holds(b, L"cd\nefghij\n"), "oldest whole line evicted");
		CORE_CHECK(b.firstSeq() == 3, "sequence advanced past the newline");
		CORE_CHECK(b.droppedNewlines() == 1, "one newline evicted");
	}

	// more than one line goes when one is not enough
	{
		LogBuffer b(10);
		b.add(L"a\nb\nc\nd\n");		// 8 chars, 4 lines
		b.add(L"eeeeee\n");			// needs 7, only 2 free
		CORE_CHECK(holds(b, L"d\neeeeee\n"), "three lines evicted");
		CORE_CHECK(b.droppedNewlines() == 3, "three newlines evicted");
	}

	// the ring wraps: written and read across the end of the array
	{
		LogBuffer b(8);
		b.add(L"12\n");
		b.add(L"34\n");
		b.add(L"56\n");				// forces a wrap
		CORE_CHECK(holds(b, L"34\n56\n"), "content correct across the wrap");
		CORE_CHECK(b.size() == 6, "size after wrap");
	}

	// Eviction searches the held text as one or two contiguous spans.  Here
	// the newline it has to find is in the wrapped one, which is the branch
	// most easily broken by a later edit.
	{
		LogBuffer b(8);
		b.add(L"123456\n");			// 7 chars, no wrap yet
		b.add(L"ab\n");				// needs 3, 1 free: evicts, then wraps
		CORE_CHECK(holds(b, L"ab\n"), "content correct when the newline wrapped");
		CORE_CHECK(b.droppedNewlines() == 1, "the wrapped newline was counted");
		// now the held text itself straddles the end, with its newline in
		// the second span
		b.add(L"cd\n");
		CORE_CHECK(holds(b, L"ab\ncd\n"), "held across the wrap");
		b.add(L"ef\n");
		CORE_CHECK(holds(b, L"cd\nef\n"), "evicted from across the wrap");
		CORE_CHECK(b.droppedNewlines() == 2, "count still right after wrapping");
	}

	// far more text than capacity, where an off-by-one in the modulo would
	// show up as drift
	{
		LogBuffer b(64);
		for (int i = 0; i < 1000; ++ i) {
			wchar_t t[32];
			swprintf_s(t, L"%d\n", i);
			b.add(t);
		}
		std::wstring s = held(b);
		CORE_CHECK(s.size() <= 64, "never exceeds capacity");
		CORE_CHECK(s.find(L"999\n") != std::wstring::npos, "newest is held");
		CORE_CHECK(s[0] != L'\n', "does not start with a bare terminator");
		// every line kept must be whole, i.e. the text ends where it should
		CORE_CHECK(s[s.size() - 1] == L'\n', "ends on a line boundary");
	}

	// a single line longer than the whole ring keeps its tail
	{
		LogBuffer b(8);
		b.add(L"abcdefghijklmno\n");	// 16 chars into an 8 char ring
		std::wstring s = held(b);
		CORE_CHECK(s.size() <= 8, "clamped to capacity");
		CORE_CHECK(s == L"ijklmno\n", "the tail is what survives");
	}

	// a chunk longer than the ring, arriving in one call
	{
		LogBuffer b(4);
		b.add(L"xxxxxxxxxx");		// 10 chars, no newline at all
		CORE_CHECK(held(b) == L"xxxx", "clamped with no newline present");
		CORE_CHECK(b.size() == 4, "size clamped");
	}

	// clearing counts what it discards, so that a viewer can follow it
	{
		LogBuffer b(32);
		b.add(L"a\nb\n");
		unsigned long long end = b.endSeq();
		b.clear();
		CORE_CHECK(b.size() == 0, "cleared");
		CORE_CHECK(b.firstSeq() == end && b.endSeq() == end,
				   "sequence preserved over clear");
		CORE_CHECK(b.droppedNewlines() == 2,
				   "the newlines cleared are counted as evicted");
		b.add(L"c\n");
		CORE_CHECK(holds(b, L"c\n"), "usable after clear");
		CORE_CHECK(b.firstSeq() == end, "sequence continues");
	}

	// copyRange clamps rather than reading outside what is held
	{
		LogBuffer b(10);
		b.add(L"ab\ncd\n");
		std::wstring s;
		b.copyRange(0, 100, &s);
		CORE_CHECK(s == L"ab\ncd\n", "clamped to the end");
		s.clear();
		b.copyRange(2, 4, &s);
		CORE_CHECK(s == L"\nc", "a range inside");
		s.clear();
		b.copyRange(5, 3, &s);
		CORE_CHECK(s.empty(), "an inverted range yields nothing");
	}

	// capacity of one, the degenerate case the modulo has to survive
	{
		LogBuffer b(1);
		b.add(L"a\nb\n");
		CORE_CHECK(b.size() <= 1, "capacity 1 holds at most one character");
	}

	// zero is coerced to one rather than dividing by zero
	{
		LogBuffer b(0);
		CORE_CHECK(b.capacity() == 1, "zero capacity coerced to one");
		b.add(L"a\n");
		CORE_CHECK(b.size() <= 1, "zero capacity still usable");
	}

	// an empty add changes nothing
	{
		LogBuffer b(8);
		b.add(L"a\n");
		b.add(L"", 0);
		CORE_CHECK(holds(b, L"a\n"), "an empty chunk is a no-op");
		CORE_CHECK(b.endSeq() == 2, "an empty chunk does not move the end");
	}
}
