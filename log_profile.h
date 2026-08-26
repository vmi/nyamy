//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// log_profile.h


#ifndef _LOG_PROFILE_H
#  define _LOG_PROFILE_H

/** How long a writer waits to get into the log, and how long it holds it.

    The log rework is about a tail rather than an average: the keyboard
    handler thread must never be held up by whatever the reader happens to be
    doing.  So this keeps the maximum and a power of two histogram, not a
    mean - a mean hides exactly the case that matters.

    <p>"wait" is the time spent inside Acquire's constructor: what a writer
    pays before it may start formatting.  "hold" runs from there until
    release() has returned: what every other writer pays for this one, and
    what the reader competes with.</p>

    <p>Everything here compiles away unless NYAMY_LOG_PROFILE is defined, and
    no project file defines it.  Uncomment the line below to take a
    measurement; doc/testing.md has the procedure and how to read the
    result.</p>
*/
//#  define NYAMY_LOG_PROFILE


#  ifdef NYAMY_LOG_PROFILE

#    include <windows.h>
#    include <atomic>
#    include <bit>
#    include <cstdio>


/// power of two histogram of nanosecond samples, plus count and maximum
class LogProfileHistogram
{
public:
	/// 2^39 ns is about nine minutes; anything past that is its own bug
	static const size_t BUCKETS = 40;

private:
	std::atomic<unsigned long long> m_bucket[BUCKETS];	///
	std::atomic<unsigned long long> m_count;		///
	std::atomic<unsigned long long> m_max;		///

public:
	///
	LogProfileHistogram() : m_count(0), m_max(0) {
		for (size_t i = 0; i < BUCKETS; ++ i)
			m_bucket[i].store(0, std::memory_order_relaxed);
	}

	/** Add one sample.  Every writing thread calls this, so it is all atomic
	    and all relaxed: the counters are only ever read after the writers
	    have stopped, and ordering between them means nothing.
	*/
	void add(unsigned long long i_nanosec) {
		// bucket b holds [2^(b-1), 2^b); bucket 0 holds exactly 0
		size_t b = static_cast<size_t>(std::bit_width(i_nanosec));
		if (BUCKETS <= b)
			b = BUCKETS - 1;
		m_bucket[b].fetch_add(1, std::memory_order_relaxed);
		m_count.fetch_add(1, std::memory_order_relaxed);

		unsigned long long m = m_max.load(std::memory_order_relaxed);
		while (m < i_nanosec &&
				!m_max.compare_exchange_weak(m, i_nanosec,
											 std::memory_order_relaxed))
			;
	}

	///
	void reset() {
		for (size_t i = 0; i < BUCKETS; ++ i)
			m_bucket[i].store(0, std::memory_order_relaxed);
		m_count.store(0, std::memory_order_relaxed);
		m_max.store(0, std::memory_order_relaxed);
	}

	/// write the histogram through OutputDebugString, one line per bucket
	void report(const wchar_t *i_name) const {
		unsigned long long count = m_count.load(std::memory_order_relaxed);
		wchar_t line[256];
		swprintf_s(line, L"  %s: n=%llu max=%s\n", i_name, count,
				   duration(m_max.load(std::memory_order_relaxed)).m_text);
		OutputDebugString(line);
		if (count == 0)
			return;

		// only the occupied range is worth printing; an empty tail of 30
		// buckets makes the interesting part harder to find
		size_t last = 0;
		for (size_t i = 0; i < BUCKETS; ++ i)
			if (m_bucket[i].load(std::memory_order_relaxed))
				last = i;

		for (size_t i = 0; i <= last; ++ i) {
			unsigned long long n = m_bucket[i].load(std::memory_order_relaxed);
			if (n == 0)
				continue;
			// bar length is a share of the total, so the shape survives
			// however many samples were taken
			wchar_t bar[41];
			size_t len = static_cast<size_t>(n * 40 / count);
			for (size_t j = 0; j < len; ++ j)
				bar[j] = L'#';
			bar[len] = L'\0';
			swprintf_s(line, L"    <%-8s %10llu %5.1f%% %s\n",
					   duration(i == 0 ? 0 : (1ULL << i)).m_text, n,
					   static_cast<double>(n) * 100.0 /
					   static_cast<double>(count), bar);
			OutputDebugString(line);
		}
	}

private:
	/// a nanosecond count rendered in whichever unit keeps it readable
	struct Duration {
		wchar_t m_text[24];			///
	};

	///
	static Duration duration(unsigned long long i_nanosec) {
		Duration d;
		if (i_nanosec < 1000ULL)
			swprintf_s(d.m_text, L"%lluns", i_nanosec);
		else if (i_nanosec < 1000ULL * 1000)
			swprintf_s(d.m_text, L"%.1fus",
					   static_cast<double>(i_nanosec) / 1000.0);
		else if (i_nanosec < 1000ULL * 1000 * 1000)
			swprintf_s(d.m_text, L"%.1fms",
					   static_cast<double>(i_nanosec) / (1000.0 * 1000.0));
		else
			swprintf_s(d.m_text, L"%.2fs",
					   static_cast<double>(i_nanosec) /
					   (1000.0 * 1000.0 * 1000.0));
		return d;
	}
};


/// the two histograms, and the clock they are sampled with
class LogProfile
{
	LogProfileHistogram m_wait;			///
	LogProfileHistogram m_hold;			///

public:
	///
	static LogProfile &instance() {
		static LogProfile theInstance;
		return theInstance;
	}

	///
	LogProfileHistogram &wait() {
		return m_wait;
	}

	///
	LogProfileHistogram &hold() {
		return m_hold;
	}

	///
	static long long now() {
		LARGE_INTEGER li;
		QueryPerformanceCounter(&li);
		return li.QuadPart;
	}

	///
	static unsigned long long nanosecBetween(long long i_from, long long i_to) {
		long long ticks = i_to - i_from;
		if (ticks < 0)			// only if the counter was read out of order
			return 0;
		// Split rather than multiplying first: ticks * 1e9 overflows after
		// about 920 seconds at the usual 10 MHz, and a wait that long is
		// exactly the case worth not corrupting.
		long long freq = frequency();
		return static_cast<unsigned long long>(ticks / freq) * 1000000000ULL +
			   static_cast<unsigned long long>(ticks % freq) * 1000000000ULL /
			   static_cast<unsigned long long>(freq);
	}

	/// write both histograms, then start over
	void reportAndReset(const wchar_t *i_tag) {
		wchar_t line[256];
		swprintf_s(line, L"nyamy log profile [%s]\n", i_tag);
		OutputDebugString(line);
		m_wait.report(L"wait");
		m_hold.report(L"hold");
		m_wait.reset();
		m_hold.reset();
	}

private:
	///
	static long long frequency() {
		static const long long theFrequency = []() {
			LARGE_INTEGER li;
			QueryPerformanceFrequency(&li);
			return li.QuadPart ? li.QuadPart : 1;
		}();
		return theFrequency;
	}
};


/// times one Acquire; see the comment at the top of this file
class AcquireProfile
{
	long long m_begin;				///
	long long m_acquired;				///

public:
	///
	AcquireProfile() : m_begin(LogProfile::now()), m_acquired(0) {
	}

	///
	void acquired() {
		m_acquired = LogProfile::now();
		LogProfile::instance().wait().add(
			LogProfile::nanosecBetween(m_begin, m_acquired));
	}

	///
	void released() {
		LogProfile::instance().hold().add(
			LogProfile::nanosecBetween(m_acquired, LogProfile::now()));
	}
};

/// write the measurement so far and start over
#    define LOG_PROFILE_REPORT(tag)	LogProfile::instance().reportAndReset(tag)

#  else // !NYAMY_LOG_PROFILE

/// does nothing; see the comment at the top of this file
class AcquireProfile
{
public:
	///
	void acquired() {
	}
	///
	void released() {
	}
};

///
#    define LOG_PROFILE_REPORT(tag)	((void)0)

#  endif // !NYAMY_LOG_PROFILE

#endif // !_LOG_PROFILE_H
