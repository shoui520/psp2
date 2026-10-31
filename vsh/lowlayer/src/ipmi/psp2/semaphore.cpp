#include "semaphore.h"
#include "../ipmi_error.h"

namespace IPMI {
namespace impl {
namespace psp2 {

Semaphore::Semaphore()
{
	m_id = -1;
	m_owner = false;
}

Semaphore::~Semaphore()
{
	m_id = -1;
}

int Semaphore::create(const char *name, int maxCount, int initCount)
{
	SceUID id = sceKernelCreateSema(name,
		SCE_KERNEL_SEMA_ATTR_TH_PRIO | SCE_KERNEL_ATTR_OPENABLE, initCount, maxCount,
		NULL);

	if (id < 0)
		return id;
	m_id = id;
	m_owner = true;
	return 0;
}

int Semaphore::open(const char *name)
{
	SceUID id = sceKernelOpenSema(name);

	if (id < 0) {
		if (id == SCE_KERNEL_ERROR_UID_CANNOT_FIND_BY_NAME ||
		    id == SCE_KERNEL_ERROR_FIXEDHEAP_ILLEGAL_INDEX)
			return IPMI_ERROR_NOT_FOUND;
		return id;
	}
	m_id = id;
	m_owner = false;
	return 0;
}

int Semaphore::close()
{
	int ret;

	if (m_id == -1)
		return 0;
	if (m_owner)
		ret = sceKernelDeleteSema(m_id);
	else
		ret = sceKernelCloseSema(m_id);
	if (ret == 0)
		m_id = -1;
	return ret;
}

int Semaphore::wait(SceUInt32 *timeout)
{
	int ret;

	if (m_id == -1)
		return IPMI_ERROR_CANCELED;
	ret = sceKernelWaitSema(m_id, 1, timeout);
	if (ret == SCE_KERNEL_ERROR_WAIT_TIMEOUT)
		return IPMI_ERROR_TIMEOUT;
	if (ret == SCE_KERNEL_ERROR_WAIT_CANCEL || ret == SCE_KERNEL_ERROR_WAIT_DELETE)
		return IPMI_ERROR_CANCELED;
	return ret;
}

int Semaphore::waitCB(SceUInt32 *timeout)
{
	int ret;

	if (m_id == -1)
		return IPMI_ERROR_CANCELED;
	ret = sceKernelWaitSemaCB(m_id, 1, timeout);
	if (ret == SCE_KERNEL_ERROR_WAIT_TIMEOUT)
		return IPMI_ERROR_TIMEOUT;
	if (ret == SCE_KERNEL_ERROR_WAIT_CANCEL || ret == SCE_KERNEL_ERROR_WAIT_DELETE)
		return IPMI_ERROR_CANCELED;
	return ret;
}

int Semaphore::tryWait()
{
	if (m_id == -1)
		return IPMI_ERROR_CANCELED;
	if (sceKernelPollSema(m_id, 1) == SCE_KERNEL_ERROR_SEMA_ZERO)
		return IPMI_ERROR_WOULD_BLOCK;
	return 0;
}

int Semaphore::signal()
{
	if (m_id == -1) {
		__builtin_trap();
		return 0;
	}
	return sceKernelSignalSema(m_id, 1);
}

int Semaphore::cancel(int setCount)
{
	if (m_id == -1)
		return 0;
	return sceKernelCancelSema(m_id, setCount, NULL);
}

int Semaphore::destroy()
{
	int ret;

	if (m_id == -1)
		return 0;
	if (m_owner)
		ret = sceKernelDeleteSema(m_id);
	else
		ret = sceKernelCloseSema(m_id);
	if (ret == 0)
		m_id = -1;
	return ret;
}

}
}
}
