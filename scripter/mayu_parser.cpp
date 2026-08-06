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
		throw ErrorMessage() << L"too few words.";
	return &*(m_ti++);
}


Token *MayuParser::lookToken()
{
	if (isEOL())
		throw ErrorMessage() << L"too few words.";
	return &*m_ti;
}


//=============================================================================
// Location and error reporting
//=============================================================================

AstSourceLoc MayuParser::currentLoc() const
{
	return AstSourceLoc(m_filename, m_lineNumber);
}


void MayuParser::error(const std::wstring &msg)
{
	m_hasErrors = true;
	std::wstringstream ss;
	ss << m_filename << L"(" << m_lineNumber
	   << L") : error: " << msg;
	m_messages.push_back(ss.str());
}


void MayuParser::warning(const std::wstring &msg)
{
	std::wstringstream ss;
	ss << m_filename << L"(" << m_lineNumber
	   << L") : warning: " << msg;
	m_messages.push_back(ss.str());
}


//=============================================================================
// Prefix initialization
//=============================================================================

static bool prefixSortPred(const wstringi &a, const wstringi &b)
{
	return b.size() < a.size();
}


void MayuParser::initPrefixes()
{
	if (s_prefixes)
		return;

	static const wchar_t *prefixes[] = {
		L"=", L"=>", L"&&", L"||", L":", L"$", L"&", L"@",
		L"-=", L"+=", L"!!!", L"!!", L"!",
		L"E0-", L"E1-",
		L"S-", L"A-", L"M-", L"C-",
		L"W-", L"*", L"~",
		L"U-", L"D-",
		L"R-", L"IL-", L"IC-", L"I-",
		L"NL-", L"CL-", L"SL-", L"KL-",
		L"MAX-", L"MIN-", L"MMAX-", L"MMIN-",
		L"M0-", L"M1-", L"M2-", L"M3-", L"M4-",
		L"M5-", L"M6-", L"M7-", L"M8-", L"M9-",
		L"L0-", L"L1-", L"L2-", L"L3-", L"L4-",
		L"L5-", L"L6-", L"L7-", L"L8-", L"L9-",
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
	return isModifierTokenName(t->getString());
}


bool MayuParser::isModifierTokenName(const wstringi &name) const
{
	static const wchar_t *modifiers[] = {
		L"S-", L"A-", L"M-", L"C-", L"W-",
		L"U-", L"D-",
		L"R-", L"IL-", L"IC-", L"I-",
		L"NL-", L"CL-", L"SL-", L"KL-",
		L"MAX-", L"MIN-", L"MMAX-", L"MMIN-",
		L"M0-", L"M1-", L"M2-", L"M3-", L"M4-",
		L"M5-", L"M6-", L"M7-", L"M8-", L"M9-",
		L"L0-", L"L1-", L"L2-", L"L3-", L"L4-",
		L"L5-", L"L6-", L"L7-", L"L8-", L"L9-",
		L"*", L"~",
	};
	for (size_t i = 0; i < NUMBER_OF(modifiers); ++i)
		if (name == modifiers[i])
			return true;
	return false;
}


std::vector<AstModifierSpec> MayuParser::tokensToModifierSpecs(
	const std::vector<wstringi> &tokens) const
{
	std::vector<AstModifierSpec> specs;
	AstModifierSpec::Flag flag = AstModifierSpec::Flag::Press;

	for (const auto &tok : tokens) {
		if (tok == L"*") {
			flag = AstModifierSpec::Flag::Dontcare;
			continue;
		}
		if (tok == L"~") {
			flag = AstModifierSpec::Flag::Release;
			continue;
		}
		AstModifierSpec spec;
		spec.name = tok;
		spec.flag = flag;
		specs.push_back(spec);
		flag = AstModifierSpec::Flag::Press;
	}
	// Handle trailing bare * or ~
	if (flag != AstModifierSpec::Flag::Press) {
		AstModifierSpec wildcard;
		wildcard.name = (flag == AstModifierSpec::Flag::Dontcare) ? L"*" : L"~";
		wildcard.flag = flag;
		specs.push_back(wildcard);
	}
	return specs;
}


bool MayuParser::isAssignModifierToken(const Token *t) const
{
	return isModifierToken(t);
}


std::vector<AstModifierSpec> MayuParser::parseModifierSpecs()
{
	std::vector<AstModifierSpec> specs;
	AstModifierSpec::Flag flag = AstModifierSpec::Flag::Press;

	while (!isEOL()) {
		Token *t = lookToken();
		if (!t->isString())
			break;

		if (*t == L"*") {
			getToken();
			flag = AstModifierSpec::Flag::Dontcare;
			continue;
		}
		if (*t == L"~") {
			getToken();
			flag = AstModifierSpec::Flag::Release;
			continue;
		}

		if (!isModifierToken(t))
			break;

		AstModifierSpec spec;
		spec.name = t->getString();
		spec.flag = flag;
		specs.push_back(spec);
		getToken();
		flag = AstModifierSpec::Flag::Press;
	}

	// If a bare * or ~ was seen (no following modifier token), add a wildcard
	// sentinel so the compiler can apply dontcare/release to all unspecified
	// modifiers.  This replicates the old pipeline's "trailing flag" behavior.
	if (flag != AstModifierSpec::Flag::Press) {
		AstModifierSpec wildcard;
		wildcard.name = (flag == AstModifierSpec::Flag::Dontcare) ? L"*" : L"~";
		wildcard.flag = flag;
		specs.push_back(wildcard);
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
			if (*t == L"E0-" || *t == L"E1-")
				sc.extensions.push_back(t->getString());
			else
				throw ErrorMessage() << L"`" << *t
					<< L"': invalid modifier.";
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
	const wchar_t *buffer, size_t length,
	const wstringi &filename)
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
			std::wstringstream ss;
			ss << e;
			error(ss.str());
		}
	}

	m_lexer = NULL;
	return file;
}


std::unique_ptr<AstFile> MayuParser::parseFile(
	const wstringi &filename,
	ConfigFiles &configFiles)
{
	std::wstring data;
	if (!configFiles.readFile(&data, filename)) {
		m_hasErrors = true;
		std::wstringstream ss;
		ss << filename << L" : error: file not found";
		m_messages.push_back(ss.str());
		return std::make_unique<AstFile>();
	}
	return parseBuffer(data.c_str(), data.size(), filename);
}


std::unique_ptr<AstKeySequence> MayuParser::parseActions(
	const wchar_t *buffer, size_t length,
	const wstringi &filename)
{
	initPrefixes();

	m_filename = filename;
	m_hasErrors = false;
	m_messages.clear();

	auto localPrefixes = s_prefixes;
	Lexer lexer(buffer, length);
	lexer.setPrefixes(localPrefixes.get());
	m_lexer = &lexer;

	std::unique_ptr<AstKeySequence> seq;
	if (nextLine()) {
		try {
			seq = parseKeySequence(false);
		} catch (ErrorMessage &e) {
			std::wstringstream ss;
			ss << e;
			error(ss.str());
		}
	}

	m_lexer = nullptr;

	if (!seq)
		seq = std::make_unique<AstKeySequence>();
	return seq;
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
	if (*t == L"if" || *t == L"and")
		return parseConditional(*t == L"and");

	if (*t == L"else" || *t == L"elseif" ||
		*t == L"elsif" || *t == L"elif" ||
		*t == L"or" || *t == L"endif") {
		// These are handled by parseConditional's multi-line loop.
		// If we see them at top-level, it's an unbalanced error.
		throw ErrorMessage() << L"unbalanced `" << *t << L"'.";
	}

	if (*t == L"define")
		return parseDefine();

	if (*t == L"include")
		return parseInclude();

	if (*t == L"def")
		return parseKeyboardDefinition();

	if (*t == L"keymap" || *t == L"keymap2" || *t == L"window")
		return parseKeymapDefinition(t->getString());

	if (*t == L"key")
		return parseKeyAssign();

	if (*t == L"event")
		return parseEventAssign();

	if (*t == L"mod")
		return parseModifierAssign();

	if (*t == L"keyseq")
		return parseKeySeqDefinition();

	throw ErrorMessage() << L"syntax error `" << *t << L"'.";
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
			throw ErrorMessage() << L"there must be `(' after `if'.";
		Token *t = getToken();
		if (*t == L"!") {
			branch.isNegated = true;
			t = getToken();
		}
		branch.symbol = t->getString();
		if (!getToken()->isCloseParen())
			throw ErrorMessage() << L"there must be `)'.";
	};

	// Parse inline statement on same line (if any)
	auto parseInline = [this](AstCondBranch &branch) {
		if (!isEOL()) {
			try {
				AstNodePtr stmt = parseLine();
				if (stmt)
					branch.body.push_back(std::move(stmt));
			} catch (ErrorMessage &e) {
				std::wstringstream ss;
				ss << e;
				error(ss.str());
			}
		}
	};

	// Consume inline AND conditions: "and ( !? SYMBOL )" on the same line
	auto parseAndConditions = [this](AstCondBranch &branch) {
		while (!isEOL() && *lookToken() == L"and") {
			getToken(); // consume "and"
			if (!getToken()->isOpenParen())
				throw ErrorMessage() << L"there must be `(' after `and'.";
			Token *t = getToken();
			AstCondBranch::AndCond ac;
			if (*t == L"!") {
				ac.isNegated = true;
				t = getToken();
			}
			ac.symbol = t->getString();
			if (!getToken()->isCloseParen())
				throw ErrorMessage() << L"there must be `)' after `and' condition.";
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

		if (*t == L"endif") {
			getToken();
			return node;
		}

		if (*t == L"elseif" || *t == L"elsif" ||
			*t == L"elif" || *t == L"or") {
			getToken();
			AstCondBranch branch;
			parseCond(branch);
			parseAndConditions(branch);
			parseInline(branch);
			node->branches.push_back(std::move(branch));
			continue;
		}

		if (*t == L"else") {
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
			std::wstringstream ss;
			ss << e;
			error(ss.str());
		}
	}

	// Reached EOF without endif
	error(L"unbalanced `if'. you forget `endif', didn't you?");
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

	if (*t == L"key") return parseDefKey();
	if (*t == L"mod") return parseDefModifier();
	if (*t == L"sync") return parseDefSync();
	if (*t == L"alias") return parseDefAlias();
	if (*t == L"subst") return parseDefSubstitute();
	if (*t == L"option") return parseDefOption();

	throw ErrorMessage() << L"syntax error `" << *t << L"'.";
}


std::unique_ptr<AstDefKey> MayuParser::parseDefKey()
{
	auto node = std::make_unique<AstDefKey>();
	node->m_loc = currentLoc();

	Token *t = getToken();

	if (*t == L'(') {
		node->isParenthesized = true;
		node->names.push_back(getToken()->getString());
		while (t = getToken(), *t != L')')
			node->names.push_back(t->getString());
		if (*getToken() != L"=")
			throw ErrorMessage()
				<< L"there must be `=' after `)'.";
	} else {
		node->names.push_back(t->getString());
		while (t = getToken(), *t != L"=")
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

	if (*getToken() != L"=")
		throw ErrorMessage()
			<< L"there must be `=' after modifier name.";

	while (!isEOL())
		node->keyNames.push_back(getToken()->getString());

	return node;
}


std::unique_ptr<AstDefSync> MayuParser::parseDefSync()
{
	auto node = std::make_unique<AstDefSync>();
	node->m_loc = currentLoc();

	if (*getToken() != L"=")
		throw ErrorMessage()
			<< L"there must be `=' after `sync'.";

	node->scanCodes = parseScanCodes();
	return node;
}


std::unique_ptr<AstDefAlias> MayuParser::parseDefAlias()
{
	auto node = std::make_unique<AstDefAlias>();
	node->m_loc = currentLoc();
	node->aliasName = getToken()->getString();

	if (*getToken() != L"=")
		throw ErrorMessage()
			<< L"there must be `=' after alias name.";

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
			 !(*lookToken() == L"=>" || *lookToken() == L"="));

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

	if (*t == L"delay-of") {
		if (*getToken() != L"!!!")
			throw ErrorMessage()
				<< L"there must be `!!!' after `def option delay-of'.";
		node->qualifier = L"!!!";
	}

	if (*getToken() != L"=")
		throw ErrorMessage()
			<< L"there must be `=' in def option.";

	node->value = getToken()->getString();
	return node;
}


//=============================================================================
// Keymap definition
//=============================================================================

std::unique_ptr<AstKeymapDef> MayuParser::parseKeymapDefinition(
	const wstringi &keyword)
{
	auto node = std::make_unique<AstKeymapDef>();
	node->m_loc = currentLoc();
	node->keyword = keyword;
	node->name = getToken()->getString();

	if (!isEOL()) {
		Token *t = lookToken();

		if (keyword == L"window") {
			if (t->isOpenParen()) {
				getToken();
				auto ws = std::make_unique<AstWindowSpec>();
				ws->className = getToken()->getRegexp();
				t = getToken();
				if (*t == L"&&")
					ws->op = L"&&";
				else if (*t == L"||")
					ws->op = L"||";
				else
					throw ErrorMessage() << L"`" << *t
						<< L"': unknown operator.";
				ws->titleName = getToken()->getRegexp();
				if (!getToken()->isCloseParen())
					throw ErrorMessage()
						<< L"there must be `)'.";
				node->window = std::move(ws);
			} else if (t->isRegexp()) {
				getToken();
				auto ws = std::make_unique<AstWindowSpec>();
				ws->className = t->getRegexp();
				ws->op = L"&&";
				node->window = std::move(ws);
			}
		}

		if (!isEOL()) {
			t = lookToken();
			if (*t == L":") {
				getToken();
				node->parentName = getToken()->getString();
			}
		}

		if (!isEOL()) {
			Token *t2 = getToken();
			if (!(*t2 == L"=>" || *t2 == L"="))
				throw ErrorMessage() << L"`" << *t2
					<< L"': syntax error.";
			node->defaultKeySeq = parseKeySequence();
		}
	}

	return node;
}


//=============================================================================
// Key assignment
//=============================================================================

AstNodePtr MayuParser::parseKeyAssign()
{
	AstSourceLoc loc = currentLoc();

	// Parse modifier sequence
	std::vector<AstModifierSpec> mods = parseModifierSpecs();

	// `key MODIFIER* = MODIFIER*' used to change the default modifiers of every
	// line below it.  Rejected rather than silently parsed as an assignment to a
	// key named "=", which is what falling through would report.
	if (!isEOL() && *lookToken() == L"=")
		throw ErrorMessage()
			<< L"changing the default modifiers is no longer supported; "
			L"spell the modifiers out on each key.";

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
		   !(*lookToken() == L"=>" || *lookToken() == L"=")) {
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
	if (!(*t == L"=>" || *t == L"="))
		throw ErrorMessage() << L"`=' is expected.";

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
		wstringi assignMode;
		if (*t == L"!")
			assignMode = L"!", t = getToken();
		else if (*t == L"!!")
			assignMode = L"!!", t = getToken();
		else if (*t == L"!!!")
			assignMode = L"!!!", t = getToken();

		wstringi modName = t->getString();

		// Look ahead to see if this is the main modifier
		// (followed by "=", "+=", "-=") or a prefix
		if (assignMode.empty() && !isEOL()) {
			Token *next = lookToken();
			if (*next == L"=" || *next == L"+=" ||
				*next == L"-=") {
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
	if (*t == L"=")
		node->op = L"=";
	else if (*t == L"+=")
		node->op = L"+=";
	else if (*t == L"-=")
		node->op = L"-=";
	else
		throw ErrorMessage() << L"`" << *t
			<< L"': is unknown operator.";

	// RHS: (ASSIGN_MODE? KEY_NAME)+
	while (!isEOL()) {
		t = getToken();
		AstModAssignKey key;
		if (*t == L"!")
			key.assignMode = L"!", t = getToken();
		else if (*t == L"!!")
			key.assignMode = L"!!", t = getToken();
		else if (*t == L"!!!")
			key.assignMode = L"!!!", t = getToken();
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

	if (*getToken() != L"$")
		throw ErrorMessage()
			<< L"there must be `$' after `keyseq'.";

	node->name = getToken()->getString();

	if (*getToken() != L"=")
		throw ErrorMessage()
			<< L"there must be `=' after keyseq name.";

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
		} else if (*t == L"$") {
			getToken(); // consume '$'
			auto action = std::make_unique<AstActionKeySeqRef>();
			action->modifiers = std::move(mods);
			action->name = getToken()->getString();
			seq->actions.push_back(std::move(action));
		} else if (*t == L"&") {
			getToken(); // consume '&'
			auto action = std::make_unique<AstActionFuncCall>();
			action->modifiers = std::move(mods);
			action->functionName = getToken()->getString();
			action->arguments = parseArguments();
			seq->actions.push_back(std::move(action));
		} else if (*t == L"@") {
			// @FuncName[(arg, ...)]  ->  &ExecUserFunc("FuncName"[, arg, ...])
			getToken(); // consume '@'
			auto action = std::make_unique<AstActionFuncCall>();
			action->modifiers = std::move(mods);
			action->functionName = wstringi(L"ExecUserFunc");
			auto nameArg = std::make_unique<AstArgument>();
			nameArg->kind = AstArgument::Kind::String;
			nameArg->stringValue = getToken()->getString();
			action->arguments.push_back(std::move(nameArg));
			// Optional argument list: @FuncName(arg1, arg2, ...)
			auto extraArgs = parseArguments();
			for (auto &a : extraArgs)
				action->arguments.push_back(std::move(a));
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

	// $NAME -> Kind::KeySeqRef
	if (*t == L"$") {
		getToken();
		arg->kind = AstArgument::Kind::KeySeqRef;
		arg->stringValue = getToken()->getString();
		return arg;
	}

	// (KEY_SEQUENCE) -> Kind::KeySeqLiteral
	if (t->isOpenParen()) {
		getToken();
		arg->kind = AstArgument::Kind::KeySeqLiteral;
		arg->keySeq = parseKeySequence(true);
		getToken(); // consume ')'
		return arg;
	}

	// NUMBER -> Kind::Number
	if (t->isNumber()) {
		getToken();
		arg->kind = AstArgument::Kind::Number;
		arg->numberValue = t->getNumber();
		return arg;
	}

	// /REGEXP/ -> Kind::Regexp
	if (t->isRegexp()) {
		getToken();
		arg->kind = AstArgument::Kind::Regexp;
		arg->stringValue = t->getRegexp();
		return arg;
	}

	// General case: greedily collect all tokens until ',' / ')' / EOL.
	// Each comma-separated element is one argument regardless of token count.
	std::vector<wstringi> rawTokens;
	while (!isEOL() && !lookToken()->isComma() && !lookToken()->isCloseParen()) {
		rawTokens.push_back(getToken()->getString());
	}

	bool allModifiers = !rawTokens.empty() &&
		std::all_of(rawTokens.begin(), rawTokens.end(),
			[this](const wstringi &s) { return isModifierTokenName(s); });

	if (rawTokens.size() == 1 && !isModifierTokenName(rawTokens[0])) {
		// Single non-modifier token -> Kind::String (backward compatible)
		arg->kind = AstArgument::Kind::String;
		arg->stringValue = rawTokens[0];
	} else if (allModifiers) {
		// All modifier tokens -> Kind::ModifierSeq (backward compatible)
		arg->kind = AstArgument::Kind::ModifierSeq;
		arg->modifierSeq = tokensToModifierSpecs(rawTokens);
	} else {
		// Multiple tokens or modifier+non-modifier mix -> Kind::TokenSeq
		arg->kind = AstArgument::Kind::TokenSeq;
		arg->tokens = std::move(rawTokens);
	}
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
		throw ErrorMessage() << L"`)' expected in function arguments.";

	return args;
}


//=============================================================================
// Public static helpers
//=============================================================================

// Parse a modifier-key string (e.g. "C-A", "*-LButton", "S-C-Return").
// Uses parseActions() on the string; expects exactly one AstActionKey.
/*static*/
bool MayuParser::parseModifiedKey(const wstringi &str,
	std::vector<AstModifierSpec> &mods, wstringi &keyName)
{
	MayuParser p;
	auto seq = p.parseActions(str.c_str(), str.size(), L"<modkey>");
	if (p.hasErrors() || !seq || seq->actions.size() != 1)
		return false;
	const auto *key = dynamic_cast<const AstActionKey *>(seq->actions[0].get());
	if (!key)
		return false;
	mods    = key->modifiers;
	keyName = key->keyName;
	return true;
}


// Parse a single scan-code string (e.g. "0x1c", "E0-0x1c", "28").
// Wraps the string in a synthetic "def sync = <str>" and extracts the result.
/*static*/
bool MayuParser::parseScanCode(const wstringi &str, AstScanCode &out)
{
	MayuParser p;
	wstringi synth = wstringi(L"def sync = ") + str;
	auto ast = p.parseBuffer(synth.c_str(), synth.size(), L"<scancode>");
	if (p.hasErrors() || !ast || ast->statements.empty())
		return false;
	const auto *sync =
		dynamic_cast<const AstDefSync *>(ast->statements[0].get());
	if (!sync || sync->scanCodes.empty())
		return false;
	out = sync->scanCodes[0];
	return true;
}
