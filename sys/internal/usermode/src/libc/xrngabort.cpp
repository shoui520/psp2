// xrngabort.cpp: _Rng_abort, the failed-check report of <random>
#include <cstdio>
#include <cstdlib>

_STD_BEGIN
void _Rng_abort(const char *_Msg)
	{	// abort on precondition failure
	_CSTD fputs(_Msg, stderr);
	_CSTD fputc('\n', stderr);
	_CSTD abort();
	}
_STD_END
