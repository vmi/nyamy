//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// setting_dump.cpp

#include "misc.h"

#include "setting_dump.h"
#include "setting.h"
#include "keymap.h"
#include "keyboard.h"
#include "function.h"

#include <algorithm>
#include <sstream>
#include <vector>


std::wstring dumpSetting(const Setting &i_setting)
{
	std::wostringstream os;

	// --- symbols (std::set is already sorted) ---
	os << L"=== symbols ===" << std::endl;
	for (const auto &sym : i_setting.m_symbols)
		os << sym << std::endl;

	// --- options ---
	os << L"=== options ===" << std::endl;
	os << L"correctKanaLockHandling=" << (i_setting.m_correctKanaLockHandling ? 1 : 0) << std::endl;
	os << L"mouseEvent=" << (i_setting.m_mouseEvent ? 1 : 0) << std::endl;
	os << L"dragThreshold=" << i_setting.m_dragThreshold << std::endl;
	os << L"oneShotRepeatableDelay=" << i_setting.m_oneShotRepeatableDelay << std::endl;

	// KeyIterator / getModifiers / etc. are non-const accessors.
	Keyboard &kb = const_cast<Keyboard &>(i_setting.m_keyboard);

	// --- keys (name + scan codes) ---
	{
		std::vector<std::wstring> lines;
		Keyboard::KeyIterator it = kb.getKeyIterator();
		while (Key *k = *it) {
			std::wostringstream l;
			l << k->getName();
			for (size_t j = 0; j < k->getScanCodesSize(); ++j) {
				const ScanCode &sc = k->getScanCodes()[j];
				l << L" " << std::hex << static_cast<unsigned>(sc.m_flags)
				  << L":" << static_cast<unsigned>(sc.m_scan) << std::dec;
			}
			lines.push_back(l.str());
			++it;
		}
		std::sort(lines.begin(), lines.end());
		os << L"=== keys ===" << std::endl;
		for (const auto &ln : lines)
			os << ln << std::endl;
	}

	// --- keyboard modifiers (basic modifiers only) ---
	os << L"=== modifiers ===" << std::endl;
	for (int t = Modifier::Type_begin; t < Modifier::Type_BASIC; ++t) {
		Modifier::Type mt = static_cast<Modifier::Type>(t);
		Keyboard::Mods &mods = kb.getModifiers(mt);
		std::vector<std::wstring> names;
		for (Key *k : mods)
			names.push_back(std::wstring(k->getName().c_str()));
		std::sort(names.begin(), names.end());
		os << mt << L" =";
		for (const auto &n : names)
			os << L" " << n;
		os << std::endl;
	}

	// --- aliases ---
	{
		os << L"=== aliases ===" << std::endl;
		std::vector<std::wstring> lines;
		for (const auto &a : kb.getAliases()) {
			std::wostringstream l;
			l << a.first << L" -> " << a.second->getName();
			lines.push_back(l.str());
		}
		std::sort(lines.begin(), lines.end());
		for (const auto &ln : lines)
			os << ln << std::endl;
	}

	// --- substitutes ---
	{
		os << L"=== substitutes ===" << std::endl;
		std::vector<std::wstring> lines;
		for (const auto &sub : kb.getSubstitutes()) {
			std::wostringstream l;
			l << sub.m_mkeyFrom << L" => " << sub.m_mkeyTo;
			lines.push_back(l.str());
		}
		std::sort(lines.begin(), lines.end());
		for (const auto &ln : lines)
			os << ln << std::endl;
	}

	// --- keymaps (sorted by name; describe() inlines key sequences) ---
	{
		os << L"=== keymaps ===" << std::endl;
		std::vector<const Keymap *> kms;
		for (const Keymap &km : i_setting.m_keymaps)
			kms.push_back(&km);
		std::sort(kms.begin(), kms.end(),
			[](const Keymap *a, const Keymap *b) {
				return std::wstring(a->getName().c_str())
				     < std::wstring(b->getName().c_str());
			});
		for (const Keymap *km : kms) {
			Keymap::DescribeParam dp;
			km->describe(os, &dp);
		}
	}

	return os.str();
}
