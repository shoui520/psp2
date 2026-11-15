/*
 * The frames an IBSS holds back for peers in power save (0x3b7d6..0x3c1ec). A frame for a peer
 * that sleeps is put in a list (wlanIbssTxFrame()); the machine below announces the peer with an
 * ATIM frame (macMgmtHandleDescriptor()) and sends the list once the window is over. The names of
 * the firmware's own placeholders are kept, see robin/txhold.h.
 */
#include <stddef.h>
#include "robin/btrom.h"
#include "robin/coex.h"
#include "robin/mac.h"
#include "robin/mactx.h"
#include "robin/mef.h"
#include "robin/mgmt.h"
#include "robin/psctl.h"
#include "robin/rom.h"
#include "robin/runtime.h"
#include "robin/sleep.h"
#include "robin/sync.h"
#include "robin/sys.h"
#include "robin/task.h"
#include "robin/txhold.h"
#include "robin/txring.h"
#include "robin/wlanrx.h"

/* Not converted yet. */
void fmCoexResetBuffer(void);							/* 0x4c922 */

_Static_assert(offsetof(struct WlanConnection, interfaceAddress) == 732, "interface address");

static struct WlanTxHold wlanTxHold __attribute__((section(".itcm.bss")));			/* 0x59384 */
static struct OsaSemaphore *wlanTxHoldItemsLock __attribute__((section(".itcm.rw")));	/* 0x551d4 */
static struct OsaSemaphore *wlanTxHoldListsLock __attribute__((section(".itcm.rw")));	/* 0x551d8 */
static struct WlanTxHoldItem *wlanTxHoldCursor __attribute__((section(".itcm.rw")));	/* 0x551d0 */
static uint32_t wlanTxHoldItemsLockStorage[7] __attribute__((section(".itcm.bss")));	/* 0x593d8 */
static uint32_t wlanTxHoldListsLockStorage[7] __attribute__((section(".itcm.bss")));	/* 0x593f4 */
static struct WlanTxHoldList wlanTxHoldGroup __attribute__((section(".itcm.bss")));	/* 0x59410 */
static struct WlanTxHoldList wlanTxHoldSlot __attribute__((section(".itcm.bss")));	/* 0x5941c */
static struct WlanTxHoldItem wlanTxHoldItems[WLAN_TX_HOLD_ITEMS] __attribute__((section(".itcm.bss"))); /* 0x59428 */
static uint32_t wlanTxHoldFreeCount __attribute__((section(".itcm.bss")));		/* 0x595e8 */
static struct WlanTxHoldItem *wlanTxHoldFree __attribute__((section(".itcm.bss")));	/* 0x595ec */
struct WlanTxHoldMachine wlanTxHoldMachine __attribute__((section(".itcm.bss")));	/* 0x595f0 */

uint32_t wlanPsWakeCycle __attribute__((section(".itcm.rw")));				/* 0x562b4 */

/* The ROM's time and link routines the table of the window uses. */
uint64_t romLibSelectState(void);							/* 0x03f3a6ec */
int romLibAdjustState(uint64_t time, uint32_t period);					/* 0x03f3a700 */
int romLibApplyContext(unsigned int count);						/* 0x03f3a94e */
uint64_t __aeabi_uldivmod(uint64_t numerator, uint64_t denominator);

static inline __attribute__((always_inline)) WlanWirelessFrame *wlanTxHoldFrame(OsaBuffer *buffer)
{
	return (WlanWirelessFrame *)TX_DATA_FRAME((uint8_t *)buffer + buffer->dataOffset);
}

/* The frame's destination: the first address. */
static inline __attribute__((always_inline)) const uint8_t *wlanTxHoldDestination(OsaBuffer *buffer)
{
	return wlanTxHoldFrame(buffer)->address1;
}

/* Moves the list other to the end of list; other is empty afterwards. */
int macMgmtAcquireHeader(struct WlanTxHoldList *list, struct WlanTxHoldList *other)
{
	if (list->tail == NULL) {
		list->head = other->head;
		list->tail = other->tail;
		list->groupCount = other->groupCount;
		list->unicastCount = other->unicastCount;
	} else if (other->groupCount + other->unicastCount != 0) {
		list->tail->next = other->head;
		list->tail = other->tail;
		list->groupCount += other->groupCount;
		list->unicastCount += other->unicastCount;
	}
	other->groupCount = 0;
	other->unicastCount = 0;
	other->head = NULL;
	other->tail = NULL;
	return 0;
}

/* Gives an item back. */
__attribute__((noinline)) int macMgmtReleaseHeader(struct WlanTxHoldItem *item)
{
	osaSemaphoreWait(wlanTxHoldItemsLock, 0xffffffff);
	if (item != NULL) {
		item->frame = NULL;
		item->mode = WLAN_TX_HOLD_FREE;
		item->group = 0;
		if (wlanTxHoldFreeCount != 0) {
			item->next = wlanTxHoldFree;
			wlanTxHoldFree = item;
		} else {
			wlanTxHoldFree = item;
			item->next = NULL;
		}
		++wlanTxHoldFreeCount;
	}
	return osaSemaphoreRelease(wlanTxHoldItemsLock);
}

/* Puts a frame in a list; 0 when no item is free. */
int macMgmtSetupHeader(struct WlanTxHoldList *list, OsaBuffer *frame, uint8_t mode)
{
	struct WlanTxHoldItem *item;
	const uint8_t *address;

	if (wlanPsUpdateBuffer(&item) == 0)
		return 0;
	item->frame = frame;
	item->mode = mode;
	address = wlanTxHoldDestination(frame);
	if ((((const uint16_t *)address)[0] == 0xffff && ((const uint16_t *)address)[1] == 0xffff &&
	     ((const uint16_t *)address)[2] == 0xffff) || (address[0] & 1))
		item->group = 1;
	wlanPsProcessBuffer(list, item);
	return 1;
}

/* Whether power save may start: nothing is held and the window has run its course. */
int macMgmtClearHeader(void)
{
	if (wlanPsDtimPolicy != 0xffff) {
		if (!wlanPsEnabled() || wlanIbssPsActive != 0 || wlanTxHold.held != 0 ||
		    wlanTxHold.drainAll != 0)
			return 1;
		if (wlanTxHold.psActive != 0) {
			++wlanTxHold.windowCount;
			if (wlanTxHold.windowDivisor != 255 &&
			    wlanTxHold.windowCount % wlanTxHold.windowDivisor == 0)
				return 1;
		} else if (wlanTxHold.waiting == 0) {
			return 1;
		}
	}
	return 0;
}

int macMgmtConvertHeader(void)
{
	wlanTxHold.drainAll = 1;
	return 1;
}

int macMgmtVerifyHeader(void)
{
	wlanTxHold.waiting = 1;
	return 1;
}

/* Whether the peer was announced in this window. */
int macMgmtSelectHeader(const void *address)
{
	unsigned int i;

	for (i = 0; wlanTxHold.peerCount > i; ++i) {
		if (memcmp(address, wlanTxHold.peers[i], 6) == 0)
			return 1;
	}
	return 0;
}

void macMgmtAdjustHeader(void)
{
	MAC_REG(0x634) = 2000;
}

/* Sets the lists up: the locks, and every item on the free list. */
void macMgmtRecordHeader(void)
{
	int i;

	wlanTxHoldItemsLock = (struct OsaSemaphore *)wlanTxHoldItemsLockStorage;
	wlanTxHoldListsLock = (struct OsaSemaphore *)wlanTxHoldListsLockStorage;
	osaSemaphoreInit(&wlanTxHoldItemsLock, 1, 12);
	osaSemaphoreInit(&wlanTxHoldListsLock, 1, 12);
	wlanTxHoldFreeCount = 0;
	i = 0;
	do
		macMgmtReleaseHeader(&wlanTxHoldItems[i++]);
	while (i < WLAN_TX_HOLD_ITEMS);
	wlanTxHoldSlot.groupCount = 0;
	wlanTxHoldSlot.unicastCount = 0;
	wlanTxHoldSlot.head = NULL;
	wlanTxHoldSlot.tail = NULL;
	wlanTxHoldGroup.groupCount = 0;
	wlanTxHoldGroup.unicastCount = 0;
	wlanTxHoldGroup.head = NULL;
	wlanTxHoldGroup.tail = NULL;
	wlanTxHold.sending = 0;
}

void macMgmtFilterHeader(void)
{
	UINT posture = sysIrqDisable();

	wlanTxHold.sending = 0;
	sysIrqRestore(posture);
}

/* Announces the peer of a held frame with an ATIM frame; 1 when the peer is known or the frame went out. */
int macMgmtHandleDescriptor(OsaBuffer *frame)
{
	struct WlanConnection *connection = frame->context;
	const uint8_t *address = wlanTxHoldDestination(frame);
	int broadcast;
	int result;

	if (macMgmtSelectHeader(address) != 0)
		return 1;
	wlanTxHold.sending = 1;
	wlanInitMgmtHeader(&wlanAtimBuffer.frame, 0, 9, address, connection->interfaceAddress,
			   connection->localAddress);
	wlanAtimBuffer.frame.duration[0] = 2;
	wlanAtimBuffer.frame.duration[1] = 1;
	wlanAtimBuffer.frame.bodyLength[0] = 0;
	wlanAtimBuffer.frame.frameControl[1] = (wlanAtimBuffer.frame.frameControl[1] & 0xef) |
					       ((16 * wlanLocalState->powerSave) & 0x1f);
	wlanAtimBuffer.buffer.context = connection;
	wlanAtimBuffer.buffer.dataOffset = 28;
	wlanAtimData = wlanAtimBuffer.data;
	wlanAtimBuffer.frameAddress = &wlanAtimBuffer.frame;
	wlanAtimBuffer.data[0] = (wlanAtimBuffer.data[0] & ~7) | 1;
	broadcast = ((const uint16_t *)address)[0] == 0xffff && ((const uint16_t *)address)[1] == 0xffff &&
		    ((const uint16_t *)address)[2] == 0xffff;
	wlanAtimData[2] = wlanPsProcessFrame(connection, 9, broadcast);
	result = wlanTxReleaseFrame(&wlanAtimBuffer.buffer);
	if (result != 0) {
		if (wlanTxHold.peerCount < WLAN_TX_HOLD_PEERS) {
			__aeabi_memcpy(wlanTxHold.peers[wlanTxHold.peerCount], address, 6);
			++wlanTxHold.peerCount;
		}
		return 1;
	}
	return result;
}

/* Sends or drops the frames of the list whose peer was announced (all of them when drainAll is set). */
void macMgmtProcessDescriptor(void)
{
	struct WlanTxHoldItem *item;

	for (item = wlanTxHoldSlot.head; item != NULL; item = wlanTxHoldSlot.head) {
		if (wlanTxHold.drainAll == 0 &&
		    macMgmtSelectHeader(wlanTxHoldDestination(item->frame)) == 0)
			break;
		if (item->mode != 0) {
			if (item->mode == 1)
				wlanTxResubmit(item->frame);
		} else {
			wlanTxReleaseFrame(item->frame);
		}
		item = wlanTxHoldSlot.head;
		if (item != NULL) {
			wlanTxHoldSlot.head = item->next;
			if (wlanTxHoldSlot.head == NULL)
				wlanTxHoldSlot.tail = NULL;
			item->next = NULL;
			if (item->group != 0)
				--wlanTxHoldSlot.groupCount;
			else
				--wlanTxHoldSlot.unicastCount;
		}
		macMgmtReleaseHeader(item);
	}
}

int macMgmtUpdateDescriptor(void)
{
	return wlanRoleOtherOnly();
}

/* Takes a frame of the IBSS to the list of the window, or sends it straight away. */
int wlanIbssTxFrame(OsaBuffer *buffer, int transmit)
{
	WlanWirelessFrame *frame = wlanTxHoldFrame(buffer);
	const uint8_t *address = frame->address1;
	int context;
	int group;
	int result;

	if (frame->frameControl[0] >> 4 == 4) {
		MAC_090_POLICY_MASK &= ~1u;
		MAC_090_POLICY_MASK &= ~0x10u;
		return wlanTxReleaseFrame(buffer);
	}
	context = macTxGetDrainWaits();
	if (context != 0) {
		if (context == 1) {
			result = 0;
			osaSemaphoreWait(wlanTxHoldListsLock, 0xffffffff);
			if (wlanTxHold.flushPending != 0)
				macMgmtProcessDescriptor();
			group = 1;
			if ((((const uint16_t *)address)[0] != 0xffff || ((const uint16_t *)address)[1] != 0xffff ||
			     ((const uint16_t *)address)[2] != 0xffff) && !(address[0] & 1))
				group = 0;
			if (wlanTxHold.phase != 0) {
				if (wlanTxHold.phase != 1) {
					if (wlanTxHold.phase == 2)
						result = macMgmtSetupHeader(&wlanTxHoldGroup, buffer, transmit);
				} else if (group == 0) {
					result = macMgmtSetupHeader(&wlanTxHoldSlot, buffer, transmit);
					if (wlanTxHold.flushPending != 0)
						macMgmtProcessDescriptor();
				} else {
					result = macMgmtSetupHeader(&wlanTxHoldGroup, buffer, transmit);
					wlanTxHold.phase = 2;
				}
			} else {
				result = macMgmtSetupHeader(&wlanTxHoldSlot, buffer, transmit);
				if (wlanTxHold.flushPending != 0)
					macMgmtProcessDescriptor();
				if (group == 0)
					wlanTxHold.phase = 1;
			}
			osaSemaphoreRelease(wlanTxHoldListsLock);
			return result;
		}
	} else {
		if (transmit == 0)
			return wlanTxReleaseFrame(buffer);
		if (transmit == 1)
			return wlanTxResubmit(buffer);
		osaSemaphoreRelease(macTxSemaphore);
	}
	return 0;
}

/* Counts the calls of macMgmtClearHeader() since the window opened and checks the others. */
void macMgmtCheckDescriptor(void)
{
	uint8_t *descriptor;

	if (wlanRoleOtherOnly() == 0) {
		descriptor = (uint8_t *)__aeabi_uread4((const void *)(MAC_004_REGION_B + 4));
		descriptor[3] = (descriptor[3] & 0xef) | ((16 * wlanTxHold.groupBit) & 0x1f);
	}
}

void macMgmtValidateDescriptor(uint8_t enabled)
{
	wlanTxHold.groupBit = enabled;
}

void macMgmtPrepareDescriptor(uint16_t atimWindow)
{
	if (wlanRoleIbssOnly() != 0)
		wlanTxHold.atimWindow = atimWindow;
}

unsigned int macMgmtDispatchDescriptor(void)
{
	if (wlanRoleIbssOnly() != 0)
		return wlanTxHold.atimWindow;
	return 0;
}

/* True when a pool has all its blocks back. */
static inline __attribute__((always_inline)) int wlanTxHoldPoolFull(struct OsaBufferPool *pool)
{
	int blocks = osa_BufferClone(pool);
	int free = osa_PoolQuery(pool, 8);

	return blocks == free + osa_PoolQuery(pool, 16);
}

/* Waits for the host side, the frames and the baseband to be quiet; 1 when they are. */
int macMgmtParseDescriptor(void)
{
	unsigned int status;
	int retries;

	if (wlanHostSleepPending != 0 || mdevIoctl(wlanSdioDev, 8, 0) != 0) {
		macTxPostEvent(1);
		osaTaskSleep(1);
	}
	osaSemaphoreWait(macTxSemaphore, 0xffffffff);
	if (wlanTxHoldSlot.unicastCount + wlanTxHoldSlot.groupCount != 0 ||
	    wlanTxHoldGroup.groupCount + wlanTxHoldGroup.unicastCount != 0) {
		macMgmtAcquireHeader(&wlanTxHoldSlot, &wlanTxHoldGroup);
		macMgmtProcessDescriptor();
	}
	osaSemaphoreRelease(macTxSemaphore);
	retries = 4;
	for (;;) {
		if (wlanTxHoldPoolFull(&wlanBlockPool28) && wlanTxHoldPoolFull(&wlanBlockPool7) &&
		    wlanTxHoldPoolFull(&wlanReplyFramePool))
			break;
		if (retries-- == 0)
			return 0;
		osaTaskSleep(1);
	}
	if (!macTxPoolsIdle())
		return 0;
	status = (32 * MAC_REG(0x890)) >> 26;
	retries = 10;
	while (status > 1) {
		uint64_t clocks;

		if (retries-- == 0)
			return 0;
		clocks = romMul32x32(sysClockFrequency, 100);
		if ((int)(clocks >> 10) != 0)
			robinDelayLoop(clocks >> 10);
		status = (32 * MAC_REG(0x890)) >> 26;
	}
	return 1;
}

/* The states of the machine. */
static const int32_t *macMgmtBuildDescriptor(struct WlanTxHoldMachine *machine, const int32_t *event)
{
	if (*event == ROM_HSM_ENTRY) {
		macMgmtRecordHeader();
		romHsmTransition(machine, &machine->compute);
	}
	return NULL;
}

static const int32_t *macMgmtComputeDescriptor(struct WlanTxHoldMachine *machine, const int32_t *event)
{
	switch (*event) {
	case 0:
		romHsmTransition(machine, &machine->reset);
		return NULL;
	case ROM_HSM_EXIT:
		return NULL;
	case ROM_HSM_ENTRY:
		macTxSetDrainWaits(0);
		return NULL;
	default:
		return event;
	}
}

static const int32_t *macMgmtQueryDescriptor(struct WlanTxHoldMachine *machine, const int32_t *event)
{
	struct RomHsmState *target;

	if (*event == ROM_HSM_EXIT)
		return NULL;
	if (*event == ROM_HSM_ENTRY) {
		wlanTxHold.active = 0;
		macTxSetDrainWaits(1);
		wlanTxHold.windowCount = 0;
		wlanTxHold.windowDivisor = 4;
		return NULL;
	}
	if (*event == 1) {
		MAC_090_POLICY_MASK &= ~1u;
		MAC_090_POLICY_MASK &= ~0x10u;
		fmCoexResetBuffer();
		macMgmtParseDescriptor();
		target = &machine->compute;
	} else {
		if (*event != 6)
			return event;
		wlanTxHold.active = 1;
		target = &machine->reset;
	}
	romHsmTransition(machine, target);
	return NULL;
}

static const int32_t *macMgmtResetDescriptor(struct WlanTxHoldMachine *machine, const int32_t *event)
{
	switch (*event) {
	case 2:
		romHsmTransition(machine, &machine->sync);
		return NULL;
	case 5:
	case ROM_HSM_EXIT:
		return NULL;
	case ROM_HSM_ENTRY:
		if (wlanTxHold.active != 0) {
			wlanTxHold.active = 0;
			macMgmtRecordHeader();
		}
		osaSemaphoreWait(wlanTxHoldListsLock, 0xffffffff);
		wlanTxHold.flushPending = 0;
		osaSemaphoreRelease(wlanTxHoldListsLock);
		MAC_090_POLICY_MASK |= 1u;
		MAC_090_POLICY_MASK |= 0x10u;
		wlanTxHold.held = 0;
		wlanTxHold.drainAll = 0;
		wlanTxHold.psActive = 0;
		wlanTxHold.waiting = 0;
		return NULL;
	default:
		return event;
	}
}

static const int32_t *macMgmtSyncDescriptor(struct WlanTxHoldMachine *machine, const int32_t *event)
{
	struct RomHsmState *target;
	struct WlanTxHoldItem *item;
	int i;

	if (*event == ROM_HSM_EXIT)
		return NULL;
	if (*event == ROM_HSM_ENTRY) {
		i = 0;
		wlanTxHold.peerCount = 0;
		do
			__aeabi_memclr(wlanTxHold.peers[i++], 6);
		while (i < WLAN_TX_HOLD_PEERS);
		osaSemaphoreWait(wlanTxHoldListsLock, 0xffffffff);
		macMgmtAcquireHeader(&wlanTxHoldSlot, &wlanTxHoldGroup);
		wlanTxHold.phase = wlanTxHoldSlot.unicastCount != 0;
		item = wlanTxHoldSlot.head;
		wlanTxHoldCursor = item;
		while (item != NULL) {
			if (macMgmtHandleDescriptor(item->frame) != 0)
				wlanTxHold.held = 1;
			item = wlanTxHoldCursor->next;
			wlanTxHoldCursor = item;
		}
		osaSemaphoreRelease(wlanTxHoldListsLock);
		return NULL;
	}
	if (*event == 3) {
		MAC_090_POLICY_MASK &= ~1u;
		MAC_090_POLICY_MASK &= ~0x10u;
		target = &machine->apply;
	} else {
		if (*event != 5)
			return event;
		wlanPsCancelWakeTimer();
		target = &machine->reset;
	}
	romHsmTransition(machine, target);
	return NULL;
}

static const int32_t *macMgmtApplyDescriptor(struct WlanTxHoldMachine *machine, const int32_t *event)
{
	switch (*event) {
	case 4:
	case 5:
		wlanPsCancelWakeTimer();
		romHsmTransition(machine, &machine->reset);
		return NULL;
	case ROM_HSM_EXIT:
		return NULL;
	case ROM_HSM_ENTRY:
		osaSemaphoreWait(wlanTxHoldListsLock, 0xffffffff);
		macMgmtProcessDescriptor();
		wlanTxHold.flushPending = 1;
		osaSemaphoreRelease(wlanTxHoldListsLock);
		return NULL;
	default:
		return event;
	}
}

void macMgmtResolveDescriptor(void)
{
	romHsmStateInit(&wlanTxHoldMachine.build, NULL, (RomHsmHandler)macMgmtBuildDescriptor);
	romHsmStateInit(&wlanTxHoldMachine.compute, &wlanTxHoldMachine.build,
			(RomHsmHandler)macMgmtComputeDescriptor);
	romHsmStateInit(&wlanTxHoldMachine.query, &wlanTxHoldMachine.build,
			(RomHsmHandler)macMgmtQueryDescriptor);
	romHsmStateInit(&wlanTxHoldMachine.reset, &wlanTxHoldMachine.query,
			(RomHsmHandler)macMgmtResetDescriptor);
	romHsmStateInit(&wlanTxHoldMachine.sync, &wlanTxHoldMachine.query,
			(RomHsmHandler)macMgmtSyncDescriptor);
	romHsmStateInit(&wlanTxHoldMachine.apply, &wlanTxHoldMachine.query,
			(RomHsmHandler)macMgmtApplyDescriptor);
	romHsmStart(&wlanTxHoldMachine, &wlanTxHoldMachine.build);
}

/* The events the machine is sent by the power-save code. */
__attribute__((noinline)) int wlanPsClearTable(struct WlanConnection *connection, unsigned int event)
{
	int result = wlanRoleOtherOnly();

	if (result == 0)
		return mdevRomQueryState(&wlanTxHoldMachine, event, NULL);
	return result;
}

int wlanPsConvertTable(int enabled)
{
	if (enabled != 0) {
		wlanTxHold.psActive = 1;
		if (wlanTxHold.windowDivisor == 255)
			return wlanPostConnectionEvent(NULL, 0x200000);
		return wlanTxHold.windowDivisor;
	}
	wlanTxHold.psActive = 0;
	return 0;
}

void wlanPsVerifyTable(void)
{
	if (wlanTxHoldMachine.state == &wlanTxHoldMachine.apply && macMgmtClearHeader() == 0) {
		wlanPsCancelWakeTimer();
		wlanPsPostEvent(18);
	}
}

int wlanPsSelectTable(void)
{
	return wlanPostConnectionEvent(NULL, 0x8000);
}

int wlanPsAdjustTable(struct WlanConnection *connection)
{
	return wlanPsClearTable(connection, 1);
}

/* Brings the timers into step: the link check's time and the beacon period. */
void wlanPsRecordTable(void)
{
	UINT posture = sysIrqDisable();
	uint64_t now = romGetTimerTimestamp();
	uint64_t other = romLibSelectState();

	if ((uint64_t)wlanLinkCheckPeriod() + now < other || other < now)
		romLibAdjustState(now, wlanLinkCheckPeriod());
	if (wlanPsCheckHeader() != MAC_618_BEACON_PERIOD)
		romLibApplyContext(wlanPsCheckHeader());
	sysIrqRestore(posture);
}

int wlanPsFilterTable(struct WlanConnection *connection)
{
	wlanPsRecordTable();
	if (wlanTxHold.atimWindow != 0)
		return wlanPsClearTable(connection, 0);
	return wlanTxHold.atimWindow;
}

void wlanPsHandleBuffer(uint8_t divisor)
{
	wlanTxHold.windowDivisor = divisor;
}

/* Puts an item in a list: the items for group addresses stay in front of the others. */
void wlanPsProcessBuffer(struct WlanTxHoldList *list, struct WlanTxHoldItem *item)
{
	struct WlanTxHoldItem *previous;
	struct WlanTxHoldItem *next;
	unsigned int i;

	if (list->tail != NULL) {
		if (item->group != 0) {
			previous = NULL;
			next = list->head;
			for (i = 0; list->groupCount > i; ++i) {
				previous = next;
				next = next->next;
			}
			item->next = next;
			if (previous != NULL) {
				previous->next = item;
				if (next == NULL)
					list->tail = item;
			} else {
				list->head = item;
			}
			++list->groupCount;
			return;
		}
		item->next = NULL;
		list->tail->next = item;
		list->tail = item;
	} else {
		item->next = NULL;
		list->head = item;
		list->tail = item;
		if (item->group != 0) {
			++list->groupCount;
			return;
		}
	}
	++list->unicastCount;
}

/* Takes an item off the free list; 0 when none is left. */
int wlanPsUpdateBuffer(struct WlanTxHoldItem **item)
{
	osaSemaphoreWait(wlanTxHoldItemsLock, 0xffffffff);
	if (wlanTxHoldFreeCount != 0) {
		*item = wlanTxHoldFree;
		wlanTxHoldFree = wlanTxHoldFree->next;
		--wlanTxHoldFreeCount;
		osaSemaphoreRelease(wlanTxHoldItemsLock);
		return 1;
	}
	osaSemaphoreRelease(wlanTxHoldItemsLock);
	return 0;
}

int wlanPsConfigureBuffer(void)
{
	uint64_t cycles = __aeabi_uldivmod(romGetTimerTimestamp(), wlanLinkCheckPeriod());

	return wlanPsWakeCycle > (uint32_t)cycles + 1000;
}

