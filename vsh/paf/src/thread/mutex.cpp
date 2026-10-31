/* paf::thread::Mutex: a lightweight, non-recursive mutex. */
#include <thread/thread.h>
#include <common/common_internal.h>
#include <kernel/error.h>

namespace paf {
namespace thread {

Mutex::Mutex(const char *name, int option)
{
	if (sceKernelCreateLwMutex(&m_mutex, name,
				   (option & OPTION_FIFO) ? SCE_KERNEL_LW_MUTEX_ATTR_TH_FIFO
							  : SCE_KERNEL_LW_MUTEX_ATTR_TH_PRIO,
				   0, NULL) != SCE_OK)
		common_internal::on_error();
}

Mutex::~Mutex()
{
	if (sceKernelDeleteLwMutex(&m_mutex) != SCE_OK)
		common_internal::on_error();
}

void Mutex::Lock()
{
	if (sceKernelLockLwMutex(&m_mutex, 1, NULL) != SCE_OK)
		common_internal::on_error();
}

void Mutex::Unlock()
{
	if (sceKernelUnlockLwMutex(&m_mutex, 1) != SCE_OK)
		common_internal::on_error();
}

/* True when another thread owns the mutex. */
bool Mutex::TryLock()
{
	int result = sceKernelTryLockLwMutex(&m_mutex, 1);

	if (result == SCE_KERNEL_ERROR_LW_MUTEX_FAILED_TO_OWN)
		return true;
	if (result != SCE_OK)
		common_internal::on_error();
	return false;
}

}
}
