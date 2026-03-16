//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// setting_builder.cpp


#include "misc.h"

#include "setting_builder.h"
#include "adhoc_keyseq.h"
#include "function.h"   // createFunctionData


KeySeq *SettingBuilder::materializeKeySeq(const CmdArgsRegKeySeq &cmdKs)
{
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
			}
			break;
		}
		case CmdAction::KeySeqRef: {
			KeySeq *ref = searchKeySeqByName(action.name);
			if (ref) {
				ks.setMode(ref->getMode());
				ks.add(ActionKeySeq(ref));
			}
			break;
		}
		case CmdAction::FuncCall: {
			Modifier mod = modifierFromCmd(action.modifier);
			FunctionData *fd = createFunctionData(action.name);
			if (fd) {
				fd->loadFromCmd(action.arguments, this);
				ks.add(ActionFunction(fd, mod));
			}
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
					}
				} else if (sub.type == CmdAction::FuncCall) {
					Modifier mod = modifierFromCmd(sub.modifier);
					FunctionData *fd = createFunctionData(sub.name);
					if (fd) {
						fd->loadFromCmd(sub.arguments, this);
						subKs.add(ActionFunction(fd, mod));
					}
				}
			}
			KeySeq *addedSub = addKeySeq(subKs);
			ks.add(ActionKeySeq(addedSub));
			break;
		}
		}
	}
	return addKeySeq(ks);
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
			FunctionData *fd = createFunctionData(action.name);
			if (fd) {
				fd->loadFromCmd(action.arguments, this);
				ks->add(ActionFunction(fd, mod));
			}
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
					FunctionData *fd = createFunctionData(sub.name);
					if (fd) {
						fd->loadFromCmd(sub.arguments, this);
						subKs->add(ActionFunction(fd, mod));
					}
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
