// exceptions.cpp: the threads library's exceptions, lock_error and thread_resource_error
#include <system_error>
#include <Dinkum/threads/exceptions>

namespace Dinkum
	{	// Dinkum Libraries
	namespace threads
		{	// Dinkum C++ Threads Library

void _Throw_lock_error()
	{	// throw lock_error
	throw lock_error();
	}

void _Throw_resource_error()
	{	// throw thread_resource_error
	throw thread_resource_error();
	}
		}	// namespace threads
	}	// namespace Dinkum
