/*
 * The table of the vendor elements (ID 221) peers' beacons and probe requests carry
 * (wlan/vendorie.c, the firmware's placeholder names "macMgmt*Table" and "macMgmt*Buffer" of
 * 0x37a34..0x37db4): one 300-byte entry per transmitter address, from three pools of five, five and
 * six blocks that exist while an entry does, kept in a hash table of eight buckets. Marvell
 * published no source for it; the names of the structures are inferred.
 */
#ifndef ROBIN_VENDORIE_H
#define ROBIN_VENDORIE_H

#include <stdint.h>
#include "robin/buffer.h"
#include "robin/conn.h"
#include "robin/wlan.h"

/* The elements the table keeps: the vendor elements that begin with these bytes. */
struct WlanVendorIeFilter {
	uint8_t matchLength;		/* 6 */
	uint8_t match[6];		/* the OUI, its type and what follows: set elsewhere */
	uint8_t reserved7;
};

_Static_assert(sizeof(struct WlanVendorIeFilter) == 8, "vendor element filter");

/* One entry of the table: the buffer of a block of the pools. */
struct WlanVendorIeEntry {
	uint8_t header[28];		/* OsaBuffer */
	uint8_t address[6];		/* 28: the transmitter's */
	uint8_t element[257];		/* 34: the vendor element, its ID, length and bytes */
	uint8_t reserved291;
	uint8_t link[8];		/* 292: what the ROM's hash table links it with */
};

_Static_assert(sizeof(struct WlanVendorIeEntry) == 300, "vendor element entry");
_Static_assert(offsetof(struct WlanVendorIeEntry, link) == 292, "vendor element link");

/* The ROM's hash table: its header and the eight buckets. */
struct WlanVendorIeTable {
	uint32_t rom[6];
	uint32_t buckets[8];
};

_Static_assert(sizeof(struct WlanVendorIeTable) == 56, "vendor element table");

extern struct WlanVendorIeFilter wlanVendorIeFilter;		/* 0x55184 */

void macMgmtTrackTable(struct WlanConnection *connection, unsigned int classes);	/* 0x379f4 */
int macMgmtManageTable(struct WlanVendorIeEntry *entry, uint8_t *link);		/* 0x37a34 */
int macMgmtAcquireTable(OsaBuffer *buffer);					/* 0x37a52 */
int macMgmtReleaseTable(struct WlanVendorIeEntry *entry, const uint8_t *address,
			const uint8_t *element);				/* 0x37aa6 */
OsaBuffer *macMgmtSetupTable(void);						/* 0x37ad0 */
int macMgmtClearTable(struct WlanVendorIeEntry *entry);				/* 0x37bb0 */
int macMgmtConvertTable(struct WlanVendorIeEntry *entry, const uint8_t *element);	/* 0x37bd6 */
struct WlanVendorIeEntry *macMgmtVerifyTable(const uint8_t *address);		/* 0x37bee */
int macMgmtSelectTable(const uint8_t *address, const uint8_t *element);		/* 0x37c18 */
void macMgmtAdjustTable(WlanWirelessFrame *frame, int frameKind, int unused1, int unused2);	/* 0x37c72 */
int macMgmtRecordTable(const uint8_t *address, uint8_t *out);			/* 0x37d02 */
void macMgmtFilterTable(WlanWirelessFrame *frame, const uint8_t *address);	/* 0x37d26 */
void macMgmtHandleBuffer(void);							/* 0x37d46 */
int macMgmtProcessBuffer(const struct WlanVendorIeEntry *entry);		/* 0x37d5c */
int macMgmtUpdateBuffer(const struct WlanVendorIeEntry *left, const struct WlanVendorIeEntry *right);	/* 0x37d66 */
int macMgmtConfigureBuffer(void);						/* 0x37d74 */

#endif /* ROBIN_VENDORIE_H */
