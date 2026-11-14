// memory.cpp: bad_weak_ptr's destructor, which the psp2 headers declare out of line
#include <memory>

_STD_BEGIN
bad_weak_ptr::~bad_weak_ptr() _NOEXCEPT
	{	// destroy the object
	}
_STD_END
