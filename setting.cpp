//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// setting.cpp


#include "misc.h"

#include "setting.h"


namespace Event
{
Key prefixed(_T("prefixed"));
Key before_key_down(_T("before-key-down"));
Key after_key_up(_T("after-key-up"));
Key *events[] = {
	&prefixed,
	&before_key_down,
	&after_key_up,
	NULL,
};
}
