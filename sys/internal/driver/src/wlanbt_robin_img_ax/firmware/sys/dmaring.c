/*
 * DMA descriptor rings of the MAC: software state per ring and the walks
 * that re-arm or reclaim descriptors. The filename is inferred.
 */
#include "robin/dmaring.h"
#include "robin/hostcmd.h"
#include "robin/rom.h"
#include "robin/runtime.h"
#include "robin/soc.h"
#include "robin/sys.h"
#include "robin/task.h"
#include "robin/wlan.h"
#include "robin/wlantask.h"

/* Hardware pointer of a ring, read through the ROM. */
uint32_t romDmaRingHardwarePointer(unsigned int ring);		/* 0x03f3abd1 */
/* Hands the ring's new software pointer to the MAC, through the ROM. */
void romDmaRingSetPointer(uint16_t pointer, unsigned int ring);	/* 0x03f3abf7 */
/* Starts the receive data ring of the given size, through the ROM. */
void romDmaRxDataRingStart(unsigned int size);			/* 0x03f3a359 */

/* Not converted yet; provisional name. */

/* The MAC's read pointer of the receive data ring (bits 3-12, as WlanDmaRing.pointer). */
#define MAC_390_RX_DATA_POINTER	MAC_REG(0x390)
/* Software pointers of receive rings A, B and C, as MAC_390_RX_DATA_POINTER. */
#define MAC_3A0_RX_A_POINTER	MAC_REG(0x3a0)
#define MAC_3A4_RX_B_POINTER	MAC_REG(0x3a4)
#define MAC_3A8_RX_C_POINTER	MAC_REG(0x3a8)
#define MAC_3C0_RX_CONTROL	MAC_REG(0x3c0)
/*
 * Per receive ring: the ring size (low half; the hardware pointer is read
 * back in the high half) and the address of its descriptors.
 */
#define MAC_3F0_RX_DATA_RING	MAC_REG(0x3f0)
#define MAC_3F4_RX_DATA_DESCRIPTORS MAC_REG(0x3f4)
#define MAC_410_RX_A_RING	MAC_REG(0x410)
#define MAC_414_RX_A_DESCRIPTORS MAC_REG(0x414)
#define MAC_418_RX_B_RING	MAC_REG(0x418)
#define MAC_41C_RX_B_DESCRIPTORS MAC_REG(0x41c)
#define MAC_420_RX_C_RING	MAC_REG(0x420)
#define MAC_424_RX_C_DESCRIPTORS MAC_REG(0x424)
#define MAC_4C8_RX_CONTROL2	MAC_REG(0x4c8)

/* mdevIoctl() of the SDIO device: kicks the host to take the frames still queued. */
#define SDIO_IOCTL_KICK_HOST	0x62

struct WlanDmaRing wlanDmaRings[WLAN_DMA_RING_COUNT] __attribute__((section(".sqram1.bss")));
uint8_t wlanDmaRingPending[128] __attribute__((section(".sqram1.bss")));

/* Times the receive data ring held a descriptor not yet written. */
uint32_t wlanDmaRxDataStalls __attribute__((section(".sqram1.rw"))) = 0;

/* Descriptors and buffers of the receive rings. */
#define WLAN_DMA_RX_C_BUFFER		0x100

static struct WlanDmaDescriptor wlanDmaRxDataDescriptors[WLAN_DMA_RX_DATA_SIZE]
	__attribute__((section(".sqram1.bss")));			/* 0xc0014130 */
static struct WlanDmaDescriptor wlanDmaRxADescriptors[WLAN_DMA_RX_SMALL_SIZE]
	__attribute__((section(".sqram1.bss")));			/* 0xc00143c8 */
static struct WlanDmaDescriptor wlanDmaRxBDescriptors[WLAN_DMA_RX_SMALL_SIZE]
	__attribute__((section(".sqram1.bss")));			/* 0xc00143d8 */
static struct WlanDmaDescriptor wlanDmaRxCDescriptors[WLAN_DMA_RX_SMALL_SIZE]
	__attribute__((section(".sqram1.bss")));			/* 0xc00143e8 */
uint8_t wlanDmaRxBBuffers[WLAN_DMA_RX_SMALL_SIZE][WLAN_DMA_RX_B_BUFFER]
	__attribute__((section(".sqram1.bss"), aligned(8)));		/* 0xc0014400 */
uint8_t wlanDmaRxABuffers[WLAN_DMA_RX_SMALL_SIZE][WLAN_DMA_RX_A_BUFFER]
	__attribute__((section(".sqram1.bss"), aligned(8)));		/* 0xc0014c00 */
static uint8_t wlanDmaRxCBuffers[WLAN_DMA_RX_SMALL_SIZE][WLAN_DMA_RX_C_BUFFER]
	__attribute__((section(".sqram1.bss"), aligned(8)));		/* 0xc0015200 */
uint8_t wlanDmaRxDataBuffers[WLAN_DMA_RX_DATA_SIZE][WLAN_DMA_RX_DATA_BUFFER]
	__attribute__((section(".sqram1.bss"), aligned(8)));		/* 0xc0015400 */

#define RING_INDEX(pointer)	(((pointer) << 20) >> 23)

/* Takes the index from the hardware pointer and returns the buffer of that descriptor. */
uint32_t wlanDmaRingReadBuffer(unsigned int ring)
{
	struct WlanDmaRing *r = &wlanDmaRings[ring];
	uint32_t index = RING_INDEX(r->pointer);

	r->index = index;
	return r->descriptors[index].buffer;
}

/*
 * Arms descriptors from the current one onwards until one that the hardware
 * had completed, advancing the pointer past each. Returns the new pointer.
 */
uint32_t wlanDmaRingRearm(unsigned int ring)
{
	struct WlanDmaRing *r = &wlanDmaRings[ring];
	uint32_t index = RING_INDEX(r->pointer);
	struct WlanDmaDescriptor *descriptor = &r->descriptors[index];
	uint32_t count = 0;
	uint32_t done;

	do {
		done = descriptor->control & WLAN_DMA_DESCRIPTOR_DONE;
		if (r->size - 1 == index) {
			descriptor->control = (r->controlLast << 4) | WLAN_DMA_DESCRIPTOR_ARMED;
			r->pointer = ~r->pointer & 0x1000;
			index = 0;
		} else {
			r->pointer = (r->pointer + 8) & 0x1fff;
			++index;
			descriptor->control = (r->controlNext << 4) | WLAN_DMA_DESCRIPTOR_ARMED;
		}
		if (done != 0)
			break;
		descriptor = &r->descriptors[index];
		r->index = index;
		++count;
	} while (r->size >= count);
	return r->pointer;
}

/* True when the pointer kept in software differs from the hardware's. */
int wlanDmaRingHasPending(unsigned int ring)
{
	uint32_t hardware = romDmaRingHardwarePointer(ring);

	return (wlanDmaRings[ring].pointer & 0x1fff) != (hardware & 0x1fff);
}

/* Waits until the receive rings are empty, nudging the host while data frames wait. */
void wlanDmaRingsDrain(void)
{
	while (wlanDmaRingHasPending(WLAN_DMA_RING_RX_A))
		osaTaskSleep(1);
	while (wlanDmaRingHasPending(WLAN_DMA_RING_RX_B))
		osaTaskSleep(1);
	while (wlanDmaRingHasPending(WLAN_DMA_RING_RX_C))
		osaTaskSleep(1);
	while (wlanDmaRingHasPending(WLAN_DMA_RING_RX_DATA)) {
		if (hostCmdNodeGet())
			mdevIoctl(wlanSdioDev, SDIO_IOCTL_KICK_HOST, 0);
		osaTaskSleep(1);
	}
}

void wlanDmaRingClearPending(void)
{
	__aeabi_memclr4(wlanDmaRingPending, sizeof(wlanDmaRingPending));
}

/* Flags the descriptors from one pointer up to another as pending. */
void wlanDmaRingMarkPending(uint32_t from, uint32_t to, unsigned int ring)
{
	struct WlanDmaRing *r = &wlanDmaRings[ring];
	uint32_t index = RING_INDEX(from);
	uint32_t end = RING_INDEX(to);

	while (index != end) {
		wlanDmaRingPending[index] = 1;
		if (r->size - 1 == index)
			index = 0;
		else
			index = (uint16_t)(index + 1);
	}
}

/*
 * Marks the descriptors from one pointer up to another as released, then
 * re-arms every released descriptor from the MAC's receive pointer on and
 * hands the new pointer to the MAC. Returns sysIrqRestore()'s result.
 */
int wlanReleaseDmaSpan(uint32_t from, uint32_t to, unsigned int ring)
{
	struct WlanDmaRing *r = &wlanDmaRings[ring];
	UINT posture = sysIrqDisable();
	uint32_t pointer = MAC_390_RX_DATA_POINTER;
	uint32_t index = RING_INDEX(pointer);

	wlanDmaRingMarkPending(from, to, ring);
	while (wlanDmaRingPending[index]) {
		struct WlanDmaDescriptor *descriptor = &r->descriptors[index];

		*(uint32_t *)descriptor->buffer = 0;
		wlanDmaRingPending[index] = 0;
		if (r->size - 1 == index) {
			descriptor->control = (r->controlLast << 4) | WLAN_DMA_DESCRIPTOR_ARMED;
			pointer = ~pointer & 0x1000;
			index = 0;
		} else {
			pointer = (pointer + 8) & 0x1fff;
			index = (uint16_t)(index + 1);
			descriptor->control = (r->controlNext << 4) | WLAN_DMA_DESCRIPTOR_ARMED;
		}
	}
	romDmaRingSetPointer(pointer, ring);
	return sysIrqRestore(posture);
}

/*
 * Moves the pointer on by count >> 8 descriptors and one more when the low
 * byte of count is not zero; the phase flips if it passes the end.
 */
uint32_t wlanDmaRingAdvance(unsigned int ring, uint32_t unused, uint32_t count)
{
	struct WlanDmaRing *r = &wlanDmaRings[ring];
	uint32_t pointer = r->pointer;

	if (r->size - 1 != RING_INDEX(pointer)) {
		pointer = (pointer + 8 * (count >> 8)) & 0x1fff;
		r->pointer = pointer;
		if ((count & 0xff) != 0) {
			pointer = (pointer + 8) & 0x1fff;
			r->pointer = pointer;
		}
		if ((pointer & 0xfff) <= 0x290)
			return r->pointer;
	}
	r->pointer = ~pointer & 0x1000;
	return r->pointer;
}

/* Walks the pointer on over completed descriptors without re-arming them. */
uint32_t wlanDmaRingReclaim(unsigned int ring)
{
	struct WlanDmaRing *r = &wlanDmaRings[ring];
	uint32_t index = RING_INDEX(r->pointer);
	struct WlanDmaDescriptor *descriptor = &r->descriptors[index];
	uint32_t count = 0;
	uint32_t done;

	do {
		done = descriptor->control & WLAN_DMA_DESCRIPTOR_DONE;
		if (r->size - 1 == index) {
			index = 0;
			r->pointer = ~r->pointer & 0x1000;
		} else {
			++index;
			r->pointer = (r->pointer + 8) & 0x1fff;
		}
		if (done != 0)
			break;
		descriptor = &r->descriptors[index];
		++count;
		r->index = index;
	} while (r->size >= count);
	return r->pointer;
}

/* False when ring 3 is full: same descriptor as the MAC's pointer in the opposite phase. */
int wlanDmaRing3HasRoom(void)
{
	uint32_t hardware = MAC_REG(0x3f0) >> 16;
	uint32_t pointer = wlanDmaRings[3].pointer;

	if ((pointer & 0xfff) != (hardware & 0xfff) || (pointer & 0x1000) == (hardware & 0x1000))
		return 1;
	return 0;
}

/*
 * Looks for the next descriptor the hardware completed with a frame. Every
 * other descriptor on the way is re-armed. Returns the buffer of the frame
 * and the ring pointer in *pointer, or 0 when the ring has caught up.
 */
uint32_t wlanDmaRingFindCompleted(unsigned int ring, uint16_t *pointer)
{
	struct WlanDmaRing *r = &wlanDmaRings[ring];
	uint32_t index = RING_INDEX(r->pointer);
	struct WlanDmaDescriptor *descriptor;
	uint32_t own;
	uint32_t hardware;

	r->index = index;
	descriptor = &r->descriptors[index];
	for (;;) {
		own = r->pointer;
		hardware = r->hardwarePointer;
		if ((own & 0xfff) == (hardware & 0xfff) && (own & 0x1000) != (hardware & 0x1000))
			return 0;
		if ((descriptor->control & 0x200000) != 0 && (descriptor->control & 1) != 0)
			break;
		if (r->size - 1 == index) {
			descriptor->control = (r->controlLast << 4) | WLAN_DMA_DESCRIPTOR_ARMED;
			r->pointer = ~r->pointer & 0x1000;
			index = 0;
		} else {
			descriptor->control = (r->controlNext << 4) | WLAN_DMA_DESCRIPTOR_ARMED;
			++index;
			r->pointer = (r->pointer + 8) & 0x1fff;
		}
		descriptor = &r->descriptors[index];
		r->index = index;
	}
	*pointer = r->pointer;
	return descriptor->buffer;
}

/*
 * Copies a received frame out of the DMA buffer: the 64-byte status, then
 * the 32-byte header and the body into the buffer the status points to.
 */
void wlanRxCopyFrame(const uint8_t *raw, struct WlanRxStatus *status)
{
	uint8_t *frame;

	__aeabi_memcpy(status, raw, 0x40);
	wlanRxConvertStatus(status);
	frame = (uint8_t *)__aeabi_uread4(status->frame);
	__aeabi_memcpy(frame, raw + 64, 0x20);
	__aeabi_memcpy(frame + 32, raw + 100, ((raw[65] << 8) | raw[64]) - 96);
}

/*
 * Looks at the next frame of ring B without taking it. It must carry the
 * ready mark and a plausible length; *header and *body then point into the DMA
 * buffer. Returns 1 if it did.
 */
int wlanDmaRingPeekReady(uint8_t **raw, uint8_t **header, uint8_t **body, uint16_t *bodyLength)
{
	uint8_t *buffer = (uint8_t *)wlanDmaRingReadBuffer(WLAN_DMA_RING_RX_B);
	unsigned int length;
	unsigned int payload;

	if (((buffer[3] << 8) | buffer[2]) != WLAN_DMA_FRAME_READY)
		return 0;
	*raw = buffer;
	*header = buffer + 64;
	length = (uint16_t)((buffer[65] << 8) | buffer[64]);
	payload = (uint16_t)(length - 64);
	if (length == 64 || payload > 0x3c0)
		return 0;
	*body = buffer + 100;
	*bodyLength = payload - 32;
	return 1;
}

/* Takes the next frame of ring B if it is marked ready, marking it used. */
int wlanDmaRingTakeB(struct WlanRxStatus *status)
{
	unsigned int posture = sysIrqDisable();
	uint16_t pointer;
	uint8_t *raw;
	int result = 0;

	wlanDmaRings[WLAN_DMA_RING_RX_B].hardwarePointer = MAC_REG(0x418) >> 16;
	raw = (uint8_t *)wlanDmaRingFindCompleted(WLAN_DMA_RING_RX_B, &pointer);
	if (raw != NULL && ((raw[3] << 8) | raw[2]) == WLAN_DMA_FRAME_READY) {
		raw[2] = WLAN_DMA_FRAME_USED & 0xff;
		raw[3] = WLAN_DMA_FRAME_USED >> 8;
		wlanRxCopyFrame(raw, status);
		result = 1;
	}
	sysIrqRestore(posture);
	return result;
}

int wlanDmaRingTakeA(struct WlanRxStatus *status)
{
	uint16_t pointer;
	uint32_t raw;

	wlanDmaRings[WLAN_DMA_RING_RX_A].hardwarePointer = MAC_REG(0x410) >> 16;
	raw = wlanDmaRingFindCompleted(WLAN_DMA_RING_RX_A, &pointer);
	if (raw == 0)
		return 0;
	wlanRxCopyFrame((const uint8_t *)raw, status);
	return 1;
}

int wlanDmaRingTakeC(struct WlanRxStatus *status)
{
	uint16_t pointer;
	uint32_t raw;

	wlanDmaRings[WLAN_DMA_RING_RX_C].hardwarePointer = MAC_REG(0x420) >> 16;
	raw = wlanDmaRingFindCompleted(WLAN_DMA_RING_RX_C, &pointer);
	if (raw == 0)
		return 0;
	wlanRxCopyFrame((const uint8_t *)raw, status);
	return 1;
}

/*
 * Refreshes the hardware pointers of rings 3 and 6 from the MAC and tells
 * whether either of them has room (the pointers differ, or share a phase).
 */
int wlanDmaRingsHaveRoom(void)
{
	struct WlanDmaRing *first = &wlanDmaRings[3];
	struct WlanDmaRing *second = &wlanDmaRings[6];
	uint32_t firstHardware = MAC_REG(0x3f0) >> 16;
	uint32_t secondHardware;
	uint32_t firstPointer;
	uint32_t secondPointer;

	first->hardwarePointer = firstHardware;
	secondHardware = MAC_REG(0x420) >> 16;
	second->hardwarePointer = secondHardware;
	firstPointer = first->pointer;
	secondPointer = second->pointer;
	return (firstPointer & 0x1000) == (firstHardware & 0x1000) ||
	       (firstPointer & 0xfff) != (firstHardware & 0xfff) ||
	       (secondPointer & 0x1000) == (secondHardware & 0x1000) ||
	       (secondPointer & 0xfff) != (secondHardware & 0xfff);
}

/*
 * True while the receive data ring has room: none of the next 16
 * descriptors (to the end of the ring) is the one the ROM reports, in the
 * hardware pointer's phase.
 */
int wlanDmaRingRxDataWindowOpen(void)
{
	struct WlanDmaRing *r = &wlanDmaRings[WLAN_DMA_RING_RX_DATA];
	uint32_t hardware = MAC_3F0_RX_DATA_RING >> 16;
	uint32_t index;
	uint32_t pointer;
	int step;

	r->hardwarePointer = hardware;
	index = RING_INDEX(hardware);
	pointer = romDmaRingHardwarePointer(WLAN_DMA_RING_RX_DATA);
	for (step = 0; step < 16; step++) {
		if (RING_INDEX(pointer) == index &&
		    (r->hardwarePointer & 0x1000) == (pointer & 0x1000))
			return 0;
		if (++index >= WLAN_DMA_RX_DATA_SIZE)
			break;
	}
	return 1;
}

/*
 * True when the receive data ring is empty or its next descriptor holds a
 * frame. Otherwise the hardware has not written it yet: counts the stall
 * and resumes the cbProc task.
 */
int wlanDmaRingRxDataSettled(void)
{
	struct WlanDmaRing *r = &wlanDmaRings[WLAN_DMA_RING_RX_DATA];
	struct WlanDmaDescriptor *descriptor;
	uint32_t pointer;
	uint32_t hardware;

	r->index = RING_INDEX(r->pointer);
	descriptor = &r->descriptors[r->index];
	pointer = r->pointer;
	hardware = r->hardwarePointer;
	if ((pointer & 0xfff) == (hardware & 0xfff) && (pointer & 0x1000) != (hardware & 0x1000))
		return 1;
	if (*(volatile uint32_t *)descriptor->buffer >> 16 == WLAN_DMA_FRAME_READY)
		return 1;
	wlanDmaRxDataStalls++;
	wlanMacResume(0);
	return 0;
}

/*
 * Takes the next frame of the receive data ring: its status into *status,
 * the frame itself left in place in the ring buffer, which is returned as
 * the packet buffer. *length is the frame length with its 4-byte trailer, 0
 * when there is no frame.
 */
OsaBuffer *wlanDmaRingTakeData(struct WlanRxStatus *status, uint16_t *length)
{
	uint16_t pointer;
	uint8_t *raw;

	*length = 0;
	wlanDmaRings[WLAN_DMA_RING_RX_DATA].hardwarePointer = MAC_3F0_RX_DATA_RING >> 16;
	if (!wlanDmaRingRxDataSettled())
		return NULL;
	raw = (uint8_t *)wlanDmaRingFindCompleted(WLAN_DMA_RING_RX_DATA, &pointer);
	if (raw == NULL)
		return NULL;
	__aeabi_memcpy(status, raw, 0x40);
	wlanRxConvertStatus(status);
	((OsaBuffer *)raw)->ringStart = pointer;
	((OsaBuffer *)raw)->pool = NULL;
	__aeabi_uwrite4((int)(raw + 64), status->frame);
	*length = ((raw[65] << 8) | raw[64]) + 4;
	return (OsaBuffer *)raw;
}

/*
 * Lays out the receive rings, each descriptor armed with its buffer, and
 * hands them to the MAC. The buffers of the data ring start with their
 * first word cleared, so wlanDmaRingRxDataSettled() sees no ready mark.
 */
static inline __attribute__((always_inline)) void
wlanDmaRingSetup(unsigned int number, uint8_t *buffers, uint32_t bufferSize, uint32_t lastControl,
		 uint32_t size, struct WlanDmaDescriptor *descriptors, int clear)
{
	struct WlanDmaRing *r = &wlanDmaRings[number];
	uint8_t *buffer = buffers;
	uint32_t i;

	r->number = number;
	r->buffers = buffers;
	r->controlNext = bufferSize;
	r->index = 0;
	r->controlLast = lastControl;
	r->descriptors = descriptors;
	r->size = size;
	r->hardwarePointer = 0;
	r->pointer = 0x1000;
	for (i = 0; i < size - 1; i++) {
		if (clear)
			__aeabi_memclr(buffer, 4);
		descriptors[i].buffer = (uint32_t)buffer;
		buffer += bufferSize;
		descriptors[i].control = (bufferSize << 4) | WLAN_DMA_DESCRIPTOR_ARMED;
	}
	if (clear)
		__aeabi_memclr(buffer, 4);
	descriptors[i].buffer = (uint32_t)buffer;
	descriptors[i].control = (lastControl << 4) | WLAN_DMA_DESCRIPTOR_ARMED;
}

void wlanDmaRingsInit(void)
{
	wlanDmaRingSetup(WLAN_DMA_RING_RX_A, wlanDmaRxABuffers[0], WLAN_DMA_RX_A_BUFFER,
			 WLAN_DMA_RX_A_BUFFER, WLAN_DMA_RX_SMALL_SIZE, wlanDmaRxADescriptors, 0);
	wlanDmaRingSetup(WLAN_DMA_RING_RX_C, wlanDmaRxCBuffers[0], WLAN_DMA_RX_C_BUFFER,
			 WLAN_DMA_RX_C_BUFFER, WLAN_DMA_RX_SMALL_SIZE, wlanDmaRxCDescriptors, 0);
	wlanDmaRingSetup(WLAN_DMA_RING_RX_B, wlanDmaRxBBuffers[0], WLAN_DMA_RX_B_BUFFER,
			 WLAN_DMA_RX_B_BUFFER, WLAN_DMA_RX_SMALL_SIZE, wlanDmaRxBDescriptors, 0);
	wlanDmaRingSetup(WLAN_DMA_RING_RX_DATA, wlanDmaRxDataBuffers[0], WLAN_DMA_RX_DATA_BUFFER,
			 0x1000, WLAN_DMA_RX_DATA_SIZE, wlanDmaRxDataDescriptors, 1);

	MAC_3C0_RX_CONTROL = 0;
	MAC_41C_RX_B_DESCRIPTORS = (uint32_t)wlanDmaRxBDescriptors;
	MAC_418_RX_B_RING = WLAN_DMA_RX_SMALL_SIZE;
	MAC_3A4_RX_B_POINTER = 0x1000;
	MAC_414_RX_A_DESCRIPTORS = (uint32_t)wlanDmaRxADescriptors;
	MAC_410_RX_A_RING = WLAN_DMA_RX_SMALL_SIZE;
	MAC_3A0_RX_A_POINTER = 0x1000;
	MAC_424_RX_C_DESCRIPTORS = (uint32_t)wlanDmaRxCDescriptors;
	MAC_420_RX_C_RING = WLAN_DMA_RX_SMALL_SIZE;
	MAC_3A8_RX_C_POINTER = 0x1000;
	MAC_3F4_RX_DATA_DESCRIPTORS = (uint32_t)wlanDmaRxDataDescriptors;
	MAC_3F0_RX_DATA_RING = WLAN_DMA_RX_DATA_SIZE;
	MAC_390_RX_DATA_POINTER = 0x1000;
	romDmaRxDataRingStart(WLAN_DMA_RX_DATA_SIZE);
	MAC_4C8_RX_CONTROL2 = 0;
}
