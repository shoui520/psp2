// new.cpp: operator new(size_t), through the program's replacement
#include <stdlib.h>
#include "newreplace.h"
_STD_BEGIN
void _Xbad_alloc();	// xthrow.cpp
_STD_END

void *_Default_new(_CSTD size_t size)
	{	// the library's: allocate size bytes, or call the new handler, or throw bad_alloc
	void *p;

	if (size == 0)
		size = 1;
	while ((p = malloc(size)) == 0)
		{	// no memory: the new handler frees some, or bad_alloc
		_STD new_handler handler = *_New_handler_address();

		if (handler == 0)
			_STD _Xbad_alloc();
		else
			(*handler)();
		}
	return (p);
	}

void *operator new(_CSTD size_t size) _THROWS(_XSTD bad_alloc)
	{	// allocate size bytes
	return ((*_User_new)(size));
	}
