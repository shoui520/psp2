// newopnt.cpp: operator new(size_t, const nothrow_t&), through the program's replacement
#include <stdlib.h>
#include "newreplace.h"

void *_Default_new_nothrow(_CSTD size_t size, const _STD nothrow_t&) _THROW0()
	{	// the library's: allocate size bytes, or call the new handler, or return a null pointer
	void *p;

	_TRY_BEGIN
	if (size == 0)
		size = 1;
	while ((p = malloc(size)) == 0)
		{	// no memory: the new handler frees some, or a null pointer
		_STD new_handler handler = *_New_handler_address();

		if (handler == 0)
			return (0);
		(*handler)();
		}
	_CATCH(_XSTD bad_alloc)
	return (0);
	_CATCH_END
	return (p);
	}

void *operator new(_CSTD size_t size, const _STD nothrow_t& nt) _THROW0()
	{	// allocate size bytes, or return a null pointer
	return ((*_User_new_nothrow)(size, nt));
	}
