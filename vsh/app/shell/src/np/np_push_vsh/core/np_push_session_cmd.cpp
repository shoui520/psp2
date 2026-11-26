/*
 * np_push_session_cmd.cpp: Session thread command queue (SceNpPushSessionCmdQueue).
 * Sony path: vsh/app/common/src/np/np_push_vsh/core/np_push_session_cmd.cpp
 * 3.65-CEX: 0x813B17F4..0x813B1A27
 */
#include "../np_push_internal.h"

namespace sce {
namespace Np {
namespace Push {

/* 0x813B17F4 */
SessionCmdQueue::SessionCmdQueue() : m_initialized(false)
{
	SceNpCommonPs4_7E37A6BF(&m_queue, GetAllocator());
}

/* 0x813B1860 */
SessionCmdQueue::~SessionCmdQueue()
{
	NP_PUSH_ASSERT(!m_initialized, "~SessionCmdQueue",
		       "core\\np_push_session_cmd.cpp", 41, "!m_initialized");
	SceNpCommonPs4_3311DD67(&m_queue);
}

/* 0x813B18E0 */
int SessionCmdQueue::Init(void)
{
	if (m_initialized)
		return SCE_NP_PUSH_ERROR_ALREADY_INITIALIZED;

	int ret = SceNpCommonPs4_EDE54448(&m_queue, "SceNpPushSessionCmdQueue",
					  sizeof(SessionCmd), NP_PUSH_SESSION_CMD_QUEUE_CAPACITY);
	if (ret < 0)
		return ret;

	m_initialized = true;
	return 0;
}

/* 0x813B1920 */
void SessionCmdQueue::Finalize(void)
{
	if (!m_initialized)
		return;

	SceNpCommonPs4_DC7BB7F1(&m_queue);
	m_initialized = false;
}

/* 0x813B1948 */
int SessionCmdQueue::Push(const SessionCmd *cmd)
{
	if (!m_initialized)
		return SCE_NP_PUSH_ERROR_NOT_INITIALIZED;
	if (cmd == NULL)
		return SCE_NP_PUSH_ERROR_INVALID_ARGUMENT;

	int ret = SceNpCommonPs4_38E19C68(&m_queue, cmd, sizeof(SessionCmd), 0);
	if (ret < 0)
		return ret;
	return 0;
}

/* 0x813B1970 */
int SessionCmdQueue::Pop(SessionCmd *outCmd, SceUInt32 timeoutUsec)
{
	if (!m_initialized)
		return SCE_NP_PUSH_ERROR_NOT_INITIALIZED;
	if (outCmd == NULL)
		return SCE_NP_PUSH_ERROR_INVALID_ARGUMENT;

	int ret = SceNpCommonPs4_F634B111(&m_queue, outCmd, sizeof(SessionCmd), timeoutUsec);
	if (ret < 0)
		return ret;
	return 0;
}

/* 0x813B1984 */
int SessionCmdQueue::TryPop(SessionCmd *outCmd)
{
	if (!m_initialized)
		return SCE_NP_PUSH_ERROR_NOT_INITIALIZED;
	if (outCmd == NULL)
		return SCE_NP_PUSH_ERROR_INVALID_ARGUMENT;

	int ret = SceNpCommonPs4_6C3B5D0F(&m_queue, outCmd, sizeof(SessionCmd));
	if (ret < 0)
		return ret;
	return 0;
}

/* 0x813B1990 */
void SessionCmdQueue::Abort(void)
{
	if (!m_initialized)
		return;

	SceNpCommonPs4_3A4163FD(&m_queue, 3);
}

} /* namespace Push */
} /* namespace Np */
} /* namespace sce */
