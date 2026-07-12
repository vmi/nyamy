//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// test_harness.cpp

#include "misc.h"

#include "test_harness.h"

#include "yamy_scripter.h"     // ys_start, YsCallbacks (DLL C API)
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
// matching how ys_start() parses YS_CTRL / YS_CMD.
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
                                      int i_loadCount)
{
	// ctrl pipe: test (write) -> scripter (read, via YS_CTRL)
	// data pipe: scripter (write, via YS_CMD) -> test (read)
	HANDLE ctrlR = nullptr, ctrlW = nullptr, dataR = nullptr, dataW = nullptr;
	if (!CreatePipe(&ctrlR, &ctrlW, nullptr, 0)) return nullptr;
	if (!CreatePipe(&dataR, &dataW, nullptr, 0)) {
		CloseHandle(ctrlR); CloseHandle(ctrlW);
		return nullptr;
	}

	setHandleEnv(L"YS_CTRL", ctrlR);   // scripter reads this end
	setHandleEnv(L"YS_CMD",  dataW);   // scripter writes this end

	// Scripter context: argv = { program, scriptPath }.
	const char *argv[2] = { "yamy-scripter-tests", i_scriptPathUtf8.c_str() };
	MRubyContext ctx = { 2, argv, nullptr };
	YsCallbacks cb = {};
	cb.on_load_setting = mruby_on_load_setting;
	cb.on_quit         = mruby_on_quit;

	std::thread scripterThread([&]() { ys_start(&cb, &ctx); });

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
	std::thread consumerThread([&]() { proc.process(reader); });

	// Send the Start command (carries the symbol set); once more per extra
	// load to exercise the reload path.
	PipeWriteStreambuf ctrlBuf(ctrlW);
	std::ostream ctrlOut(&ctrlBuf);
	CtrlStreamWriter ctrlWriter(ctrlOut);
	for (int i = 0; i < i_loadCount; ++i)
		ctrlWriter.writeStart(wstringi(L"test"), wstringi(L""), i_symbols);
	ctrlOut.flush();

	// Wait for the Commit (or time out on a Ruby error / hang).
	std::shared_ptr<Setting> result;
	if (committedFut.wait_for(std::chrono::seconds(30)) == std::future_status::ready)
		result = committedFut.get();

	// Quit the scripter; this closes dataW, giving the consumer EOF.
	ctrlWriter.writeQuit();
	ctrlOut.flush();

	scripterThread.join();
	consumerThread.join();

	// ctrlR and dataW are closed by ys_start(); close our remaining ends.
	CloseHandle(ctrlW);
	CloseHandle(dataR);
	return result;
}
