//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// function.cpp


#include "cmd_stream.h"
#include "engine.h"
#include "hook.h"
#include "mayu.h"
#include "mayurc.h"
#include "misc.h"
#include "registry.h"
#include "vkeytable.h"
#include "windowstool.h"
#include <algorithm>
#include <process.h>

VKey loadVKeyFromCmd(const CmdFuncArg &arg)
{
	// Mirrors the VKey encoding logic used in the scripter pipeline.
	// Accepts Number (pre-encoded), String (bare key name), or
	// TokenSeq (prefix tokens + key name, e.g. ["U-", "D-", "RButton"]).
	std::vector<wstringi> toks;
	if (arg.type == CmdFuncArg::TokenSeq)
		toks = arg.tokens;
	else if (arg.type == CmdFuncArg::String)
		toks.push_back(arg.stringValue);
	else
		return static_cast<VKey>(arg.numberValue); // Number: already encoded

	int vkey = 0;
	for (const auto &tok : toks) {
		if (tok == L"E-") { vkey |= VKey_extended; continue; }
		if (tok == L"U-") { vkey |= VKey_released; continue; }
		if (tok == L"D-") { vkey |= VKey_pressed;  continue; }
		// Last token: virtual key name (e.g. "RButton") or numeric code
		const VKeyTable *vkt;
		for (vkt = g_vkeyTable; vkt->m_name; ++vkt)
			if (wstringi(vkt->m_name) == tok)
				break;
		if (vkt->m_name)
			vkey |= vkt->m_code;
	}
	// Default state: if neither U- nor D- specified, apply both
	if (!(vkey & VKey_released) && !(vkey & VKey_pressed))
		vkey |= VKey_released | VKey_pressed;
	return static_cast<VKey>(vkey);
}


#define FUNCTION_DATA
#include "functions.h"
#undef FUNCTION_DATA


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// TypeTable


template <class T> class TypeTable
{
public:
	T m_type;
	const wchar_t *m_name;
};


template <class T> static inline
bool getTypeName(std::wstring *o_name, T i_type,
				 const TypeTable<T> *i_table, size_t i_n)
{
	for (size_t i = 0; i < i_n; ++ i)
		if (i_table[i].m_type == i_type) {
			*o_name = i_table[i].m_name;
			return true;
		}
	return false;
}

template <class T> static inline
bool getTypeValue(T *o_type, const wstringi &i_name,
				  const TypeTable<T> *i_table, size_t i_n)
{
	for (size_t i = 0; i < i_n; ++ i)
		if (i_table[i].m_name == i_name) {
			*o_type = i_table[i].m_type;
			return true;
		}
	return false;
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// VKey


// stream output
std::wostream &operator<<(std::wostream &i_ost, VKey i_data)
{
	if (i_data & VKey_extended)
		i_ost << L"E-";
	if (i_data & VKey_released)
		i_ost << L"U-";
	if (i_data & VKey_pressed)
		i_ost << L"D-";

	u_int8 code = i_data & ~(VKey_extended | VKey_released | VKey_pressed);
	const VKeyTable *vkt;
	for (vkt = g_vkeyTable; vkt->m_name; ++ vkt)
		if (vkt->m_code == code)
			break;
	if (vkt->m_name)
		i_ost << vkt->m_name;
	else
		i_ost << L"0x" << std::hex << code << std::dec;
	return i_ost;
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ToWindowType


// ToWindowType table
using TypeTable_ToWindowType = TypeTable<ToWindowType>;
static const TypeTable_ToWindowType g_toWindowTypeTable[] = {
	{ ToWindowType_toOverlappedWindow, L"toOverlappedWindow" },
	{ ToWindowType_toMainWindow,       L"toMainWindow"       },
	{ ToWindowType_toItself,           L"toItself"           },
	{ ToWindowType_toParentWindow,     L"toParentWindow"     },
};


// stream output
std::wostream &operator<<(std::wostream &i_ost, ToWindowType i_data)
{
	std::wstring name;
	if (getTypeName(&name, i_data,
					g_toWindowTypeTable, NUMBER_OF(g_toWindowTypeTable)))
		i_ost << name;
	else
		i_ost << static_cast<int>(i_data);
	return i_ost;
}


// get value of ToWindowType
bool getTypeValue(ToWindowType *o_type, const std::wstring &i_name)
{
	return getTypeValue(o_type, i_name,
						g_toWindowTypeTable, NUMBER_OF(g_toWindowTypeTable));
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// GravityType


// GravityType table
using TypeTable_GravityType = TypeTable<GravityType>;
static const TypeTable_GravityType g_gravityTypeTable[] = {
	{ GravityType_C,  L"C"  },
	{ GravityType_N,  L"N"  },
	{ GravityType_E,  L"E"  },
	{ GravityType_W,  L"W"  },
	{ GravityType_S,  L"S"  },
	{ GravityType_NW, L"NW" },
	{ GravityType_NW, L"WN" },
	{ GravityType_NE, L"NE" },
	{ GravityType_NE, L"EN" },
	{ GravityType_SW, L"SW" },
	{ GravityType_SW, L"WS" },
	{ GravityType_SE, L"SE" },
	{ GravityType_SE, L"ES" },
};


// stream output
std::wostream &operator<<(std::wostream &i_ost, GravityType i_data)
{
	std::wstring name;
	if (getTypeName(&name, i_data,
					g_gravityTypeTable, NUMBER_OF(g_gravityTypeTable)))
		i_ost << name;
	else
		i_ost << L"(GravityType internal error)";
	return i_ost;
}


// get value of GravityType
bool getTypeValue(GravityType *o_type, const std::wstring &i_name)
{
	return getTypeValue(o_type, i_name,
						g_gravityTypeTable, NUMBER_OF(g_gravityTypeTable));
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// MouseHookType


// MouseHookType table
using TypeTable_MouseHookType = TypeTable<MouseHookType>;
static const TypeTable_MouseHookType g_mouseHookTypeTable[] = {
	{ MouseHookType_None,  L"None"  },
	{ MouseHookType_Wheel,  L"Wheel"  },
	{ MouseHookType_WindowMove,  L"WindowMove"  },
};


// stream output
std::wostream &operator<<(std::wostream &i_ost, MouseHookType i_data)
{
	std::wstring name;
	if (getTypeName(&name, i_data,
					g_mouseHookTypeTable, NUMBER_OF(g_mouseHookTypeTable)))
		i_ost << name;
	else
		i_ost << L"(MouseHookType internal error)";
	return i_ost;
}


// get value of MouseHookType
bool getTypeValue(MouseHookType *o_type, const std::wstring &i_name)
{
	return getTypeValue(o_type, i_name, g_mouseHookTypeTable,
						NUMBER_OF(g_mouseHookTypeTable));
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// MayuDialogType


// ModifierLockType table
using TypeTable_MayuDialogType = TypeTable<MayuDialogType>;
static const TypeTable_MayuDialogType g_mayuDialogTypeTable[] = {
	{ MayuDialogType_investigate, L"investigate"  },
	{ MayuDialogType_log,         L"log"          },
};


// stream output
std::wostream &operator<<(std::wostream &i_ost, MayuDialogType i_data)
{
	std::wstring name;
	if (getTypeName(&name, i_data,
					g_mayuDialogTypeTable, NUMBER_OF(g_mayuDialogTypeTable)))
		i_ost << name;
	else
		i_ost << L"(MayuDialogType internal error)";
	return i_ost;
}


// get value of MayuDialogType
bool getTypeValue(MayuDialogType *o_type, const std::wstring &i_name)
{
	return getTypeValue(o_type, i_name, g_mayuDialogTypeTable,
						NUMBER_OF(g_mayuDialogTypeTable));
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ToggleType


// ToggleType table
using TypeTable_ToggleType = TypeTable<ToggleType>;
static const TypeTable_ToggleType g_toggleType[] = {
	{ ToggleType_toggle, L"toggle" },
	{ ToggleType_off, L"off" },
	{ ToggleType_off, L"false" },
	{ ToggleType_off, L"released" },
	{ ToggleType_on,  L"on"  },
	{ ToggleType_on,  L"true"  },
	{ ToggleType_on,  L"pressed"  },
};


// stream output
std::wostream &operator<<(std::wostream &i_ost, ToggleType i_data)
{
	std::wstring name;
	if (getTypeName(&name, i_data, g_toggleType, NUMBER_OF(g_toggleType)))
		i_ost << name;
	else
		i_ost << L"(ToggleType internal error)";
	return i_ost;
}


// get value of ToggleType
bool getTypeValue(ToggleType *o_type, const std::wstring &i_name)
{
	return getTypeValue(o_type, i_name, g_toggleType,
						NUMBER_OF(g_toggleType));
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ModifierLockType


// ModifierLockType table
using TypeTable_ModifierLockType = TypeTable<ModifierLockType>;
static const TypeTable_ModifierLockType g_modifierLockTypeTable[] = {
	{ ModifierLockType_Lock0, L"lock0" },
	{ ModifierLockType_Lock1, L"lock1" },
	{ ModifierLockType_Lock2, L"lock2" },
	{ ModifierLockType_Lock3, L"lock3" },
	{ ModifierLockType_Lock4, L"lock4" },
	{ ModifierLockType_Lock5, L"lock5" },
	{ ModifierLockType_Lock6, L"lock6" },
	{ ModifierLockType_Lock7, L"lock7" },
	{ ModifierLockType_Lock8, L"lock8" },
	{ ModifierLockType_Lock9, L"lock9" },
};


// stream output
std::wostream &operator<<(std::wostream &i_ost, ModifierLockType i_data)
{
	std::wstring name;
	if (getTypeName(&name, i_data,
					g_modifierLockTypeTable, NUMBER_OF(g_modifierLockTypeTable)))
		i_ost << name;
	else
		i_ost << L"(ModifierLockType internal error)";
	return i_ost;
}


// get value of ModifierLockType
bool getTypeValue(ModifierLockType *o_type, const std::wstring &i_name)
{
	return getTypeValue(o_type, i_name, g_modifierLockTypeTable,
						NUMBER_OF(g_modifierLockTypeTable));
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ShowCommandType


// ShowCommandType table
using TypeTable_ShowCommandType = TypeTable<ShowCommandType>;
static const TypeTable_ShowCommandType g_showCommandTypeTable[] = {
	{ ShowCommandType_hide,            L"hide"            },
	{ ShowCommandType_maximize,        L"maximize"        },
	{ ShowCommandType_minimize,        L"minimize"        },
	{ ShowCommandType_restore,         L"restore"         },
	{ ShowCommandType_show,            L"show"            },
	{ ShowCommandType_showDefault,     L"showDefault"     },
	{ ShowCommandType_showMaximized,   L"showMaximized"   },
	{ ShowCommandType_showMinimized,   L"showMinimized"   },
	{ ShowCommandType_showMinNoActive, L"showMinNoActive" },
	{ ShowCommandType_showNA,          L"showNA"          },
	{ ShowCommandType_showNoActivate,  L"showNoActivate"  },
	{ ShowCommandType_showNormal,      L"showNormal"      },
};


// stream output
std::wostream &operator<<(std::wostream &i_ost, ShowCommandType i_data)
{
	std::wstring name;
	if (getTypeName(&name, i_data,
					g_showCommandTypeTable, NUMBER_OF(g_showCommandTypeTable)))
		i_ost << name;
	else
		i_ost << L"(ShowCommandType internal error)";
	return i_ost;
}


// get value of ShowCommandType
bool getTypeValue(ShowCommandType *o_type, const std::wstring &i_name)
{
	return getTypeValue(o_type, i_name, g_showCommandTypeTable,
						NUMBER_OF(g_showCommandTypeTable));
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// TargetWindowType


// ModifierLockType table
using TypeTable_TargetWindowType = TypeTable<TargetWindowType>;
static const TypeTable_TargetWindowType g_targetWindowType[] = {
	{ TargetWindowType_overlapped, L"overlapped" },
	{ TargetWindowType_mdi,        L"mdi"        },
};


// stream output
std::wostream &operator<<(std::wostream &i_ost, TargetWindowType i_data)
{
	std::wstring name;
	if (getTypeName(&name, i_data,
					g_targetWindowType, NUMBER_OF(g_targetWindowType)))
		i_ost << name;
	else
		i_ost << L"(TargetWindowType internal error)";
	return i_ost;
}


// get value of TargetWindowType
bool getTypeValue(TargetWindowType *o_type, const std::wstring &i_name)
{
	return getTypeValue(o_type, i_name, g_targetWindowType,
						NUMBER_OF(g_targetWindowType));
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// BooleanType


// BooleanType table
using TypeTable_BooleanType = TypeTable<BooleanType>;
static const TypeTable_BooleanType g_booleanType[] = {
	{ BooleanType_false, L"false" },
	{ BooleanType_true,  L"true"  },
};


// stream output
std::wostream &operator<<(std::wostream &i_ost, BooleanType i_data)
{
	std::wstring name;
	if (getTypeName(&name, i_data, g_booleanType, NUMBER_OF(g_booleanType)))
		i_ost << name;
	else
		i_ost << L"(BooleanType internal error)";
	return i_ost;
}


// get value of BooleanType
bool getTypeValue(BooleanType *o_type, const std::wstring &i_name)
{
	return getTypeValue(o_type, i_name, g_booleanType,
						NUMBER_OF(g_booleanType));
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// LogicalOperatorType


// LogicalOperatorType table
using TypeTable_LogicalOperatorType = TypeTable<LogicalOperatorType>;
static const TypeTable_LogicalOperatorType g_logicalOperatorType[] = {
	{ LogicalOperatorType_or, L"||" },
	{ LogicalOperatorType_and,  L"&&"  },
};


// stream output
std::wostream &operator<<(std::wostream &i_ost, LogicalOperatorType i_data)
{
	std::wstring name;
	if (getTypeName(&name, i_data, g_logicalOperatorType,
					NUMBER_OF(g_logicalOperatorType)))
		i_ost << name;
	else
		i_ost << L"(LogicalOperatorType internal error)";
	return i_ost;
}


// get value of LogicalOperatorType
bool getTypeValue(LogicalOperatorType *o_type, const std::wstring &i_name)
{
	return getTypeValue(o_type, i_name, g_logicalOperatorType,
						NUMBER_OF(g_logicalOperatorType));
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// WindowMonitorFromType


// WindowMonitorFromType table
using TypeTable_WindowMonitorFromType = TypeTable<WindowMonitorFromType>;
static const TypeTable_WindowMonitorFromType g_windowMonitorFromType[] = {
	{ WindowMonitorFromType_primary, L"primary" },
	{ WindowMonitorFromType_current, L"current" },
};


// stream output
std::wostream &operator<<(std::wostream &i_ost, WindowMonitorFromType i_data)
{
	std::wstring name;
	if (getTypeName(&name, i_data, g_windowMonitorFromType,
					NUMBER_OF(g_windowMonitorFromType)))
		i_ost << name;
	else
		i_ost << L"(WindowMonitorFromType internal error)";
	return i_ost;
}


// get value of WindowMonitorFromType
bool getTypeValue(WindowMonitorFromType *o_type, const std::wstring &i_name)
{
	return getTypeValue(o_type, i_name, g_windowMonitorFromType,
						NUMBER_OF(g_windowMonitorFromType));
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// std::list<wstringq>


/// stream output
std::wostream &operator<<(std::wostream &i_ost, const std::list<wstringq> &i_data)
{
	for (std::list<wstringq>::const_iterator
			i = i_data.begin(); i != i_data.end(); ++ i) {
		i_ost << *i << L", ";
	}
	return i_ost;
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// FunctionData


//
FunctionData::~FunctionData()
{
}


// stream output
std::wostream &operator<<(std::wostream &i_ost, const FunctionData *i_data)
{
	return i_data->output(i_ost);
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// FunctionData_UserFunc
//
// Implements &UserFunc("name", arg1, arg2, ...) - invokes a user-defined
// function in the scripter process via Engine::callExecUserFuncCallback().
//
// NOTE: FuncArg, TriggerInfo, and callExecUserFuncCallback() are
// added in Stage 2 (ctrl_stream.h / engine.h changes).  This class will
// compile once those definitions are in place.


class FunctionData_UserFunc : public FunctionData
{
	wstringi m_name;                  ///< function name (args[0])
	std::vector<FuncArg> m_args;      ///< additional arguments (args[1..])

public:
	static FunctionData *create() { return new FunctionData_UserFunc; }

	void exec(Engine *i_engine, FunctionParam *i_param) const override
	{
		TriggerInfo ctx;

		// Trigger key info: read from Current::m_mkey.m_key
		// getScanCodes() returns const ScanCode*; check size via getScanCodesSize()
		if (i_param && i_param->m_c.m_mkey.m_key) {
			const Key *key = i_param->m_c.m_mkey.m_key;
			if (key->getScanCodesSize() > 0) {
				const ScanCode &sc = key->getScanCodes()[0];
				ctx.scanCode = static_cast<uint8_t>(sc.m_scan);
				ctx.extended = (sc.m_flags & ScanCode::E0) != 0;
			}
		}

		// Focus window info: use Engine public accessors
		// (FunctionParam does not expose m_currentFocusOfThread)
		ctx.windowClass = i_engine->getCurrentWindowClassName();
		ctx.windowTitle = i_engine->getCurrentWindowTitleName();

		i_engine->callExecUserFuncCallback(m_name, m_args, ctx);
	}

	const wchar_t *getName() const override { return L"UserFunc"; }

	std::wostream &output(std::wostream &i_ost) const override
	{
		i_ost << L"&" << getName() << L"(" << m_name;
		for (const auto &arg : m_args) {
			i_ost << L", ";
			if (std::holds_alternative<int64_t>(arg))
				i_ost << std::get<int64_t>(arg);
			else
				i_ost << L"\"" << std::get<std::wstring>(arg) << L"\"";
		}
		i_ost << L")";
		return i_ost;
	}

	void loadFromCmd(const std::vector<CmdFuncArg> &i_args, CmdLoadContext *) override
	{
		if (!i_args.empty())
			m_name = i_args[0].stringValue;
		for (size_t i = 1; i < i_args.size(); ++i) {
			const auto &a = i_args[i];
			if (a.type == CmdFuncArg::Number)
				m_args.push_back(static_cast<int64_t>(a.numberValue));
			else
				m_args.push_back(std::wstring(a.stringValue));
		}
	}

	FunctionData *clone() const override { return new FunctionData_UserFunc(*this); }
};


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// FunctionCreator


///
class FunctionCreator
{
public:
	using Creator = FunctionData *(*)();		///

public:
	const wchar_t *m_name;				/// function name
	Creator m_creator;				/// function data creator
};


// create function
FunctionData *createFunctionData(const std::wstring &i_name)
{
	static
#define FUNCTION_CREATOR
#include "functions.h"
#undef FUNCTION_CREATOR
	;

	for (size_t i = 0; i != NUMBER_OF(functionCreators); ++ i)
		if (i_name == functionCreators[i].m_name)
			return functionCreators[i].m_creator();
	if (i_name == L"UserFunc")
		return FunctionData_UserFunc::create();
	return NULL;
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// misc. functions


//
bool getSuitableWindow(FunctionParam *i_param, HWND *o_hwnd)
{
	if (!i_param->m_isPressed)
		return false;
	*o_hwnd = getToplevelWindow(i_param->m_hwnd, NULL);
	if (!*o_hwnd)
		return false;
	return true;
}

//
bool getSuitableMdiWindow(FunctionParam *i_param, HWND *o_hwnd,
						  TargetWindowType *io_twt,
						  RECT *o_rcWindow = NULL, RECT *o_rcParent = NULL)
{
	if (!i_param->m_isPressed)
		return false;
	bool isMdi = *io_twt == TargetWindowType_mdi;
	*o_hwnd = getToplevelWindow(i_param->m_hwnd, &isMdi);
	*io_twt = isMdi ? TargetWindowType_mdi : TargetWindowType_overlapped;
	if (!*o_hwnd)
		return false;
	switch (*io_twt) {
	case TargetWindowType_overlapped:
		if (o_rcWindow)
			GetWindowRect(*o_hwnd, o_rcWindow);
		if (o_rcParent) {
			HMONITOR hm = monitorFromWindow(i_param->m_hwnd,
											MONITOR_DEFAULTTONEAREST);
			MONITORINFO mi;
			mi.cbSize = sizeof(mi);
			getMonitorInfo(hm, &mi);
			*o_rcParent = mi.rcWork;
		}
		break;
	case TargetWindowType_mdi:
		if (o_rcWindow)
			getChildWindowRect(*o_hwnd, o_rcWindow);
		if (o_rcParent)
			GetClientRect(GetParent(*o_hwnd), o_rcParent);
		break;
	}
	return true;
}

// get clipboard text (you must call closeClopboard())
static const wchar_t *getTextFromClipboard(HGLOBAL *o_hdata)
{
	*o_hdata = NULL;

	if (!OpenClipboard(NULL))
		return NULL;

	*o_hdata = GetClipboardData(CF_UNICODETEXT);
	if (!*o_hdata)
		return NULL;

	wchar_t *data = reinterpret_cast<wchar_t *>(GlobalLock(*o_hdata));
	if (!data)
		return NULL;
	return data;
}

// close clipboard that opend by getTextFromClipboard()
static void closeClipboard(HGLOBAL i_hdata, HGLOBAL i_hdataNew = NULL)
{
	if (i_hdata)
		GlobalUnlock(i_hdata);
	if (i_hdataNew) {
		EmptyClipboard();
		SetClipboardData(CF_UNICODETEXT, i_hdataNew);
	}
	CloseClipboard();
}


// EmacsEditKillLineFunc.
// clear the contents of the clopboard
// at that time, confirm if it is the result of the previous kill-line
void Engine::EmacsEditKillLine::func()
{
	if (!m_buf.empty()) {
		HGLOBAL g;
		const wchar_t *text = getTextFromClipboard(&g);
		if (text == NULL || m_buf != text)
			reset();
		closeClipboard(g);
	}
	if (OpenClipboard(NULL)) {
		EmptyClipboard();
		CloseClipboard();
	}
}


/** if the text of the clipboard is
@doc
<pre>
1: EDIT Control (at EOL C-K): ""            =&gt; buf + "\r\n", Delete
0: EDIT Control (other  C-K): "(.+)"        =&gt; buf + "\1"
0: IE FORM TEXTAREA (at EOL C-K): "\r\n"    =&gt; buf + "\r\n"
2: IE FORM TEXTAREA (other C-K): "(.+)\r\n" =&gt; buf + "\1", Return Left
^retval
</pre>
*/
HGLOBAL Engine::EmacsEditKillLine::makeNewKillLineBuf(
	const wchar_t *i_data, int *o_retval)
{
	size_t len = m_buf.size();
	len += wcslen(i_data) + 3;

	HGLOBAL hdata = GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE,
								len * sizeof(wchar_t));
	if (!hdata)
		return NULL;
	wchar_t *dataNew = reinterpret_cast<wchar_t *>(GlobalLock(hdata));
	*dataNew = L'\0';
	if (!m_buf.empty())
		wcscpy(dataNew, m_buf.c_str());

	len = wcslen(i_data);
	if (3 <= len &&
			i_data[len - 2] == L'\r' && i_data[len - 1] == L'\n') {
		wcscat(dataNew, i_data);
		len = wcslen(dataNew);
		dataNew[len - 2] = L'\0'; // chomp
		*o_retval = 2;
	} else if (len == 0) {
		wcscat(dataNew, L"\r\n");
		*o_retval = 1;
	} else {
		wcscat(dataNew, i_data);
		*o_retval = 0;
	}

	m_buf = dataNew;

	GlobalUnlock(hdata);
	return hdata;
}


// EmacsEditKillLinePred
int Engine::EmacsEditKillLine::pred()
{
	HGLOBAL g;
	const wchar_t *text = getTextFromClipboard(&g);
	int retval;
	HGLOBAL hdata = makeNewKillLineBuf(text ? text : L"", &retval);
	closeClipboard(g, hdata);
	return retval;
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// functions


// send a default key to Windows
void Engine::funcDefault(FunctionParam *i_param)
{
	{
		Acquire a(&m_log, 1);
		m_log << std::endl;
		i_param->m_doesNeedEndl = false;
	}
	if (i_param->m_isPressed)
		generateModifierEvents(i_param->m_c.m_mkey.m_modifier);
	generateKeyEvent(i_param->m_c.m_mkey.m_key, i_param->m_isPressed, true);
}

// use a corresponding key of a parent keymap
void Engine::funcKeymapParent(FunctionParam *i_param)
{
	Current c(i_param->m_c);
	c.m_keymap = c.m_keymap->getParentKeymap();
	if (!c.m_keymap) {
		funcDefault(i_param);
		return;
	}

	{
		Acquire a(&m_log, 1);
		m_log << L"(" << c.m_keymap->getName() << L")" << std::endl;
	}
	i_param->m_doesNeedEndl = false;
	generateKeyboardEvents(c);
}

// use a corresponding key of a current window
void Engine::funcKeymapWindow(FunctionParam *i_param)
{
	Current c(i_param->m_c);
	c.m_keymap = m_currentFocusOfThread->m_keymaps.front();
	c.m_i = m_currentFocusOfThread->m_keymaps.begin();
	generateKeyboardEvents(c);
}

// use a corresponding key of the previous prefixed keymap
void Engine::funcKeymapPrevPrefix(FunctionParam *i_param, int i_previous)
{
	Current c(i_param->m_c);
	if (0 < i_previous && 0 <= m_keymapPrefixHistory.size() - i_previous) {
		int n = i_previous - 1;
		KeymapPtrList::reverse_iterator i = m_keymapPrefixHistory.rbegin();
		while (0 < n && i != m_keymapPrefixHistory.rend())
			--n, ++i;
		c.m_keymap = *i;
		generateKeyboardEvents(c);
	}
}

// use a corresponding key of an other window class, or use a default key
void Engine::funcOtherWindowClass(FunctionParam *i_param)
{
	Current c(i_param->m_c);
	++ c.m_i;
	if (c.m_i == m_currentFocusOfThread->m_keymaps.end()) {
		funcDefault(i_param);
		return;
	}

	c.m_keymap = *c.m_i;
	{
		Acquire a(&m_log, 1);
		m_log << L"(" << c.m_keymap->getName() << L")" << std::endl;
	}
	i_param->m_doesNeedEndl = false;
	generateKeyboardEvents(c);
}

// prefix key
void Engine::funcPrefix(FunctionParam *i_param, const Keymap *i_keymap,
						BooleanType i_doesIgnoreModifiers)
{
	if (!i_param->m_isPressed)
		return;

	setCurrentKeymap(i_keymap, true);

	// generate prefixed event
	generateEvents(i_param->m_c, m_currentKeymap, &Event::prefixed);

	m_isPrefix = true;
	m_doesEditNextModifier = false;
	m_doesIgnoreModifierForPrefix = !!i_doesIgnoreModifiers;

	{
		Acquire a(&m_log, 1);
		m_log << L"(" << i_keymap->getName() << L", "
		<< (i_doesIgnoreModifiers ? L"true" : L"false") << L")";
	}
}

// other keymap's key
void Engine::funcKeymap(FunctionParam *i_param, const Keymap *i_keymap)
{
	Current c(i_param->m_c);
	c.m_keymap = i_keymap;
	{
		Acquire a(&m_log, 1);
		m_log << L"(" << c.m_keymap->getName() << L")" << std::endl;
		i_param->m_doesNeedEndl = false;
	}
	generateKeyboardEvents(c);
}

// sync
void Engine::funcSync(FunctionParam *i_param)
{
	if (i_param->m_isPressed)
		generateModifierEvents(i_param->m_af->m_modifier);
	if (!i_param->m_isPressed || m_currentFocusOfThread->m_isConsole)
		return;

	Key *sync = m_setting.load(std::memory_order_relaxed)->m_keyboard.getSyncKey();
	if (sync->getScanCodesSize() == 0)
		return;
	const ScanCode *sc = sync->getScanCodes();

	// set variables exported from mayu.dll
	g_hookData->m_syncKey = sc->m_scan;
	g_hookData->m_syncKeyIsExtended = !!(sc->m_flags & ScanCode::E0E1);
	m_isSynchronizing = true;
	generateKeyEvent(sync, false, false);

	m_mutex.unlock();
	DWORD r = WaitForSingleObject(m_eSync, 5000);
	if (r == WAIT_TIMEOUT) {
		Acquire a(&m_log, 0);
		m_log << L" *FAILED*" << std::endl;
	}
	m_mutex.lock();
	m_isSynchronizing = false;
}

// toggle lock
void Engine::funcToggle(FunctionParam *i_param, ModifierLockType i_lock,
						ToggleType i_toggle)
{
	if (i_param->m_isPressed)			// ignore PRESS
		return;

	Modifier::Type mt = static_cast<Modifier::Type>(i_lock);
	switch (i_toggle) {
	case ToggleType_toggle:
		m_currentLock.press(mt, !m_currentLock.isPressed(mt));
		break;
	case ToggleType_off:
		m_currentLock.press(mt, false);
		break;
	case ToggleType_on:
		m_currentLock.press(mt, true);
		break;
	}
}

// edit next user input key's modifier
void Engine::funcEditNextModifier(FunctionParam *i_param,
								  const Modifier &i_modifier)
{
	if (!i_param->m_isPressed)
		return;

	m_isPrefix = true;
	m_doesEditNextModifier = true;
	m_doesIgnoreModifierForPrefix = true;
	m_modifierForNextKey = i_modifier;
}

// variable
void Engine::funcVariable(FunctionParam *i_param, int i_mag, int i_inc)
{
	if (!i_param->m_isPressed)
		return;
	m_variable *= i_mag;
	m_variable += i_inc;
}

// repeat N times
void Engine::funcRepeat(FunctionParam *i_param, const KeySeq *i_keySeq,
						int i_max)
{
	if (i_param->m_isPressed) {
		int end = MIN(m_variable, i_max);
		for (int i = 0; i < end - 1; ++ i)
			generateKeySeqEvents(i_param->m_c, i_keySeq, Part_all);
		if (0 < end)
			generateKeySeqEvents(i_param->m_c, i_keySeq, Part_down);
	} else
		generateKeySeqEvents(i_param->m_c, i_keySeq, Part_up);
}

// undefined (bell)
void Engine::funcUndefined(FunctionParam *i_param)
{
	if (!i_param->m_isPressed)
		return;
	MessageBeep(MB_OK);
}

// ignore
void Engine::funcIgnore(FunctionParam *)
{
	// do nothing
}

// post message
void Engine::funcPostMessage(FunctionParam *i_param, ToWindowType i_window,
							 UINT i_message, WPARAM i_wParam, LPARAM i_lParam)
{
	if (!i_param->m_isPressed)
		return;

	int window = static_cast<int>(i_window);

	HWND hwnd = i_param->m_hwnd;
	if (0 < window) {
		for (int i = 0; i < window; ++ i)
			hwnd = GetParent(hwnd);
	} else if (window == ToWindowType_toMainWindow) {
		while (true) {
			HWND p = GetParent(hwnd);
			if (!p)
				break;
			hwnd = p;
		}
	} else if (window == ToWindowType_toOverlappedWindow) {
		while (hwnd) {
			LONG_PTR style = GetWindowLongPtr(hwnd, GWL_STYLE);
			if ((style & WS_CHILD) == 0)
				break;
			hwnd = GetParent(hwnd);
		}
	}

	if (hwnd)
		PostMessage(hwnd, i_message, i_wParam, i_lParam);
}


// ShellExecute
void Engine::funcShellExecute(FunctionParam *i_param,
							  const StrExprArg &/*i_operation*/,
							  const StrExprArg &/*i_file*/,
							  const StrExprArg &/*i_parameters*/,
							  const StrExprArg &/*i_directory*/,
							  ShowCommandType /*i_showCommand*/)
{
	if (!i_param->m_isPressed)
		return;
	m_afShellExecute = i_param->m_af;
	PostMessage(m_hwndAssocWindow,
				WM_APP_engineNotify, EngineNotify_shellExecute, 0);
}


// shell execute
void Engine::shellExecute()
{
	std::lock_guard<std::recursive_mutex> lock(m_mutex);

	FunctionData_ShellExecute *fd =
		reinterpret_cast<FunctionData_ShellExecute *>(
			m_afShellExecute->m_functionData.get());

	int r = static_cast<int>(reinterpret_cast<intptr_t>(ShellExecute(
				NULL,
				fd->m_operation.eval().empty() ? L"open" : fd->m_operation.eval().c_str(),
				fd->m_file.eval().empty() ? NULL : fd->m_file.eval().c_str(),
				fd->m_parameters.eval().empty() ? NULL : fd->m_parameters.eval().c_str(),
				fd->m_directory.eval().empty() ? NULL : fd->m_directory.eval().c_str(),
				fd->m_showCommand)));
	if (32 < r)
		return; // success

	using ErrorTable = TypeTable<int>;
	static const ErrorTable errorTable[] = {
		{ 0, L"The operating system is out of memory or resources." },
		{ ERROR_FILE_NOT_FOUND, L"The specified file was not found." },
		{ ERROR_PATH_NOT_FOUND, L"The specified path was not found." },
		{ ERROR_BAD_FORMAT, L"The .exe file is invalid "
		  L"(non-Win32R .exe or error in .exe image)." },
		{ SE_ERR_ACCESSDENIED,
		  L"The operating system denied access to the specified file." },
		{ SE_ERR_ASSOCINCOMPLETE,
		  L"The file name association is incomplete or invalid." },
		{ SE_ERR_DDEBUSY,
		  L"The DDE transaction could not be completed "
		  L"because other DDE transactions were being processed. " },
		{ SE_ERR_DDEFAIL, L"The DDE transaction failed." },
		{ SE_ERR_DDETIMEOUT, L"The DDE transaction could not be completed "
		  L"because the request timed out." },
		{ SE_ERR_DLLNOTFOUND,
		  L"The specified dynamic-link library was not found." },
		{ SE_ERR_FNF, L"The specified file was not found." },
		{ SE_ERR_NOASSOC, L"There is no application associated "
		  L"with the given file name extension." },
		{ SE_ERR_OOM,
		  L"There was not enough memory to complete the operation." },
		{ SE_ERR_PNF, L"The specified path was not found." },
		{ SE_ERR_SHARE, L"A sharing violation occurred." },
	};

	std::wstring errorMessage(L"Unknown error.");
	getTypeName(&errorMessage, r, errorTable, NUMBER_OF(errorTable));

	Acquire b(&m_log, 0);
	m_log << L"error: " << fd << L": " << errorMessage << std::endl;
}


struct EnumWindowsForSetForegroundWindowParam {
	const FunctionData_SetForegroundWindow *m_fd;
	HWND m_hwnd;

public:
	EnumWindowsForSetForegroundWindowParam(
		const FunctionData_SetForegroundWindow *i_fd)
			: m_fd(i_fd),
			m_hwnd(NULL) {
	}
};


/// enum windows for SetForegroundWindow
static BOOL CALLBACK enumWindowsForSetForegroundWindow(
	HWND i_hwnd, LPARAM i_lParam)
{
	EnumWindowsForSetForegroundWindowParam &ep =
		*reinterpret_cast<EnumWindowsForSetForegroundWindowParam *>(i_lParam);

	wchar_t name[GANA_MAX_ATOM_LENGTH];
	if (!GetClassName(i_hwnd, name, NUMBER_OF(name)))
		return TRUE;
	std::wsmatch what;
	std::wstring name1(name);
	if (!std::regex_search(name1, what, ep.m_fd->m_windowClassName))
		if (ep.m_fd->m_logicalOp == LogicalOperatorType_and)
			return TRUE;				// match failed

	if (ep.m_fd->m_logicalOp == LogicalOperatorType_and) {
		if (GetWindowText(i_hwnd, name, NUMBER_OF(name)) == 0)
			name[0] = L'\0';
        std::wstring name2(name);
		if (!std::regex_search(name2, what,
								 ep.m_fd->m_windowTitleName))
			return TRUE;				// match failed
	}

	ep.m_hwnd = i_hwnd;
	return FALSE;
}


/// SetForegroundWindow
void Engine::funcSetForegroundWindow(FunctionParam *i_param, const wregex_stored &,
									 LogicalOperatorType , const wregex_stored &)
{
	if (!i_param->m_isPressed)
		return;
	EnumWindowsForSetForegroundWindowParam
	ep(static_cast<const FunctionData_SetForegroundWindow *>(
		   i_param->m_af->m_functionData.get()));
	EnumWindows(enumWindowsForSetForegroundWindow,
				reinterpret_cast<LPARAM>(&ep));
	if (ep.m_hwnd)
		PostMessage(m_hwndAssocWindow,
					WM_APP_engineNotify, EngineNotify_setForegroundWindow,
					reinterpret_cast<LPARAM>(ep.m_hwnd));

}


// load setting
void Engine::funcLoadSetting(FunctionParam *i_param, const StrExprArg &i_name)
{
	if (!i_param->m_isPressed)
		return;
	if (!i_name.eval().empty()) {
		// set MAYU_REGISTRY_ROOT\.mayuIndex which name is same with i_name
		Registry reg(MAYU_REGISTRY_ROOT);

		wregex_stored split(L"^([^;]*);([^;]*);(.*)$");
		wstringi dot_mayu;
		for (size_t i = 0; i < MAX_MAYU_REGISTRY_ENTRIES; ++ i) {
			wchar_t buf[100];
			_snwprintf(buf, NUMBER_OF(buf), L".mayu%d", (int)i);
			if (!reg.read(buf, &dot_mayu))
				break;

			std::wsmatch what;
			if (std::regex_match(dot_mayu, what, split) &&
					what.str(1) == i_name.eval()) {
				reg.write(L".mayuIndex", (int)i);
				goto success;
			}
		}

		{
			Acquire a(&m_log, 0);
			m_log << L"unknown setting name: " << i_name << std::endl;
		}
		return;

success:
		;
	}
	PostMessage(m_hwndAssocWindow,
				WM_APP_engineNotify, EngineNotify_loadSetting, 0);
}

// virtual key
void Engine::funcVK(FunctionParam *i_param, VKey i_vkey)
{
	long key = static_cast<long>(i_vkey);
	BYTE vkey = static_cast<BYTE>(i_vkey);
	bool isExtended = !!(key & VKey_extended);
	bool isUp       = !i_param->m_isPressed && !!(key & VKey_released);
	bool isDown     = i_param->m_isPressed && !!(key & VKey_pressed);

	if (vkey == VK_LBUTTON && isDown)
		mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
	else if (vkey == VK_LBUTTON && isUp)
		mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
	else if (vkey == VK_MBUTTON && isDown)
		mouse_event(MOUSEEVENTF_MIDDLEDOWN, 0, 0, 0, 0);
	else if (vkey == VK_MBUTTON && isUp)
		mouse_event(MOUSEEVENTF_MIDDLEUP, 0, 0, 0, 0);
	else if (vkey == VK_RBUTTON && isDown)
		mouse_event(MOUSEEVENTF_RIGHTDOWN, 0, 0, 0, 0);
	else if (vkey == VK_RBUTTON && isUp)
		mouse_event(MOUSEEVENTF_RIGHTUP, 0, 0, 0, 0);
	else if (vkey == VK_XBUTTON1 && isDown)
		mouse_event(MOUSEEVENTF_XDOWN, 0, 0, XBUTTON1, 0);
	else if (vkey == VK_XBUTTON1 && isUp)
		mouse_event(MOUSEEVENTF_XUP, 0, 0, XBUTTON1, 0);
	else if (vkey == VK_XBUTTON2 && isDown)
		mouse_event(MOUSEEVENTF_XDOWN, 0, 0, XBUTTON2, 0);
	else if (vkey == VK_XBUTTON2 && isUp)
		mouse_event(MOUSEEVENTF_XUP, 0, 0, XBUTTON2, 0);
	else if (isUp || isDown)
		keybd_event(vkey,
					static_cast<BYTE>(MapVirtualKey(vkey, 0)),
					(isExtended ? KEYEVENTF_EXTENDEDKEY : 0) |
					(i_param->m_isPressed ? 0 : KEYEVENTF_KEYUP), 0);
}

// wait
void Engine::funcWait(FunctionParam *i_param, int i_milliSecond)
{
	if (!i_param->m_isPressed)
		return;
	if (i_milliSecond < 0 || 5000 < i_milliSecond)	// too long wait
		return;

	m_isSynchronizing = true;
	m_mutex.unlock();
	Sleep(i_milliSecond);
	m_mutex.lock();
	m_isSynchronizing = false;
}

// investigate WM_COMMAND, WM_SYSCOMMAND
void Engine::funcInvestigateCommand(FunctionParam *i_param)
{
	if (!i_param->m_isPressed)
		return;
	Acquire a(&m_log, 0);
	g_hookData->m_doesNotifyCommand = !g_hookData->m_doesNotifyCommand;
	if (g_hookData->m_doesNotifyCommand)
		m_log << L" begin" << std::endl;
	else
		m_log << L" end" << std::endl;
}

// show mayu dialog box
void Engine::funcMayuDialog(FunctionParam *i_param, MayuDialogType i_dialog,
							ShowCommandType i_showCommand)
{
	if (!i_param->m_isPressed)
		return;
	PostMessage(getAssociatedWndow(), WM_APP_engineNotify, EngineNotify_showDlg,
				static_cast<LPARAM>(i_dialog) |
				static_cast<LPARAM>(i_showCommand));
}

// describe bindings
void Engine::funcDescribeBindings(FunctionParam *i_param)
{
	if (!i_param->m_isPressed)
		return;
	{
		Acquire a(&m_log, 1);
		m_log << std::endl;
	}
	describeBindings();
}

// show help message
void Engine::funcHelpMessage(FunctionParam *i_param, const StrExprArg &i_title,
							 const StrExprArg &i_message)
{
	if (!i_param->m_isPressed)
		return;

	m_helpTitle = i_title.eval();
	m_helpMessage = i_message.eval();
	bool doesShow = !(i_title.eval().size() == 0 && i_message.eval().size() == 0);
	PostMessage(getAssociatedWndow(), WM_APP_engineNotify,
				EngineNotify_helpMessage, doesShow);
}

// show variable
void Engine::funcHelpVariable(FunctionParam *i_param, const StrExprArg &i_title)
{
	if (!i_param->m_isPressed)
		return;

	wchar_t buf[20];
	_snwprintf(buf, NUMBER_OF(buf), L"%d", m_variable);

	m_helpTitle = i_title.eval();
	m_helpMessage = buf;
	PostMessage(getAssociatedWndow(), WM_APP_engineNotify,
				EngineNotify_helpMessage, true);
}

// raise window
void Engine::funcWindowRaise(FunctionParam *i_param,
							 TargetWindowType i_twt)
{
	HWND hwnd;
	if (!getSuitableMdiWindow(i_param, &hwnd, &i_twt))
		return;
	SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0,
				 SWP_ASYNCWINDOWPOS | SWP_NOMOVE | SWP_NOACTIVATE | SWP_NOSIZE);
}

// lower window
void Engine::funcWindowLower(FunctionParam *i_param, TargetWindowType i_twt)
{
	HWND hwnd;
	if (!getSuitableMdiWindow(i_param, &hwnd, &i_twt))
		return;
	SetWindowPos(hwnd, HWND_BOTTOM, 0, 0, 0, 0,
				 SWP_ASYNCWINDOWPOS | SWP_NOMOVE | SWP_NOACTIVATE | SWP_NOSIZE);
}

// minimize window
void Engine::funcWindowMinimize(FunctionParam *i_param, TargetWindowType i_twt)
{
	HWND hwnd;
	if (!getSuitableMdiWindow(i_param, &hwnd, &i_twt))
		return;
	PostMessage(hwnd, WM_SYSCOMMAND,
				IsIconic(hwnd) ? SC_RESTORE : SC_MINIMIZE, 0);
}

// maximize window
void Engine::funcWindowMaximize(FunctionParam *i_param, TargetWindowType i_twt)
{
	HWND hwnd;
	if (!getSuitableMdiWindow(i_param, &hwnd, &i_twt))
		return;
	PostMessage(hwnd, WM_SYSCOMMAND,
				IsZoomed(hwnd) ? SC_RESTORE : SC_MAXIMIZE, 0);
}

// maximize horizontally or virtically
void Engine::funcWindowHVMaximize(FunctionParam *i_param,
								  BooleanType i_isHorizontal,
								  TargetWindowType i_twt)
{
	HWND hwnd;
	RECT rc, rcd;
	if (!getSuitableMdiWindow(i_param, &hwnd, &i_twt, &rc, &rcd))
		return;

	// erase non window
	while (true) {
		WindowPositions::iterator i = m_windowPositions.begin();
		WindowPositions::iterator end = m_windowPositions.end();
		for (; i != end; ++ i)
			if (!IsWindow((*i).m_hwnd))
				break;
		if (i == end)
			break;
		m_windowPositions.erase(i);
	}

	// find target
	WindowPositions::iterator i = m_windowPositions.begin();
	WindowPositions::iterator end = m_windowPositions.end();
	WindowPositions::iterator target = end;
	for (; i != end; ++ i)
		if ((*i).m_hwnd == hwnd) {
			target = i;
			break;
		}

	if (IsZoomed(hwnd))
		PostMessage(hwnd, WM_SYSCOMMAND, SC_RESTORE, 0);
	else {
		WindowPosition::Mode mode = WindowPosition::Mode_normal;

		if (target != end) {
			WindowPosition &wp = *target;
			rc = wp.m_rc;
			if (wp.m_mode == WindowPosition::Mode_HV)
				mode = wp.m_mode =
						   i_isHorizontal ? WindowPosition::Mode_V : WindowPosition::Mode_H;
			else if (( i_isHorizontal && wp.m_mode == WindowPosition::Mode_V) ||
					 (!i_isHorizontal && wp.m_mode == WindowPosition::Mode_H))
				mode = wp.m_mode = WindowPosition::Mode_HV;
			else
				m_windowPositions.erase(target);
		} else {
			mode = i_isHorizontal ? WindowPosition::Mode_H : WindowPosition::Mode_V;
			m_windowPositions.push_front(WindowPosition(hwnd, rc, mode));
		}

		if (static_cast<int>(mode) & static_cast<int>(WindowPosition::Mode_H))
			rc.left = rcd.left, rc.right = rcd.right;
		if (static_cast<int>(mode) & static_cast<int>(WindowPosition::Mode_V))
			rc.top = rcd.top, rc.bottom = rcd.bottom;

		asyncMoveWindow(hwnd, rc.left, rc.top, rcWidth(&rc), rcHeight(&rc));
	}
}

// maximize window horizontally
void Engine::funcWindowHMaximize(FunctionParam *i_param,
								 TargetWindowType i_twt)
{
	funcWindowHVMaximize(i_param, BooleanType_true, i_twt);
}

// maximize window virtically
void Engine::funcWindowVMaximize(FunctionParam *i_param,
								 TargetWindowType i_twt)
{
	funcWindowHVMaximize(i_param, BooleanType_false, i_twt);
}

// move window
void Engine::funcWindowMove(FunctionParam *i_param, int i_dx, int i_dy,
							TargetWindowType i_twt)
{
	funcWindowMoveTo(i_param, GravityType_C, i_dx, i_dy, i_twt);
}

// move window to ...
void Engine::funcWindowMoveTo(FunctionParam *i_param,
							  GravityType i_gravityType,
							  int i_dx, int i_dy, TargetWindowType i_twt)
{
	HWND hwnd;
	RECT rc, rcd;
	if (!getSuitableMdiWindow(i_param, &hwnd, &i_twt, &rc, &rcd))
		return;

	int x = rc.left + i_dx;
	int y = rc.top + i_dy;

	if (i_gravityType & GravityType_N)
		y = i_dy + rcd.top;
	if (i_gravityType & GravityType_E)
		x = i_dx + rcd.right - rcWidth(&rc);
	if (i_gravityType & GravityType_W)
		x = i_dx + rcd.left;
	if (i_gravityType & GravityType_S)
		y = i_dy + rcd.bottom - rcHeight(&rc);
	asyncMoveWindow(hwnd, x, y);
}


// move window visibly
void Engine::funcWindowMoveVisibly(FunctionParam *i_param,
								   TargetWindowType i_twt)
{
	HWND hwnd;
	RECT rc, rcd;
	if (!getSuitableMdiWindow(i_param, &hwnd, &i_twt, &rc, &rcd))
		return;

	int x = rc.left;
	int y = rc.top;
	if (rc.left < rcd.left)
		x = rcd.left;
	else if (rcd.right < rc.right)
		x = rcd.right - rcWidth(&rc);
	if (rc.top < rcd.top)
		y = rcd.top;
	else if (rcd.bottom < rc.bottom)
		y = rcd.bottom - rcHeight(&rc);
	asyncMoveWindow(hwnd, x, y);
}


struct EnumDisplayMonitorsForWindowMonitorToParam {
	std::vector<HMONITOR> m_monitors;
	std::vector<MONITORINFO> m_monitorinfos;
	int m_primaryMonitorIdx;
	int m_currentMonitorIdx;

	HMONITOR m_hmon;

public:
	EnumDisplayMonitorsForWindowMonitorToParam(HMONITOR i_hmon)
			: m_hmon(i_hmon),
			m_primaryMonitorIdx(-1), m_currentMonitorIdx(-1) {
	}
};

static BOOL CALLBACK enumDisplayMonitorsForWindowMonitorTo(
	HMONITOR i_hmon, HDC i_hdc, LPRECT i_rcMonitor, LPARAM i_data)
{
	EnumDisplayMonitorsForWindowMonitorToParam &ep =
		*reinterpret_cast<EnumDisplayMonitorsForWindowMonitorToParam *>(i_data);

	ep.m_monitors.push_back(i_hmon);

	MONITORINFO mi;
	mi.cbSize = sizeof(mi);
	getMonitorInfo(i_hmon, &mi);
	ep.m_monitorinfos.push_back(mi);

	if (mi.dwFlags & MONITORINFOF_PRIMARY)
		ep.m_primaryMonitorIdx = (int)ep.m_monitors.size() - 1;
	if (i_hmon == ep.m_hmon)
		ep.m_currentMonitorIdx = (int)ep.m_monitors.size() - 1;

	return TRUE;
}

/// move window to other monitor
void Engine::funcWindowMonitorTo(
	FunctionParam *i_param, WindowMonitorFromType i_fromType, int i_monitor,
	BooleanType i_adjustPos, BooleanType i_adjustSize)
{
	HWND hwnd;
	if (! getSuitableWindow(i_param, &hwnd))
		return;

	HMONITOR hmonCur;
	hmonCur = monitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);

	EnumDisplayMonitorsForWindowMonitorToParam ep(hmonCur);
	enumDisplayMonitors(NULL, NULL, enumDisplayMonitorsForWindowMonitorTo,
						reinterpret_cast<LPARAM>(&ep));
	if (ep.m_monitors.size() < 1 ||
			ep.m_primaryMonitorIdx < 0 || ep.m_currentMonitorIdx < 0)
		return;

	int targetIdx;
	switch (i_fromType) {
	case WindowMonitorFromType_primary:
		targetIdx = (ep.m_primaryMonitorIdx + i_monitor) % ep.m_monitors.size();
		break;

	case WindowMonitorFromType_current:
		targetIdx = (ep.m_currentMonitorIdx + i_monitor) % ep.m_monitors.size();
		break;
	}
	if (ep.m_currentMonitorIdx == targetIdx)
		return;

	RECT rcCur, rcTarget, rcWin;
	rcCur = ep.m_monitorinfos[ep.m_currentMonitorIdx].rcWork;
	rcTarget = ep.m_monitorinfos[targetIdx].rcWork;
	GetWindowRect(hwnd, &rcWin);

	int x = rcTarget.left + (rcWin.left - rcCur.left);
	int y = rcTarget.top + (rcWin.top - rcCur.top);
	int w = rcWidth(&rcWin);
	int h = rcHeight(&rcWin);

	if (i_adjustPos) {
		if (x + w > rcTarget.right)
			x = rcTarget.right - w;
		if (x < rcTarget.left)
			x = rcTarget.left;
		if (w > rcWidth(&rcTarget)) {
			x = rcTarget.left;
			w = rcWidth(&rcTarget);
		}

		if (y + h > rcTarget.bottom)
			y = rcTarget.bottom - h;
		if (y < rcTarget.top)
			y = rcTarget.top;
		if (h > rcHeight(&rcTarget)) {
			y = rcTarget.top;
			h = rcHeight(&rcTarget);
		}
	}

	if (i_adjustPos && i_adjustSize) {
		if (IsZoomed(hwnd))
			PostMessage(hwnd, WM_SYSCOMMAND, SC_RESTORE, 0);
		asyncMoveWindow(hwnd, x, y, w, h);
	} else {
		asyncMoveWindow(hwnd, x, y);
	}
}

/// move window to other monitor
void Engine::funcWindowMonitor(
	FunctionParam *i_param, int i_monitor,
	BooleanType i_adjustPos, BooleanType i_adjustSize)
{
	funcWindowMonitorTo(i_param, WindowMonitorFromType_primary, i_monitor,
						i_adjustPos, i_adjustSize);
}


//
void Engine::funcWindowClingToLeft(FunctionParam *i_param,
								   TargetWindowType i_twt)
{
	funcWindowMoveTo(i_param, GravityType_W, 0, 0, i_twt);
}

//
void Engine::funcWindowClingToRight(FunctionParam *i_param,
									TargetWindowType i_twt)
{
	funcWindowMoveTo(i_param, GravityType_E, 0, 0, i_twt);
}

//
void Engine::funcWindowClingToTop(FunctionParam *i_param,
								  TargetWindowType i_twt)
{
	funcWindowMoveTo(i_param, GravityType_N, 0, 0, i_twt);
}

//
void Engine::funcWindowClingToBottom(FunctionParam *i_param,
									 TargetWindowType i_twt)
{
	funcWindowMoveTo(i_param, GravityType_S, 0, 0, i_twt);
}

// close window
void Engine::funcWindowClose(FunctionParam *i_param, TargetWindowType i_twt)
{
	HWND hwnd;
	if (!getSuitableMdiWindow(i_param, &hwnd, &i_twt))
		return;
	PostMessage(hwnd, WM_SYSCOMMAND, SC_CLOSE, 0);
}

// toggle top-most flag of the window
void Engine::funcWindowToggleTopMost(FunctionParam *i_param)
{
	HWND hwnd;
	if (!getSuitableWindow(i_param, &hwnd))
		return;
	SetWindowPos(
		hwnd,
		(GetWindowLongPtr(hwnd, GWL_EXSTYLE) & WS_EX_TOPMOST) ?
		HWND_NOTOPMOST : HWND_TOPMOST,
		0, 0, 0, 0,
		SWP_ASYNCWINDOWPOS | SWP_NOMOVE | SWP_NOACTIVATE | SWP_NOSIZE);
}

// identify the window
void Engine::funcWindowIdentify(FunctionParam *i_param)
{
	if (!i_param->m_isPressed)
		return;

	wchar_t className[GANA_MAX_ATOM_LENGTH];
	bool ok = false;
	if (GetClassName(i_param->m_hwnd, className, NUMBER_OF(className))) {
		if (_wcsicmp(className, L"ConsoleWindowClass") == 0) {
			wchar_t titleName[1024];
			if (GetWindowText(i_param->m_hwnd, titleName, NUMBER_OF(titleName)) == 0)
				titleName[0] = L'\0';
			{
				Acquire a(&m_log, 1);
				m_log << L"HWND:\t" << std::hex
				<< static_cast<DWORD>(reinterpret_cast<uintptr_t>(i_param->m_hwnd))
				<< std::dec << std::endl;
			}
			Acquire a(&m_log, 0);
			m_log << L"CLASS:\t" << className << std::endl;
			m_log << L"TITLE:\t" << titleName << std::endl;

			HWND hwnd = getToplevelWindow(i_param->m_hwnd, NULL);
			RECT rc;
			GetWindowRect(hwnd, &rc);
			m_log << L"Toplevel Window Position/Size: ("
			<< rc.left << L", " << rc.top << L") / ("
			<< rcWidth(&rc) << L"x" << rcHeight(&rc)
			<< L")" << std::endl;

			SystemParametersInfo(SPI_GETWORKAREA, 0, (void *)&rc, FALSE);
			m_log << L"Desktop Window Position/Size: ("
			<< rc.left << L", " << rc.top << L") / ("
			<< rcWidth(&rc) << L"x" << rcHeight(&rc)
			<< L")" << std::endl;

			m_log << std::endl;
			ok = true;
		}
	}
	if (!ok) {
		UINT WM_MAYU_MESSAGE = RegisterWindowMessage(
								   addSessionId(WM_MAYU_MESSAGE_NAME).c_str());
		CHECK_TRUE( PostMessage(i_param->m_hwnd, WM_MAYU_MESSAGE,
								MayuMessage_notifyName, 0) );
	}
}

// set alpha blending parameter to the window
void Engine::funcWindowSetAlpha(FunctionParam *i_param, int i_alpha)
{
	HWND hwnd;
	if (!getSuitableWindow(i_param, &hwnd))
		return;

	if (i_alpha < 0) {	// remove all alpha
		for (WindowsWithAlpha::iterator i = m_windowsWithAlpha.begin();
				i != m_windowsWithAlpha.end(); ++ i) {
			SetWindowLongPtr(*i, GWL_EXSTYLE,
							 GetWindowLongPtr(*i, GWL_EXSTYLE) & ~WS_EX_LAYERED);
			RedrawWindow(*i, NULL, NULL,
						 RDW_ERASE | RDW_INVALIDATE | RDW_FRAME | RDW_ALLCHILDREN);
		}
		m_windowsWithAlpha.clear();
	} else {
		LONG_PTR exStyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
		if (exStyle & WS_EX_LAYERED) {	// remove alpha
			WindowsWithAlpha::iterator
			i = std::find(m_windowsWithAlpha.begin(), m_windowsWithAlpha.end(),
						  hwnd);
			if (i == m_windowsWithAlpha.end())
				return;	// already layered by the application

			m_windowsWithAlpha.erase(i);

			SetWindowLongPtr(hwnd, GWL_EXSTYLE, exStyle & ~WS_EX_LAYERED);
		} else {	// add alpha
			SetWindowLongPtr(hwnd, GWL_EXSTYLE, exStyle | WS_EX_LAYERED);
			i_alpha %= 101;
			if (!setLayeredWindowAttributes(hwnd, 0,
											(BYTE)(255 * i_alpha / 100), LWA_ALPHA)) {
				Acquire a(&m_log, 0);
				m_log << L"error: &WindowSetAlpha(" << i_alpha
				<< L") failed for HWND: " << std::hex
				<< hwnd << std::dec << std::endl;
				return;
			}
			m_windowsWithAlpha.push_front(hwnd);
		}
		RedrawWindow(hwnd, NULL, NULL,
					 RDW_ERASE | RDW_INVALIDATE | RDW_FRAME | RDW_ALLCHILDREN);
	}
}


// redraw the window
void Engine::funcWindowRedraw(FunctionParam *i_param)
{
	HWND hwnd;
	if (!getSuitableWindow(i_param, &hwnd))
		return;
	RedrawWindow(hwnd, NULL, NULL,
				 RDW_ERASE | RDW_INVALIDATE | RDW_FRAME | RDW_ALLCHILDREN);
}

// resize window to
void Engine::funcWindowResizeTo(FunctionParam *i_param, int i_width,
								int i_height, TargetWindowType i_twt)
{
	HWND hwnd;
	RECT rc, rcd;
	if (!getSuitableMdiWindow(i_param, &hwnd, &i_twt, &rc, &rcd))
		return;

	if (i_width == 0)
		i_width = rcWidth(&rc);
	else if (i_width < 0)
		i_width += rcWidth(&rcd);

	if (i_height == 0)
		i_height = rcHeight(&rc);
	else if (i_height < 0)
		i_height += rcHeight(&rcd);

	asyncResize(hwnd, i_width, i_height);
}

// move the mouse cursor
void Engine::funcMouseMove(FunctionParam *i_param, int i_dx, int i_dy)
{
	if (!i_param->m_isPressed)
		return;
	POINT pt;
	GetCursorPos(&pt);
	SetCursorPos(pt.x + i_dx, pt.y + i_dy);
}

// send a mouse-wheel-message to Windows
void Engine::funcMouseWheel(FunctionParam *i_param, int i_delta)
{
	if (!i_param->m_isPressed)
		return;
	mouse_event(MOUSEEVENTF_WHEEL, 0, 0, i_delta, 0);
}

// convert the contents of the Clipboard to upper case
void Engine::funcClipboardChangeCase(FunctionParam *i_param,
									 BooleanType i_doesConvertToUpperCase)
{
	if (!i_param->m_isPressed)
		return;

	HGLOBAL hdata;
	const wchar_t *text = getTextFromClipboard(&hdata);
	HGLOBAL hdataNew = NULL;
	if (text) {
		int size = static_cast<int>(GlobalSize(hdata));
		hdataNew = GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE, size);
		if (hdataNew) {
			if (wchar_t *dataNew = reinterpret_cast<wchar_t *>(GlobalLock(hdataNew))) {
				std::memcpy(dataNew, text, size);
				wchar_t *dataEnd = dataNew + size;
				while (dataNew < dataEnd && *dataNew) {
					wchar_t c = *dataNew;
					*dataNew++ = i_doesConvertToUpperCase ? towupper(c) : towlower(c);
				}
				GlobalUnlock(hdataNew);
			}
		}
	}
	closeClipboard(hdata, hdataNew);
}

// convert the contents of the Clipboard to upper case
void Engine::funcClipboardUpcaseWord(FunctionParam *i_param)
{
	funcClipboardChangeCase(i_param, BooleanType_true);
}

// convert the contents of the Clipboard to lower case
void Engine::funcClipboardDowncaseWord(FunctionParam *i_param)
{
	funcClipboardChangeCase(i_param, BooleanType_false);
}

// set the contents of the Clipboard to the string
void Engine::funcClipboardCopy(FunctionParam *i_param, const StrExprArg &i_text)
{
	if (!i_param->m_isPressed)
		return;
	if (!OpenClipboard(NULL))
		return;

	HGLOBAL hdataNew =
		GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE,
					(i_text.eval().size() + 1) * sizeof(wchar_t));
	if (!hdataNew)
		return;
	wchar_t *dataNew = reinterpret_cast<wchar_t *>(GlobalLock(hdataNew));
	wcscpy(dataNew, i_text.eval().c_str());
	GlobalUnlock(hdataNew);
	closeClipboard(NULL, hdataNew);
}

//
void Engine::funcEmacsEditKillLinePred(
	FunctionParam *i_param, const KeySeq *i_keySeq1, const KeySeq *i_keySeq2)
{
	m_emacsEditKillLine.m_doForceReset = false;
	if (!i_param->m_isPressed)
		return;

	int r = m_emacsEditKillLine.pred();
	const KeySeq *keySeq;
	if (r == 1)
		keySeq = i_keySeq1;
	else if (r == 2)
		keySeq = i_keySeq2;
	else // r == 0
		return;
	ASSERT(keySeq);
	generateKeySeqEvents(i_param->m_c, keySeq, Part_all);
}

//
void Engine::funcEmacsEditKillLineFunc(FunctionParam *i_param)
{
	if (!i_param->m_isPressed)
		return;
	m_emacsEditKillLine.func();
	m_emacsEditKillLine.m_doForceReset = false;
}

// clear log
void Engine::funcLogClear(FunctionParam *i_param)
{
	if (!i_param->m_isPressed)
		return;
	PostMessage(getAssociatedWndow(), WM_APP_engineNotify,
				EngineNotify_clearLog, 0);
}

// recenter
void Engine::funcRecenter(FunctionParam *i_param)
{
	if (!i_param->m_isPressed)
		return;
	if (m_hwndFocus) {
		UINT WM_MAYU_MESSAGE = RegisterWindowMessage(
								   addSessionId(WM_MAYU_MESSAGE_NAME).c_str());
		PostMessage(m_hwndFocus, WM_MAYU_MESSAGE, MayuMessage_funcRecenter, 0);
	}
}

// set IME open status
void Engine::funcSetImeStatus(FunctionParam *i_param, ToggleType i_toggle)
{
	if (!i_param->m_isPressed)
		return;
	if (m_hwndFocus) {
		UINT WM_MAYU_MESSAGE = RegisterWindowMessage(
								   addSessionId(WM_MAYU_MESSAGE_NAME).c_str());
		int status = -1;
		switch (i_toggle) {
		case ToggleType_toggle:
			status = -1;
			break;
		case ToggleType_off:
			status = 0;
			break;
		case ToggleType_on:
			status = 1;
			break;
		}
		PostMessage(m_hwndFocus, WM_MAYU_MESSAGE, MayuMessage_funcSetImeStatus, status);
	}
}

// set IME open status
void Engine::funcSetImeString(FunctionParam *i_param, const StrExprArg &i_data)
{
	if (!i_param->m_isPressed)
		return;
	if (m_hwndFocus) {
		UINT WM_MAYU_MESSAGE = RegisterWindowMessage(
								   addSessionId(WM_MAYU_MESSAGE_NAME).c_str());
		PostMessage(m_hwndFocus, WM_MAYU_MESSAGE, MayuMessage_funcSetImeString, i_data.eval().size() * sizeof(wchar_t));

		DWORD len = 0;
		DWORD error;
		DisconnectNamedPipe(m_hookPipe);
		ConnectNamedPipe(m_hookPipe, NULL);
		error = WriteFile(m_hookPipe, i_data.eval().c_str(),
						  (DWORD)(i_data.eval().size() * sizeof(wchar_t)),
						  &len, NULL);

		//FlushFileBuffers(m_hookPipe);
	}
}

// Direct SSTP Server
class DirectSSTPServer
{
public:
	std::wstring m_path;
	HWND m_hwnd;
	std::wstring m_name;
	std::wstring m_keroname;

public:
	DirectSSTPServer()
			: m_hwnd(NULL) {
	}
};


class ParseDirectSSTPData
{
	using MR = std::cmatch;

public:
	using DirectSSTPServers = std::map<std::wstring, DirectSSTPServer>;

private:
	DirectSSTPServers *m_directSSTPServers;

public:
	// constructor
	ParseDirectSSTPData(DirectSSTPServers *i_directSSTPServers)
			: m_directSSTPServers(i_directSSTPServers) {
	}

	bool operator()(const MR& i_what) {
		std::wstring id(to_wstring(std::string(i_what[1].first, i_what[1].second)));
		std::wstring member(to_wstring(std::string(i_what[2].first, i_what[2].second)));
		std::wstring value(to_wstring(std::string(i_what[3].first, i_what[3].second)));

		if (member == L"path")
			(*m_directSSTPServers)[id].m_path = value;
		else if (member == L"hwnd")
			(*m_directSSTPServers)[id].m_hwnd =
				reinterpret_cast<HWND>(static_cast<uintptr_t>(static_cast<unsigned>(_wtoi(value.c_str()))));
		else if (member == L"name")
			(*m_directSSTPServers)[id].m_name = value;
		else if (member == L"keroname")
			(*m_directSSTPServers)[id].m_keroname = value;
		return true;
	}
};

// Direct SSTP
void Engine::funcDirectSSTP(FunctionParam *i_param,
							const wregex_stored &i_name,
							const StrExprArg &i_protocol,
							const std::list<wstringq> &i_headers)
{
	if (!i_param->m_isPressed)
		return;

	// check Direct SSTP server exist ?
	if (HANDLE hm = OpenMutex(MUTEX_ALL_ACCESS, FALSE, L"sakura"))
		CloseHandle(hm);
	else {
		Acquire a(&m_log, 0);
		m_log << L" Error(1): Direct SSTP server does not exist." << std::endl;
		return;
	}

	HANDLE hfm = OpenFileMapping(FILE_MAP_READ, FALSE, L"Sakura");
	if (!hfm) {
		Acquire a(&m_log, 0);
		m_log << L" Error(2): Direct SSTP server does not provide data." << std::endl;
		return;
	}

	char *data =
		reinterpret_cast<char *>(MapViewOfFile(hfm, FILE_MAP_READ, 0, 0, 0));
	if (!data) {
		CloseHandle(hfm);
		Acquire a(&m_log, 0);
		m_log << L" Error(3): Direct SSTP server does not provide data." << std::endl;
		return;
	}

	long length = *(long *)data;
	const char *begin = data + 4;
	const char *end = data + length;
	std::regex getSakura("([0-9a-fA-F]{32})\\.([^\x01]+)\x01(.*?)\r\n");

	ParseDirectSSTPData::DirectSSTPServers servers;
	std::cregex_iterator
	it(begin, end, getSakura), last;
	for (; it != last; ++it)
		((ParseDirectSSTPData)(&servers))(*it);

	// make request
	std::wstring request;
	if (!i_protocol.eval().size())
		request += L"NOTIFY SSTP/1.1";
	else
		request += i_protocol.eval();
	request += L"\r\n";

	bool hasSender = false;
	for (std::list<wstringq>::const_iterator
			i = i_headers.begin(); i != i_headers.end(); ++ i) {
		if (_wcsnicmp(L"Charset", i->c_str(), 7) == 0 ||
				_wcsnicmp(L"Hwnd",    i->c_str(), 4) == 0)
			continue;
		if (_wcsnicmp(L"Sender", i->c_str(), 6) == 0)
			hasSender = true;
		request += i->c_str();
		request += L"\r\n";
	}

	if (!hasSender) {
		request += L"Sender: ";
		request += loadString(IDS_mayu);
		request += L"\r\n";
	}

	wchar_t buf[100];
	_snwprintf(buf, NUMBER_OF(buf), L"HWnd: %d\r\n",
			   (int) reinterpret_cast<uintptr_t>(m_hwndAssocWindow));
	request += buf;

	request += L"Charset: UTF-8\r\n";
	request += L"\r\n";

	std::string request_UTF_8 = to_UTF_8(request);

	// send request to Direct SSTP Server which matches i_name;
	for (ParseDirectSSTPData::DirectSSTPServers::iterator
			i = servers.begin(); i != servers.end(); ++ i) {
		std::wsmatch what;
		if (std::regex_match(i->second.m_name, what, i_name)) {
			COPYDATASTRUCT cd;
			cd.dwData = 9801;
			cd.cbData = (DWORD)request_UTF_8.size();
			cd.lpData = (void *)request_UTF_8.c_str();
			DWORD_PTR result;
			SendMessageTimeout(i->second.m_hwnd, WM_COPYDATA,
							   reinterpret_cast<WPARAM>(m_hwndAssocWindow),
							   reinterpret_cast<LPARAM>(&cd),
							   SMTO_ABORTIFHUNG | SMTO_BLOCK, 5000, &result);
		}
	}

	UnmapViewOfFile(data);
	CloseHandle(hfm);
}


namespace shu
{
class PlugIn
{
	enum Type {
		Type_A,
		Type_W
	};

private:
	HMODULE m_dll;
	FARPROC m_func;
	Type m_type;
	wstringq m_funcParam;

public:
	PlugIn() : m_dll(NULL) {
	}

	~PlugIn() {
		FreeLibrary(m_dll);
	}

	bool load(const wstringq &i_dllName, const wstringq &i_funcName,
			  const wstringq &i_funcParam, womsgstream &i_log) {
		m_dll = LoadLibrary((L"Plugins\\" + i_dllName).c_str());
		if (!m_dll) {
			m_dll = LoadLibrary((L"Plugin\\" + i_dllName).c_str());
			if (!m_dll) {
				m_dll = LoadLibrary(i_dllName.c_str());
				if (!m_dll) {
					Acquire a(&i_log);
					i_log << std::endl;
					i_log << L"error: &PlugIn() failed to load " << i_dllName << std::endl;
					return false;
				}
			}
		}

		// get function
#define to_wstring
		m_type = Type_W;
		m_func = GetProcAddress(m_dll, to_string(L"mayu" + i_funcName + L"W").c_str());
		if (!m_func) {
			m_type = Type_A;
			m_func
			= GetProcAddress(m_dll, to_string(L"mayu" + i_funcName + L"A").c_str());
			if (!m_func) {
				m_func = GetProcAddress(m_dll, to_string(L"mayu" + i_funcName).c_str());
				if (!m_func) {
					m_func = GetProcAddress(m_dll, to_string(i_funcName).c_str());
					if (!m_func) {
						Acquire a(&i_log);
						i_log << std::endl;
						i_log << L"error: &PlugIn() failed to find function: "
						<< i_funcName << std::endl;
						return false;
					}
				}
			}
		}

		m_funcParam = i_funcParam;
		return true;
	}

	void exec() {
		ASSERT( m_dll );
		ASSERT( m_func );

		using PLUGIN_FUNCTION_A = void (WINAPI *)(const char *i_arg);
		using PLUGIN_FUNCTION_W = void (WINAPI *)(const wchar_t *i_arg);
		switch (m_type) {
		case Type_A:
			reinterpret_cast<PLUGIN_FUNCTION_A>(m_func)(to_string(m_funcParam).c_str());
			break;
		case Type_W:
			reinterpret_cast<PLUGIN_FUNCTION_W>(m_func)(to_wstring(m_funcParam).c_str());
			break;
		}
	}
#undef to_string
#undef to_wstring
};

static void plugInThread(void *i_plugin)
{
	PlugIn *plugin = static_cast<PlugIn *>(i_plugin);
	plugin->exec();
	delete plugin;
}
}

void Engine::funcPlugIn(FunctionParam *i_param,
						const StrExprArg &i_dllName,
						const StrExprArg &i_funcName,
						const StrExprArg &i_funcParam,
						BooleanType i_doesCreateThread)
{
	if (!i_param->m_isPressed)
		return;

	auto plugin = std::make_unique<shu::PlugIn>();
	if (!plugin->load(i_dllName.eval(), i_funcName.eval(), i_funcParam.eval(), m_log)) {
		return;
	}
	if (i_doesCreateThread) {
		auto rawPlugin = plugin.release();
		if (_beginthread(shu::plugInThread, 0, rawPlugin) == -1) {
			delete rawPlugin;
			Acquire a(&m_log);
			m_log << std::endl;
			m_log << L"error: &PlugIn() failed to create thread." << std::endl;
		}
		return;
	} else
		plugin->exec();
}


void Engine::funcMouseHook(FunctionParam *i_param,
						   MouseHookType i_hookType, int i_hookParam)
{
	GetCursorPos(&g_hookData->m_mousePos);
	g_hookData->m_mouseHookType = i_hookType;
	g_hookData->m_mouseHookParam = i_hookParam;

	switch (i_hookType) {
	case MouseHookType_WindowMove: {
		// For this type, g_hookData->m_mouseHookParam means
		// target window type to move.
		HWND target;
		bool isMDI;

		// i_hooParam < 0 means target window to move is MDI.
		if (i_hookParam < 0)
			isMDI = true;
		else
			isMDI = false;

		// abs(i_hookParam) == 2: target is window under mouse cursor
		// otherwise: target is current focus window
		if (i_hookParam == 2 || i_hookParam == -2)
			target = WindowFromPoint(g_hookData->m_mousePos);
		else
			target = i_param->m_hwnd;

		g_hookData->setHwndMouseHookTarget(getToplevelWindow(target, &isMDI));
		break;
		default:
			g_hookData->setHwndMouseHookTarget(NULL);
			break;
		}
	}
	return;
}


// cancel prefix
void Engine::funcCancelPrefix(FunctionParam *i_param)
{
	m_isPrefix = false;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// StrExpr
class StrExpr
{
private:
	wstringq m_symbol;
protected:
	static const Engine *s_engine;
public:
	StrExpr(const wstringq &i_symbol) : m_symbol(i_symbol) {};

	virtual ~StrExpr() {};

	virtual StrExpr *clone() const {
		return new StrExpr(*this);
	}

	virtual wstringq eval() const {
		return m_symbol;
	}

	static void setEngine(const Engine *i_engine) {
		s_engine = i_engine;
	}
};

const Engine *StrExpr::s_engine = NULL;

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// StrExprClipboard
class StrExprClipboard : public StrExpr
{
public:
	StrExprClipboard(const wstringq &i_symbol) : StrExpr(i_symbol) {};

	~StrExprClipboard() {};

	StrExpr *clone() const {
		return new StrExprClipboard(*this);
	}

	wstringq eval() const {
		HGLOBAL g;
		const wchar_t *text = getTextFromClipboard(&g);
		const std::wstring value(text == NULL ? L"" : text);
		closeClipboard(g);
		return value;
	}
};


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// StrExprWindowClassName
class StrExprWindowClassName : public StrExpr
{
public:
	StrExprWindowClassName(const wstringq &i_symbol) : StrExpr(i_symbol) {};

	~StrExprWindowClassName() {};

	StrExpr *clone() const {
		return new StrExprWindowClassName(*this);
	}

	wstringq eval() const {
		return s_engine->getCurrentWindowClassName();
	}
};


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// StrExprWindowTitleName
class StrExprWindowTitleName : public StrExpr
{
public:
	StrExprWindowTitleName(const wstringq &i_symbol) : StrExpr(i_symbol) {};

	~StrExprWindowTitleName() {};

	StrExpr *clone() const {
		return new StrExprWindowTitleName(*this);
	}

	wstringq eval() const {
		return s_engine->getCurrentWindowTitleName();
	}
};


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// StrExprArg


// default constructor
StrExprArg::StrExprArg()
{
	m_expr = std::make_unique<StrExpr>(L"");
}


// copy contructor
StrExprArg::StrExprArg(const StrExprArg &i_data)
{
	m_expr.reset(i_data.m_expr->clone());
}


StrExprArg &StrExprArg::operator=(const StrExprArg &i_data)
{
	if (i_data.m_expr.get() == m_expr.get())
		return *this;

	m_expr.reset(i_data.m_expr->clone());

	return *this;
}


// initializer
StrExprArg::StrExprArg(const wstringq &i_symbol, Type i_type)
{
	switch (i_type) {
	case Literal:
		m_expr = std::make_unique<StrExpr>(i_symbol);
		break;
	case Builtin:
		if (i_symbol == L"Clipboard")
			m_expr = std::make_unique<StrExprClipboard>(i_symbol);
		else if (i_symbol == L"WindowClassName")
			m_expr = std::make_unique<StrExprWindowClassName>(i_symbol);
		else if (i_symbol == L"WindowTitleName")
			m_expr = std::make_unique<StrExprWindowTitleName>(i_symbol);
		break;
	default:
		break;
	}
}


StrExprArg::~StrExprArg()
{
}


wstringq StrExprArg::eval() const
{
	return m_expr->eval();
}

void StrExprArg::setEngine(const Engine *i_engine)
{
	StrExpr::setEngine(i_engine);
}

// stream output
std::wostream &operator<<(std::wostream &i_ost, const StrExprArg &i_data)
{
	i_ost << i_data.eval();
	return i_ost;
}
