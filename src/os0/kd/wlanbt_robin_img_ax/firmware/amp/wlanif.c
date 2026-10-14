/*
 * The WLAN-facing side of the AMP PAL: the "btamp_pal" mdev driver the WLAN
 * stack calls, its link-control messages, the access-window timers and the
 * association request sent for a link. The filename is inferred.
 */
#include <stddef.h>
#include "robin/amp.h"
#include "robin/btrom.h"
#include "robin/mdev.h"
#include "robin/rom.h"
#include "robin/runtime.h"
#include "robin/soc.h"
#include "robin/sync.h"
#include "robin/sys.h"
#include "robin/task.h"
#include "robin/wlan.h"

/* Link-control messages the AMP side sends through the "btamp_pal" device. */
enum {
	AMP_WLAN_SET_ADDRESS = 0,
	AMP_WLAN_CREATE_LINK = 1,
	AMP_WLAN_ACCEPT_LINK = 2,
	AMP_WLAN_DISCONNECT_LINK = 3,
	AMP_WLAN_RELEASE_4 = 4,
	AMP_WLAN_RELEASE_5 = 5,
	AMP_WLAN_RELEASE_8 = 8,
	AMP_WLAN_ACTIVITY_REPORT = 9,
	AMP_WLAN_SHORT_RANGE = 10,
	AMP_WLAN_STATUS = 11,
	AMP_WLAN_SCAN_SETTINGS = 12,
	AMP_WLAN_SUPERVISION = 13,
	AMP_WLAN_SET_ADDRESS_REVERSED = 14,
};

/* Events delivered to the driver. */
enum {
	AMP_WLAN_EVENT_CONTROL = 1,
	AMP_WLAN_EVENT_SECURITY_FRAME = 3,
	AMP_WLAN_EVENT_TRANSMIT = 4,
	AMP_WLAN_EVENT_RECEIVE = 5,
	AMP_WLAN_EVENT_SCHEDULE = 21,
};

/* Control commands of the driver (mdevIoctl on the device). */
enum {
	AMP_WLAN_IOCTL_DROP_PACKET = 20,
	AMP_WLAN_IOCTL_SUPERVISION_REQUEST = 80,
	AMP_WLAN_IOCTL_SUPERVISION_REPLY = 81,
	AMP_WLAN_IOCTL_COMPLETED = 83,
	AMP_WLAN_IOCTL_START_WINDOW = 84,
	AMP_WLAN_IOCTL_CHANNEL_SET = 85,
	AMP_WLAN_IOCTL_DATA_LOOP = 86,
};

/* Link-control message as sent by the AMP side. */
struct AmpWlanMessage {
	uint8_t code[2];
	uint8_t length;
	uint8_t reserved;
	uint8_t first[4];	/* record pointer or physical handle, by code */
	uint8_t second[4];
};

/* Record behind the pointer of the create, accept and disconnect messages. */
struct AmpLinkRequest {
	uint8_t reserved0;
	uint8_t physicalHandle;
	uint8_t policy;
	uint8_t state;
	uint8_t reserved1[4];
	uint8_t key[32];
	uint8_t reserved2[88];
	uint8_t peer[6];
};

/* Provisional names of unconverted callees. */
extern struct OsaEventFlags *ampEventFlags;
extern struct OsaBufferPool wlanFramePool;
OsaBuffer *wlanAllocateFrame(struct OsaBufferPool *pool, uint32_t size, uint32_t a, uint32_t b);
int macTxHandlePacket(void);				/* 0xcff8 */
struct WlanConnection *wlanConnectionConfigure(int interfaceType, int bssType, int bssNumber,
					       const uint8_t *local, const uint8_t *peer,
					       int policy, void *driver);	/* 0x1d604 */
int macTxApplyQueue(void *slot, int value, int flag);	/* 0x10096 */
int macTxReleaseTimer(int which);			/* 0x10ce0 */
int macTxClearChannel(struct WlanConnection *connection, const uint8_t *body, unsigned int length);	/* 0x12678 */
int wlanBuildRsnElement(uint8_t *out);			/* 0x2094, security element builder */
/* PLACEHOLDER: cryptographic routine, deferred (link key cache, 0x6632 and 0x66b0). */
int wlanKeyCacheStore(const uint8_t *peer, const uint8_t *key);
int wlanKeyCacheRemove(const uint8_t *peer);
extern uint8_t wlanConnectionPhase;			/* 0x54b70 */
extern uint32_t wlanPrimaryEvents;			/* 0x4008828 */
extern uint32_t wlanSlot181[];				/* connection tail used by macTxApplyQueue */

uint8_t ampLocalFrameAddress[6] __attribute__((section(".itcm.rw")));
const char *ampPalDriverName __attribute__((section(".itcm.rw"))) = "btamp_pal";
uint32_t ampLinkSupervisionInterval __attribute__((section(".itcm.rw"))) = 10000000;
uint8_t ampScanParameter __attribute__((section(".itcm.rw"))) = 1;
uint8_t ampScanChannel __attribute__((section(".itcm.rw"))) = 1;
uint8_t ampActivityPayloadLength __attribute__((section(".itcm.rw")));
uint8_t ampSupportedRates[12] __attribute__((section(".itcm.rw"))) = {
	0x82, 0x84, 0x8b, 0x96, 0x0c, 0x12, 0x18, 0x24, 0x30, 0x48, 0x60, 0x6c,
};
OsaBuffer *ampSecurityFrame __attribute__((section(".itcm.rw")));
OsaBuffer *ampPalLastTransmit __attribute__((section(".itcm.rw")));
uint32_t ampPalWindowTimer __attribute__((section(".itcm.rw")));
struct OsaEventFlags *ampWlanEventFlags __attribute__((section(".itcm.rw")));
uint8_t ampActivityPayload[2] __attribute__((section(".itcm.bss")));
uint8_t ampActivityTimestamp[4] __attribute__((section(".itcm.bss")));
uint8_t ampScanEnabled __attribute__((section(".itcm.bss")));
static uint8_t ampPalLocalName[48] __attribute__((section(".itcm.bss")));
static uint8_t ampWlanFlagsStorage[32] __attribute__((section(".itcm.bss")));
struct MdevDriver ampWlanDriver __attribute__((section(".dtcm.bss")));

/* Copies count bytes of source in reverse order. */
void ampReverseBytes(const uint8_t *source, int count, uint8_t *destination)
{
	int i;

	for (i = 0; i < count; i++)
		destination[i] = source[count - i - 1];
}

/* Records the controller address (the stack gives it reversed when asked). */
void ampPalSetLocalAddress(const uint8_t *address, int reversed)
{
	if (reversed)
		ampReverseBytes(address, 6, ampLocalFrameAddress);
	else
		__aeabi_memcpy(ampLocalFrameAddress, address, 6);
	__aeabi_memcpy4(&ampLocalAssoc, ampLocalFrameAddress, 6);
}

extern const uint8_t *wlanDeviceAddress;	/* 0x54a7c */

void ampPalLoadLocalAddress(void)
{
	__aeabi_memcpy4(ampLocalFrameAddress, wlanDeviceAddress, 6);
}

int ampPalQueryLocalAddress(void)
{
	ampPalLoadLocalAddress();
	return 0;
}

int ampPalNoop(void)
{
	return 0;
}

static unsigned int get16(const uint8_t *p)
{
	return p[0] | (p[1] << 8);
}

static void put16(uint8_t *p, unsigned int value)
{
	p[0] = value;
	p[1] = value >> 8;
}

/* Starts a physical link on the WLAN side: sends the association request. */
void ampPalStartLink(struct WlanConnection *connection)
{
	OsaBuffer *buffer;
	uint8_t *command;
	uint8_t *tlv;
	uint8_t *next;
	unsigned int ssidLength;
	unsigned int size;

	buffer = wlanAllocateFrame(&wlanFramePool, 0x100, 0, 3);
	if (!buffer)
		return;
	buffer->context = connection;
	buffer->dataOffset = 176;
	command = (uint8_t *)buffer + 176;
	command[0] = 0;
	command[1] = 0;
	command[5] = (command[5] & 0xf0) | (connection->bssNumber & 0xf);
	command[5] = (command[5] & 0xf) | (connection->bssType << 4);
	__aeabi_memcpy(command + 8, connection->peerAddress, 6);
	put16(command + 14, 0x0431);
	put16(command + 16, 0x0100);
	put16(command + 18, connection->capability);
	command[20] = 16;
	/* SSID TLV */
	put16(command + 21, 0);
	ssidLength = strlen(connection->ssid);
	put16(command + 23, ssidLength);
	__aeabi_memcpy(command + 25, connection->ssid, ssidLength);
	/* DS parameter TLV */
	tlv = command + 25 + ssidLength;
	put16(tlv, 3);
	put16(tlv + 2, 1);
	tlv[4] = connection->channel;
	/* Supported rates TLV */
	next = tlv + 5;
	put16(next, 1);
	put16(next + 2, 12);
	__aeabi_memcpy(next + 4, ampSupportedRates, 12);
	/* Channel list TLV */
	put16(next + 16, 0x0101);
	put16(next + 18, 7);
	__aeabi_memclr(next + 20, 7);
	next[21] = connection->channel;
	next[20] = (4 * (next[20] >> 2)) & 0xf3;
	tlv = next + 27;
	size = ssidLength + 57;
	if (wlanBuildRsnElement(tlv + 2) != 0) {
		/* Turn the element header (id, length) into a TLV header. */
		unsigned int length = tlv[3];

		put16(tlv, 48);
		put16(tlv + 2, length);
		size += length + 4;
	}
	put16(command + 2, size);
	if (macTxClearChannel(connection, command + 8, (uint16_t)(size - 8)) == 0)
		ampPalNotify(connection->bssNumber, 5);
	osa_BufferFree(buffer);
}

/* Events from the WLAN device. */
int ampPalWlanEvent(void *device, OsaBuffer *buffer, uintptr_t argument, unsigned int event)
{
	struct WlanConnection *connection;
	struct AmpWlanMessage *message;
	struct AmpLinkRequest *request;
	OsaBuffer *release = NULL;
	uint8_t *data;
	unsigned int handle;
	unsigned int physical;
	unsigned int priority;
	int isNew = 1;

	if (event == AMP_WLAN_EVENT_TRANSMIT) {
		ampPalLastTransmit = buffer;
		data = (uint8_t *)buffer + buffer->dataOffset - 4;
		handle = ((data[1] << 8) & 0xfff) | data[0];
		priority = 0;
		if (ampLogicalLinkServiceType(handle) == 2)
			priority = 4;
		physical = ampPhysicalLinkFindByLogical(handle);
		connection = wlanConnectionFind(WLAN_BSS_TYPE_AMP, physical);
		if (!connection) {
			osa_BufferFree(buffer);
			return 0;
		}
		buffer->context = connection;
		ampPalTransmit(buffer, get16(data + 2), priority, 1);
		return 0;
	}
	if (event == AMP_WLAN_EVENT_RECEIVE) {
		connection = buffer->context;
		physical = connection->bssNumber;
		ampPalLinkAlive(connection);
		ampPalDataToHost(buffer, physical);
		return 0;
	}
	if (event == AMP_WLAN_EVENT_SCHEDULE) {
		uint8_t *schedule = (uint8_t *)buffer + buffer->dataOffset;

		connection = buffer->context;
		if (ampPalWindowTimer) {
			osaCancelTimer(ampPalWindowTimer);
			ampPalWindowTimer = 0;
		}
		if (schedule[1] == 0 && schedule[0] == 1) {
			connection->flags &= ~8;
		} else if (schedule[0] == 0) {
			connection->flags |= 8;
		} else if (schedule[1] != 0 && schedule[0] == 1) {
			connection->flags &= ~8;
			__aeabi_memcpy(&connection->schedule, schedule, sizeof(connection->schedule));
			connection->scheduleValid = 1;
			macTxReleaseTimer(2);
		}
		wlanReleasePacket(buffer);
		return 0;
	}
	if (event == AMP_WLAN_EVENT_SECURITY_FRAME) {
		ampSecurityFrame = buffer;
		osaEventFlagsSet(ampEventFlags, 8, 7);
		return 0;
	}
	if (event == AMP_WLAN_EVENT_CONTROL) {
		release = buffer;
		message = (struct AmpWlanMessage *)((uint8_t *)buffer + buffer->dataOffset);
		switch (message->code[0]) {
		case AMP_WLAN_SET_ADDRESS:
			__aeabi_memcpy((uint8_t *)(uintptr_t)__aeabi_uread4(message->first + 1) + 88,
				       ampLocalFrameAddress, 6);
			break;
		case AMP_WLAN_CREATE_LINK:
		case AMP_WLAN_ACCEPT_LINK:
			request = (struct AmpLinkRequest *)(uintptr_t)__aeabi_uread4(message->first);
			physical = ampPhysicalLinkFind(request->physicalHandle);
			if (wlanConnectionExists(WLAN_IF_AMP)) {
				isNew = 0;
			} else {
				wlanPsCancelPendingTimer();
				if (wlanConnectionPhase == 5) {
					wlanPostConnectionEvent(wlanConnectionByRole(wlanHasApRole() ? 2 : 0), 4096);
					wlanConnectionPhase = 6;
				}
				wlanPrimaryEvents |= 1;
				if (sysEventEngineIsActive())
					sysProcessEventQueues(0x400000);
				macTxHandlePacket();
				SOC_PENDING_EVENT_CONTROL |= 4;
			}
			connection = wlanConnectionConfigure(WLAN_IF_AMP, WLAN_BSS_TYPE_AMP, physical,
							     ampLocalFrameAddress,
							     request->peer,
							     request->policy, &ampWlanDriver);
			if (!connection)
				for (;;)
					;
			ampPalFormatSsid((char *)ampPalLocalName, connection->localAddress);
			connection->localName.length = strlen((const char *)ampPalLocalName);
			connection->localName.name = (const char *)ampPalLocalName;
			ampPalFormatSsid(connection->ssid, connection->peerAddress);
			wlanConnectionMarkReady();
			wlanKeyCacheStore(connection->peerAddress, (const uint8_t *)request + 8);
			ampPalPrepareTxHeader(connection);
			if (isNew)
				mdevIoctl(wlanDevice, 0x58, (uintptr_t)connection);
			mdevIoctl(wlanDevice, 0x59, (uintptr_t)connection);
			ampPalNotify(physical, 1);
			if (get16(message->code) == AMP_WLAN_CREATE_LINK) {
				connection->initiator = 0;
				connection->state = 4;
				do
					osaTaskSleep(1);
				while (!wlanConnectEventTryWait());
			} else {
				connection->initiator = 1;
				ampPalStartLink(connection);
			}
			break;
		case AMP_WLAN_DISCONNECT_LINK:
			request = (struct AmpLinkRequest *)(uintptr_t)__aeabi_uread4(message->first);
			physical = ampPhysicalLinkFind(request->physicalHandle);
			connection = wlanConnectionFind(WLAN_BSS_TYPE_AMP, physical);
			if (connection) {
				connection->state = 0;
				osaCancelTimer(connection->timerHandle);
				connection->retryCount = 0;
				wlanKeyCacheRemove(connection->peerAddress);
				while (connection->transmitBusy)
					osaTaskSleep(1);
				wlanConnectionDecrement();
				wlanConnectionRelease(connection);
			}
			ampPalNotify(physical, 8);
			break;
		case AMP_WLAN_RELEASE_4:
		case AMP_WLAN_RELEASE_5:
		case AMP_WLAN_RELEASE_8:
			((uint8_t *)(uintptr_t)__aeabi_uread4(message->second))[2] = 0;
			break;
		case AMP_WLAN_ACTIVITY_REPORT:
			physical = ampPhysicalLinkFind(message->first[0]);
			ampActivityPayloadLength = message->length - 1;
			__aeabi_memcpy(ampActivityPayload, message->first + 1, ampActivityPayloadLength);
			__aeabi_uwrite4(__aeabi_uread4(ampActivityTimestamp) +
					(uint32_t)romGetTimerTimestamp(), ampActivityTimestamp);
			connection = wlanConnectionFind(WLAN_BSS_TYPE_AMP, physical);
			if (connection)
				ampPalSendActivityReport(connection);
			break;
		case AMP_WLAN_SHORT_RANGE:
			request = (struct AmpLinkRequest *)(uintptr_t)__aeabi_uread4(message->first);
			connection = wlanConnectionFirst(WLAN_IF_AMP);
			macTxApplyQueue((uint32_t *)connection + 181, request->state ? 0 : 255, 1);
			ampPalNotifyCode10(ampPhysicalLinkFind(request->physicalHandle), request->state);
			break;
		case AMP_WLAN_STATUS: {
			uint32_t status = get16(message->first);

			mdevIoctl(wlanDevice, 0x52, (uintptr_t)&status);
			break;
		}
		case AMP_WLAN_SCAN_SETTINGS:
			ampScanParameter = message->first[1];
			ampScanChannel = message->first[2];
			ampScanEnabled = ampScanChannel != 0;
			break;
		case AMP_WLAN_SUPERVISION:
			ampLinkSupervisionInterval = __aeabi_uread4(message->first) >> 1;
			break;
		case AMP_WLAN_SET_ADDRESS_REVERSED:
			ampPalSetLocalAddress(message->first + 1, 1);
			break;
		default:
			break;
		}
		osa_BufferFree(release);
		return 0;
	}
	/* Other events carry nothing the PAL needs; event 2 frees its buffer. */
	if (event == 2)
		release = buffer;
	osa_BufferFree(release);
	return 0;
}

/* Timer: the access window of a scheduled link ends. */
void ampPalWindowClose(void)
{
	struct WlanConnection *connection = wlanConnectionGetSession();

	MAC_REG(0x8e4) &= ~2u;
	osaCreateTimer((OsaTimerCallback)ampPalWindowOpen, (uint32_t)connection,
		       __aeabi_uread4(connection->schedule.interval) -
		       __aeabi_uread4(connection->schedule.length),
		       &ampPalWindowTimer, 0);
}

/* Timer: the access window of a scheduled link begins. */
void ampPalWindowOpen(void)
{
	struct WlanConnection *connection = wlanConnectionGetSession();

	MAC_REG(0x2d4) |= 0x30;
	MAC_REG(0x8e4) |= 2;
	osaCreateTimer((OsaTimerCallback)ampPalWindowClose, (uint32_t)connection,
		       __aeabi_uread4(connection->schedule.length), &ampPalWindowTimer, 0);
}

/* Control commands of the driver. */
int ampPalWlanControl(void *device, int command, void *argument)
{
	OsaBuffer *buffer = argument;
	struct WlanConnection *connection;
	unsigned int handle;
	int delay;

	switch (command) {
	case AMP_WLAN_IOCTL_START_WINDOW:
		connection = argument;
		delay = __aeabi_uread4(connection->schedule.start) - connection->scheduleReference;
		if (delay < 0)
			delay += __aeabi_uread4(connection->schedule.interval);
		wlanConnectionSetSession(connection);
		osaCreateTimer((OsaTimerCallback)ampPalWindowOpen, (uint32_t)connection, delay,
			       &ampPalWindowTimer, 0);
		break;
	case AMP_WLAN_IOCTL_CHANNEL_SET:
		handle = (uint16_t)buffer->allocationMetadata;
		ampPalNotifyCode12(ampPhysicalLinkFindByLogical(handle), handle);
		break;
	case AMP_WLAN_IOCTL_DATA_LOOP:
		ampPalDataLoop();
		break;
	case AMP_WLAN_IOCTL_DROP_PACKET:
		ampPalDropPacket(buffer);
		break;
	case AMP_WLAN_IOCTL_SUPERVISION_REQUEST:
		ampPalSendEmptyFrame(AMP_PROTOCOL_LINK_SUPERVISION_REPLY, buffer->context);
		break;
	case AMP_WLAN_IOCTL_SUPERVISION_REPLY:
		ampPalLinkAlive(buffer->context);
		break;
	case AMP_WLAN_IOCTL_COMPLETED:
		handle = buffer->ringEnd;
		ampPalNotifyCode11(ampPhysicalLinkFindByLogical(handle), handle);
		break;
	default:
		break;
	}
	return 0;
}

/* Registers the "btamp_pal" device with the mdev framework. */
int ampWlanDriverInit(void)
{
	__aeabi_memclr4(&ampWlanDriver, sizeof(ampWlanDriver));
	ampWlanEventFlags = (struct OsaEventFlags *)ampWlanFlagsStorage;
	osaEventFlagsInit(&ampWlanEventFlags);
	ampWlanDriver.name = ampPalDriverName;
	ampWlanDriver.callbacks[0] = (void *)ampPalQueryLocalAddress;
	ampWlanDriver.callbacks[1] = (void *)ampPalNoop;
	ampWlanDriver.callbacks[2] = (void *)ampPalSetSupervisionInterval;
	ampWlanDriver.callbacks[3] = NULL;
	ampWlanDriver.callbacks[4] = (void *)ampPalWlanEvent;
	ampWlanDriver.callbacks[5] = (void *)ampPalWlanControl;
	ampWlanDriver.callbacks[6] = NULL;
	mdevRegisterDriver(&ampWlanDriver, 0);
	return 0;
}

/* True for an AMP connection owned by this driver; marks it ready to send. */
int ampPalOwnsConnection(struct WlanConnection *connection)
{
	int owns = 0;

	if (connection->interfaceType == WLAN_IF_AMP &&
	    strcmp(((struct MdevDriver *)connection->device)->name, ampPalDriverName) == 0) {
		connection->linkFlags = 3;
		owns = 1;
		macTxReleaseTimer(2);
	}
	return owns;
}
