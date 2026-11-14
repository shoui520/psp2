// delaopv.cpp: operator delete[](void *, void *), for a placement new[] that fails
#include <new>

void operator delete[](void *, void *) _THROW0()
	{	// nothing to free
	}
