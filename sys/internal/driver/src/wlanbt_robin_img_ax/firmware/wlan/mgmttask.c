/*
 * The "MAC Mgmt" task (0x342a8, created by macMgmtCreateTask()): it waits for the events the
 * interrupt handler and the other tasks set and works through what they queued: the host's
 * state machine commands (4), the channel time scheduler (0x10, 0x40, 0x80), the connections
 * whose link is to be checked (8) and looked at for a missing peer (0x800), the timers whose
 * callbacks run in this task (0x20), and the frames of the three receive rings that carry the
 * management and control frames (1: ring A, 2: ring B, 0x200: ring C). Marvell published no
 * source for it; the names are the firmware's own placeholder names, kept, and the file name
 * is inferred.
 */
#include <stddef.h>
#include "robin/apps.h"
#include "robin/apsta.h"
#include "robin/btrom.h"
#include "robin/conn.h"
#include "robin/coex.h"
#include "robin/dmaring.h"
#include "robin/ibss.h"
#include "robin/joinframe.h"
#include "robin/mgmt.h"
#include "robin/osa.h"
#include "robin/pssm.h"
#include "robin/rom.h"
#include "robin/runtime.h"
#include "robin/signal.h"
#include "robin/slot.h"
#include "robin/softtimer.h"
#include "robin/sync.h"
#include "robin/sys.h"
#include "robin/task.h"
#include "robin/threadx.h"
#include "robin/wlan.h"
#include "robin/wlanrx.h"
#include "robin/wlantask.h"

#define U16(bytes)		((uint16_t)((bytes)[0] | (bytes)[1] << 8))
#define PUT_U16(bytes, value)	((bytes)[0] = (uint8_t)(value), (bytes)[1] = (uint8_t)((value) >> 8))

/* The events macMgmtTaskMain() waits for. */

#define OSA_QUEUE_EMPTY		15
#define TX_QUEUE_EMPTY		10

/* Where the frame of a ring's status starts in the buffer: the data offset the frame is read at. */
#define MGMT_RING_C_OFFSET	120
#define MGMT_RING_AB_OFFSET	128

#define MGMT_FRAME_TYPE(frame)		(((frame)[2] >> 2) & 3)
#define MGMT_FRAME_SUBTYPE(frame)	((frame)[2] >> 4)

extern struct OsaBufferPool wlanPacketBufferPool;		/* 0x04000dcc */
extern struct OsaBufferPool wlanMgmtTaskPool;			/* 0x04000e5c */
extern uint32_t wlanPsHostWakeDue;				/* 0x54c54 */

void wlanSmPostCommand(OsaBuffer *buffer);
int wlanRxCheckFrameClass(struct WlanConnection *connection, const WlanWirelessFrame *frame);
int wlanMgmtRxControl(const WlanWirelessFrame *frame, OsaBuffer *buffer);
void wlanRxManagementRoute(OsaBuffer *buffer, uintptr_t argument);
void wlanRxManagementToAll(OsaBuffer *buffer, uintptr_t argument, unsigned int interfaceType);
void romDmaRingSetPointer(uint16_t pointer, unsigned int ring);
void macMgmtPreparePacket(struct WlanConnection *connection);

/*
 * A received management frame: the channel time request that is running may take it first
 * (the slot callback), then the handler of its connection, else the routing by subtype.
 * With check set, a frame the connection's state does not allow is dropped (its length is cleared).
 */
int macTxNotifyResolveHeader(OsaBuffer *buffer, WlanWirelessFrame *frame, uintptr_t argument, int check)
{
	int result = wlanSlotRunningNotify((uint32_t)(uintptr_t)buffer, argument);
	struct WlanConnection *connection;

	if (result != 0)
		return result;
	if (check && wlanRxCheckFrameClass(buffer->context, frame) != 0) {
		buffer->length = 0;
		return 0;
	}
	connection = buffer->context;
	if (connection != NULL && connection->rxManagement != NULL)
		return connection->rxManagement(buffer, argument);
	wlanRxManagementRoute(buffer, argument);
	return 0;
}

/* Queues a connection for the link check; without room in the queue, the task is woken to catch up later. */
int wlanLinkCheckPost(struct WlanConnection *connection)
{
	struct WlanConnection *message = connection;

	if (tx_queue_send((TX_QUEUE *)&macMgmtLinkCheckQueue, &message, 0) != 0)
		return 1;
	osaEventFlagsSet(*wlanTaskHandles.mgmtEventFlags, MGMT_EVENT_LINK_CHECK, MGMT_EVENT_SET_OR);
	return 0;
}

/* The frames of ring C: the control frames; each goes to the handler of the connection of its sender. */
static inline int macMgmtTakeRingC(void)
{
	int taken;

	do {
		OsaBuffer *buffer = osa_BufferAlloc(&wlanPacketBufferPool, 2000, 0);
		struct WlanRxStatus *status;
		struct WlanConnection *connection;
		uint8_t *frame;

		if (buffer == NULL) {
			osaEventFlagsSet(*wlanTaskHandles.mgmtEventFlags, MGMT_EVENT_RING_C, MGMT_EVENT_SET_OR);
			return 1;
		}
		buffer->dataOffset = MGMT_RING_C_OFFSET;
		status = (struct WlanRxStatus *)buffer->data;
		__aeabi_uwrite4((int)&buffer->data[92], status->frame);
		taken = wlanDmaRingTakeC(status);
		if (taken != 0) {
			frame = (uint8_t *)__aeabi_uread4(status->frame);
			connection = wlanConnectionFindByPeer(frame + 12);
			buffer->context = connection;
			romDmaRingSetPointer(wlanDmaRingRearm(6), 6);
			if (wlanRxCheckFrameClass(connection, (const WlanWirelessFrame *)frame) != 1 &&
			    connection != NULL) {
				frame = (uint8_t *)__aeabi_uread4(status->frame);
				if (connection->rxControl != NULL)
					connection->rxControl((const WlanWirelessFrame *)frame, buffer);
				else
					wlanMgmtRxControl((const WlanWirelessFrame *)frame, buffer);
				continue;
			}
		}
		osa_BufferFree(buffer);
	} while (taken != 0);
	return 0;
}

/* Ring A: the management frames a station sent the access point's role; its power save state is kept. */
static inline int macMgmtTakeRingA(void)
{
	OsaBuffer *buffer = osa_BufferAlloc(&wlanMgmtTaskPool, 2000, 0);
	struct WlanRxStatus *status;

	if (buffer == NULL) {
		osaEventFlagsSet(*wlanTaskHandles.mgmtEventFlags, MGMT_EVENT_RING_A, MGMT_EVENT_SET_OR);
		return 1;
	}
	status = (struct WlanRxStatus *)buffer->data;
	for (;;) {
		uint8_t *frame;
		unsigned int length;

		buffer->dataOffset = MGMT_RING_AB_OFFSET;
		__aeabi_uwrite4((int)&buffer->data[100], status->frame);
		if (wlanDmaRingTakeA(status) == 0)
			break;
		frame = (uint8_t *)__aeabi_uread4(status->frame);
		romDmaRingSetPointer(wlanDmaRingRearm(4), 4);
		if (frame == NULL)
			break;
		length = U16(frame);
		if (length == 0)
			break;
		PUT_U16(frame, length - 96);
		if (MGMT_FRAME_TYPE(frame) == 0) {
			struct WlanConnection *connection;
			UINT posture = sysIrqDisable();

			connection = wlanConnectionFindByPeer(frame + 12);
			buffer->context = connection;
			if (connection != NULL && connection->mode == 2) {
				struct WlanConnectionRole *role = wlanConnectionInfoSlot(connection);
				unsigned int subtype = MGMT_FRAME_SUBTYPE(frame);

				if (subtype == 0 || subtype == 2 || subtype == 11 || subtype == 13) {
					unsigned int powerSave = (frame[3] >> 4) & 1;

					if (role->peerPowerSave != powerSave)
						wlanApPsSetState(connection, powerSave);
				}
				if (role->state == 1)
					buffer->length = 0;
			}
			sysIrqRestore(posture);
			if (buffer->length != 0 && wlanApPsEnabled()) {
				uint8_t forward = 1;
				int filtered = wlanPsRxFilter(frame, &forward);

				if (forward != 0)
					wlanPsActivity(wlanConnectionByRole(2));
				if (filtered == 0)
					wlanPsHostWakeDue &= ~0x800u;
			}
			if (buffer->length != 0)
				macTxNotifyResolveHeader(buffer, (WlanWirelessFrame *)frame,
							 (uintptr_t)buffer->data, 1);
		}
	}
	osa_BufferFree(buffer);
	return 0;
}

/* Ring B: the frames the hardware received for no connection (probe requests, beacons); everyone is offered them. */
static inline int macMgmtTakeRingB(void)
{
	OsaBuffer *buffer = osa_BufferAlloc(&wlanMgmtTaskPool, 2000, 0);
	struct WlanRxStatus *status;

	if (buffer == NULL) {
		osaEventFlagsSet(*wlanTaskHandles.mgmtEventFlags, MGMT_EVENT_RING_B, MGMT_EVENT_SET_OR);
		return 1;
	}
	status = (struct WlanRxStatus *)buffer->data;
	for (;;) {
		struct WlanConnection *connection;
		uint8_t *frame;
		unsigned int length;

		buffer->dataOffset = MGMT_RING_AB_OFFSET;
		__aeabi_uwrite4((int)&buffer->data[100], status->frame);
		if (wlanDmaRingTakeB(status) == 0)
			break;
		romDmaRingSetPointer(wlanDmaRingRearm(5), 5);
		frame = (uint8_t *)__aeabi_uread4(status->frame);
		if (frame == NULL || U16(frame) == 0)
			break;
		connection = wlanConnectionFindByPeer(frame + 12);
		if (connection == NULL) {
			connection = wlanConnectionFindByLocal(1, frame + 18);
			if (connection != NULL && !(connection->stationFlags & 0x80))
				connection = NULL;
		}
		buffer->context = connection;
		length = (uint16_t)(U16(frame) - 96);
		PUT_U16(frame, length);
		if (length != 0) {
			macTxNotifyResolveHeader(buffer, (WlanWirelessFrame *)frame, (uintptr_t)buffer->data, 0);
			wlanRxManagementToAll(buffer, (uintptr_t)buffer->data, 3);
			wlanRxManagementToAll(buffer, (uintptr_t)buffer->data, 2);
		}
	}
	osa_BufferFree(buffer);
	return 0;
}

void macMgmtTaskMain(ULONG input)
{
	struct WlanConnection *connection = NULL;
	int retry = 0;
	uint32_t actual;
	uintptr_t message;

	(void)input;
	for (;;) {
		osaEventFlagsWait(*wlanTaskHandles.mgmtEventFlags, 0xffff, MGMT_EVENT_WAIT_OR_CLEAR, &actual,
				  0xffffffff);
		if (actual != 0) {
			if (actual & MGMT_EVENT_COMMANDS) {
				OsaBuffer *command;

				while (osaMsgQueueReceive(*wlanTaskHandles.mgmtSmeQueue, (uintptr_t *)&command, 4, 0) !=
				       OSA_QUEUE_EMPTY) {
					wlanSmPostCommand(command);
					osa_BufferFree(command);
				}
			}
			if (actual & MGMT_EVENT_SLOT_RUN)
				wlanSlotRun();
			if (actual & MGMT_EVENT_SLOT_TICK)
				wlanSlotNotifyTick();
			if (actual & MGMT_EVENT_SLOT_POLL) {
				wlanSlotPoll();
				romMacSleepNotify();
			}
			if (actual & MGMT_EVENT_LINK_CHECK) {
				while (tx_queue_receive((TX_QUEUE *)&macMgmtLinkCheckQueue, &connection, 0) !=
				       TX_QUEUE_EMPTY) {
					if (connection != NULL && (connection->flags2 & WLAN_CONN2_ASSOCIATED))
						wlanLinkCheck(connection);
				}
			}
			if (actual & MGMT_EVENT_RING_C)
				retry |= macMgmtTakeRingC();
			if (actual & MGMT_EVENT_RING_A)
				retry |= macMgmtTakeRingA();
			if (actual & MGMT_EVENT_RING_B)
				retry |= macMgmtTakeRingB();
			if (actual & MGMT_EVENT_TIMERS) {
				while (osaMsgQueueReceive(*wlanTaskHandles.timerQueue, &message, 4, 0) !=
				       OSA_QUEUE_EMPTY) {
					struct WlanSoftTimer *timer = (struct WlanSoftTimer *)message;

					timer->unstarted = 0;
					if (timer->callback != NULL)
						timer->callback(timer->argument);
				}
			}
			if (actual & MGMT_EVENT_PEER_CHECK) {
				while (tx_queue_receive((TX_QUEUE *)&macMgmtPeerCheckQueue, &connection, 0) !=
				       TX_QUEUE_EMPTY) {
					if (connection == NULL)
						continue;
					if (connection->interfaceType == 1)
						wlanIbssCheckPeers();
					else if (connection->mode == 2)
						macMgmtPreparePacket(connection);
				}
			}
		}
		if (retry != 0) {
			retry = 0;
			osaTaskRelinquish();
		}
	}
}
