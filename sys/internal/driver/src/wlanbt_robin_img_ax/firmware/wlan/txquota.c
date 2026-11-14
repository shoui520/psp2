/*
 * Transmit buffer quotas (0x18814..0x189b4): the blocks of wlanBlockPool28
 * are shared by four transmit queues. Each queue has a limit, and blocks
 * reserved for it that the others may not take; a buffer remembers the
 * queue it is charged to, and its release callback gives the block back.
 * The names are inferred from use.
 */
#include <stdint.h>
#include "robin/btrom.h"
#include "robin/buffer.h"
#include "robin/sys.h"
#include "robin/txring.h"

/* The reserved blocks and the limit of each queue. */
struct WlanTxQuotaConfig {
	uint32_t reserved[WLAN_TX_QUOTA_QUEUES];
	uint32_t limit[WLAN_TX_QUOTA_QUEUES];
};

/* The blocks each queue holds, and the queue each block of the pool is charged to. */
struct WlanTxQuotaState {
	uint32_t used[WLAN_TX_QUOTA_QUEUES];
	uint8_t blockQueue[WLAN_TX_QUOTA_BLOCKS];
};

#define WLAN_TX_BUFFER_QUOTA	0x01	/* OsaBuffer.flags: charged to a queue */

static struct WlanTxQuotaConfig wlanTxQuotaConfig __attribute__((section(".dtcm.rw"))) = {	/* 0x04000f10 */
	.reserved = { 4, 0, 0, 2 },
	.limit = { WLAN_TX_QUOTA_BLOCKS, WLAN_TX_QUOTA_BLOCKS, WLAN_TX_QUOTA_BLOCKS,
		   WLAN_TX_QUOTA_BLOCKS },
};
static struct WlanTxQuotaState wlanTxQuota __attribute__((section(".dtcm.bss")));	/* 0x040087c0 */

int wlanTxQuotaSet(unsigned int queue, uint32_t reserved, uint32_t limit)
{
	if (reserved > limit)
		reserved = limit;
	wlanTxQuotaConfig.limit[queue] = limit;
	wlanTxQuotaConfig.reserved[queue] = reserved;
	return 1;
}

/*
 * The blocks the queue may still take: the pool's blocks less what every
 * queue holds or has reserved, and no more than the queue's own limit allows.
 */
static uint16_t wlanTxQuotaAvailable(struct OsaBufferPool *pool, unsigned int queue)
{
	uint16_t taken = 0;
	uint16_t blocks, available;
	uint32_t held;
	UINT posture;
	uint8_t i;

	posture = sysIrqDisable();
	blocks = osa_PoolCapacity(pool);
	for (i = 0; i < WLAN_TX_QUOTA_QUEUES; i++) {
		if (i == queue) {
			held = wlanTxQuota.used[i];
		} else {
			held = wlanTxQuota.used[i];
			if (held <= wlanTxQuotaConfig.reserved[i])
				held = wlanTxQuotaConfig.reserved[i];
		}
		taken += held;
	}
	available = blocks - taken;
	if (wlanTxQuotaConfig.limit[queue] - wlanTxQuota.used[queue] <= available)
		available = wlanTxQuotaConfig.limit[queue] - wlanTxQuota.used[queue];
	sysIrqRestore(posture);
	return available;
}

/* OsaBuffer.release of a charged buffer. */
int wlanTxQuotaRelease(OsaBuffer *buffer)
{
	if (buffer->pool == &wlanBlockPool28)
		wlanTxQuota.used[wlanTxQuota.blockQueue[buffer->blockIndex]]--;
	return 0;
}

/* A buffer of the pool; one of wlanBlockPool28 is charged to the queue, if it has room. */
OsaBuffer *wlanTxBufferAlloc(struct OsaBufferPool *pool, uint32_t size, uint32_t flags,
			     unsigned int queue)
{
	OsaBuffer *buffer = NULL;
	UINT posture;

	if (pool != &wlanBlockPool28)
		return osa_BufferAlloc(pool, size, flags);
	posture = sysIrqDisable();
	if (wlanTxQuotaAvailable(pool, queue) != 0) {
		buffer = osa_BufferAlloc(pool, size, flags);
		if (buffer != NULL) {
			wlanTxQuota.blockQueue[buffer->blockIndex] = queue;
			wlanTxQuota.used[queue]++;
			buffer->flags |= WLAN_TX_BUFFER_QUOTA;
			buffer->release = wlanTxQuotaRelease;
		}
	}
	sysIrqRestore(posture);
	return buffer;
}

unsigned int wlanTxBufferQueue(const OsaBuffer *buffer)
{
	return wlanTxQuota.blockQueue[buffer->blockIndex];
}

/*
 * Charges the buffer to another queue. Returns 0 when it is charged to the
 * queue, 1 when the queue has no room (or the buffer is not the pool's).
 */
int wlanTxBufferRecharge(OsaBuffer *buffer, unsigned int queue)
{
	unsigned int from;
	int result = 1;
	UINT posture;

	if (buffer->pool != &wlanBlockPool28)
		return 1;
	from = wlanTxQuota.blockQueue[buffer->blockIndex];
	if (from == queue)
		return 0;
	posture = sysIrqDisable();
	if (wlanTxQuota.used[queue] < wlanTxQuotaConfig.limit[queue] &&
	    (wlanTxQuota.used[from] > wlanTxQuotaConfig.reserved[from] ||
	     wlanTxQuotaAvailable(buffer->pool, queue) != 0)) {
		result = 0;
		if (wlanTxQuota.used[from] == 0)
			for (;;)
				;
		wlanTxQuota.used[from]--;
		wlanTxQuota.blockQueue[buffer->blockIndex] = queue;
		wlanTxQuota.used[queue]++;
	}
	sysIrqRestore(posture);
	return result;
}
