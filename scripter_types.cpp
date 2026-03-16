//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// scripter_types.cpp

#include "misc.h"

#include "scripter_types.h"
#include "keyboard.h"


static const struct {
	Modifier::Type type;
	const wchar_t *name;
} g_modNameTable[] = {
	{ Modifier::Type_Shift,          L"S-" },
	{ Modifier::Type_Alt,            L"A-" },
	{ Modifier::Type_Control,        L"C-" },
	{ Modifier::Type_Windows,        L"W-" },
	{ Modifier::Type_Up,             L"U-" },
	{ Modifier::Type_Down,           L"D-" },
	{ Modifier::Type_Repeat,         L"R-" },
	{ Modifier::Type_ImeLock,        L"IL-" },
	{ Modifier::Type_ImeComp,        L"IC-" },
	{ Modifier::Type_NumLock,        L"NL-" },
	{ Modifier::Type_CapsLock,       L"CL-" },
	{ Modifier::Type_ScrollLock,     L"SL-" },
	{ Modifier::Type_KanaLock,       L"KL-" },
	{ Modifier::Type_Maximized,      L"MAX-" },
	{ Modifier::Type_Minimized,      L"MIN-" },
	{ Modifier::Type_MdiMaximized,   L"MMAX-" },
	{ Modifier::Type_MdiMinimized,   L"MMIN-" },
	{ Modifier::Type_Mod0,           L"M0-" },
	{ Modifier::Type_Mod1,           L"M1-" },
	{ Modifier::Type_Mod2,           L"M2-" },
	{ Modifier::Type_Mod3,           L"M3-" },
	{ Modifier::Type_Mod4,           L"M4-" },
	{ Modifier::Type_Mod5,           L"M5-" },
	{ Modifier::Type_Mod6,           L"M6-" },
	{ Modifier::Type_Mod7,           L"M7-" },
	{ Modifier::Type_Mod8,           L"M8-" },
	{ Modifier::Type_Mod9,           L"M9-" },
	{ Modifier::Type_Lock0,          L"L0-" },
	{ Modifier::Type_Lock1,          L"L1-" },
	{ Modifier::Type_Lock2,          L"L2-" },
	{ Modifier::Type_Lock3,          L"L3-" },
	{ Modifier::Type_Lock4,          L"L4-" },
	{ Modifier::Type_Lock5,          L"L5-" },
	{ Modifier::Type_Lock6,          L"L6-" },
	{ Modifier::Type_Lock7,          L"L7-" },
	{ Modifier::Type_Lock8,          L"L8-" },
	{ Modifier::Type_Lock9,          L"L9-" },
};


std::wostream& operator<<(std::wostream& out, const ModifierSpec& mod)
{
	out << L"{";
	bool first = true;
	for (size_t i = 0; i < NUMBER_OF(g_modNameTable); ++i) {
		uint64_t bit = static_cast<uint64_t>(1) << g_modNameTable[i].type;
		if (mod.dontcares & bit) {
			if (!first) out << L" ";
			out << L"*" << g_modNameTable[i].name;
			first = false;
		} else if (mod.modifiers & bit) {
			if (!first) out << L" ";
			out << g_modNameTable[i].name;
			first = false;
		}
	}
	out << L"}";
	return out;
}


std::wostream& operator<<(std::wostream& out, const FuncArg& arg)
{
	std::visit(overloaded{
		[&](const FuncArgString&    a) { out << a; },
		[&](const FuncArgNumber&    a) { out << a; },
		[&](const FuncArgRegexp&    a) { out << a; },
		[&](const FuncArgKeySeqIdx& a) { out << L"@" << a; },
		[&](const FuncArgModSeq&    a) { out << a; },
		[&](const FuncArgTokenSeq&  a) {
			out << L"[";
			for (size_t i = 0; i < a.size(); ++i) {
				if (i) out << L" ";
				out << a[i];
			}
			out << L"]";
		},
	}, arg);
	return out;
}
