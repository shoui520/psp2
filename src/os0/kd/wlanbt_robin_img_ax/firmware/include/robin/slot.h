/*
 * Channel-time requests: a task asks to hold the radio on a channel for a
 * while (scan dwell, remain-on-channel, P2P listen). One request runs at a
 * time; the others wait in a FIFO. The names are inferred from use.
 */
#ifndef ROBIN_SLOT_H
#define ROBIN_SLOT_H

#include <stdint.h>
#include "robin/conn.h"
#include "robin/sync.h"

enum {
	WLAN_SLOT_REQUESTS = 15,
	/* States of WlanSlotRequest.state. */
	WLAN_SLOT_IDLE = 0,
	WLAN_SLOT_CURRENT = 1,
	WLAN_SLOT_RUNNING = 2,
	WLAN_SLOT_QUEUED = 3,
	WLAN_SLOT_CANCELLED = 4,
	/* Events passed to a request's callback. */
	WLAN_SLOT_EVENT_START = 3,
	WLAN_SLOT_EVENT_TICK = 5,
	WLAN_SLOT_EVENT_POLL = 9,
	WLAN_SLOT_EVENT_CANCEL = 11,
	/* Results of the control functions. */
	WLAN_SLOT_OK = 3,
	WLAN_SLOT_FAILED = 4,
	WLAN_SLOT_UNKNOWN = 5,
};

struct WlanSlotRequest {
	uint32_t handle;		/* 0: request id, 0 when the entry is free */
	uint8_t state;			/* 4 */
	uint8_t mode;			/* 5 */
	uint8_t bssType;		/* 6 */
	uint8_t reserved7;
	uint32_t startTime;		/* 8: when it was submitted */
	uint8_t reserved12[8];
	uint32_t duration;		/* 20: 0 for no limit */
	uint32_t *elapsed;		/* 24: receives the time spent waiting */
	int (*callback)(uint32_t handle, int event, uint32_t argument);	/* 28 */
	uint32_t context;		/* 32 */
	uint32_t argument;		/* 36 */
	uint32_t timer;			/* 40: timer that starts or ends the request */
	uint8_t phase;			/* 44: reason code or phase for the callback */
	uint8_t flags;			/* 45 */
	uint8_t timerFlags;		/* 46 */
	uint8_t reserved47;
	struct WlanConnection *connection;	/* 48 */
	struct WlanSlotRequest *next;	/* 52 */
};

_Static_assert(sizeof(struct WlanSlotRequest) == 56, "slot request");

struct WlanSlotScheduler {
	uint32_t rejectedInvalid;	/* submitted without a handle */
	uint32_t rejectedToken;		/* submitted by a stale timer */
	uint32_t queued;
	uint32_t dequeued;
	uint32_t cancelledQueued;
	uint32_t reserved20;
	struct WlanSlotRequest requests[WLAN_SLOT_REQUESTS];
	struct WlanSlotRequest *current;
	uint32_t reserved364;
	struct WlanSlotRequest *head;
	struct WlanSlotRequest *tail;
	struct OsaSemaphore *lock;
	uint32_t window;		/* timer that ends the current request's window */
	uint8_t polls;
	uint8_t activePolls;
	uint8_t reserved[6];
};

_Static_assert(sizeof(struct WlanSlotScheduler) == 0x380, "slot scheduler");
_Static_assert(offsetof(struct WlanSlotScheduler, current) == 0x360, "current request");

extern struct WlanSlotScheduler wlanSlotScheduler;

int wlanSlotStart(uint32_t timer, struct WlanSlotRequest *request);
void wlanSlotWindowExpired(uint32_t timer, struct WlanSlotRequest *request);
int wlanSlotCancel(struct WlanSlotRequest *request);
void wlanSlotGetRunning(uint32_t *context, uint32_t *handle, uint32_t *argument);
int wlanSlotPoll(void);
int wlanSlotResume(void);
int wlanSlotNotifyTick(void);
int wlanSlotInit(void);
int wlanSlotHasCurrent(void);
int wlanSlotLock(void);
int wlanSlotUnlock(void);
struct WlanSlotRequest *wlanSlotPopNext(void);
struct WlanSlotRequest *wlanSlotFind(uint32_t handle);
int wlanSlotCancelByFlags(int first, int second);
int wlanSlotFinish(void);
int wlanSlotSetDuration(uint32_t handle, uint32_t duration);
int wlanSlotClearTimer(uint32_t handle);
int wlanSlotRunningEvent(uint32_t event);
int wlanSlotRunningNotify(uint32_t first, uint32_t third);
int wlanSlotRunningResult(uint32_t second, uint32_t third);
int wlanSlotSetWindow(uint32_t handle, uint32_t delay);
int wlanSlotClearWindow(uint32_t handle);
int wlanSlotAbort(uint32_t handle, int code);
void wlanSlotCancelById(uint32_t handle);
int wlanSlotSetChannel(int channel, int band);
int wlanSlotBegin(void);
int wlanSlotQueryStart(int power);

#endif /* ROBIN_SLOT_H */
