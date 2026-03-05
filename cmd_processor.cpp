//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// cmd_processor.cpp


#include "misc.h"

#include "cmd_processor.h"
#include "errormessage.h"
#include "function.h"


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
	: m_soLog(soLog), m_log(log) {}


void CmdProcessor::onCommit(CommitCallback cb) { m_commitCallback = std::move(cb); }


void CmdProcessor::initBuilder()
{
	m_builder = std::make_unique<SettingBuilder>();
	ActionFunction af(createFunctionData(L"OtherWindowClass"));
	KeySeq *globalDefault = m_builder->addKeySeq(KeySeq(L"").add(af));
	m_builder->setCurrentKeymap(m_builder->addKeymap(
		Keymap(Keymap::Type_windowOr, L"Global", L"", L"",
			   globalDefault, nullptr)));
}


void CmdProcessor::error(const std::wstring &msg)
{
	if (m_log) {
		Acquire a(m_soLog);
		*m_log << L"loader error: " << msg << std::endl;
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
	initBuilder();
	while (auto cmd = cr.readCmd()) {
		try { std::visit(*this, *cmd); }
		catch (ErrorMessage &e) { error(e.getMessage()); }
	}
}


void CmdProcessor::operator()(CmdKeySequence &bks)
{
	m_builder->pushKeySeqRef(m_builder->materializeKeySeq(bks));
}


void CmdProcessor::operator()(CmdDefKeyData &data)
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


void CmdProcessor::operator()(CmdDefModifierData &data)
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


void CmdProcessor::operator()(CmdDefSyncData &data)
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


void CmdProcessor::operator()(CmdDefAliasData &data)
{
	Key *key = m_builder->searchKeyByNonAliasName(data.keyName);
	if (!key) throw ErrorMessage() << L"invalid key: " << data.keyName;
	m_builder->addAlias(data.aliasName, key);
}


void CmdProcessor::operator()(CmdDefSubstituteData &data)
{
	KeySeq *keySeq = m_builder->getKeySeqRef(data.rhsKeySeqIdx);
	ModifiedKey rhs = keySeq->getFirstModifiedKey();
	if (!rhs.m_key) throw ErrorMessage() << L"no key for substitute";
	for (const auto &bmk : data.lhsKeys) {
		ModifiedKey lhs = m_builder->toModifiedKey(bmk);
		if (!lhs.m_key) throw ErrorMessage() << L"invalid lhs key";
		m_builder->addSubstitute(lhs, rhs);
	}
}


void CmdProcessor::operator()(CmdDefOptionData &data)
{
	const wstringi &name = data.optionName;
	const wstringi &value = data.value;
	if (name == L"KL-") *m_builder->correctKanaLockHandling() = !(value == L"false");
	else if (name == L"delay-of") *m_builder->oneShotRepeatableDelay() = static_cast<unsigned int>(_wtoi(value.c_str()));
	else if (name == L"sts4mayu") *m_builder->sts4mayu() = !(value == L"false");
	else if (name == L"cts4mayu") *m_builder->cts4mayu() = !(value == L"false");
	else if (name == L"mouse-event") *m_builder->mouseEvent() = !(value == L"false");
	else if (name == L"drag-threshold") *m_builder->dragThreshold() = static_cast<LONG>(_wtoi(value.c_str()));
}


void CmdProcessor::operator()(CmdDefSymbolData &data)
{
	m_builder->addSymbol(data.symbolName);
}


void CmdProcessor::operator()(CmdKeymapDefData &data)
{
	Keymap::Type type = Keymap::Type_keymap;
	if (!data.windowOp.empty()) {
		if (data.windowOp == L"&&") type = Keymap::Type_windowAnd;
		else if (data.windowOp == L"||") type = Keymap::Type_windowOr;
	} else if (data.keyword == L"window" && !data.windowClassName.empty()) {
		type = Keymap::Type_windowAnd;
	}
	m_builder->setCurrentKeymap(m_builder->addKeymap(Keymap(type, data.name, data.windowClassName, data.windowTitleName, nullptr, nullptr)));

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
	m_builder->currentKeymap()->setIfNotYet(keySeq, parent);
}


void CmdProcessor::operator()(CmdKeyAssignData &data)
{
	KeySeq *keySeq = m_builder->getKeySeqRef(data.rhsKeySeqIdx);
	for (const auto &bmk : data.lhsKeys) {
		ModifiedKey mkey = m_builder->toModifiedKey(bmk);
		if (mkey.m_key && m_builder->currentKeymap()) m_builder->currentKeymap()->addAssignment(mkey, keySeq);
	}
}


void CmdProcessor::operator()(CmdKeyDefaultModData &)
{
	// no-op
}


void CmdProcessor::operator()(CmdEventAssignData &data)
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


void CmdProcessor::operator()(CmdModAssignData &data)
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


void CmdProcessor::operator()(CmdKeySeqDefData &)
{
	// no-op
}


void CmdProcessor::operator()(CmdCommit)
{
	if (m_commitCallback && m_builder)
		m_commitCallback(m_builder->build());
	initBuilder();
}
