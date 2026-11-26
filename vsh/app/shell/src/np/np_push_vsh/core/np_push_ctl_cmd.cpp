/*
 * np_push_ctl_cmd.cpp: Control thread command queue (SceNpPushCtlCmdQueue).
 * Sony path: vsh/app/common/src/np/np_push_vsh/core/np_push_ctl_cmd.cpp
 * 3.65-CEX: 0x813ACEE4..0x813AD09F
 */
#include "../np_push_internal.h"

namespace sce {
namespace Np {
namespace Push {

/* 0x813ACEE4 */
CtlCmdQueue::CtlCmdQueue() : m_initialized(false)
{
	SceNpCommonPs4_7E37A6BF(&m_queue, GetAllocator());
}

/* 0x813ACF28 */
CtlCmdQueue::~CtlCmdQueue()
{
	NP_PUSH_ASSERT(!m_initialized, "~CtlCmdQueue",
		       "core\\np_push_ctl_cmd.cpp", 41, "!m_initialized");
	SceNpCommonPs4_3311DD67(&m_queue);
}

/* 0x813ACF9C */
int CtlCmdQueue::Init(void)
{
	if (m_initialized)
		return SCE_NP_PUSH_ERROR_ALREADY_INITIALIZED;

	int ret = SceNpCommonPs4_EDE54448(&m_queue, "SceNpPushCtlCmdQueue",
					  sizeof(CtlCmd), NP_PUSH_CTL_CMD_QUEUE_CAPACITY);
	if (ret < 0)
		return ret;

	m_initialized = true;
	return 0;
}

/* 0x813ACFE4 */
void CtlCmdQueue::Finalize(void)
{
	if (!m_initialized)
		return;

	SceNpCommonPs4_DC7BB7F1(&m_queue);
	m_initialized = false;
}

/* 0x813AD014 */
int CtlCmdQueue::Push(const CtlCmd *cmd)
{
	if (!m_initialized)
		return SCE_NP_PUSH_ERROR_NOT_INITIALIZED;
	if (cmd == NULL)
		return SCE_NP_PUSH_ERROR_INVALID_ARGUMENT;

	int ret = SceNpCommonPs4_38E19C68(&m_queue, cmd, sizeof(CtlCmd), 0);
	if (ret < 0)
		return ret;
	return 0;
}

/* 0x813AD04C */
int CtlCmdQueue::Pop(CtlCmd *outCmd, SceUInt32 timeoutUsec)
{
	if (!m_initialized)
		return SCE_NP_PUSH_ERROR_NOT_INITIALIZED;
	if (outCmd == NULL)
		return SCE_NP_PUSH_ERROR_INVALID_ARGUMENT;

	int ret = SceNpCommonPs4_F634B111(&m_queue, outCmd, sizeof(CtlCmd), timeoutUsec);
	if (ret < 0)
		return ret;
	return 0;
}

/* 0x813AD080 */
void CtlCmdQueue::Abort(void)
{
	if (!m_initialized)
		return;

	SceNpCommonPs4_3A4163FD(&m_queue, 3);
}

} /* namespace Push */
} /* namespace Np */
} /* namespace sce */
