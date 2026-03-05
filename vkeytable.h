//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// vkeytable.h


#ifndef _VKEYTABLE_H
#  define _VKEYTABLE_H

#  include "misc.h"


/// define virtual key code and its name
class VKeyTable
{
public:
	u_int8 m_code;				/// VKey code
	const wchar_t *m_name;				/// VKey name
};

extern const VKeyTable g_vkeyTable[];		/** Vkey table (terminated by
						    NULL) */


#endif // !_VKEYTABLE_H
