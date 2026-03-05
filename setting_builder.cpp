//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// setting_builder.cpp


#include "misc.h"

#include "setting_builder.h"


KeySeq *SettingBuilder::materializeKeySeq(const CmdKeySequence &cmdKs)
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
