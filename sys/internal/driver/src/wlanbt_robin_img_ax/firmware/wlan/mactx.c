/*
 * Transmit-path helpers of the MAC: window queues, event posting, statistics
 * and the 802.11 data header. The filename is inferred; no source filename
 * survives.
 */
#include "robin/apps.h"
#include "robin/stapssm.h"
#include "robin/ibss.h"
#include "robin/mgmt.h"
#include "robin/btrom.h"
#include "robin/coex.h"
#include "robin/mac.h"
#include "robin/mef.h"
#include "robin/phy.h"
#include "robin/psctl.h"
#include "robin/pspoll.h"
#include "robin/txring.h"
#include "robin/mactx.h"
#include "robin/ieee80211.h"
#include "robin/wlanrx.h"
#include "robin/rom.h"
#include "robin/runtime.h"
#include "robin/soc.h"
#include "robin/stream.h"
#include "robin/sync.h"
#include "robin/sys.h"
#include "robin/task.h"
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
/* Loops macTxWaitIdle() spent waiting for the host side. */
uint8_t macTxDrainWaits __attribute__((section(".sqram1.rw")));
/* Null frames not sent while the previous one was still queued. */
static uint32_t macTxNullDeferred __attribute__((section(".sqram1.rw")));
/* An exclusive null frame (macTxSendNullFrame()) is in flight. */
static uint32_t macTxNullBusy __attribute__((section(".sqram1.rw")));
/* A time-sharing PS-Poll (macTxSendPsPoll()) is in flight. */
static uint32_t macTxPsPollBusy __attribute__((section(".sqram1.rw")));
/* Descriptor of the host frame the transmit task is working on. */
WlanTxDescriptor macTxDescriptor __attribute__((section(".dtcm.bss")));
/* Serialises the frame builders of the transmit task, the IBSS path and the management task. */
struct OsaSemaphore *macTxSemaphore __attribute__((section(".itcm.bss")));
static uint32_t macTxSemaphoreObject[7] __attribute__((section(".itcm.bss")));
/* Read by the frame builder; only the stage reset writes it. */
uint8_t macTxBuildOverride __attribute__((section(".itcm.bss")));

/* Hardware counters read through the ROM (queue number in, running count out). */
int romMacStatValidate(unsigned int queue, int a, int b, int c);	/* 0x03f3acdb */
int romMacStatPrepare(unsigned int queue);				/* 0x03f3ace7 */
int romMacStatCheck(unsigned int queue);				/* 0x03f3accf */

/* Called as the MAC is first woken and when the last wake bit is released. */

/* Debug statistics hook (a no-op in this firmware). */

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

/* Not converted yet; provisional names. */
int macTxResolveDescriptor(struct WlanConnection *connection);		/* 0xf826 */

/* Control-task event bit raised for an event to the host while the AP sleeps clients. */
#define WLAN_EVENT_HOST_EVENT		5

/* mdevIoctl() of the SDIO device: the host wake GPIO, and the end of host sleep. */
#define SDIO_IOCTL_WAKE_GPIO		0x61
#define SDIO_IOCTL_HOST_AWAKE		0x0c

/*
 * Events for the host wait here while it sleeps, and go out once it is awake
 * (wlanHostEventFlush()).
 */
struct OsaMsgList wlanHostEventQueue __attribute__((section(".itcm.bss")));
/* Host sleep ends with the host's next wake-up. */
uint16_t wlanHostSleepEndOnWake __attribute__((section(".itcm.rw"))) = 0;

void wlanHostEventQueueInit(void)
{
	__aeabi_memclr4(&wlanHostEventQueue, sizeof(wlanHostEventQueue));
}

/*
 * Sends an event frame to the host for a connection: the frame is tagged with
 * the connection's BSS, and a link-lost event is dropped when the connection
 * no longer has a descriptor. Only frames of kind WLAN_HOST_FRAME_EVENT go
 * out. Returns 1 when the buffer was freed (or was NULL), 0 otherwise.
 */
int wlanHostEventSend(struct WlanConnection *connection, OsaBuffer *buffer)
{
	struct WlanHostEventFrame *frame;

	if (buffer == NULL)
		return 1;
	frame = (struct WlanHostEventFrame *)((uint8_t *)buffer + buffer->dataOffset);
	if (connection != NULL) {
		if (frame->kind == WLAN_HOST_FRAME_EVENT &&
		    frame->event == WLAN_HOST_EVENT_LINK_LOST &&
		    !macTxResolveDescriptor(connection))
			goto drop;
		frame->bssNumber = connection->bssNumber;
		frame->bssType = connection->bssType;
	}
	if (frame->kind != WLAN_HOST_FRAME_EVENT)
		return 0;
	if (wlanHostEventNotify(WLAN_WAKE_EVENT, buffer) != 0)
		return 0;
drop:
	osa_BufferFree(buffer);
	return 1;
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

/* An event without data; release, if not NULL, is called when the buffer is freed. */
int wlanHostEventSendRelease(struct WlanConnection *connection, uint16_t event,
			     int (*release)(OsaBuffer *buffer))
{
	OsaBuffer *buffer = wlanTxAllocFrameType2(event, release);

	if (buffer == NULL)
		return 0;
	return wlanHostEventSend(connection, buffer);
}

int wlanHostEventSendPlain(struct WlanConnection *connection, uint16_t event)
{
	return wlanHostEventSendRelease(connection, event, NULL);
}

/* An event that carries a 16-bit code. */
int wlanHostEventSendCode(struct WlanConnection *connection, uint16_t event, uint16_t code)
{
	OsaBuffer *buffer;
	struct WlanHostEventFrame *frame;

	if (!wlanTxAllocFrame(&buffer, (WlanTxDescriptor **)&frame, 100, event))
		return 0;
	frame->code = code;
	frame->length = 2;
	return wlanHostEventSend(connection, buffer);
}

/* An event that carries a MAC address, with a zero code. */
int wlanHostEventSendAddress(struct WlanConnection *connection, uint16_t event,
			     const uint8_t *address)
{
	OsaBuffer *buffer;
	struct WlanHostEventFrame *frame;

	if (!wlanTxAllocFrame(&buffer, (WlanTxDescriptor **)&frame, 400, event))
		return 0;
	frame->code = 0;
	__aeabi_memcpy(frame->address, address, 6);
	frame->length = 8;
	return wlanHostEventSend(connection, buffer);
}

/* An event that carries the given data. */
int wlanHostEventSendData(struct WlanConnection *connection, uint16_t event, const void *data,
			  uint16_t length)
{
	OsaBuffer *buffer;
	struct WlanHostEventFrame *frame;

	if (!wlanTxAllocFrame(&buffer, (WlanTxDescriptor **)&frame, 400, event))
		return 0;
	__aeabi_memcpy(&frame->code, data, length);
	frame->length = length;
	return wlanHostEventSend(connection, buffer);
}

/* An event that carries a 16-bit code and a MAC address. */
int wlanHostEventSendCodeAddress(struct WlanConnection *connection, uint16_t event, uint16_t code,
				 const uint8_t *address)
{
	OsaBuffer *buffer;
	struct WlanHostEventFrame *frame;

	if (!wlanTxAllocFrame(&buffer, (WlanTxDescriptor **)&frame, 100, event))
		return 0;
	frame->code = code;
	__aeabi_memcpy(frame->address, address, 6);
	frame->length = 8;
	return wlanHostEventSend(connection, buffer);
}

/* Puts a zero code and a MAC address in front of an event's data, and sends it. */
int wlanHostEventSendWithAddress(struct WlanConnection *connection, const uint8_t *address,
				 OsaBuffer *buffer)
{
	struct WlanHostEventFrame *frame =
		(struct WlanHostEventFrame *)((uint8_t *)buffer + buffer->dataOffset);

	frame->code = 0;
	__aeabi_memcpy(frame->address, address, 6);
	frame->length += 8;
	return wlanHostEventSend(connection, buffer);
}

/* Hands an event frame to the host. */
int wlanHostEventDeliver(OsaBuffer *buffer)
{
	struct WlanHostEventFrame *frame =
		(struct WlanHostEventFrame *)((uint8_t *)buffer + buffer->dataOffset);

	if (wlanApPsEnabled() && frame->event != WLAN_HOST_EVENT_PS_SLEEP)
		wlanRaiseEvent(WLAN_EVENT_HOST_EVENT);
	return wlanHostCallback(wlanDevice, buffer, 0, 2);
}

/* Delivers the events that waited while the host slept. */
void wlanHostEventFlush(void)
{
	OsaBuffer *buffer;

	while ((buffer = osa_MsgQueueRecv(&wlanHostEventQueue)) != NULL)
		wlanHostEventDeliver(buffer);
}

/*
 * The host is awake: lowers the wake GPIO if it was raised, tells the SDIO
 * device and delivers the events that waited.
 */
void wlanHostAwake(void)
{
	unsigned int posture = sysIrqDisable();

	if (wlanMefConfig.wakeSignalled && wlanHostSleepSettings.gpio != -1 &&
	    wlanHostSleepSettings.gpio != -2)
		mdevIoctl(wlanSdioDev, SDIO_IOCTL_WAKE_GPIO, 0);
	if (wlanPsHostMode == 1)
		wlanPsPostEvent(19);
	wlanMefConfig.hostState = WLAN_HOST_AWAKE;
	macTxPostEvent(4);
	mdevIoctl(wlanSdioDev, SDIO_IOCTL_HOST_AWAKE, 0);
	wlanHostEventFlush();
	sysIrqRestore(posture);
}

/* Ends host sleep. */
void wlanHostSleepCancel(void)
{
	wlanHostAwake();
	wlanMefConfig.activated = 0;
	wlanHostSleepActive = NULL;
	wlanPsSleepTimeout = 0;
}

void wlanHostSleepEnter(void)
{
	if (wlanMefConfig.hostState != WLAN_HOST_ASLEEP)
		wlanMefConfig.hostState = WLAN_HOST_ASLEEP;
}

/* True while host sleep is configured and armed. */
int wlanHostSleepArmed(void)
{
	if (wlanHostSleepActive == NULL || !wlanMefConfig.activated)
		return 0;
	return 1;
}

/* The wake classes that match the conditions; WLAN_WAKE_ALL matches whatever they are. */
unsigned int wlanHostSleepWakeMatch(unsigned int classes, unsigned int conditions)
{
	if (classes == WLAN_WAKE_ALL)
		return classes;
	return classes & conditions;
}

/* Waits the host sleep gap, in milliseconds, before the host is woken. */
void wlanHostSleepGap(unsigned int gap)
{
	uint32_t iterations = ((uint64_t)gap * sysClockFrequency * 1000) >> 10;

	if (iterations != 0)
		robinDelayLoop(iterations);
}

/*
 * Something for the host happened, of the given wake classes, with an event
 * frame or none. While host sleep is off the frame goes out at once; while
 * the host sleeps it is queued. Otherwise, when the host sleep conditions
 * match, the host is woken (its GPIO raised after the gap) and the frame
 * queued until it is awake. Link and power-save events wake it whatever the
 * conditions. Returns 0 when the conditions do not match.
 */
int wlanHostEventNotify(unsigned int classes, OsaBuffer *buffer)
{
	struct WlanHostEventFrame *frame = NULL;
	unsigned int event = 0;
	unsigned int posture;
	uint8_t state;
	int8_t gpio;

	if (buffer != NULL)
		frame = (struct WlanHostEventFrame *)((uint8_t *)buffer + buffer->dataOffset);
	if (!wlanHostSleepArmed()) {
		if ((classes & WLAN_WAKE_EVENT) && buffer != NULL) {
			wlanHostEventFlush();
			wlanHostEventDeliver(buffer);
		}
		return 1;
	}
	/* The state is read once: a host still waking up hangs this loop. */
	state = wlanMefConfig.hostState;
	while (state == WLAN_HOST_WAKING)
		;
	if (state == WLAN_HOST_ASLEEP) {
		if ((classes & WLAN_WAKE_EVENT) && buffer != NULL)
			osa_MsgQueueSend(&wlanHostEventQueue, buffer);
		return 1;
	}
	if (buffer != NULL) {
		if (classes & WLAN_WAKE_EVENT) {
			event = frame->event;
			switch (event) {
			case WLAN_HOST_EVENT_PS_AWAKE:
			case WLAN_HOST_EVENT_PS_SLEEP:
				if ((wlanSessionId | wlanConnectionPhase) == 0) {
					classes &= ~WLAN_WAKE_EVENT;
					break;
				}
				/* fall through */
			case WLAN_HOST_EVENT_LINK_LOST:
			case WLAN_HOST_EVENT_DEAUTHENTICATED:
			case WLAN_HOST_EVENT_DISASSOCIATED:
			case WLAN_HOST_EVENT_DEEP_SLEEP_AWAKE:
			case WLAN_HOST_EVENT_HOST_SLEEP_AWAKE:
				classes = WLAN_WAKE_ALL;
				break;
			}
		}
		if ((classes >> 4) & 3)
			event = frame->event;
	}
	if (wlanHostSleepWakeMatch(classes, __aeabi_uread4(wlanHostSleepActive->conditions)) == 0)
		return 0;
	if (event != 0) {
		if (wlanHostSleepEndOnWake != 0 && wlanHostSleepSettings.gpio == -1) {
			wlanHostSleepGap(wlanHostSleepActive->gap);
			wlanHostSleepCancel();
			wlanHostSleepEndOnWake = 0;
			wlanMefConfig.wakeSignalled = 0;
			wlanHostEventDeliver(buffer);
			return 1;
		}
		posture = sysIrqDisable();
		osa_MsgQueueSend(&wlanHostEventQueue, buffer);
		sysIrqRestore(posture);
	}
	gpio = wlanHostSleepActive->gpio;
	rfCalibSyncState(5000);
	wlanMefConfig.hostState = WLAN_HOST_WAKING;
	if (gpio != -1 && gpio != -2)
		mdevIoctl(wlanSdioDev, SDIO_IOCTL_WAKE_GPIO, 1);
	wlanHostSleepGap(wlanHostSleepActive->gap);
	if (wlanHostSleepEndOnWake != 0 && wlanHostSleepArmed()) {
		wlanHostSleepCancel();
		wlanMefConfig.wakeSignalled = 0;
		wlanHostSleepEndOnWake = 0;
	}
	return 1;
}

/* Gives up the wake bits in *bits; the last one out lets the MAC go to sleep. */
void macTxReleaseWakeBits(uint32_t *bits)
{
	unsigned int posture = sysIrqDisable();

	macTxWakeBits &= ~*bits;
	*bits = 0;
	if (macTxWakeBits == 0) {
		macTxMeasureWait(111);
		romMacSleepNotify();
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

/*
 * Loopback information of a frame (the pointer at 112 of its data): kind 2
 * frames carry a 16-byte header to strip, kind 3 frames go back as they are.
 */
#define TX_DATA_LOOPBACK(data)	(*(const uint8_t *const *)((data) + 112))
#define TX_LOOPBACK_KIND(info)	((info)[7])

/* Not converted yet; provisional names. */
int wlanTxQueueMgmtFrame(OsaBuffer *buffer);			/* 0x32038 */
void wlanTxCountDropped(OsaBuffer *buffer);			/* 0x41f2e */
void cbProcLoopbackFrame(OsaBuffer *buffer);			/* 0x1eea6 */
void cbProcFlushConnection(struct WlanConnection *connection);	/* 0x1ecde */
int wlanWmmEnabled(void);					/* 0x41cb8 */
unsigned int wlanWmmTid(unsigned int priority);			/* 0x41e76 */
unsigned int wlanNextSequence(struct WlanConnection *connection, unsigned int tid); /* 0x42068 */
int wlanWmmSetQueue(struct WlanConnection *connection, uint8_t *data, unsigned int tid,
		    int ampdu);						/* 0x41e84 */
uint8_t wlanTxDataRate(struct WlanConnection *connection, int unused); /* 0x3d2f8 */
uint8_t wlanTxLowRate(struct WlanConnection *connection, unsigned int limit); /* 0x31db0 */
void wlanPsTriggerSent(void);					/* 0x2eade */
int wlanPsUapsdActive(struct WlanConnection *connection);	/* 0x4123c */
struct WlanPsPeerState *wlanConnectionPsState(struct WlanConnection *connection); /* 0x1d1f6 */
unsigned int wlanTxRingLastQueue(void);				/* 0x32448 */
extern uint8_t wlanTxRateFlags;				/* 0x040088a6, bit 6: no low rate for null frames */
uint32_t wlanRateTimeoutValue(uint8_t *rateControl, uint32_t timeout, unsigned int shift); /* 0x2bd4a */
void cbProcSetBaWindow(unsigned int tid, unsigned int ring, unsigned int hwSlot); /* 0x1e9c2 */
void cbProcMoveFrames(unsigned int from, unsigned int to, unsigned int tid,
		      const uint8_t *peer);				/* 0x1ee52 */
void wlanBaSlotDetach(unsigned int queue, unsigned int hwSlot);	/* 0x2931c */
void wlanBaSlotClear(unsigned int hwSlot);			/* 0x2940e */
int wlanBaSlotSetup(const struct WlanBaRequest *request, unsigned int windowId,
		    unsigned int hwSlot);					/* 0x29264 */
void wlanRxBaHwRelease(const uint8_t *peer, unsigned int tid);	/* 0x296f6 */
void wlanTxRingMovePending(unsigned int window, unsigned int queue);	/* 0x30eec */
void wlanTxRingMoveToWindow(unsigned int window, unsigned int queue, unsigned int tid,
			    const uint8_t *peer);			/* 0x30efe */
void wlanBaNotifyAdded(struct WlanConnection *connection, unsigned int hwSlot,
		       const uint8_t *peer);				/* 0x333f6 */
void wlanBaNotifyDeleted(struct WlanConnection *connection, unsigned int hwSlot,
			 const uint8_t *peer);				/* 0x33420 */
int wlanP2pConnection(struct WlanConnection *connection);		/* 0x41228 */
unsigned int wlanPeerNextSequence(struct WlanConnection *connection, unsigned int tid,
				  int advance);				/* 0x1e246 */
unsigned int wlanGroupNextSequence(unsigned int tid);			/* 0x4204c */

/* Hardware window numbers handed to transmit Block Ack sessions. */
struct WlanWindowId wlanTxWindowIds[WLAN_TX_BA_MAX] __attribute__((section(".itcm.bss")));
extern uint32_t macTxResetMarker;					/* 0x04000170 */

static inline uint8_t *macTxBufferData(OsaBuffer *buffer)
{
	return (uint8_t *)buffer + buffer->dataOffset;
}

static inline OsaBuffer *macTxEntryBuffer(uint32_t entry)
{
	return (OsaBuffer *)(entry - offsetof(OsaBuffer, data));
}

/*
 * Sends a management frame from the "MAC Mgmt" task. With PS-Poll forced a
 * management frame goes straight into its ring and wakes the poll; otherwise
 * an access point may keep it for a sleeping station.
 */
void macTxSendMgmtFrame(OsaBuffer *buffer)
{
	struct WlanConnection *connection = buffer->context;
	struct WlanReplyFrame *request = (struct WlanReplyFrame *)macTxBufferData(buffer);

	if (wlanCoexPsPollForced() && ((request->frame[2] >> 2) & 3) == 0) {
		wlanTxInFlightAdd(buffer);
		if (wlanTxQueueMgmtFrame(buffer) != 0) {
			wlanPsPollKick(1);
			return;
		}
		wlanTxInFlightRemove(buffer);
	} else {
		int result;

		if (connection->mode != 2 || (result = wlanTxHoldForPeer(buffer, connection)) == 0)
			result = wlanTxDiscardFrame(buffer);
		if (result != 0)
			return;
	}
	osa_BufferFree(buffer);
}

/* Initialisation stage of the transmit task: drops everything queued and forgets the current frame. */
int macTxResetStage(void)
{
	uintptr_t buffer;

	osaSemaphoreRelease(macTxSemaphore);
	osaEventFlagsSet(macTxEventFlags, 0, 5);
	osaEventFlagsSet(macTxNotifyEventFlags, 0, 5);
	while (osaMsgQueueReceive(macTxMgmtQueue, &buffer, 4, 0) != 15)
		osa_BufferFree((OsaBuffer *)buffer);
	wlanHostSleepPending = 0;
	__aeabi_memclr4(&macTxDescriptor, 15);
	macTxBuildOverride = 0;
	macTxResetMarker = 0;
	return 0;
}

/* Sends a data frame; loopback frames go back to the callback processor instead. */
void macTxSendDataFrame(OsaBuffer *buffer)
{
	struct WlanReplyFrame *request = (struct WlanReplyFrame *)macTxBufferData(buffer);
	const uint8_t *loopback = TX_DATA_LOOPBACK((uint8_t *)request);
	struct WlanConnection *connection;

	if (loopback != NULL) {
		unsigned int kind = TX_LOOPBACK_KIND(loopback);

		if (kind == 2) {
			uint8_t *frame = request->frame;
			uint16_t length = (uint16_t)((frame[1] << 8 | frame[0]) - 16);

			frame[0] = (uint8_t)length;
			frame[1] = (uint8_t)(length >> 8);
			cbProcLoopbackFrame(buffer);
			return;
		}
		if (kind == 3) {
			cbProcLoopbackFrame(buffer);
			return;
		}
	}
	connection = buffer->context;
	if (connection != NULL && connection->mode == 2 &&
	    wlanTxHoldForPeer(buffer, connection) == 1)
		return;
	if (wlanTxSendFrame(buffer) == 0) {
		wlanTxCountDropped(buffer);
		wlanCompletePacket(buffer, 0);
	}
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

/*
 * Queues a frame while its block-ack window changes; returns 1 when the
 * frame was not queued (kept for a sleeping peer, or the queue is full).
 */
int macTxEnqueue(struct WlanConnection *connection, unsigned int window, uint32_t entry)
{
	struct WlanTxWindowQueue *queue;
	unsigned int next;

	if (connection->mode == 2 && wlanTxPeerHolding(connection) == 1 &&
	    wlanTxBufferRecharge(macTxEntryBuffer(entry), 2) == 1)
		return 1;
	queue = &wlanTxWindowQueues[window];
	next = (queue->tail + 1) & 0x1f;
	if (queue->head == next)
		return 1;
	queue->entries[queue->tail] = entry;
	queue->tail = next;
	++queue->count;
	return 0;
}

/* Sends every frame queued in a window again, with the selector of the frame that reopened it. */
static inline void macTxWindowRelease(struct WlanTxWindowQueue *queue, unsigned int window,
			       const uint8_t *data)
{
	uint32_t entry;

	while (macTxDequeue(window, &entry) == 0) {
		/* byte 2 of the descriptor, as the reopening frame has it */
		((uint8_t *)entry)[2] = data[2];
		++queue->pending;
		macTxSendDataFrame(macTxEntryBuffer(entry));
	}
}

/*
 * Adds (MAC_TX_WINDOW_ADD) or deletes a block-ack window of ring 5 or 6 as a
 * frame goes out. While frames of the old setting are still in flight, new
 * frames wait in the window queue ("draining") and are sent once it is done.
 * Returns 1 when the caller sends the frame now, 0 when it was queued or
 * completed here.
 */
int macTxBlockAckWindow(struct WlanConnection *connection, int operation, unsigned int ring,
			int slot, uint32_t entry, uint8_t *data)
{
	struct WlanReplyFrame *request = (struct WlanReplyFrame *)data;
	struct WlanTxWindowQueue *queue;
	unsigned int window;
	uint32_t draining, enabled, flags;

	if (operation == 0)
		return 1;
	if (ring != 5 && ring != 6)
		for (;;)
			;
	request->queue = ring;
	window = (uint8_t)(ring - 5);
	queue = &wlanTxWindowQueues[window];
	draining = WLAN_TX_WINDOW_DRAINING << window;
	enabled = WLAN_TX_WINDOW_ENABLED << window;
	flags = wlanTxWindowFlags;

	if (operation == MAC_TX_WINDOW_ADD) {
		if (!(flags & draining)) {
			if (flags & enabled)
				goto send;
			if (queue->pending == 0) {
				wlanBaSlotSetSequence(slot, *(uint16_t *)(request->frame + 24));
				wlanBaSlotApply(slot, request->queue);
				wlanTxWindowFlags |= enabled;
				goto send;
			}
			wlanTxWindowFlags = enabled | draining | flags;
			goto queue;
		}
		if (flags & enabled)
			goto queueOrComplete;
		wlanTxWindowFlags = enabled | (flags & ~draining);
		macTxWindowRelease(queue, window, data);
		goto send;
	}
	if (operation != MAC_TX_WINDOW_DELETE)
		return 1;
	if (!(flags & draining)) {
		if (flags & enabled) {
			if (queue->pending != 0) {
				wlanTxWindowFlags = (draining | flags) & ~enabled;
				goto queue;
			}
			wlanBaSlotFlush(slot, ring);
			wlanTxWindowFlags &= ~enabled;
		}
		goto send;
	}
	if (!(flags & enabled))
		goto queueOrComplete;
	wlanTxWindowFlags = (flags & ~draining) & ~enabled;
	macTxWindowRelease(queue, window, data);
send:
	++queue->pending;
	return 1;

queue:
	if (macTxEnqueue(connection, window, entry) == 0) {
		const uint8_t *frame = request->frame;

		queue->startSequence = frame[24] | frame[25] << 8;
		queue->slot = slot;
		sysGetTime(&queue->queuedAt);
		return 0;
	}
	wlanCompletePacket(macTxEntryBuffer(entry), 0);
	return 0;

queueOrComplete:
	if (macTxEnqueue(connection, window, entry) == 1)
		wlanCompletePacket(macTxEntryBuffer(entry), 0);
	return 0;
}

/*
 * Not converted yet; provisional names for what the transmit task uses them
 * for.
 */
void uapGroupRekey(uintptr_t message);					/* 0x1f7b4 */
void mdevWlanTrackState(void);						/* 0x015f0 */
int cbProcTxAllowed(void);						/* 0x1e032 */
int wlanDirectLinkPoll(struct WlanConnection *connection, uintptr_t *message); /* 0x41050 */
void wlanDirectLinkTrigger(void);					/* 0x41184 */
void wlanDirectLinkSend(struct WlanConnection *connection, uintptr_t message); /* 0x41202 */
OsaBuffer *macTxNotifyNextFrame(void);					/* 0x2fe92 */
struct WlanConnection *wlanConnectionFindDirectPeer(struct WlanConnection *connection,
						    const uint8_t *peer);	/* 0x0fbf2 */
void macTxBeginFrame(void);						/* 0x0f662 */
unsigned int cbProcNullPriority(void);					/* 0x1e9ba */
int wlanPsTriggerWanted(void);						/* 0x2ea82 */
int wlanPeerQosEnabled(struct WlanConnection *connection, const uint8_t *peer); /* 0x1e234 */
unsigned int wlanRateSelect(uint8_t *rateControl);			/* 0x2be18 */
unsigned int wlanTxApGroupRate(struct WlanConnection *connection);	/* 0x31e42 */
int wlanRateAmpduAllowed(unsigned int rate);				/* 0x3d2e2 */
uint8_t *wlanTxBuildFrame(struct MacTxFrameBuild *build, OsaBuffer *buffer, int *drop, int *eapol,
			  int reserved, unsigned int packetType, int ampdu,
			  const uint8_t **loopback, int qos);		/* 0x2ed44 */
int wlanPsLifetimeWanted(void);						/* 0x3dfa0 */
void wlanTxPrepareRequest(struct WlanReplyFrame *data, struct WlanMacTxRequest *request); /* 0x31ab0 */
void wlanTxSetSecurity(struct WlanConnection *connection, struct WlanReplyFrame *data,
		       struct WlanMacTxRequest *request);		/* 0x31798 */
void wlanTxSetRetry(struct WlanConnection *connection, struct WlanReplyFrame *data,
		    struct WlanMacTxRequest *request);			/* 0x31a3a */
extern struct OsaDevice ampWlanDriver;					/* 0x04009860 */
extern struct OsaMessageQueue wlanDriverQueue15;			/* 0x04004a18 */
extern struct OsaMessageQueue wlanDriverQueue4;				/* 0x04004a04 */
extern uint8_t wlanPrimaryEvents;					/* 0x04008828, bit 5: no rate below index 5 */

/* Management frames from the "MAC Mgmt" task keep the frame at a fixed place. */
#define MAC_TX_MGMT_FRAME	176
/* Bits 3-9 of WlanMacTxRequest.rate hold the rate index. */
#define MAC_TX_RATE_MASK		0x3f8u
#define MAC_TX_RATE(index)		(((index) << 25) >> 22)
#define MAC_TX_RATE_INDEX(rate)		(((uint32_t)(rate) << 22) >> 25)

static inline void macTxSetRate(struct WlanMacTxRequest *request, unsigned int index)
{
	request->rate = (request->rate & ~MAC_TX_RATE_MASK) | MAC_TX_RATE(index);
}

static inline void macTxSetFlags(struct WlanMacTxRequest *request, uint32_t set, uint32_t clear)
{
	__aeabi_uwrite4((__aeabi_uread4(request->flags) | set) & ~clear, request->flags);
}

/*
 * The "MAC Tx" task. It handles the events posted to it, sends the frames of
 * the management task, and while the host has data and the MAC may transmit
 * builds a transmit request for every host frame: the connection it goes
 * to, the 802.11 header, the rate, the Block Ack session and the ring.
 */
void macTxTaskMain(ULONG input)
{
	struct WlanConnection *connection = NULL;
	struct WlanConnectionInfo *info = NULL;
	struct MacTxFrameBuild build;
	struct WlanReplyFrame tx;
	struct WlanMacTxRequest *request;
	uintptr_t message;
	uint32_t actual;

	macTxSemaphore = (struct OsaSemaphore *)macTxSemaphoreObject;
	osaSemaphoreInit(&macTxSemaphore, 1, 12);
	macTxResetQueues();

	for (;;) {
		OsaBuffer *buffer;
		unsigned int window;

		do
			osaEventFlagsWait(macTxEventFlags, MAC_TX_EVENT_ALL, 8, &actual, 0xffffffff);
		while (actual == 0);

		if (actual & MAC_TX_EVENT_RESUME) {
			wlanMacResume(0);
			MAC_REG(0x544) |= 0x80000000u;
		}
		if (actual & MAC_TX_EVENT_FLUSH) {
			wlanMacFlush();
			osaEventFlagsSet(macMgmtEventFlags, 0x10, 7);
			wlanRaiseEvent(47);
		}
		if (actual & MAC_TX_EVENT_STAGE)
			wlanConnectionStage = 0;
		if (actual & MAC_TX_EVENT_REKEY)
			while (osaMsgQueueReceive(uapGroupRekeyQueue, &message, 4, 0) != 15)
				if (wlanConnectionBssStarted((struct WlanConnection *)message) != 0)
					uapGroupRekey(message);
		if (actual & MAC_TX_EVENT_TRACK)
			mdevWlanTrackState();
		if (actual & MAC_TX_EVENT_MGMT) {
			uintptr_t entry;

			while (osaMsgQueueReceive(macTxMgmtQueue, &entry, 4, 0) != 15) {
				WlanWirelessFrame *frame;
				struct WlanReplyFrame *data;

				buffer = (OsaBuffer *)entry;
				buffer->dataOffset = 28;
				connection = buffer->context;
				frame = (WlanWirelessFrame *)((uint8_t *)buffer + MAC_TX_MGMT_FRAME);
				frame->frameControl[1] &= ~4;
				frame->sequenceControl[0] &= 0x0f;
				frame->sequenceControl[1] = 0;
				frame->sequenceControl[0] = frame->sequenceControl[0] >> 4 << 4;
				data = (struct WlanReplyFrame *)buffer->data;
				data->frame = frame->bodyLength;
				data->queue = 4;
				macTxSendMgmtFrame(buffer);
			}
		}

		if ((actual & (MAC_TX_EVENT_DATA | MAC_TX_EVENT_NOTIFY)) == 0) {
			wlanHostSleepPending = 0;
			continue;
		}
		message = 0;
		mdevIoctl(&ampWlanDriver, 0x56, 0);
		if (cbProcTxAllowed() == 0 || macTxWakeBits != 0 || macTxPathReady() != 0)
			continue;
		connection = wlanConnectionFirst(3);
		if (connection != NULL && connection->mode != 0 &&
		    wlanDirectLinkPoll(connection, &message) == 0) {
			wlanConnectionPsState(connection)->triggerPending = 1;
			wlanDirectLinkTrigger();
			wlanDirectLinkSend(connection, message);
			continue;
		}

		/* Windows whose Block Ack change waited for the hardware to finish. */
		if (wlanTxWindowFlags & (WLAN_TX_WINDOW_DRAINING * 3)) {
			for (window = 0; window < 2; ++window) {
				struct WlanTxWindowQueue *queue = &wlanTxWindowQueues[window];
				uint32_t entry;

				if (!(wlanTxWindowFlags & (WLAN_TX_WINDOW_DRAINING << window)) ||
				    queue->pending != 0)
					continue;
				wlanTxWindowFlags &= ~(WLAN_TX_WINDOW_DRAINING << window);
				if (wlanTxWindowFlags & (WLAN_TX_WINDOW_ENABLED << window)) {
					wlanTrace(0xbbbbbbbb, 0x11111111);
					wlanBaSlotSetSequence(queue->slot, queue->startSequence);
					wlanBaSlotApply(queue->slot, window + 5);
				} else {
					wlanTrace(0xbbbbbbbb, 0x22222222);
					wlanBaSlotFlush(queue->slot, window + 5);
				}
				while (macTxDequeue(window, &entry) == 0) {
					wlanTrace(0xbbbbbbbb, 0x33333333);
					macTxSendDataFrame(macTxEntryBuffer(entry));
					++queue->pending;
				}
			}
		}

		for (;;) {
			WlanTxDescriptor *descriptor;
			const uint8_t *peer;
			WlanWirelessFrame *frame;
			uint8_t *data, *built;
			const uint8_t *loopback;
			unsigned int skip, rate, sequence, category;
			int qos, forced, lowRate, ampdu, drop, eapol;
			uint32_t control, txControl;
			uint8_t baIndex;

			if (actual & MAC_TX_EVENT_NOTIFY) {
				buffer = macTxNotifyNextFrame();
			} else {
				buffer = osa_MsgQueueRecv(&wlanDriverQueue15);
				if (buffer == NULL)
					buffer = osa_MsgQueueRecv(&wlanDriverQueue4);
				if (buffer != NULL)
					buffer->packetClass = 4;
			}
			if (buffer == NULL)
				break;

			descriptor = (WlanTxDescriptor *)((uint8_t *)buffer + buffer->dataOffset);
			peer = (const uint8_t *)descriptor + (descriptor->header.wire.packetOffset[1] << 8 |
							      descriptor->header.wire.packetOffset[0]);

			/* The connection the frame goes to. */
			switch (descriptor->header.bssType) {
			case 0:
				connection = wlanConnectionFind(0, descriptor->header.bssNumber);
				if (connection->interfaceType == 1 && !(connection->stationFlags & 0x80)) {
					struct WlanConnection *direct = wlanConnectionFindByPeer(peer);

					if (direct != NULL) {
						connection = direct;
						goto found;
					}
					if (!(peer[0] & 1) && connection->state == 6) {
						direct = wlanConnectionFindDirectPeer(connection, peer);
						if (direct != NULL) {
							connection = direct;
							wlanConnectionTouch(direct);
							goto found;
						}
					}
				}
				break;
			case 1:
				if (!(peer[0] & 1))
					goto byPeer;
				connection = wlanConnectionFind(1, descriptor->header.bssNumber);
				info = wlanConnectionInfo(connection);
				wlanConnectionInfoSlot(connection);
				if (connection == NULL)
					goto complete;
				goto group;
			case 2:
				connection = wlanConnectionFind(2, descriptor->header.bssNumber);
				if (connection == NULL)
					goto complete;
				if (wlanP2pConnection(connection) == 0)
					goto found;
				if (peer[0] & 1) {
					info = wlanConnectionInfo(connection);
					wlanConnectionInfoSlot(connection);
group:
					if (info == NULL || info->dataAllowed != 0)
						goto found;
					connection = NULL;
					goto complete;
				}
byPeer:
				{
					struct WlanConnectionRole *role;

					connection = wlanConnectionFindByPeer(peer);
					info = wlanConnectionInfo(connection);
					role = wlanConnectionInfoSlot(connection);
					if (connection == NULL)
						goto complete;
					if (role->state != 3) {
						connection = NULL;
						goto complete;
					}
				}
				goto found;
			case 7:
				connection = buffer->context;
				break;
			}
			if (connection == NULL)
				goto complete;
found:
			buffer->context = connection;
			wlanHostSleepPending = 1;
			macTxBeginFrame();
			__aeabi_memclr4(&tx, offsetof(struct WlanReplyFrame, frameLength));
			__aeabi_memcpy(&macTxDescriptor, descriptor, 15);
			if (macTxDescriptor.header.packetLength >= 8000)
				goto complete;

			/* A null frame for the power-save machinery, not data. */
			if (macTxDescriptor.flags & 1) {
				macTxAdvanceStage();
				if (wlanCoexUseP2pMode() && wlanPsNullSuppressed == 0) {
					if (wlanPsTriggerWanted()) {
						unsigned int priority = cbProcNullPriority();

						macTxSendNullFrame(connection, 1, priority,
								   (macTxDescriptor.flags >> 3) & 1, 1, 0);
					} else {
						wlanCoexRequestTx();
					}
				}
				if (wlanCoexUseStationMode())
					wlanPsHostFrameSeen();
				goto complete;
			}

			request = &tx.request;
			__aeabi_uwrite4((int)descriptor, request->host);
			__aeabi_uwrite4((int)build.header, request->frame);
			request->hwStatus = 0;
			request->hwRetries = 0;
			request->hwFlags = 0;
			build.data = (uint8_t *)&tx;
			if (connection->interfaceType == 1 || connection->mode == 2)
				qos = wlanPeerQosEnabled(connection, peer);
			else
				qos = wlanWmmEnabled();
			macTxBuildDataHeader(connection, &macTxDescriptor,
					     (WlanWirelessFrame *)__aeabi_uread4(request->frame), peer, qos);
			frame = (WlanWirelessFrame *)__aeabi_uread4(request->frame);
			if ((frame->frameControl[1] & 2) && (frame->frameControl[1] & 1))
				macTxSetFlags(request, 1, 0);

			/* The payload after the Ethernet addresses, unless the host sent 802.11. */
			skip = macTxDescriptor.header.packetType == 230 ? 0 : 12;
			build.payloadLength = macTxDescriptor.header.packetLength - skip;
			build.payload = (uint8_t *)descriptor + skip +
					(descriptor->header.wire.packetOffset[1] << 8 |
					 descriptor->header.wire.packetOffset[0]);
			build.tid = 0;
			if (macTxDescriptor.priority >= 8)
				macTxDescriptor.priority = 0;

			if ((connection->mode == 3 || connection->mode == 1) && wlanWmmEnabled()) {
				macTxDescriptor.priority = wlanWmmTid(macTxDescriptor.priority);
				build.tid = macTxDescriptor.priority;
				sequence = wlanNextSequence(connection, build.tid);
				build.sequenceControl = (build.sequenceControl & 0xf) | sequence << 4;
			} else if (((connection->interfaceType == 1 && wlanWmmEnabled()) ||
				    connection->mode == 2 || connection->interfaceType == 4) &&
				   wlanPeerQosEnabled(connection, peer)) {
				build.tid = macTxDescriptor.priority;
				sequence = wlanPeerNextSequence(connection, macTxDescriptor.priority, 1);
				build.sequenceControl = (build.sequenceControl & 0xf) | sequence << 4;
			}

			/* The rate: fixed by the host or the connection, else chosen per frame. */
			forced = 0;
			if (connection->interfaceType == 4) {
				request->rate = (request->rate & ~MAC_TX_RATE_MASK) + MAC_TX_RATE(12);
				forced = 1;
			}
			if (connection->dataRate != 254) {
				macTxSetRate(request, connection->dataRate);
				forced = 1;
				macTxSetFlags(request, 0x1000, 0);
			} else {
				macTxSetRate(request, wlanRateSelect(connection->rateControl));
				if (forced != 0 ||
				    (((WlanWirelessFrame *)__aeabi_uread4(request->frame))->address1[0] & 1))
					macTxSetFlags(request, 0x1000, 0);
			}
			if (((WlanWirelessFrame *)__aeabi_uread4(request->frame))->address1[0] & 1) {
				macTxSetFlags(request, MAC_TX_FLAG_NO_ACK, MAC_TX_FLAG_RTS);
				if (connection->mode == 2) {
					if (info->txRate != 0)
						rate = romRateIndex(info->txRate);
					else
						rate = wlanTxApGroupRate(connection);
					macTxSetRate(request, rate);
				} else if (connection->groupRate != 254) {
					macTxSetRate(request, connection->groupRate);
				} else if (forced == 0) {
					macTxSetRate(request, wlanTxControlRate(connection));
				}
			}
			if (MAC_TX_RATE_INDEX(request->rate) <= 3 && (wlanPrimaryEvents & 0x20))
				request->rate = (request->rate & ~MAC_TX_RATE_MASK) + MAC_TX_RATE(5);

			lowRate = 1;
			if (!(wlanProtection.flags & WLAN_PROTECTION_USE_PROTECTION) &&
			    (connection->mode != 2 || !(info->rateFlags & 8)))
				lowRate = 0;
			if (connection->interfaceType == 0 && !(connection->txOptions & 8)) {
				unsigned int value = romRateValue(MAC_TX_RATE_INDEX(request->rate));

				if (value <= romRateValue(18)) {
					wlanTxSetProtection(request, 1, (connection->txOptions >> 1) & 1);
					goto protected;
				}
			}
			wlanTxSetProtection(request, lowRate, (connection->txOptions >> 1) & 1);
protected:

			/* Aggregate when a Block Ack session is up and the rate allows it. */
			frame = (WlanWirelessFrame *)__aeabi_uread4(request->frame);
			if (wlanTxBaFind(macTxDescriptor.priority, frame->address1, &baIndex,
					 wlanTxBaSessions) != 0) {
				if (wlanRateAmpduAllowed(MAC_TX_RATE_INDEX(request->rate))) {
					wlanTrace(0xbbbbbbbb, 0x44444444);
					ampdu = 1;
				} else {
					wlanTrace(0xbbbbbbbb, 0x55555555);
					ampdu = 2;
				}
			} else {
				wlanTrace(0xbbbbbbbb, 0x66666666);
				ampdu = 0;
			}

			buffer->dataOffset = 28;
			data = buffer->data;
			loopback = NULL;
			built = wlanTxBuildFrame(&build, buffer, &drop, &eapol, 0,
						 descriptor->header.wire.packetType[0], ampdu == 1,
						 &loopback, qos);
			if (drop != 0 || built == NULL)
				goto complete;
			if (eapol != 0 && lowRate != 0 && MAC_TX_RATE_INDEX(request->rate) > 3) {
				request->rate = (request->rate & ~MAC_TX_RATE_MASK) + MAC_TX_RATE(3);
				macTxSetFlags(request, 0, MAC_TX_FLAG_RTS | MAC_TX_FLAG_CTS_TO_SELF);
			}
			control = macTxDescriptor.control;
			rate = MAC_TX_RATE_INDEX(request->rate);
			txControl = rate | 0x10 | control >> 5 << 5;
			macTxDescriptor.control = txControl;
			if (loopback != NULL)
				tx.loopback = loopback;
			tx.frame = built;
			__aeabi_uwrite4((int)txControl, &tx.descriptor.priority);
			tx.queue = 0;
			tx.descriptor.header.wire.packetLength[0] = rate;
			tx.descriptor.header.packetType = request->length;
			sequence = ((built[25] << 8 | built[24]) & 0xf) | (build.sequenceControl >> 4 << 4);
			built[24] = (uint8_t)sequence;
			built[25] = (uint8_t)(sequence >> 8);
			tx.descriptor.header.bssType = 0;
			tx.descriptor.header.bssNumber = 0;
			if ((built[2] >> 4) == 8)
				wlanTxSetQosControl(request, &tx, macTxDescriptor.control, build.tid,
						    macTxDescriptor.header.packetType == 230);

			/* The ring. */
			if (connection->mode == 3 || connection->mode == 1) {
				if (!wlanWmmSetQueue(connection, (uint8_t *)&tx, macTxDescriptor.priority,
						     macTxDescriptor.delay * 2))
					goto complete;
			} else if (connection->interfaceType == 1 || connection->mode == 2) {
				category = qos;
				if (qos != 0) {
					if (ampdu != 0) {
						wlanTxBaWindowId(baIndex, &tx.queue, wlanTxBaSessions);
						goto queued;
					}
					category = wlanTidToAccessCategory(macTxDescriptor.priority);
				}
				tx.queue = wlanAccessCategoryToQueue(category);
			}
queued:
			if (eapol != 0 || ((connection->interfaceType == 0 || connection->mode == 3) &&
					   wlanPsLifetimeWanted())) {
				tx.descriptor.header.packetOffset = 0x7fff;
				if (ampdu != 1)
					__aeabi_uwrite4((int)wlanTxControlSetRetryLimit(
								__aeabi_uread4(&tx.descriptor.priority), 14),
							&tx.descriptor.priority);
			}
			wlanTxPrepareRequest(&tx, request);
			if (ampdu == 1) {
				wlanTrace(0xbbbbbbbb, 0x77777777);
				wlanTxBaCountFrame(baIndex, wlanTxBaSessions);
				macTxSetFlags(request, 0x20000000, 0);
				request->mode = 10;
			} else {
				wlanTrace(0xbbbbbbbb, 0x88888888);
				wlanTxSetSecurity(connection, &tx, request);
			}
			if (macTxDescriptor.flags & 8) {
				macTxAdvanceStage();
				macTxSetFlags(request, 0x80000000u, 0);
			}
			tx.descriptor.control = __aeabi_uread4(request->flags);
			tx.sequence[0] = request->sequence[0];
			tx.sequence[1] = request->sequence[1];
			wlanTxSetRetry(connection, &tx, request);
			__aeabi_uwrite4((int)(__aeabi_uread4(request->flags) & 0x0fffffff), request->flags);
			request->type = tx.descriptor.header.bssType;
			request->info = tx.descriptor.header.bssNumber;
			__aeabi_uwrite4((int)data, request->buffer);
			__aeabi_uwrite4(0, request->next);
			__aeabi_memcpy4(data, &tx, offsetof(struct WlanReplyFrame, frameLength));
			wlanTrace(0xbbbbbbbb, 0x99999999);
			wlanTrace(0xbbbbbbbb, tx.descriptor.control);
			wlanTrace(0xbbbbbbbb, tx.queue);
			if (macTxBlockAckWindow(connection, ampdu, tx.queue, baIndex, (uint32_t)data,
						(uint8_t *)&tx) != 0)
				macTxSendDataFrame(buffer);
			continue;
complete:
			wlanCompletePacket(buffer, 0);
		}
		wlanHostSleepPending = 0;
	}
}

/* Sends every frame still queued in window 5 or 6 and clears its state. */
void wlanTxWindowFlush(unsigned int ring)
{
	unsigned int window = ring - 5;
	uint32_t entry;
	UINT posture;

	if (window >= 2)
		return;
	posture = sysIrqDisable();
	if ((WLAN_TX_WINDOW_DRAINING << window) & wlanTxWindowFlags) {
		while (macTxDequeue(window, &entry) == 0)
			macTxSendDataFrame(macTxEntryBuffer(entry));
	}
	wlanTxWindowFlags &= ~((WLAN_TX_WINDOW_DRAINING | WLAN_TX_WINDOW_ENABLED |
				WLAN_TX_WINDOW_OPEN) << window);
	__aeabi_memclr4(&wlanTxWindowQueues[window], sizeof(wlanTxWindowQueues[0]));
	sysIrqRestore(posture);
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

/* Takes back every frame of a connection from the first four transmit rings. */
void macTxFlushConnection(struct WlanConnection *connection)
{
	UINT posture = sysIrqDisable();
	uint32_t wakeBit;
	int ring;

	wlanTrace(0xbbbbbbbb, 0xffff1111);
	wakeBit = macTxAcquireWakeBit();
	cbProcFlushConnection(connection);
	for (ring = 0; ring < 4; ++ring) {
		wlanTxRingFlush((uint8_t)ring, (uintptr_t)connection);
		wlanTxRingRequeueFilled((uint8_t)ring, (uintptr_t)connection);
	}
	macTxReleaseWakeBits(&wakeBit);
	sysIrqRestore(posture);
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

int macTxGetDrainWaits(void)
{
	return macTxDrainWaits;
}

void macTxSetDrainWaits(int value)
{
	macTxDrainWaits = value;
}

/* Release callback of an exclusive null frame. */
void macTxNullReleased(void)
{
	macTxNullBusy = 0;
}

/*
 * True when the MAC has sent everything it was given: with per-queue
 * pointers (MAC_370 bit 2) every queue up to the last one in use, else the
 * single pair of pointers.
 */
int macTxHardwareIdle(void)
{
	int idle = 1;
	unsigned int last, queue;

	if ((MAC_370_QUEUE_MODE & 4) == 0)
		return MAC_028_TX_DONE == MAC_02C_TX_QUEUED;
	last = wlanTxRingLastQueue();
	queue = 0;
	do {
		if (queue > last)
			break;
		if (MAC_934_QUEUE_DONE(queue) != MAC_930_QUEUE_QUEUED(queue))
			idle = 0;
		++queue;
	} while (idle == 1);
	return idle;
}

int macTxIdle(void)
{
	if (!macTxHardwareIdle())
		return 0;
	return macTxQueuesIdle();
}

/* True when every buffer of a pool is back. */
static inline int macTxPoolFull(struct OsaBufferPool *pool)
{
	int blocks = osa_BufferClone(pool);
	int free = osa_PoolQuery(pool, 8);

	return blocks == free + osa_PoolQuery(pool, 16);
}

/* True when nothing is pending anywhere: host side, SDIO, the MAC and the frame pools. */
int macTxPoolsIdle(void)
{
	if (wlanHostSleepPending != 0 || mdevIoctl(wlanSdioDev, 8, 0) != 0 || !macTxIdle())
		return 0;
	return macTxPoolFull(&wlanBlockPool28) && macTxPoolFull(&wlanBlockPool7) &&
	       macTxPoolFull(&wlanReplyFramePool);
}

/* Without an IBSS, waits until the host side is done and everything has been sent. */
void macTxWaitIdle(void)
{
	if (wlanConnectionExists(WLAN_IF_IBSS))
		return;
	while (wlanHostSleepPending != 0 || mdevIoctl(wlanSdioDev, 8, 0) != 0) {
		++macTxDrainWaits;
		macTxPostEvent(1);
		osaTaskSleep(1);
	}
	while (!macTxIdle() || !macTxPoolFull(&wlanBlockPool28) || !macTxPoolFull(&wlanBlockPool7) ||
	       !macTxPoolFull(&wlanReplyFramePool))
		osaTaskSleep(1);
}

/* Release callback of the macTxSendPsPoll() buffer. */
void macTxPsPollReleased(void)
{
	macTxPsPollBusy = 0;
}

/*
 * Sends a PS-Poll for the time sharing, one at a time: urgent is passed to
 * wlanCoexMayTransmit() and wlanCoexTxQueued(). Returns 1 if it went out.
 */
int macTxSendPsPoll(struct WlanConnection *connection, int urgent)
{
	OsaBuffer *buffer;
	struct WlanReplyFrame *request;
	WlanWirelessFrame *frame;

	if (macTxPsPollBusy != 0)
		return 0;
	if (!wlanCoexMayTransmit(urgent))
		return 0;
	buffer = osa_BufferAlloc(&wlanBlockPool7, 300, 0);
	if (buffer == NULL)
		return 0;
	macTxPsPollBusy = 1;
	buffer->packetClass = 2;
	buffer->context = connection;
	buffer->flags |= 1;
	buffer->release = (int (*)(OsaBuffer *))macTxPsPollReleased;
	buffer->dataOffset = 176;
	frame = wlanBuildPsPoll(buffer, connection->interfaceAddress, connection->localAddress,
				wlanLocalState->powerSave, connection->associationId);
	buffer->dataOffset = 28;
	request = (struct WlanReplyFrame *)buffer->data;
	__aeabi_memclr4(request, offsetof(struct WlanReplyFrame, frameLength));
	request->frame = frame->bodyLength;
	request->descriptor.header.wire.packetLength[0] = wlanTxControlRate(connection);
	request->descriptor.control = 0x402;
	if (!wlanCoexBtNeedsSharing())
		request->descriptor.control |= 0x1000;
	if ((wlanProtection.flags & WLAN_PROTECTION_LONG_PREAMBLE) == 0 && request->descriptor.header.wire.packetLength[0] != 0)
		request->descriptor.control |= 0x40000000;
	request->queue = wlanCoexActive() ? wlanCoexRetryLimit() : 3;
	if (wlanTxSubmit(buffer) == 0) {
		osa_BufferFree(buffer);
		return 0;
	}
	wlanCoexTxQueued(urgent);
	return 1;
}

/*
 * Sends a null data frame (QoS null with WMM) to the access point, with the
 * power management bit as asked. An exclusive frame waits for the previous
 * one: while that is still queued in ring 0 the notify task is woken
 * instead. A trigger frame goes at the data rate and, with triggers in use,
 * straight into its ring.
 */
void macTxSendNullFrame(struct WlanConnection *connection, int powerManagement,
			unsigned int priority, int exclusive, int trigger, uint8_t kind)
{
	OsaBuffer *buffer;
	struct WlanReplyFrame *request;
	WlanWirelessFrame *frame;
	unsigned int subtype;

	if (exclusive != 0 && macTxNullBusy != 0) {
		if (MAC_010_QUEUE_HEAD != MAC_014_QUEUE_TAIL) {
			++macTxNullDeferred;
			osaEventFlagsSet(macTxNotifyEventFlags, 1, 7);
			wlanRaiseEvent(WLAN_EVENT_PS_POLL);
		}
		return;
	}
	while ((buffer = wlanMgmtAllocFrame(connection, 300)) == NULL)
		osaTaskSleep(1);
	frame = (WlanWirelessFrame *)((uint8_t *)buffer + buffer->dataOffset);
	subtype = wlanWmmEnabled() ? 12 : 4;
	wlanInitMgmtHeader(frame, 2, subtype, connection->localAddress,
			   connection->interfaceAddress, connection->localAddress);
	frame->frameControl[1] = ((frame->frameControl[1] | 1) & 0xef) |
				 ((powerManagement << 4) & 0x10);
	frame->duration[0] = 0x02;
	frame->duration[1] = 0x01;
	frame->bodyLength[0] = 0;
	frame->bodyLength[1] = 0;

	buffer->dataOffset = 28;
	buffer->packetClass = 2;
	request = (struct WlanReplyFrame *)buffer->data;
	__aeabi_memclr4(request, offsetof(struct WlanReplyFrame, frameLength));
	request->kind = kind;
	request->frame = frame->bodyLength;
	if (exclusive != 0) {
		request->descriptor.control = 0x80000000;
		macTxNullBusy = 1;
		buffer->flags |= 1;
		buffer->release = (int (*)(OsaBuffer *))macTxNullReleased;
	}
	request->descriptor.header.bssType = 0;
	request->descriptor.header.bssNumber = 0;
	if (subtype == 12) {
		uint8_t *info = &request->descriptor.header.bssType;
		unsigned int sequence;

		*info = (*info >> 3 << 3) | (wlanWmmTid(priority) & 7);
		sequence = wlanNextSequence(connection, *info & 7);
		sequence = ((frame->sequenceControl[1] << 8 | frame->sequenceControl[0]) & 0xf) |
			   sequence << 4;
		frame->sequenceControl[0] = (uint8_t)sequence;
		frame->sequenceControl[1] = (uint8_t)(sequence >> 8);
		frame->sequenceControl[0] = frame->sequenceControl[0] >> 4 << 4;
		*info &= 0x9f;
	}
	wlanWmmSetQueue(connection, buffer->data, priority, 0);

	if (trigger == 0) {
		request->queue = 4;
		request->descriptor.header.wire.packetLength[0] = wlanTxControlRate(connection);
		request->descriptor.control |= 0x1000;
	} else {
		uint8_t rate = wlanTxDataRate(connection, 0);

		request->descriptor.header.wire.packetLength[0] = rate;
		if (rate > 3 &&
		    (((wlanProtection.flags & WLAN_PROTECTION_USE_PROTECTION) && !(wlanTxRateFlags & 0x40)) ||
		     (connection->mode == 2 && (connection->stationInfo->rateFlags & 8)))) {
			request->descriptor.header.wire.packetLength[0] = wlanTxLowRate(connection, 3);
			request->descriptor.control |= 0x1000;
		}
	}
	if ((wlanProtection.flags & WLAN_PROTECTION_LONG_PREAMBLE) == 0 && request->descriptor.header.wire.packetLength[0] != 0)
		request->descriptor.control |= 0x40000000;

	if (wlanPsTriggerMode() != 0 && trigger != 0) {
		wlanTxInFlightAdd(buffer);
		if (wlanTxQueueFrame(buffer) != 0) {
			wlanPsTriggerSent();
			return;
		}
		wlanTxInFlightRemove(buffer);
	} else {
		if (wlanPsUapsdActive(connection))
			wlanConnectionPsState(connection)->powerManagement = powerManagement;
		if (wlanTxSubmit(buffer) != 0)
			return;
	}
	wlanTxCountDropped(buffer);
	osa_BufferFree(buffer);
}

/* Block Ack timeout of a session in the units of the rate control of its connection. */
uint32_t wlanTxBaTimeoutRegister(uintptr_t owner, uint32_t timeout)
{
	return wlanRateTimeoutValue(((struct WlanConnection *)owner)->rateControl, timeout, 14);
}

/*
 * Ends the transmit Block Ack session of a peer and TID: frames waiting in
 * its window go back to the queue of the TID's access category. Returns 1 if
 * there was a session.
 */
int macTxBaDelete(struct WlanConnection *connection, const uint8_t *peer, unsigned int tid)
{
	uint8_t windowId, hwSlot;
	unsigned int queue;
	uint32_t wakeBit;
	UINT posture;

	wlanTxBaRemove(peer, tid, &windowId, &hwSlot, wlanTxBaSessions);
	if (windowId != 5 && windowId != 6)
		return 0;
	tid = (uint8_t)tid;
	queue = wlanAccessCategoryToQueue(wlanTidToAccessCategory(tid));
	if (connection->mode == 3 || connection->mode == 1)
		cbProcSetBaWindow(tid, queue, hwSlot);
	posture = sysIrqDisable();
	wakeBit = macTxAcquireWakeBit();
	wlanWindowFree(windowId, wlanWindowOwners, wlanTxWindowIds);
	wlanBaSlotRelease(hwSlot, windowId);
	wlanTxWindowFlush(windowId);
	cbProcMoveFrames(windowId, queue, tid, connection->peerAddress);
	wlanBaSlotDetach(queue, hwSlot);
	wlanTxRingMovePending(windowId, queue);
	wlanTxRingMoveFilled(windowId, queue);
	macTxReleaseWakeBits(&wakeBit);
	sysIrqRestore(posture);
	wlanBaNotifyDeleted(connection, hwSlot, peer);
	macTxStatsReset(hwSlot);
	return 1;
}

/* Like macTxBaDelete(), but the frames of the window are dropped (the link is going away). */
int macTxBaAbort(struct WlanConnection *connection, const uint8_t *peer, unsigned int tid)
{
	uint8_t windowId, hwSlot;
	uint32_t wakeBit;
	UINT posture;

	wlanTxBaRemove(peer, tid, &windowId, &hwSlot, wlanTxBaSessions);
	wlanTrace(0xbbbbbbbb, 0xffff6666);
	if (windowId != 5 && windowId != 6)
		return 0;
	wlanTrace(0xbbbbbbbb, 0xffff7777);
	wlanAccessCategoryToQueue(wlanTidToAccessCategory((uint8_t)tid));
	posture = sysIrqDisable();
	wakeBit = macTxAcquireWakeBit();
	wlanWindowFree(windowId, wlanWindowOwners, wlanTxWindowIds);
	wlanBaSlotRelease(hwSlot, windowId);
	wlanTxWindowReset(windowId);
	cbProcFlushConnection(connection);
	wlanBaSlotClear(hwSlot);
	wlanTxRingFlush(windowId, 0);
	wlanTxRingRequeueFilled(windowId, 0);
	macTxReleaseWakeBits(&wakeBit);
	sysIrqRestore(posture);
	wlanBaNotifyDeleted(connection, hwSlot, peer);
	return 1;
}

/* Ends the receive Block Ack session of a peer and TID; 1 if there was one. */
int macRxBaDelete(const uint8_t *peer, unsigned int tid)
{
	int deleted = 0;
	UINT posture = sysIrqDisable();
	unsigned int index = wlanRxBaFind(peer, (uint8_t)tid, wlanRxBaSessions);

	if (index < WLAN_RX_BA_MAX) {
		wlanRxBaSessions[index].state = 0;
		wlanRxBaSessions[index].frameCount = 0;
		wlanRxBaHwRelease(peer, tid);
		wlanBaTablesChangedHandler();
		deleted = 1;
	}
	sysIrqRestore(posture);
	return deleted;
}

/*
 * Starts a transmit Block Ack session from an accepted request: a hardware
 * slot and window are taken, and the frames of the TID waiting in its
 * access-category queue move to the window. Returns 1 on success.
 */
int macTxBaAdd(struct WlanConnection *connection, const struct WlanBaRequest *request)
{
	unsigned int hwSlot, windowId, queue;
	int window;
	uint32_t wakeBit;
	UINT posture;

	if (((request->flags << 28) >> 29) != 0)
		return 0;
	hwSlot = wlanTxBaFindFreeHwSlot();
	if (hwSlot == 2)
		return 0;
	window = wlanWindowAlloc(wlanTxWindowIds);
	if (window == -1)
		return 0;
	hwSlot = (uint8_t)hwSlot;
	windowId = (uint8_t)window;
	if (!wlanBaSlotSetup(request, windowId, hwSlot))
		return 0;
	wlanBaNotifyAdded(connection, hwSlot, request->peer);
	if (connection->mode == 3 || connection->mode == 1)
		cbProcSetBaWindow((uint8_t)__aeabi_uread4(request->tid), windowId, hwSlot);
	wlanBaWindowCounterReset(windowId);
	wlanBaWindowEnable(window);
	wlanWindowOwners[window] = 1;
	wlanTxBaAdd((uintptr_t)connection, request, hwSlot, windowId, wlanTxBaSessions);
	macTxActivateWindow(windowId);
	queue = wlanAccessCategoryToQueue(
		wlanTidToAccessCategory((uint8_t)__aeabi_uread4(request->tid)));
	posture = sysIrqDisable();
	wakeBit = macTxAcquireWakeBit();
	cbProcMoveFrames(queue, windowId, (uint8_t)__aeabi_uread4(request->tid),
			 connection->peerAddress);
	wlanTxRingMoveToWindow(windowId, queue, (uint8_t)__aeabi_uread4(request->tid),
			       connection->peerAddress);
	wlanTxRingRouteFilled(windowId, queue, (uint8_t)__aeabi_uread4(request->tid),
			      connection->peerAddress);
	macTxReleaseWakeBits(&wakeBit);
	sysIrqRestore(posture);
	return 1;
}

/* Gives every transmit Block Ack slot the next sequence number of its peer and TID. */
void macTxBaResyncSequences(void)
{
	unsigned int index;

	for (index = 0; index < WLAN_TX_BA_MAX; index = (uint8_t)(index + 1)) {
		struct WlanTxBaSession *session = &wlanTxBaSessions[index];
		struct WlanConnection *connection;
		unsigned int sequence;

		if (session->active == 0)
			continue;
		connection = wlanConnectionFindByPeer(session->peer);
		if (connection != NULL &&
		    (connection->interfaceType == WLAN_IF_IBSS ||
		     connection->interfaceType == WLAN_IF_STATION || wlanP2pConnection(connection)))
			sequence = wlanPeerNextSequence(connection, session->tid, 0);
		else
			sequence = wlanGroupNextSequence(session->tid);
		wlanBaSlotSetSequence(session->hwSlot, (uint16_t)(sequence << 4));
	}
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
