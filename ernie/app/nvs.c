/*
 * The NVS: 0x1000 bytes of settings kept in the data flash's last 16 blocks (0xf0-0xff,
 * 0x3c000-0x3ffff, 1 KiB each) through the flash library. Four blocks hold the data in
 * 0x20-byte records, four more take a copy when the data is moved, the rest are spares; a
 * block's header (at 0x3e8) names its place and generation. Area 0 is offsets 0-0xf3f, area 1
 * the 0x40 bytes after it.
 */
#pragma DI
#pragma EI

#include <string.h>

#include "ernie.h"
#include "nvs.h"

#define NVS_FIRST_BLOCK		0xf0
#define NVS_BLOCKS		0x10
#define NVS_DATA_BLOCKS		4
#define NVS_SPARE_BLOCKS	8
#define NVS_RECORD_SIZE		0x20
#define NVS_RECORDS		0x1f	/* a block's records */
#define NVS_HEADER		0x3e8	/* a block's header, after its records */
#define NVS_ENTRIES		0x19	/* a spare's entries */
#define NVS_AREA0_SIZE		0xf40
#define NVS_AREA1_SIZE		0x40

/* the flash library's status */
#define FLASH_OK		0x00
#define FLASH_ERASE_ERROR	0x1a
#define FLASH_NOT_BLANK		0x1b
#define FLASH_WRITE_ERROR	0x1c
#define FLASH_BUSY		0x1f

/* a block's header */
struct NvsHeader {
	unsigned char	mark;			/* 2 */
	unsigned char	index;			/* the block's place in the data */
	unsigned int	generation;		/* counts down with each move */
};

/* a block's first 8 bytes: its status (0x55555500 the first data block, 0x55555555 the others,
   0xaaaaaaaa or 0xaaaaaa00 a spare, all ones erased) */
struct NvsMark {
	unsigned long	status;
	unsigned long	extra;			/* all ones */
};

/* a block of the NVS, in the flash */
struct NvsBlock {
	struct NvsMark	mark;
	unsigned char	records[NVS_RECORDS][NVS_RECORD_SIZE];
	struct NvsHeader header;			/* at NVS_HEADER */
};

/* an entry of a spare: a record written since the data was last moved */
struct NvsEntry {
	unsigned int	index;			/* the record's, all ones while free */
	unsigned int	reserved;
	unsigned char	data[NVS_RECORD_SIZE];
	unsigned long	status;			/* 0xff00ffff once written */
};

/* a spare block, in the flash */
struct NvsSpareBlock {
	struct NvsMark	mark;
	struct NvsEntry	entries[NVS_ENTRIES];
};

static unsigned char nvsState;
static unsigned char nvsDataBlocks[NVS_DATA_BLOCKS];
static unsigned char nvsCopyBlocks[NVS_DATA_BLOCKS];
static unsigned char nvsSpareBlocks[NVS_SPARE_BLOCKS];
static unsigned char nvsRecord[NVS_RECORD_SIZE];
static unsigned char nvsBuffer[2 * NVS_RECORD_SIZE];

/* the flash library (the updater's, at 0xf2b2-0xf6f2) */
extern void sub_F6EE(void);
extern unsigned char sub_F365(unsigned char *descriptor);
extern unsigned char loc_F369(unsigned char *descriptor);
extern unsigned char sub_F2F0(void);
extern unsigned char sub_F2B2(unsigned char block);
extern unsigned char loc_F2B6(unsigned char block);
extern unsigned char loc_F2BA(unsigned int block);
extern unsigned char loc_F479(unsigned long address, unsigned char words);
extern void sub_F6F2(void);
extern unsigned char byteFE06C[];		/* the library's descriptor and write buffer */


static unsigned char nvsErase(unsigned char block);
static unsigned char nvsFlashWrite(unsigned char *address, void *data, unsigned char words,
				   unsigned char verify);
static unsigned char nvsReadRecords(unsigned char *buffer, unsigned int index,
				    unsigned char count);
static unsigned char nvsWriteRecords(unsigned char *buffer, unsigned int index,
				     unsigned char count);
unsigned char nvsMove(void);
unsigned char nvsClean(unsigned char count);
unsigned char nvsMount(unsigned char mode);
unsigned char nvsMarkData(unsigned char block, unsigned char first);
unsigned char nvsMarkSpare(unsigned char block, unsigned char first);
unsigned char nvsSetMark(unsigned char block, unsigned long extra);
unsigned char nvsMarkBad(unsigned char block);
unsigned char nvsAssignBlocks(void);
unsigned char nvsBlockRole(unsigned char block);
unsigned char nvsWriteBlockRecord(unsigned char *record, unsigned int index, unsigned char block);
unsigned char nvsGetRecord(unsigned char *record, unsigned int index);
unsigned char nvsPutRecord(unsigned char *record, unsigned int index);

unsigned char nvsRead(unsigned int offset, unsigned int size, unsigned char *data,
		      unsigned char area)
{
	unsigned int index;
	unsigned int start;
	unsigned int count;
	unsigned char result;

	switch (area) {
	case 0:
		if (offset + size > NVS_AREA0_SIZE)
			return 6;
		break;
	case 1:
		if (offset + size > NVS_AREA1_SIZE)
			return 6;
		offset += NVS_AREA0_SIZE;
		break;
	default:
		return 0x0b;
	}
	if (size > NVS_RECORD_SIZE)
		return 6;
	index = offset >> 5;
	start = (unsigned char)offset & 0x1f;
	count = (size + start + NVS_RECORD_SIZE - 1) >> 5;
	result = nvsReadRecords(nvsBuffer, index, (unsigned char)count);
	if (result == 0)
		memcpy(data, &nvsBuffer[start], size);
	return result;
}

unsigned char nvsWrite(unsigned int offset, unsigned int size, unsigned char *data,
		       unsigned char area)
{
	unsigned int index;
	unsigned int start;
	unsigned int count;
	unsigned char result;

	switch (area) {
	case 0:
		if (offset + size > NVS_AREA0_SIZE)
			return 6;
		break;
	case 1:
		if (offset + size > NVS_AREA1_SIZE)
			return 6;
		offset += NVS_AREA0_SIZE;
		break;
	default:
		return 0x0b;
	}
	if (size > NVS_RECORD_SIZE)
		return 6;
	index = offset >> 5;
	start = (unsigned char)offset & 0x1f;
	count = (size + start + NVS_RECORD_SIZE - 1) >> 5;
	result = nvsReadRecords(nvsBuffer, index, (unsigned char)count);
	if (result == 0) {
		memcpy(&nvsBuffer[start], data, size);
		do {
			result = nvsWriteRecords(nvsBuffer, index, (unsigned char)count);
			if (result != 7)
				break;
			result = nvsMove();
			if (result != 0)
				break;
		} while ((result = nvsClean(0x10)) == 0);
	}
	return result;
}

/* erases a block of the NVS: tried 5 more times while the erase fails */
static unsigned char nvsErase(unsigned char block)
{
	unsigned char result;
	unsigned char retry;

	retry = 5;
	if (block < NVS_FIRST_BLOCK)
		return 0x23;
	sub_F6EE();
	result = sub_F365(byteFE06C);
	while (result == FLASH_BUSY)
		result = loc_F369(byteFE06C);
	if (result == FLASH_OK) {
		do {
			result = sub_F2F0();
			if (result == FLASH_OK) {
				do
					result = sub_F2B2(block);
				while (result == FLASH_BUSY);
				if (result == FLASH_NOT_BLANK) {
					do
						result = loc_F2B6(block);
					while (result == FLASH_BUSY);
				}
			}
			if (result != FLASH_OK) {
			}
		} while (retry-- != 0 && result == FLASH_ERASE_ERROR);
	}
	DI();
	sub_F6F2();
	EI();
	return result;
}

/* writes words of 4 bytes at an address of the NVS, verifying the block if asked */
static unsigned char nvsFlashWrite(unsigned char *address, void *data, unsigned char words,
				   unsigned char verify)
{
	unsigned char result;
	unsigned char retry;

	retry = 0;
	if ((unsigned long)address < 0x3c000)
		return 0x23;
	sub_F6EE();
	result = sub_F365(byteFE06C);
	while (result == FLASH_BUSY)
		result = loc_F369(byteFE06C);
	if (result == FLASH_OK) {
		memcpy(byteFE06C, data, words << 2);
		do {
			result = sub_F2F0();
			if (result == FLASH_OK) {
				do
					result = loc_F479((unsigned long)address, words);
				while (result == FLASH_BUSY);
			}
			if (result == FLASH_OK && verify == 1) {
				do
					result = loc_F2BA((unsigned long)address >> 10);
				while (result == FLASH_BUSY);
			}
		} while (retry-- != 0 && result == FLASH_WRITE_ERROR);
	}
	sub_F6F2();
	return result;
}

unsigned char nvsInit(void)
{
	unsigned char result;

	nvsState = 0;
	result = nvsMount(0);
	if (result == 8) {
		result = nvsMount(1);
		if (result != 0)
			return result;
		if (nvsMove() != 0)
			return 8;
		result = nvsClean(0x10);
	}
	return result;
}

static unsigned char nvsReadRecords(unsigned char *buffer, unsigned int index,
				    unsigned char count)
{
	unsigned char result;

	result = 0;
	switch (nvsState) {
	case 0xf0:
		return 8;
	case 2:
	case 0x10:
	case 0x11:
	case 0x12:
		return 0x22;
	case 0:
		return 9;
	default:
		return 9;
	case 1:
	case 3:
	case 0x80:
		break;
	}
	while (count-- != 0) {
		result = nvsGetRecord(buffer, index);
		if (result != 0)
			break;
		buffer += NVS_RECORD_SIZE;
		index++;
	}
	return result;
}

static unsigned char nvsWriteRecords(unsigned char *buffer, unsigned int index,
				     unsigned char count)
{
	unsigned char result;

	result = 0;
	switch (nvsState) {
	case 3:
	case 0x12:
		return 7;
	case 0x80:
		return 0x21;
	case 0xf0:
		return 8;
	case 2:
	case 0x10:
	case 0x11:
		return 0x22;
	case 0:
		return 9;
	case 1:
		break;
	default:
		return 9;
	}
	while (count-- != 0) {
		result = nvsPutRecord(buffer, index);
		if (result != 0)
			break;
		buffer += NVS_RECORD_SIZE;
		index++;
	}
	return result;
}

unsigned char nvsGetState(void)
{
	unsigned char state;

	state = 0;
	state = nvsState;
	return state;
}

/* erases the NVS's blocks and gives them their places */
unsigned char nvsFormat(void)
{
	unsigned char result;
	unsigned char i;
	unsigned char block;
	unsigned char used;
	unsigned char retry;

	nvsState = 0;
	memset(nvsDataBlocks, 0, sizeof(nvsDataBlocks));
	memset(nvsCopyBlocks, 0, sizeof(nvsCopyBlocks));
	memset(nvsSpareBlocks, 0, sizeof(nvsSpareBlocks));
	block = NVS_FIRST_BLOCK;
	retry = 3;
	i = 0;
	used = 0;
	while (i < NVS_BLOCKS) {
		result = nvsErase(block);
		switch (result) {
		case FLASH_OK:
			if (used < NVS_DATA_BLOCKS) {
				nvsDataBlocks[used] = block;
				result = nvsMarkData(block, i == 0);
			} else if (used < 2 * NVS_DATA_BLOCKS) {
				nvsCopyBlocks[used - NVS_DATA_BLOCKS] = block;
			} else {
				nvsSpareBlocks[used - 2 * NVS_DATA_BLOCKS] = block;
				result = nvsMarkSpare(block, i == 0);
			}
			if (result != 0 && retry-- != 0)
				continue;
			used++;
			break;
		case FLASH_ERASE_ERROR:
			nvsMarkBad(block);
			break;
		default:
			if (result != 0 && retry-- != 0)
				continue;
			nvsState = 0xf0;
			return 9;
		}
		block++;
		i++;
		retry = 3;
	}
	if (used > 2 * NVS_DATA_BLOCKS)
		nvsState = 1;
	else
		nvsState = 0xf0;
	return 0;
}

/* moves the data to the copy blocks, which take the data blocks' places */
unsigned char nvsMove(void)
{
	unsigned char result;
	unsigned char record;
	unsigned char block;
	unsigned char i;
	unsigned char j;
	struct NvsBlock *flashBlock;
	unsigned long value;
	struct NvsHeader header;

	result = 0;
	if (nvsState != 1 && nvsState != 3)
		return 0x0a;
	nvsState = 0x10;
	record = 0;
	header = ((struct NvsBlock *)((unsigned long)nvsDataBlocks[0] << 10))->header;
	header.mark = 2;
	if (header.generation != 0)
		header.generation--;
	for (i = 0; i < NVS_DATA_BLOCKS; i++) {
		block = nvsCopyBlocks[i];
		if (block == 0) {
			result = 8;
			goto done;
		}
		result = nvsErase(block);
		if (result != 0)
			goto done;
		for (j = 0; j < NVS_RECORDS; j++) {
			nvsGetRecord(nvsRecord, record);
			result = nvsWriteBlockRecord(nvsRecord, record % NVS_RECORDS, block);
			if (result != 0)
				goto done;
			record++;
		}
		flashBlock = (struct NvsBlock *)((unsigned long)block << 10);
		header.index = i;
		result = nvsFlashWrite((unsigned char *)&flashBlock->header, &header, 1, 0);
		if (result != 0)
			goto done;
		nvsSetMark(nvsDataBlocks[i], 0xaaaaaaaa);
		nvsDataBlocks[i] = block;
	}
	for (i = 0; i < NVS_DATA_BLOCKS; i++) {
		flashBlock = (struct NvsBlock *)((unsigned long)nvsDataBlocks[i] << 10);
		value = (i == 0) ? 0x55555500 : 0x55555555;
		result = nvsFlashWrite((unsigned char *)&flashBlock->mark, &value, 1, 0);
		if (result != 0)
			goto done;
	}
	for (i = 0; i < NVS_SPARE_BLOCKS; i++) {
		block = nvsSpareBlocks[i];
		nvsSetMark(block, 0xaaaaaaaa);
	}
	result = nvsAssignBlocks();
done:
	switch (result) {
	case 0:
		nvsState = 0x12;
		break;
	case 0x21:
		nvsState = 0x80;
		break;
	default:
		nvsState = 3;
		break;
	}
	return result;
}

/* erases up to count of the copy and spare blocks that are not blank */
unsigned char nvsClean(unsigned char count)
{
	unsigned char result;
	unsigned char i;
	unsigned char block;
	unsigned char n;
	unsigned char retry;
	struct NvsMark *mark;

	result = 0x20;
	switch (nvsState) {
	case 1:
	case 3:
	case 0x12:
		break;
	case 0x80:
		return 0x21;
	case 0xf0:
		return 8;
	case 2:
	case 0x10:
		return 0x22;
	case 0:
		return 9;
	case 0x11:
		for (i = 0; i < NVS_DATA_BLOCKS; i++) {
			block = nvsCopyBlocks[i];
			nvsSetMark(block, 0xaaaaaaaa);
		}
		for (i = 0; i < NVS_SPARE_BLOCKS; i++) {
			block = nvsSpareBlocks[i];
			nvsSetMark(block, 0xaaaaaaaa);
		}
		nvsState = 0x12;
		break;
	default:
		return 9;
	}
	for (i = 0, n = 0; i < NVS_DATA_BLOCKS + NVS_SPARE_BLOCKS && count != 0; i++) {
		if (n < NVS_DATA_BLOCKS) {
			block = nvsCopyBlocks[n];
			mark = (struct NvsMark *)((unsigned long)block << 10);
			if (mark->extra == 0xffffffff && mark->status == 0xffffffff) {
				n++;
				continue;
			}
		} else {
			block = nvsSpareBlocks[n - NVS_DATA_BLOCKS];
			if (block == 0) {
				n++;
				continue;
			}
			mark = (struct NvsMark *)((unsigned long)block << 10);
			if (mark->extra == 0xffffffff &&
			    (mark->status == 0xaaaaaaaa || mark->status == 0xaaaaaa00)) {
				n++;
				continue;
			}
		}
		retry = 3;
		do {
			result = nvsErase(block);
			switch (result) {
			case FLASH_OK:
				if (n >= NVS_DATA_BLOCKS) {
					nvsMarkSpare(block, n - NVS_DATA_BLOCKS == 0);
					if (nvsState == 0x12)
						nvsState = 1;
				}
				break;
			case FLASH_ERASE_ERROR:
				if (retry == 0) {
					nvsMarkBad(block);
					result = nvsAssignBlocks();
					if (result != 0) {
						nvsState = 0x80;
						return result;
					}
				}
				break;
			default:
				return result;
			}
		} while (retry-- != 0 && result != 0);
		count--;
		n++;
	}
	if (nvsState == 3 && (result == 0x20 || i == NVS_DATA_BLOCKS + NVS_SPARE_BLOCKS))
		nvsState = 1;
	return result;
}

/*
 * finds the NVS's blocks from their marks: the first data block (status 0x55555500), the
 * blocks after it in turn as the other data blocks, the copy blocks and the spares. Mode 0
 * takes only data blocks still erased after their mark, mode 1 any not cleared.
 */
unsigned char nvsMount(unsigned char mode)
{
	unsigned char j;
	unsigned char block;
	unsigned char i;
	unsigned char count;
	unsigned char flags;
	struct NvsMark *mark;

	memset(nvsDataBlocks, 0, sizeof(nvsDataBlocks));
	memset(nvsCopyBlocks, 0, sizeof(nvsCopyBlocks));
	memset(nvsSpareBlocks, 0, sizeof(nvsSpareBlocks));
	flags = 0;
	count = 0;
	for (i = 0; i < NVS_BLOCKS; i++) {
		block = (i & 0x0f) + NVS_FIRST_BLOCK;
		mark = (struct NvsMark *)((unsigned long)block << 10);
		if ((mark->status == 0x55555555 || mark->status == 0x55555500) &&
		    mark->extra == 0xaaaaaaaa)
			count++;
	}
	if (count != 0) {
		if (mode == 0)
			flags |= 0x02;
		else
			flags |= 0x10;
	}
	count = 0;
	j = 0;
	for (i = 0; i < NVS_BLOCKS && count == 0; i++, j++) {
		block = (j & 0x0f) + NVS_FIRST_BLOCK;
		mark = (struct NvsMark *)((unsigned long)block << 10);
		if (mark->status == 0x55555500) {
			switch (mode) {
			case 0:
				if (mark->extra == 0xffffffff)
					nvsDataBlocks[count++] = block;
				break;
			case 1:
			default:
				if (mark->extra != 0 && mark->extra != 0xffffffff)
					nvsDataBlocks[count++] = block;
				break;
			}
		}
	}
	for (i = 0; i < NVS_BLOCKS - 1; i++, j++) {
		block = (j & 0x0f) + NVS_FIRST_BLOCK;
		mark = (struct NvsMark *)((unsigned long)block << 10);
		if (mark->extra == 0)
			continue;
		if (count < NVS_DATA_BLOCKS) {
			if (mark->status == 0x55555555) {
				switch (mode) {
				case 0:
					if (mark->extra == 0xffffffff)
						nvsDataBlocks[count++] = block;
					break;
				case 1:
				default:
					if (mark->extra != 0)
						nvsDataBlocks[count++] = block;
					break;
				}
			}
		} else if (count < 2 * NVS_DATA_BLOCKS) {
			if (mark->status != 0xaaaaaaaa || (flags & 0x02) != 0) {
				nvsCopyBlocks[count++ - NVS_DATA_BLOCKS] = block;
				if (mark->status != 0xffffffff || mark->extra != 0xffffffff)
					flags |= 0x04;
			}
		} else {
			if (mark->status == 0xaaaaaaaa || (flags & 0x02) != 0) {
				nvsSpareBlocks[count++ - 2 * NVS_DATA_BLOCKS] = block;
				if (mark->extra != 0xffffffff)
					flags |= 0x08;
			}
		}
	}
	if (count < 2 * NVS_DATA_BLOCKS) {
		flags |= 0x01;
	} else if (count == 2 * NVS_DATA_BLOCKS) {
		flags |= 0x20;
	} else if ((flags & 0x04) != 0) {
		block = nvsSpareBlocks[count - 1 - 2 * NVS_DATA_BLOCKS];
		mark = (struct NvsMark *)((unsigned long)block << 10);
		if (mark->extra != 0xaaaaaaaa)
			flags |= 0x40;
	}
	if (flags == 0) {
		nvsState = 1;
	} else if ((flags & 0x01) != 0) {
		nvsState = 0xf0;
		return 8;
	} else if ((flags & 0x20) != 0) {
		nvsState = 0x80;
		if (flags == 0x20 || mode == 1)
			return 0;
		return 8;
	} else if ((flags & 0x10) != 0) {
		nvsState = 3;
	} else if ((flags & 0x40) != 0) {
		nvsState = 0x11;
	} else if ((flags & 0x0a) != 0) {
		nvsState = 0x12;
	} else {
		nvsState = 1;
	}
	return 0;
}

/* the data's header mark, the spares missing and the moves made */
unsigned char nvsGetInfo(unsigned char *mark, unsigned char *missing, unsigned int *moves)
{
	struct NvsHeader header;
	unsigned char i;

	switch (nvsState) {
	case 0xf0:
		return 8;
	case 0:
		return 9;
	}
	header = ((struct NvsBlock *)((unsigned long)nvsDataBlocks[0] << 10))->header;
	*mark = header.mark;
	*moves = 0xffff - header.generation;
	*missing = 0;
	for (i = 0; i < NVS_SPARE_BLOCKS; i++) {
		if (nvsSpareBlocks[i] == 0)
			(*missing)++;
	}
	return 0;
}

void sub_233C0(void)
{
}

/* marks a block as data (the first block of the data, or another) and writes its header */
unsigned char nvsMarkData(unsigned char block, unsigned char first)
{
	unsigned char result;
	union {
		unsigned long	status;
		struct NvsHeader header;
	} value;
	struct NvsBlock *flashBlock;

	flashBlock = (struct NvsBlock *)((unsigned long)block << 10);
	value.status = first ? 0x55555500 : 0x55555555;
	result = nvsFlashWrite((unsigned char *)&flashBlock->mark, &value, 1, 0);
	if (result == 0) {
		value.status = 0xffffffff;
		value.header.mark = 2;
		nvsFlashWrite((unsigned char *)&flashBlock->header, &value, 1, 0);
	}
	return result;
}

/* marks an erased block as a spare */
unsigned char nvsMarkSpare(unsigned char block, unsigned char first)
{
	unsigned char result;
	unsigned long status;
	struct NvsMark *mark;

	mark = (struct NvsMark *)((unsigned long)block << 10);
	status = 0xaaaaaaaa;
	result = nvsFlashWrite((unsigned char *)&mark->status, &status, 1, 0);
	return result;
}

/* writes the second word of a block's mark */
unsigned char nvsSetMark(unsigned char block, unsigned long extra)
{
	unsigned char result;
	struct NvsMark *mark;

	if (block < NVS_FIRST_BLOCK)
		return 0x23;
	mark = (struct NvsMark *)((unsigned long)block << 10);
	result = nvsFlashWrite((unsigned char *)&mark->extra, &extra, 1, 0);
	return result;
}

/* clears a block's mark: the block is bad */
unsigned char nvsMarkBad(unsigned char block)
{
	unsigned char result;
	unsigned long value;
	struct NvsMark *mark;

	mark = (struct NvsMark *)((unsigned long)block << 10);
	value = 0;
	result = nvsFlashWrite((unsigned char *)&mark->status, &value, 1, 0);
	if (result != 0) {
	}
	value = 0;
	result = nvsFlashWrite((unsigned char *)&mark->extra, &value, 1, 0);
	if (result != 0) {
	}
	return result;
}

/* takes the blocks after the last data block that hold no data as the copy blocks and spares */
unsigned char nvsAssignBlocks(void)
{
	unsigned char i;
	unsigned char n;
	unsigned char block;

	block = nvsDataBlocks[NVS_DATA_BLOCKS - 1];
	memset(nvsCopyBlocks, 0, sizeof(nvsCopyBlocks));
	memset(nvsSpareBlocks, 0, sizeof(nvsSpareBlocks));
	for (i = 0, n = 0; i < NVS_BLOCKS - NVS_DATA_BLOCKS; i++) {
		block = ((block - NVS_FIRST_BLOCK + 1) & 0x0f) + NVS_FIRST_BLOCK;
		if (nvsBlockRole(block) != 0)
			continue;
		if (((struct NvsMark *)((unsigned long)block << 10))->extra == 0)
			continue;
		if (n < NVS_DATA_BLOCKS)
			nvsCopyBlocks[n++] = block;
		else
			nvsSpareBlocks[n++ - NVS_DATA_BLOCKS] = block;
	}
	if (n >= NVS_DATA_BLOCKS + 1)
		return 0;
	return 0x21;
}

/* a block's role: 1 data, 2 copy, 3 spare, 0 none */
unsigned char nvsBlockRole(unsigned char block)
{
	unsigned char i;

	for (i = 0; i < NVS_DATA_BLOCKS; i++) {
		if (nvsDataBlocks[i] == block)
			return 1;
		if (nvsCopyBlocks[i] == block)
			return 2;
	}
	for (i = 0; i < NVS_SPARE_BLOCKS; i++) {
		if (nvsSpareBlocks[i] == block)
			return 3;
	}
	return 0;
}

/* writes a record into its place in a block */
unsigned char nvsWriteBlockRecord(unsigned char *record, unsigned int index, unsigned char block)
{
	unsigned char *address;
	struct NvsBlock *flashBlock;
	unsigned char result;

	flashBlock = (struct NvsBlock *)((unsigned long)block << 10);
	address = flashBlock->records[index];
	result = nvsFlashWrite(&address[0], record, NVS_RECORD_SIZE / 4, 0);
	return result;
}

/* reads a record: its newest entry in the spares, else its place in the data blocks */
unsigned char nvsGetRecord(unsigned char *record, unsigned int index)
{
	unsigned char i;
	unsigned char j;
	unsigned char block;
	struct NvsSpareBlock *spare;
	struct NvsBlock *flashBlock;
	struct NvsEntry *entry;

	i = NVS_SPARE_BLOCKS;
	spare = NULL;
	while (i-- != 0) {
		block = nvsSpareBlocks[i];
		if (block == 0)
			continue;
		spare = (struct NvsSpareBlock *)((unsigned long)block << 10);
		if (spare->mark.status != 0xaaaaaaaa)
			continue;
		j = NVS_ENTRIES;
		while (j-- != 0) {
			entry = &spare->entries[j];
			if (entry->status == 0xff00ffff && entry->index == index) {
				memcpy(record, entry->data, NVS_RECORD_SIZE);
				return 0;
			}
		}
	}
	j = index % NVS_RECORDS;
	block = index / NVS_RECORDS;
	if (block >= NVS_DATA_BLOCKS)
		return 2;
	block = nvsDataBlocks[block];
	flashBlock = (struct NvsBlock *)((unsigned long)block << 10);
	memcpy(record, flashBlock->records[j], NVS_RECORD_SIZE);
	return 0;
}

/* writes a record as a new entry in the first spare with room */
unsigned char nvsPutRecord(unsigned char *record, unsigned int index)
{
	struct NvsSpareBlock *spare;
	struct NvsEntry *entry;
	unsigned char j;
	unsigned char block;
	unsigned char i;
	unsigned char result;
	unsigned long value;

	spare = NULL;
	result = 0;
	if (index > NVS_DATA_BLOCKS * NVS_RECORDS)
		return 2;
	nvsState = 2;
	for (i = 0; i < NVS_SPARE_BLOCKS; i++) {
		block = nvsSpareBlocks[i];
		if (block == 0)
			continue;
		spare = (struct NvsSpareBlock *)((unsigned long)block << 10);
		if (spare->mark.status != 0xaaaaaaaa)
			continue;
		if (spare->mark.extra != 0xffffffff)
			continue;
		for (j = 0; j < NVS_ENTRIES; j++) {
			entry = &spare->entries[j];
			if (entry->status != 0xffffffff || entry->index != 0xffff)
				continue;
			value = index;
			result = nvsFlashWrite((unsigned char *)&entry->index, &value, 1, 0);
			if (result != 0)
				continue;
			result = nvsFlashWrite(entry->data, record, NVS_RECORD_SIZE / 4, 0);
			if (result != 0)
				continue;
			value = 0xff00ffff;
			result = nvsFlashWrite((unsigned char *)&entry->status, &value, 1, 0);
			if (result != 0)
				continue;
			nvsState = 1;
			return 0;
		}
	}
	nvsState = 3;
	return 7;
}
