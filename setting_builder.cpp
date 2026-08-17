//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// setting_builder.cpp


#include "misc.h"

#include "setting_builder.h"
#include "adhoc_keyseq.h"
#include "function.h"   // createFunctionData


void SettingBuilder::fillKeySeq(KeySeq *o_target, const CmdArgsRegKeySeq &cmdKs,
                                std::vector<std::wstring> *o_warnings)
{
	auto warn = [&](const wchar_t *what, const wstringi &name) {
		if (o_warnings)
			o_warnings->push_back(std::wstring(what) + L": " + name.c_str()
			                      + L" (dropped from keyseq "
			                      + cmdKs.name.c_str() + L")");
	};

	/* Build a function's data, or return null having warned.

	   loadFromCmd() resolves the $NAME arguments, and an unknown name throws.
	   That is one more way an action can fail to resolve, so it is reported the
	   same way as an undefined key or keyseq: warn, drop this action, keep the
	   rest of the sequence.  Letting it out of here instead would abandon the
	   Commit half way and cost the whole setting.
	*/
	auto loadFunc = [&](const CmdAction &i_action) -> FunctionData * {
		FunctionData *fd = createFunctionData(i_action.name);
		if (!fd) {
			warn(L"unknown function", i_action.name);
			return NULL;
		}
		try {
			fd->loadFromCmd(i_action.arguments, this);
		} catch (ErrorMessage &e) {
			delete fd;
			if (o_warnings)
				o_warnings->push_back(e.getMessage() + L" (in &"
				                      + i_action.name.c_str()
				                      + L", dropped from keyseq "
				                      + cmdKs.name.c_str() + L")");
			return NULL;
		}
		return fd;
	};

	KeySeq ks(cmdKs.name);
	if (cmdKs.mode != 0)
		ks.setMode(static_cast<Modifier::Type>(cmdKs.mode));

	for (const auto &action : cmdKs.actions) {
		switch (action.type) {
		case CmdAction::Key: {
			ModifiedKey mkey;
			mkey.m_modifier = modifierFromCmd(action.modifier);
			Key *key = searchKey(action.name);
			if (key) {
				mkey.m_key = key;
				ks.add(ActionKey(mkey));
			} else {
				warn(L"undefined key", action.name);
			}
			break;
		}
		case CmdAction::KeySeqRef: {
			KeySeq *ref = searchKeySeqByName(action.name);
			if (ref) {
				ks.setMode(ref->getMode());
				ks.add(ActionKeySeq(ref));
			} else {
				warn(L"undefined keyseq", action.name);
			}
			break;
		}
		case CmdAction::FuncCall: {
			Modifier mod = modifierFromCmd(action.modifier);
			if (FunctionData *fd = loadFunc(action))
				ks.add(ActionFunction(fd, mod));
			break;
		}
		case CmdAction::SubSeq: {
			KeySeq subKs(L"");
			for (const auto &sub : action.subActions) {
				if (sub.type == CmdAction::Key) {
					ModifiedKey mkey;
					mkey.m_modifier = modifierFromCmd(sub.modifier);
					Key *key = searchKey(sub.name);
					if (key) {
						mkey.m_key = key;
						subKs.add(ActionKey(mkey));
					} else {
						warn(L"undefined key", sub.name);
					}
				} else if (sub.type == CmdAction::FuncCall) {
					Modifier mod = modifierFromCmd(sub.modifier);
					if (FunctionData *fd = loadFunc(sub))
						subKs.add(ActionFunction(fd, mod));
				}
			}
			KeySeq *addedSub = addKeySeq(subKs);
			ks.add(ActionKeySeq(addedSub));
			break;
		}
		}
	}
	// Assign into the shell registered when the RegKeySeq command arrived,
	// so pointers already handed out to keymaps / assignments stay valid.
	*o_target = ks;
}


//=============================================================================
// AdHocMaterializer
//=============================================================================

const Keymap *AdHocMaterializer::resolveKeymap(const wstringi &name)
{
	return m_setting.m_keymaps.searchByName(name);
}


AdHocKeySeq AdHocMaterializer::materialize(const std::vector<CmdAction> &actions,
                                            const TriggerInfo &ctx)
{
	auto item = std::make_unique<AdHocItem>();
	item->context = ctx;
	auto ks = std::make_unique<KeySeq>(L"");

	// As in SettingBuilder::fillKeySeq, an action that will not resolve is
	// dropped rather than allowed to abandon the sequence.  There is nowhere to
	// warn to here - materialize() has no log - which matches the silent skips
	// the other cases already do.
	auto loadFunc = [&](const CmdAction &i_action) -> FunctionData * {
		FunctionData *fd = createFunctionData(i_action.name);
		if (!fd)
			return NULL;
		try {
			fd->loadFromCmd(i_action.arguments, this);
		} catch (ErrorMessage &) {
			delete fd;
			return NULL;
		}
		return fd;
	};

	for (const auto &action : actions) {
		switch (action.type) {
		case CmdAction::Key: {
			ModifiedKey mkey;
			mkey.m_modifier = resolveModifier(action.modifier);
			Key *key = m_setting.m_keyboard.searchKey(action.name);
			if (key) {
				mkey.m_key = key;
				ks->add(ActionKey(mkey));
			}
			break;
		}
		case CmdAction::KeySeqRef:
			// KeySeqRef is not supported in ExecKeySeq: skip
			break;
		case CmdAction::FuncCall: {
			Modifier mod = resolveModifier(action.modifier);
			if (FunctionData *fd = loadFunc(action))
				ks->add(ActionFunction(fd, mod));
			break;
		}
		case CmdAction::SubSeq: {
			// sub-sequence: create a new KeySeq and store in item->subKeySeqs
			auto subKs = std::make_unique<KeySeq>(L"");
			for (const auto &sub : action.subActions) {
				if (sub.type == CmdAction::Key) {
					ModifiedKey mkey;
					mkey.m_modifier = resolveModifier(sub.modifier);
					Key *key = m_setting.m_keyboard.searchKey(sub.name);
					if (key) {
						mkey.m_key = key;
						subKs->add(ActionKey(mkey));
					}
				} else if (sub.type == CmdAction::FuncCall) {
					Modifier mod = resolveModifier(sub.modifier);
					if (FunctionData *fd = loadFunc(sub))
						subKs->add(ActionFunction(fd, mod));
				}
			}
			KeySeq *subPtr = subKs.get();
			item->subKeySeqs.push_back(std::move(subKs));
			ks->add(ActionKeySeq(subPtr));
			break;
		}
		}
	}

	item->keySeq = std::move(ks);
	return item;
}
