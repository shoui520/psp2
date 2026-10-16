/*
 * AMP PAL glue: the messages the PAL driver ("lamphci") sends the controller,
 * the notifications the controller sends back, the "AmphciTask" thread, and
 * the 802.11 frames of a physical link. The filename is inferred.
 */
#include <stddef.h>
#include "robin/amp.h"
#include "robin/btrom.h"
#include "robin/hct.h"
#include "robin/mdev.h"
#include "robin/task.h"
#include "robin/osa.h"
#include "robin/rom.h"
#include "robin/runtime.h"
#include "robin/sync.h"
#include "robin/sys.h"
#include "robin/wlan.h"


/* A message from the PAL driver: a 16-bit code, a 16-bit length and a body. */
struct AmpPalMessage {
	uint16_t code;
	uint16_t length;
	uint8_t body[];
};

/* Host notification codes sent with ampPalNotify(). */
enum {
	AMP_PAL_NOTIFY_DISCONNECT = 8,
	AMP_PAL_NOTIFY_STATE = 10,
	AMP_PAL_NOTIFY_CHANNEL = 11,
	AMP_PAL_NOTIFY_CHANNEL_SET = 12,
	AMP_PAL_NOTIFY_ACTIVITY = 13,
};

extern struct AmpQueue ampCommandQueue;
extern struct AmpQueue ampEventQueue;
extern struct AmpQueue ampDataQueue;
extern struct OsaEventFlags *ampEventFlags;
extern struct OsaEventFlags *ampDataEventFlags;
extern TX_THREAD *ampTaskThread;
extern uint8_t ampPalUsers;
extern int osa_MsgQueueSend(struct AmpQueue *queue, void *message);

static struct AmpPalMessage *ampPalBody(OsaBuffer *buffer)
{
	return (struct AmpPalMessage *)((uint8_t *)buffer + buffer->dataOffset);
}

/*
 * Forwards a PAL message that names a link to the link's state machine. The
 * record carries the two registers the message loop passes on.
 */
static void ampPalPostLinkEvent(OsaBuffer *buffer, uint32_t event, uint32_t argument0,
				uint32_t argument1)
{
	struct AmpPalMessage *message = ampPalBody(buffer);
	struct AmpLinkEvent notice;

	notice.opcode = argument0;
	notice.link = message->body[0];
	notice.state = ampPhysicalLinks[notice.link].state;
	notice.word1 = argument1;
	ampLinkPostEvent(notice.link, event, &notice);
}

/* 0x000221f8 */
int ampPalPostLinkEvent7(uint16_t code, OsaBuffer *buffer, uint32_t argument0, uint32_t argument1)
{
	ampPalPostLinkEvent(buffer, 7, argument0, argument1);
	return 0;
}

/* 0x0002221c */
int ampPalPostLinkEvent8(uint16_t code, OsaBuffer *buffer, uint32_t argument0, uint32_t argument1)
{
	ampPalPostLinkEvent(buffer, 8, argument0, argument1);
	return 0;
}

/* 0x00022240 */
int ampPalPostLinkEvent9(uint16_t code, OsaBuffer *buffer, uint32_t argument0, uint32_t argument1)
{
	ampPalPostLinkEvent(buffer, 9, argument0, argument1);
	return 0;
}

/* 0x00022264 */
int ampPalPostLinkEvent10(uint16_t code, OsaBuffer *buffer, uint32_t argument0, uint32_t argument1)
{
	ampPalPostLinkEvent(buffer, 10, argument0, argument1);
	return 0;
}

/* 0x00022288 */
int ampPalPostLinkEvent13(uint16_t code, OsaBuffer *buffer, uint32_t argument0, uint32_t argument1)
{
	ampPalPostLinkEvent(buffer, 13, argument0, argument1);
	return 0;
}

/* 0x000222ac */
int ampPalPostLinkEvent14(uint16_t code, OsaBuffer *buffer, uint32_t argument0, uint32_t argument1)
{
	ampPalPostLinkEvent(buffer, 14, argument0, argument1);
	return 0;
}

/* WLAN stack: wakes whoever waits for a state change (not converted). */
int wlanReleaseStateSemaphore(void);

/* 0x000222f4: Disconnect Physical Link Complete event. */
int ampHciDisconnectPhysicalLinkComplete(int status, int physicalHandle)
{
	ampHciSendEvent(0x42, 4, "bbb", status, physicalHandle, ampDisconnectReason);
	return 0;
}

/* 0x0002230e: the peer or the link went away; removes its logical links. */
int ampPalPhysicalLinkDisconnected(uint16_t code, OsaBuffer *buffer, uint32_t argument0,
				   uint32_t argument1)
{
	struct AmpPalMessage *message = ampPalBody(buffer);
	struct AmpLinkEvent notice;
	unsigned int index = message->body[0];
	unsigned int slot;

	ampDisconnectReason = 8;
	for (slot = 0; slot < AMP_LOGICAL_LINKS; slot++)
		ampLogicalLinkRemove(ampPhysicalLinks[index].logicalLinks[slot].handle, 8);
	ampHciDisconnectPhysicalLinkComplete(0, ampPhysicalLinks[index].handle);
	ampPhysicalLinkReset(index);
	notice.opcode = argument0;
	notice.link = index;
	notice.state = argument0 >> 24;
	notice.word1 = argument1;
	ampLinkPostEvent(index, 15, &notice);
	return 0;
}

/* 0x00022374: Physical Link Complete event; then releases the WLAN state semaphore. */
int ampHciPhysicalLinkComplete(int status, int physicalHandle)
{
	ampHciSendEvent(0x40, 1, "bb", status, physicalHandle);
	wlanReleaseStateSemaphore();
	return 0;
}

/* 0x0002238c: Short Range Mode Change Complete event for a link of the PAL driver. */
void ampPalShortRangeModeChanged(uint16_t code, OsaBuffer *buffer)
{
	struct AmpPalMessage *message = ampPalBody(buffer);

	ampHciAdjustCommandCount(-1);
	ampHciSendEvent(0x4c, 0x110, "bbb", 0, ampPhysicalLinks[message->body[0]].handle,
			message->body[1]);
	ampHciSendNopCommandStatus();
}

/* 0x000229ba and 0x000229d6 are in amp/hci.c. */

/* 0x00022a66: a data packet from the host for a logical link. */
int ampPalDataFromHost(OsaBuffer *buffer)
{
	uint8_t *data = (uint8_t *)buffer + buffer->dataOffset;
	uint32_t handle = ((data[1] << 8) & 0xfff) | data[0];

	if (ampLogicalLinkFind(handle) < AMP_LOGICAL_LINKS) {
		buffer->dataOffset += 4;
		buffer->ringEnd = handle;
		mdevForwardEvent(ampPalDevice, buffer, buffer->length, 4);
	} else {
		osa_BufferFree(buffer);
	}
	if (ampFlowControlMode != 0xfe) {
		if (ampFlowControlMode == 1)
			ampHciNumberOfCompletedDataBlocks(handle, 1, 1);
		else
			ampHciNumberOfCompletedPackets(handle, 1);
	}
	return 0;
}

/* 0x00022ac8: prepends the HCI data header of a received packet and gives it to the host. */
int ampPalDataToHost(OsaBuffer *buffer, unsigned int physicalIndex)
{
	uint8_t *header;
	uint32_t word;

	if (ampLogicalLinkServiceType(physicalIndex) != 0) {
		buffer->dataOffset -= 4;
		header = (uint8_t *)buffer + buffer->dataOffset;
		word = (((header[1] << 8) >> 12) << 12) | ampPhysicalLinks[physicalIndex].handle;
		header[0] = word;
		header[1] = word >> 8;
		header[1] = (header[1] | 0x30) & 0x3f;
		header[2] = buffer->length;
		header[3] = buffer->length >> 8;
		buffer->length += 4;
		buffer->pool = (struct OsaBufferPool *)(uintptr_t)buffer->ringStart;
		btLmpLcAcquireContext(1);
		mdevForwardEvent(ampSdioDev, buffer, buffer->length, 5);
	} else {
		osa_BufferFree(buffer);
		mdevIoctl(ampHciDev, 0x14, 0);
	}
	return 0;
}

/* 0x00022b4e */
int ampPalDropPacket(OsaBuffer *buffer)
{
	osa_BufferFree(buffer);
	mdevIoctl(ampSdioDev, 0x14, 0);
	return 0;
}

/* ---- Notifications to the PAL driver ("lamphci") ------------------------------- */

/* WLAN stack (not converted). */
extern struct OsaBufferPool wlanFramePool;
extern struct OsaBufferPool wlanControlFramePool;
OsaBuffer *wlanAllocateFrame(struct OsaBufferPool *pool, uint32_t size, uint32_t a, uint32_t b);
extern uint32_t ampLinkSupervisionInterval;

/* Message body: a 16-bit code, a 16-bit length, then the value bytes. */
static OsaBuffer *ampPalAllocNotice(uint16_t code, uint16_t length)
{
	OsaBuffer *buffer = ampHciAllocBuffer(&ampHciPool, 0x80, 0xffffffff);
	uint8_t *data;

	if (buffer == NULL)
		return NULL;
	data = (uint8_t *)buffer + buffer->dataOffset;
	data[0] = code;
	data[1] = code >> 8;
	data[2] = length;
	data[3] = length >> 8;
	return buffer;
}

static void ampPalSendNotice(OsaBuffer *buffer)
{
	mdevForwardEvent(ampHciDev, buffer, 0x1c, 0xb);
}

/* 0x00022b84: notice with one value byte. */
int ampPalNotify(int value, int code)
{
	OsaBuffer *buffer = ampPalAllocNotice(code, 1);

	if (buffer != NULL) {
		((uint8_t *)buffer + buffer->dataOffset)[4] = value;
		ampPalSendNotice(buffer);
	}
	return 0;
}

/* 0x00022c18: notice 13, a link's supervision timed out. */
int ampPalNotifyLinkLost(int link)
{
	OsaBuffer *buffer = ampHciAllocBuffer(&ampHciPool, 0x80, 0);
	uint8_t *data;

	if (buffer != NULL) {
		data = (uint8_t *)buffer + buffer->dataOffset;
		data[0] = AMP_PAL_NOTIFY_ACTIVITY;
		data[1] = 0;
		data[2] = 2;
		data[3] = 0;
		data[4] = link;
		data[5] = 0;
		ampPalSendNotice(buffer);
	}
	return 0;
}

/* 0x00022fbc: notice 8, a physical link is being disconnected. */
void ampPalNotifyDisconnect(unsigned int index)
{
	ampPalNotify(index, AMP_PAL_NOTIFY_DISCONNECT);
}

/* 0x00022fc6: notice 10 with two bytes. */
int ampPalNotifyCode10(int first, int second)
{
	OsaBuffer *buffer = ampPalAllocNotice(AMP_PAL_NOTIFY_STATE, 1);
	uint8_t *data;

	if (buffer != NULL) {
		data = (uint8_t *)buffer + buffer->dataOffset;
		data[4] = first;
		data[5] = second;
		ampPalSendNotice(buffer);
	}
	return 0;
}

/* 0x00023006: notice 12, a byte and a halfword. */
int ampPalNotifyCode12(int first, int second)
{
	OsaBuffer *buffer = ampPalAllocNotice(AMP_PAL_NOTIFY_CHANNEL_SET, 3);
	uint8_t *data;

	if (buffer != NULL) {
		data = (uint8_t *)buffer + buffer->dataOffset;
		data[4] = first;
		data[5] = second;
		data[6] = second >> 8;
		ampPalSendNotice(buffer);
	}
	return 0;
}

/* 0x0002304a: notice 11, a byte and a halfword. */
int ampPalNotifyCode11(int first, int second)
{
	OsaBuffer *buffer = ampPalAllocNotice(AMP_PAL_NOTIFY_CHANNEL, 3);
	uint8_t *data;

	if (buffer != NULL) {
		data = (uint8_t *)buffer + buffer->dataOffset;
		data[4] = first;
		data[5] = second;
		data[6] = second >> 8;
		ampPalSendNotice(buffer);
	}
	return 0;
}

/* ---- Frames of a physical link --------------------------------------------- */

extern int rfCalibApplyContext(uint32_t value);
int ampPalSdioDataCallback(OsaBuffer *buffer);
extern uint32_t ampActivityReports;		/* nonzero: send Activity Reports */
extern uint8_t ampLocalFrameAddress[6];		/* source address of AMP frames */

/* LLC/SNAP prefix of the Bluetooth SIG (organization 00:19:58). */
static const uint8_t ampSnapPrefix[6] = { 0xaa, 0xaa, 0x03, 0x00, 0x19, 0x58 };

/* 0x0002375a: writes the transmit descriptor and the Ethernet and SNAP headers. */
int ampPalBuildFrame(WlanTxFrameHeader *frame, const uint8_t *peer, uint32_t length,
		     uint32_t kind, int priority, struct WlanConnection *context)
{
	uint16_t total = length + 22;

	if (kind == AMP_PROTOCOL_L2CAP) {
		__aeabi_memcpy(frame, &context->txHeader, sizeof(*frame));
		frame->packetLength[0] = total;
		frame->packetLength[1] = total >> 8;
		return 0;
	}
	frame->packetType[0] = 0xe0;
	frame->packetType[1] = 0;
	frame->packetOffset[0] = 15;
	frame->packetOffset[1] = 0;
	frame->packetLength[0] = total;
	frame->packetLength[1] = total >> 8;
	__aeabi_memcpy(frame->destination, peer, 6);
	__aeabi_memcpy(frame->source, ampLocalFrameAddress, 6);
	frame->lengthBigEndian[0] = (length + 8) >> 8;
	frame->lengthBigEndian[1] = length + 8;
	__aeabi_memcpy(frame->snap, ampSnapPrefix, 6);
	frame->protocol[0] = kind >> 8;
	frame->protocol[1] = kind;
	frame->priority = priority;
	__aeabi_uwrite4(0, frame->control);
	frame->flags = 0;
	frame->delay = 0;
	frame->bssType = context->bssType;
	frame->bssNumber = context->bssNumber;
	return 0;
}

/* 0x00022bc0: sends a frame of the given PAL protocol to the peer of the link. */
int ampPalTransmit(OsaBuffer *buffer, uint32_t length, int priority, int kind)
{
	struct WlanConnection *context = buffer->context;
	uint32_t headroom;

	if (length >= 2000)
		return 1;
	headroom = (uint16_t)wlanPacketHeadroom();
	buffer->dataOffset = headroom;
	context->txHeader.priority = priority;
	if (ampPalBuildFrame((WlanTxFrameHeader *)((uint8_t *)buffer + headroom),
			     context->peerAddress, length, kind, priority, context) == 1)
		return 1;
	mdevForwardEvent(wlanDevice, buffer, 0x25, 4);
	return 0;
}

/* 0x00023844: sends a frame without payload, for example a link supervision request. */
OsaBuffer *ampPalSendEmptyFrame(int kind, struct WlanConnection *context)
{
	OsaBuffer *buffer = osa_BufferAlloc(&wlanControlFramePool, 0x12c, 0);

	if (buffer == NULL)
		return buffer;
	buffer->context = context;
	return (OsaBuffer *)(uintptr_t)ampPalTransmit(buffer, 0, 0, kind);
}

/* 0x00022c54: timer: a supervision request went unanswered. */
int ampPalLinkTimeout(uint32_t timer, struct WlanConnection *context)
{
	if (context->retryCount == 2) {
		ampPalNotifyLinkLost(context->bssNumber);
		return ampHciSendHostEvent(3, 0, "w", (uint32_t)&ampPhysicalLinks[context->bssNumber]);
	}
	context->retryCount++;
	ampPalSendEmptyFrame(AMP_PROTOCOL_LINK_SUPERVISION_REQUEST, context);
	return osaCreateTimer((OsaTimerCallback)ampPalLinkTimeout, (uint32_t)context,
			      ampLinkSupervisionInterval, &context->timerHandle, 0);
}

/* 0x00022ca6: the peer answered; restarts supervision. */
int ampPalLinkAlive(struct WlanConnection *context)
{
	context->retryCount = 0;
	osaCancelTimer(context->timerHandle);
	return osaCreateTimer((OsaTimerCallback)ampPalLinkTimeout, (uint32_t)context,
			      ampLinkSupervisionInterval, &context->timerHandle, 0);
}

/* 0x00022cd8: writes the SSID "AMP-xx-xx-xx-xx-xx-xx" of a link, returns the end. */
char *ampPalFormatSsid(char *out, const uint8_t *address)
{
	unsigned int i;
	uint32_t value;

	*out++ = 'A';
	*out++ = 'M';
	*out = 'P';
	out++;
	for (i = 0; i < 6; i++) {
		*out = '-';
		value = *address;
		out[1] = ampHexDigit(value >> 4);
		out[2] = ampHexDigit(value & 0xf);
		address++;
		out += 3;
	}
	*out = '\0';
	return out;
}

/* 0x0002374a: lowercase hexadecimal digit. */
char ampHexDigit(uint32_t nibble)
{
	if (nibble >= 10)
		return nibble + 'a' - 10;
	return nibble + '0';
}

/* 0x00023868: prepares the cached transmit header of a link. */
int ampPalPrepareTxHeader(struct WlanConnection *context)
{
	uint8_t *frame = (uint8_t *)&context->txHeader;
	struct WlanTxFrameHeader *header = &context->txHeader;

	/* The original clears 50 bytes: the header and the bytes behind it. */
	__aeabi_memclr(frame, 0x32);
	header->packetType[0] = 0xe0;
	header->packetType[1] = 0;
	header->packetOffset[0] = 15;
	header->packetOffset[1] = 0;
	header->bssType = context->bssType;
	header->bssNumber = context->bssNumber;
	__aeabi_memcpy(header->destination, context->peerAddress, 6);
	__aeabi_memcpy(header->source, context->localAddress, 6);
	__aeabi_memcpy(header->snap, ampSnapPrefix, 6);
	header->protocol[0] = 0;
	header->protocol[1] = AMP_PROTOCOL_L2CAP;
	return 1;
}

/* WLAN buffer allocator (not converted). */
extern struct OsaBufferPool wlanFramePool;

extern uint8_t ampActivityPayload[2];
extern uint8_t ampActivityPayloadLength;

/* 0x00022edc: sends an Activity Report with the payload prepared by the caller. */
int ampPalSendActivityReport(struct WlanConnection *context)
{
	OsaBuffer *buffer = wlanAllocateFrame(&wlanFramePool, 0x100, 0, 3);
	uint32_t headroom;

	if (buffer == NULL)
		return 0;
	headroom = (uint16_t)(wlanPacketHeadroom() + 37);
	buffer->dataOffset = headroom;
	buffer->context = context;
	__aeabi_memcpy((uint8_t *)buffer + headroom, ampActivityPayload, ampActivityPayloadLength);
	return ampPalTransmit(buffer, ampActivityPayloadLength, 0, AMP_PROTOCOL_ACTIVITY_REPORT);
}

/* 0x00022f1a: timer: the periodic Activity Report. */
int ampPalActivityTimer(uint32_t timer, struct WlanConnection *context)
{
	ampActivityPayload[0] = 1;
	ampActivityPayload[1] = 0;
	ampActivityPayloadLength = 2;
	return ampPalSendActivityReport(context);
}

/* 0x00022f2e: the physical link is up; starts supervision and the reports. */
int ampPalLinkEstablished(struct WlanConnection *context)
{
	rfCalibApplyContext(128000);
	context->flags |= 8;
	context->linkFlags = 7;
	context->retryCount = 0;
	osaCreateTimer((OsaTimerCallback)ampPalLinkTimeout, (uint32_t)context,
		       ampLinkSupervisionInterval, &context->timerHandle, 0);
	if (ampActivityReports != 0)
		osaCreateTimer((OsaTimerCallback)ampPalActivityTimer, (uint32_t)context, 1000000,
			       &context->secondTimerHandle, 0);
	return ampPalNotify(context->bssNumber, 6);
}

/* 0x0002311c: link supervision interval, 10 seconds in microseconds. */
int ampPalSetSupervisionInterval(void)
{
	ampLinkSupervisionInterval = 10000000;
	return 0;
}

/* ---- The "AmphciTask" thread and the lamphci driver -------------------------- */

struct OsaBufferPool ampTransmitPool __attribute__((section(".itcm.rw"))) = { { 1, 0, 0 } };
struct OsaBufferPool ampHciPool __attribute__((section(".itcm.rw"))) = { { 1, 0, 0 } };
void *ampSdioDev __attribute__((section(".itcm.rw")));
void *ampPalDevice __attribute__((section(".itcm.rw")));
void *ampHciDev __attribute__((section(".itcm.rw")));
static const char *ampHciDevName __attribute__((section(".itcm.rw"))) = "amphci";
struct OsaEventFlags *ampEventFlags __attribute__((section(".itcm.rw")));
TX_THREAD *ampTaskThread __attribute__((section(".itcm.rw")));
uint8_t ampPalUsers __attribute__((section(".itcm.bss")));

/* Pool geometry: (block count << 16) | block size. */
struct OsaPoolConfig {
	uint32_t reserved;
	uint32_t geometry;
};
static struct OsaPoolConfig ampHciPoolConfig __attribute__((section(".itcm.rw"))) = { 0, 0x60118 };
static struct OsaPoolConfig ampTransmitPoolConfig __attribute__((section(".itcm.rw"))) = { 0, 0x307e8 };
static uint32_t ampHciPoolControl[6] __attribute__((section(".itcm.bss")));
static uint32_t ampTransmitPoolControl[6] __attribute__((section(".itcm.bss")));
static uint8_t ampHciPoolMemory[6 * 0x118] __attribute__((section(".sqram1.bss")));
static uint8_t ampTransmitPoolMemory[3 * 0x7e8] __attribute__((section(".sqram1.bss")));

struct AmpQueue ampCommandQueue __attribute__((section(".itcm.bss")));
struct AmpQueue ampEventQueue __attribute__((section(".itcm.bss")));
struct AmpQueue ampDataQueue __attribute__((section(".itcm.bss")));
static struct MdevDriver ampPalDriver __attribute__((section(".itcm.bss")));
static TX_THREAD ampTaskThreadStorage __attribute__((section(".itcm.bss")));
static uint32_t ampEventFlagsStorage[8] __attribute__((section(".itcm.bss")));
static uint8_t ampTaskStack[1024] __attribute__((section(".itcm.bss")));

/* Event flags of the data path the control callback signals (not converted). */
extern struct OsaEventFlags *ampDataEventFlags;

/* 0x00002582: gives the SDIO device its next buffer. */
int ampPalAckSdio(void)
{
	return mdevIoctl(ampSdioDev, 0x14, 0);
}

/* 0x00002592: buffer release hook of the data path. */
int (*ampPalDataReleased(void))(void)
{
	return (int (*)(void))ampPalAckSdio;
}

/* 0x00002596: passes the packets queued by the host on, marking them as owned by the PAL. */
int ampPalDataLoop(void)
{
	OsaBuffer *buffer;

	while ((buffer = osa_MsgQueueRecv(&ampDataQueue)) != NULL) {
		buffer->flags |= 1;
		buffer->release = (int (*)(OsaBuffer *))ampPalDataReleased;
		ampPalDataFromHost(buffer);
	}
	return 1;
}

/* Bits of the event flags the HCI task waits for. */
enum {
	AMP_TASK_COMMANDS = 1,
	AMP_TASK_DATA = 2,
	AMP_TASK_EVENTS = 4,
	AMP_TASK_SECURITY = 8,
};

extern int ampPalDropSecurityFrame(void);	/* 0x000236c4 */
int ampPalEventLoop(void);

/* 0x000025ba: the thread. */
void ampTask(ULONG input)
{
	uint32_t actual;

	for (;;) {
		osaEventFlagsWait(ampEventFlags, 0xf, 8, &actual, 0xffffffff);
		if ((actual & AMP_TASK_COMMANDS) != 0)
			ampHciProcessCommands();
		if ((actual & AMP_TASK_DATA) != 0)
			ampPalDataLoop();
		if ((actual & AMP_TASK_EVENTS) != 0)
			ampPalEventLoop();
		if ((actual & AMP_TASK_SECURITY) != 0)
			ampPalDropSecurityFrame();
	}
}

/* 0x00002620: creates the event flags and the thread; 1 on failure. */
int ampTaskStart(void)
{
	ampEventFlags = (struct OsaEventFlags *)ampEventFlagsStorage;
	osaEventFlagsInit(&ampEventFlags);
	ampTaskThread = &ampTaskThreadStorage;
	if (osaCreateTask(&ampTaskThread, ampTaskStack, 1024, 12, "AmphciTask", ampTask, 0) != 0)
		return 1;
	ampPalUsers = 1;
	return 0;
}

/* 0x00002662: drops everything that is queued and resets the controller state. */
int ampFlushQueues(void)
{
	OsaBuffer *buffer;

	while ((buffer = osa_MsgQueueRecv(&ampCommandQueue)) != NULL)
		osa_BufferFree(buffer);
	while ((buffer = osa_MsgQueueRecv(&ampEventQueue)) != NULL)
		osa_BufferFree(buffer);
	while ((buffer = osa_MsgQueueRecv(&ampDataQueue)) != NULL)
		osa_BufferFree(buffer);
	ampHciResetState();
	return 0;
}

/* 0x000026a0: driver callback, the PAL driver opened the device. */
int ampPalOpen(void)
{
	int status = 0;

	if (ampPalUsers != 0)
		ampPalUsers++;
	else
		status = ampTaskStart();
	ampFlushQueues();
	return status;
}

/* 0x000026c4: driver callback, the PAL driver closed the device. */
int ampPalClose(void)
{
	if (ampPalUsers != 0)
		ampPalUsers--;
	return 0;
}

/* 0x000026d6: control events of the "amp_sdio" device. */
int ampPalSdioControlCallback(void *device, OsaBuffer *buffer, uint16_t length, int event)
{
	UINT posture = sysIrqDisable();
	OsaBuffer *copy = NULL;
	uint8_t *source;
	unsigned int size;
	unsigned int room;

	buffer->length = length;
	switch (event) {
	case 1:
		copy = ampHciAllocBuffer(&ampHciPool, 200, 0);
		if (copy != NULL) {
			source = (uint8_t *)buffer + buffer->dataOffset;
			room = (uint16_t)(200 - copy->dataOffset);
			size = source[2] + 3;
			if (size >= room)
				size = room;
			__aeabi_memcpy((uint8_t *)copy + copy->dataOffset, source, size);
			copy->length = size;
		}
		osa_BufferFree(buffer);
		mdevIoctl(ampSdioDev, 0x14, 0);
		if (copy != NULL) {
			osa_MsgQueueSend(&ampCommandQueue, copy);
			osaEventFlagsSet(ampEventFlags, 1, 7);
		}
		break;
	case 11:
		osa_MsgQueueSend(&ampEventQueue, buffer);
		osaEventFlagsSet(ampEventFlags, 4, 7);
		break;
	case 2:
		osa_MsgQueueSend(&ampDataQueue, buffer);
		osaEventFlagsSet(ampDataEventFlags, 1, 7);
		break;
	}
	sysIrqRestore(posture);
	return 0;
}

/* 0x00002782: creates the buffer pools and registers the driver with the mdev framework. */
int ampDriverInit(void)
{
	osa_BlockPoolCreate(&ampHciPool, &ampHciPoolConfig, ampHciPoolControl, ampHciPoolMemory);
	osa_BlockPoolCreate(&ampTransmitPool, &ampTransmitPoolConfig, ampTransmitPoolControl,
			    ampTransmitPoolMemory);
	__aeabi_memclr4(&ampCommandQueue, 20);
	__aeabi_memclr4(&ampEventQueue, 20);
	__aeabi_memclr4(&ampDataQueue, 20);
	ampPalDriver.name = ampHciDevName;
	ampPalDriver.callbacks[0] = ampPalOpen;
	ampPalDriver.callbacks[1] = ampPalClose;
	ampPalDriver.callbacks[2] = ampFlushQueues;
	ampPalDriver.callbacks[3] = NULL;
	ampPalDriver.callbacks[4] = ampPalSdioControlCallback;
	ampPalDriver.callbacks[5] = NULL;
	ampPalDriver.callbacks[6] = NULL;
	mdevRegisterDriver(&ampPalDriver, NULL);
	return 0;
}

/* 0x000027ec: init table entry; opens the devices of the AMP stack. */
int ampPalInit(void)
{
	ampSdioDev = mdevOpenDevice("amp_sdio", 0);
	if (ampSdioDev == NULL)
		return 1;
	ampHciDev = mdevOpenDevice(ampHciDevName, 0);
	if (ampHciDev == NULL)
		return 1;
	mdevIoctl(ampSdioDev, 0x1a, (uintptr_t)ampHciDev);
	mdevIoctl(ampSdioDev, 4, (uintptr_t)ampPalSdioDataCallback);
	mdevIoctl(ampSdioDev, 0x20, (uintptr_t)ampPalSdioControlCallback);
	ampPalDevice = mdevOpenDevice("btamp_pal", 0);
	if (ampPalDevice == NULL)
		return 1;
	mdevIoctl(ampPalDevice, 0x1a, (uintptr_t)ampHciDev);
	osaTaskResume(ampTaskThread);
	mdevIoctl(ampSdioDev, 0xd, 0);
	return 0;
}

/* 0x000021dc: data callback of the "amp_sdio" device. */
int ampPalSdioDataCallback(OsaBuffer *buffer)
{
	if (buffer->transportTag == 5) {
		buffer->transportTag = (uint16_t)(uintptr_t)buffer->pool;
		buffer->pool = NULL;
		wlanReleasePacket(buffer);
	} else {
		osa_BufferFree(buffer);
	}
	return mdevIoctl(ampHciDev, 0x14, 0);
}

/* The buffer the security frame check looks at (not converted writer). */
extern OsaBuffer *ampSecurityFrame;
extern uint8_t ampSupportedRates[12];

/* 0x000236c4: drops a pending frame whose frame control is an Action frame type 0x8024. */
int ampPalDropSecurityFrame(void)
{
	uint8_t *data = (uint8_t *)ampSecurityFrame + ampSecurityFrame->dataOffset;

	if ((((data[1] << 8) & 0xcfff) | data[0]) == 0x8024)
		osa_BufferFree(ampSecurityFrame);
	return 0;
}

/* 0x000236ea: offset of the frame body behind the transmit headers. */
int ampPalFrameDataOffset(void)
{
	return (uint16_t)(wlanPacketHeadroom() + 29);
}

/* 0x0002372a: copies the supported rate set of the controller into a context. */
int ampPalCopySupportedRates(struct WlanConnection *context)
{
	uint8_t *out = context->supportedRates;
	unsigned int i;

	for (i = 0; i < 12; i++)
		*out++ = ampSupportedRates[i];
	context->supportedRatesLength = 12;
	return 12;
}

void ampPalIgnoreMessage(void);

/* 0x000024d6: handles the messages the PAL driver queued for the controller. */
int ampPalEventLoop(void)
{
	OsaBuffer *buffer;
	struct AmpPalMessage *message;
	uint32_t code;

	for (;;) {
		buffer = osa_MsgQueueRecv(&ampEventQueue);
		if (buffer == NULL)
			return 1;
		message = ampPalBody(buffer);
		code = message->code;
		switch (code) {
		case 1:
		case 2:
			ampPalPostLinkEvent7(code, buffer, code & 0xff, code);
			break;
		case 3:
			ampPalPostLinkEvent8(code, buffer, code & 0xff, code);
			break;
		case 4:
			ampPalPostLinkEvent9(code, buffer, code & 0xff, code);
			break;
		case 5:
			ampPalPostLinkEvent10(code, buffer, code & 0xff, code);
			break;
		case 6:
		case 7:
			ampPalPostLinkEvent13(code, buffer, code & 0xff, code);
			break;
		case 8:
			ampPalPostLinkEvent14(code, buffer, code & 0xff, code);
			break;
		case 10:
			ampPalShortRangeModeChanged(code, buffer);
			break;
		case 11:
			ampHciFlushOccurred(code, buffer);
			break;
		case 12:
			ampHciEnhancedFlushComplete(code, buffer);
			break;
		case 13:
			ampPalPhysicalLinkDisconnected(code, buffer, code & 0xff, 0);
			break;
		case 128:
			ampPalIgnoreMessage();
			break;
		}
		osa_BufferFree(buffer);
	}
}

/* 0x000222f0: message 128 has no handler. */
void ampPalIgnoreMessage(void)
{
}
