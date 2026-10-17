/* Transmit-path state of the MAC: window queues, power-save mode, counters. */
#ifndef ROBIN_MACTX_H
#define ROBIN_MACTX_H

#include <stdint.h>
#include <wlanbt/wlanbt_protocol.h>
#include "robin/conn.h"
#include "robin/wlan.h"

/* Queue of frames waiting in one hardware window (144 bytes, two windows). */
struct WlanTxWindowQueue {
	uint8_t pending;	/* frames handed to the hardware and not yet completed */
	uint8_t reserved0[3];
	uint16_t head;
	uint16_t tail;
	uint32_t entries[32];	/* frame data pointers (OsaBuffer.data) */
	uint32_t count;
	uint32_t reserved1;
};

_Static_assert(sizeof(struct WlanTxWindowQueue) == 144, "transmit window queue");

/* Hardware counters of one channel-access queue accumulated in software. */
struct MacTxQueueStats {
	uint32_t prepared;
	uint32_t checked;
	uint32_t validated;
};

/* Mode block of the local station; byte 4 is the power-save mode. */
struct WlanLocalState {
	uint8_t reserved0[4];
	uint8_t powerSave;
	uint8_t reserved1[3];
};

/* Bits of wlanTxWindowFlags, one set per window (shifted by the window number). */
#define WLAN_TX_WINDOW_OPEN		0x001u
#define WLAN_TX_WINDOW_ENABLED		0x010u
#define WLAN_TX_WINDOW_DRAINING		0x100u

extern struct WlanTxWindowQueue wlanTxWindowQueues[2];
extern uint32_t macTxWakeBits;
extern uint32_t wlanTxWindowFlags;
extern struct MacTxQueueStats macTxQueueStats[];
extern uint16_t wlanSessionId;
extern struct WlanLocalState *wlanLocalState;
extern uint32_t macTxMaxWait;
extern uint8_t wlanSlotContext;

int macWaitPendingClear(unsigned int mask, unsigned int limit);
int macWaitBasebandStable(uint32_t mask, uint32_t *iterations, uint32_t samples);
extern uint64_t macWaitElapsed;
int macTxPostEvent(uint32_t events);
int macTxWakeup(void);
int macTxMeasureWait(unsigned int mask);
int macTxPathReady(void);
void macTxAdvanceStage(void);
int wlanTxAllocFrame(OsaBuffer **buffer, WlanTxDescriptor **descriptor, uint32_t size,
		     uint16_t priority);
OsaBuffer *wlanTxAllocFrameType2(uint16_t priority, int (*release)(OsaBuffer *buffer));
void macWakeSettle(uint32_t mask);
uint32_t macTxAcquireWakeBit(void);
void macTxReleaseWakeBits(uint32_t *bits);
void wlanTxWindowReset(unsigned int window);
void wlanTxWindowsClearPending(void);
void wlanTxWindowComplete(unsigned int window);
int macTxQueueMgmtMessage(uintptr_t message);
int macTxDequeue(unsigned int window, uint32_t *entry);
int macTxResetQueues(void);
unsigned int macTxActivateWindow(unsigned int window);
void macTxClearSlotTable(void);
void macTxClearSlotBuffer(void);
int macTxGetContext(void);
void macTxSetContext(int value);
void macTxSetPowerSave(int mode);
int macTxGetPowerSave(void);
void macTxStatsReset(unsigned int queue);
int macTxStatsAddPrepared(unsigned int queue, int *last);
int macTxStatsAddValidated(unsigned int queue, int *last);
int macTxStatsAddChecked(unsigned int queue, int *last);
int8_t wlanClampSnr(int value);
int wlanClampNoise(int value);
void macTxBuildDataHeader(struct WlanConnection *connection, WlanTxDescriptor *descriptor,
			  WlanWirelessFrame *frame, const uint8_t *addresses, int qos);

#endif /* ROBIN_MACTX_H */
