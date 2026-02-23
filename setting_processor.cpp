//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// setting_processor.cpp


#include "misc.h"

#include "setting_processor.h"
#include "cmd_stream.h"
#include "cmd_stream_reader.h"
#include "errormessage.h"
#include "function.h"
#include <sstream>


namespace {

//-----------------------------------------------------------------------------
// Modifier name -> Modifier::Type mapping (for ModAssign)
//-----------------------------------------------------------------------------

struct ModNameEntry {
	const _TCHAR *name;
	Modifier::Type type;
};

const ModNameEntry g_modNameMap[] = {
	{ _T("shift"),   Modifier::Type_Shift },
	{ _T("alt"),     Modifier::Type_Alt },
	{ _T("meta"),    Modifier::Type_Alt },
	{ _T("menu"),    Modifier::Type_Alt },
	{ _T("control"), Modifier::Type_Control },
	{ _T("ctrl"),    Modifier::Type_Control },
	{ _T("windows"), Modifier::Type_Windows },
	{ _T("win"),     Modifier::Type_Windows },
	{ _T("mod0"),    Modifier::Type_Mod0 },
	{ _T("mod1"),    Modifier::Type_Mod1 },
	{ _T("mod2"),    Modifier::Type_Mod2 },
	{ _T("mod3"),    Modifier::Type_Mod3 },
	{ _T("mod4"),    Modifier::Type_Mod4 },
	{ _T("mod5"),    Modifier::Type_Mod5 },
	{ _T("mod6"),    Modifier::Type_Mod6 },
	{ _T("mod7"),    Modifier::Type_Mod7 },
	{ _T("mod8"),    Modifier::Type_Mod8 },
	{ _T("mod9"),    Modifier::Type_Mod9 },
};

bool lookupModifierType(const tstringi &name, Modifier::Type *o_mt)
{
	for (size_t i = 0; i < NUMBER_OF(g_modNameMap); ++i) {
		if (_tcsicmp(name.c_str(), g_modNameMap[i].name) == 0) {
			*o_mt = g_modNameMap[i].type;
			return true;
		}
	}
	return false;
}

Keymap::AssignMode parseAssignMode(const tstringi &s)
{
	if (s == _T("!"))   return Keymap::AM_true;
	if (s == _T("!!"))  return Keymap::AM_oneShot;
	if (s == _T("!!!")) return Keymap::AM_oneShotRepeatable;
	return Keymap::AM_notModifier;
}


//-----------------------------------------------------------------------------
// Helper: CmdModifier -> Modifier
//-----------------------------------------------------------------------------

static Modifier toModifier(const CmdModifier &bm)
{
	Modifier mod;
	for (int i = Modifier::Type_begin; i != Modifier::Type_end; ++i) {
		uint64_t bit = uint64_t(1) << i;
		Modifier::Type mt = static_cast<Modifier::Type>(i);
		if (bm.dontcares & bit) {
			mod.dontcare(mt);
		} else {
			mod.care(mt);
			if (bm.modifiers & bit)
				mod.press(mt);
			else
				mod.release(mt);
		}
	}
	return mod;
}

static ModifiedKey toModifiedKey(const CmdModifiedKey &bmk, SettingBuilder &b)
{
	ModifiedKey mkey;
	mkey.m_modifier = toModifier(bmk.modifier);
	mkey.m_key = b.searchKey(bmk.keyName);
	return mkey;
}


//-----------------------------------------------------------------------------
// Interpreter implementation
//-----------------------------------------------------------------------------

class SettingProcessorImpl : public CmdLoadContext
{
private:
	SettingBuilder &m_builder;
	std::vector<KeySeq *> m_keySeqs;
	Keymap *m_currentKeymap;
	SyncObject *m_soLog;
	tostream *m_log;

public:
	SettingProcessorImpl(SettingBuilder &builder, SyncObject *soLog, tostream *log)
		: m_builder(builder), m_soLog(soLog), m_log(log), m_currentKeymap(nullptr)
	{
	}

	// CmdLoadContext interface
	const Keymap *resolveKeymap(const tstringi &name) override {
		return m_builder.searchKeymapByName(name);
	}
	const KeySeq *resolveKeySeq(uint32_t index) override {
		if (index < m_keySeqs.size()) return m_keySeqs[index];
		return nullptr;
	}
	Modifier resolveModifier(const CmdModifier &bm) override {
		return toModifier(bm);
	}

	void error(const tstring &msg) {
		if (m_log) {
			Acquire a(m_soLog);
			*m_log << _T("loader error: ") << msg << std::endl;
		}
	}

	KeySeq *materializeKeySeq(const CmdKeySequence &cmdKs) {
		KeySeq ks(cmdKs.name);
		if (cmdKs.mode != 0)
			ks.setMode(static_cast<Modifier::Type>(cmdKs.mode));

		for (const auto &action : cmdKs.actions) {
			switch (action.type) {
			case CmdAction::Key: {
				ModifiedKey mkey;
				mkey.m_modifier = toModifier(action.modifier);
				Key *key = m_builder.searchKey(action.name);
				if (key) {
					mkey.m_key = key;
					ks.add(ActionKey(mkey));
				}
				break;
			}
			case CmdAction::KeySeqRef: {
				KeySeq *ref = m_builder.searchKeySeqByName(action.name);
				if (ref) {
					ks.setMode(ref->getMode());
					ks.add(ActionKeySeq(ref));
				}
				break;
			}
			case CmdAction::FuncCall: {
				Modifier mod = toModifier(action.modifier);
				FunctionData *fd = createFunctionData(action.name);
				if (fd) {
					fd->loadFromCmd(action.arguments, this);
					ks.add(ActionFunction(fd, mod));
				}
				break;
			}
			case CmdAction::SubSeq: {
				KeySeq subKs(_T(""));
				for (const auto &sub : action.subActions) {
					if (sub.type == CmdAction::Key) {
						ModifiedKey mkey;
						mkey.m_modifier = toModifier(sub.modifier);
						Key *key = m_builder.searchKey(sub.name);
						if (key) {
							mkey.m_key = key;
							subKs.add(ActionKey(mkey));
						}
					} else if (sub.type == CmdAction::FuncCall) {
						Modifier mod = toModifier(sub.modifier);
						FunctionData *fd = createFunctionData(sub.name);
						if (fd) {
							fd->loadFromCmd(sub.arguments, this);
							subKs.add(ActionFunction(fd, mod));
						}
					}
				}
				KeySeq *addedSub = m_builder.addKeySeq(subKs);
				ks.add(ActionKeySeq(addedSub));
				break;
			}
			}
		}
		return m_builder.addKeySeq(ks);
	}

	void load(std::istream &in) {
		// Initialize global defaults - m_currentKeymap starts as Global
		// so that top-level key/mod assignments (before any explicit keymap
		// block) are attached to it, matching the old SettingLoader behavior.
		ActionFunction af(createFunctionData(_T("OtherWindowClass")));
		KeySeq *globalDefault = m_builder.addKeySeq(KeySeq(_T("")).add(af));
		m_currentKeymap = m_builder.addKeymap(
			Keymap(Keymap::Type_windowOr, _T("Global"), _T(""), _T(""),
				   globalDefault, nullptr));

		CmdStreamReader reader(in);
		CmdId cmdId;

		while (reader.readNext(cmdId)) {
			try {
				switch (cmdId) {
				case CmdId::DefKeySeq: {
					CmdKeySequence bks = reader.readDefKeySeq();
					m_keySeqs.push_back(materializeKeySeq(bks));
					break;
				}
				case CmdId::DefKey: {
					auto data = reader.readDefKey();
					Key key;
					for (const auto &name : data.names) key.addName(name);
					for (const auto &sc : data.scanCodes) {
						ScanCode scanCode;
						scanCode.m_scan = static_cast<u_char>(sc.scan);
						scanCode.m_flags = sc.flags;
						key.addScanCode(scanCode);
					}
					m_builder.addKey(key);
					break;
				}
				case CmdId::DefModifier: {
					auto data = reader.readDefModifier();
					Modifier::Type mt;
					if (!lookupModifierType(data.modifierName, &mt))
						throw ErrorMessage() << _T("invalid modifier: ") << data.modifierName;
					for (const auto &keyName : data.keyNames) {
						Key *key = m_builder.searchKeyByNonAliasName(keyName);
						if (!key) throw ErrorMessage() << _T("invalid key: ") << keyName;
						m_builder.addModifier(mt, key);
					}
					break;
				}
				case CmdId::DefSync: {
					auto data = reader.readDefSync();
					Key *key = m_builder.getSyncKey();
					key->initialize();
					key->addName(_T("sync"));
					for (const auto &sc : data.scanCodes) {
						ScanCode scanCode;
						scanCode.m_scan = static_cast<u_char>(sc.scan);
						scanCode.m_flags = sc.flags;
						key->addScanCode(scanCode);
					}
					break;
				}
				case CmdId::DefAlias: {
					auto data = reader.readDefAlias();
					Key *key = m_builder.searchKeyByNonAliasName(data.keyName);
					if (!key) throw ErrorMessage() << _T("invalid key: ") << data.keyName;
					m_builder.addAlias(data.aliasName, key);
					break;
				}
				case CmdId::DefSubstitute: {
					auto data = reader.readDefSubstitute();
					KeySeq *keySeq = m_keySeqs[data.rhsKeySeqIdx];
					ModifiedKey rhs = keySeq->getFirstModifiedKey();
					if (!rhs.m_key) throw ErrorMessage() << _T("no key for substitute");
					for (const auto &bmk : data.lhsKeys) {
						ModifiedKey lhs = toModifiedKey(bmk, m_builder);
						if (!lhs.m_key) throw ErrorMessage() << _T("invalid lhs key");
						m_builder.addSubstitute(lhs, rhs);
					}
					break;
				}
				case CmdId::DefOption: {
					auto data = reader.readDefOption();
					const tstringi &name = data.optionName;
					const tstringi &value = data.value;
					if (name == _T("KL-")) *m_builder.correctKanaLockHandling() = !(value == _T("false"));
					else if (name == _T("delay-of")) *m_builder.oneShotRepeatableDelay() = static_cast<unsigned int>(_ttoi(value.c_str()));
					else if (name == _T("sts4mayu")) *m_builder.sts4mayu() = !(value == _T("false"));
					else if (name == _T("cts4mayu")) *m_builder.cts4mayu() = !(value == _T("false"));
					else if (name == _T("mouse-event")) *m_builder.mouseEvent() = !(value == _T("false"));
					else if (name == _T("drag-threshold")) *m_builder.dragThreshold() = static_cast<LONG>(_ttoi(value.c_str()));
					break;
				}
				case CmdId::DefSymbol: {
					auto data = reader.readDefSymbol();
					m_builder.addSymbol(data.symbolName);
					break;
				}
				case CmdId::KeymapDef: {
					auto data = reader.readKeymapDef();
					Keymap::Type type = Keymap::Type_keymap;
					if (!data.windowOp.empty()) {
						if (data.windowOp == _T("&&")) type = Keymap::Type_windowAnd;
						else if (data.windowOp == _T("||")) type = Keymap::Type_windowOr;
					} else if (data.keyword == _T("window") && !data.windowClassName.empty()) {
						type = Keymap::Type_windowAnd;
					}
					m_currentKeymap = m_builder.addKeymap(Keymap(type, data.name, data.windowClassName, data.windowTitleName, nullptr, nullptr));

					Keymap *parent = nullptr;
					if (!data.parentName.empty())
						parent = m_builder.addKeymap(
							Keymap(Keymap::Type_keymap, data.parentName,
								   _T(""), _T(""), nullptr, nullptr));

					KeySeq *keySeq = nullptr;
					if (data.defaultKeySeqIdx >= 0) keySeq = m_keySeqs[data.defaultKeySeqIdx];
					if (!keySeq) {
						FunctionData *fd = createFunctionData(_T("KeymapParent"));
						keySeq = m_builder.addKeySeq(KeySeq(data.name).add(ActionFunction(fd)));
					}
					m_currentKeymap->setIfNotYet(keySeq, parent);
					break;
				}
				case CmdId::KeyAssign: {
					auto data = reader.readKeyAssign();
					KeySeq *keySeq = m_keySeqs[data.rhsKeySeqIdx];
					for (const auto &bmk : data.lhsKeys) {
						ModifiedKey mkey = toModifiedKey(bmk, m_builder);
						if (mkey.m_key && m_currentKeymap) m_currentKeymap->addAssignment(mkey, keySeq);
					}
					break;
				}
				case CmdId::KeyDefaultMod: {
					reader.readKeyDefaultMod(); // no-op
					break;
				}
				case CmdId::EventAssign: {
					auto data = reader.readEventAssign();
					ModifiedKey mkey;
					mkey.m_modifier.dontcare();
					Key **e;
					for (e = Event::events; *e; ++e) {
						if (data.eventName == (*e)->getName()) {
							mkey.m_key = *e;
							break;
						}
					}
					KeySeq *keySeq = m_keySeqs[data.rhsKeySeqIdx];
					if (mkey.m_key && m_currentKeymap) m_currentKeymap->addAssignment(mkey, keySeq);
					break;
				}
				case CmdId::ModAssign: {
					auto data = reader.readModAssign();
					for (const auto &p : data.prefixes) {
						Modifier::Type mt;
						if (lookupModifierType(p.modifierName, &mt) && m_currentKeymap)
							m_currentKeymap->addModifier(mt, Keymap::AO_overwrite, parseAssignMode(p.assignMode), nullptr);
					}
					Modifier::Type mt;
					lookupModifierType(data.mainModifierName, &mt);
					Keymap::AssignOperator ao;
					if (data.op == _T("+=")) ao = Keymap::AO_add;
					else if (data.op == _T("-=")) ao = Keymap::AO_sub;
					else ao = Keymap::AO_new;

					for (const auto &ke : data.keys) {
						Key *key = m_builder.searchKey(ke.keyName);
						if (key && m_currentKeymap) m_currentKeymap->addModifier(mt, ao, parseAssignMode(ke.assignMode), key);
						if (ao == Keymap::AO_new) ao = Keymap::AO_add;
					}
					break;
				}
				case CmdId::KeySeqDef: {
					reader.readKeySeqDef();
					break;
				}
				case CmdId::Reset:
				case CmdId::Commit:
					return;
				}
			} catch (ErrorMessage &e) {
				error(e.getMessage());
			}
		}
	}
};

} // namespace


SettingProcessor::SettingProcessor(SyncObject *i_soLog, tostream *i_log)
	: m_soLog(i_soLog), m_log(i_log)
{
}


std::unique_ptr<Setting> SettingProcessor::process(std::istream &in,
													const Symbols &i_initialSymbols)
{
	SettingBuilder builder;

	for (const auto &sym : i_initialSymbols)
		builder.addSymbol(sym);

	SettingProcessorImpl impl(builder, m_soLog, m_log);
	impl.load(in);

	return builder.build();
}
