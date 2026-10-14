/*
 * DMA descriptor rings of the MAC: software state per ring and the walks
 * that re-arm or reclaim descriptors. The filename is inferred.
 */
#include "robin/dmaring.h"
#include "robin/rom.h"
#include "robin/soc.h"
#include "robin/sys.h"

/* Hardware pointer of a ring, read through the ROM. */
uint32_t romDmaRingHardwarePointer(unsigned int ring, unsigned int offset,
				   struct WlanDmaRing *rings);	/* 0x03f3abd1 */

struct WlanDmaRing wlanDmaRings[WLAN_DMA_RING_COUNT] __attribute__((section(".sqram1.bss")));
uint8_t wlanDmaRingPending[128] __attribute__((section(".sqram1.bss")));

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
	return (wlanDmaRings[ring].pointer & 0x1fff) !=
	       (romDmaRingHardwarePointer(ring, ring * sizeof(struct WlanDmaRing), wlanDmaRings) &
		0x1fff);
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
