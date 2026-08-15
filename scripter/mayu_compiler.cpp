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

void MayuCompiler::compile(const AstFile &file,
						   uint32_t initialKeySeqIdx,
						   bool writeSymbols)
{
	m_hasErrors = false;
	m_nextKeySeqIdx = initialKeySeqIdx;

	if (writeSymbols) {
		for (const auto &sym : m_symbols) {
			CmdArgsDefSymbol data;
			data.symbolName = sym;
			m_writer.writeDefSymbol(data);
		}
	}

	file.accept(*this);
}


//=============================================================================
// Helpers
//=============================================================================

void MayuCompiler::error(const AstSourceLoc &loc, const std::wstring &msg)
{
	m_hasErrors = true;
	if (!m_log)
		return;
	auto emit = [&]() {
		*m_log << loc.filename << L"(" << loc.line
			   << L") : error: " << msg << std::endl;
	};
	// callers that own the log stream pass no lock object
	if (m_soLog) {
		Acquire a(m_soLog);
		emit();
	} else {
		emit();
	}
}


namespace {

/// every modifier type as a bit mask
constexpr uint64_t allModifierBits =
	(static_cast<uint64_t>(1) << Modifier::Type_ASSIGN) - 1;

inline uint64_t modifierBit(Modifier::Type type)
{
	return static_cast<uint64_t>(1) << type;
}

/// The first modifier type that may not be specified in i_context.  Types from
/// here up are forced to dontcare, as the old loader's load_MODIFIER() did for
/// everything at or above its i_mode.
Modifier::Type contextLimit(ModifierContext context)
{
	return context == ModifierContext::KeySeq ? Modifier::Type_KEYSEQ
											  : Modifier::Type_ASSIGN;
}

/// The state of the modifiers a prefix says nothing about.
ModifierSpec modifierDefaults(ModifierContext context)
{
	ModifierSpec mod;
	// a MODIFIER argument starts out caring about nothing; everywhere else the
	// defaults are the ones Modifier() itself supplies
	mod.dontcares = context == ModifierContext::Argument
		? allModifierBits : Modifier::defaultDontcares();
	return mod;
}

} // namespace


/*static*/
ModifierSpec MayuCompiler::compileModifierSpecs(
	const std::vector<AstModifierSpec> &specs,
	ModifierContext context,
	wstringi *o_invalidName)
{
	ModifierSpec mod = modifierDefaults(context);

	// Types the context does not allow are dontcare and count as specified, so
	// that a trailing wildcard does not touch them either.
	uint64_t specifiedBits = 0;
	for (int i = contextLimit(context); i < Modifier::Type_ASSIGN; ++i)
		specifiedBits |= modifierBit(static_cast<Modifier::Type>(i));
	mod.dontcares |= specifiedBits;
	mod.modifiers &= ~specifiedBits;

	// Apply the explicit specifiers.  Wildcards ("*" / "~" sentinels) are
	// recognised by their name value and handled below.
	for (const auto &spec : specs) {
		for (size_t i = 0; i < NUMBER_OF(g_modifierMap); ++i) {
			if (_wcsicmp(spec.name.c_str(), g_modifierMap[i].name) != 0)
				continue;
			Modifier::Type mt = g_modifierMap[i].type;
			if (contextLimit(context) <= mt) {
				if (o_invalidName && o_invalidName->empty())
					*o_invalidName = spec.name;
				break;
			}
			uint64_t bit = modifierBit(mt);
			specifiedBits |= bit;
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

	// Apply wildcard sentinel ("*" = all-dontcare, "~" = all-release) to every
	// modifier bit that was not specified.  This replicates the old pipeline's
	// "trailing flag" behaviour (e.g. bare * before a key name).
	for (const auto &spec : specs) {
		if (spec.name == L"*" || spec.name == L"~") {
			const uint64_t unspecBits = allModifierBits & ~specifiedBits;
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

	// Fix up U- / D-: specifying one of them implies the other is its opposite,
	// and caring for both with the same value means neither matters.
	{
		const uint64_t up = modifierBit(Modifier::Type_Up);
		const uint64_t down = modifierBit(Modifier::Type_Down);
		bool isDontcareUp = !!(mod.dontcares & up);
		bool isDontcareDown = !!(mod.dontcares & down);
		bool isOnUp = !!(mod.modifiers & up);
		bool isOnDown = !!(mod.modifiers & down);
		if (isDontcareUp && isDontcareDown)
			;
		else if (isDontcareUp) {
			mod.dontcares &= ~up;
			if (isOnDown) mod.modifiers &= ~up; else mod.modifiers |= up;
		} else if (isDontcareDown) {
			mod.dontcares &= ~down;
			if (isOnUp) mod.modifiers &= ~down; else mod.modifiers |= down;
		} else if (isOnUp == isOnDown) {
			mod.dontcares |= up | down;
			mod.modifiers &= ~(up | down);
		}
	}

	// R- only ever matters when it is spelled out.
	if (!(specifiedBits & modifierBit(Modifier::Type_Repeat))) {
		mod.dontcares |= modifierBit(Modifier::Type_Repeat);
		mod.modifiers &= ~modifierBit(Modifier::Type_Repeat);
	}

	return mod;
}


ModifierSpec MayuCompiler::compileModifiers(
	const std::vector<AstModifierSpec> &specs,
	ModifierContext context,
	const AstSourceLoc &loc)
{
	wstringi invalidName;
	ModifierSpec mod = compileModifierSpecs(specs, context, &invalidName);
	if (!invalidName.empty())
		error(loc, L"`" + invalidName + L"': invalid modifier at this context.");
	return mod;
}


/*static*/
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
			return FuncArgKeySeqIdx{
				compileKeySequence(*arg.keySeq, ModifierContext::KeySeq) };
		break;
	case AstArgument::Kind::ModifierSeq:
		return FuncArgModifierSpec{
			compileModifierSpecs(arg.modifierSeq, ModifierContext::Argument) };
	case AstArgument::Kind::TokenSeq:
		return FuncArgTokenSeq{ arg.tokens };
	}
	return FuncArgString{};
}


CmdAction MayuCompiler::compileAction(const AstAction &action,
									  ModifierContext context)
{
	CmdAction ba;
	ba.modifier = compileModifiers(action.modifiers, context, m_currentLoc);

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
				ba.subActions.push_back(compileAction(*a, context));
		}
	}
	return ba;
}


uint32_t MayuCompiler::compileKeySequence(const AstKeySequence &seq,
										  ModifierContext context)
{
	CmdArgsRegKeySeq bks;
	for (const auto &action : seq.actions) {
		m_currentLoc = seq.m_loc;
		bks.actions.push_back(compileAction(*action, context));
	}

	if (m_subSeqCollector) {
		// Collection mode: gather the sub-sequence's actions and return its
		// position in the collector (children were appended first -> post-order).
		m_subSeqCollector->push_back(std::move(bks.actions));
		return static_cast<uint32_t>(m_subSeqCollector->size() - 1);
	}

	uint32_t idx = m_nextKeySeqIdx++;
	m_writer.writeRegKeySeq(bks);
	return idx;
}


std::vector<CmdAction> MayuCompiler::compileActions(const AstKeySequence &seq,
													ModifierContext context)
{
	std::vector<CmdAction> result;
	result.reserve(seq.actions.size());
	for (const auto &action : seq.actions) {
		m_currentLoc = seq.m_loc;
		result.push_back(compileAction(*action, context));
	}
	return result;
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
		bmk.modifier = compileModifiers(mkey.modifiers, ModifierContext::Assign,
										node.m_loc);
		bmk.keyName = mkey.keyName;
		data.lhsKeys.push_back(std::move(bmk));
	}
	// the substitute target may carry ASSIGN-class modifiers
	if (node.rhsKeySeq)
		data.rhsKeySeqIdx =
			compileKeySequence(*node.rhsKeySeq, ModifierContext::Assign);
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
	// A .mayu keymap statement stays in effect until the next one: there is no
	// block form in that language, so Enter is the only scope it ever emits.
	CmdArgsDefKeymap data;
	data.scope = CmdKeymapScope::Enter;
	data.keyword = node.keyword;
	data.name = node.name;
	if (node.window) {
		data.windowClassName = node.window->className;
		data.windowTitleName = node.window->titleName;
		data.windowOp = node.window->op;
	}
	data.parentName = node.parentName;
	if (node.defaultKeySeq)
		data.defaultKeySeqIdx = static_cast<int32_t>(
			compileKeySequence(*node.defaultKeySeq, ModifierContext::KeySeq));
	m_writer.writeDefKeymap(data);
}


void MayuCompiler::visit(const AstKeyAssign &node)
{
	CmdArgsAssignKey data;
	for (const auto &mkey : node.lhsKeys) {
		CmdModifiedKey bmk;
		bmk.modifier = compileModifiers(mkey.modifiers, ModifierContext::Assign,
										node.m_loc);
		bmk.keyName = mkey.keyName;
		data.lhsKeys.push_back(std::move(bmk));
	}
	if (node.rhsKeySeq)
		data.rhsKeySeqIdx =
			compileKeySequence(*node.rhsKeySeq, ModifierContext::KeySeq);
	m_writer.writeAssignKey(data);
}


void MayuCompiler::visit(const AstEventAssign &node)
{
	CmdArgsAssignEvent data;
	data.eventName = node.eventName;
	if (node.keySeq)
		data.rhsKeySeqIdx =
			compileKeySequence(*node.keySeq, ModifierContext::KeySeq);
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
		// a named key sequence may carry ASSIGN-class modifiers
		for (const auto &action : node.keySeq->actions) {
			m_currentLoc = node.m_loc;
			bks.actions.push_back(
				compileAction(*action, ModifierContext::Assign));
		}

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

