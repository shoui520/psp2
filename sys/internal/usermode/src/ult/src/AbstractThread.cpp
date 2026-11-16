/*
 * AbstractThread.cpp: the current thread, from the TLS slot.
 */
#include "AbstractThread.h"

#define ABSTRACT_THREAD_CPP_ASSERT(line, cond) \
	ULT_ASSERT_AT("AbstractThread.cpp", line, #cond, cond)

AbstractThread **AbstractThread::getTLS()
{
	return (AbstractThread **)sceKernelGetTLSAddr(ULT_TLS_KEY);
}

uint32_t AbstractThread::getID2()
{
	uint32_t id = *(uint32_t *)sceKernelGetTLSAddr(ULT_TLS_KEY);

	ABSTRACT_THREAD_CPP_ASSERT(37, (id & 0x3) == 0);
	if (id != 0)
		return id;
	uint32_t tid = sceKernelGetThreadId();
	ABSTRACT_THREAD_CPP_ASSERT(43, (tid & 0x80000000)==0);
	return (tid << 1) | 1;
}
