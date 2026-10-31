#include "mutex.h"
#include "../ipmi_error.h"

namespace IPMI {
namespace impl {
namespace psp2 {

Mutex::Mutex()
{
	m_id = -1;
}

int Mutex::create()
{
	SceUID id = sceKernelCreateMutex("SceLibIpmi", SCE_KERNEL_MUTEX_ATTR_TH_PRIO, 0, NULL);

	if (id < 0)
		return id;
	m_id = id;
	return 0;
}

int Mutex::finalize()
{
	SceUID id = m_id;

	if (id == -1)
		return 0;
	m_id = -1;
	return sceKernelDeleteMutex(id);
}

int Mutex::lock()
{
	int ret;

	if (m_id == -1)
		return IPMI_ERROR_CANCELED;
	ret = sceKernelLockMutex(m_id, 1, NULL);
	if (ret == SCE_KERNEL_ERROR_WAIT_CANCEL || ret == SCE_KERNEL_ERROR_WAIT_DELETE ||
	    ret == SCE_KERNEL_ERROR_INVALID_UID || ret == SCE_KERNEL_ERROR_UNKNOWN_MUTEX_ID)
		return IPMI_ERROR_CANCELED;
	return ret;
}

int Mutex::unlock()
{
	int ret;

	if (m_id == -1)
		return IPMI_ERROR_NOT_OWNER;
	ret = sceKernelUnlockMutex(m_id, 1);
	if (ret == SCE_KERNEL_ERROR_MUTEX_NOT_OWNED || ret == SCE_KERNEL_ERROR_INVALID_UID ||
	    ret == SCE_KERNEL_ERROR_UNKNOWN_MUTEX_ID)
		return IPMI_ERROR_NOT_OWNER;
	return ret;
}

int Mutex::destroy()
{
	int ret;

	if (m_id == -1)
		return 0;
	ret = sceKernelDeleteMutex(m_id);
	if (ret == 0)
		m_id = -1;
	return ret;
}

}
}
}
