// delaop.cpp: operator delete[](void *), through the program's replacement
#include "newreplace.h"

void _Default_delete_array(void *ptr) _THROW0()
	{	// the library's: free an allocated array
	operator delete(ptr);
	}

void operator delete[](void *ptr) _THROW0()
	{	// free an allocated array
	(*_User_delete_array)(ptr);
	}
