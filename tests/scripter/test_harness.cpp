//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// test_harness.cpp

#include "misc.h"

#include "test_harness.h"

#include "nyamy_scripter.h"     // nys_start, NYsCallbacks (DLL C API)
#include "mruby_binding.h"     // mruby_on_load_setting / mruby_on_quit / MRubyContext
#include "ctrl_stream_writer.h"
#include "cmd_stream_reader.h"
#include "cmd_processor.h"
#include "pipe_streambuf.h"
#include "setting.h"

#include <windows.h>
#include <chrono>
#include <future>
#include <istream>
#include <ostream>
#include <thread>


namespace {

// Store a HANDLE value into an environment variable as a decimal string,
// matching how nys_start() parses NYS_CTRL / NYS_CMD.
void setHandleEnv(const wchar_t *i_name, HANDLE i_handle)
{
	wchar_t buf[32];
	_ui64tow_s(static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(i_handle)),
	           buf, 32, 10);
	SetEnvironmentVariableW(i_name, buf);
}

// No-op lock: the consumer thread is the only writer of the log stream.
struct NullSyncObject : public SyncObject {
	void acquire() override {}
	void release() override {}
};

// Wide streambuf that writes its content to stderr as UTF-8 on flush,
// matching the scripter's log line format.
class Utf8StderrBuf : public std::wstringbuf {
protected:
	int sync() override {
		std::wstring s = str();
		if (!s.empty()) {
			int n = WideCharToMultiByte(CP_UTF8, 0, s.c_str(), (int)s.size(),
			                            nullptr, 0, nullptr, nullptr);
			std::string u8(n, '\0');
			WideCharToMultiByte(CP_UTF8, 0, s.c_str(), (int)s.size(),
			                    &u8[0], n, nullptr, nullptr);
			fwrite(u8.data(), 1, u8.size(), stderr);
			str(L"");
		}
		return 0;
	}
};

} // namespace


std::shared_ptr<Setting> buildSetting(const std::string &i_scriptPathUtf8,
                                      const Symbols &i_symbols,
                                      int i_loadCount,
                                      const std::vector<ExecUserFuncRequest>
                                          *i_execs)
{
	// ctrl pipe: test (write) -> scripter (read, via NYS_CTRL)
	// data pipe: scripter (write, via NYS_CMD) -> test (read)
	HANDLE ctrlR = nullptr, ctrlW = nullptr, dataR = nullptr, dataW = nullptr;
	if (!CreatePipe(&ctrlR, &ctrlW, nullptr, 0)) return nullptr;
	if (!CreatePipe(&dataR, &dataW, nullptr, 0)) {
		CloseHandle(ctrlR); CloseHandle(ctrlW);
		return nullptr;
	}

	setHandleEnv(L"NYS_CTRL", ctrlR);   // scripter reads this end
	setHandleEnv(L"NYS_CMD",  dataW);   // scripter writes this end

	// Scripter context: argv = { program, scriptPath }.  The script path may
	// be relative; it is resolved against the config search path, never the
	// current directory.
	const char *argv[2] = { "nyamy-scripter-tests", i_scriptPathUtf8.c_str() };
	MRubyContext ctx = {};
	ctx.argc           = 2;
	ctx.argv           = argv;
	ctx.mrb            = nullptr;
	ctx.scriptArgIndex = 1;
	ctx.includeDirs    = nullptr;
	NYsCallbacks cb = {};
	cb.on_load_setting = mruby_on_load_setting;
	cb.on_quit         = mruby_on_quit;

	std::thread scripterThread([&]() { nys_start(&cb, &ctx); });

	// Consumer: read the CmdStream and build a Setting.
	PipeReadStreambuf dataBuf(dataR);
	std::istream dataIn(&dataBuf);
	CmdStreamReader reader(dataIn);

	NullSyncObject soLog;
	Utf8StderrBuf logBuf;
	std::wostream logStream(&logBuf);
	CmdProcessor proc(&soLog, &logStream);
	std::promise<std::shared_ptr<Setting>> committed;
	std::future<std::shared_ptr<Setting>> committedFut = committed.get_future();
	int commitCount = 0;
	proc.onCommit([&](std::shared_ptr<Setting> s) {
		// Deliver the Setting of the i_loadCount-th Commit.
		if (++commitCount == i_loadCount) committed.set_value(s);
	});
	std::thread consumerThread([&]() {
		proc.process(reader);
		// End of stream without the expected Commit: the load failed, so
		// unblock the waiter instead of letting it time out.  commitCount is
		// only ever touched by this thread.
		if (commitCount < i_loadCount) committed.set_value(nullptr);
	});

	// Send the Start command (carries the symbol set); once more per extra
	// load to exercise the reload path.
	PipeWriteStreambuf ctrlBuf(ctrlW);
	std::ostream ctrlOut(&ctrlBuf);
	CtrlStreamWriter ctrlWriter(ctrlOut);
	for (int i = 0; i < i_loadCount; ++i)
		ctrlWriter.writeStart(wstringi(L"test"), wstringi(L""), i_symbols,
							  kLogLevelNormal);
	// Behind the Start commands: the ctrl stream is processed in order, so the
	// script has been loaded and its handlers registered before these run.
	if (i_execs)
		for (const auto &e : *i_execs)
			ctrlWriter.writeExecUserFunc(e.name, e.args, e.context);
	// Quit goes right behind the Start commands: the ctrl stream is processed in
	// order, so every load runs first.  Quitting closes dataW, which gives the
	// consumer EOF even when a load failed and no Commit is coming.
	ctrlWriter.writeQuit();
	ctrlOut.flush();

	// Wait for the Commit (or for the stream to end on a failed load).
	std::shared_ptr<Setting> result;
	if (committedFut.wait_for(std::chrono::seconds(30)) == std::future_status::ready)
		result = committedFut.get();

	scripterThread.join();
	consumerThread.join();

	// ctrlR and dataW are closed by nys_start(); close our remaining ends.
	CloseHandle(ctrlW);
	CloseHandle(dataR);
	return result;
}
