#include "msg_pipe.h"
#include <libkernel/libkernel.h>
#include "../ipmi_error.h"

namespace IPMI {
namespace impl {
namespace psp2 {

MsgPipe::MsgPipe()
{
	m_id = -1;
	m_owner = false;
}

MsgPipe::~MsgPipe()
{
	m_id = -1;
}

int MsgPipe::create(const char *name, SceSize bufSize, int budget, int openLimit)
{
	SceKernelMsgPipeOptParam opt;
	SceKernelMsgPipeOptParam *pOpt;
	SceUInt32 type = SCE_KERNEL_MSG_PIPE_TYPE_USER_MAIN;
	SceUID id;

	switch (budget) {
	case KERNEL_BUDGET:
		sceClibPrintf("[SceIpmi] *WARNING* Ignored IPMI::KERNEL_BUDGET (%s)\n", name);
		break;
	case USER_BUDGET:
		break;
	case PHYCONT_BUDGET:
		type = MSG_PIPE_TYPE_USER_MAIN_PHYCONT;
		break;
	}
	pOpt = NULL;
	if (openLimit != -1) {
		pOpt = &opt;
		sceClibMemset(&opt, 0, sizeof(opt));
		opt.size = sizeof(opt);
		opt.attr = 1;
		opt.reserved[0] = openLimit;
	}
	id = sceKernelCreateMsgPipe(name, type,
		SCE_KERNEL_ATTR_OPENABLE | SCE_KERNEL_MSG_PIPE_ATTR_TH_PRIO, bufSize, pOpt);
	if (id >= 0) {
		m_id = id;
		id = 0;
		m_owner = true;
	}
	return id;
}

int MsgPipe::open(const char *name)
{
	SceUID id = sceKernelOpenMsgPipe(name);

	if (id < 0) {
		if (id == SCE_KERNEL_ERROR_UID_CANNOT_FIND_BY_NAME ||
		    id == SCE_KERNEL_ERROR_FIXEDHEAP_ILLEGAL_INDEX)
			return IPMI_ERROR_NOT_FOUND;
		return id;
	}
	m_id = id;
	return 0;
}

int MsgPipe::close()
{
	int ret;

	if (m_id == -1)
		return IPMI_ERROR_NOT_INITIALIZED;
	if (m_owner)
		ret = sceKernelDeleteMsgPipe(m_id);
	else
		ret = sceKernelCloseMsgPipe(m_id);
	if (ret == 0)
		m_id = -1;
	return ret;
}

int MsgPipe::send(const void *buf, SceSize size, SceUInt32 *timeout)
{
	int ret;

	if (size == 0)
		return 0;
	ret = sceKernelSendMsgPipe(m_id, buf, size, SCE_KERNEL_MSG_PIPE_MODE_FULL, NULL,
				   timeout);
	if (ret == SCE_KERNEL_ERROR_WAIT_TIMEOUT)
		return IPMI_ERROR_TIMEOUT;
	if (ret == SCE_KERNEL_ERROR_WAIT_CANCEL)
		return IPMI_ERROR_CANCELED;
	if (ret == SCE_KERNEL_ERROR_MSG_PIPE_DELETED)
		return 0;
	return ret;
}

int MsgPipe::trySend(const void *buf, SceSize size)
{
	int ret;

	if (size == 0)
		return 0;
	ret = sceKernelTrySendMsgPipe(m_id, buf, size, SCE_KERNEL_MSG_PIPE_MODE_FULL, NULL);
	if (ret == SCE_KERNEL_ERROR_MSG_PIPE_FULL)
		return IPMI_ERROR_WOULD_BLOCK;
	if (ret == SCE_KERNEL_ERROR_MSG_PIPE_DELETED)
		return 0;
	return ret;
}

int MsgPipe::sendVector(const BufferInfo *info, SceUInt32 count, SceUInt32 *timeout)
{
	SceUInt32 used, i;
	int ret;

	if (count == 0)
		return 0;

	SceKernelMsgPipeVector vector[count];

	used = 0;
	for (i = 0; i < count; i++, info++) {
		if (info->bufferSize == 0)
			continue;
		vector[used].pBase = info->pBuffer;
		vector[used].bufSize = info->bufferSize;
		used++;
	}
	ret = 0;
	if (used != 0) {
		ret = sceKernelSendMsgPipeVector(m_id, vector, used,
						 SCE_KERNEL_MSG_PIPE_MODE_FULL, NULL, timeout);
		if (ret == SCE_KERNEL_ERROR_WAIT_TIMEOUT)
			ret = IPMI_ERROR_TIMEOUT;
		else if (ret == SCE_KERNEL_ERROR_WAIT_CANCEL)
			ret = IPMI_ERROR_CANCELED;
		else if (ret == SCE_KERNEL_ERROR_MSG_PIPE_DELETED)
			ret = 0;
	}
	return ret;
}

int MsgPipe::trySendVector(const BufferInfo *info, SceUInt32 count)
{
	SceUInt32 used, i;
	int ret;

	if (count == 0)
		return 0;

	SceKernelMsgPipeVector vector[count];

	used = 0;
	for (i = 0; i < count; i++, info++) {
		if (info->bufferSize == 0)
			continue;
		vector[used].pBase = info->pBuffer;
		vector[used].bufSize = info->bufferSize;
		used++;
	}
	ret = 0;
	if (used != 0) {
		ret = sceKernelTrySendMsgPipeVector(m_id, vector, used,
						    SCE_KERNEL_MSG_PIPE_MODE_FULL, NULL);
		if (ret == SCE_KERNEL_ERROR_MSG_PIPE_FULL)
			ret = IPMI_ERROR_WOULD_BLOCK;
		else if (ret == SCE_KERNEL_ERROR_MSG_PIPE_DELETED)
			ret = 0;
	}
	return ret;
}

int MsgPipe::tryReceive(void *buf, SceSize *result, SceSize size)
{
	int ret;

	if (size == 0) {
		if (result != NULL)
			*result = 0;
		return 0;
	}
	ret = sceKernelTryReceiveMsgPipe(m_id, buf, size, SCE_KERNEL_MSG_PIPE_MODE_FULL,
					 result);
	if (ret == SCE_KERNEL_ERROR_MSG_PIPE_EMPTY)
		return IPMI_ERROR_WOULD_BLOCK;
	if (ret == SCE_KERNEL_ERROR_MSG_PIPE_DELETED)
		return IPMI_ERROR_INVALID_ARGUMENT;
	return ret;
}

int MsgPipe::cancel()
{
	int ret = sceKernelCancelMsgPipe(m_id, NULL, NULL);

	if (ret == SCE_KERNEL_ERROR_MSG_PIPE_DELETED)
		return 0;
	return ret;
}

}
}
}
