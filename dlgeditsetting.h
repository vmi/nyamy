//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// dlgeditsetting.h


#ifndef _DLGEDITSETTING_H
#  define _DLGEDITSETTING_H

#  include "stringtool.h"


/// dialog procedure of "Edit Setting" dialog box
INT_PTR CALLBACK dlgEditSetting_dlgProc(
	HWND i_hwnd, UINT i_message, WPARAM i_wParam, LPARAM i_lParam);

/// parameters for "Edit Setting" dialog box
class DlgEditSettingData {
public:
	wstringi m_name;				/// setting name
	wstringi m_filename;				/// filename of setting
	wstringi m_symbols;		/// symbol list (-Dsymbol1;-Dsymbol2;-D...)
};


#endif // !_DLGEDITSETTING_H
