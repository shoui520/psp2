/*
 * The channel between a parallel operation's two threads (Sony's producer_consumer_channel.cpp):
 * a ring of pointers under a lightweight mutex, each push and pop signalling the condition the
 * other side waits on; and the helpers that start and join the consumer thread.
 */
#include <stddef.h>
#include <scetypes.h>
#include <kernel/threadmgr.h>
#include <libkernel/libkernel.h>
#include <updater_core/updater_core.h>
#include "updater_log.h"
#include "updater_memory.h"
#include "producer_consumer_channel.h"

#define UPDATER_FILE_ID		0xd2df751d

#define CHANNEL_ITEMS_ALIGN	512
#define SUB_THREAD_STACK_SIZE	0x4000

ProducerConsumerChannel::ProducerConsumerChannel(SceUInt32 capacity)
{
	m_capacity = capacity;
	m_items = NULL;
	m_count = 0;
	m_tail = 0;
	m_head = 0;
	m_aborted = false;
}

ProducerConsumerChannel::~ProducerConsumerChannel()
{
	sceKernelDeleteLwCond(&m_cond);
	sceKernelDeleteLwMutex(&m_mutex);
	if (m_items != NULL)
		g_updaterMemoryInfo.updaterCoreFree(m_items);
}

/* makes the mutex, the condition and the ring, which starts with count items */
int ProducerConsumerChannel::Initialize(void **items, SceUInt32 count)
{
	SceUInt32 i;
	int ret;

	if (count > m_capacity || (count != 0 && items == NULL))
		return SCE_UPDATER_ERROR_INVALID_ARGUMENT;
	ret = sceKernelCreateLwMutex(&m_mutex, "SceUpdaterCore", SCE_KERNEL_ATTR_TH_PRIO, 0, NULL);
	if (ret < 0)
		return ret;
	ret = sceKernelCreateLwCond(&m_cond, "SceUpdaterCore", SCE_KERNEL_ATTR_TH_PRIO, &m_mutex,
				    NULL);
	if (ret < 0)
		return ret;
	m_items = (void **)g_updaterMemoryInfo.updaterCoreMemalign(CHANNEL_ITEMS_ALIGN,
								   m_capacity * sizeof(void *));
	if (m_items == NULL)
		return SCE_UPDATER_ERROR_NO_MEMORY;
	for (i = 0; i < count; i++)
		m_items[i] = items[i];
	m_count = count;
	return 0;
}

/* adds an item, waiting for room if wait is set */
int ProducerConsumerChannel::Push(void *item, bool wait)
{
	SceUInt32 count;

	SceLibKernel_A7819967(&m_mutex, 1, NULL);
	while (m_count >= m_capacity) {
		if (m_aborted) {
			SceLibKernel_499EA781(&m_mutex, 1);
			return SCE_UPDATER_ERROR_ABORTED;
		}
		if (!wait) {
			SceLibKernel_499EA781(&m_mutex, 1);
			return SCE_UPDATER_ERROR_CHANNEL_FULL;
		}
		sceKernelWaitLwCond(&m_cond, NULL);
	}
	m_items[m_tail++] = item;
	count = m_count;
	if (m_tail >= m_capacity)
		m_tail = 0;
	m_count = count + 1;
	sceKernelSignalLwCondAll(&m_cond);
	SceLibKernel_499EA781(&m_mutex, 1);
	return 0;
}

/* takes the oldest item, waiting for one if wait is set */
int ProducerConsumerChannel::Pop(void **item, bool wait)
{
	SceUInt32 count;

	if (item == NULL)
		return SCE_UPDATER_ERROR_INVALID_ARGUMENT;
	SceLibKernel_A7819967(&m_mutex, 1, NULL);
	while (m_count == 0) {
		if (m_aborted) {
			SceLibKernel_499EA781(&m_mutex, 1);
			return SCE_UPDATER_ERROR_ABORTED;
		}
		if (!wait) {
			SceLibKernel_499EA781(&m_mutex, 1);
			return SCE_UPDATER_ERROR_CHANNEL_EMPTY;
		}
		sceKernelWaitLwCond(&m_cond, NULL);
	}
	*item = m_items[m_head++];
	count = m_count;
	if (m_head >= m_capacity)
		m_head = 0;
	m_count = count - 1;
	sceKernelSignalLwCondAll(&m_cond);
	SceLibKernel_499EA781(&m_mutex, 1);
	return 0;
}

/* makes every push and pop, waiting or not, fail */
int ProducerConsumerChannel::Abort(void)
{
	SceLibKernel_A7819967(&m_mutex, 1, NULL);
	m_aborted = true;
	sceKernelSignalLwCondAll(&m_cond);
	return SceLibKernel_499EA781(&m_mutex, 1);
}

int StartThread(SceKernelThreadEntry entry, void *argp, SceUID *threadId)
{
	SceUID thid;
	int ret;

	thid = sceKernelCreateThread("SceUpdaterSub", entry, 0, SUB_THREAD_STACK_SIZE, 0, 0, NULL);
	*threadId = thid;
	if (thid < 0)
		return thid;
	ret = sceKernelStartThread(thid, sizeof(void *), argp);
	if (ret < 0) {
		UPDATER_ERROR_FMT(209, "%x", ret);
		return ret;
	}
	return 0;
}

int JoinThread(SceUID threadId, SceInt32 *exitStatus)
{
	int ret;

	ret = sceKernelWaitThreadEnd(threadId, exitStatus, NULL);
	if (ret < 0)
		UPDATER_ERROR_FMT(219, "%x", ret);
	return sceKernelDeleteThread(threadId);
}
