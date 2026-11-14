// newhandler.cpp: the new handler operator new calls when it finds no memory, read and set
// under the library's miscellaneous lock
#include <new>
#include "newreplace.h"

_STD_BEGIN
new_handler _get_new_handler() _THROW0()
	{	// get new handler
	_Lockit _Lock(_LOCK_MISC);

	return (*_New_handler_address());
	}

new_handler get_new_handler() _NOEXCEPT
	{	// get new handler
	_Lockit _Lock(_LOCK_MISC);

	return (*_New_handler_address());
	}

new_handler set_new_handler(new_handler _Pnew) _THROW0()
	{	// establish alternate new handler
	_Lockit _Lock(_LOCK_MISC);
	new_handler *_Phandler = _New_handler_address();
	new_handler _Pold = *_Phandler;

	*_Phandler = _Pnew;
	return (_Pold);
	}
_STD_END
