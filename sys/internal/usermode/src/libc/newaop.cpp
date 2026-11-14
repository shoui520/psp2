// newaop.cpp: operator new[](size_t), through the program's replacement
#include "newreplace.h"

void *_Default_new_array(_CSTD size_t count)
	{	// the library's: allocate an array
	return (operator new(count));
	}

void *operator new[](_CSTD size_t count) _THROWS(_XSTD bad_alloc)
	{	// allocate an array
	return ((*_User_new_array)(count));
	}
