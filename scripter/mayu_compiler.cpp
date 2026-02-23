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
	const _TCHAR *name;
	Modifier::Type type;
} g_modifierMap[] = {
	{ _T("S-"),   Modifier::Type_Shift },
	{ _T("A-"),   Modifier::Type_Alt },
	{ _T("M-"),   Modifier::Type_Alt },
	{ _T("C-"),   Modifier::Type_Control },
	{ _T("W-"),   Modifier::Type_Windows },
	{ _T("U-"),   Modifier::Type_Up },
	{ _T("D-"),   Modifier::Type_Down },
	{ _T("R-"),   Modifier::Type_Repeat },
	{ _T("IL-"),  Modifier::Type_ImeLock },
	{ _T("IC-"),  Modifier::Type_ImeComp },
	{ _T("I-"),   Modifier::Type_ImeComp },
	{ _T("NL-"),  Modifier::Type_NumLock },
	{ _T("CL-"),  Modifier::Type_CapsLock },
	{ _T("SL-"),  Modifier::Type_ScrollLock },
	{ _T("KL-"),  Modifier::Type_KanaLock },
	{ _T("MAX-"), Modifier::Type_Maximized },
	{ _T("MIN-"), Modifier::Type_Minimized },
	{ _T("MMAX-"), Modifier::Type_MdiMaximized },
	{ _T("MMIN-"), Modifier::Type_MdiMinimized },
	{ _T("T-"),   Modifier::Type_Touchpad },
	{ _T("TS-"),  Modifier::Type_TouchpadSticky },
	{ _T("M0-"),  Modifier::Type_Mod0 },
	{ _T("M1-"),  Modifier::Type_Mod1 },
	{ _T("M2-"),  Modifier::Type_Mod2 },
	{ _T("M3-"),  Modifier::Type_Mod3 },
	{ _T("M4-"),  Modifier::Type_Mod4 },
	{ _T("M5-"),  Modifier::Type_Mod5 },
	{ _T("M6-"),  Modifier::Type_Mod6 },
	{ _T("M7-"),  Modifier::Type_Mod7 },
	{ _T("M8-"),  Modifier::Type_Mod8 },
	{ _T("M9-"),  Modifier::Type_Mod9 },
	{ _T("L0-"),  Modifier::Type_Lock0 },
	{ _T("L1-"),  Modifier::Type_Lock1 },
	{ _T("L2-"),  Modifier::Type_Lock2 },
	{ _T("L3-"),  Modifier::Type_Lock3 },
	{ _T("L4-"),  Modifier::Type_Lock4 },
	{ _T("L5-"),  Modifier::Type_Lock5 },
	{ _T("L6-"),  Modifier::Type_Lock6 },
	{ _T("L7-"),  Modifier::Type_Lock7 },
	{ _T("L8-"),  Modifier::Type_Lock8 },
	{ _T("L9-"),  Modifier::Type_Lock9 },
};


//=============================================================================
// Constructor
//=============================================================================

MayuCompiler::MayuCompiler(
	CmdStreamWriter &writer,
	const Symbols &initialSymbols,
	ConfigFiles &configFiles,
	SyncObject *soLog,
	tostream *log)
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
	file.accept(*this);
}


//=============================================================================
// Helpers
//=============================================================================

void MayuCompiler::error(const AstSourceLoc &loc, const tstring &msg)
{
	m_hasErrors = true;
	if (m_log && m_soLog) {
		Acquire a(m_soLog);
		*m_log << loc.filename << _T("(") << loc.line
			   << _T(") : error: ") << msg << std::endl;
	}
}


CmdModifier MayuCompiler::compileModifierSpecs(
	const std::vector<AstModifierSpec> &specs)
{
	CmdModifier mod;

	// Collect which modifier bits were explicitly specified (not wildcard).
	// Wildcards ("*" / "~" sentinels) are recognised by their name value.
	uint64_t explicitBits = 0;
	for (const auto &spec : specs) {
		// Find the modifier type
		for (size_t i = 0; i < NUMBER_OF(g_modifierMap); ++i) {
			if (_tcsicmp(spec.name.c_str(), g_modifierMap[i].name) == 0) {
				Modifier::Type mt = g_modifierMap[i].type;
				uint64_t bit = static_cast<uint64_t>(1) << mt;
				explicitBits |= bit;
				switch (spec.flag) {
				case AstModifierSpec::Press:
					mod.modifiers |= bit;
					mod.dontcares &= ~bit;
					break;
				case AstModifierSpec::Release:
					mod.modifiers &= ~bit;
					mod.dontcares &= ~bit;
					break;
				case AstModifierSpec::Dontcare:
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
		if (spec.name == _T("*") || spec.name == _T("~")) {
			const uint64_t allBits =
				(static_cast<uint64_t>(1) << Modifier::Type_ASSIGN) - 1;
			const uint64_t unspecBits = allBits & ~explicitBits;
			if (spec.flag == AstModifierSpec::Dontcare) {
				mod.dontcares |= unspecBits;
				mod.modifiers  &= ~unspecBits;
			} else if (spec.flag == AstModifierSpec::Release) {
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
		if (_tcsicmp(ext.c_str(), _T("E0-")) == 0)
			bsc.flags |= ScanCode::E0;
		else if (_tcsicmp(ext.c_str(), _T("E1-")) == 0)
			bsc.flags |= ScanCode::E1;
	}
	return bsc;
}


CmdArgument MayuCompiler::compileArgument(const AstArgument &arg)
{
	CmdArgument ba;
	switch (arg.kind) {
	case AstArgument::Kind_String:
		ba.type = CmdArgument::String;
		ba.stringValue = arg.stringValue;
		break;
	case AstArgument::Kind_Number:
		ba.type = CmdArgument::Number;
		ba.numberValue = arg.numberValue;
		break;
	case AstArgument::Kind_Regexp:
		ba.type = CmdArgument::Regexp;
		ba.stringValue = arg.stringValue;
		break;
	case AstArgument::Kind_KeySeqRef:
		ba.type = CmdArgument::String;
		ba.stringValue = arg.stringValue;
		break;
	case AstArgument::Kind_KeySeqLiteral:
		if (arg.keySeq) {
			ba.type = CmdArgument::KeySeqIdx;
			ba.keySeqIndex = compileKeySequence(*arg.keySeq);
		}
		break;
	case AstArgument::Kind_ModifierSeq:
		ba.type = CmdArgument::ModSeq;
		ba.modifierValue = compileModifierSpecs(arg.modifierSeq);
		break;
	case AstArgument::Kind_TokenSeq:
		ba.type = CmdArgument::TokenSeq;
		ba.tokens = arg.tokens;
		break;
	}
	return ba;
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
	CmdKeySequence bks;
	for (const auto &action : seq.actions)
		bks.actions.push_back(compileAction(*action));

	uint32_t idx = m_nextKeySeqIdx++;
	m_writer.writeDefKeySeq(bks);
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
	CmdDefSymbolData data;
	data.symbolName = node.symbol;
	m_writer.writeDefSymbol(data);
}


void MayuCompiler::visit(const AstInclude &node)
{
	// Resolve the include filename to a full path via home directories
	tstringi resolvedPath;
	if (!m_configFiles.getFilename(node.filename, &resolvedPath, nullptr)) {
		error(node.m_loc,
			  _T("include file not found: `") + node.filename + _T("'."));
		m_hasErrors = true;
		return;
	}

	if (m_log) *m_log << _T("  loading: ") << resolvedPath << std::endl;

	MayuParser parser;
	auto ast = parser.parseFile(resolvedPath, m_configFiles);
	if (parser.hasErrors()) {
		if (m_log && m_soLog) {
			Acquire a(m_soLog);
			for (const auto &msg : parser.getMessages())
				*m_log << msg << std::endl;
		}
		error(node.m_loc,
			  _T("errors in included file `") + node.filename + _T("'."));
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
	CmdDefKeyData data;
	data.names = node.names;
	for (const auto &sc : node.scanCodes)
		data.scanCodes.push_back(compileScanCode(sc));
	m_writer.writeDefKey(data);
}


void MayuCompiler::visit(const AstDefModifier &node)
{
	CmdDefModifierData data;
	data.modifierName = node.modifierName;
	data.keyNames = node.keyNames;
	m_writer.writeDefModifier(data);
}


void MayuCompiler::visit(const AstDefSync &node)
{
	CmdDefSyncData data;
	for (const auto &sc : node.scanCodes)
		data.scanCodes.push_back(compileScanCode(sc));
	m_writer.writeDefSync(data);
}


void MayuCompiler::visit(const AstDefAlias &node)
{
	CmdDefAliasData data;
	data.aliasName = node.aliasName;
	data.keyName = node.keyName;
	m_writer.writeDefAlias(data);
}


void MayuCompiler::visit(const AstDefSubstitute &node)
{
	CmdDefSubstituteData data;
	for (const auto &mkey : node.lhsKeys) {
		CmdModifiedKey bmk;
		bmk.modifier = compileModifierSpecs(mkey.modifiers);
		bmk.keyName = mkey.keyName;
		data.lhsKeys.push_back(std::move(bmk));
	}
	if (node.rhsKeySeq)
		data.rhsKeySeqIdx = compileKeySequence(*node.rhsKeySeq);
	m_writer.writeDefSubstitute(data);
}


void MayuCompiler::visit(const AstDefOption &node)
{
	CmdDefOptionData data;
	data.optionName = node.optionName;
	data.qualifier = node.qualifier;
	data.value = node.value;
	m_writer.writeDefOption(data);
}


//=============================================================================
// AstVisitor: keymap and assignments
//=============================================================================

void MayuCompiler::visit(const AstKeymapDef &node)
{
	CmdKeymapDefData data;
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
	m_writer.writeKeymapDef(data);
}


void MayuCompiler::visit(const AstKeyAssign &node)
{
	CmdKeyAssignData data;
	for (const auto &mkey : node.lhsKeys) {
		CmdModifiedKey bmk;
		bmk.modifier = compileModifierSpecs(mkey.modifiers);
		bmk.keyName = mkey.keyName;
		data.lhsKeys.push_back(std::move(bmk));
	}
	if (node.rhsKeySeq)
		data.rhsKeySeqIdx = compileKeySequence(*node.rhsKeySeq);
	m_writer.writeKeyAssign(data);
}


void MayuCompiler::visit(const AstKeyDefaultModifier &node)
{
	CmdKeyDefaultModData data;
	data.assignMod = compileModifierSpecs(node.assignModifier);
	data.keySeqMod = compileModifierSpecs(node.keySeqModifier);
	m_writer.writeKeyDefaultMod(data);
}


void MayuCompiler::visit(const AstEventAssign &node)
{
	CmdEventAssignData data;
	data.eventName = node.eventName;
	if (node.keySeq)
		data.rhsKeySeqIdx = compileKeySequence(*node.keySeq);
	m_writer.writeEventAssign(data);
}


void MayuCompiler::visit(const AstModifierAssign &node)
{
	CmdModAssignData data;
	for (const auto &p : node.prefixes) {
		CmdModAssignData::PrefixMod pm;
		pm.assignMode = p.assignMode;
		pm.modifierName = p.modifierName;
		data.prefixes.push_back(pm);
	}
	data.mainModifierName = node.mainModifierName;
	data.op = node.op;
	for (const auto &k : node.keys) {
		CmdModAssignData::KeyEntry ke;
		ke.assignMode = k.assignMode;
		ke.keyName = k.keyName;
		data.keys.push_back(ke);
	}
	m_writer.writeModAssign(data);
}


void MayuCompiler::visit(const AstKeySeqDef &node)
{
	CmdKeySeqDefData data;
	if (node.keySeq) {
		CmdKeySequence bks;
		bks.name = node.name;
		for (const auto &action : node.keySeq->actions)
			bks.actions.push_back(compileAction(*action));

		data.keySeqIdx = m_nextKeySeqIdx++;
		m_writer.writeDefKeySeq(bks);
	}
	m_writer.writeKeySeqDef(data);
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

