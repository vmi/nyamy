//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// test_pipeline.cpp
//
// Compares the old pipeline (SettingLoader) and new pipeline
// (MayuParser -> MayuCompiler -> CmdInterpreter) by loading the same
// .mayu file through both paths and comparing the resulting Setting objects.

#include "misc.h"

#include "setting.h"
#include "setting_loader.h"
#include "config_files.h"
#include "mayu_parser.h"
#include "mayu_compiler.h"
#include "cmd_stream.h"
#include "keyboard.h"
#include "keymap.h"
#include "function.h"
#include "stringtool.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <vector>
#include <filesystem>

namespace fs = std::filesystem;

#ifdef UNICODE
#  define tcout std::wcout
#  define tcerr std::wcerr
#endif

using tostringstream = std::basic_ostringstream<_TCHAR>;
using tistringstream = std::basic_istringstream<_TCHAR>;

class NullSyncObject : public SyncObject
{
	public:
	void acquire() override { }
	void release() override { }
};

static NullSyncObject nullSyncObject;

//=============================================================================
// dumpSetting - produce a text representation of a Setting for comparison
//=============================================================================

static void dumpOptions(tostream &ost, const Setting &s)
{
	ost << _T("[Options]") << std::endl;
	ost << _T("correctKanaLockHandling=") << s.m_correctKanaLockHandling
		<< std::endl;
	ost << _T("sts4mayu=") << s.m_sts4mayu << std::endl;
	ost << _T("cts4mayu=") << s.m_cts4mayu << std::endl;
	ost << _T("mouseEvent=") << s.m_mouseEvent << std::endl;
	ost << _T("dragThreshold=") << s.m_dragThreshold << std::endl;
	ost << _T("oneShotRepeatableDelay=") << s.m_oneShotRepeatableDelay
		<< std::endl;
}


static void dumpSymbols(tostream &ost, const Setting &s)
{
	ost << _T("[Symbols]") << std::endl;
	for (const auto &sym : s.m_symbols)
		ost << sym << std::endl;
}


static void dumpKeyboard(tostream &ost, Setting &s)
{
	ost << _T("[Keyboard]") << std::endl;

	// Collect all keys, sort by name for deterministic output
	std::vector<Key *> keys;
	for (Keyboard::KeyIterator ki = s.m_keyboard.getKeyIterator();
		 *ki; ++ki) {
		keys.push_back(*ki);
	}
	std::sort(keys.begin(), keys.end(),
			  [](const Key *a, const Key *b) {
				  return a->getName() < b->getName();
			  });

	for (const Key *k : keys) {
		ost << _T("key ") << *k;
		for (size_t i = 0; i < k->getScanCodesSize(); ++i) {
			const ScanCode &sc = k->getScanCodes()[i];
			ost << _T(" ") << sc.m_scan;
			if (sc.m_flags & ScanCode::E0)
				ost << _T("-E0");
			if (sc.m_flags & ScanCode::E1)
				ost << _T("-E1");
		}
		ost << std::endl;
	}

	// Modifiers
	ost << _T("[Modifiers]") << std::endl;
	for (int t = Modifier::Type_begin; t < Modifier::Type_BASIC; ++t) {
		Modifier::Type mt = static_cast<Modifier::Type>(t);
		const Keyboard::Mods &mods = s.m_keyboard.getModifiers(mt);
		if (!mods.empty()) {
			ost << mt << _T(":");
			for (const Key *k : mods)
				ost << _T(" ") << *k;
			ost << std::endl;
		}
	}

	// Aliases
	ost << _T("[Aliases]") << std::endl;
	for (const auto &pair : s.m_keyboard.getAliases()) {
		ost << pair.first << _T(" = ") << *pair.second << std::endl;
	}

	// Substitutes
	ost << _T("[Substitutes]") << std::endl;
	for (const auto &sub : s.m_keyboard.getSubstitutes()) {
		ost << sub.m_mkeyFrom << _T(" => ") << sub.m_mkeyTo << std::endl;
	}
}


static void dumpKeymaps(tostream &ost, const Setting &s)
{
	ost << _T("[Keymaps]") << std::endl;
	Keymap::DescribeParam dp;
	for (const Keymap &km : s.m_keymaps) {
		km.describe(ost, &dp);
	}
}


static void dumpKeySeqs(tostream &ost, const Setting &s)
{
	ost << _T("[KeySeqs]") << std::endl;
	for (const KeySeq &ks : s.m_keySeqs) {
		ost << ks << std::endl;
	}
}


static tstring dumpSetting(Setting &s)
{
	tostringstream ost;
	dumpOptions(ost, s);
	dumpSymbols(ost, s);
	dumpKeyboard(ost, s);
	dumpKeymaps(ost, s);
	dumpKeySeqs(ost, s);
	return ost.str();
}


//=============================================================================
// Old pipeline: SettingLoader
//=============================================================================

static std::unique_ptr<Setting> loadOldPipeline(
	const tstringi &fullpath, const Symbols &initialSymbols,
	tostream &log)
{
	SettingLoader sl(&nullSyncObject, &log);
	return sl.load(fullpath, initialSymbols);
}


//=============================================================================
// New pipeline: MayuParser -> MayuCompiler -> CmdStreamWriter
//=============================================================================

static std::unique_ptr<Setting> loadNewPipeline(
	const tstringi &fullpath, const Symbols &initialSymbols,
	tostream &log)
{
	ConfigFiles configFiles(nullptr, &log);

	// 1. Parse
	MayuParser parser;
	auto ast = parser.parseFile(fullpath, configFiles);
	if (parser.hasErrors()) {
		log << _T("new pipeline: parse errors:") << std::endl;
		for (const auto &msg : parser.getMessages())
			log << _T("  ") << msg << std::endl;
		return nullptr;
	}

	// 2. Compile -> command stream (in-memory buffer)
	std::stringstream cmdBuf(std::ios::in | std::ios::out | std::ios::binary);
	CmdStreamWriter writer(cmdBuf);
	MayuCompiler compiler(writer, initialSymbols, configFiles, nullptr, &log);
	compiler.compile(*ast);
	if (compiler.hasErrors()) {
		log << _T("new pipeline: compile errors") << std::endl;
		return nullptr;
	}
	writer.writeCommit();

	// 3. Interpret command stream
	cmdBuf.seekg(0);
	SettingLoader loader(&nullSyncObject, &log);
	return loader.loadFromStream(cmdBuf, initialSymbols);
}



//=============================================================================
// Line-by-line diff
//=============================================================================

static void showDiff(const tstring &oldText, const tstring &newText,
					 tostream &out)
{
	// Split into lines
	std::vector<tstring> oldLines, newLines;

	auto splitLines = [](const tstring &text, std::vector<tstring> &lines) {
		tistringstream iss(text);
		tstring line;
		while (std::getline(iss, line)) {
			// Remove trailing \r if present
			if (!line.empty() && line.back() == _T('\r'))
				line.pop_back();
			lines.push_back(line);
		}
	};

	splitLines(oldText, oldLines);
	splitLines(newText, newLines);

	size_t maxLines = (std::max)(oldLines.size(), newLines.size());
	int diffCount = 0;

	for (size_t i = 0; i < maxLines; ++i) {
		const tstring &ol = (i < oldLines.size()) ? oldLines[i] : _T("");
		const tstring &nl = (i < newLines.size()) ? newLines[i] : _T("");

		if (ol != nl) {
			out << _T("line ") << (i + 1) << _T(":") << std::endl;
			out << _T("  old: ") << ol << std::endl;
			out << _T("  new: ") << nl << std::endl;
			++diffCount;
		}
	}

	if (diffCount == 0) {
		out << _T("OK - no differences found (")
			<< oldLines.size() << _T(" lines)") << std::endl;
	} else {
		out << diffCount << _T(" difference(s) found") << std::endl;
	}
}


//=============================================================================
// main
//=============================================================================

int _tmain(int argc, _TCHAR *argv[])
{
	setlocale(LC_ALL, "");

	// Parse arguments before changing current directory
	Symbols initialSymbols;
	tstringi inputPath;

	for (int i = 1; i < argc; ++i) {
		if (argv[i][0] == _T('-') && argv[i][1] == _T('D')) {
			initialSymbols.insert(argv[i] + 2);
		} else {
			inputPath = argv[i];
		}
	}

	if (inputPath.empty()) {
		tcerr << _T("Usage: test_pipeline [-D<SYMBOL>...] <mayufile>")
			  << std::endl;
		return 1;
	}

	tostringstream logStream;

	// Load via old pipeline
	tcout << _T("Loading via old pipeline (SettingLoader)...") << std::endl;
	auto oldSetting = loadOldPipeline(inputPath, initialSymbols, logStream);
	if (!oldSetting) {
		tcerr << _T("Old pipeline failed to load.") << std::endl;
		tcerr << logStream.str();
		return 1;
	}
	tcout << _T("  done.") << std::endl;

	// Load via new pipeline
	logStream.str(_T(""));
	tcout << _T("Loading via new pipeline (Parser->Compiler->Interpreter)...")
		  << std::endl;
	auto newSetting = loadNewPipeline(inputPath, initialSymbols, logStream);
	if (!newSetting) {
		tcerr << _T("New pipeline failed to load.") << std::endl;
		tcerr << logStream.str();
		return 1;
	}
	tcout << _T("  done.") << std::endl;

	// Dump both
	tstring oldDump = dumpSetting(*oldSetting);
	tstring newDump = dumpSetting(*newSetting);

	// Output to files (UTF-8, no BOM)
#ifdef UNICODE
	std::string oldDumpUtf8 = to_UTF_8(oldDump);
	std::string newDumpUtf8 = to_UTF_8(newDump);
#else
	std::string oldDumpUtf8 = oldDump;
	std::string newDumpUtf8 = newDump;
#endif

	auto writeUtf8 = [](const char *filename, const std::string &content) {
		std::ofstream ofs(filename, std::ios::out | std::ios::binary);
		if (ofs) {
			ofs.write(content.data(), content.size());
		}
	};
	writeUtf8("test_pipeline_old.txt", oldDumpUtf8);
	writeUtf8("test_pipeline_new.txt", newDumpUtf8);

	// Compare
	tcout << std::endl << _T("=== Comparison ===") << std::endl;
	showDiff(oldDump, newDump, tcout);

	return (oldDump == newDump) ? 0 : 1;
}
