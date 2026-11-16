/*
 * CinterfaceInternal.cpp: the C interface's internal side: the thread factories (SceUlt's
 * own C types SceUltUlthreadFactory and SceUltKernelThreadFactory, over ThreadFactory.h's
 * classes), which nothing in the library calls, and the module's start and stop beyond the
 * kernel's revision.
 */
#include "ult_internal.h"
#include "ThreadFactory.h"

/* a factory's storage, as a C caller holds it (no SDK header gives their sizes) */
#define _SCE_ULT_ULTHREAD_FACTORY_SIZE		128
#define _SCE_ULT_KERNEL_THREAD_FACTORY_SIZE	32

typedef struct SceUltUlthreadFactory {
	uint64_t __reserved__[_SCE_ULT_ULTHREAD_FACTORY_SIZE / sizeof(uint64_t)];
} SceUltUlthreadFactory;

typedef struct SceUltKernelThreadFactory {
	uint64_t __reserved__[_SCE_ULT_KERNEL_THREAD_FACTORY_SIZE / sizeof(uint64_t)];
} SceUltKernelThreadFactory;

typedef Impl::ThreadFactoryInterface SceUltThreadFactory;

extern "C" {

int sceUltThreadFactoryCreateThread(SceUltThreadFactory *factory, Impl::id_t *thread,
				    Impl::ThreadEntry entry, uint32_t arg)
{
	return factory->functions->createThread(factory->factory, thread, entry, arg);
}

uint32_t sceUltThreadFactoryGetNumMaxThreads(SceUltThreadFactory *factory)
{
	return factory->functions->getNumMaxThreads(factory->factory);
}

int sceUltThreadFactoryJoinThread(SceUltThreadFactory *factory, Impl::id_t id, int32_t *status)
{
	return factory->functions->joinThread(factory->factory, id, status);
}

int sceUltUlthreadFactoryGetWorkAreaSize(uint32_t *workAreaSize, uint32_t maxThreads,
					 uint32_t sizeContext)
{
	*workAreaSize = Impl::UlthreadFactory::getWorkAreaSize(maxThreads, sizeContext);
	return SCE_OK;
}

int sceUltUlthreadFactoryInitialize(SceUltUlthreadFactory *factory, SceUltUlthreadRuntime *runtime,
				    uint32_t maxThreads, uint32_t sizeContext, void *workArea)
{
	return ((Impl::UlthreadFactory *)factory)->initialize(runtime, maxThreads, sizeContext,
							       workArea);
}

int sceUltUlthreadFactoryGetInterface(SceUltUlthreadFactory *factory,
				      SceUltThreadFactory **interface)
{
	*interface = &((Impl::UlthreadFactory *)factory)->m_interface;
	return SCE_OK;
}

int sceUltUlthreadFactoryFinalize(SceUltUlthreadFactory *factory)
{
	return ((Impl::UlthreadFactory *)factory)->finalize();
}

int sceUltKernelThreadFactoryInitialize(SceUltKernelThreadFactory *factory, int32_t initPriority,
					uint32_t stackSize)
{
	return ((Impl::KernelThreadFactory *)factory)->initialize(initPriority, stackSize);
}

int sceUltKernelThreadFactoryGetInterface(SceUltKernelThreadFactory *factory,
					  SceUltThreadFactory **interface)
{
	*interface = &((Impl::KernelThreadFactory *)factory)->m_interface;
	return SCE_OK;
}

int sceUltKernelThreadFactoryFinalize(SceUltKernelThreadFactory *factory)
{
	return ((Impl::KernelThreadFactory *)factory)->finalize();
}

}

void initializeInternal()
{
}

void finalizeInternal()
{
}
