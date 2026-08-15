//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// cmd_processor.cpp


#include "misc.h"

#include "cmd_processor.h"
#include "errormessage.h"
#include "function.h"

#include <cstdlib>


namespace {

struct ModNameEntry {
	const wchar_t *name;
	Modifier::Type type;
};

const ModNameEntry g_modNameMap[] = {
	{ L"shift",   Modifier::Type_Shift },
	{ L"alt",     Modifier::Type_Alt },
	{ L"meta",    Modifier::Type_Alt },
	{ L"menu",    Modifier::Type_Alt },
	{ L"control", Modifier::Type_Control },
	{ L"ctrl",    Modifier::Type_Control },
	{ L"windows", Modifier::Type_Windows },
	{ L"win",     Modifier::Type_Windows },
	{ L"mod0",    Modifier::Type_Mod0 },
	{ L"mod1",    Modifier::Type_Mod1 },
	{ L"mod2",    Modifier::Type_Mod2 },
	{ L"mod3",    Modifier::Type_Mod3 },
	{ L"mod4",    Modifier::Type_Mod4 },
	{ L"mod5",    Modifier::Type_Mod5 },
	{ L"mod6",    Modifier::Type_Mod6 },
	{ L"mod7",    Modifier::Type_Mod7 },
	{ L"mod8",    Modifier::Type_Mod8 },
	{ L"mod9",    Modifier::Type_Mod9 },
};

} // namespace


CmdProcessor::CmdProcessor(SyncObject *soLog, std::wostream *log)
	: m_soLog(soLog), m_log(log)
{}


void CmdProcessor::onCommit(CommitCallback cb) { m_commitCallback = std::move(cb); }
void CmdProcessor::onExecKeySeq(ExecKeySeqCallback cb) { m_execKeySeqCallback = std::move(cb); }


void CmdProcessor::error(const std::wstring &msg)
{
	if (m_log) {
		Acquire a(m_soLog);
		*m_log << L"loader error: " << msg << std::endl;
	}
}


void CmdProcessor::warning(const std::wstring &msg)
{
	if (m_log) {
		Acquire a(m_soLog);
		*m_log << L"loader warning: " << msg << std::endl;
	}
}


bool CmdProcessor::lookupModifierType(const wstringi &name, Modifier::Type *o_mt)
{
	for (size_t i = 0; i < NUMBER_OF(g_modNameMap); ++i) {
		if (_wcsicmp(name.c_str(), g_modNameMap[i].name) == 0) {
			*o_mt = g_modNameMap[i].type;
			return true;
		}
	}
	return false;
}


Keymap::AssignMode CmdProcessor::parseAssignMode(const wstringi &s)
{
	if (s == L"!")   return Keymap::AM_true;
	if (s == L"!!")  return Keymap::AM_oneShot;
	if (s == L"!!!") return Keymap::AM_oneShotRepeatable;
	return Keymap::AM_notModifier;
}


void CmdProcessor::process(CmdStreamReader &cr)
{
	while (auto cmd = cr.readCmd()) {
		// Reset opens a setting definition block and ExecKeySeq operates on
		// the last committed Setting; every other command requires an open
		// block (m_builder alive).
		if (!m_builder
			&& !std::holds_alternative<CmdArgsReset>(*cmd)
			&& !std::holds_alternative<CmdArgsExecKeySeq>(*cmd)) {
			error(L"command received outside a Reset..Commit block");
			continue;
		}
		try { std::visit(*this, *cmd); }
		catch (ErrorMessage &e) { error(e.getMessage()); }
	}
}


// Start building a fresh Setting, discarding any partially built one.
// The last committed Setting (m_committed) is untouched: the engine may
// still be using it.
void CmdProcessor::beginSetting()
{
	m_pendingKeySeqs.clear();
	m_pendingSubsts.clear();
	m_keymapStack.clear();
	m_setting = std::make_shared<Setting>();
	m_builder = std::make_unique<SettingBuilder>(*m_setting);

	ActionFunction af(createFunctionData(L"OtherWindowClass"));
	KeySeq *globalDefault = m_builder->addKeySeq(KeySeq(L"").add(af));
	m_builder->setCurrentKeymap(m_builder->addKeymap(
		Keymap(Keymap::Type_windowOr, L"Global", L"", L"",
		       globalDefault, nullptr)));
}


void CmdProcessor::operator()(CmdArgsReset)
{
	beginSetting();
}


void CmdProcessor::operator()(CmdArgsRegKeySeq &bks)
{
	// Register an empty shell so later commands can bind to a stable pointer
	// by index or name.  The contents are filled at Commit, when every key,
	// keymap and keyseq referenced by the actions is guaranteed to exist.
	KeySeq *shell = m_builder->addKeySeq(KeySeq(bks.name));
	m_builder->pushKeySeqRef(shell);
	m_pendingKeySeqs.emplace_back(shell, std::move(bks));
}


void CmdProcessor::operator()(CmdArgsExecKeySeq &data)
{
	if (!m_committed) return;	// nothing committed yet
	AdHocMaterializer materializer(*m_committed);
	auto item = materializer.materialize(data.actions, data.context);
	if (!item || !item->keySeq) return;
	item->origin = m_committed;
	if (m_execKeySeqCallback) m_execKeySeqCallback(std::move(item));
}


void CmdProcessor::operator()(CmdArgsDefKey &data)
{
	Key key;
	for (const auto &name : data.names) key.addName(name);
	for (const auto &sc : data.scanCodes) {
		ScanCode scanCode;
		scanCode.m_scan = static_cast<u_char>(sc.scan);
		scanCode.m_flags = sc.flags;
		key.addScanCode(scanCode);
	}
	m_builder->addKey(key);
}


void CmdProcessor::operator()(CmdArgsDefMod &data)
{
	Modifier::Type mt;
	if (!lookupModifierType(data.modifierName, &mt))
		throw ErrorMessage() << L"invalid modifier: " << data.modifierName;
	for (const auto &keyName : data.keyNames) {
		Key *key = m_builder->searchKeyByNonAliasName(keyName);
		if (!key) throw ErrorMessage() << L"invalid key: " << keyName;
		m_builder->addModifier(mt, key);
	}
}


void CmdProcessor::operator()(CmdArgsDefSync &data)
{
	Key *key = m_builder->getSyncKey();
	key->initialize();
	key->addName(L"sync");
	for (const auto &sc : data.scanCodes) {
		ScanCode scanCode;
		scanCode.m_scan = static_cast<u_char>(sc.scan);
		scanCode.m_flags = sc.flags;
		key->addScanCode(scanCode);
	}
}


void CmdProcessor::operator()(CmdArgsDefAlias &data)
{
	Key *key = m_builder->searchKeyByNonAliasName(data.keyName);
	if (!key) throw ErrorMessage() << L"invalid key: " << data.keyName;
	m_builder->addAlias(data.aliasName, key);
}


void CmdProcessor::operator()(CmdArgsDefSubst &data)
{
	// Deferred until Commit: the rhs keyseq is an empty shell until then.
	m_pendingSubsts.push_back(std::move(data));
}


void CmdProcessor::applyDefSubst(const CmdArgsDefSubst &data)
{
	KeySeq *keySeq = m_builder->getKeySeqRef(data.rhsKeySeqIdx);
	if (!keySeq) throw ErrorMessage() << L"invalid keyseq for substitute";
	ModifiedKey rhs = keySeq->getFirstModifiedKey();
	if (!rhs.m_key) throw ErrorMessage() << L"no key for substitute";
	for (const auto &bmk : data.lhsKeys) {
		ModifiedKey lhs = m_builder->toModifiedKey(bmk);
		if (!lhs.m_key) throw ErrorMessage() << L"invalid lhs key";
		m_builder->addSubstitute(lhs, rhs);
	}
}


void CmdProcessor::operator()(CmdArgsDefOption &data)
{
	const wstringi &name = data.optionName;
	const wstringi &value = data.value;
	if (name == L"KL-") *m_builder->correctKanaLockHandling() = !(value == L"false");
	else if (name == L"delay-of !!!") *m_builder->oneShotRepeatableDelay() = static_cast<unsigned int>(_wtoi(value.c_str()));
	else if (name == L"mouse-event") *m_builder->mouseEvent() = !(value == L"false");
	else if (name == L"drag-threshold") *m_builder->dragThreshold() = static_cast<LONG>(_wtoi(value.c_str()));
	else if (name == L"nls-keys") parseNlsKeys(value);
}


// One item of "def option nls-keys": either a scan code - decimal, or hex with
// a 0x prefix, optionally preceded by "E0-" / "E1-" - or the name of a key
// defined earlier by defkey.  A name is only tried when the item is not a
// number in its entirety, so key names that begin with "E0" stay unambiguous.
// See Setting::m_nlsKeys for the encoding.
bool CmdProcessor::nlsScanCode(const std::wstring &item, USHORT *o_code)
{
	const wchar_t *p = item.c_str();
	USHORT prefix = 0;
	if ((p[0] == L'E' || p[0] == L'e') && p[1] && p[2] == L'-') {
		if (p[1] == L'0') prefix = 0xe000;
		else if (p[1] == L'1') prefix = 0xe100;
		if (prefix) p += 3;
	}

	wchar_t *end = NULL;
	unsigned long scan = wcstoul(p, &end, 0);
	if (*p && end && *end == L'\0') {
		if (0xff < scan) {
			error(L"scan code out of range in `def option nls-keys': " + item);
			return false;
		}
		*o_code = static_cast<USHORT>(prefix | scan);
		return true;
	}

	Key *key = m_builder->searchKey(wstringi(item.c_str()));
	if (!key) {
		error(L"unknown scan code or key name in `def option nls-keys': "
			  + item);
		return false;
	}
	// a key spanning several scan codes never matches a single input event,
	// so a synthesized break could not reach it anyway
	if (key->getScanCodesSize() != 1) {
		error(L"key with multiple scan codes in `def option nls-keys': "
			  + item);
		return false;
	}

	const ScanCode &sc = key->getScanCodes()[0];
	USHORT code = static_cast<USHORT>(sc.m_scan & 0xff);
	if (sc.m_flags & ScanCode::E0) code |= 0xe000;
	else if (sc.m_flags & ScanCode::E1) code |= 0xe100;
	*o_code = code;
	return true;
}


// "0x3a, E0-0x29, 112, 英数" -> a set of scan codes.  Separators are commas
// and whitespace.  Key names have to be defined before this option is read.
void CmdProcessor::parseNlsKeys(const wstringi &value)
{
	std::set<USHORT> *keys = m_builder->nlsKeys();
	keys->clear();

	const wchar_t *p = value.c_str();
	while (*p) {
		if (*p == L',' || *p == L' ' || *p == L'\t') {
			++ p;
			continue;
		}

		const wchar_t *begin = p;
		while (*p && *p != L',' && *p != L' ' && *p != L'\t')
			++ p;

		USHORT code = 0;
		if (!nlsScanCode(std::wstring(begin, p), &code)) {
			keys->clear();
			return;
		}
		keys->insert(code);
	}
}


void CmdProcessor::operator()(CmdArgsDefSymbol &data)
{
	m_builder->addSymbol(data.symbolName);
}


void CmdProcessor::operator()(CmdArgsDefKeymap &data)
{
	Keymap::Type type = Keymap::Type_keymap;
	if (!data.windowOp.empty()) {
		if (data.windowOp == L"&&") type = Keymap::Type_windowAnd;
		else if (data.windowOp == L"||") type = Keymap::Type_windowOr;
	} else if (data.keyword == L"window" && !data.windowClassName.empty()) {
		type = Keymap::Type_windowAnd;
	}
	Keymap *keymap = m_builder->addKeymap(Keymap(type, data.name, data.windowClassName, data.windowTitleName, nullptr, nullptr));

	Keymap *parent = nullptr;
	if (!data.parentName.empty())
		parent = m_builder->addKeymap(
			Keymap(Keymap::Type_keymap, data.parentName,
				   L"", L"", nullptr, nullptr));

	KeySeq *keySeq = nullptr;
	if (data.defaultKeySeqIdx >= 0) keySeq = m_builder->getKeySeqRef(data.defaultKeySeqIdx);
	if (!keySeq) {
		FunctionData *fd = createFunctionData(L"KeymapParent");
		keySeq = m_builder->addKeySeq(KeySeq(data.name).add(ActionFunction(fd)));
	}
	keymap->setIfNotYet(keySeq, parent);

	// Declaring the keymap is one thing; deciding where the assignments that
	// follow it land is another.  Block saves the keymap it displaces so that
	// the matching EndKeymap can put it back, which is how a `keymap`/`window`
	// block keeps what is written after it out of its own keymap.
	switch (data.scope) {
	case CmdKeymapScope::Declare:
		break;
	case CmdKeymapScope::Block:
		m_keymapStack.push_back(m_builder->currentKeymap());
		[[fallthrough]];
	case CmdKeymapScope::Enter:
		m_builder->setCurrentKeymap(keymap);
		break;
	}
}


void CmdProcessor::operator()(CmdArgsEndKeymap)
{
	// An unmatched End means the producer is broken.  Say so and carry on with
	// the current keymap: the rest of the setting is still worth having.
	if (m_keymapStack.empty()) {
		error(L"EndKeymap without a matching DefKeymap(Block)");
		return;
	}
	m_builder->setCurrentKeymap(m_keymapStack.back());
	m_keymapStack.pop_back();
}


void CmdProcessor::operator()(CmdArgsAssignKey &data)
{
	KeySeq *keySeq = m_builder->getKeySeqRef(data.rhsKeySeqIdx);
	for (const auto &bmk : data.lhsKeys) {
		ModifiedKey mkey = m_builder->toModifiedKey(bmk);
		if (mkey.m_key && m_builder->currentKeymap()) m_builder->currentKeymap()->addAssignment(mkey, keySeq);
	}
}


void CmdProcessor::operator()(CmdArgsAssignEvent &data)
{
	ModifiedKey mkey;
	mkey.m_modifier.dontcare();
	Key **e;
	for (e = Event::events; *e; ++e) {
		if (data.eventName == (*e)->getName()) {
			mkey.m_key = *e;
			break;
		}
	}
	KeySeq *keySeq = m_builder->getKeySeqRef(data.rhsKeySeqIdx);
	if (mkey.m_key && m_builder->currentKeymap()) m_builder->currentKeymap()->addAssignment(mkey, keySeq);
}


void CmdProcessor::operator()(CmdArgsAssignMod &data)
{
	for (const auto &p : data.prefixes) {
		Modifier::Type mt;
		if (lookupModifierType(p.modifierName, &mt) && m_builder->currentKeymap())
			m_builder->currentKeymap()->addModifier(mt, Keymap::AO_overwrite, parseAssignMode(p.assignMode), nullptr);
	}
	Modifier::Type mt;
	lookupModifierType(data.mainModifierName, &mt);
	Keymap::AssignOperator ao;
	if (data.op == L"+=") ao = Keymap::AO_add;
	else if (data.op == L"-=") ao = Keymap::AO_sub;
	else ao = Keymap::AO_new;

	for (const auto &ke : data.keys) {
		Key *key = m_builder->searchKey(ke.keyName);
		if (key && m_builder->currentKeymap()) m_builder->currentKeymap()->addModifier(mt, ao, parseAssignMode(ke.assignMode), key);
		if (ao == Keymap::AO_new) ao = Keymap::AO_add;
	}
}


void CmdProcessor::operator()(CmdArgsCommit)
{
	// Every key, keymap and keyseq shell now exists: materialize the deferred
	// keyseq contents, then the substitutes that read them.
	for (const auto &p : m_pendingKeySeqs) {
		std::vector<std::wstring> warnings;
		m_builder->fillKeySeq(p.first, p.second, &warnings);
		for (const auto &w : warnings)
			warning(w);
	}
	m_pendingKeySeqs.clear();

	for (const auto &d : m_pendingSubsts) {
		try { applyDefSubst(d); }
		catch (ErrorMessage &e) { error(e.getMessage()); }
	}
	m_pendingSubsts.clear();

	// Every keymap starts with only its explicit "mod" statements; fold in the
	// keyboard-wide defmod keys and the parent keymap's assignments.  Without
	// this no key is recognized as a modifier, so the engine reads Shift and
	// Control as released and injects a spurious release before each key.
	m_builder->adjustModifiers();

	m_builder.reset();
	m_committed = std::move(m_setting);
	if (m_commitCallback) m_commitCallback(m_committed);
}
