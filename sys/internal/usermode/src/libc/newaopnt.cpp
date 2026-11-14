// newaopnt.cpp: operator new[](size_t, const nothrow_t&), through the program's replacement
#include "newreplace.h"

void *_Default_new_array_nothrow(_CSTD size_t count, const _STD nothrow_t& nt) _THROW0()
	{	// the library's: allocate an array, or return a null pointer
	return (operator new(count, nt));
	}

void *operator new[](_CSTD size_t count, const _STD nothrow_t& nt) _THROW0()
	{	// allocate an array, or return a null pointer
	return ((*_User_new_array_nothrow)(count, nt));
	}
