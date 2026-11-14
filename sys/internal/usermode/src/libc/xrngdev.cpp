// xrngdev.cpp: the source behind random_device, which on psp2 is rand() and claims no entropy
#include <cstdlib>

_STD_BEGIN
double _Random_device_entropy()
	{	// return entropy of random number source
	return (0.0);
	}

unsigned int _Random_device()
	{	// return a random value
	return (_CSTD rand());
	}
_STD_END
