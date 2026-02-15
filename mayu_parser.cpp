//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// mayu_parser.cpp


#include "misc.h"

#include "mayu_parser.h"
#include "ast_visitor.h"
#include "config_files.h"
#include "errormessage.h"

#include <algorithm>


// static member
std::shared_ptr<MayuParser::Prefixes> MayuParser::s_prefixes;


//=============================================================================
// Token access
//=============================================================================

bool MayuParser::nextLine()
{
	if (!m_lexer->getLine(&m_tokens))
		return false;
	m_ti = m_tokens.begin();
	m_lineNumber = m_lexer->getLineNumber();
	return true;
}


bool MayuParser::isEOL() const
{
	return m_ti == m_tokens.end();
}


Token *MayuParser::getToken()
{
	if (isEOL())
		throw ErrorMessage() << _T("too few words.");
	return &*(m_ti++);
}


Token *MayuParser::lookToken()
{
	if (isEOL())
		throw ErrorMessage() << _T("too few words.");
	return &*m_ti;
}


//=============================================================================
// Location and error reporting
//=============================================================================

AstSourceLoc MayuParser::currentLoc() const
{
	return AstSourceLoc(m_filename, m_lineNumber);
}


void MayuParser::error(const tstring &msg)
{
	m_hasErrors = true;
	tstringstream ss;
	ss << m_filename << _T("(") << m_lineNumber
	   << _T(") : error: ") << msg;
	m_messages.push_back(ss.str());
}


void MayuParser::warning(const tstring &msg)
{
	tstringstream ss;
	ss << m_filename << _T("(") << m_lineNumber
	   << _T(") : warning: ") << msg;
	m_messages.push_back(ss.str());
}


//=============================================================================
// Prefix initialization
//=============================================================================

static bool prefixSortPred(const tstringi &a, const tstringi &b)
{
	return b.size() < a.size();
}


void MayuParser::initPrefixes()
{
	if (s_prefixes)
		return;

	static const _TCHAR *prefixes[] = {
		_T("="), _T("=>"), _T("&&"), _T("||"), _T(":"), _T("$"), _T("&"),
		_T("-="), _T("+="), _T("!!!"), _T("!!"), _T("!"),
		_T("E0-"), _T("E1-"),
		_T("S-"), _T("A-"), _T("M-"), _T("C-"),
		_T("W-"), _T("*"), _T("~"),
		_T("U-"), _T("D-"),
		_T("R-"), _T("IL-"), _T("IC-"), _T("I-"),
		_T("NL-"), _T("CL-"), _T("SL-"), _T("KL-"),
		_T("MAX-"), _T("MIN-"), _T("MMAX-"), _T("MMIN-"),
		_T("T-"), _T("TS-"),
		_T("M0-"), _T("M1-"), _T("M2-"), _T("M3-"), _T("M4-"),
		_T("M5-"), _T("M6-"), _T("M7-"), _T("M8-"), _T("M9-"),
		_T("L0-"), _T("L1-"), _T("L2-"), _T("L3-"), _T("L4-"),
		_T("L5-"), _T("L6-"), _T("L7-"), _T("L8-"), _T("L9-"),
	};
	s_prefixes = std::make_shared<Prefixes>();
	for (size_t i = 0; i < NUMBER_OF(prefixes); ++i)
		s_prefixes->push_back(prefixes[i]);
	std::sort(s_prefixes->begin(), s_prefixes->end(), prefixSortPred);
}


//=============================================================================
// Modifier parsing helpers
//=============================================================================

bool MayuParser::isModifierToken(const Token *t) const
{
	if (!t->isString())
		return false;
	static const _TCHAR *modifiers[] = {
		_T("S-"), _T("A-"), _T("M-"), _T("C-"), _T("W-"),
		_T("U-"), _T("D-"),
		_T("R-"), _T("IL-"), _T("IC-"), _T("I-"),
		_T("NL-"), _T("CL-"), _T("SL-"), _T("KL-"),
		_T("MAX-"), _T("MIN-"), _T("MMAX-"), _T("MMIN-"),
		_T("T-"), _T("TS-"),
		_T("M0-"), _T("M1-"), _T("M2-"), _T("M3-"), _T("M4-"),
		_T("M5-"), _T("M6-"), _T("M7-"), _T("M8-"), _T("M9-"),
		_T("L0-"), _T("L1-"), _T("L2-"), _T("L3-"), _T("L4-"),
		_T("L5-"), _T("L6-"), _T("L7-"), _T("L8-"), _T("L9-"),
		_T("*"), _T("~"),
	};
	for (size_t i = 0; i < NUMBER_OF(modifiers); ++i)
		if (*t == modifiers[i])
			return true;
	return false;
}


bool MayuParser::isAssignModifierToken(const Token *t) const
{
	return isModifierToken(t);
}


std::vector<AstModifierSpec> MayuParser::parseModifierSpecs()
{
	std::vector<AstModifierSpec> specs;
	AstModifierSpec::Flag flag = AstModifierSpec::Press;

	while (!isEOL()) {
		Token *t = lookToken();
		if (!t->isString())
			break;

		if (*t == _T("*")) {
			getToken();
			flag = AstModifierSpec::Dontcare;
			continue;
		}
		if (*t == _T("~")) {
			getToken();
			flag = AstModifierSpec::Release;
			continue;
		}

		if (!isModifierToken(t))
			break;

		AstModifierSpec spec;
		spec.name = t->getString();
		spec.flag = flag;
		specs.push_back(spec);
		getToken();
		flag = AstModifierSpec::Press;
	}
	return specs;
}


//=============================================================================
// Scan code parsing
//=============================================================================

std::vector<AstScanCode> MayuParser::parseScanCodes()
{
	std::vector<AstScanCode> scanCodes;
	for (int j = 0; j < 4 && !isEOL(); ++j) {
		AstScanCode sc;
		while (true) {
			Token *t = getToken();
			if (t->isNumber()) {
				sc.scanCode = t->getNumber();
				scanCodes.push_back(sc);
				break;
			}
			if (*t == _T("E0-") || *t == _T("E1-"))
				sc.extensions.push_back(t->getString());
			else
				throw ErrorMessage() << _T("`") << *t
					<< _T("': invalid modifier.");
		}
	}
	return scanCodes;
}


//=============================================================================
// Constructor
//=============================================================================

MayuParser::MayuParser()
	: m_lexer(NULL),
	  m_lineNumber(0),
	  m_hasErrors(false)
{
}


//=============================================================================
// Public entry points
//=============================================================================

std::unique_ptr<AstFile> MayuParser::parseBuffer(
	const _TCHAR *buffer, size_t length,
	const tstringi &filename)
{
	initPrefixes();

	m_filename = filename;
	m_hasErrors = false;
	m_messages.clear();

	auto localPrefixes = s_prefixes;
	Lexer lexer(buffer, length);
	lexer.setPrefixes(localPrefixes.get());
	m_lexer = &lexer;

	auto file = std::make_unique<AstFile>();
	file->m_loc = AstSourceLoc(filename, 0);

	while (nextLine()) {
		try {
			AstNodePtr node = parseLine();
			if (node)
				file->statements.push_back(std::move(node));
		} catch (ErrorMessage &e) {
			tstringstream ss;
			ss << e;
			error(ss.str());
		}
	}

	m_lexer = NULL;
	return file;
}


std::unique_ptr<AstFile> MayuParser::parseFile(
	const tstringi &filename,
	ConfigFiles &configFiles)
{
	tstring data;
	if (!configFiles.readFile(&data, filename)) {
		m_hasErrors = true;
		tstringstream ss;
		ss << filename << _T(" : error: file not found");
		m_messages.push_back(ss.str());
		return std::make_unique<AstFile>();
	}
	return parseBuffer(data.c_str(), data.size(), filename);
}


//=============================================================================
// Top-level line dispatch
//=============================================================================

AstNodePtr MayuParser::parseLine()
{
	if (isEOL())
		return nullptr;

	Token *t = getToken();

	// Conditional symbols
	if (*t == _T("if") || *t == _T("and"))
		return parseConditional(*t == _T("and"));

	if (*t == _T("else") || *t == _T("elseif") ||
		*t == _T("elsif") || *t == _T("elif") ||
		*t == _T("or") || *t == _T("endif")) {
		// These are handled by parseConditional's multi-line loop.
		// If we see them at top-level, it's an unbalanced error.
		throw ErrorMessage() << _T("unbalanced `") << *t << _T("'.");
	}

	if (*t == _T("define"))
		return parseDefine();

	if (*t == _T("include"))
		return parseInclude();

	if (*t == _T("def"))
		return parseKeyboardDefinition();

	if (*t == _T("keymap") || *t == _T("keymap2") || *t == _T("window"))
		return parseKeymapDefinition(t->getString());

	if (*t == _T("key"))
		return parseKeyAssignOrDefaultModifier();

	if (*t == _T("event"))
		return parseEventAssign();

	if (*t == _T("mod"))
		return parseModifierAssign();

	if (*t == _T("keyseq"))
		return parseKeySeqDefinition();

	throw ErrorMessage() << _T("syntax error `") << *t << _T("'.");
}


//=============================================================================
// Conditional blocks (multi-line)
//=============================================================================

std::unique_ptr<AstConditional> MayuParser::parseConditional(bool /*isAnd*/)
{
	auto node = std::make_unique<AstConditional>();
	node->m_loc = currentLoc();

	// Parse first "if" branch's condition: ( !? SYMBOL )
	auto parseCond = [this](AstCondBranch &branch) {
		if (!getToken()->isOpenParen())
			throw ErrorMessage() << _T("there must be `(' after `if'.");
		Token *t = getToken();
		if (*t == _T("!")) {
			branch.isNegated = true;
			t = getToken();
		}
		branch.symbol = t->getString();
		if (!getToken()->isCloseParen())
			throw ErrorMessage() << _T("there must be `)'.");
	};

	// Parse inline statement on same line (if any)
	auto parseInline = [this](AstCondBranch &branch) {
		if (!isEOL()) {
			try {
				AstNodePtr stmt = parseLine();
				if (stmt)
					branch.body.push_back(std::move(stmt));
			} catch (ErrorMessage &e) {
				tstringstream ss;
				ss << e;
				error(ss.str());
			}
		}
	};

	// Consume inline AND conditions: "and ( !? SYMBOL )" on the same line
	auto parseAndConditions = [this](AstCondBranch &branch) {
		while (!isEOL() && *lookToken() == _T("and")) {
			getToken(); // consume "and"
			if (!getToken()->isOpenParen())
				throw ErrorMessage() << _T("there must be `(' after `and'.");
			Token *t = getToken();
			AstCondBranch::AndCond ac;
			if (*t == _T("!")) {
				ac.isNegated = true;
				t = getToken();
			}
			ac.symbol = t->getString();
			if (!getToken()->isCloseParen())
				throw ErrorMessage() << _T("there must be `)' after `and' condition.");
			branch.andConditions.push_back(std::move(ac));
		}
	};

	// First "if" branch
	{
		AstCondBranch branch;
		parseCond(branch);
		parseAndConditions(branch);
		bool isSingleLine = !isEOL(); // body on same line -> no endif needed
		parseInline(branch);
		node->branches.push_back(std::move(branch));
		if (isSingleLine)
			return node;
	}

	// Continue consuming lines until endif (multi-line if)
	while (nextLine()) {
		if (isEOL())
			continue;

		Token *t = lookToken();

		if (*t == _T("endif")) {
			getToken();
			return node;
		}

		if (*t == _T("elseif") || *t == _T("elsif") ||
			*t == _T("elif") || *t == _T("or")) {
			getToken();
			AstCondBranch branch;
			parseCond(branch);
			parseAndConditions(branch);
			parseInline(branch);
			node->branches.push_back(std::move(branch));
			continue;
		}

		if (*t == _T("else")) {
			getToken();
			AstCondBranch branch;
			// else branch has no condition (empty symbol)
			parseInline(branch);
			node->branches.push_back(std::move(branch));
			continue;
		}

		// Regular statement - add to last branch
		try {
			AstNodePtr stmt = parseLine();
			if (stmt && !node->branches.empty())
				node->branches.back().body.push_back(std::move(stmt));
		} catch (ErrorMessage &e) {
			tstringstream ss;
			ss << e;
			error(ss.str());
		}
	}

	// Reached EOF without endif
	error(_T("unbalanced `if'. you forget `endif', didn't you?"));
	return node;
}


//=============================================================================
// define / include
//=============================================================================

std::unique_ptr<AstDefineSymbol> MayuParser::parseDefine()
{
	auto node = std::make_unique<AstDefineSymbol>();
	node->m_loc = currentLoc();
	node->symbol = getToken()->getString();
	return node;
}


std::unique_ptr<AstInclude> MayuParser::parseInclude()
{
	auto node = std::make_unique<AstInclude>();
	node->m_loc = currentLoc();
	node->filename = getToken()->getString();
	return node;
}


//=============================================================================
// Keyboard definitions (def ...)
//=============================================================================

AstNodePtr MayuParser::parseKeyboardDefinition()
{
	Token *t = getToken();

	if (*t == _T("key")) return parseDefKey();
	if (*t == _T("mod")) return parseDefModifier();
	if (*t == _T("sync")) return parseDefSync();
	if (*t == _T("alias")) return parseDefAlias();
	if (*t == _T("subst")) return parseDefSubstitute();
	if (*t == _T("option")) return parseDefOption();

	throw ErrorMessage() << _T("syntax error `") << *t << _T("'.");
}


std::unique_ptr<AstDefKey> MayuParser::parseDefKey()
{
	auto node = std::make_unique<AstDefKey>();
	node->m_loc = currentLoc();

	Token *t = getToken();

	if (*t == _T('(')) {
		node->isParenthesized = true;
		node->names.push_back(getToken()->getString());
		while (t = getToken(), *t != _T(')'))
			node->names.push_back(t->getString());
		if (*getToken() != _T("="))
			throw ErrorMessage()
				<< _T("there must be `=' after `)'.");
	} else {
		node->names.push_back(t->getString());
		while (t = getToken(), *t != _T("="))
			node->names.push_back(t->getString());
	}

	node->scanCodes = parseScanCodes();
	return node;
}


std::unique_ptr<AstDefModifier> MayuParser::parseDefModifier()
{
	auto node = std::make_unique<AstDefModifier>();
	node->m_loc = currentLoc();
	node->modifierName = getToken()->getString();

	if (*getToken() != _T("="))
		throw ErrorMessage()
			<< _T("there must be `=' after modifier name.");

	while (!isEOL())
		node->keyNames.push_back(getToken()->getString());

	return node;
}


std::unique_ptr<AstDefSync> MayuParser::parseDefSync()
{
	auto node = std::make_unique<AstDefSync>();
	node->m_loc = currentLoc();

	if (*getToken() != _T("="))
		throw ErrorMessage()
			<< _T("there must be `=' after `sync'.");

	node->scanCodes = parseScanCodes();
	return node;
}


std::unique_ptr<AstDefAlias> MayuParser::parseDefAlias()
{
	auto node = std::make_unique<AstDefAlias>();
	node->m_loc = currentLoc();
	node->aliasName = getToken()->getString();

	if (*getToken() != _T("="))
		throw ErrorMessage()
			<< _T("there must be `=' after alias name.");

	node->keyName = getToken()->getString();
	return node;
}


std::unique_ptr<AstDefSubstitute> MayuParser::parseDefSubstitute()
{
	auto node = std::make_unique<AstDefSubstitute>();
	node->m_loc = currentLoc();

	// Parse LHS: MODIFIED_KEY+
	do {
		AstModifiedKey mkey;
		mkey.modifiers = parseModifierSpecs();
		mkey.keyName = getToken()->getString();
		node->lhsKeys.push_back(std::move(mkey));
	} while (!isEOL() &&
			 !(*lookToken() == _T("=>") || *lookToken() == _T("=")));

	getToken(); // consume "=" or "=>"

	node->rhsKeySeq = parseKeySequence(false);
	return node;
}


std::unique_ptr<AstDefOption> MayuParser::parseDefOption()
{
	auto node = std::make_unique<AstDefOption>();
	node->m_loc = currentLoc();

	Token *t = getToken();
	node->optionName = t->getString();

	if (*t == _T("delay-of")) {
		if (*getToken() != _T("!!!"))
			throw ErrorMessage()
				<< _T("there must be `!!!' after `def option delay-of'.");
		node->qualifier = _T("!!!");
	}

	if (*getToken() != _T("="))
		throw ErrorMessage()
			<< _T("there must be `=' in def option.");

	node->value = getToken()->getString();
	return node;
}


//=============================================================================
// Keymap definition
//=============================================================================

std::unique_ptr<AstKeymapDef> MayuParser::parseKeymapDefinition(
	const tstringi &keyword)
{
	auto node = std::make_unique<AstKeymapDef>();
	node->m_loc = currentLoc();
	node->keyword = keyword;
	node->name = getToken()->getString();

	if (!isEOL()) {
		Token *t = lookToken();

		if (keyword == _T("window")) {
			if (t->isOpenParen()) {
				getToken();
				auto ws = std::make_unique<AstWindowSpec>();
				ws->className = getToken()->getRegexp();
				t = getToken();
				if (*t == _T("&&"))
					ws->op = _T("&&");
				else if (*t == _T("||"))
					ws->op = _T("||");
				else
					throw ErrorMessage() << _T("`") << *t
						<< _T("': unknown operator.");
				ws->titleName = getToken()->getRegexp();
				if (!getToken()->isCloseParen())
					throw ErrorMessage()
						<< _T("there must be `)'.");
				node->window = std::move(ws);
			} else if (t->isRegexp()) {
				getToken();
				auto ws = std::make_unique<AstWindowSpec>();
				ws->className = t->getRegexp();
				ws->op = _T("&&");
				node->window = std::move(ws);
			}
		}

		if (!isEOL()) {
			t = lookToken();
			if (*t == _T(":")) {
				getToken();
				node->parentName = getToken()->getString();
			}
		}

		if (!isEOL()) {
			Token *t2 = getToken();
			if (!(*t2 == _T("=>") || *t2 == _T("=")))
				throw ErrorMessage() << _T("`") << *t2
					<< _T("': syntax error.");
			node->defaultKeySeq = parseKeySequence();
		}
	}

	return node;
}


//=============================================================================
// Key assignment / default modifier
//=============================================================================

AstNodePtr MayuParser::parseKeyAssignOrDefaultModifier()
{
	AstSourceLoc loc = currentLoc();

	// Parse modifier sequence
	std::vector<AstModifierSpec> mods = parseModifierSpecs();

	// Check if this is a default modifier change: key MOD = MOD
	if (!isEOL() && *lookToken() == _T("=")) {
		// Could be default modifier change OR key assignment.
		// Default modifier change: no key name before "="
		// We need to look ahead: if the current token is "=",
		// and we've only seen modifiers (no key names), it's default mod.
		// But we need to distinguish from "key S-a = ..." which starts
		// with modifier then key name then "=".
		//
		// In the original code, after parsing modifiers, if the next
		// token is "=", it's a default modifier change.
		getToken(); // consume "="
		auto node = std::make_unique<AstKeyDefaultModifier>();
		node->m_loc = loc;
		node->assignModifier = std::move(mods);
		node->keySeqModifier = parseModifierSpecs();
		return node;
	}

	// Key assignment: key MODIFIER* KEY+ = KEY_SEQUENCE
	auto node = std::make_unique<AstKeyAssign>();
	node->m_loc = loc;

	// First modified key
	{
		AstModifiedKey mkey;
		mkey.modifiers = std::move(mods);
		mkey.keyName = getToken()->getString();
		node->lhsKeys.push_back(std::move(mkey));
	}

	// Additional modified keys until "=" or "=>"
	while (!isEOL() &&
		   !(*lookToken() == _T("=>") || *lookToken() == _T("="))) {
		AstModifiedKey mkey;
		mkey.modifiers = parseModifierSpecs();
		mkey.keyName = getToken()->getString();
		node->lhsKeys.push_back(std::move(mkey));
	}

	getToken(); // consume "=" or "=>"

	node->rhsKeySeq = parseKeySequence();
	return node;
}


//=============================================================================
// Event assignment
//=============================================================================

std::unique_ptr<AstEventAssign> MayuParser::parseEventAssign()
{
	auto node = std::make_unique<AstEventAssign>();
	node->m_loc = currentLoc();
	node->eventName = getToken()->getString();

	Token *t = getToken();
	if (!(*t == _T("=>") || *t == _T("=")))
		throw ErrorMessage() << _T("`=' is expected.");

	node->keySeq = parseKeySequence();
	return node;
}


//=============================================================================
// Modifier assignment
//=============================================================================

std::unique_ptr<AstModifierAssign> MayuParser::parseModifierAssign()
{
	auto node = std::make_unique<AstModifierAssign>();
	node->m_loc = currentLoc();

	Token *t = getToken();

	// Parse prefix modifiers (assign_mode + modifier_name pairs)
	// and the main modifier name
	while (true) {
		tstringi assignMode;
		if (*t == _T("!"))
			assignMode = _T("!"), t = getToken();
		else if (*t == _T("!!"))
			assignMode = _T("!!"), t = getToken();
		else if (*t == _T("!!!"))
			assignMode = _T("!!!"), t = getToken();

		tstringi modName = t->getString();

		// Look ahead to see if this is the main modifier
		// (followed by "=", "+=", "-=") or a prefix
		if (assignMode.empty() && !isEOL()) {
			Token *next = lookToken();
			if (*next == _T("=") || *next == _T("+=") ||
				*next == _T("-=")) {
				node->mainModifierName = modName;
				break;
			}
		}

		if (!assignMode.empty()) {
			AstModAssignPrefix prefix;
			prefix.assignMode = assignMode;
			prefix.modifierName = modName;
			node->prefixes.push_back(prefix);

			if (isEOL())
				return node;
			t = getToken();
			continue;
		}

		// No assign mode and no operator follows - this is the main mod
		node->mainModifierName = modName;
		break;
	}

	// Operator
	t = getToken();
	if (*t == _T("="))
		node->op = _T("=");
	else if (*t == _T("+="))
		node->op = _T("+=");
	else if (*t == _T("-="))
		node->op = _T("-=");
	else
		throw ErrorMessage() << _T("`") << *t
			<< _T("': is unknown operator.");

	// RHS: (ASSIGN_MODE? KEY_NAME)+
	while (!isEOL()) {
		t = getToken();
		AstModAssignKey key;
		if (*t == _T("!"))
			key.assignMode = _T("!"), t = getToken();
		else if (*t == _T("!!"))
			key.assignMode = _T("!!"), t = getToken();
		else if (*t == _T("!!!"))
			key.assignMode = _T("!!!"), t = getToken();
		key.keyName = t->getString();
		node->keys.push_back(key);
	}

	return node;
}


//=============================================================================
// Keyseq definition
//=============================================================================

std::unique_ptr<AstKeySeqDef> MayuParser::parseKeySeqDefinition()
{
	auto node = std::make_unique<AstKeySeqDef>();
	node->m_loc = currentLoc();

	if (*getToken() != _T("$"))
		throw ErrorMessage()
			<< _T("there must be `$' after `keyseq'.");

	node->name = getToken()->getString();

	if (*getToken() != _T("="))
		throw ErrorMessage()
			<< _T("there must be `=' after keyseq name.");

	node->keySeq = parseKeySequence(false);
	return node;
}


//=============================================================================
// Key sequence
//=============================================================================

std::unique_ptr<AstKeySequence> MayuParser::parseKeySequence(bool inParen)
{
	auto seq = std::make_unique<AstKeySequence>();
	seq->m_loc = currentLoc();

	while (!isEOL()) {
		std::vector<AstModifierSpec> mods = parseModifierSpecs();

		Token *t = lookToken();

		if (t->isCloseParen() && inParen)
			break;

		if (t->isOpenParen()) {
			getToken(); // consume '('
			auto action = std::make_unique<AstActionSubSeq>();
			action->modifiers = std::move(mods);
			action->sequence = parseKeySequence(true);
			getToken(); // consume ')'
			seq->actions.push_back(std::move(action));
		} else if (*t == _T("$")) {
			getToken(); // consume '$'
			auto action = std::make_unique<AstActionKeySeqRef>();
			action->modifiers = std::move(mods);
			action->name = getToken()->getString();
			seq->actions.push_back(std::move(action));
		} else if (*t == _T("&")) {
			getToken(); // consume '&'
			auto action = std::make_unique<AstActionFuncCall>();
			action->modifiers = std::move(mods);
			action->functionName = getToken()->getString();
			action->arguments = parseArguments();
			seq->actions.push_back(std::move(action));
		} else {
			auto action = std::make_unique<AstActionKey>();
			action->modifiers = std::move(mods);
			action->keyName = getToken()->getString();
			seq->actions.push_back(std::move(action));
		}
	}

	return seq;
}


//=============================================================================
// Function arguments
//=============================================================================

std::unique_ptr<AstArgument> MayuParser::parseArgument()
{
	auto arg = std::make_unique<AstArgument>();

	Token *t = lookToken();

	if (*t == _T("$")) {
		getToken();
		arg->kind = AstArgument::Kind_KeySeqRef;
		arg->stringValue = getToken()->getString();
		return arg;
	}

	if (t->isOpenParen()) {
		getToken();
		arg->kind = AstArgument::Kind_KeySeqLiteral;
		arg->keySeq = parseKeySequence(true);
		getToken(); // consume ')'
		return arg;
	}

	if (t->isNumber()) {
		getToken();
		arg->kind = AstArgument::Kind_Number;
		arg->numberValue = t->getNumber();
		return arg;
	}

	if (t->isRegexp()) {
		getToken();
		arg->kind = AstArgument::Kind_Regexp;
		arg->stringValue = t->getRegexp();
		return arg;
	}

	// Modifier-like tokens as arguments
	if (isModifierToken(t)) {
		arg->kind = AstArgument::Kind_ModifierSeq;
		arg->modifierSeq = parseModifierSpecs();
		return arg;
	}

	// Default: string
	getToken();
	arg->kind = AstArgument::Kind_String;
	arg->stringValue = t->getString();
	return arg;
}


std::vector<std::unique_ptr<AstArgument>> MayuParser::parseArguments()
{
	std::vector<std::unique_ptr<AstArgument>> args;

	if (isEOL() || !lookToken()->isOpenParen())
		return args;

	getToken(); // consume '('

	if (!isEOL() && lookToken()->isCloseParen()) {
		getToken(); // empty argument list
		return args;
	}

	args.push_back(parseArgument());

	while (!isEOL() && lookToken()->isComma()) {
		getToken(); // consume ','
		args.push_back(parseArgument());
	}

	if (!isEOL() && lookToken()->isCloseParen())
		getToken(); // consume ')'
	else
		throw ErrorMessage() << _T("`)' expected in function arguments.");

	return args;
}
