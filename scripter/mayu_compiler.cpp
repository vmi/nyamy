//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// mayu_compiler.cpp


#include "misc.h"

#include "mayu_compiler.h"
#include "mayu_parser.h"
#include "config_files.h"
#include "keyboard.h"

#include <algorithm>


//=============================================================================
// Modifier name to Modifier::Type mapping
//=============================================================================

static const struct {
	const wchar_t *name;
	Modifier::Type type;
} g_modifierMap[] = {
	{ L"S-",   Modifier::Type_Shift },
	{ L"A-",   Modifier::Type_Alt },
	{ L"M-",   Modifier::Type_Alt },
	{ L"C-",   Modifier::Type_Control },
	{ L"W-",   Modifier::Type_Windows },
	{ L"U-",   Modifier::Type_Up },
	{ L"D-",   Modifier::Type_Down },
	{ L"R-",   Modifier::Type_Repeat },
	{ L"IL-",  Modifier::Type_ImeLock },
	{ L"IC-",  Modifier::Type_ImeComp },
	{ L"I-",   Modifier::Type_ImeComp },
	{ L"NL-",  Modifier::Type_NumLock },
	{ L"CL-",  Modifier::Type_CapsLock },
	{ L"SL-",  Modifier::Type_ScrollLock },
	{ L"KL-",  Modifier::Type_KanaLock },
	{ L"MAX-", Modifier::Type_Maximized },
	{ L"MIN-", Modifier::Type_Minimized },
	{ L"MMAX-", Modifier::Type_MdiMaximized },
	{ L"MMIN-", Modifier::Type_MdiMinimized },
	{ L"M0-",  Modifier::Type_Mod0 },
	{ L"M1-",  Modifier::Type_Mod1 },
	{ L"M2-",  Modifier::Type_Mod2 },
	{ L"M3-",  Modifier::Type_Mod3 },
	{ L"M4-",  Modifier::Type_Mod4 },
	{ L"M5-",  Modifier::Type_Mod5 },
	{ L"M6-",  Modifier::Type_Mod6 },
	{ L"M7-",  Modifier::Type_Mod7 },
	{ L"M8-",  Modifier::Type_Mod8 },
	{ L"M9-",  Modifier::Type_Mod9 },
	{ L"L0-",  Modifier::Type_Lock0 },
	{ L"L1-",  Modifier::Type_Lock1 },
	{ L"L2-",  Modifier::Type_Lock2 },
	{ L"L3-",  Modifier::Type_Lock3 },
	{ L"L4-",  Modifier::Type_Lock4 },
	{ L"L5-",  Modifier::Type_Lock5 },
	{ L"L6-",  Modifier::Type_Lock6 },
	{ L"L7-",  Modifier::Type_Lock7 },
	{ L"L8-",  Modifier::Type_Lock8 },
	{ L"L9-",  Modifier::Type_Lock9 },
};


//=============================================================================
// Constructor
//=============================================================================

MayuCompiler::MayuCompiler(
	CmdStreamWriter &writer,
	const Symbols &initialSymbols,
	ConfigFiles &configFiles,
	SyncObject *soLog,
	std::wostream *log)
	: m_symbols(initialSymbols),
	  m_configFiles(configFiles),
	  m_soLog(soLog),
	  m_log(log),
	  m_writer(writer),
	  m_hasErrors(false),
	  m_nextKeySeqIdx(0)
{
}


//=============================================================================
// Public entry point
//=============================================================================

void MayuCompiler::compile(const AstFile &file)
{
	m_hasErrors = false;
	m_nextKeySeqIdx = 0;

	for (const auto &sym : m_symbols) {
		CmdArgsDefSymbol data;
		data.symbolName = sym;
		m_writer.writeDefSymbol(data);
	}

	file.accept(*this);
}


//=============================================================================
// Helpers
//=============================================================================

void MayuCompiler::error(const AstSourceLoc &loc, const std::wstring &msg)
{
	m_hasErrors = true;
	if (m_log && m_soLog) {
		Acquire a(m_soLog);
		*m_log << loc.filename << L"(" << loc.line
			   << L") : error: " << msg << std::endl;
	}
}


ModifierSpec MayuCompiler::compileModifierSpecs(
	const std::vector<AstModifierSpec> &specs)
{
	ModifierSpec mod;

	// Collect which modifier bits were explicitly specified (not wildcard).
	// Wildcards ("*" / "~" sentinels) are recognised by their name value.
	uint64_t explicitBits = 0;
	for (const auto &spec : specs) {
		// Find the modifier type
		for (size_t i = 0; i < NUMBER_OF(g_modifierMap); ++i) {
			if (_wcsicmp(spec.name.c_str(), g_modifierMap[i].name) == 0) {
				Modifier::Type mt = g_modifierMap[i].type;
				uint64_t bit = static_cast<uint64_t>(1) << mt;
				explicitBits |= bit;
				switch (spec.flag) {
				case AstModifierSpec::Flag::Press:
					mod.modifiers |= bit;
					mod.dontcares &= ~bit;
					break;
				case AstModifierSpec::Flag::Release:
					mod.modifiers &= ~bit;
					mod.dontcares &= ~bit;
					break;
				case AstModifierSpec::Flag::Dontcare:
					mod.dontcares |= bit;
					break;
				}
				break;
			}
		}
	}

	// Apply wildcard sentinel ("*" = all-dontcare, "~" = all-release) to every
	// modifier bit that was not explicitly specified.  This replicates the old
	// pipeline's "trailing flag" behaviour (e.g. bare * before a key name).
	for (const auto &spec : specs) {
		if (spec.name == L"*" || spec.name == L"~") {
			const uint64_t allBits =
				(static_cast<uint64_t>(1) << Modifier::Type_ASSIGN) - 1;
			const uint64_t unspecBits = allBits & ~explicitBits;
			if (spec.flag == AstModifierSpec::Flag::Dontcare) {
				mod.dontcares |= unspecBits;
				mod.modifiers  &= ~unspecBits;
			} else if (spec.flag == AstModifierSpec::Flag::Release) {
				mod.modifiers  &= ~unspecBits;
				mod.dontcares  &= ~unspecBits;
			}
			break;  // only one wildcard sentinel expected
		}
	}

	return mod;
}


CmdScanCode MayuCompiler::compileScanCode(const AstScanCode &sc)
{
	CmdScanCode bsc;
	bsc.scan = static_cast<uint16_t>(sc.scanCode);
	bsc.flags = 0;
	for (const auto &ext : sc.extensions) {
		if (_wcsicmp(ext.c_str(), L"E0-") == 0)
			bsc.flags |= ScanCode::E0;
		else if (_wcsicmp(ext.c_str(), L"E1-") == 0)
			bsc.flags |= ScanCode::E1;
	}
	return bsc;
}


FuncArg MayuCompiler::compileArgument(const AstArgument &arg)
{
	switch (arg.kind) {
	case AstArgument::Kind::String:
		return FuncArgString{ arg.stringValue };
	case AstArgument::Kind::Number:
		return FuncArgNumber{ static_cast<int32_t>(arg.numberValue) };
	case AstArgument::Kind::Regexp:
		return FuncArgRegexp{ arg.stringValue };
	case AstArgument::Kind::KeySeqRef:
		return FuncArgString{ arg.stringValue };
	case AstArgument::Kind::KeySeqLiteral:
		if (arg.keySeq)
			return FuncArgKeySeqIdx{ compileKeySequence(*arg.keySeq) };
		break;
	case AstArgument::Kind::ModifierSeq:
		return FuncArgModSeq{ compileModifierSpecs(arg.modifierSeq) };
	case AstArgument::Kind::TokenSeq:
		return FuncArgTokenSeq{ arg.tokens };
	}
	return FuncArgString{};
}


CmdAction MayuCompiler::compileAction(const AstAction &action)
{
	CmdAction ba;
	ba.modifier = compileModifierSpecs(action.modifiers);

	if (auto *key = dynamic_cast<const AstActionKey *>(&action)) {
		ba.type = CmdAction::Key;
		ba.name = key->keyName;
	} else if (auto *ref = dynamic_cast<const AstActionKeySeqRef *>(&action)) {
		ba.type = CmdAction::KeySeqRef;
		ba.name = ref->name;
	} else if (auto *func = dynamic_cast<const AstActionFuncCall *>(&action)) {
		ba.type = CmdAction::FuncCall;
		ba.name = func->functionName;
		for (const auto &arg : func->arguments)
			ba.arguments.push_back(compileArgument(*arg));
	} else if (auto *sub = dynamic_cast<const AstActionSubSeq *>(&action)) {
		ba.type = CmdAction::SubSeq;
		if (sub->sequence) {
			for (const auto &a : sub->sequence->actions)
				ba.subActions.push_back(compileAction(*a));
		}
	}
	return ba;
}


uint32_t MayuCompiler::compileKeySequence(const AstKeySequence &seq)
{
	CmdArgsRegKeySeq bks;
	for (const auto &action : seq.actions)
		bks.actions.push_back(compileAction(*action));

	uint32_t idx = m_nextKeySeqIdx++;
	m_writer.writeRegKeySeq(bks);
	return idx;
}


//=============================================================================
// AstVisitor: top-level
//=============================================================================

void MayuCompiler::visit(const AstFile &node)
{
	for (const auto &stmt : node.statements)
		stmt->accept(*this);
}


void MayuCompiler::visit(const AstConditional &node)
{
	for (const auto &branch : node.branches) {
		if (branch.symbol.empty()) {
			// "else" branch - execute it
			for (const auto &stmt : branch.body)
				stmt->accept(*this);
			return;
		}

		bool hasSymbol = m_symbols.count(branch.symbol) > 0;
		bool shouldExecute = branch.isNegated ? !hasSymbol : hasSymbol;

		for (const auto &ac : branch.andConditions) {
			bool has = m_symbols.count(ac.symbol) > 0;
			shouldExecute = shouldExecute && (ac.isNegated ? !has : has);
		}

		if (shouldExecute) {
			for (const auto &stmt : branch.body)
				stmt->accept(*this);
			return;
		}
	}
	// No branch matched - nothing to emit
}


void MayuCompiler::visit(const AstDefineSymbol &node)
{
	m_symbols.insert(node.symbol);
	CmdArgsDefSymbol data;
	data.symbolName = node.symbol;
	m_writer.writeDefSymbol(data);
}


void MayuCompiler::visit(const AstInclude &node)
{
	// Resolve the include filename to a full path via home directories
	wstringi resolvedPath;
	if (!m_configFiles.getFilename(node.filename, &resolvedPath, nullptr)) {
		error(node.m_loc,
			  L"include file not found: `" + node.filename + L"'.");
		m_hasErrors = true;
		return;
	}

	if (m_log) *m_log << L"  loading: " << resolvedPath << std::endl;

	MayuParser parser;
	auto ast = parser.parseFile(resolvedPath, m_configFiles);
	if (parser.hasErrors()) {
		if (m_log && m_soLog) {
			Acquire a(m_soLog);
			for (const auto &msg : parser.getMessages())
				*m_log << msg << std::endl;
		}
		error(node.m_loc,
			  L"errors in included file `" + node.filename + L"'.");
		m_hasErrors = true;
	}
	if (ast) {
		// Compile the included file's AST into this stream
		ast->accept(*this);
	}
}


//=============================================================================
// AstVisitor: keyboard definitions
//=============================================================================

void MayuCompiler::visit(const AstDefKey &node)
{
	CmdArgsDefKey data;
	data.names = node.names;
	for (const auto &sc : node.scanCodes)
		data.scanCodes.push_back(compileScanCode(sc));
	m_writer.writeDefKey(data);
}


void MayuCompiler::visit(const AstDefModifier &node)
{
	CmdArgsDefMod data;
	data.modifierName = node.modifierName;
	data.keyNames = node.keyNames;
	m_writer.writeDefMod(data);
}


void MayuCompiler::visit(const AstDefSync &node)
{
	CmdArgsDefSync data;
	for (const auto &sc : node.scanCodes)
		data.scanCodes.push_back(compileScanCode(sc));
	m_writer.writeDefSync(data);
}


void MayuCompiler::visit(const AstDefAlias &node)
{
	CmdArgsDefAlias data;
	data.aliasName = node.aliasName;
	data.keyName = node.keyName;
	m_writer.writeDefAlias(data);
}


void MayuCompiler::visit(const AstDefSubstitute &node)
{
	CmdArgsDefSubst data;
	for (const auto &mkey : node.lhsKeys) {
		CmdModifiedKey bmk;
		bmk.modifier = compileModifierSpecs(mkey.modifiers);
		bmk.keyName = mkey.keyName;
		data.lhsKeys.push_back(std::move(bmk));
	}
	if (node.rhsKeySeq)
		data.rhsKeySeqIdx = compileKeySequence(*node.rhsKeySeq);
	m_writer.writeDefSubst(data);
}


void MayuCompiler::visit(const AstDefOption &node)
{
	CmdArgsDefOption data;
	data.optionName = node.qualifier.empty()
		? node.optionName
		: wstringi(node.optionName + L" " + node.qualifier);
	data.value = node.value;
	m_writer.writeDefOption(data);
}


//=============================================================================
// AstVisitor: keymap and assignments
//=============================================================================

void MayuCompiler::visit(const AstKeymapDef &node)
{
	CmdArgsBeginKeymap data;
	data.keyword = node.keyword;
	data.name = node.name;
	if (node.window) {
		data.windowClassName = node.window->className;
		data.windowTitleName = node.window->titleName;
		data.windowOp = node.window->op;
	}
	data.parentName = node.parentName;
	if (node.defaultKeySeq)
		data.defaultKeySeqIdx =
			static_cast<int32_t>(compileKeySequence(*node.defaultKeySeq));
	m_writer.writeBeginKeymap(data);
}


void MayuCompiler::visit(const AstKeyAssign &node)
{
	CmdArgsAssignKey data;
	for (const auto &mkey : node.lhsKeys) {
		CmdModifiedKey bmk;
		bmk.modifier = compileModifierSpecs(mkey.modifiers);
		bmk.keyName = mkey.keyName;
		data.lhsKeys.push_back(std::move(bmk));
	}
	if (node.rhsKeySeq)
		data.rhsKeySeqIdx = compileKeySequence(*node.rhsKeySeq);
	m_writer.writeAssignKey(data);
}


void MayuCompiler::visit(const AstEventAssign &node)
{
	CmdArgsAssignEvent data;
	data.eventName = node.eventName;
	if (node.keySeq)
		data.rhsKeySeqIdx = compileKeySequence(*node.keySeq);
	m_writer.writeAssignEvent(data);
}


void MayuCompiler::visit(const AstModifierAssign &node)
{
	CmdArgsAssignMod data;
	for (const auto &p : node.prefixes) {
		CmdArgsAssignMod::PrefixMod pm;
		pm.assignMode = p.assignMode;
		pm.modifierName = p.modifierName;
		data.prefixes.push_back(pm);
	}
	data.mainModifierName = node.mainModifierName;
	data.op = node.op;
	for (const auto &k : node.keys) {
		CmdArgsAssignMod::KeyEntry ke;
		ke.assignMode = k.assignMode;
		ke.keyName = k.keyName;
		data.keys.push_back(ke);
	}
	m_writer.writeAssignMod(data);
}


void MayuCompiler::visit(const AstKeySeqDef &node)
{
	if (node.keySeq) {
		CmdArgsRegKeySeq bks;
		bks.name = node.name;
		for (const auto &action : node.keySeq->actions)
			bks.actions.push_back(compileAction(*action));

		m_nextKeySeqIdx++;
		m_writer.writeRegKeySeq(bks);
	}
}


//=============================================================================
// AstVisitor: sub-structure stubs (not called directly)
//=============================================================================

void MayuCompiler::visit(const AstKeySequence &) {}
void MayuCompiler::visit(const AstModifierSeq &) {}
void MayuCompiler::visit(const AstActionKey &) {}
void MayuCompiler::visit(const AstActionKeySeqRef &) {}
void MayuCompiler::visit(const AstActionFuncCall &) {}
void MayuCompiler::visit(const AstActionSubSeq &) {}

