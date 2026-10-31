#include "event_flag.h"
#include "../ipmi_error.h"

namespace IPMI {
namespace impl {
namespace psp2 {

EventFlag::EventFlag()
{
	m_id = -1;
	m_owner = false;
}

int EventFlag::create(const char *name)
{
	SceUID id = sceKernelCreateEventFlag(name,
		SCE_KERNEL_ATTR_MULTI | SCE_KERNEL_ATTR_OPENABLE, 0, NULL);

	if (id < 0)
		return id;
	m_id = id;
	m_owner = true;
	return 0;
}

int EventFlag::open(const char *name)
{
	SceUID id = sceKernelOpenEventFlag(name);

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

int EventFlag::close()
{
	int ret;

	if (m_id == -1)
		return 0;
	if (m_owner)
		ret = sceKernelDeleteEventFlag(m_id);
	else
		ret = sceKernelCloseEventFlag(m_id);
	if (ret == 0)
		m_id = -1;
	return ret;
}

int EventFlag::set(SceUInt32 bits)
{
	if (m_id == -1)
		return 0;
	return sceKernelSetEventFlag(m_id, bits);
}

int EventFlag::poll(SceUInt32 bits, SceUInt32 waitMode, SceUInt32 *resultBits)
{
	if (m_id == -1)
		return IPMI_ERROR_CANCELED;
	return sceKernelPollEventFlag(m_id, bits, waitMode, resultBits);
}

int EventFlag::wait(SceUInt32 bits, SceUInt32 waitMode, SceUInt32 *resultBits,
		    SceUInt32 *timeout)
{
	int ret;

	if (m_id == -1)
		return IPMI_ERROR_CANCELED;
	ret = sceKernelWaitEventFlag(m_id, bits, waitMode, resultBits, timeout);
	if (ret == SCE_KERNEL_ERROR_WAIT_CANCEL || ret == SCE_KERNEL_ERROR_WAIT_DELETE)
		return IPMI_ERROR_CANCELED;
	if (ret == SCE_KERNEL_ERROR_WAIT_TIMEOUT)
		return IPMI_ERROR_TIMEOUT;
	return ret;
}

int EventFlag::destroy()
{
	int ret;

	if (m_id == -1)
		return 0;
	if (m_owner)
		ret = sceKernelDeleteEventFlag(m_id);
	else
		ret = sceKernelCloseEventFlag(m_id);
	if (ret == 0)
		m_id = -1;
	return ret;
}

}
}
}
