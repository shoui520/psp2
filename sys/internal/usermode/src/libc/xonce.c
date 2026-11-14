/*
 * xonce.c: _Call_once, the C threads' call_once: the once lock serializes the calls, and
 * the flag is set after the call returns (the file name is ours).
 */
#include <yvals.h>
#include <Dinkum/threads/xthreads.h>

void _Call_once(_Once_flag *flag, _Once_callback_t func)
{
	_Locksyslock(_LOCK_ONCE);
	if (*flag == 0) {
		func();
		*flag = 1;
	}
	_Unlocksyslock(_LOCK_ONCE);
}
