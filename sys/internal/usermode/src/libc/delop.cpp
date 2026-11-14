// delop.cpp: operator delete(void *), through the program's replacement
#include <stdlib.h>
#include "newreplace.h"

void _Default_delete(void *ptr) _THROW0()
	{	// the library's: free an allocated object
	if (ptr != 0)
		free(ptr);
	}

void operator delete(void *ptr) _THROW0()
	{	// free an allocated object
	(*_User_delete)(ptr);
	}
