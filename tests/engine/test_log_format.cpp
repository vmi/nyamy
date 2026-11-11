//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// test_log_format.cpp
//
// What a log line and a log block look like, and the promises made about
// them: no blank lines, one fixed width prefix, and a block that reaches the
// reader whole with its braces and indentation worked out by the stream.


#include "engine_test.h"
#include "function.h"

#include <memory>
#include <sstream>
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


/// the rules every line has to keep, whoever wrote it
void checkShape(const Lines &i_lines, const char *i_what)
{
	bool isInBlock = false;
	for (const std::wstring &line : i_lines) {
		if (isDateLine(line))
			continue;
		ENGINE_CHECK(kPrefix < line.size() && line[kPrefix - 1] == L' ', i_what);
		if (line.size() <= kPrefix)
			continue;
		const std::wstring body = line.substr(kPrefix);
		const size_t spaces = body.find_first_not_of(L' ');
		ENGINE_CHECK(spaces != std::wstring::npos, i_what);
		ENGINE_CHECK(body.back() != L' ', i_what);
		ENGINE_CHECK(spaces == std::wstring::npos ||
					 body.find(L"  ", spaces) == std::wstring::npos, i_what);
		const bool opens = 2 <= body.size() &&
						   body.compare(body.size() - 2, 2, L" {") == 0;
		if (!isInBlock) {
			ENGINE_CHECK(spaces == 0, i_what);
			ENGINE_CHECK(body != L"}", i_what);
			isInBlock = opens;
		} else if (body == L"}") {
			isInBlock = false;
		} else {
			ENGINE_CHECK(2 <= spaces && spaces % 2 == 0, i_what);
			ENGINE_CHECK(!opens, i_what);
		}
	}
	ENGINE_CHECK(!isInBlock, i_what);
}


/// a Key named "A", as the keyboard definition would make it
struct KeyA
{
	Key m_key;
	ModifiedKey m_mkey;

	///
	KeyA() : m_mkey(&m_key) {
		m_key.addName(L"A");
		m_key.addScanCode(ScanCode(0x1e, 0));
		m_mkey.m_modifier.on(Modifier::Type_Down);
	}
};


/// An engine and its log, with the startup chatter already taken away.
class Rig
{
public:
	womsgstream m_log;
	Engine m_engine;

	///
	explicit Rig(LogLevel i_threshold = LogLevel::Debug)
			: m_log(WM_APP), m_engine(m_log) {
		m_log.setThreshold(i_threshold);
		write(&m_log, LogLevel::Error, L"start");
		takeLog(&m_log);
	}

	/// the lines since the last call, as written
	Lines lines() {
		return takeLines(&m_log);
	}

	/// the text of one line
	static std::wstring body(const std::wstring &i_line) {
		return i_line.substr(kPrefix);
	}
};


///
void aKeyEventIsOneBlock()
{
	Rig r;
	KeyA a;
	EngineTestAccess::outputInputToLog(&r.m_engine, &a.m_key, a.m_mkey,
									   LogLevel::Debug);
	EngineTestAccess::logNote(&r.m_engine, LogLevel::Debug, L"a note");
	EngineTestAccess::outputToLog(&r.m_engine, &a.m_key, a.m_mkey,
								  LogLevel::Debug, L"OUT");
	ENGINE_CHECK(takeLog(&r.m_log).empty(), "nothing shows until the event ends");
	EngineTestAccess::closeInputBlock(&r.m_engine);

	Lines lines = r.lines();
	checkShape(lines, "a key event keeps the shape");
	ENGINE_CHECK(lines.size() == 4, "header, two lines and the brace");
	if (lines.size() != 4)
		return;
	ENGINE_CHECK(Rig::body(lines[0]).compare(0, 2, L"IN") == 0, "IN is the header");
	ENGINE_CHECK(Rig::body(lines[1]).find(L"a note") != std::wstring::npos &&
				 Rig::body(lines[1]).compare(0, 2, L"  ") == 0,
				 "the body is indented by the stream");
	ENGINE_CHECK(Rig::body(lines[3]) == L"}", "and it ends with the brace");

	EngineTestAccess::closeInputBlock(&r.m_engine);
	ENGINE_CHECK(takeLog(&r.m_log).empty(), "closing twice writes nothing");
}


/// That the key is a modifier key is found out before its IN line is written,
/// and said inside its block all the same: it is about this key event.
void aModifierKeyIsSaidInsideItsKeyEvent()
{
	Rig r;
	KeyA a;
	EngineTestAccess::outputInputToLog(&r.m_engine, &a.m_key, a.m_mkey,
									   LogLevel::Debug, true);
	EngineTestAccess::closeInputBlock(&r.m_engine);

	Lines lines = r.lines();
	checkShape(lines, "a modifier key's event keeps the shape");
	ENGINE_CHECK(lines.size() == 3, "header, the note and the brace");
	if (lines.size() != 3)
		return;
	ENGINE_CHECK(Rig::body(lines[0]).compare(0, 2, L"IN") == 0, "IN is the header");
	ENGINE_CHECK(Rig::body(lines[1]).compare(0, 3, L"  *") == 0 &&
				 Rig::body(lines[1]).find(L"Modifier Key") != std::wstring::npos,
				 "the note is the first line of its body");
	ENGINE_CHECK(Rig::body(lines[2]) == L"}", "and the brace ends it");
}


/// What the investigate dialog's log mode writes: the IN line and no more.
void aKeyEventWithNothingElseIsOneLine()
{
	Rig r(LogLevel::Info);
	KeyA a;
	EngineTestAccess::outputInputToLog(&r.m_engine, &a.m_key, a.m_mkey,
									   LogLevel::Info);
	EngineTestAccess::logNote(&r.m_engine, LogLevel::Debug, L"filtered");
	EngineTestAccess::closeInputBlock(&r.m_engine);

	Lines lines = r.lines();
	checkShape(lines, "a lone IN line keeps the shape");
	ENGINE_CHECK(lines.size() == 1 &&
				 lines[0].find(L'{') == std::wstring::npos,
				 "one line and no braces");
}


/// In normal mode the IN line is filtered, and a failure has no block to be in.
void aFailureWithoutItsKeyEventStandsAlone()
{
	Rig r(LogLevel::Info);
	KeyA a;
	EngineTestAccess::outputInputToLog(&r.m_engine, &a.m_key, a.m_mkey,
									   LogLevel::Debug);
	EngineTestAccess::logNote(&r.m_engine, LogLevel::Warn, L"sync timed out");
	EngineTestAccess::closeInputBlock(&r.m_engine);

	Lines lines = r.lines();
	checkShape(lines, "a lone failure keeps the shape");
	ENGINE_CHECK(lines.size() == 1 &&
				 lines[0].find(L"sync timed out") != std::wstring::npos,
				 "only the failure is written");
}


///
void describeBindingsIsABlock()
{
	Rig r(LogLevel::Info);
	EngineTestAccess::applySetting(&r.m_engine, makeGlobalSetting());
	takeLog(&r.m_log);

	EngineTestAccess::describeBindings(&r.m_engine);
	Lines lines = r.lines();
	checkShape(lines, "the bindings keep the shape");
	ENGINE_CHECK(3 <= lines.size(), "header, a keymap and the brace");
	if (lines.size() < 3)
		return;
	ENGINE_CHECK(Rig::body(lines[0]) == L"DescribeBindings {", "it has a header");
	ENGINE_CHECK(Rig::body(lines[1]).compare(0, 9, L"  window ") == 0,
				 "and the keymap heads its own part of the body");
}


/// Run from a key in detail mode, the key event's block is already open.
void describeBindingsNestsInAKeyEvent()
{
	Rig r;
	EngineTestAccess::applySetting(&r.m_engine, makeGlobalSetting());
	takeLog(&r.m_log);
	KeyA a;

	EngineTestAccess::outputInputToLog(&r.m_engine, &a.m_key, a.m_mkey,
									   LogLevel::Debug);
	EngineTestAccess::describeBindings(&r.m_engine);
	EngineTestAccess::closeInputBlock(&r.m_engine);

	Lines lines = r.lines();
	checkShape(lines, "nested bindings keep the shape");
	ENGINE_CHECK(4 <= lines.size(), "the event, its header, a keymap, the brace");
	if (lines.size() < 4)
		return;
	ENGINE_CHECK(Rig::body(lines[1]) == L"  DescribeBindings",
				 "no braces inside the key event's block");
	ENGINE_CHECK(Rig::body(lines[2]).compare(0, 11, L"    window ") == 0,
				 "and its body goes one step deeper");
}


/// A wait says when it began and how long it took.  The line itself carries
/// the time the key event's block closes, which is after the wait.
void aWaitSaysWhenItBeganAndHowLongItTook()
{
	Rig r;
	EngineTestAccess::funcWait(&r.m_engine, 50);
	Lines lines;
	bool hasDateLine = false;			// midnight passed, and the times wrap
	for (const std::wstring &line : r.lines()) {
		if (isDateLine(line))
			hasDateLine = true;
		else
			lines.push_back(line);
	}
	ENGINE_CHECK(lines.size() == 1, "one line for the wait");
	if (lines.size() != 1)
		return;

	// what follows the mark; the mark's own spacing is not what this is about
	const std::wstring body = Rig::body(lines[0]);
	const size_t at = body.find(L"waited ");
	unsigned long long ms = 0;
	unsigned hh = 99, mm = 99, ss = 99, sss = 9999;
	int length = 0;
	const bool isParsed = body.compare(0, 1, L"*") == 0 &&
		at != std::wstring::npos &&
		swscanf_s(body.c_str() + at,
				  L"waited %llu ms, started %2u:%2u:%2u.%3u%n",
				  &ms, &hh, &mm, &ss, &sss, &length) == 5 &&
		at + static_cast<size_t>(length) == body.size();
	ENGINE_CHECK(isParsed, "it reads \"* waited N ms, started hh:mm:ss.SSS\"");
	if (!isParsed)
		return;
	ENGINE_CHECK(hh < 24 && mm < 60 && ss < 60 && sss < 1000,
				 "the start is a time of day");
	// a timeout may end up to a timer tick early
	ENGINE_CHECK(40 <= ms && ms < 5000, "the wait is about as long as asked");
	ENGINE_CHECK(hasDateLine ||
				 body.substr(body.size() - 12) <= lines[0].substr(0, 12),
				 "and it began before the line was written");
}


/// the text of every line after the first
Lines bodiesAfterTheFirst(const Lines &i_lines)
{
	Lines bodies;
	for (size_t i = 1; i < i_lines.size(); ++ i)
		bodies.push_back((kPrefix <= i_lines[i].size())
						 ? Rig::body(i_lines[i]) : L"<short>");
	return bodies;
}


/// A key with no assignment walks from keymap to keymap, one function inside
/// the next.  Each closes its own block before handing the key on, so the
/// chain reads down the page at one depth rather than a step deeper each time.
void aKeyHandedOnStaysAtOneDepth()
{
	ActionFunction toParent(createFunctionData(L"KeymapParent"));
	ActionFunction ignore(createFunctionData(L"Ignore"));
	KeySeq handOn(L"");
	handOn.add(toParent);
	KeySeq stop(L"");
	stop.add(ignore);
	Keymap last(Keymap::Type_keymap, L"Last", L"", L"", &stop, NULL);
	Keymap middle(Keymap::Type_keymap, L"Middle", L"", L"", &handOn, &last);
	Keymap first(Keymap::Type_keymap, L"First", L"", L"", &handOn, &middle);

	Rig r;
	KeyA a;
	EngineTestAccess::outputInputToLog(&r.m_engine, &a.m_key, a.m_mkey,
									   LogLevel::Debug);
	EngineTestAccess::generateKeyboardEvents(&r.m_engine, a.m_mkey, &first,
											 { &first });
	EngineTestAccess::closeInputBlock(&r.m_engine);

	Lines lines = r.lines();
	checkShape(lines, "a chain keeps the shape");
	ENGINE_CHECK(bodiesAfterTheFirst(lines) ==
				 Lines({ L"  FN &KeymapParent",
						 L"    * keymap: Middle",
						 L"  FN &KeymapParent",
						 L"    * keymap: Last",
						 L"  FN &Ignore",
						 L"}" }),
				 "every function of the chain at one depth, each with its report");
}


/// &KeymapWindow and &OtherWindowClass hand the key on as well, and say where
/// to.  The function that ends the chain keeps what it does nested under it.
void theLastOfAChainKeepsItsOutput()
{
	ActionFunction toWindow(createFunctionData(L"KeymapWindow"));
	ActionFunction toNext(createFunctionData(L"OtherWindowClass"));
	ActionFunction describe(createFunctionData(L"DescribeBindings"));
	KeySeq window(L"");
	window.add(toWindow);
	KeySeq next(L"");
	next.add(toNext);
	KeySeq bindings(L"");
	bindings.add(describe);
	Keymap second(Keymap::Type_windowOr, L"Second", L"", L"", &bindings, NULL);
	Keymap first(Keymap::Type_windowOr, L"First", L"", L"", &next, NULL);
	Keymap start(Keymap::Type_keymap, L"Start", L"", L"", &window, NULL);

	{
		Rig r;
		KeyA a;
		EngineTestAccess::outputInputToLog(&r.m_engine, &a.m_key, a.m_mkey,
										   LogLevel::Debug);
		EngineTestAccess::generateKeyboardEvents(&r.m_engine, a.m_mkey, &start,
												 { &first, &second });
		EngineTestAccess::closeInputBlock(&r.m_engine);

		Lines lines = r.lines();
		checkShape(lines, "a chain through window keymaps keeps the shape");
		Lines bodies = bodiesAfterTheFirst(lines);
		ENGINE_CHECK(7 <= bodies.size(), "the chain, the bindings and the brace");
		if (bodies.size() < 7)
			return;
		ENGINE_CHECK(Lines(bodies.begin(), bodies.begin() + 5) ==
					 Lines({ L"  FN &KeymapWindow",
							 L"    * keymap: First",
							 L"  FN &OtherWindowClass",
							 L"    * keymap: Second",
							 L"  FN &DescribeBindings" }),
					 "the chain at one depth, each step reporting where it went");
		ENGINE_CHECK(bodies[5] == L"    DescribeBindings",
					 "and what the last function does stays under it");
		ENGINE_CHECK(bodies.back() == L"}", "the key event's brace ends it");
	}

	// In normal mode no function opens a block, and there is none to close.
	{
		Rig r(LogLevel::Info);
		KeyA a;
		EngineTestAccess::outputInputToLog(&r.m_engine, &a.m_key, a.m_mkey,
										   LogLevel::Debug);
		EngineTestAccess::generateKeyboardEvents(&r.m_engine, a.m_mkey, &start,
												 { &first, &second });
		EngineTestAccess::closeInputBlock(&r.m_engine);

		Lines lines = r.lines();
		checkShape(lines, "the bindings alone keep the shape");
		ENGINE_CHECK(!lines.empty() &&
					 Rig::body(lines[0]) == L"DescribeBindings {",
					 "in normal mode the bindings stand alone, unindented");
	}
}


/// &KeymapPrevPrefix(n) hands the key on to the keymap n prefixes back, and
/// does nothing when the history does not go back that far.  It used to read
/// past the end of the history then, an empty one included.
void prevPrefixGoesNoFurtherBackThanItsHistory()
{
	ActionFunction ignore(createFunctionData(L"Ignore"));
	KeySeq stop(L"");
	stop.add(ignore);
	Keymap older(Keymap::Type_keymap, L"Older", L"", L"", &stop, NULL);
	Keymap newer(Keymap::Type_keymap, L"Newer", L"", L"", &stop, NULL);
	const Keymaps::KeymapPtrList history = { &older, &newer };

	auto run = [&](const Keymaps::KeymapPtrList &i_history, int i_previous) {
		Rig r;
		KeyA a;
		EngineTestAccess::funcKeymapPrevPrefix(&r.m_engine, a.m_mkey,
											   i_history, i_previous);
		Lines bodies;
		for (const std::wstring &line : r.lines())
			bodies.push_back(Rig::body(line));
		return bodies;
	};

	ENGINE_CHECK(run(history, 1) == Lines({ L"* keymap: Newer", L"FN &Ignore" }),
				 "one back is the latest prefix keymap");
	ENGINE_CHECK(run(history, 2) == Lines({ L"* keymap: Older", L"FN &Ignore" }),
				 "two back is the one before it");
	ENGINE_CHECK(run(history, 3).empty(), "further back than the history: nothing");
	ENGINE_CHECK(run({}, 1).empty(), "an empty history: nothing");
	ENGINE_CHECK(run(history, 0).empty(), "and nothing for zero either");
}


///
void anActivationHasADetailBody()
{
	{
		Rig r;
		EngineTestAccess::applySetting(&r.m_engine, makeGlobalSetting());
		Lines lines = r.lines();
		checkShape(lines, "activation keeps the shape in detail mode");
		ENGINE_CHECK(lines.size() == 3, "header, details and the brace");
		if (lines.size() == 3)
			ENGINE_CHECK(Rig::body(lines[1]).compare(0, 17,
													 L"  global keymaps:") == 0,
						 "the details are the body");
	}

	Rig r(LogLevel::Info);
	EngineTestAccess::applySetting(&r.m_engine, makeGlobalSetting());
	Lines lines = r.lines();
	checkShape(lines, "activation keeps the shape in normal mode");
	ENGINE_CHECK(lines.size() == 1 && lines[0].find(L'{') == std::wstring::npos,
				 "with the details filtered it is one plain line");
}


/// what one call of outputToLog() writes, with the prefix taken off
std::wstring keyLine(Rig *io_r, const Key &i_key, const ModifiedKey &i_mkey,
					 const wchar_t *i_note = NULL)
{
	EngineTestAccess::outputToLog(&io_r->m_engine, &i_key, i_mkey,
								  LogLevel::Debug, L"OUT", i_note);
	Lines lines = io_r->lines();
	checkShape(lines, "a key line keeps the shape");
	return lines.size() == 1 ? Rig::body(lines[0]) : L"<not one line>";
}


///
void aKeyLineHasTheNameFirst()
{
	Rig r;
	KeyA a;
	ENGINE_CHECK(keyLine(&r, a.m_key, a.m_mkey) == L"OUT D-A (0x1e)",
				 "name, then the scan code in parentheses");
	ENGINE_CHECK(keyLine(&r, a.m_key, a.m_mkey, L"(already released)")
				 == L"OUT D-A (0x1e) (already released)",
				 "a note goes at the end, where it moves nothing");

	Key left;
	left.addName(L"Left");
	left.addScanCode(ScanCode(0x4b, ScanCode::E0));
	ModifiedKey leftUp(&left);
	leftUp.m_modifier.on(Modifier::Type_Up);
	ENGINE_CHECK(keyLine(&r, left, leftUp) == L"OUT U-Left (E0-0x4b)",
				 "an extended key says so");

	Key pause;
	pause.addName(L"Pause");
	pause.addScanCode(ScanCode(0x1d, ScanCode::E1));
	pause.addScanCode(ScanCode(0x45, 0));
	ModifiedKey pauseDown(&pause);
	pauseDown.m_modifier.on(Modifier::Type_Down);
	ENGINE_CHECK(keyLine(&r, pause, pauseDown) == L"OUT D-Pause (E1-0x1d 0x45)",
				 "several scan codes are separated by a space");

	Key unknown;
	unknown.addScanCode(ScanCode(0x7e, 0));
	ENGINE_CHECK(keyLine(&r, unknown, ModifiedKey()) == L"OUT 0x7e",
				 "with no name there is nothing to put in parentheses");

	Key event;
	event.addName(L"prefixed");
	ModifiedKey eventDown(&event);
	eventDown.m_modifier.on(Modifier::Type_Down);
	ENGINE_CHECK(keyLine(&r, event, eventDown) == L"OUT D-prefixed",
				 "an event has no scan code, and so no parentheses");
}


///
void aNoteHasOneSpaceAfterItsMark()
{
	Rig r;
	EngineTestAccess::logNote(&r.m_engine, LogLevel::Info, L"text");
	Lines lines = r.lines();
	checkShape(lines, "a note keeps the shape");
	ENGINE_CHECK(lines.size() == 1 && Rig::body(lines[0]) == L"* text",
				 "the mark and the text");
}


///
void aFunctionsTextEndsWithItsLastCharacter()
{
	std::unique_ptr<FunctionData> fd(createFunctionData(L"WindowRaise"));
	std::wostringstream text;
	text << fd.get();
	ENGINE_CHECK(!text.str().empty() && text.str().back() == L')',
				 "nothing is left over after the closing parenthesis");
}


///
void bindingsHaveNoStraySpaces()
{
	std::shared_ptr<Setting> setting = std::make_shared<Setting>();
	ActionFunction af(createFunctionData(L"OtherWindowClass"));
	KeySeq *seq = setting->m_keySeqs.add(KeySeq(L"").add(af).add(af));
	setting->m_keymaps.add(Keymap(Keymap::Type_windowOr, L"Global", L"", L"",
								  seq, NULL));

	Rig r(LogLevel::Info);
	EngineTestAccess::applySetting(&r.m_engine, setting);
	takeLog(&r.m_log);
	EngineTestAccess::describeBindings(&r.m_engine);
	Lines lines = r.lines();
	checkShape(lines, "the bindings have no stray spaces");
	ENGINE_CHECK(3 <= lines.size(), "header, a keymap and the brace");
	if (lines.size() < 3)
		return;
	const std::wstring tail = L" = &OtherWindowClass &OtherWindowClass";
	const std::wstring &line = lines[1];
	ENGINE_CHECK(tail.size() < line.size() &&
				 line.compare(line.size() - tail.size(), tail.size(), tail) == 0,
				 "a key sequence is separated by single spaces");
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
	aKeyEventIsOneBlock();
	aModifierKeyIsSaidInsideItsKeyEvent();
	aKeyEventWithNothingElseIsOneLine();
	aFailureWithoutItsKeyEventStandsAlone();
	describeBindingsIsABlock();
	describeBindingsNestsInAKeyEvent();
	aWaitSaysWhenItBeganAndHowLongItTook();
	aKeyHandedOnStaysAtOneDepth();
	theLastOfAChainKeepsItsOutput();
	prevPrefixGoesNoFurtherBackThanItsHistory();
	anActivationHasADetailBody();
	aKeyLineHasTheNameFirst();
	aNoteHasOneSpaceAfterItsMark();
	aFunctionsTextEndsWithItsLastCharacter();
	bindingsHaveNoStraySpaces();
}
