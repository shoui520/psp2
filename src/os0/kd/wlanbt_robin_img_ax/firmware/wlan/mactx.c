/*
 * Transmit-path helpers of the MAC: window queues, event posting, statistics
 * and the 802.11 data header. The filename is inferred; no source filename
 * survives.
 */
#include "robin/btrom.h"
#include "robin/mactx.h"
#include "robin/ieee80211.h"
#include "robin/wlanrx.h"
#include "robin/rom.h"
#include "robin/runtime.h"
#include "robin/soc.h"
#include "robin/sync.h"
#include "robin/sys.h"
#include "robin/wlantask.h"

struct WlanTxWindowQueue wlanTxWindowQueues[2] __attribute__((section(".dtcm.bss")));
uint32_t wlanTxWindowFlags __attribute__((section(".itcm.bss")));
/* One bit per user that needs the MAC awake; the MAC may sleep when none is set. */
uint32_t macTxWakeBits __attribute__((section(".itcm.bss")));
/* Time the last macWaitBasebandStable() took. */
uint64_t macWaitElapsed __attribute__((section(".itcm.bss")));
struct MacTxQueueStats macTxQueueStats[4] __attribute__((section(".dtcm.bss")));
uint16_t wlanSessionId __attribute__((section(".itcm.bss")));
struct WlanLocalState *wlanLocalState __attribute__((section(".itcm.bss")));
uint32_t macTxMaxWait __attribute__((section(".itcm.bss")));
uint8_t wlanSlotContext __attribute__((section(".sqram1.rw")));
static uint32_t wlanSlotTable __attribute__((section(".sqram1.rw")));
static uint32_t wlanSlotBuffer __attribute__((section(".sqram1.rw")));

/* Hardware counters read through the ROM (queue number in, running count out). */
int romMacStatValidate(unsigned int queue, int a, int b, int c);	/* 0x03f3acdb */
int romMacStatPrepare(unsigned int queue);				/* 0x03f3ace7 */
int romMacStatCheck(unsigned int queue);				/* 0x03f3accf */


/* Called as the MAC is first woken and when the last wake bit is released. */
void romMacWakeRequest(void);						/* 0x03f3a917 */
void romMacSleepNotify(int waited);					/* 0x03f3a907 */

/* Debug statistics hook (a no-op in this firmware). */
void sysLogStatistic(unsigned int id, uint32_t value, uint32_t extra);

/* Statistics of macWaitBasebandStable() for the wake sequence. */
static struct {
	uint32_t iterations;
	uint32_t maxIterations;
	uint32_t acknowledged;
	uint32_t wakeCount;
	uint32_t maxElapsed;
	uint32_t meanElapsed;
	uint32_t samples;
	uint32_t failures;
} macWakeStats __attribute__((section(".itcm.bss")));

/*
 * Clears the given pending bits in the MAC and spins until the hardware
 * reports them clear, at most limit polls. Returns the polls left.
 */
int macWaitPendingClear(unsigned int mask, unsigned int limit)
{
	MAC_REG(0x254) &= ~mask;
	while ((MAC_REG(0x258) & mask) && limit)
		--limit;
	return limit;
}

/*
 * Requests the MAC and baseband to stay awake for the given event bits and
 * samples the baseband status until it has read idle for `samples` polls in
 * a row. *iterations receives the number of polls. Returns 1 when some of the
 * requested bits were not set in the MAC's acknowledgement register at the
 * end. The time spent is kept in macWaitElapsed.
 */
int macWaitBasebandStable(uint32_t mask, uint32_t *iterations, uint32_t samples)
{
	uint32_t stable = 0;
	uint32_t count = 0;
	uint32_t previous = 0x12345;
	uint32_t status;
	uint32_t acknowledged;
	int result = 0;
	uint64_t start;

	MAC_REG(0x254) |= mask | 0x80000000u;
	MAC_REG(0x2d0) |= 0x70000;
	MAC_REG(0x300) &= ~8u;
	SOC_MAC_EVENT_CONTROL &= ~8u;
	acknowledged = MAC_REG(0x258);
	start = osaGetHardwareTime();
	while (stable < samples) {
		++count;
		acknowledged = MAC_REG(0x258);
		status = MAC_REG(0x890) & 0xfe00000;
		if ((status == 0 || status == 0x200000) && previous == status)
			++stable;
		else
			stable = 0;
		previous = status;
	}
	macWaitElapsed = osaGetHardwareTime() - start;
	if (mask & ~acknowledged)
		result = 1;
	SOC_MAC_EVENT_CONTROL |= 8;
	MAC_REG(0x300) |= 8;
	MAC_REG(0x2d0) &= ~0x70000u;
	*iterations = count;
	return result;
}

/* Sets bits in the event flags of the "MAC Tx" task. */
int macTxPostEvent(uint32_t events)
{
	osaEventFlagsSet(macTxEventFlags, events, 7);
	return 0;
}

int macTxWakeup(void)
{
	return macTxPostEvent(0x20);
}

/* Remembers the longest time macWaitPendingClear() has taken. */
int macTxMeasureWait(unsigned int mask)
{
	uint64_t start = osaGetHardwareTime();
	uint64_t end;
	uint32_t elapsed;
	int result;

	macWaitPendingClear(mask, 0xffff);
	end = osaGetHardwareTime();
	elapsed = (uint32_t)(end - start);
	result = macTxMaxWait - elapsed;
	if (macTxMaxWait < elapsed)
		macTxMaxWait = elapsed;
	return result;
}

/* Runs one baseband wait for the wake sequence and keeps statistics about it. */
void macWakeSettle(uint32_t mask)
{
	uint64_t start;
	uint64_t elapsed;

	++macWakeStats.wakeCount;
	start = osaGetHardwareTime();
	if (macWaitBasebandStable(mask, &macWakeStats.iterations, 30) != 0)
		++macWakeStats.failures;
	macWakeStats.acknowledged = MAC_REG(0x258);
	elapsed = osaGetHardwareTime() - start;
	macWakeStats.meanElapsed = ((uint64_t)(macWakeStats.meanElapsed * macWakeStats.samples) +
				    elapsed) / (macWakeStats.samples + 1);
	++macWakeStats.samples;
	if ((uint64_t)macWakeStats.maxElapsed < elapsed)
		macWakeStats.maxElapsed = (uint32_t)elapsed;
	if (macWakeStats.iterations > macWakeStats.maxIterations)
		macWakeStats.maxIterations = macWakeStats.iterations;
	sysLogStatistic(0x11, macWakeStats.maxElapsed, macWakeStats.failures);
}

/* Takes the lowest free wake bit (0 when all 32 are taken); the first user wakes the MAC. */
uint32_t macTxAcquireWakeBit(void)
{
	uint32_t acquired = 0;
	int wasEmpty = macTxWakeBits == 0;
	unsigned int posture = sysIrqDisable();
	int index = 0;

	do {
		uint32_t bit = 1u << index;

		if (!(bit & macTxWakeBits)) {
			acquired = bit;
			macTxWakeBits |= bit;
		}
		++index;
	} while (acquired == 0 && index <= 31);
	if (wasEmpty) {
		romMacWakeRequest();
		macWakeSettle(0x3ff);
		macTxMeasureWait(0x390);
	}
	sysIrqRestore(posture);
	return acquired;
}

/* Pool the transmit frames of the management tasks come from; created by bufferPoolsInit(). */
extern struct OsaBufferPool wlanTxFramePool;

/*
 * Allocates a buffer for a frame to be sent and sets up its TX descriptor
 * at buffer + 32. Returns 0 when the pool is empty.
 */
int wlanTxAllocFrame(OsaBuffer **buffer, WlanTxDescriptor **descriptor, uint32_t size,
		     uint16_t priority)
{
	OsaBuffer *allocated = osa_BufferAlloc(&wlanTxFramePool, size, 0);

	*buffer = allocated;
	if (allocated == NULL)
		return 0;
	allocated->packetClass = 2;
	allocated->dataOffset = 32;
	*descriptor = (WlanTxDescriptor *)(allocated->data + 4);
	(*descriptor)->control = 1;
	(*descriptor)->priority = priority;
	(*descriptor)->flags = priority >> 8;
	return 1;
}

/*
 * Allocates a frame of type 2 for the command path. A non-zero release makes
 * the buffer call it when it is freed. Returns the buffer, 0 if none was free.
 */
OsaBuffer *wlanTxAllocFrameType2(uint16_t priority, int (*release)(OsaBuffer *buffer))
{
	OsaBuffer *buffer;
	WlanTxDescriptor *descriptor;

	if (wlanTxAllocFrame(&buffer, &descriptor, 100, priority) != 0) {
		((uint8_t *)(descriptor + 1))[0] = 0;
		((uint8_t *)(descriptor + 1))[1] = 0;
		if (release != NULL) {
			buffer->flags |= 1;
			buffer->release = release;
		}
		descriptor->control = (descriptor->control & 0xffff) | (2u << 16);
	}
	return buffer;
}

/* Gives up the wake bits in *bits; the last one out lets the MAC go to sleep. */
void macTxReleaseWakeBits(uint32_t *bits)
{
	unsigned int posture = sysIrqDisable();

	macTxWakeBits &= ~*bits;
	*bits = 0;
	if (macTxWakeBits == 0) {
		romMacSleepNotify(macTxMeasureWait(111));
		macTxPostEvent(1);
	}
	sysIrqRestore(posture);
}

/* True when a session exists and has reached stage 4. */
int macTxPathReady(void)
{
	if (wlanSessionId == 0)
		return 0;
	return wlanConnectionStage == 4;
}

void macTxAdvanceStage(void)
{
	if (wlanSessionId != 0 && wlanConnectionStage == 3)
		wlanConnectionStage = 4;
}

/* Takes the oldest frame off a window queue; returns 1 when it is empty. */
int macTxDequeue(unsigned int window, uint32_t *entry)
{
	struct WlanTxWindowQueue *queue = &wlanTxWindowQueues[window];

	if (queue->tail == queue->head)
		return 1;
	*entry = queue->entries[queue->head];
	queue->head = (queue->head + 1) & 0x1f;
	--queue->count;
	return 0;
}

/* Completes every frame still queued in window 5 or 6 and clears its state. */
void wlanTxWindowReset(unsigned int window)
{
	unsigned int index = window - 5;
	uint32_t entry;
	unsigned int posture;

	if (index >= 2)
		return;
	posture = sysIrqDisable();
	if ((WLAN_TX_WINDOW_DRAINING << index) & wlanTxWindowFlags) {
		while (macTxDequeue(index, &entry) == 0)
			wlanCompletePacket((OsaBuffer *)(entry - offsetof(OsaBuffer, data)), 0);
	}
	wlanTxWindowFlags &= ~((WLAN_TX_WINDOW_DRAINING | WLAN_TX_WINDOW_ENABLED |
				WLAN_TX_WINDOW_OPEN) << index);
	__aeabi_memclr4(&wlanTxWindowQueues[index], sizeof(wlanTxWindowQueues[0]));
	sysIrqRestore(posture);
}

/* Forgets the frames in flight in both windows; draining windows are re-checked. */
void wlanTxWindowsClearPending(void)
{
	int index;

	for (index = 0; index < 2; ++index) {
		wlanTxWindowQueues[index].pending = 0;
		if ((WLAN_TX_WINDOW_DRAINING << index) & wlanTxWindowFlags)
			macTxPostEvent(1);
	}
}

/* One frame of window 5 or 6 was completed; a draining window wakes the task at zero. */
void wlanTxWindowComplete(unsigned int window)
{
	uint32_t flags;
	struct WlanTxWindowQueue *queue;

	if (window != 5 && window != 6)
		return;
	window -= 5;
	flags = wlanTxWindowFlags;
	if (!(flags & (WLAN_TX_WINDOW_OPEN << window)))
		return;
	queue = &wlanTxWindowQueues[window];
	if (queue->pending != 0)
		--queue->pending;
	if ((flags & (WLAN_TX_WINDOW_DRAINING << window)) && queue->pending == 0)
		macTxPostEvent(1);
}

/* Hands a message to the "MAC Mgmt" task; returns 1 if its queue is full. */
int macTxQueueMgmtMessage(uintptr_t message)
{
	if (osaMsgQueueSend(macTxMgmtQueue, 4, message, 0) != 0)
		return 1;
	macTxPostEvent(2);
	return 0;
}

int macTxResetQueues(void)
{
	__aeabi_memclr4(wlanTxWindowQueues, sizeof(wlanTxWindowQueues[0]));
	wlanTxWindowFlags = 0;
	return 0;
}

/* Empties the queue of window 5 or 6 and marks it active. */
unsigned int macTxActivateWindow(unsigned int window)
{
	unsigned int index = window - 5;

	if ((uint8_t)index < 2) {
		__aeabi_memclr4(&wlanTxWindowQueues[(uint8_t)index], sizeof(wlanTxWindowQueues[0]));
		wlanTxWindowFlags = ((16 << (uint8_t)index) | (1 << (uint8_t)index) |
				     wlanTxWindowFlags) & ~(256 << (uint8_t)index);
		return 256 << (uint8_t)index;
	}
	return index;
}

int macTxGetContext(void)
{
	return wlanSlotContext;
}

void macTxSetContext(int value)
{
	wlanSlotContext = value;
}

void macTxClearSlotTable(void)
{
	wlanSlotTable = 0;
}

void macTxClearSlotBuffer(void)
{
	wlanSlotBuffer = 0;
}

/* Power-save mode of the local station, mirrored into a MAC register. */
void macTxSetPowerSave(int mode)
{
	wlanLocalState->powerSave = mode;
	MAC_REG(0x654) = mode;
}

int macTxGetPowerSave(void)
{
	return wlanLocalState->powerSave;
}

/* Clears the software counters of a queue and the hardware counters behind them. */
void macTxStatsReset(unsigned int queue)
{
	macTxQueueStats[queue].prepared = 0;
	macTxQueueStats[queue].checked = 0;
	macTxQueueStats[queue].validated = 0;
	romMacStatValidate(queue, 0, 0, 0);
	romMacStatPrepare(queue);
	romMacStatCheck(queue);
}

/* Adds the hardware count to the software one and returns the change since *last. */
int macTxStatsAddPrepared(unsigned int queue, int *last)
{
	int total = romMacStatPrepare(queue) + macTxQueueStats[queue].prepared;
	int previous = *last;

	macTxQueueStats[queue].prepared = total;
	*last = total;
	return total - previous;
}

int macTxStatsAddValidated(unsigned int queue, int *last)
{
	int total = romMacStatValidate(queue, 0, 0, 0) + macTxQueueStats[queue].validated;
	int previous = *last;

	macTxQueueStats[queue].validated = total;
	*last = total;
	return total - previous;
}

int macTxStatsAddChecked(unsigned int queue, int *last)
{
	int total = romMacStatCheck(queue) + macTxQueueStats[queue].checked;
	int previous = *last;

	macTxQueueStats[queue].checked = total;
	*last = total;
	return total - previous;
}

/* Signal-to-noise value limited to 0..102. */
int8_t wlanClampSnr(int value)
{
	if (value > 102)
		return 102;
	if (value < 0)
		return 0;
	return value;
}

/* Noise floor in dBm limited to -102..0, reported as a positive number. */
int wlanClampNoise(int value)
{
	if (value > 0)
		value = 0;
	else if (value < -102)
		value = -102;
	return -(int8_t)value;
}

/*
 * Fills the 802.11 header of a data frame to be sent for a connection: the
 * frame control flags and the addresses depend on the interface type and role.
 * addresses holds the destination and then the source address of the frame.
 */
void macTxBuildDataHeader(struct WlanConnection *connection, WlanTxDescriptor *descriptor,
			  WlanWirelessFrame *frame, const uint8_t *addresses, int qos)
{
	uint8_t address4[6] = { 0 };
	unsigned int packetType = descriptor->header.wire.packetType[0] |
				  (descriptor->header.wire.packetType[1] << 8);
	uint16_t control;
	const uint8_t *third;

	control = wlanDataFrameControl(qos, descriptor->flags & 1, packetType == 233);
	if (packetType == 234)
		descriptor->flags |= 8;
	if (connection->interfaceType == WLAN_IF_IBSS) {
		wlanSetHeaderAddresses(frame, addresses, connection->interfaceAddress,
				       connection->localAddress);
	} else {
		if (connection->interfaceType == WLAN_IF_AMP) {
			/* Four-address frame between the two AMP ends. */
			__aeabi_memcpy(frame->address1, addresses, 6);
			__aeabi_memcpy(frame->address2, connection->interfaceAddress, 6);
			__aeabi_memcpy(frame->address3, frame->address1, 6);
			__aeabi_memcpy(address4, frame->address2, 6);
			control |= (IEEE80211_FC1_TODS | IEEE80211_FC1_FROMDS) << 8;
			goto store;
		}
		if (connection->mode == 2) {
			wlanSetHeaderAddresses(frame, addresses, connection->localAddress,
					       addresses + 6);
			control |= IEEE80211_FC1_FROMDS << 8;
			goto store;
		}
		third = packetType == 230 ? connection->localAddress : addresses;
		wlanSetHeaderAddresses(frame, connection->localAddress,
				       connection->interfaceAddress, third);
		control |= IEEE80211_FC1_TODS << 8;
	}
	if (wlanLocalState->powerSave)
		control |= IEEE80211_FC1_PWR_MGT << 8;
store:
	frame->frameControl[0] = control;
	frame->frameControl[1] = control >> 8;
	__aeabi_memcpy(frame->address4, address4, 6);
}
