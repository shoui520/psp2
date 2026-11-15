/*
 * The table of the vendor elements of the peers' frames (0x37a34..0x37db4, with the filter of the
 * elements, 0x55184). The firmware's own placeholder names (macMgmt*Table, macMgmt*Buffer) are
 * kept; the file name and the names of the structures are inferred.
 */
#include <stddef.h>
#include "robin/buffer.h"
#include "robin/btrom.h"
#include "robin/conn.h"
#include "robin/ie.h"
#include "robin/mactx.h"
#include "robin/mef.h"
#include "robin/osa.h"
#include "robin/rom.h"
#include "robin/runtime.h"
#include "robin/scan.h"
#include "robin/sys.h"
#include "robin/txring.h"
#include "robin/vendorie.h"

struct WlanVendorIeFilter wlanVendorIeFilter __attribute__((section(".itcm.rw"))) = { .matchLength = 6 };	/* 0x55184 */
static struct OsaBufferPool wlanVendorIePools[3] __attribute__((section(".itcm.rw")));		/* 0x5518c */
static struct OsaPoolConfig wlanVendorIePoolConfigs[3] __attribute__((section(".itcm.rw"))) = {	/* 0x551b0 */
	{ 0, 300, 5 }, { 0, 300, 5 }, { 0, 300, 6 },
};
static uint32_t wlanVendorIePoolControl[16] __attribute__((section(".itcm.bss")));		/* 0x592ac */
static OsaBuffer *wlanVendorIeBlocks[3] __attribute__((section(".itcm.bss")));			/* 0x592ec */
struct WlanVendorIeTable wlanVendorIeTable __attribute__((section(".itcm.bss")));	/* 0x592f8 */

/* 0x379f4: tells the host the table is full (16 buffers in use) or has changed (32): an event with the class. */
void macMgmtTrackTable(struct WlanConnection *connection, unsigned int classes)
{
	OsaBuffer *buffer;
	WlanTxDescriptor *descriptor;
	uint8_t *data;

	if (wlanTxAllocFrame(&buffer, &descriptor, 0x64, 77) == 0)
		return;
	data = (uint8_t *)descriptor;
	__aeabi_memcpy(data + 16, &classes, 4);
	data[10] = 4;
	data[11] = 0;
	if (wlanHostEventNotify(classes, buffer) == 0)
		osa_BufferFree(buffer);
}

/* 0x37a34: puts an entry into the table. */
int macMgmtManageTable(struct WlanVendorIeEntry *entry, uint8_t *link)
{
	UINT posture = sysIrqDisable();

	mdevRomDispatchState(&wlanVendorIeTable, entry, link);
	return sysIrqRestore(posture);
}

/* 0x37a52: frees an entry; when no block is in use, the pools go. */
int macMgmtAcquireTable(OsaBuffer *buffer)
{
	unsigned int used;
	int i;

	buffer->packetClass = 1;
	osa_BufferFree(buffer);
	used = osa_BufferClone(&wlanVendorIePools[0]);
	used += osa_BufferClone(&wlanVendorIePools[1]);
	used = (uint8_t)(osa_BufferClone(&wlanVendorIePools[2]) + used);
	if (used == 0) {
		for (i = 0; i < 3; i++) {
			osaRomHandleState(&wlanVendorIePools[i], (int (*)(struct OsaPoolEntry *))osaDeletePoolEntry);
			osa_BufferFree(wlanVendorIeBlocks[i]);
			wlanVendorIeBlocks[i] = NULL;
		}
	}
	return used;
}

/* 0x37aa6: an entry gets its address and its element: 1 without an entry. */
int macMgmtReleaseTable(struct WlanVendorIeEntry *entry, const uint8_t *address, const uint8_t *element)
{
	if (entry == NULL)
		return 1;
	__aeabi_memcpy(entry->address, address, 6);
	__aeabi_memcpy(entry->element, element, sizeof(entry->element));
	return 0;
}

/* 0x37ad0: a new entry, from the first pool that has a block; the pools are made first if there are none. */
OsaBuffer *macMgmtSetupTable(void)
{
	OsaBuffer *result;
	unsigned int controlIndex = 0;
	int created = 0;
	int i;

	if (wlanVendorIeBlocks[0] == NULL) {
		while (created < 3) {
			OsaBuffer *memory = wlanTxBufferAlloc(&wlanBlockPool28, 0, 0, 3);

			if (memory != NULL)
				memory->packetClass = 16;
			wlanVendorIeBlocks[created] = memory;
			if (memory == NULL)
				break;
			if (osa_BlockPoolCreate(&wlanVendorIePools[created], &wlanVendorIePoolConfigs[created],
						&wlanVendorIePoolControl[controlIndex],
						(uint8_t *)memory + memory->dataOffset) != 0) {
				if (wlanVendorIeBlocks[created] != NULL) {
					osa_BufferFree(wlanVendorIeBlocks[created]);
					wlanVendorIeBlocks[created] = NULL;
				}
				break;
			}
			created++;
			controlIndex = (uint8_t)(controlIndex + (uint8_t)wlanVendorIePoolConfigs[created - 1].blockCount);
		}
		if (created < 3) {
			for (i = 0; i < created; i++) {
				osaRomHandleState(&wlanVendorIePools[i], (int (*)(struct OsaPoolEntry *))osaDeletePoolEntry);
				osa_BufferFree(wlanVendorIeBlocks[i]);
				wlanVendorIeBlocks[i] = NULL;
			}
			return NULL;
		}
	}
	result = osa_BufferAlloc(&wlanVendorIePools[0], 300, 0);
	if (result == NULL) {
		result = osa_BufferAlloc(&wlanVendorIePools[1], 300, 0);
		if (result == NULL) {
			result = osa_BufferAlloc(&wlanVendorIePools[2], 300, 0);
			if (result == NULL)
				return NULL;
		}
	}
	result->packetClass = 2;
	return result;
}

/* 0x37bb0: takes an entry out of the table and frees it. */
__attribute__((noinline)) int macMgmtClearTable(struct WlanVendorIeEntry *entry)
{
	UINT posture = sysIrqDisable();
	OsaBuffer *removed = mdevRomParseState(&wlanVendorIeTable, entry);
	int result = sysIrqRestore(posture);

	if (removed != NULL)
		return macMgmtAcquireTable(removed);
	return result;
}

/* 0x37bd6: the element of an entry is replaced: 1 without an entry. */
__attribute__((noinline)) int macMgmtConvertTable(struct WlanVendorIeEntry *entry, const uint8_t *element)
{
	if (entry == NULL)
		return 1;
	__aeabi_memcpy(entry->element, element, sizeof(entry->element));
	return 0;
}

/* 0x37bee: the entry of the address, or NULL. */
struct WlanVendorIeEntry *macMgmtVerifyTable(const uint8_t *address)
{
	struct WlanVendorIeEntry key;
	struct WlanVendorIeEntry *entry;
	UINT posture;

	__aeabi_memcpy(key.address, address, 6);
	posture = sysIrqDisable();
	entry = mdevRomPrepareState(&wlanVendorIeTable, &key);
	sysIrqRestore(posture);
	return entry;
}

/* 0x37c18: the element of the address is kept: 0 for a new entry, 1 for a replaced one, 2 for none. */
int macMgmtSelectTable(const uint8_t *address, const uint8_t *element)
{
	struct WlanVendorIeEntry *entry = macMgmtVerifyTable(address);
	OsaBuffer *buffer;

	if (entry != NULL) {
		if (macMgmtConvertTable(entry, element) == 1) {
			macMgmtClearTable(entry);
			return 2;
		}
		return 1;
	}
	buffer = macMgmtSetupTable();
	if (buffer == NULL)
		return 2;
	entry = (struct WlanVendorIeEntry *)buffer;
	if (macMgmtReleaseTable(entry, address, element) == 1) {
		macMgmtAcquireTable(buffer);
		return 2;
	}
	macMgmtManageTable(entry, entry->link);
	return 0;
}

/*
 * 0x37c72: a beacon, a probe response or a probe request of a peer: its vendor element of the filter
 * is kept; the host is told when the pools are full (16 blocks) or have more.
 */
void macMgmtAdjustTable(WlanWirelessFrame *frame, int frameKind, int unused1, int unused2)
{
	struct WlanConnection *connection = wlanConnectionByRole(0);
	uint8_t *element;
	unsigned int used;

	if (wlanHostSleepArmed() == 0)
		return;
	if ((__aeabi_uread4(wlanHostSleepActive->conditions) & 0x30) == 0)
		return;
	if (frameKind != 4 && (frame->body[10] & 2) == 0)
		return;
	element = wlanFindIe(frame, 221, frameKind, wlanVendorIeFilter.match, wlanVendorIeFilter.matchLength);
	if (element == NULL)
		return;
	if (macMgmtSelectTable(frame->address2, element) != 0)
		return;
	used = osa_BufferClone(&wlanVendorIePools[0]);
	used += osa_BufferClone(&wlanVendorIePools[1]);
	used = (uint8_t)(osa_BufferClone(&wlanVendorIePools[2]) + used);
	macMgmtTrackTable(connection, used == 16 ? 16 : 32);
}

/* 0x37d02: the element of the address into out: its size, 0 without one. */
int macMgmtRecordTable(const uint8_t *address, uint8_t *out)
{
	struct WlanVendorIeEntry *entry = macMgmtVerifyTable(address);

	if (entry == NULL)
		return 0;
	__aeabi_memcpy(out, entry->element, entry->element[1] + 2);
	return entry->element[1] + 2;
}

/* 0x37d26: the element of the address is appended to the frame. */
void macMgmtFilterTable(WlanWirelessFrame *frame, const uint8_t *address)
{
	struct WlanVendorIeEntry *entry = macMgmtVerifyTable(address);

	if (entry != NULL)
		wlanAppendIe(frame, 0xdd, &entry->element[2], entry->element[1]);
}

/* 0x37d46: every entry leaves the table. */
void macMgmtHandleBuffer(void)
{
	OsaBuffer *buffer;

	while ((buffer = mdevRomParseState(&wlanVendorIeTable, NULL)) != NULL)
		macMgmtAcquireTable(buffer);
}

/* 0x37d5c: the bucket of an entry: the low bits of its address's last byte. */
int macMgmtProcessBuffer(const struct WlanVendorIeEntry *entry)
{
	return entry->address[5] & 7;
}

/* 0x37d66: the entries of two addresses are compared. */
int macMgmtUpdateBuffer(const struct WlanVendorIeEntry *left, const struct WlanVendorIeEntry *right)
{
	return memcmp(left->address, right->address, 6);
}

/* 0x37d74: the table is made: 1 if the ROM did it. */
int macMgmtConfigureBuffer(void)
{
	return mdevRomBuildState(&wlanVendorIeTable, wlanVendorIeTable.buckets, 8,
				 (int (*)(void *))macMgmtProcessBuffer,
				 (int (*)(void *, void *))macMgmtUpdateBuffer) == 0;
}
