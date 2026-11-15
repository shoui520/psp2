/*
 * The MAC's Block Ack hardware: the slots of the transmit and receive
 * sessions, the table entries that name a session's peer and the rings of
 * frames a transmit slot has in flight. The filename is inferred; the
 * register and entry names are positional (robin/mac.h).
 */
#include <stddef.h>
#include <stdint.h>
#include "robin/baslot.h"
#include "robin/mac.h"
#include "robin/patch.h"
#include "robin/rom.h"
#include "robin/runtime.h"
#include "robin/socpower.h"
#include "robin/stream.h"
#include "robin/threadx.h"
#include "robin/txring.h"

/* Entries of the MAC's table that the slots use: 192 entries, 64 for each of three groups of rings. */
#define BA_ENTRY_RING_FIRST		128
#define BA_ENTRY_RING_COUNT		64
#define BA_ENTRY_RING_END		319
#define BA_ENTRY_RX_PEER		272	/* two entries for each receive slot: the peer, then the sequence */
#define BA_ENTRY_TX_PEER		280
#define BA_ENTRY_RX_SEQUENCE		256
#define BA_ENTRY_TX_SEQUENCE		281
#define BA_ENTRY_SLOT_STATE		264
#define BA_ENTRY_RX_RELEASE_FIRST	288
#define BA_ENTRY_RX_RELEASE_END		303
#define BA_ENTRY_RING_BUSY		0x10000	/* in an entry's second word: the frame is being completed */

#define MAC_E00_WINDOWS_ENABLE		MAC_E00_BA_CONTROL
#define MAC_E08_WINDOW_PARAMETER	MAC_REG(0xe08)
#define MAC_E58_WINDOW_PARAMETER	MAC_REG(0xe58)
#define MAC_8D8_BUSY			MAC_REG(0x8d8)
#define MAC_8E4_ACTIVE_NODE		MAC_REG(0x8e4)
#define MAC_644_STATE			MAC_REG(0x644)
#define MAC_AD0_EVENT			MAC_REG(0xad0)
#define MAC_AC8_STATUS_ADDRESS		MAC_REG(0xac8)
#define MAC_BC4_RX_STATE		MAC_REG(0xbc4)
#define MAC_800_RESET			MAC_REG(0x800)

/* Override slots of wlanBaHwReset(): before it, and after it. */
static ROBIN_PATCH_SLOT(wlanBaHwResetOverride);
static ROBIN_PATCH_SLOT(wlanBaHwResetDone);

/* Set by the ROM: the parameter of a rate (0x54e90). */
int (*wlanRateParameterHook)(unsigned int rate) __attribute__((section(".itcm.rw")));

struct WlanTxRingOps wlanTxRingOps __attribute__((section(".itcm.rw")));

/* The transmit rings' parameters and callbacks (init table entry of the transmit path). */
void wlanTxRingOpsInit(void)
{
	wlanTxRingOps.statusBuffer = (uint32_t *)0xc0031b64;
	wlanTxRingOps.slotParameter = 18;
	wlanTxRingOps.windowParameter = 32;
	wlanTxRingOps.retarget = wlanTxRingRetarget;
	wlanTxRingOps.complete = wlanTxRingComplete;
	wlanTxRingOps.bufferAddress = wlanTxRingBufferAddress;
}

uint32_t wlanTxRingBufferAddress(void)
{
	return 0x4004c828;
}

/*
 * Quiesces the Block Ack and cipher hardware around a channel change: waits
 * for its engines, lets the rings catch up and pulses the resets. Returns what
 * an override says when it handles the call.
 */
int wlanBaHwReset(void)
{
	int wait = 1000;
	uint32_t pending;
	uint32_t windows;
	unsigned int i;
	int result;

	result = wlanBaHwResetOverride();
	if (result != 0)
		return result;
	romLibResolveState();
	MAC_2D4_FLAGS |= 0x30;
	MAC_8E4_ACTIVE_NODE |= 2;
	MAC_824_CONTROL |= 4;
	MAC_644_STATE &= ~1u;
	pending = MAC_254_PENDING_REQUEST;
	MAC_254_PENDING_REQUEST |= 0x3ff;
	while ((MAC_8D8_BUSY & 0x10000) || (MAC_8D8_BUSY & 0x400) || (MAC_8D8_BUSY & 0x10)) {
		if (wait-- == 0)
			break;
		sysDelayHook(4);
	}
	for (i = 0; i < 2; i++) {
		volatile struct MacBaSlot *slot = MAC_BA_SLOT(i);

		slot->statusWrite = slot->statusRead;
		slot->ringWrite = slot->ringRead;
	}
	result = wlanTxRingsResetHook();
	if (result != 0)
		return result;
	windows = MAC_E00_WINDOWS_ENABLE;
	MAC_E00_WINDOWS_ENABLE &= ~1u;
	MAC_828_CONTROL &= ~2u;
	MAC_824_CONTROL |= 0x80;
	MAC_800_RESET = 2;
	MAC_824_CONTROL |= 0x40;
	MAC_828_CONTROL = (MAC_828_CONTROL >> 1) << 1;
	MAC_828_CONTROL |= 1;
	MAC_828_CONTROL &= ~1u;
	MAC_824_CONTROL &= ~0x40u;
	MAC_824_CONTROL |= 0x80;
	MAC_828_CONTROL &= ~2u;
	MAC_828_CONTROL |= 2;
	MAC_828_CONTROL &= ~2u;
	MAC_824_CONTROL &= ~0x80u;
	MAC_E00_WINDOWS_ENABLE = windows;
	MAC_824_CONTROL &= ~0x80u;
	MAC_254_PENDING_REQUEST = pending;
	romLibApplyState();
	MAC_824_CONTROL &= ~4u;
	MAC_644_STATE |= 1;
	MAC_2D4_FLAGS &= ~0x30u;
	MAC_8E4_ACTIVE_NODE &= ~2u;
	return wlanBaHwResetDone();
}

/* Gives the ROM the parameter of each of the 13 rates. */
void wlanRateParametersClear(void)
{
	unsigned int rate;

	for (rate = 0; rate <= 12; rate = (uint8_t)(rate + 1))
		romLibClearContext(rate, wlanRateParameterHook(rate));
}

/* Clears the 192 entries of the rings and points the hardware at the status buffer. */
void wlanBaEntriesInit(void)
{
	unsigned int entry;

	for (entry = BA_ENTRY_RING_FIRST; entry <= BA_ENTRY_RING_END; entry++)
		romMacEntryWrite(entry, 0, 0);
	MAC_AC8_STATUS_ADDRESS = (uint32_t)wlanTxRingOps.statusBuffer;
	MAC_AC0_BA_ACTIVE |= 0xb000;
}

/*
 * Writes a session's peer and TID, and its starting sequence number, to the
 * entries of a slot: mode 0 for a transmit slot, 1 for a receive slot.
 */
void wlanBaEntryRecord(unsigned int tid, const uint8_t *peer, uint16_t sequence, int mode,
		       unsigned int slot)
{
	struct {
		uint32_t low;
		uint32_t high;
	} address = { 0, 0 };
	unsigned int entry = 2 * slot;

	if (mode == 0) {
		__aeabi_memcpy(&address.low, peer, 4);
		address.high = peer[4] | (peer[5] << 8);
		address.high |= tid << 28;
		romMacEntryWrite(entry + BA_ENTRY_TX_PEER, address.low, address.high);
		address.high = 0;
		address.low = sequence;
		romMacEntryWrite(entry + BA_ENTRY_TX_SEQUENCE, address.low, 0);
	} else if (mode == 1) {
		__aeabi_memcpy(&address.low, peer, 4);
		address.high = peer[4] | (peer[5] << 8);
		address.high |= tid << 28;
		romMacEntryWrite(entry + BA_ENTRY_RX_PEER, address.low, address.high);
		address.high = 0;
		address.low = sequence;
		romMacEntryWrite(entry + BA_ENTRY_RX_PEER + 1, address.low, 0);
	}
}

/* Sets the window's starting sequence number in a transmit slot and the entry that follows it. */
void wlanBaSlotSetSequence(int slot, uint16_t sequence)
{
	volatile struct MacBaSlot *regs = MAC_BA_SLOT(slot);
	uint32_t info;

	regs->sequence = sequence;
	info = (sequence & 0xf) | (((sequence >> 4) - 1) << 4);
	info = (info & ~0xfu) | 0;
	regs->sequenceInfo = (uint16_t)info;
	romMacEntryWrite(2 * slot + BA_ENTRY_TX_SEQUENCE, sequence, 0);
}

/* Programs a transmit slot for a session (window is the hardware window it belongs to). */
int wlanBaSlotSetup(const struct WlanBaRequest *request, unsigned int window, unsigned int slot)
{
	volatile struct MacBaSlot *regs = MAC_BA_SLOT(slot);
	unsigned int i;
	uint16_t sequence;

	for (i = 0; i < BA_ENTRY_RING_COUNT; i = (uint8_t)(i + 1))
		romMacEntryWrite(BA_ENTRY_RING_FIRST + slot * 64 + i, 0, 0);
	romMacEntryWrite(BA_ENTRY_SLOT_STATE + slot, 0, 0);
	MAC_AC0_BA_ACTIVE |= MAC_AC0_SLOT_BITS << slot;
	MAC_ACC_BA_WINDOWS |= (slot | 8) << (4 * window);
	sequence = request->startSequence[0] | (request->startSequence[1] << 8);
	wlanBaEntryRecord((uint8_t)__aeabi_uread4(request->tid), request->peer, sequence, 0, slot);
	sequence = request->startSequence[0] | (request->startSequence[1] << 8);
	wlanBaSlotSetSequence(slot, sequence);
	regs->timeout = __aeabi_uread4(request->timeout) - 1;
	regs->windowSize = __aeabi_uread4(request->windowSize) - 1;
	regs->buffer = wlanTxRingOps.bufferAddress();
	MAC_AD0_EVENT = 0xffff;
	regs->parameter = wlanTxRingOps.slotParameter;
	return 1;
}

/* Hands the frames a transmit slot has in flight back to the ring they came from. */
void wlanBaSlotDetach(unsigned int ring, unsigned int slot)
{
	volatile struct MacBaSlot *regs = MAC_BA_SLOT(slot);
	uint32_t read;
	uint32_t write;
	uint32_t request;
	uint32_t control;

	if ((regs->statusRead & 0x1ff) != (regs->statusWrite & 0x1ff)) {
		regs->statusRead = 0;
		regs->statusWrite = 0;
	}
	read = regs->ringRead;
	write = regs->ringWrite;
	while (read != write) {
		romMacEntryRead(BA_ENTRY_RING_FIRST + (slot << 6) + ((uint8_t)read >> 2), &request, &control);
		if (!(control & BA_ENTRY_RING_BUSY) && request != 0)
			wlanTxRingOps.retarget((struct WlanMacTxRequest *)request, ring);
		read = (read + 4) & 0x1ff;
		regs->ringRead = read;
	}
}

/* Hands the frames from the read index to the write index of a ring to the target ring. */
void wlanTxRingRetargetAll(unsigned int ring, unsigned int target, uint32_t *readIndex,
			   volatile uint32_t *writeIndex)
{
	uint32_t read = *readIndex;
	uint32_t write = (uint16_t)*writeIndex;

	while (read != write) {
		wlanTxRingOps.retarget((struct WlanMacTxRequest *)MAC_TX_RING_ENTRY(ring, read), target);
		read = (read + 4) & 0x1ff;
	}
	*readIndex = read;
}

/*
 * Hands the frames of a ring the same way, those of the TID and address to
 * the target and the rest back to the ring.
 */
void wlanTxRingRetargetMatching(unsigned int target, unsigned int ring, unsigned int type,
				const uint8_t *address, uint32_t *readIndex,
				volatile uint32_t *writeIndex)
{
	uint32_t read = *readIndex;
	uint32_t write = *writeIndex;

	while (read != write) {
		struct WlanMacTxRequest *request = (struct WlanMacTxRequest *)MAC_TX_RING_ENTRY(ring, read);
		unsigned int to = ring;

		if ((request->type & 7) == type &&
		    memcmp((const uint8_t *)__aeabi_uread4(request->frame) + 6, address, 6) == 0)
			to = target;
		wlanTxRingOps.retarget(request, to);
		read = (read + 4) & 0x1ff;
	}
	*readIndex = read;
}

/* Completes, with the retry flag, the frames a transmit slot has in flight. */
void wlanBaSlotClear(unsigned int slot)
{
	volatile struct MacBaSlot *regs = MAC_BA_SLOT(slot);
	uint32_t read;
	uint32_t write;
	uint32_t request;
	uint32_t control;
	unsigned int entry;

	if ((regs->statusRead & 0x1ff) != (regs->statusWrite & 0x1ff)) {
		regs->statusRead = 0;
		regs->statusWrite = 0;
	}
	read = regs->ringRead;
	write = regs->ringWrite;
	while (read != write) {
		entry = BA_ENTRY_RING_FIRST + (slot << 6) + ((uint8_t)read >> 2);
		romMacEntryRead(entry, &request, &control);
		if (!(control & BA_ENTRY_RING_BUSY) && request != 0) {
			control |= BA_ENTRY_RING_BUSY;
			romMacEntryWrite(entry, request, control);
			((struct WlanMacTxRequest *)request)->reserved1[1] |= 1;
			wlanTxRingOps.complete((struct WlanMacTxRequest *)request);
		}
		read = (read + 4) & 0x1ff;
		regs->ringRead = read;
	}
}

/* Starts a receive session in a free slot (stored in *slot); 0 when there is none. */
int wlanRxBaHwSetup(const struct WlanBaRequest *request, uint8_t *slot)
{
	unsigned int found = wlanRxBaFindFreeHwSlot();
	unsigned int i;
	uint16_t sequence;

	*slot = found;
	if (found == wlanRxBaCount)
		return 0;
	MAC_AC0_BA_ACTIVE |= (16u << found) | MAC_ACD_RX_SLOT_BITS;
	sequence = request->startSequence[0] | (request->startSequence[1] << 8);
	wlanBaEntryRecord((uint8_t)__aeabi_uread4(request->tid), request->peer, sequence, 1, found);
	for (i = 0; i < 2; i = (uint8_t)(i + 1))
		romMacEntryWrite(2 * found + i + BA_ENTRY_RX_SEQUENCE, 0, 0);
	MAC_BA_RX_PARAMETER(found) = 30;
	MAC_BC4_RX_STATE &= ~1u;
	return 1;
}

/* Rewrites the entries of a receive slot for a session that is changed. */
int wlanRxBaHwUpdate(const struct WlanBaRequest *request, unsigned int slot)
{
	unsigned int i;
	uint16_t sequence = request->startSequence[0] | (request->startSequence[1] << 8);

	wlanBaEntryRecord((uint8_t)__aeabi_uread4(request->tid), request->peer, sequence, 1, slot);
	for (i = 0; i < 2; i = (uint8_t)(i + 1))
		romMacEntryWrite(2 * slot + i + BA_ENTRY_RX_SEQUENCE, 0, 0);
	return 1;
}

/* Starts the Block Ack windows: the parameter, the window size and the enable bit. */
void wlanBaWindowsStart(void)
{
	MAC_E58_WINDOW_PARAMETER = wlanTxRingOps.windowParameter;
	MAC_E08_WINDOW_PARAMETER = 16;
	MAC_E00_WINDOWS_ENABLE |= 1;
}

/* Releases a transmit slot and its window and completes, with the retry flag, the frames it has in flight. */
void wlanBaSlotFlush(int slot, int window)
{
	volatile struct MacBaSlot *regs;
	uint32_t read;
	uint32_t write;
	uint32_t request;
	uint32_t control;
	unsigned int entry;

	MAC_E00_WINDOWS_ENABLE &= ~(1u << (window + 8));
	MAC_ACC_BA_WINDOWS &= ~((slot | 8) << (4 * window));
	MAC_AC0_BA_ACTIVE &= ~(1u << slot);
	regs = MAC_BA_SLOT(slot);
	if ((regs->statusRead & 0x1ff) != (regs->statusWrite & 0x1ff))
		(void)regs->statusWrite;
	read = regs->ringRead;
	write = regs->ringWrite & 0x1ff;
	while ((read & 0x1ff) != write) {
		entry = BA_ENTRY_RING_FIRST + (slot << 6) + ((uint8_t)read >> 2);
		romMacEntryRead(entry, &request, &control);
		if (!(control & BA_ENTRY_RING_BUSY) && request != 0) {
			control |= BA_ENTRY_RING_BUSY;
			romMacEntryWrite(entry, request, control);
			((struct WlanMacTxRequest *)request)->reserved1[1] |= 1;
			wlanTxRingOps.complete((struct WlanMacTxRequest *)request);
		}
		read = (read + 4) & 0x1ff;
		regs->ringRead = read;
	}
}

/* Clears the entry of a receive session's peer and TID. */
int wlanRxBaHwRelease(const uint8_t *peer, unsigned int tid)
{
	uint32_t key[2];
	uint32_t low;
	uint32_t high;
	unsigned int posture;
	unsigned int entry;

	__aeabi_memcpy(key, peer, 6);
	key[1] |= tid << 28;
	posture = tx_interrupt_control(TX_INT_DISABLE);
	for (entry = BA_ENTRY_RX_RELEASE_FIRST; entry < BA_ENTRY_RX_RELEASE_END; entry = (uint16_t)(entry + 2)) {
		romMacEntryRead(entry, &low, &high);
		if (key[0] == low && key[1] == (high & 0xf000ffff)) {
			romMacEntryWrite(entry, 0, 0);
			break;
		}
	}
	tx_interrupt_control(posture);
	return 1;
}
