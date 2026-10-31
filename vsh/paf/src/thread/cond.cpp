/* paf::thread::Cond: a lightweight condition variable on a paf::thread::Mutex. */
#include <thread/thread.h>
#include <common/common_internal.h>
#include <kernel/error.h>

namespace paf {
namespace thread {

Cond::Cond(Mutex &mutex, const char *name, int option)
{
	m_mutex = &mutex;
	if (sceKernelCreateLwCond(&m_cond, name,
				  (option & Mutex::OPTION_FIFO) ? SCE_KERNEL_LW_COND_ATTR_TH_FIFO
								: SCE_KERNEL_LW_COND_ATTR_TH_PRIO,
				  &mutex.m_mutex, NULL) != SCE_OK)
		common_internal::on_error();
}

Cond::~Cond()
{
	if (sceKernelDeleteLwCond(&m_cond) != SCE_OK)
		common_internal::on_error();
}

/* Waits for a notification, at most milliseconds when positive; true on timeout. */
bool Cond::Wait(int milliseconds)
{
	SceUInt32 timeout;
	SceUInt32 *pTimeout = NULL;

	if (milliseconds > 0) {
		pTimeout = &timeout;
		timeout = milliseconds * 1000;
	}

	int result = sceKernelWaitLwCond(&m_cond, pTimeout);

	if (result == SCE_KERNEL_ERROR_WAIT_TIMEOUT)
		return true;
	if (result != SCE_OK)
		common_internal::on_error();
	return false;
}

void Cond::Notify()
{
	if (sceKernelSignalLwCond(&m_cond) != SCE_OK)
		common_internal::on_error();
}

void Cond::NotifyAll()
{
	if (sceKernelSignalLwCondAll(&m_cond) != SCE_OK)
		common_internal::on_error();
}

}
}
