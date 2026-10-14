/*
 * Host controller transport ("hct") of the Bluetooth controller: the thread
 * that moves HCI packets between the SDIO device "bt_sdio" and the Mask ROM
 * controller, the coexistence interval selection that goes with it, and the
 * AFH channel map derived from the WLAN channels in use.
 *
 * The filename and all role names are inferred; no source name survives.
 */
#include <stddef.h>
#include "robin/btrom.h"
#include "robin/hct.h"
#include "robin/mdev.h"
#include "robin/osa.h"
#include "robin/power.h"
#include "robin/rom.h"
#include "robin/runtime.h"
#include "robin/soc.h"
#include "robin/sys.h"
#include "robin/wlan.h"

/* Coexistence interval table and its helpers (rf/coex, not converted). */
int cbProcFilterHeader(int selector, uint32_t value);
uint32_t rfCalibHandleTable(void);
uint32_t rfCalibProcessTable(void);
uint32_t rfCalibUpdateTable(void);
int rfCalibFilterEntry(void);
int rfCalibRecordEntry(void);
int rfCalibTrackEntry(void);
int rfCalibSetupEntry(int enable);
int rfCalibReleaseEntry(int enable);
int rfCalibVerifyEntry(int enable);
uint32_t rfCalibParseState(int a1);
uint32_t rfCalibCheckState(uint32_t a1, int a2);
extern uint8_t cbProcPolicy;
extern uint32_t cbProcWindow;
int rfCalibConvertEntry(void);
extern uint32_t cbProcSecondaryCounter;
extern uint32_t romHctCoexMode;
extern uint32_t romCoexPrimaryFlag;

/* BT link controller RAM patches (bt/lmp, not converted). */
int btLmpLcVerifyContext(int a1);
void btLmpLcAcquireContext(int a1);
void btLmpLcReleaseContext(uint32_t type, uint32_t a, uint32_t b);
void *btLmpLcComputeEntry(uint32_t a1);
void btLmpLcCheckDescriptor(uint32_t handle, uint8_t *data, uint32_t type, uint32_t a4);
void btLmpLcConfigureDescriptor(void);
void btLmpLcResetTable(void);

/* AMP controller callbacks (amp, not converted). */
int ampPalConvertContext(void);
int ampPalParseContext(void);

extern struct OsaBufferPool hctRxPool;	/* SQRAM pool of 0x118-byte buffers */

#define HCT_LINK_COUNT		11
#define HCT_ACL_LINKS		7

/* Packet filter state; the fields are matched against incoming HCI data. */
uint8_t hctCoexTickCount __attribute__((section(".itcm.rw")));
uint8_t hctCoexDirty __attribute__((section(".itcm.rw")));
uint8_t hctHostActive __attribute__((section(".itcm.rw")));
uint8_t hctModeCurrent __attribute__((section(".itcm.rw"))) = 2;
uint8_t hctModeRequested __attribute__((section(".itcm.rw"))) = 2;
uint8_t hctSleepRequested __attribute__((section(".itcm.rw")));
uint32_t hctDeviceContext __attribute__((section(".itcm.rw")));
void *btSdioDev __attribute__((section(".itcm.rw")));
void *btHciDev __attribute__((section(".itcm.rw")));
void *btSdioActiveHandle __attribute__((section(".itcm.rw")));
uint32_t hctChannelMask __attribute__((section(".itcm.rw"))) = 0xffffffff;
const char *hctDriverName __attribute__((section(".itcm.rw"))) = "bt_hci";
OsaBuffer *hctPending __attribute__((section(".itcm.rw")));
uint16_t hctMatchFrame __attribute__((section(".itcm.rw"))) = 2;
uint16_t hctMatchHeader __attribute__((section(".itcm.rw"))) = 0x1011;
uint8_t hctMatchEnabled __attribute__((section(".itcm.rw")));
uint8_t hctMatchLength __attribute__((section(".itcm.rw"))) = 1;
uint8_t hctMatchData[64] __attribute__((section(".itcm.rw"))) = { 0x82 };

static struct MdevDriver hctSdioDriver __attribute__((section(".itcm.bss")));

/* Pending buffers are chained through their first word. */
static OsaBuffer *hctNext(OsaBuffer *buffer)
{
	return *(OsaBuffer **)buffer;
}

/* Header of every HCI event or command that arrives as a message. */
struct HctLinkMessage {
	uint32_t id;
	uint16_t handle;
	uint16_t length;
	uint8_t *data;
	uint8_t reserved;
	uint8_t flag;
	uint16_t status;
	uint32_t extra;
};

/* 0x00020044: chooses the coexistence intervals from the state of the ACL links. */
void hctComputeCoexIntervals(void)
{
	uint32_t slow = 4000;
	uint32_t fast = 4000;
	uint32_t interval;
	uint8_t *link;
	uint8_t *info;
	unsigned int i;

	for (i = 1; i < HCT_ACL_LINKS + 1; i++) {
		link = btHccRomApplyState();
		if (link[120 * i] == 2) {
			info = btLcoRomVerifyState(i);
			if (info[4] == 1 && info[6] == 1)
				interval = link[120 * i + 3] ? 8000 : 4000;
			else
				interval = link[120 * i + 3] ? 32000 : 8000;
			if (slow <= interval)
				slow = interval;
		} else if (link[120 * i] == 1) {
			interval = 16000;
			if (slow > 16000)
				interval = slow;
			slow = interval;
		}
	}
	if (romCoexPrimaryFlag != 0) {
		slow = 32000;
		fast = 16000;
	}
	if (romTargetTable == 2) {
		interval = 16000;
		if (slow > 16000)
			interval = slow;
		slow = interval;
		fast = 16000;
	}
	cbProcFilterHeader(1, 16000);
	cbProcFilterHeader(3, fast);
	cbProcFilterHeader(2, slow);
}

/* 0x00020112: once per coexistence tick, refreshes the scheduling state. */
void hctCoexTick(void)
{
	uint32_t slow;
	uint32_t fast;

	if (hctCoexTickCount < 5 || (SOC_EVENT_STATE & 4) != 0) {
		if (hctCoexTickCount < 5)
			hctCoexTickCount++;
		if ((cbProcPolicy & 8) != 0) {
			if (rfCalibFilterEntry() == 0)
				rfCalibSetupEntry(1);
			if (rfCalibRecordEntry() == 0)
				rfCalibReleaseEntry(1);
			if (!rfCalibTrackEntry())
				rfCalibVerifyEntry(1);
			cbProcFilterHeader(4, romHctCoexMode);
			hctComputeCoexIntervals();
			slow = 4000;
			fast = 4000;
			if (cbProcSecondaryCounter > 16000) {
				fast = 16000;
				slow = 16000;
			}
			cbProcFilterHeader(7, slow);
			cbProcFilterHeader(6, fast);
			if (hctCoexDirty != 0)
				cbProcFilterHeader(0x80000000, 0);
		}
	}
}

/* 0x000201a2: lowers the coexistence deadline when the programmed one is later. */
uint32_t sysUpdateEventDeadline(void)
{
	uint32_t result = (uint32_t)cbProcPolicy << 28;
	uint32_t interval;
	uint32_t parsed;

	if ((cbProcPolicy & 8) != 0) {
		interval = 16000;
		parsed = rfCalibParseState(6);
		result = rfCalibConvertEntry();
		if (result != 0) {
			result = rfCalibUpdateTable();
			if (result >= 16000)
				result = 16000;
			interval = result;
		}
		if (parsed < interval) {
			rfCalibCheckState(interval, 0);
			hctCoexDirty = 1;
			return 1;
		}
	}
	return result;
}

/* 0x000201f6 */
uint32_t *hctEnableCoexScheduling(void)
{
	cbProcPolicy |= 8;
	return &cbProcWindow;
}

/* 0x00020294: hands every queued packet to the SDIO device. */
void hctFlushPending(void)
{
	OsaBuffer *buffer;
	OsaBuffer *next;
	uint32_t length;
	uint32_t event;

	while (hctPending != NULL) {
		buffer = hctPending;
		next = hctNext(buffer);
		if (buffer->transportTag == 2) {
			length = buffer->data[9] + 2;
			event = 2;
		} else {
			uint8_t *header = (uint8_t *)buffer + buffer->dataOffset;

			event = 5;
			length = (header[2] | (header[3] << 8)) + 4;
		}
		mdevForwardEvent(btSdioActiveHandle, buffer, length, event);
		hctPending = next;
	}
}

/* 0x000202ce: queues a packet for later delivery; events of code 5 are dropped. */
void hctQueuePending(OsaBuffer *buffer)
{
	uint8_t *data = (uint8_t *)buffer + buffer->dataOffset;
	OsaBuffer **tail;

	if (data[8] == 5) {
		osa_MsgFree((void **)&data);
		return;
	}
	if (hctPending != NULL) {
		tail = &hctPending;
		while (*tail != NULL)
			tail = (OsaBuffer **)*tail;
		*tail = buffer;
		*(OsaBuffer **)buffer = NULL;
	} else {
		hctPending = buffer;
	}
}

/* 0x00020300: delivers a packet to the host, or queues it while the host is busy. */
void hctDeliverToHost(uint8_t *payload, uint32_t event)
{
	OsaBuffer *buffer = (OsaBuffer *)(payload - sizeof(OsaBuffer));

	if (hctHostActive != 0 &&
	    (hctMatchEnabled == 0 || !btLmpLcVerifyContext(0) || event == 3)) {
		btLmpLcAcquireContext(0);
		if (hctMatchEnabled != 0 && hctPending != NULL)
			hctFlushPending();
		mdevForwardEvent(btSdioActiveHandle, buffer, payload[9] + 2, event);
	} else {
		buffer->transportTag = event;
		hctQueuePending(buffer);
	}
}

/* 0x0002035e: completes the transfer of a link and sends its packet to the host. */
void hctCompleteTransfer(uint32_t handle, uint32_t flag)
{
	uint8_t *payload;

	if (romHctWindow != 0) {
		btBdhRomProcessContext();
		return;
	}
	payload = btBdhRomResolveState(handle, flag);
	if (btSdioActiveHandle == btSdioDev)
		mdevIoctl(btSdioActiveHandle, 0x14, 0);
	hctDeliverToHost(payload, 2);
}

/* 0x00020398: receive completion from the SDIO device, by packet kind. */
int hctRxCallback(OsaBuffer *buffer)
{
	uint8_t *header = (uint8_t *)buffer;
	uint32_t *message;
	uint8_t *payload;
	unsigned int link;
	struct RomHctLinkQueue *queue;
	void (*release)(void *);

	switch (buffer->transportTag) {
	case 2:
		payload = (uint8_t *)buffer + sizeof(OsaBuffer);
		if (buffer->data[8] == 5) {
			message = osa_MsgAlloc(4);
			*message = 5001;
			osa_MsgSend((void **)&message, romQueueHct);
		}
		return osa_MsgFree((void **)&payload);
	case 5:
		release = romHctMode == 1 ? romHctAcquireSession : romHctReleaseBuffer;
		release(buffer->data + 12);
		btBdhRomScheduleState();
		return 0;
	case 8:
		buffer->dataOffset = 36;
		link = (header[36] | (header[37] << 8)) & 0xfff;
		queue = &romHctLinkQueue[link];
		if (queue->flags != 0 && queue->credits > 0)
			queue->credits--;
		/* fall through */
	default:
		return osa_BufferFree(buffer);
	}
}

/* 0x0002041e: drops the pending packets that belong to a link or an address. */
void hctDropPending(uint32_t key, const void *address)
{
	OsaBuffer *buffer = hctPending;
	OsaBuffer *previous = hctPending;
	OsaBuffer *next;
	uint8_t *data;
	uint32_t tag;
	uint32_t match;

	while (buffer != NULL) {
		tag = buffer->transportTag;
		data = (uint8_t *)buffer + buffer->dataOffset;
		if (tag == 2) {
			switch (data[8]) {
			case 3:
				match = (uint32_t)(data[11] | (data[12] << 8)) == key;
				break;
			case 27:
				match = (uint32_t)(data[10] | (data[11] << 8)) == key;
				break;
			case 18:
				match = memcmp(data + 11, address, 6) == 0;
				break;
			case 32:
				match = memcmp(data + 10, address, 6) == 0;
				break;
			default:
				match = 0;
				break;
			}
		} else {
			match = (uint32_t)(data[0] | ((data[1] << 8) & 0xfff)) == key && tag != 0;
		}
		if (!match) {
			previous = buffer;
			buffer = hctNext(buffer);
			continue;
		}
		next = hctNext(buffer);
		if (buffer == hctPending)
			hctPending = next;
		else
			*(OsaBuffer **)previous = next;
		if (tag == 2)
			osa_MsgFree((void **)&data);
		else
			hctRxCallback(buffer);
		buffer = next;
	}
}

/* Message 5100: releases the link context named by the message. */
struct HctReleaseMessage {
	uint32_t id;
	uint8_t type;
	uint8_t reserved[3];
	uint8_t first;
	uint8_t reserved2;
	uint16_t second;
};

/* 0x000204be */
void hctReleaseLinkContext(struct HctReleaseMessage *message)
{
	uint32_t first = message->first;
	uint32_t second = message->second;
	uint32_t type = message->type;

	osa_MsgFree((void **)&message);
	btLmpLcReleaseContext(type, first, second);
}

/* The 5010 message carries an HCI command for the controller core. */
struct HctCommandMessage {
	uint32_t id;
	uint16_t opcode;
	uint8_t length;
	uint8_t parameters[];
} __attribute__((packed));

/* Message 5134 body: the packet pattern to match, as the host set it. */
struct HctMatchSetup {
	uint16_t frame;
	uint16_t header;
	uint8_t enabled;
	uint8_t length;
	uint8_t data[];
};

/* 0x000204e4: ACL data from the controller, to the host or into the pending queue. */
void hctSendAclToHost(struct HctLinkMessage *message)
{
	uint32_t handleWord = message->handle;
	uint32_t length = message->length;
	OsaBuffer *buffer = (OsaBuffer *)((uint8_t *)message->data - 40);
	uint8_t *packet = (uint8_t *)message->data - 4;
	uint8_t *header;
	uint32_t patternLength;
	uint32_t handle;

	buffer->dataOffset = 36;
	buffer->data[8] = handleWord;
	buffer->data[9] = handleWord >> 8;
	buffer->data[10] = length;
	buffer->data[11] = length >> 8;
	header = message->data;
	patternLength = (uint8_t)(header[0] - 2);
	if (hctMatchEnabled == 0 || !btLmpLcVerifyContext(0) ||
	    ((hctMatchFrame == (header[2] | (header[3] << 8))) &&
	     (hctMatchHeader == (header[4] | (header[5] << 8))) &&
	     hctMatchLength == patternLength &&
	     memcmp(header + 6, hctMatchData, patternLength) == 0)) {
		btLmpLcAcquireContext(0);
		handle = handleWord & 0xfff;
		btLmpLcCheckDescriptor(handle, message->data, handleWord >> 12, 1);
		osa_MsgFree((void **)&message);
		if (romHctControl[handle] != 0) {
			btBdhRomUpdateContext(packet, (uint16_t)(length + 4));
			buffer->transportTag = 5;
			hctRxCallback(buffer);
		} else if (hctHostActive != 0) {
			if (romHctFilterAcl == NULL || romHctFilterAcl(buffer) == 0) {
				if (romHctFlowEnabled != 0) {
					--romHctChannel;
					++romHctLinkInfo[handle].aclCount;
				}
				if (hctMatchEnabled != 0)
					hctFlushPending();
				mdevForwardEvent(btSdioActiveHandle, buffer, length + 4, 5);
			}
		} else {
			buffer->transportTag = 5;
			hctQueuePending(buffer);
		}
	} else {
		osa_MsgFree((void **)&message);
		romHctReleaseBuffer(&buffer->data[12]);
	}
}

/* 0x00020648: events for the AMP controller are routed by their opcode. */
int hctRouteMessage(struct HctLinkMessage *message)
{
	if (message->handle == 0xfe80)
		return ampPalConvertContext();
	return ampPalParseContext();
}

/* 0x0002065e: posts a received HCI packet to the transport thread (message 510). */
int hctPostPacket(uint8_t *packet, uint32_t flag)
{
	struct HctLinkMessage *message;

	message = osa_MsgAlloc(sizeof(*message));
	message->id = 510;
	message->handle = packet[0] | (packet[1] << 8);
	message->length = packet[2] | (packet[3] << 8);
	message->status = 0;
	message->flag = flag;
	message->data = packet + 4;
	osa_MsgSend((void **)&message, romQueueHct);
	return 0;
}

/* 0x0002069e: mdev callback of the "bt_sdio" device; the packet kind is in the tag. */
int hctEventCallback(void *device, OsaBuffer *buffer, uintptr_t argument, unsigned int event)
{
	uint8_t *packet;
	struct HctCommandMessage *command;
	uint32_t *message;

	if (hctHostActive == 0)
		btSdioActiveHandle = buffer->context;
	packet = (uint8_t *)buffer + buffer->dataOffset;
	switch (buffer->transportTag) {
	case 3:
	case 8:
		message = osa_MsgAlloc(12);
		if (message != NULL) {
			ampPalParseContext();
			message[0] = 5097;
			*(uint16_t *)&message[1] = packet[0] | (packet[1] << 8);
			message[2] = (uint32_t)buffer;
			osa_MsgSend((void **)&message, romQueueHct);
			return 0;
		}
		break;
	case 2:
	case 10:
		ampPalParseContext();
		if (hctPostPacket(packet, 0) != 0)
			osa_BufferFree(buffer);
		return 0;
	case 17:
		ampPalParseContext();
		hctPostPacket(packet, 1);
		return 0;
	case 1:
	case 9:
		if (hctHostActive == 0)
			hctEnableHost();
		if (btSdioActiveHandle != btSdioDev)
			return 0;
		command = osa_MsgAlloc(packet[2] + 9);
		command->id = 5010;
		command->opcode = packet[0] | (packet[1] << 8);
		command->length = packet[2];
		__aeabi_memcpy(command->parameters, packet + 3, packet[2]);
		hctRouteMessage((struct HctLinkMessage *)command);
		osa_MsgSend((void **)&command, romQueueBtCore);
		break;
	}
	osa_BufferFree(buffer);
	mdevIoctl(btSdioActiveHandle, 0x14, 0);
	return 0;
}

/* 0x00020782: an event from the controller core, copied into a fresh buffer for the host. */
void hctReceiveEvent(struct HctLinkMessage *message)
{
	OsaBuffer *buffer;
	uint8_t *header;
	struct RomHctLinkQueue *queue;
	uint8_t *entry;
	uint8_t *source;
	uint32_t size;
	uint32_t length;
	uint32_t handle;
	uint8_t *destination;
	uint8_t *rest;

	buffer = osa_BufferAlloc(&hctRxPool, 0x118, 0);
	if (buffer == NULL)
		return;
	handle = message->handle & 0xfff;
	btLmpLcAcquireContext(0);
	buffer->transportTag = 3;
	buffer->dataOffset = 36;
	header = (uint8_t *)buffer + buffer->dataOffset;
	header[0] = message->handle;
	header[1] = message->handle >> 8;
	header[2] = message->length;
	queue = &romHctLinkQueue[handle];
	queue->credits++;
	entry = btLmpLcComputeEntry(romHctHostSlot[handle][5]);
	source = entry + *(uint16_t *)(entry + 8);
	size = source - message->data + 722;
	length = message->length;
	destination = header + 3;
	rest = message->data;
	if (length > size) {
		__aeabi_memcpy(destination, message->data, size);
		rest = source;
		length = message->length - size;
		destination = header + size + 3;
	}
	__aeabi_memcpy(destination, rest, length);
	if (romHctMode == 2) {
		queue->credits--;
		hctEventCallback(NULL, buffer, message->length, 3);
		return;
	}
	if (romHctFilterEvent != NULL && romHctFilterEvent(buffer) != 0)
		return;
	mdevForwardEvent(btSdioActiveHandle, buffer, header[2] + 3, 8);
}

/* 0x00020848: command completion for a link; message 5097. */
int hctSendCommand(struct HctLinkMessage *message)
{
	unsigned int link = message->handle;
	OsaBuffer *buffer = (OsaBuffer *)message->data;
	struct RomHctLinkQueue *queue;
	uint8_t *packet;
	void *destination;

	if (link < HCT_LINK_COUNT && (queue = &romHctLinkQueue[link])->pending != 0) {
		message->id = 502;
		message->length = buffer->dataOffset;
		queue->counter++;
		if (romHctMode == 1) {
			packet = (uint8_t *)buffer + buffer->dataOffset;
			destination = romHctSourceCounter(romHctHostSlot[link][5]);
			message->length = packet[2];
			__aeabi_memcpy(destination, packet + 3, packet[2]);
			btBdhRomHandleContext(message);
			osa_BufferFree(buffer);
			mdevIoctl(btSdioActiveHandle, 0x14, 0);
		} else {
			osa_MsgSend((void **)&message, romQueueBtCore);
		}
		return 0;
	}
	osa_BufferFree(buffer);
	mdevIoctl(btSdioActiveHandle, 0x14, 0);
	return osa_MsgFree((void **)&message);
}

/* 0x000208ea: data for a link from the host (message 510 or 517 after filtering). */
void hctSendAclData(struct HctLinkMessage *message)
{
	uint32_t word = message->handle;
	uint32_t flag = message->flag;
	uint32_t handle = word & 0xfff;
	uint32_t index = handle;
	struct RomHctLinkInfo *link;

	if (handle < HCT_LINK_COUNT) {
		if (word >> 14 != 0)
			index = 0;
		link = &romHctLinkInfo[index];
		if (link->active != 0) {
			link->handle = handle;
			btLmpLcCheckDescriptor(index, message->data, message->handle >> 12, 0);
			if (flag == 0)
				link->outstanding++;
			if (romHctMode == 1)
				btBdhRomRecordState(message);
			else
				osa_MsgSend((void **)&message, romQueueBtCore);
			return;
		}
	}
	romHctAcquireSession(message->data);
	osa_MsgFree((void **)&message);
	if (flag == 0)
		hctCompleteTransfer(handle, 1);
}

/* 0x0002095e: credits returned for a link (message 504). */
int hctCreditReturned(struct HctLinkMessage *message)
{
	unsigned int link = message->handle & 0xfff;

	if (romHctLinkQueue[link].flags != 0) {
		btBdhRomNotifyState(link, romQueueBtCore);
		if (hctHostActive != 0 && romHctLinkQueue[link].credits < 3 &&
		    hctDeviceContext == 0)
			hctReceiveEvent(message);
	}
	return osa_MsgFree((void **)&message);
}

/* 0x000209a2: a transfer for a link finished (messages 511 and 512). */
int hctTransferDone(struct HctLinkMessage *message)
{
	unsigned int word;
	unsigned int link;
	struct RomHctLinkInfo *info;

	if (romHctMode == 2)
		return romHctMode;
	word = message->handle;
	link = word >> 14 != 0 ? 0 : word & 0xfff;
	info = &romHctLinkInfo[link];
	if (info->active != 0 && (uint8_t)message->length == 0) {
		info->outstanding--;
		if (romHctQueueMode != 1 || message->id != 512)
			hctCompleteTransfer(message->handle & 0xfff, 1);
	}
	return osa_MsgFree((void **)&message);
}

/* 0x000209fa: data for a link that is passed on or kept back (message 517). */
void hctHandleLinkData(struct HctLinkMessage *message)
{
	if (romHctLinkInfo[message->handle & 0xfff].flags != 0) {
		if (romHctMode == 2) {
			btBdhRomFilterState();
		} else if (romHctFlowEnabled != 0 && romHctChannel == 0) {
			romHctPendingCommand[romHctDeferredCount++] = message;
		} else {
			hctSendAclToHost(message);
		}
	} else {
		void (*release)(void *) =
			romHctMode == 1 ? romHctAcquireSession : romHctReleaseBuffer;

		release(message->data);
		osa_MsgFree((void **)&message);
		btBdhRomScheduleState();
	}
}

/* 0x00020aae: releases the buffer of a command and tells the controller (message 501). */
int hctCommandDone(struct HctLinkMessage *message)
{
	OsaBuffer *buffer = (OsaBuffer *)message->data;

	if (buffer != NULL) {
		osa_BufferFree(buffer);
		mdevIoctl(btSdioActiveHandle, 0x14, 0);
	}
	return btBdhRomTrackState(message);
}

/* 0x00020ad0 */
int hctInitControllerDescriptor(void)
{
	romHctHeaderSize = 120;
	romHctDescriptor = 6;
	return btBdhRomApplyState(6, &romHctDescriptor);
}

/* 0x00020ae4: asks the transport thread to switch the controller on or off. */
int hctSetEnabled(int enable)
{
	uint32_t *message;

	btLmpLcResetTable();
	message = osa_MsgAlloc(8);
	if (message == NULL)
		return 0;
	message[0] = 5123;
	*(uint8_t *)&message[1] = enable != 0;
	return osa_MsgSend((void **)&message, romQueueHct);
}

/* 0x00020b14: tells the controller core about the new mode. */
int hctSetCoreMode(int mode)
{
	uint32_t *message;

	message = osa_MsgAlloc(8);
	message[0] = 3278;
	*(uint8_t *)&message[1] = mode != 0;
	hctModeCurrent = mode;
	return osa_MsgSend((void **)&message, romQueueBtCore);
}

/* 0x00020b40: moves the controller to the requested mode (sleep or normal). */
int hctApplyMode(int resume)
{
	if (hctModeCurrent != hctModeRequested) {
		if (hctModeRequested == 0)
			return hctSetCoreMode(hctModeRequested);
		if (hctModeRequested != 1)
			return hctModeRequested;
		if (resume != 0)
			return hctSetCoreMode(hctModeRequested);
		if (hctSleepRequested == 0) {
			hctSleepRequested = 1;
			btLscRomApplyState(0x1000, 0x1404, 0, 0);
			return 0;
		}
	}
	return hctModeRequested;
}

/* 0x00020b7a: opens "bt_sdio" once and installs the packet callbacks. */
void hctOpenSdio(void)
{
	if (btSdioActiveHandle != NULL)
		return;
	btSdioDev = mdevOpenDevice("bt_sdio", 0);
	mdevIoctl(btSdioDev, 0x1a, (uintptr_t)btHciDev);
	mdevIoctl(btSdioDev, 0x20, (uintptr_t)hctEventCallback);
	mdevIoctl(btSdioDev, 4, (uintptr_t)hctRxCallback);
	btSdioActiveHandle = btSdioDev;
	romHctRxHook = hctRxCallback;
}

/*
 * 0x0002020c: sends the controller core an AFH channel map without the
 * Bluetooth channels that each WLAN channel in the mask would overlap.
 */
int hctSendAfhMap(void)
{
	uint32_t mask = hctChannelMask;
	uint32_t *message;
	uint8_t *map;
	int channel = 0;
	int start;
	int end;
	int bit;

	if (mask == 0xffffffff)
		return 0;
	message = osa_MsgAlloc(16);
	if (message == NULL)
		return 0;
	message[0] = 5121;
	map = (uint8_t *)&message[1];
	__aeabi_memset4(map, 10, 0xff);
	while (mask != 0) {
		if ((mask & 1) != 0 && (unsigned int)(channel - 1) <= 13) {
			start = 5 * channel - 8;
			if (start < 0)
				start = 0;
			end = start + 26;
			if (end > 79) {
				end = 79;
				if (channel != 14)
					start = 53;
			}
			for (bit = start; (unsigned int)end > (unsigned int)bit; bit++)
				map[bit >> 3] &= ~(1 << (bit & 7));
		}
		mask >>= 1;
		channel++;
	}
	return osa_MsgSend((void **)&message, romQueueBtCore);
}

/* 0x0002028c */
int hctEnableHost(void)
{
	hctHostActive = 1;
	return hctSendAfhMap();
}

/* 0x00020dfa: nonzero when the SDIO device accepts a new command. */
int hctSdioIdle(void)
{
	return mdevIoctl(btSdioActiveHandle, 8, 0) == 0;
}

/* 0x00020e14: a link of the controller is up. */
int hctLinkActive(unsigned int handle)
{
	if (handle < HCT_LINK_COUNT)
		return romHctLinkInfo[handle].active != 0;
	return 0;
}

/* 0x00020e2c: registers the "bt_hci" driver with the mdev framework. */
int hctRegisterDriver(void)
{
	hctSdioDriver.name = hctDriverName;
	hctSdioDriver.callbacks[0] = NULL;
	hctSdioDriver.callbacks[1] = NULL;
	hctSdioDriver.callbacks[2] = NULL;
	hctSdioDriver.callbacks[3] = NULL;
	hctSdioDriver.callbacks[4] = hctEventCallback;
	hctSdioDriver.callbacks[5] = NULL;
	return mdevRegisterDriver(&hctSdioDriver, NULL);
}

extern void *ampSdioDev;

/* 0x00020e4c: whether the device for a transport (0 Bluetooth, 1 AMP) is usable. */
int hctTransportReady(int transport)
{
	if (transport == 0) {
		if (btSdioActiveHandle != NULL && hctHostActive != 0 &&
		    btSdioActiveHandle == btSdioDev)
			return 1;
	} else if (transport == 1 && ampSdioDev != NULL) {
		return 1;
	}
	return 0;
}

/* Message identifiers handled by the transport thread. */
enum {
	HCT_MSG_COMMAND_DONE = 501,
	HCT_MSG_CREDIT_RETURNED = 504,
	HCT_MSG_SEND_ACL = 510,
	HCT_MSG_TRANSFER_DONE = 511,
	HCT_MSG_TRANSFER_DONE_QUEUED = 512,
	HCT_MSG_LINK_DATA = 517,
	HCT_MSG_CORE_FORMAT = 520,
	HCT_MSG_COEX_START = 5001,
	HCT_MSG_DELIVER_EVENT = 5020,
	HCT_MSG_AFH_RESET = 5021,
	HCT_MSG_LINK_CLEAR = 5022,
	HCT_MSG_LINK_SETUP = 5023,
	HCT_MSG_LINK_CONVERT = 5024,
	HCT_MSG_FLOW_ENABLE = 5027,
	HCT_MSG_CHANNEL_TIMER = 5028,
	HCT_MSG_CORE_MANAGE = 5029,
	HCT_MSG_CORE_INSPECT = 5030,
	HCT_MSG_QUERY_MODE = 5031,
	HCT_MSG_CORE_SELECT = 5032,
	HCT_MSG_CORE_ADJUST = 5033,
	HCT_MSG_CORE_ACQUIRE = 5080,
	HCT_MSG_COMMAND_COMPLETE = 5097,
	HCT_MSG_FRAME_FLAG = 5098,
	HCT_MSG_DELIVER_TO_HOST = 5099,
	HCT_MSG_RELEASE_CONTEXT = 5100,
	HCT_MSG_CONFIGURE_DESCRIPTOR = 5107,
	HCT_MSG_CHANNEL_MASK = 5120,
	HCT_MSG_MODE_REQUEST = 5123,
	HCT_MSG_MODE_RESUME = 5124,
	HCT_MSG_MATCH_SETUP = 5134,
	HCT_MSG_DROP_PENDING = 5135,
	HCT_MSG_QUERY_REPLY = 5055,
};

/* Message 5028: the timer and the channel budget for the controller. */
struct HctChannelTimerMessage {
	uint32_t id;
	uint16_t timer;
	uint16_t reserved;
	uint16_t channel;
};

/* 0x00020bb6: the transport thread; opens the devices and serves its message queue. */
void hctTask(void)
{
	void *message;
	uint32_t *reply;

	btHciDev = mdevOpenDevice("bt_hci", 0);
	hctOpenSdio();
	btBdhRomSyncState(romHctTimer, 4);
	hctInitControllerDescriptor();
	hctSetEnabled(0);
	for (;;) {
		struct HctLinkMessage *link;
		uint32_t id;
		int keep = 0;

		message = mdevReceiveMessage();
		link = message;
		id = link->id;
		switch (id) {
		case HCT_MSG_CORE_MANAGE:
			btBdhRomManageState();
			break;
		case HCT_MSG_COEX_START:
			if (romHctFlowEnabled != 0 && romHctParam != 0)
				btLscRomNotifyState(1000, 5080, 0);
			break;
		case HCT_MSG_DELIVER_EVENT:
			hctDeliverToHost(message, 2);
			keep = 1;
			break;
		case HCT_MSG_AFH_RESET:
			btBdhRomVerifyState();
			btLscRomResolveState(5124, 0);
			hctSendAfhMap();
			break;
		case HCT_MSG_LINK_CLEAR:
			btBdhRomClearState(link->handle, link->length & 0xff);
			break;
		case HCT_MSG_LINK_SETUP:
			btBdhRomSetupState(link->handle, link->length & 0xff);
			break;
		case HCT_MSG_LINK_CONVERT:
			btBdhRomConvertState(link->handle);
			break;
		case HCT_MSG_FLOW_ENABLE:
			romHctFlowEnabled = link->handle & 0xff;
			break;
		case HCT_MSG_CHANNEL_TIMER: {
			struct HctChannelTimerMessage *timer = message;

			romHctChannel = timer->channel;
			romHctTimer = timer->timer;
			btBdhRomSyncState(timer->timer, 4);
			break;
		}
		case HCT_MSG_TRANSFER_DONE:
		case HCT_MSG_TRANSFER_DONE_QUEUED:
			hctTransferDone(link);
			keep = 1;
			break;
		case HCT_MSG_COMMAND_DONE:
			hctCommandDone(link);
			keep = 1;
			break;
		case HCT_MSG_CREDIT_RETURNED:
			hctCreditReturned(link);
			keep = 1;
			break;
		case HCT_MSG_SEND_ACL:
			hctSendAclData(link);
			keep = 1;
			break;
		case HCT_MSG_LINK_DATA:
			hctHandleLinkData(link);
			keep = 1;
			break;
		case HCT_MSG_CORE_FORMAT:
			btBdhRomFormatState();
			keep = 1;
			break;
		case HCT_MSG_CORE_ADJUST:
			btBdhRomAdjustState();
			break;
		case HCT_MSG_CORE_INSPECT:
			btBdhRomInspectState(link->handle & 0xff);
			break;
		case HCT_MSG_QUERY_MODE:
			reply = osa_MsgAlloc(8);
			reply[0] = HCT_MSG_QUERY_REPLY;
			*(uint8_t *)&reply[1] = romHctMode;
			osa_MsgSend((void **)&reply, romQueueBtCore);
			break;
		case HCT_MSG_CORE_SELECT:
			btBdhRomSelectState();
			break;
		case HCT_MSG_CORE_ACQUIRE:
			btBdhRomAcquireState();
			break;
		case HCT_MSG_COMMAND_COMPLETE:
			hctSendCommand(link);
			keep = 1;
			break;
		case HCT_MSG_FRAME_FLAG:
			romHctFrameFlag = link->handle & 0xff;
			break;
		case HCT_MSG_DELIVER_TO_HOST:
			hctDeliverToHost(message, 3);
			keep = 1;
			break;
		case HCT_MSG_RELEASE_CONTEXT:
			hctReleaseLinkContext(message);
			keep = 1;
			break;
		case HCT_MSG_CONFIGURE_DESCRIPTOR:
			btLmpLcConfigureDescriptor();
			osa_MsgFree(&message);
			break;
		case HCT_MSG_CHANNEL_MASK: {
			uint32_t mask = *(uint32_t *)((uint8_t *)message + 4);

			if (mask != hctChannelMask) {
				hctChannelMask = mask;
				if (hctHostActive != 0)
					hctSendAfhMap();
			}
			break;
		}
		case HCT_MSG_MODE_REQUEST:
			hctModeRequested = link->handle & 0xff;
			hctApplyMode(0);
			break;
		case HCT_MSG_MODE_RESUME:
			hctSleepRequested = 0;
			hctApplyMode(1);
			break;
		case HCT_MSG_MATCH_SETUP: {
			uint8_t copy[24];
			struct HctMatchSetup *match = (struct HctMatchSetup *)copy;

			__aeabi_memcpy4(copy, (uint8_t *)message + 4, 24);
			hctMatchEnabled = match->enabled;
			if (match->length != 0) {
				hctMatchFrame = match->frame;
				hctMatchHeader = match->header;
				hctMatchLength = match->length;
				__aeabi_memcpy(hctMatchData, match->data, match->length);
			}
			break;
		}
		case HCT_MSG_DROP_PENDING:
			hctDropPending(link->handle, (uint8_t *)message + 6);
			break;
		default:
			break;
		}
		if (!keep && message != NULL)
			osa_MsgFree(&message);
	}
}
