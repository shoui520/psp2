/*
 * USS-1001's (BOARD_USS1001) block store (app/bstore.c): three regions of 0x3f0 bytes in the
 * code flash, each in a block of its own behind a checked header, which the host opens, reads,
 * writes and commits through the configuration requests.
 */
#ifndef ERNIE_BSTORE_H
#define ERNIE_BSTORE_H

#define BSTORE_REGIONS		3
#define BSTORE_DATA_SIZE	0x3f0
#define BSTORE_CHUNK		0x40		/* the most a read or write moves */

/* a region's block: the header, then the data */
struct BstoreBlock {
	unsigned char	magic[4];		/* "BSto"; "Bsid" in the old layout */
	unsigned int	marker;			/* 0xa5a5 */
	unsigned int	size;			/* BSTORE_DATA_SIZE */
	unsigned int	region;
	unsigned int	reserved;
	unsigned long	checksum;		/* the complement of the data's sum */
	unsigned char	data[BSTORE_DATA_SIZE];
};

/* a region's state (bstoreRegions) */
#define BSTORE_REGION_INVALID	1
#define BSTORE_REGION_VALID	2

/* the open region's state (bstoreOpenState) */
#define BSTORE_CLOSED		0
#define BSTORE_OPEN		1
#define BSTORE_COMMITTING	2

/* bstoreOpen's modes: what the open copy starts as */
#define BSTORE_OPEN_ZERO	0
#define BSTORE_OPEN_ERASED	1		/* 0xff, as erased flash */
#define BSTORE_OPEN_READ	2		/* the region's data */

extern unsigned char bstoreInit(void);
extern unsigned char bstoreRegionCount(void);
extern unsigned char bstoreRead(unsigned char region, unsigned int offset, unsigned char size,
				void *data);
extern unsigned char bstoreWrite(unsigned char region, unsigned int offset, unsigned char size,
				 void *data);
extern unsigned char bstoreOpen(unsigned char region, unsigned char mode);
extern unsigned char bstoreCommit(unsigned char region);
extern unsigned char bstoreClose(unsigned char region);
extern unsigned char bstoreMigrate(void);

#endif
