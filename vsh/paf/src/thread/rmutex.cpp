/* paf::thread::RMutex: a lightweight mutex its owner may lock again. */
#include <thread/thread.h>
#include <common/common_internal.h>
#include <kernel/error.h>

#ifdef PAF_STATIC_BUILD
extern "C" {
int SceLibKernel_A7819967(SceKernelLwMutexWork *work, int count, unsigned int *timeout);
int SceLibKernel_499EA781(SceKernelLwMutexWork *work, int count);
int SceLibKernel_9EF798C1(SceKernelLwMutexWork *work, int count);
}
#endif

namespace paf {
namespace thread {

RMutex::RMutex(const char *name, int option)
{
#ifdef PAF_STATIC_BUILD
	sceKernelCreateLwMutex(&m_mutex, name,
			       ((option & Mutex::OPTION_FIFO) ? SCE_KERNEL_LW_MUTEX_ATTR_TH_FIFO
							      : SCE_KERNEL_LW_MUTEX_ATTR_TH_PRIO) |
			       SCE_KERNEL_LW_MUTEX_ATTR_RECURSIVE,
			       0, NULL);
#else
	if (sceKernelCreateLwMutex(&m_mutex, name,
				   ((option & Mutex::OPTION_FIFO) ? SCE_KERNEL_LW_MUTEX_ATTR_TH_FIFO
								  : SCE_KERNEL_LW_MUTEX_ATTR_TH_PRIO) |
				   SCE_KERNEL_LW_MUTEX_ATTR_RECURSIVE,
				   0, NULL) != SCE_OK)
		common_internal::on_error();
#endif
}

RMutex::~RMutex()
{
#ifdef PAF_STATIC_BUILD
	sceKernelDeleteLwMutex(&m_mutex);
#else
	if (sceKernelDeleteLwMutex(&m_mutex) != SCE_OK)
		common_internal::on_error();
#endif
}

void RMutex::Lock()
{
#ifdef PAF_STATIC_BUILD
	SceLibKernel_A7819967(&m_mutex, 1, NULL);
#else
	if (sceKernelLockLwMutex(&m_mutex, 1, NULL) != SCE_OK)
		common_internal::on_error();
#endif
}

void RMutex::Unlock()
{
#ifdef PAF_STATIC_BUILD
	SceLibKernel_499EA781(&m_mutex, 1);
#else
	if (sceKernelUnlockLwMutex(&m_mutex, 1) != SCE_OK)
		common_internal::on_error();
#endif
}

/* True when another thread owns the mutex. */
bool RMutex::TryLock()
{
#ifdef PAF_STATIC_BUILD
	return SceLibKernel_9EF798C1(&m_mutex, 1) == SCE_KERNEL_ERROR_LW_MUTEX_FAILED_TO_OWN;
#else
	int result = sceKernelTryLockLwMutex(&m_mutex, 1);

	if (result == SCE_KERNEL_ERROR_LW_MUTEX_FAILED_TO_OWN)
		return true;
	if (result != SCE_OK)
		common_internal::on_error();
	return false;
#endif
}

RMutex RMutex::main_thread_mutex("ScePafMainThread", 0);

}
}
