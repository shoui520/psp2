/*
 * The send queue: a ring of five requests ("SceCompanionUtil::CTcpSendJobQueueMutex" guards
 * it) the send thread takes one by one, building and sending each request's packet and then
 * freeing its parameter.
 */
#include <libkernel/libkernel.h>
#include <kernel/threadmgr.h>
#include "companion_util_internal.h"

namespace SceCompanionUtil {

#define QUEUE_LENGTH	5
#define QUEUE_NONE	31

static SceKernelLwMutexWork s_queueMutex;
static bool s_queueMutexCreated;

int CTcpSendJobQueue::CreateMutex(void)
{
	int ret;

	if (s_queueMutexCreated)
		return -1;
	ret = sceKernelCreateLwMutex(&s_queueMutex, "SceCompanionUtil::CTcpSendJobQueueMutex",
				     SCE_KERNEL_LW_MUTEX_ATTR_RECURSIVE, 0, NULL);
	if (ret >= 0) {
		s_queueMutexCreated = true;
		return 0;
	}
	return ret;
}

int CTcpSendJobQueue::DeleteMutex(void)
{
	int ret;

	if (!s_queueMutexCreated)
		return -1;
	ret = sceKernelDeleteLwMutex(&s_queueMutex);
	if (ret >= 0) {
		s_queueMutexCreated = false;
		return 0;
	}
	return ret;
}

void CTcpSendJobQueue::Flush(void)
{
	CompanionUtilParamRelease release;
	CompanionUtilSendEntry *entry;
	void *param;
	int ret;

	for (;;) {
		ret = 0;
		sceKernelLockLwMutex(&s_queueMutex, 1, NULL);
		if (m_count != 0) {
			entry = &m_entries[m_head];
			param = entry->param;
			release = entry->release;
			if (++m_head >= QUEUE_LENGTH)
				m_head -= QUEUE_LENGTH;
			m_count--;
		} else {
			ret = -1;
		}
		sceKernelUnlockLwMutex(&s_queueMutex, 1);
		if (ret < 0)
			break;
		if (release != NULL)
			release(param);
	}
}

void CTcpSendJobQueue::Init(void)
{
	int i;

	sceKernelLockLwMutex(&s_queueMutex, 1, NULL);
	m_head = 0;
	m_count = 0;
	m_peak = 0;
	for (i = 0; i < QUEUE_LENGTH; i++) {
		m_entries[i].type = QUEUE_NONE;
		m_entries[i].param = NULL;
		m_entries[i].release = NULL;
	}
	sceKernelUnlockLwMutex(&s_queueMutex, 1);
	m_job.m_state = JOB_IDLE;
	m_job.m_abort = false;
	m_job.m_type = 0;
	m_job.m_result = -1;
	m_job.m_sending = false;
	m_job.m_packet = NULL;
	m_job.m_lastResult = -1;
}

void CTcpSendJobQueue::Term(void)
{
	if (m_job.m_packet != NULL) {
		Free(m_job.m_packet);
		m_job.m_packet = NULL;
	}
	Flush();
}

void CTcpSendJobQueue::Run(bool force)
{
	CompanionUtilParamRelease release;
	CompanionUtilSendEntry *entry;
	void *param;
	int type, result;
	int lock, ret;

	for (;;) {
		type = QUEUE_NONE;
		sceKernelLockLwMutex(&s_queueMutex, 1, NULL);
		if (m_count != 0) {
			entry = &m_entries[m_head];
			type = entry->type;
			param = entry->param;
			release = entry->release;
			if (++m_head >= QUEUE_LENGTH)
				m_head -= QUEUE_LENGTH;
			m_count--;
			ret = 0;
		} else {
			release = NULL;
			param = NULL;
			ret = -1;
		}
		sceKernelUnlockLwMutex(&s_queueMutex, 1);
		if (ret < 0)
			break;

		if (force || !m_job.m_abort) {
			if (m_job.m_state != JOB_RUNNING && !m_job.m_sending) {
				m_job.m_type = type;
				m_job.m_packet =
					(CompanionUtilPacket *)Allocate(COMPANION_UTIL_BUFFER_SIZE);
				sceClibMemset(m_job.m_packet, 0, COMPANION_UTIL_BUFFER_SIZE);
				m_job.BuildPacket(m_job.m_packet, (CompanionUtilRequestParam *)param);
				m_job.m_state = JOB_RUNNING;
			}
			result = m_job.Send();
			if (m_job.m_packet != NULL) {
				Free(m_job.m_packet);
				m_job.m_packet = NULL;
			}
			lock = LockMutex();
			m_job.m_result = result;
			m_job.m_state = JOB_DONE;
			UnlockMutex(lock);
			m_job.m_sending = false;
		}
		if (release != NULL)
			release(param);
	}
}

void CTcpSendJobQueue::Abort(void)
{
	m_job.m_abort = true;
}

int CTcpSendJobQueue::GetCount(void)
{
	int count;

	sceKernelLockLwMutex(&s_queueMutex, 1, NULL);
	count = m_count;
	sceKernelUnlockLwMutex(&s_queueMutex, 1);
	return count;
}

int CTcpSendJobQueue::Push(int type, void *param, CompanionUtilParamRelease release)
{
	CompanionUtilSendEntry *entry;
	int count, index;
	int ret = 0;

	sceKernelLockLwMutex(&s_queueMutex, 1, NULL);
	count = m_count;
	if (count == QUEUE_LENGTH) {
		ret = -1;
	} else {
		index = m_head + count;
		if (index >= QUEUE_LENGTH)
			index -= QUEUE_LENGTH;
		entry = &m_entries[index];
		entry->type = type;
		entry->param = param;
		entry->release = release;
		count++;
		m_count = count;
		if (m_peak < count)
			m_peak = count;
	}
	sceKernelUnlockLwMutex(&s_queueMutex, 1);
	return ret;
}

} /* namespace SceCompanionUtil */
