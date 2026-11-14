// delopnt.cpp: operator delete(void *, const nothrow_t&), through the program's replacement
#include "newreplace.h"

void _Default_delete_nothrow(void *ptr, const _STD nothrow_t&) _THROW0()
	{	// the library's: free an object allocated by nothrow new
	operator delete(ptr);
	}

void operator delete(void *ptr, const _STD nothrow_t& nt) _THROW0()
	{	// free an object allocated by nothrow new
	(*_User_delete_nothrow)(ptr, nt);
	}
