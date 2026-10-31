/* paf::thread::Semaphore: a counting semaphore, waiters in priority order. */
#include <thread/thread.h>
#include <common/common_internal.h>
#include <kernel/error.h>

namespace paf {
namespace thread {

Semaphore::Semaphore(const char *name, int initCount, int maxCount, int option)
{
	m_sema = sceKernelCreateSema(name, SCE_KERNEL_SEMA_ATTR_TH_PRIO, initCount, maxCount, NULL);
	if (m_sema < 0)
		common_internal::on_error();
}

Semaphore::~Semaphore()
{
	if (sceKernelDeleteSema(m_sema) != SCE_OK)
		common_internal::on_error();
}

void Semaphore::Acquire()
{
	if (sceKernelWaitSema(m_sema, 1, NULL) != SCE_OK)
		common_internal::on_error();
}

void Semaphore::Release()
{
	if (sceKernelSignalSema(m_sema, 1) != SCE_OK)
		common_internal::on_error();
}

}
}
