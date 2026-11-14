// delaopnt.cpp: operator delete[](void *, const nothrow_t&), through the program's replacement
#include "newreplace.h"

void _Default_delete_array_nothrow(void *ptr, const _STD nothrow_t& nt) _THROW0()
	{	// the library's: free an array allocated by nothrow new[]
	operator delete(ptr, nt);
	}

void operator delete[](void *ptr, const _STD nothrow_t& nt) _THROW0()
	{	// free an array allocated by nothrow new[]
	(*_User_delete_array_nothrow)(ptr, nt);
	}
