/*
 * ThreadFactory.cpp: the factories' interface functions, each given the factory.
 */
#include "ult_internal.h"
#include "ThreadFactory.h"

namespace Impl {

int UlthreadFactory::createThread(void *factory, id_t *thread, ThreadEntry entry, uint32_t arg)
{
	return ((UlthreadFactory *)factory)->createThread(thread, entry, arg);
}

uint32_t UlthreadFactory::getNumMaxThreads(void *factory)
{
	return ((UlthreadFactory *)factory)->getNumMaxThreads();
}

int UlthreadFactory::joinThread(void *factory, id_t id, int32_t *status)
{
	return ((UlthreadFactory *)factory)->joinThread(id, status);
}

int KernelThreadFactory::createThread(void *factory, id_t *thread, ThreadEntry entry, uint32_t arg)
{
	return ((KernelThreadFactory *)factory)->createThread(thread, entry, arg);
}

uint32_t KernelThreadFactory::getNumMaxThreads(void *factory)
{
	return ((KernelThreadFactory *)factory)->getNumMaxThreads();
}

int KernelThreadFactory::joinThread(void *factory, id_t id, int32_t *status)
{
	return ((KernelThreadFactory *)factory)->joinThread(id, status);
}

const ThreadFactoryFunctions UlthreadFactory::s_functions = {
	UlthreadFactory::createThread,
	UlthreadFactory::getNumMaxThreads,
	UlthreadFactory::joinThread,
};

const ThreadFactoryFunctions KernelThreadFactory::s_functions = {
	KernelThreadFactory::createThread,
	KernelThreadFactory::getNumMaxThreads,
	KernelThreadFactory::joinThread,
};

}
