//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// test_log_format.cpp
//
// What a log line and a log block look like, and the promises made about
// them: no blank lines, one fixed width prefix, and a block that reaches the
// reader whole with its braces and indentation worked out by the stream.


#include "engine_test.h"

#include <thread>
#include <vector>


/** Reach into basic_msgbuf.  Declared a friend by msgstream.h, so it has to
    live outside the anonymous namespace below. */
class MsgbufTestAccess
{
public:
	/// make the next prefix find a new date, as if midnight had just passed
	static void forgetTheDate(womsgstream *io_log) {
		womsgstream::StreamBuf *b = io_log->rdbuf();
		b->m_cachedSecond = 0;
		b->m_cachedYear = 0;
	}
};


namespace
{

const size_t kPrefix = womsgstream::StreamBuf::PREFIX_LENGTH;


/// everything written since the last call, one element per line
std::vector<std::wstring> takeLines(womsgstream *io_log)
{
	std::wstring text = takeLog(io_log);
	std::vector<std::wstring> lines;
	size_t begin = 0;
	while (begin < text.size()) {
		size_t nl = text.find(L'\n', begin);
		if (nl == std::wstring::npos)
			nl = text.size();
		lines.push_back(text.substr(begin, nl - begin));
		begin = nl + 1;
	}
	return lines;
}

/// a date line, which appears once at the start and when the date changes
bool isDateLine(const std::wstring &i_line)
{
	return !i_line.empty() && i_line[0] == L'[';
}

/// write one line in an acquire()/release() of its own
void write(womsgstream *io_log, LogLevel i_level, const wchar_t *i_text)
{
	Acquire a(io_log, i_level);
	*io_log << i_text << std::endl;
}

/// a log that has already written its first date line
class Log
{
public:
	womsgstream m_log;

	///
	explicit Log(LogLevel i_threshold = LogLevel::Debug) : m_log(WM_APP) {
		m_log.setThreshold(i_threshold);
		write(&m_log, LogLevel::Error, L"start");
		takeLog(&m_log);
	}

	/// the lines since the last call, with the prefixes taken off
	std::vector<std::wstring> bodies() {
		std::vector<std::wstring> lines = takeLines(&m_log);
		for (std::wstring &line : lines)
			line = (kPrefix <= line.size()) ? line.substr(kPrefix) : L"<short>";
		return lines;
	}

	/// the level letter of each line since the last call
	std::wstring levels() {
		std::wstring letters;
		for (const std::wstring &line : takeLines(&m_log))
			letters += (14 < line.size()) ? line[13] : L'?';
		return letters;
	}
};

using Lines = std::vector<std::wstring>;


///
void aPrefixHasOneFixedWidth()
{
	womsgstream log(WM_APP);
	write(&log, LogLevel::Warn, L"hello");
	Lines lines = takeLines(&log);

	ENGINE_CHECK(lines.size() == 2, "the first line brings a date line with it");
	if (lines.size() != 2)
		return;
	const std::wstring &l = lines[1];
	ENGINE_CHECK(kPrefix == 16, "the prefix is 16 characters");
	ENGINE_CHECK(l.size() == kPrefix + 5, "prefix and text make up the line");
	ENGINE_CHECK(l[2] == L':' && l[5] == L':' && l[8] == L'.',
				 "the time is hh:mm:ss.SSS");
	ENGINE_CHECK(l[12] == L'|' && l[13] == L'W' && l[14] == L'|',
				 "the level sits between two bars");
	ENGINE_CHECK(l[15] == L' ', "and a space separates it from the text");
	ENGINE_CHECK(l.compare(kPrefix, std::wstring::npos, L"hello") == 0,
				 "the text follows");
}


///
void aDateLineHasARule()
{
	womsgstream log(WM_APP);
	write(&log, LogLevel::Info, L"hello");
	Lines lines = takeLines(&log);

	ENGINE_CHECK(lines.size() == 2 && isDateLine(lines[0]),
				 "the date line comes first");
	if (lines.size() != 2)
		return;
	const std::wstring &d = lines[0];
	ENGINE_CHECK(d.size() == 12 + 1 + 31, "date, a space and the rule");
	ENGINE_CHECK(d[11] == L']' && d[12] == L' ', "the date is bracketed");
	ENGINE_CHECK(d.find_first_not_of(L'-', 13) == std::wstring::npos,
				 "the rule is plain dashes");

	write(&log, LogLevel::Info, L"again");
	lines = takeLines(&log);
	ENGINE_CHECK(lines.size() == 1 && !isDateLine(lines[0]),
				 "and it is not repeated within the day");
}


///
void blankLinesAreNotWritten()
{
	Log l;
	{
		Acquire a(&l.m_log, LogLevel::Info);
		l.m_log << std::endl;
		l.m_log << L"text" << std::endl;
		l.m_log << std::endl << std::endl;
	}
	ENGINE_CHECK(l.bodies() == Lines({ L"text" }),
				 "only the line with something on it is kept");

	LogBlock block(l.m_log, LogLevel::Info);
	{
		Acquire a(&l.m_log, LogLevel::Info);
		l.m_log << L"head" << std::endl << std::endl << L"body" << std::endl;
	}
	block.close();
	ENGINE_CHECK(l.bodies() == Lines({ L"head {", L"  body", L"}" }),
				 "inside a block as well");
}


///
void aBlockGetsBracesAndIndentation()
{
	Log l;
	ENGINE_CHECK(l.m_log.beginBlock(LogLevel::Debug), "the block opens");
	write(&l.m_log, LogLevel::Debug, L"head");
	write(&l.m_log, LogLevel::Debug, L"one");
	write(&l.m_log, LogLevel::Debug, L"two");
	l.m_log.endBlock();
	ENGINE_CHECK(l.bodies() == Lines({ L"head {", L"  one", L"  two", L"}" }),
				 "braces and indentation come from the stream");
}


///
void theClosingBraceTakesTheHeadersLevel()
{
	Log l;
	l.m_log.beginBlock(LogLevel::Info);
	write(&l.m_log, LogLevel::Info, L"head");
	write(&l.m_log, LogLevel::Debug, L"lighter");
	write(&l.m_log, LogLevel::Error, L"heavier");
	l.m_log.endBlock();
	ENGINE_CHECK(l.levels() == L"IDEI", "each line keeps its own level");
}


///
void aBlockWithNoBodyHasNoBraces()
{
	Log l;
	l.m_log.beginBlock(LogLevel::Debug);
	write(&l.m_log, LogLevel::Debug, L"head");
	l.m_log.endBlock();
	ENGINE_CHECK(l.bodies() == Lines({ L"head" }), "just the header");

	Log lighter(LogLevel::Info);
	lighter.m_log.beginBlock(LogLevel::Info);
	write(&lighter.m_log, LogLevel::Info, L"head");
	write(&lighter.m_log, LogLevel::Debug, L"detail");
	lighter.m_log.endBlock();
	ENGINE_CHECK(lighter.bodies() == Lines({ L"head" }),
				 "also when a lighter body was filtered out");

	l.m_log.beginBlock(LogLevel::Debug);
	l.m_log.endBlock();
	ENGINE_CHECK(takeLog(&l.m_log).empty(), "and nothing at all for no lines");
}


///
void aFilteredHeaderOpensNoBlock()
{
	Log l(LogLevel::Info);
	ENGINE_CHECK(!l.m_log.beginBlock(LogLevel::Debug),
				 "a header that would be dropped opens nothing");
	write(&l.m_log, LogLevel::Debug, L"header");
	write(&l.m_log, LogLevel::Error, L"failure");
	ENGINE_CHECK(l.bodies() == Lines({ L"failure" }),
				 "a heavier line comes out at no indent, at once");
}


///
void aNestedBlockIsOnlyIndented()
{
	Log l;
	l.m_log.beginBlock(LogLevel::Debug);
	write(&l.m_log, LogLevel::Debug, L"outer");
	l.m_log.beginBlock(LogLevel::Debug);
	write(&l.m_log, LogLevel::Debug, L"inner");
	write(&l.m_log, LogLevel::Debug, L"deep");
	l.m_log.endBlock();
	write(&l.m_log, LogLevel::Debug, L"back");
	l.m_log.endBlock();
	ENGINE_CHECK(l.bodies() == Lines({ L"outer {", L"  inner", L"    deep",
									   L"  back", L"}" }),
				 "no braces inside, and one step deeper");
}


/// A nested header that is filtered out must not take a level of indent.
void aFilteredNestedHeaderAddsNoIndent()
{
	Log l(LogLevel::Info);
	l.m_log.beginBlock(LogLevel::Info);
	write(&l.m_log, LogLevel::Info, L"outer");
	ENGINE_CHECK(!l.m_log.beginBlock(LogLevel::Debug), "the inner one is dropped");
	write(&l.m_log, LogLevel::Debug, L"inner");
	write(&l.m_log, LogLevel::Error, L"failure");
	l.m_log.endBlock();
	ENGINE_CHECK(l.bodies() == Lines({ L"outer {", L"  failure", L"}" }),
				 "the heavier line stays at the outer block's body");
}


///
void aBlockIsHiddenUntilItCloses()
{
	Log l;
	l.m_log.beginBlock(LogLevel::Debug);
	write(&l.m_log, LogLevel::Debug, L"head");
	write(&l.m_log, LogLevel::Debug, L"body");
	ENGINE_CHECK(takeLog(&l.m_log).empty(), "nothing is handed over yet");
	l.m_log.endBlock();
	ENGINE_CHECK(l.bodies().size() == 3, "and all of it when it closes");
}


///
void anotherThreadsLineLandsBeforeTheBlock()
{
	Log l;
	l.m_log.beginBlock(LogLevel::Debug);
	write(&l.m_log, LogLevel::Debug, L"head");
	std::thread other([&l]() {
		write(&l.m_log, LogLevel::Info, L"from another thread");
	});
	other.join();
	write(&l.m_log, LogLevel::Debug, L"body");
	l.m_log.endBlock();
	ENGINE_CHECK(l.bodies() == Lines({ L"from another thread", L"head {",
									   L"  body", L"}" }),
				 "it does not end up between the braces");
}


///
void twoThreadsKeepTheirBlocksApart()
{
	Log l;
	l.m_log.beginBlock(LogLevel::Debug);
	write(&l.m_log, LogLevel::Debug, L"mine");
	std::thread other([&l]() {
		l.m_log.beginBlock(LogLevel::Debug);
		write(&l.m_log, LogLevel::Debug, L"theirs");
		write(&l.m_log, LogLevel::Debug, L"their body");
		l.m_log.endBlock();
	});
	other.join();
	write(&l.m_log, LogLevel::Debug, L"my body");
	l.m_log.endBlock();
	ENGINE_CHECK(l.bodies() == Lines({ L"theirs {", L"  their body", L"}",
									   L"mine {", L"  my body", L"}" }),
				 "each block stays whole");
}


///
void aClosedBlockLeavesNothingBehind()
{
	Log l;
	for (int i = 0; i < 2; ++ i) {
		l.m_log.beginBlock(LogLevel::Debug);
		write(&l.m_log, LogLevel::Debug, L"a longer header than the next");
		write(&l.m_log, LogLevel::Debug, L"body");
		l.m_log.endBlock();
	}
	Lines lines = l.bodies();
	ENGINE_CHECK(lines.size() == 6 && lines[3] == lines[0] && lines[0].back() == L'{',
				 "the second block is formed like the first");
	write(&l.m_log, LogLevel::Info, L"plain");
	ENGINE_CHECK(l.bodies() == Lines({ L"plain" }),
				 "and a later line is an ordinary one");
}


/// A date line belongs before a block, never between its braces, whenever in
/// the block's life the date moves on: its lines get their time as it closes.
void aDateLineLandsBeforeTheBlock()
{
	const Lines expected = { L"head {", L"  body", L"}" };
	for (int atClose = 0; atClose < 2; ++ atClose) {
		Log l;
		l.m_log.beginBlock(LogLevel::Debug);
		write(&l.m_log, LogLevel::Debug, L"head");
		if (!atClose)
			MsgbufTestAccess::forgetTheDate(&l.m_log);
		write(&l.m_log, LogLevel::Debug, L"body");
		if (atClose)
			MsgbufTestAccess::forgetTheDate(&l.m_log);
		l.m_log.endBlock();

		Lines lines = takeLines(&l.m_log);
		ENGINE_CHECK(lines.size() == 4 && isDateLine(lines[0]),
					 atClose ? "a date line made for the closing brace comes first"
							 : "a date line made for the body comes first");
		Lines rest;
		for (size_t i = 1; i < lines.size(); ++ i)
			rest.push_back((kPrefix <= lines[i].size())
						   ? lines[i].substr(kPrefix) : L"<short>");
		ENGINE_CHECK(rest == expected, "and the block after it is whole");
	}
}


/// Every line of a block carries the time it closed at.  A line another thread
/// wrote in the meantime lands before the block and is the older of the two,
/// so the log reads in time order.
void aBlockCarriesTheTimeItClosedAt()
{
	Log l;
	l.m_log.beginBlock(LogLevel::Debug);
	write(&l.m_log, LogLevel::Debug, L"head");
	Sleep(20);
	std::thread other([&l]() {
		write(&l.m_log, LogLevel::Info, L"from another thread");
	});
	other.join();
	Sleep(20);
	write(&l.m_log, LogLevel::Debug, L"body");
	l.m_log.endBlock();

	Lines lines = takeLines(&l.m_log);
	for (const std::wstring &line : lines)
		if (isDateLine(line))
			return;					// midnight passed, and the times wrap
	ENGINE_CHECK(lines.size() == 4, "the other thread's line, then the block");
	if (lines.size() != 4)
		return;
	auto timeOf = [](const std::wstring &i_line) {
		return i_line.substr(0, 12);	// "hh:mm:ss.SSS"
	};
	ENGINE_CHECK(timeOf(lines[1]) == timeOf(lines[2]) &&
				 timeOf(lines[2]) == timeOf(lines[3]),
				 "the header, the body and the brace share one time");
	ENGINE_CHECK(timeOf(lines[0]) < timeOf(lines[1]),
				 "and it is later than the line before the block");
}


///
void logBlockClosesWithItsScope()
{
	Log l;
	{
		LogBlock block(l.m_log);
		write(&l.m_log, LogLevel::Info, L"before");
		block.open(LogLevel::Info);
		block.open(LogLevel::Info);		// a second open() changes nothing
		write(&l.m_log, LogLevel::Info, L"head");
		write(&l.m_log, LogLevel::Info, L"body");
	}
	ENGINE_CHECK(l.bodies() == Lines({ L"before", L"head {", L"  body", L"}" }),
				 "it closes on the way out");

	LogBlock block(l.m_log, LogLevel::Info);
	write(&l.m_log, LogLevel::Info, L"head");
	block.close();
	block.close();						// and a second close() changes nothing
	ENGINE_CHECK(l.bodies() == Lines({ L"head" }), "or when told to");
}


/// Text not ended with a newline must still end up with the level it was
/// written at, not that of whoever writes next.
void unterminatedTextKeepsItsLevel()
{
	Log l;
	{
		Acquire a(&l.m_log, LogLevel::Debug);
		l.m_log << L"unfinished";
	}
	write(&l.m_log, LogLevel::Error, L" and finished");
	ENGINE_CHECK(l.levels() == L"D", "the level is the first writer's");
}

} // namespace


void runLogFormatTests()
{
	printf("log format\n");
	aPrefixHasOneFixedWidth();
	aDateLineHasARule();
	blankLinesAreNotWritten();
	aBlockGetsBracesAndIndentation();
	theClosingBraceTakesTheHeadersLevel();
	aBlockWithNoBodyHasNoBraces();
	aFilteredHeaderOpensNoBlock();
	aNestedBlockIsOnlyIndented();
	aFilteredNestedHeaderAddsNoIndent();
	aBlockIsHiddenUntilItCloses();
	anotherThreadsLineLandsBeforeTheBlock();
	twoThreadsKeepTheirBlocksApart();
	aClosedBlockLeavesNothingBehind();
	aDateLineLandsBeforeTheBlock();
	aBlockCarriesTheTimeItClosedAt();
	logBlockClosesWithItsScope();
	unterminatedTextKeepsItsLevel();
}
