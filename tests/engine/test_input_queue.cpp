//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// test_input_queue.cpp
//
// The input queue between its producers (the hooks, the UI thread and the
// scripter's data thread) and the engine thread that drains it.  Every access
// to the queue has to be made under m_queueMutex.  The engine thread used to
// make its accesses without it: SignalObjectAndWait() releases the mutex and
// does not take it back, and two key events arriving together could then leave
// the second one waiting for the next key, or lose it, or read it twice.
//
// These run the real engine thread through Engine::start().  The hooks are
// stubs here, so nothing is installed - but a key event the engine thread
// processes goes out through SendInput.  So no KEYBOARD_INPUT_DATA is queued
// here, and the engine runs in log mode with no Setting: a key event made up
// out of an empty queue is then written to the log instead of reaching the
// desktop.  What is queued are ad-hoc key sequences, which without a Setting
// are dropped without a word as soon as they are taken.  Each one holds a
// reference to a token, whose use count therefore says how many are still in
// the queue.


#include "engine_test.h"
#include <barrier>
#include <chrono>
#include <cstdlib>
#include <random>
#include <thread>


namespace
{

/// how long the engine thread gets to do what it must do
const DWORD kTimeoutMillisec = 1000;

/// how long the engine thread gets to do what it must not do
const DWORD kGraceMillisec = 100;


/// an item the engine thread drops as soon as it takes it from the queue
AdHocKeySeq makeItem(const std::shared_ptr<Setting> &i_token)
{
	AdHocKeySeq item(new AdHocItem());
	item->keySeq = std::make_unique<KeySeq>(L"");
	item->origin = i_token;
	return item;
}


/// wait until the engine thread has taken every item made with i_token
bool waitDrained(const std::shared_ptr<Setting> &i_token,
				 DWORD i_timeout = kTimeoutMillisec)
{
	const ULONGLONG deadline = GetTickCount64() + i_timeout;
	while (1 < i_token.use_count()) {
		if (deadline <= GetTickCount64())
			return false;
		SwitchToThread();
	}
	return true;
}


/// wait until the engine thread has taken the wake-up off i_event
bool waitReset(HANDLE i_event)
{
	const ULONGLONG deadline = GetTickCount64() + kTimeoutMillisec;
	while (WaitForSingleObject(i_event, 0) == WAIT_OBJECT_0) {
		if (deadline <= GetTickCount64())
			return false;
		SwitchToThread();
	}
	return true;
}


/** End the run after a failure that leaves the engine thread holding
    m_queueMutex for good.  Stopping it takes that lock too, so the run would
    hang rather than report. */
[[noreturn]] void giveUp()
{
	printf("\nFAILED (the engine thread keeps the queue locked and cannot be "
		   "stopped; the rest is not run)\n");
	fflush(stdout);
	_exit(1);
}


/// Take m_queueMutex, which the engine thread may hold only for moments, from
/// this thread.  Does not come back without it.
void lockQueue(Engine *io_engine, const char *i_what)
{
	const bool isLocked =
		WaitForSingleObject(EngineTestAccess::queueMutex(io_engine),
							kTimeoutMillisec) == WAIT_OBJECT_0;
	ENGINE_CHECK(isLocked, i_what);
	if (!isLocked)
		giveUp();
}


///
void unlockQueue(Engine *io_engine)
{
	ReleaseMutex(EngineTestAccess::queueMutex(io_engine));
}


/// whether another thread can take m_queueMutex; see lockQueue()
void checkLockIsFree(Engine *io_engine, const char *i_what)
{
	lockQueue(io_engine, i_what);
	unlockQueue(io_engine);
}


/// Engine::start() and the shutdown nyamy puts it through, as one scope
class RunningEngine
{
public:
	///
	explicit RunningEngine(Engine *io_engine)
		: m_engine(io_engine) {
		m_engine->start();
	}

	///
	~RunningEngine() {
		stop();
	}

	RunningEngine(const RunningEngine &) = delete;
	RunningEngine &operator=(const RunningEngine &) = delete;

	/// whether the engine thread came to an end within the timeout
	bool stop() {
		if (!m_engine)
			return true;
		HANDLE thread = m_engine->signalStop();
		const bool hasEnded =
			WaitForSingleObject(thread, kTimeoutMillisec) == WAIT_OBJECT_0;
		m_engine->cleanupAfterStop(thread);
		m_engine = NULL;
		return hasEnded;
	}

private:
	Engine *m_engine;
};


/** Nothing is taken from the queue while another thread holds m_queueMutex.
    The engine thread came back from SignalObjectAndWait() without it and went
    straight for the queue, where it could pop an item in the middle of a
    producer's push. */
void theQueueIsLeftAloneWhileLocked()
{
	womsgstream log(WM_APP);
	Engine engine(log);
	engine.enableLogMode();
	std::shared_ptr<Setting> token = std::make_shared<Setting>();
	RunningEngine running(&engine);

	// One round first.  Locked right after start(), the engine thread would be
	// stopped before its first wait, which is no test of what follows it.
	engine.scheduleAdHocKeySeq(makeItem(token));
	ENGINE_CHECK(waitDrained(token), "an item is taken");
	Sleep(kGraceMillisec);

	lockQueue(&engine, "the queue can be locked while the engine waits");
	// takes the mutex once more, which is what makes it safe to call here
	engine.scheduleAdHocKeySeq(makeItem(token));
	Sleep(kGraceMillisec);
	ENGINE_CHECK(token.use_count() == 2,
				 "an item queued under the lock stays there while it is held");
	unlockQueue(&engine);
	ENGINE_CHECK(waitDrained(token), "and is taken once the lock is released");
}


/** A wake-up that finds the queue empty goes back to waiting.  It used to end
    in a failed wait instead, which left the loop with nothing taken and ran a
    key event made up of nothing - scan code 0 pressed. */
void aWakeUpWithNothingQueuedMakesNoKeyEvent()
{
	womsgstream log(WM_APP);
	Engine engine(log);
	engine.enableLogMode();
	std::shared_ptr<Setting> token = std::make_shared<Setting>();
	{
		RunningEngine running(&engine);
		HANDLE readEvent = EngineTestAccess::readEvent(&engine);

		SetEvent(readEvent);
		ENGINE_CHECK(waitReset(readEvent),
					 "the engine thread takes a wake-up with nothing queued");
		// one item behind it, to know when the engine thread is past it
		engine.scheduleAdHocKeySeq(makeItem(token));
		ENGINE_CHECK(waitDrained(token), "and goes on to the next item");
	}
	ENGINE_CHECK(!logHas(takeLog(&log), L"no setting yet"),
				 "without running a key event in between");
}


/** However it was woken, the engine thread waits holding none of
    m_queueMutex.  SignalObjectAndWait() releases a single level, so a path
    that took it twice would keep it through the wait - and every hook would
    stall behind it. */
void theLockIsFreeWhileTheEngineWaits()
{
	womsgstream log(WM_APP);
	Engine engine(log);
	engine.enableLogMode();
	std::shared_ptr<Setting> token = std::make_shared<Setting>();
	RunningEngine running(&engine);
	HANDLE readEvent = EngineTestAccess::readEvent(&engine);

	checkLockIsFree(&engine, "before anything is queued");

	engine.scheduleAdHocKeySeq(makeItem(token));
	ENGINE_CHECK(waitDrained(token), "an item is taken");
	Sleep(kGraceMillisec);
	checkLockIsFree(&engine, "after taking an item");

	SetEvent(readEvent);
	ENGINE_CHECK(waitReset(readEvent), "a wake-up with nothing queued is taken");
	Sleep(kGraceMillisec);
	checkLockIsFree(&engine, "after a wake-up with nothing queued");

	engine.unlocked();
	ENGINE_CHECK(waitReset(readEvent), "a resync request is taken");
	Sleep(kGraceMillisec);
	checkLockIsFree(&engine, "after a resync request");
}


///
void theEngineThreadStops()
{
	womsgstream log(WM_APP);
	Engine engine(log);
	engine.enableLogMode();
	std::shared_ptr<Setting> token = std::make_shared<Setting>();
	RunningEngine running(&engine);

	engine.scheduleAdHocKeySeq(makeItem(token));
	ENGINE_CHECK(waitDrained(token), "an item is taken");
	ENGINE_CHECK(running.stop(), "the engine thread ends on a stop request");
}


/** Pairs of items from two threads at once, over and over: the pattern of a
    modifier released together with the key it modified.  Every pair has to be
    taken without a further wake-up.  A race does not show on demand, so a pass
    proves little; this is here to catch what it can. */
void itemsArrivingTogetherAreAllTaken()
{
	const int kRounds = 3000;
	const DWORD kRoundTimeoutMillisec = 500;
	womsgstream log(WM_APP);
	Engine engine(log);
	engine.enableLogMode();
	std::shared_ptr<Setting> token = std::make_shared<Setting>();
	int failedRound = -1;
	{
		RunningEngine running(&engine);
		// the two producers and this thread: once to start a round, once more
		// when both items are in
		std::barrier<> sync(3);
		auto produce = [&](unsigned i_seed) {
			std::minstd_rand random(i_seed);
			for (int i = 0; i < kRounds; ++ i) {
				sync.arrive_and_wait();
				// up to 50 us apart, about what two keys released together are
				const auto until = std::chrono::steady_clock::now() +
					std::chrono::microseconds(random() % 50);
				while (std::chrono::steady_clock::now() < until)
					;
				engine.scheduleAdHocKeySeq(makeItem(token));
				sync.arrive_and_wait();
			}
		};
		std::thread first(produce, 1u);
		std::thread second(produce, 2u);
		for (int i = 0; i < kRounds; ++ i) {
			sync.arrive_and_wait();
			sync.arrive_and_wait();
			// after a failure the rounds only run the producers out
			if (failedRound < 0 && !waitDrained(token, kRoundTimeoutMillisec))
				failedRound = i;
		}
		first.join();
		second.join();
	}
	if (0 <= failedRound)
		printf("  first round left with an item untaken: %d\n", failedRound);
	ENGINE_CHECK(failedRound < 0, "every pair is taken as it arrives");
	ENGINE_CHECK(!logHas(takeLog(&log), L"no setting yet"),
				 "and no key event is made up in between");
}

} // namespace


void runInputQueueTests()
{
	theQueueIsLeftAloneWhileLocked();
	aWakeUpWithNothingQueuedMakesNoKeyEvent();
	theLockIsFreeWhileTheEngineWaits();
	theEngineThreadStops();
	itemsArrivingTogetherAreAllTaken();
}
