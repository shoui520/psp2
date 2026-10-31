/* paf::thread::RMutex: a lightweight mutex its owner may lock again. */
#include <thread/thread.h>
#include <common/common_internal.h>
#include <kernel/error.h>

namespace paf {
namespace thread {

RMutex::RMutex(const char *name, int option)
{
	if (sceKernelCreateLwMutex(&m_mutex, name,
				   ((option & Mutex::OPTION_FIFO) ? SCE_KERNEL_LW_MUTEX_ATTR_TH_FIFO
								  : SCE_KERNEL_LW_MUTEX_ATTR_TH_PRIO) |
				   SCE_KERNEL_LW_MUTEX_ATTR_RECURSIVE,
				   0, NULL) != SCE_OK)
		common_internal::on_error();
}

RMutex::~RMutex()
{
	if (sceKernelDeleteLwMutex(&m_mutex) != SCE_OK)
		common_internal::on_error();
}

void RMutex::Lock()
{
	if (sceKernelLockLwMutex(&m_mutex, 1, NULL) != SCE_OK)
		common_internal::on_error();
}

void RMutex::Unlock()
{
	if (sceKernelUnlockLwMutex(&m_mutex, 1) != SCE_OK)
		common_internal::on_error();
}

/* True when another thread owns the mutex. */
bool RMutex::TryLock()
{
	int result = sceKernelTryLockLwMutex(&m_mutex, 1);

	if (result == SCE_KERNEL_ERROR_LW_MUTEX_FAILED_TO_OWN)
		return true;
	if (result != SCE_OK)
		common_internal::on_error();
	return false;
}

RMutex RMutex::main_thread_mutex("ScePafMainThread", 0);

}
}
