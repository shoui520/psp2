/*
 * The context manager ("CTXT:"): the request running, and the cancel, terminate and pause
 * requests beside it, under the contexts' mutex.
 */
#include <stddef.h>
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <kernel/threadmgr.h>
#include <common/shared_ptr.h>
#include "mtp_internal.h"

using paf::common::SharedPtr;


MtpContextManager::MtpContextManager() : m_mutex(0), m_canceling(0)
{
}

MtpContextManager::~MtpContextManager()
{
}

int MtpContextManager::initialize()
{
	SceUID mutex;

	mutex = sceKernelCreateMutex("SceMtpContext", 0, 0, NULL);
	if (mutex < 0) {
		MTP_ERR(MTP_TAG_CTXT, 32, "sceKernelCreateMutex failed: 0x%x\n", mutex);
		return mutex;
	}
	m_mutex = mutex;
	return 0;
}

int MtpContextManager::finalize()
{
	int ret = 0;

	if (m_mutex != 0)
		ret = sceKernelDeleteMutex(m_mutex);
	return ret;
}

int MtpContextManager::lock()
{
	int ret;

	ret = sceKernelLockMutex(m_mutex, 1, NULL);
	if (ret < 0)
		MTP_ERR(MTP_TAG_CTXT, 56, "sceKernelLockMutext failed: 0x%x\n", ret);
	return ret;
}

int MtpContextManager::unlock()
{
	int ret;

	ret = sceKernelUnlockMutex(m_mutex, 1);
	if (ret < 0)
		MTP_ERR(MTP_TAG_CTXT, 65, "sceKernelUnlockMutex failed: 0x%x\n", ret);
	return ret;
}

/* hold a request: the cancel, terminate and pause requests beside the one running */
void MtpContextManager::registerContext(SharedPtr<MtpContext> context)
{
	if (context.get() == NULL)
		return;
	switch (context->opcode) {
	case MTP_OPERATION_PAUSE_EXPORT_OBJECT:
		m_pause = context;
		break;
	case MTP_OPERATION_TERMINATE:
		m_terminate = context;
		break;
	case MTP_OPERATION_CANCEL:
		m_cancel = context;
		break;
	default:
		m_current = context;
		break;
	}
}

void MtpContextManager::setCanceling(int canceling)
{
	m_canceling = canceling;
}

int MtpContextManager::isCanceling()
{
	return m_canceling;
}
