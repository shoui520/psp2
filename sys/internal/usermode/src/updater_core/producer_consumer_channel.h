/*
 * ProducerConsumerChannel (producer_consumer_channel.cpp): a bounded queue of pointers between
 * two threads, under a lightweight mutex and condition, which either side can abort; and the
 * thread helpers the parallel operations start their consumer with. The class name is the
 * file's; the method names are ours but StartThread and JoinThread (the 0.995 build's).
 */
#ifndef UPDATER_CORE_PRODUCER_CONSUMER_CHANNEL_H
#define UPDATER_CORE_PRODUCER_CONSUMER_CHANNEL_H

#include <scetypes.h>
#include <kernel/threadmgr.h>
#include "updater_object.h"

class ProducerConsumerChannel : public UpdaterObject {
public:
	ProducerConsumerChannel(SceUInt32 capacity);
	virtual ~ProducerConsumerChannel();

	int Initialize(void **items, SceUInt32 count);
	int Push(void *item, bool wait);
	int Pop(void **item, bool wait);
	int Abort(void);

	bool IsAborted(void) const
	{
		return m_aborted;
	}

private:
	void **m_items;			/* the ring, capacity entries */
	SceUInt32 m_capacity;
	SceUInt32 m_count;
	SceUInt32 m_tail;		/* where the next push goes */
	SceUInt32 m_head;		/* where the next pop comes from */
	SceKernelLwMutexWork m_mutex;
	SceKernelLwCondWork m_cond;
	bool m_aborted;
};

int StartThread(SceKernelThreadEntry entry, void *argp, SceUID *threadId);
int JoinThread(SceUID threadId, SceInt32 *exitStatus);

#endif
