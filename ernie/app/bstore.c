/*
 * USS-1001's block store: three regions of 0x3f0 bytes, region r in the code flash block at
 * 0x5ec00 + (2 - r) * 0x400. A region is opened into the configuration's open copy
 * (configBuffer), changed there, and committed: the header is filled in and the block written by
 * configFlashWrite. A region is valid when its header is "BSto", 0xa5a5, the size and the
 * complement of the data's sum; that sum adds the data's first byte 0x3f0 times, the pointer never
 * moving, here as in Sony's code. bstoreMigrate moves the regions of the old layout ("Bsid"
 * blocks from 0x5e000) into the new one.
 */
#include <stddef.h>
#include <string.h>
#include "ernie.h"
#include "bstore.h"
#include "config.h"
#include "flash.h"

static unsigned char bstoreCheck(unsigned char region);
static unsigned char bstoreFindOld(unsigned char region, unsigned char **data);
static struct BstoreBlock *bstoreAddress(unsigned char region);

const unsigned char bstoreMagic[4] = { 'B', 'S', 't', 'o' };
const unsigned char bstoreOldMagic[4] = { 'B', 's', 'i', 'd' };

unsigned char bstoreRegions[BSTORE_REGIONS];	/* each region's state */
unsigned char bstoreOpenState;
unsigned char bstoreOpenRegion;

unsigned char bstoreInit(void)
{
	unsigned char i;

	for (i = 0; i < BSTORE_REGIONS; i++)
		bstoreRegions[i] = bstoreCheck(i) == 0 ? BSTORE_REGION_VALID : BSTORE_REGION_INVALID;
	bstoreOpenState = BSTORE_CLOSED;
	bstoreOpenRegion = 0;
	return 0;
}

unsigned char bstoreRegionCount(void)
{
	return BSTORE_REGIONS;
}

/* reads size bytes (BSTORE_CHUNK at most) at offset of a valid region */
unsigned char bstoreRead(unsigned char region, unsigned int offset, unsigned char size,
			 void *data)
{
	struct BstoreBlock *block;

	if (region >= BSTORE_REGIONS)
		return 8;
	if (bstoreRegions[region] != BSTORE_REGION_VALID)
		return 7;
	if (offset + size > BSTORE_DATA_SIZE)
		return 8;
	if (size > BSTORE_CHUNK)
		return 8;
	block = bstoreAddress(region);
	memcpy(data, &block->data[offset], size);
	return 0;
}

/* writes size bytes (BSTORE_CHUNK at most) at offset of the open region's copy */
unsigned char bstoreWrite(unsigned char region, unsigned int offset, unsigned char size,
			  void *data)
{
	struct BstoreBlock *block;

	if (bstoreOpenState != BSTORE_OPEN)
		return 2;
	if (region != bstoreOpenRegion)
		return 2;
	if (offset + size > BSTORE_DATA_SIZE)
		return 8;
	if (size > BSTORE_CHUNK)
		return 8;
	block = (struct BstoreBlock *)&configBuffer;
	memcpy(&block->data[offset], data, size);
	return 0;
}

/* opens a region: its copy zeroed, erased or read from the flash */
unsigned char bstoreOpen(unsigned char region, unsigned char mode)
{
	if (region >= BSTORE_REGIONS)
		return 8;
	bstoreOpenRegion = region;
	bstoreOpenState = BSTORE_OPEN;
	switch (mode) {
	case BSTORE_OPEN_ERASED:
		memset(&configBuffer, 0xff, sizeof(struct BstoreBlock));
		break;
	case BSTORE_OPEN_READ:
		memcpy(&configBuffer, bstoreAddress(region), sizeof(struct BstoreBlock));
		break;
	case BSTORE_OPEN_ZERO:
	default:
		memset(&configBuffer, 0, sizeof(struct BstoreBlock));
		break;
	}
	return 0;
}

/* fills in the open copy's header and writes it to the region's block */
unsigned char bstoreCommit(unsigned char region)
{
	unsigned char result;
	unsigned int i;
	unsigned long sum;
	unsigned char *data;
	struct BstoreBlock *block;

	sum = 0;
	if (bstoreOpenState != BSTORE_OPEN)
		return 2;
	if (region != bstoreOpenRegion)
		return 2;
	block = (struct BstoreBlock *)&configBuffer;
	memcpy(&block->magic[0], bstoreMagic, 4);
	block->marker = 0xa5a5;
	block->size = BSTORE_DATA_SIZE;
	block->region = region;
	block->reserved = 0;
	data = block->data;
	for (i = 0; i < BSTORE_DATA_SIZE; i++)
		sum += *data;
	block->checksum = ~sum;
	bstoreOpenState = BSTORE_COMMITTING;
	bstoreRegions[region] = BSTORE_REGION_INVALID;
	result = configFlashWrite((unsigned long)bstoreAddress(region));
	bstoreOpenState = BSTORE_OPEN;
	if (result == 0)
		bstoreRegions[region] = BSTORE_REGION_VALID;
	else
		return result;
	return 0;
}

unsigned char bstoreClose(unsigned char region)
{
	bstoreOpenState = BSTORE_CLOSED;
	return 0;
}

/* each region of the old layout copied into its block; one not there written as zeroes */
unsigned char bstoreMigrate(void)
{
	unsigned char region;
	unsigned char result;
	unsigned char size;
	unsigned int offset;
	unsigned int left;
	unsigned char *data;

	for (region = 0; region < BSTORE_REGIONS; region++) {
		if (bstoreFindOld(region, &data) != 0) {
			bstoreRegions[region] = BSTORE_REGION_INVALID;
			memset(&configBuffer, 0, sizeof(struct BstoreBlock));
			result = configFlashWrite((unsigned long)bstoreAddress(region));
			if (result != 0)
				goto fail;
			continue;
		}
		result = bstoreOpen(region, BSTORE_OPEN_ERASED);
		if (result != 0)
			goto fail;
		offset = 0;
		left = BSTORE_DATA_SIZE;
		while (left != 0) {
			size = (unsigned char)(left > BSTORE_CHUNK ? BSTORE_CHUNK : left);
			result = bstoreWrite(region, offset, size, data);
			if (result != 0)
				goto fail;
			offset += size;
			left -= size;
			data += BSTORE_CHUNK;
		}
		result = bstoreCommit(region);
		if (result != 0)
			goto fail;
		bstoreClose(region);
	}
	return 0;
fail:
	bstoreClose(region);
	return result;
}

/* 0 when a region's block is valid */
static unsigned char bstoreCheck(unsigned char region)
{
	unsigned int i;
	unsigned long sum;
	unsigned char *data;
	struct BstoreBlock *block;

	block = bstoreAddress(region);
	if (memcmp(&block->magic[0], bstoreMagic, 4) != 0)
		return 7;
	if (block->marker != 0xa5a5)
		return 7;
	if (block->size != BSTORE_DATA_SIZE)
		return 7;
	sum = 0;
	data = block->data;
	for (i = 0; i < BSTORE_DATA_SIZE; i++)
		sum += *data;
	if (block->checksum != ~sum)
		return 7;
	return 0;
}

/* finds a region's valid block of the old layout; 0 and its data, or 9 */
static unsigned char bstoreFindOld(unsigned char region, unsigned char **data)
{
	unsigned int i;
	unsigned long sum;
	unsigned char *bytes;
	struct BstoreBlock *old;
	struct BstoreBlock *block;

	for (old = (struct BstoreBlock *)0x5e000; old < (struct BstoreBlock *)0x5ebff; old = old + 1) {
		block = old;
		if (memcmp(&block->magic[0], bstoreOldMagic, 4) != 0)
			continue;
		if (block->marker != 0xa5a5)
			continue;
		if (block->size != BSTORE_DATA_SIZE)
			continue;
		if (block->region != region)
			continue;
		sum = 0;
		bytes = block->data;
		for (i = 0; i < BSTORE_DATA_SIZE; i++)
			sum += *bytes;
		if (block->checksum != ~sum)
			continue;
		*data = (unsigned char *)block + offsetof(struct BstoreBlock, data);
		return 0;
	}
	return 9;
}

/* a region's block: region 0 the highest */
static struct BstoreBlock *bstoreAddress(unsigned char region)
{
	unsigned char slot;

	if (region < BSTORE_REGIONS)
		slot = (unsigned char)(2UL - region);
	else
		slot = 0;
	return (struct BstoreBlock *)(((int)slot << 10) + 0x5ec00UL);
}
