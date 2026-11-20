/*
 * The NVS: 0x1000 bytes of settings kept in the data flash's last 16 blocks (0xf0-0xff,
 * 0x3c000-0x3ffff, 1 KiB each) through the flash library. Four blocks (three on PCH-1000)
 * hold the data in 0x20-byte records, as many more take a copy when the data is moved, the
 * rest are spares; a block's header (at 0x3e8) names its place and generation. Area 0 is
 * offsets 0-0xf3f (0-0xb5f), area 1 the 0x40 bytes after it.
 */
#pragma sfr
#pragma DI
#pragma EI

#include <string.h>

#include "ernie.h"
#include "board.h"
#include "nvs.h"
#include "trace.h"
#if BOARD_DOL1001
#include "flash.h"
#endif

#if BOARD_USS1001
/*
 * USS-1001's NVS is its own: 0x5d records of 0x20 bytes (area 0 offsets 0-0xb5f, area 1 the
 * 0x40 bytes after it), logged into the data flash's eight blocks (0xf1000-0xf2fff, 1 KiB
 * each) through flash.c. A block in use has a header and 0x1c entries; a record is written to
 * the first free entry of the newest block, and read from its newest entry in the newest block
 * that has one. While fewer than three blocks are free, the oldest block's live records are
 * copied forward and the block is retired, for nvsClean to erase.
 */
#include "flash.h"

#define NVS_BLOCKS		8
#define NVS_ENTRIES		0x1c	/* a block's entries */
#define NVS_RECORD_SIZE		0x20
#define NVS_RECORDS		0x5d	/* kept in the blocks */
#define NVS_AREA0_SIZE		0xb60	/* records 0-0x5a */
#if SOURCE_0104
/* USS-1002's area 1 is records 0x5b-0x60: 0x5d-0x60 are kept as 0x57-0x5a, which read blank
   and refuse a write (nvsGetRecord, nvsPutRecord) */
#define NVS_AREA1_SIZE		0xc0
#define NVS_BLANK_FIRST		0x57
#define NVS_MOVED_FIRST		0x5b	/* the first not remapped */
#define NVS_REMAP_FIRST		0x5d
#define NVS_REMAP_END		0x61
#else
#define NVS_AREA1_SIZE		0x40
#endif
#define NVS_FREE_MIN		3	/* the free blocks kept */
#define NVS_COLLECT_MIN		5	/* the data blocks a collection needs */
#define NVS_ENTRY_VALID		0xaa
/* USS-1002's operations close the data flash library at their end (flash.c's flashClose) */
#define NVS_FLASH_CLOSE		(SOURCE_0104)
#if FW_SINCE(0x01030511)
#define NVS_HEADER_MARK		0x42
#else
#define NVS_HEADER_MARK		0x41	/* 0x01030505 */
#endif

/* nvs.state: also 1 ready, 2 busy, 3 full, 0x20 collecting */
#define NVS_STATE_COLLECTING	0x20

/* a block's role (struct NvsBlockInfo) */
#define NVS_BLOCK_ERASED	0
#define NVS_BLOCK_DATA		1
#define NVS_BLOCK_USED		2	/* retired, to be erased */
#define NVS_BLOCK_BAD		3

#define NVS_STATUS_DATA		0xffff55ff
#define NVS_STATUS_USED		0xff5555ff
#define NVS_STATUS_ERASED	0xffffffff

/* the list functions refuse a list and a block both missing; USS-1002's either */
#if SOURCE_0104
#define NVS_LIST_NULL(list, info)	((list) == NULL || (info) == NULL)
#else
#define NVS_LIST_NULL(list, info)	((list) == NULL && (info) == NULL)
#endif

/* a block's header: its status (bytes 0 and 3 cleared when it went bad), the sequence of the
   data blocks, oldest first */
struct NvsHeader {
	union {
		unsigned long	value;
		unsigned char	bytes[4];
	} status;
	unsigned long	sequence;
	unsigned char	reserved[2];
	unsigned char	mark;			/* NVS_HEADER_MARK */
	unsigned char	reserved2[5];
};

/* an entry of a block: a record, live while valid and erased hold their marks */
struct NvsEntry {
	unsigned char	index;			/* the record's, all ones while free */
	unsigned char	valid;			/* NVS_ENTRY_VALID, written last */
	unsigned char	erased;			/* all ones */
	unsigned char	data[NVS_RECORD_SIZE];
	unsigned char	end;			/* all ones */
};

/* a block of the NVS, in the data flash */
struct NvsBlock {
	struct NvsHeader header;
	struct NvsEntry	entries[NVS_ENTRIES];
};

#define NVS_BLOCK(block)	((struct NvsBlock *)(((unsigned long)(block) << 10) + 0xf1000))

/* a block, in the list of its role */
struct NvsBlockInfo {
	unsigned char	block;
	unsigned char	role;
	unsigned long	sequence;
	struct NvsBlockInfo *next;
	struct NvsBlockInfo *prev;
};

struct NvsList {
	struct NvsBlockInfo *head;
	struct NvsBlockInfo *tail;
	unsigned char	count;
};

/* the NVS's state, its blocks and their lists, and the buffers the blocks are read into */
static struct {
	unsigned char	state;
	struct NvsBlockInfo blocks[NVS_BLOCKS];
	struct NvsList	dataList;		/* oldest first */
	struct NvsList	freeList;
	struct NvsList	badList;
	struct NvsEntry	entry;
	struct NvsHeader header;
	unsigned char	moved[NVS_ENTRIES];	/* a collection's indexes seen; nvsGetInfo's bits */
} nvs;
/* two records */
static union {
	unsigned int	words[NVS_RECORD_SIZE];
	unsigned char	bytes[2 * NVS_RECORD_SIZE];
} nvsBuffer;

static unsigned char nvsReadRecords(unsigned char *buffer, unsigned int index,
				    unsigned char count);
static unsigned char nvsWriteRecords(unsigned char *buffer, unsigned int index,
				     unsigned char count);
unsigned char nvsCollect(unsigned char count);
unsigned char nvsMount(unsigned char mode);
unsigned char nvsMarkData(unsigned char block);
unsigned char nvsMarkBad(unsigned char block);
unsigned char nvsRetire(unsigned char block);
struct NvsBlockInfo *nvsAllocBlock(void);
unsigned char nvsGetRecord(unsigned char *record, unsigned int index);
unsigned char nvsPutRecord(unsigned char *record, unsigned int index);
unsigned char nvsListPushFront(struct NvsList *list, struct NvsBlockInfo *info);
unsigned char nvsListAppend(struct NvsList *list, struct NvsBlockInfo *info);
unsigned char nvsListRemove(struct NvsList *list, struct NvsBlockInfo *info);
struct NvsBlockInfo *nvsListPopFront(struct NvsList *list);
#else
#if BOARD_DOL1001	/* DOL-1001's blocks count from the NVS's first, at 0x3c000 */
#define NVS_FIRST_BLOCK		0x00
#define NVS_BASE		0x3c000
#define NVS_NO_BLOCK		0x80	/* a place without a block */
#else
#define NVS_FIRST_BLOCK		0xf0
#define NVS_BASE		0
#define NVS_NO_BLOCK		0x00
#endif
#define NVS_ADDRESS(block)	(((unsigned long)(block) << 10) + NVS_BASE)
#define NVS_BLOCKS		0x10
#define NVS_DATA_BLOCKS		BOARD_NVS_DATA_BLOCKS
#define NVS_SPARE_BLOCKS	(NVS_BLOCKS - 2 * NVS_DATA_BLOCKS)
#define NVS_RECORD_SIZE		0x20
#define NVS_RECORDS		0x1f	/* a block's records */
#define NVS_HEADER		0x3e8	/* a block's header, after its records */
#define NVS_ENTRIES		0x19	/* a spare's entries */
#if BOARD_DOL1002
/* DOL-1002's area 1 is 0xc0 bytes: its last four records (0x5d-0x60) take the place of area
   0's last four (0x57-0x5a), which read as all ones and cannot be written */
#define NVS_AREA1_SIZE		0xc0
#define NVS_AREA0_SIZE		(NVS_DATA_BLOCKS * NVS_RECORDS * NVS_RECORD_SIZE - 0x40)
#define NVS_HIDDEN_FIRST	0x57
#define NVS_HIDDEN_END		0x5b
#define NVS_MOVED_FIRST		0x5d
#define NVS_MOVED_END		0x61
#define NVS_MOVED_SHIFT		(NVS_MOVED_FIRST - NVS_HIDDEN_FIRST)
#else
#define NVS_AREA1_SIZE		0x40
#define NVS_AREA0_SIZE		(NVS_DATA_BLOCKS * NVS_RECORDS * NVS_RECORD_SIZE - NVS_AREA1_SIZE)
#endif

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
#if BOARD_DOL1002
/* DOL-1002 copies a header as one 32-bit value */
#define NVS_HEADER_COPY(to, from)	(*(unsigned long *)&(to) = *(unsigned long *)&(from))
#else
#define NVS_HEADER_COPY(to, from)	((to) = (from))
#endif

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
/* two records, word-aligned (0x00601000's follows nvsRecord at an odd address) */
static union {
	unsigned int	words[NVS_RECORD_SIZE];
	unsigned char	bytes[2 * NVS_RECORD_SIZE];
} nvsBuffer;

/* the flash library (the updater's, at 0xf2b2-0xf6f2) */
extern void FSL_Open(void);
extern unsigned char FSL_Init(unsigned char *descriptor);
extern unsigned char FSL_Init_cont(unsigned char *descriptor);
extern unsigned char FSL_ModeCheck(void);
extern unsigned char FSL_BlankCheck(unsigned char block);
extern unsigned char FSL_Erase(unsigned char block);
extern unsigned char FSL_IVerify(unsigned int block);
extern unsigned char FSL_EEPROMWrite(unsigned long address, unsigned char words);
extern void FSL_Close(void);
extern unsigned char fslDataBuffer[];		/* the library's descriptor and write buffer */


#if !BOARD_DOL1001	/* DOL-1001's are flash.h's */
static unsigned char nvsErase(unsigned char block);
#if FW_SINCE(0x01000306)
static unsigned char nvsFlashWrite(unsigned char *address, void *data, unsigned char words,
				   unsigned char verify);
#else
static unsigned char nvsFlashWrite(unsigned char *address, void *data, unsigned char words);
#endif
#endif
static unsigned char nvsReadRecords(unsigned char *buffer, unsigned int index,
				    unsigned char count);
static unsigned char nvsWriteRecords(unsigned char *buffer, unsigned int index,
				     unsigned char count);
unsigned char nvsMove(void);
unsigned char nvsClean(unsigned char count);
unsigned char nvsMount(unsigned char mode);
unsigned char nvsMarkData(unsigned char block, unsigned char first);
unsigned char nvsMarkSpare(unsigned char block, unsigned char first);
#if FW_SINCE(0x01000306)
unsigned char nvsSetMark(unsigned char block, unsigned long extra);
#endif
unsigned char nvsMarkBad(unsigned char block);
unsigned char nvsAssignBlocks(void);
unsigned char nvsBlockRole(unsigned char block);
unsigned char nvsWriteBlockRecord(unsigned char *record, unsigned int index, unsigned char block);
unsigned char nvsGetRecord(unsigned char *record, unsigned int index);
unsigned char nvsPutRecord(unsigned char *record, unsigned int index);
#endif	/* BOARD_USS1001 */

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
	result = nvsReadRecords(nvsBuffer.bytes, index, (unsigned char)count);
	if (result == 0)
		memcpy(data, &nvsBuffer.bytes[start], size);
#if NVS_FLASH_CLOSE
	flashClose();
#endif
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
	result = nvsReadRecords(nvsBuffer.bytes, index, (unsigned char)count);
	if (result == 0) {
		memcpy(&nvsBuffer.bytes[start], data, size);
#if BOARD_USS1001
		result = nvsWriteRecords(nvsBuffer.bytes, index, (unsigned char)count);
		if (result != 0) {
			/* nothing to move: nvsWriteRecords collects the blocks itself */
		}
#else
		do {
			result = nvsWriteRecords(nvsBuffer.bytes, index, (unsigned char)count);
			if (result != 7)
				break;
			result = nvsMove();
			if (result != 0)
				break;
		} while ((result = nvsClean(0x10)) == 0);
#endif
	}
#if BOARD_USS1001 && NVS_FLASH_CLOSE
	flashClose();
#endif
	return result;
}

#if !BOARD_USS1001
#if !BOARD_DOL1001	/* DOL-1001's erase and write are flash.c's */
/* erases a block of the NVS: tried 5 more times while the erase fails */
#if FW_SINCE(0x01000306)
static unsigned char nvsErase(unsigned char block)
{
	unsigned char result;
	unsigned char retry;

	retry = 5;
	if (block < NVS_FIRST_BLOCK)
		return 0x23;
	FSL_Open();
	result = FSL_Init(fslDataBuffer);
	while (result == FLASH_BUSY)
		result = FSL_Init_cont(fslDataBuffer);
	if (result == FLASH_OK) {
		do {
#if BOARD_WDT
			WDTE = 0xac;
#endif
			result = FSL_ModeCheck();
			if (result == FLASH_OK) {
				do
					result = FSL_BlankCheck(block);
				while (result == FLASH_BUSY);
				if (result == FLASH_NOT_BLANK) {
					do
						result = FSL_Erase(block);
					while (result == FLASH_BUSY);
				}
			}
			if (result != FLASH_OK) {
			}
		} while (retry-- != 0 && result == FLASH_ERASE_ERROR);
	}
	DI();
	FSL_Close();
	EI();
	return result;
}
#elif FW_SINCE(0x01000104)
static unsigned char nvsErase(unsigned char block)
{
	unsigned char result;
	unsigned char retry;

	retry = 3;
	FSL_Open();
	result = FSL_Init(fslDataBuffer);
	while (result == FLASH_BUSY)
		result = FSL_Init_cont(fslDataBuffer);
	if (result == FLASH_OK) {
		do {
			result = FSL_ModeCheck();
			if (result == FLASH_OK) {
				do
					result = FSL_BlankCheck(block);
				while (result == FLASH_BUSY);
				if (result == FLASH_NOT_BLANK) {
					do
						result = FSL_Erase(block);
					while (result == FLASH_BUSY);
				}
			}
		} while (retry-- != 0 && result == FLASH_ERASE_ERROR);
	}
	DI();
	FSL_Close();
	EI();
	return result;
}
#else
/* before 0x01000104 nothing is retried */
static unsigned char nvsErase(unsigned char block)
{
	unsigned char result;

	FSL_Open();
	result = FSL_Init(fslDataBuffer);
	while (result == FLASH_BUSY)
		result = FSL_Init_cont(fslDataBuffer);
	result = FSL_ModeCheck();
	if (result == FLASH_OK) {
		do
			result = FSL_BlankCheck(block);
		while (result == FLASH_BUSY);
		if (result == FLASH_NOT_BLANK) {
			do
				result = FSL_Erase(block);
			while (result == FLASH_BUSY);
		}
	}
	DI();
	FSL_Close();
	EI();
	return result;
}
#endif

/* writes words of 4 bytes at an address of the NVS, verifying the block if asked */
#if FW_SINCE(0x01000306)
static unsigned char nvsFlashWrite(unsigned char *address, void *data, unsigned char words,
				   unsigned char verify)
{
	unsigned char result;
	unsigned char retry;

	retry = 0;
	if ((unsigned long)address < 0x3c000)
		return 0x23;
	FSL_Open();
	result = FSL_Init(fslDataBuffer);
	while (result == FLASH_BUSY)
		result = FSL_Init_cont(fslDataBuffer);
	if (result == FLASH_OK) {
		memcpy(fslDataBuffer, data, words << 2);
		do {
			result = FSL_ModeCheck();
			if (result == FLASH_OK) {
				do
					result = FSL_EEPROMWrite((unsigned long)address, words);
				while (result == FLASH_BUSY);
			}
			if (result == FLASH_OK && verify == 1) {
				do
					result = FSL_IVerify((unsigned long)address >> 10);
				while (result == FLASH_BUSY);
			}
		} while (retry-- != 0 && result == FLASH_WRITE_ERROR);
	}
	FSL_Close();
	return result;
}
#else
static unsigned char nvsFlashWrite(unsigned char *address, void *data, unsigned char words)
{
	unsigned char result;
#if FW_SINCE(0x01000104)
	unsigned char retry;

	retry = 3;
	FSL_Open();
	result = FSL_Init(fslDataBuffer);
	while (result == FLASH_BUSY)
		result = FSL_Init_cont(fslDataBuffer);
	if (result == FLASH_OK) {
		memcpy(fslDataBuffer, data, words << 2);
		do {
			result = FSL_ModeCheck();
			if (result == FLASH_OK) {
				do
					result = FSL_EEPROMWrite((unsigned long)address, words);
				while (result == FLASH_BUSY);
			}
		} while (retry-- != 0 && result == FLASH_WRITE_ERROR);
	}
#else
	/* before 0x01000104 nothing is retried */
	FSL_Open();
	result = FSL_Init(fslDataBuffer);
	while (result == FLASH_BUSY)
		result = FSL_Init_cont(fslDataBuffer);
	result = FSL_ModeCheck();
	if (result == FLASH_OK) {
		memcpy(fslDataBuffer, data, words << 2);
		do
			result = FSL_EEPROMWrite((unsigned long)address, words);
		while (result == FLASH_BUSY);
	}
#endif
	FSL_Close();
	return result;
}

/* no verifying before 0x01000306 */
#define nvsFlashWrite(address, data, words, verify)	nvsFlashWrite(address, data, words)
#endif
#endif	/* !BOARD_DOL1001 */

unsigned char nvsInit(void)
{
	unsigned char result;

	nvsState = 0;
	result = nvsMount(0);
	if (result == 8) {
		result = nvsMount(1);
		if (result != 0)
			return result;
#if FW_SINCE(0x00090804)
		if (nvsMove() != 0)
			return 8;
		result = nvsClean(0x10);
#endif
	}
	return result;
}

#if FW_SINCE(0x01000306)
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
#elif FW_SINCE(0x00090804)
static unsigned char nvsReadRecords(unsigned char *buffer, unsigned int index,
				    unsigned char count)
{
	unsigned char result;

	result = 0;
	switch (nvsState) {
	case 0x80:
		return 8;
	case 2:
	case 3:
	case 0x82:
		return 0x22;
	case 0:
		return 9;
	default:
		return 9;
	case 1:
	case 0x81:
	case 0x83:
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
#else
/* 0x00090206: only a mounted NVS is read */
static unsigned char nvsReadRecords(unsigned char *buffer, unsigned int index,
				    unsigned char count)
{
	unsigned char result;

	result = 0;
	if (nvsState != 1)
		return 9;
	while (count-- != 0) {
		result = nvsGetRecord(buffer, index);
		if (result != 0)
			break;
		buffer += NVS_RECORD_SIZE;
		index++;
	}
	return result;
}
#endif

#if FW_SINCE(0x01000306)
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
#elif FW_SINCE(0x00090804)
static unsigned char nvsWriteRecords(unsigned char *buffer, unsigned int index,
				     unsigned char count)
{
	unsigned char result;

	result = 0;
	switch (nvsState) {
	case 3:
	case 0x81:
	case 0x82:
		return 7;
	case 0x83:
		return 0x21;
	case 0x80:
		return 8;
	case 2:
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
#else
static unsigned char nvsWriteRecords(unsigned char *buffer, unsigned int index,
				     unsigned char count)
{
	unsigned char result;

	result = 0;
	if (nvsState != 1)
		return 9;
	while (count-- != 0) {
		result = nvsPutRecord(buffer, index);
		if (result != 0)
			break;
		buffer += NVS_RECORD_SIZE;
		index++;
	}
	return result;
}
#endif

unsigned char nvsGetState(void)
{
	unsigned char state;

	state = 0;
	state = nvsState;
	return state;
}

/* erases the NVS's blocks and gives them their places */
#if FW_SINCE(0x01000306)
unsigned char nvsFormat(void)
{
	unsigned char result;
	unsigned char i;
	unsigned char block;
	unsigned char used;
	unsigned char retry;

	nvsState = 0;
	memset(nvsDataBlocks, NVS_NO_BLOCK, sizeof(nvsDataBlocks));
	memset(nvsCopyBlocks, NVS_NO_BLOCK, sizeof(nvsCopyBlocks));
	memset(nvsSpareBlocks, NVS_NO_BLOCK, sizeof(nvsSpareBlocks));
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
#elif FW_SINCE(0x00090804)
unsigned char nvsFormat(void)
{
	unsigned char result;
	unsigned char i;
	unsigned char block;
	unsigned char used;

	nvsState = 0;
	memset(nvsDataBlocks, 0, sizeof(nvsDataBlocks));
	memset(nvsCopyBlocks, 0, sizeof(nvsCopyBlocks));
	memset(nvsSpareBlocks, 0, sizeof(nvsSpareBlocks));
	block = NVS_FIRST_BLOCK;
	i = 0;
	used = 0;
	while (i < NVS_BLOCKS) {
		result = nvsErase(block);
		switch (result) {
		case FLASH_OK:
			if (used < NVS_DATA_BLOCKS) {
				nvsDataBlocks[used] = block;
				result = nvsMarkData(block, i == 0);
				if (result != 0)
					return result;
			} else if (used < 2 * NVS_DATA_BLOCKS) {
				nvsCopyBlocks[used - NVS_DATA_BLOCKS] = block;
			} else {
				nvsSpareBlocks[used - 2 * NVS_DATA_BLOCKS] = block;
				result = nvsMarkSpare(block, i == 0);
				if (result != 0)
					return result;
			}
			used++;
			break;
		case FLASH_ERASE_ERROR:
			nvsMarkBad(block);
			break;
		default:
			return result;
		}
		block++;
		i++;
	}
	nvsState = 1;
	return 0;
}
#else
/* 0x00090206: every block erased, then the data, copy and spare blocks in turn */
unsigned char nvsFormat(void)
{
	unsigned char result;
	unsigned char i;
	unsigned char block;

	nvsState = 0;
	block = NVS_FIRST_BLOCK;
	for (i = 0; i < NVS_BLOCKS; i++) {
		result = nvsErase(block);
		if (result != 0)
			return result;
		block++;
	}
	block = NVS_FIRST_BLOCK;
	for (i = 0; i < NVS_DATA_BLOCKS; i++) {
		nvsDataBlocks[i] = block;
		result = nvsMarkData(block++, i == 0);
		if (result != 0)
			return result;
	}
	for (i = 0; i < NVS_DATA_BLOCKS; i++) {
		nvsCopyBlocks[i] = block;
		block++;
	}
	for (i = 0; i < NVS_SPARE_BLOCKS; i++) {
		nvsSpareBlocks[i] = block;
		result = nvsMarkSpare(block++, i == 0);
		if (result != 0)
			return result;
	}
	nvsState = 1;
	return 0;
}
#endif

/* moves the data to the copy blocks, which take the data blocks' places */
#if FW_SINCE(0x01000306)
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
	NVS_HEADER_COPY(header, ((struct NvsBlock *)NVS_ADDRESS(nvsDataBlocks[0]))->header);
	header.mark = 2;
	if (header.generation != 0)
		header.generation--;
	for (i = 0; i < NVS_DATA_BLOCKS; i++) {
#if BOARD_WDT || BOARD_DOL1001
		WDTE = 0xac;
#endif
		block = nvsCopyBlocks[i];
		if (block == NVS_NO_BLOCK) {
			result = 8;
			goto done;
		}
		result = nvsErase(block);
		if (result != 0)
			goto done;
		for (j = 0; j < NVS_RECORDS; j++) {
#if BOARD_DOL1001
			WDTE = 0xac;
#endif
			nvsGetRecord(nvsRecord, record);
			result = nvsWriteBlockRecord(nvsRecord, record % NVS_RECORDS, block);
			if (result != 0)
				goto done;
			record++;
		}
		flashBlock = (struct NvsBlock *)NVS_ADDRESS(block);
		header.index = i;
		result = nvsFlashWrite((unsigned char *)&flashBlock->header, &header, 1, 0);
		if (result != 0)
			goto done;
		nvsSetMark(nvsDataBlocks[i], 0xaaaaaaaa);
		nvsDataBlocks[i] = block;
	}
	for (i = 0; i < NVS_DATA_BLOCKS; i++) {
		flashBlock = (struct NvsBlock *)NVS_ADDRESS(nvsDataBlocks[i]);
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
#elif FW_SINCE(0x00090804)
unsigned char nvsMove(void)
{
	unsigned char result;
	unsigned char record;
	unsigned char block;
	unsigned char i;
	unsigned char j;
	struct NvsBlock *flashBlock;
	struct NvsMark *mark;
	unsigned long value;
	struct NvsHeader header;

	result = 0;
	if (nvsState != 1 && nvsState != 0x81)
		return 0x0a;
	nvsState = 3;
	record = 0;
	NVS_HEADER_COPY(header, ((struct NvsBlock *)NVS_ADDRESS(nvsDataBlocks[0]))->header);
	header.mark = 1;
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
			nvsWriteBlockRecord(nvsRecord, record % NVS_RECORDS, block);
			record++;
		}
		flashBlock = (struct NvsBlock *)NVS_ADDRESS(block);
		header.index = i;
		result = nvsFlashWrite((unsigned char *)&flashBlock->header, &header, 1, 0);
		if (result != 0)
			goto done;
		mark = (struct NvsMark *)NVS_ADDRESS(nvsDataBlocks[i]);
		nvsDataBlocks[i] = block;
		value = 0xaaaaaaaa;
		result = nvsFlashWrite((unsigned char *)&mark->extra, &value, 1, 0);
	}
	for (i = 0; i < NVS_DATA_BLOCKS; i++) {
		flashBlock = (struct NvsBlock *)NVS_ADDRESS(nvsDataBlocks[i]);
		value = (i == 0) ? 0x55555500 : 0x55555555;
		result = nvsFlashWrite((unsigned char *)&flashBlock->mark, &value, 1, 0);
		if (result != 0)
			goto done;
	}
	value = 0xaaaaaaaa;
	for (i = 0; i < NVS_SPARE_BLOCKS; i++) {
		block = nvsSpareBlocks[i];
		if (block != 0) {
			flashBlock = (struct NvsBlock *)NVS_ADDRESS(block);
			result = nvsFlashWrite((unsigned char *)&((struct NvsMark *)flashBlock)->extra,
					       &value, 1, 0);
		}
	}
	result = nvsAssignBlocks();
done:
	switch (result) {
	case 0:
		nvsState = 0x82;
		break;
	case 0x21:
		nvsState = 0x83;
		break;
	default:
		nvsState = 0x81;
		break;
	}
	return result;
}
#else
/* 0x00090206: the records copied in turn; the old data blocks marked, the blocks' places turned */
unsigned char nvsMove(void)
{
	unsigned char result;
	unsigned char index;
	unsigned char block;
	unsigned char i;
	unsigned char record;
	struct NvsMark *mark;
	struct NvsMark *old;
	unsigned long status;

	result = 0;
	if (nvsState != 1 && nvsState != NVS_STATE_FULL)
		return 10;
	nvsState = 3;
	index = 0;
	for (i = 0; i < NVS_DATA_BLOCKS; i++) {
		block = nvsCopyBlocks[i];
		if (block == 0) {
			result = 8;
			goto out;
		}
		result = nvsErase(block);
		if (result != 0)
			goto out;
		for (record = 0; record < NVS_RECORDS; record++) {
			nvsGetRecord(nvsRecord, index);
			nvsWriteBlockRecord(nvsRecord, index % NVS_RECORDS, block);
			index++;
		}
		old = (struct NvsMark *)NVS_ADDRESS(nvsDataBlocks[i]);
		nvsDataBlocks[i] = block;
		nvsCopyBlocks[i] = ((block - (NVS_FIRST_BLOCK - NVS_DATA_BLOCKS)) & 0x0f) +
				   NVS_FIRST_BLOCK;
		status = 0xaaaaaaaa;
		result = nvsFlashWrite((unsigned char *)&old->extra, &status, 1, 0);
	}
	for (i = 0; i < NVS_DATA_BLOCKS; i++) {
		mark = (struct NvsMark *)NVS_ADDRESS(nvsDataBlocks[i]);
		status = i == 0 ? 0x55555500 : 0x55555555;
		result = nvsFlashWrite((unsigned char *)&mark->status, &status, 1, 0);
	}
	status = 0xaaaaaaaa;
	for (i = 0; i < NVS_SPARE_BLOCKS; i++) {
		mark = (struct NvsMark *)NVS_ADDRESS(nvsSpareBlocks[i]);
		result = nvsFlashWrite((unsigned char *)&mark->extra, &status, 1, 0);
	}
	block = nvsCopyBlocks[NVS_DATA_BLOCKS - 1] - NVS_FIRST_BLOCK;
	for (i = 0; i < NVS_SPARE_BLOCKS; i++) {
		block++;
		nvsSpareBlocks[i] = (block & 0x0f) + NVS_FIRST_BLOCK;
	}
	nvsState = NVS_STATE_MOVED;
out:
	return result;
}
#endif

/* erases up to count of the copy and spare blocks that are not blank */
#if FW_SINCE(0x01000306)
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
			mark = (struct NvsMark *)NVS_ADDRESS(block);
			if (mark->extra == 0xffffffff && mark->status == 0xffffffff) {
				n++;
				continue;
			}
		} else {
			block = nvsSpareBlocks[n - NVS_DATA_BLOCKS];
			if (block == NVS_NO_BLOCK) {
				n++;
				continue;
			}
			mark = (struct NvsMark *)NVS_ADDRESS(block);
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
#elif FW_SINCE(0x00090804)
unsigned char nvsClean(unsigned char count)
{
	unsigned char result;
	unsigned char i;
	unsigned char block;
	unsigned char n;
	struct NvsMark *mark;

	result = 0x20;
	for (i = 0, n = 0; i < NVS_DATA_BLOCKS + NVS_SPARE_BLOCKS && count != 0; i++) {
		if (n < NVS_DATA_BLOCKS) {
			block = nvsCopyBlocks[n];
			mark = (struct NvsMark *)NVS_ADDRESS(block);
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
			mark = (struct NvsMark *)NVS_ADDRESS(block);
			if (mark->extra == 0xffffffff &&
			    (mark->status == 0xaaaaaaaa || mark->status == 0xaaaaaa00)) {
				n++;
				continue;
			}
		}
		result = nvsErase(block);
		switch (result) {
		case FLASH_OK:
			if (n >= NVS_DATA_BLOCKS) {
				nvsMarkSpare(block, i == 0);
				if (nvsState == 0x82)
					nvsState = 1;
			}
			break;
		case FLASH_ERASE_ERROR:
			nvsMarkBad(block);
			result = nvsAssignBlocks();
			if (result != 0) {
				nvsState = 0x83;
				return result;
			}
			continue;
		default:
			return result;
		}
		count--;
		n++;
	}
	return result;
}
#else
/* 0x00090206: the spares not marked as such, then the copy blocks not blank */
unsigned char nvsClean(unsigned char count)
{
	unsigned char result;
	unsigned char i;
	unsigned char block;
	struct NvsMark *mark;

	result = 0x20;
	for (i = 0; i < NVS_SPARE_BLOCKS && count != 0; i++) {
		block = nvsSpareBlocks[i];
		mark = (struct NvsMark *)NVS_ADDRESS(block);
		if (mark->extra != 0xffffffff ||
		    (mark->status != 0xaaaaaaaa && mark->status != 0xaaaaaa00)) {
			result = nvsErase(block);
			if (result != 0)
				return result;
			nvsMarkSpare(block, i == 0);
			if (nvsState == NVS_STATE_MOVED)
				nvsState = 1;
			count--;
		}
	}
	for (i = 0; i < NVS_DATA_BLOCKS && count != 0; i++) {
		block = nvsCopyBlocks[i];
		mark = (struct NvsMark *)NVS_ADDRESS(block);
		if (mark->extra != 0xffffffff || mark->status != 0xffffffff) {
			result = nvsErase(block);
			if (result != 0)
				return result;
			count--;
		}
	}
	return result;
}
#endif

/*
 * finds the NVS's blocks from their marks: the first data block (status 0x55555500), the
 * blocks after it in turn as the other data blocks, the copy blocks and the spares. Mode 0
 * takes only data blocks still erased after their mark, mode 1 any not cleared.
 */
#if FW_SINCE(0x01000306)
unsigned char nvsMount(unsigned char mode)
{
	unsigned char j;
	unsigned char block;
	unsigned char i;
	unsigned char count;
	unsigned char flags;
	struct NvsMark *mark;

	memset(nvsDataBlocks, NVS_NO_BLOCK, sizeof(nvsDataBlocks));
	memset(nvsCopyBlocks, NVS_NO_BLOCK, sizeof(nvsCopyBlocks));
	memset(nvsSpareBlocks, NVS_NO_BLOCK, sizeof(nvsSpareBlocks));
	flags = 0;
	count = 0;
	for (i = 0; i < NVS_BLOCKS; i++) {
		block = (i & 0x0f) + NVS_FIRST_BLOCK;
		mark = (struct NvsMark *)NVS_ADDRESS(block);
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
		mark = (struct NvsMark *)NVS_ADDRESS(block);
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
		mark = (struct NvsMark *)NVS_ADDRESS(block);
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
		mark = (struct NvsMark *)NVS_ADDRESS(block);
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
#elif FW_SINCE(0x00090804)
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
	j = 0;
	flags = 0;
	count = 0;
	for (i = 0; i < NVS_BLOCKS && count == 0; j++, i++) {
		block = (j & 0x0f) + NVS_FIRST_BLOCK;
		mark = (struct NvsMark *)NVS_ADDRESS(block);
		if (mark->status == 0x55555500) {
			switch (mode) {
			case 0:
				if (mark->extra == 0xffffffff)
					nvsDataBlocks[count++] = block;
				else
					flags |= 0x08;
				break;
			case 1:
				if (mark->extra != 0 && mark->extra != 0xffffffff) {
					nvsDataBlocks[count++] = block;
					flags |= 0x08;
				}
				break;
			}
		}
	}
	for (i = 0; i < NVS_BLOCKS && count < NVS_DATA_BLOCKS; j++, i++) {
		block = (j & 0x0f) + NVS_FIRST_BLOCK;
		mark = (struct NvsMark *)NVS_ADDRESS(block);
		if (mark->status == 0x55555555) {
			switch (mode) {
			case 0:
				if (mark->extra == 0xffffffff)
					nvsDataBlocks[count++] = block;
				break;
			case 1:
				if (mark->extra != 0)
					nvsDataBlocks[count++] = block;
				break;
			}
		}
	}
	if (count != NVS_DATA_BLOCKS)
		flags |= 0x01;
	for (i = 0, count = 0; i < NVS_BLOCKS - NVS_DATA_BLOCKS; j++, i++) {
		block = (j & 0x0f) + NVS_FIRST_BLOCK;
		mark = (struct NvsMark *)NVS_ADDRESS(block);
		if (mark->extra == 0)
			continue;
		if (count < NVS_DATA_BLOCKS) {
			if (mark->status != 0xaaaaaaaa) {
				nvsCopyBlocks[count++] = block;
				if (mark->status != 0xffffffff || mark->extra != 0xffffffff)
					flags |= 0x02;
			}
		} else {
			if (mark->status == 0xaaaaaaaa) {
				nvsSpareBlocks[count++ - NVS_DATA_BLOCKS] = block;
				if (mark->extra != 0xffffffff)
					flags |= 0x04;
			}
		}
	}
	if (count < NVS_DATA_BLOCKS + 1)
		flags |= 0x10;
	if (flags == 0) {
		nvsState = 1;
	} else if ((flags & 0x01) != 0) {
		nvsState = 0x80;
		return 8;
	} else if ((flags & 0x10) != 0) {
		nvsState = 0x83;
		return 0;
	} else if ((flags & 0x08) != 0) {
		nvsState = 0x81;
		return 0;
	} else if ((flags & 0x08) != 0) {
		nvsState = 0x82;
		nvsClean(0x10);
		return 0;
	} else if ((flags & 0x06) == 0x06) {
		nvsState = 0x81;
		nvsClean(0x10);
		return 0;
	} else {
		nvsState = 1;
	}
	return 0;
}
#else
/* 0x00090206: the copy blocks and spares are the blocks after the data, whatever their marks */
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
	j = 0;
	flags = 0;
	count = 0;
	for (i = 0; i < NVS_BLOCKS && count == 0; j++, i++) {
		block = (j & 0x0f) + NVS_FIRST_BLOCK;
		mark = (struct NvsMark *)NVS_ADDRESS(block);
		if (mark->status == 0x55555500) {
			switch (mode) {
			case 0:
				if (mark->extra == 0xffffffff)
					nvsDataBlocks[count++] = block;
				else
					flags |= 0x08;
				break;
			case 1:
				if (mark->extra != 0 && mark->extra != 0xffffffff) {
					nvsDataBlocks[count++] = block;
					flags |= 0x08;
				}
				break;
			}
		}
	}
	for (i = 0; i < NVS_BLOCKS && count < NVS_DATA_BLOCKS; j++, i++) {
		block = (j & 0x0f) + NVS_FIRST_BLOCK;
		mark = (struct NvsMark *)NVS_ADDRESS(block);
		if (mark->status == 0x55555555) {
			switch (mode) {
			case 0:
				if (mark->extra == 0xffffffff)
					nvsDataBlocks[count++] = block;
				break;
			case 1:
				if (mark->extra != 0)
					nvsDataBlocks[count++] = block;
				break;
			}
		}
	}
	if (count != NVS_DATA_BLOCKS)
		flags |= 0x01;
	for (i = 0, count = 0; i < NVS_BLOCKS && count < NVS_DATA_BLOCKS; j++, i++) {
		block = (j & 0x0f) + NVS_FIRST_BLOCK;
		mark = (struct NvsMark *)NVS_ADDRESS(block);
		nvsCopyBlocks[count++] = block;
		if (mark->status != 0xffffffff || mark->extra != 0xffffffff)
			flags |= 0x02;
	}
	for (i = 0, count = 0; i < NVS_BLOCKS && count < NVS_SPARE_BLOCKS; j++, i++) {
		block = (j & 0x0f) + NVS_FIRST_BLOCK;
		mark = (struct NvsMark *)NVS_ADDRESS(block);
		nvsSpareBlocks[count++] = block;
		if (mark->status != 0xaaaaaaaa || mark->extra != 0xffffffff)
			flags |= 0x04;
	}
	if (flags == 0) {
		nvsState = 1;
	} else if ((flags & 0x01) != 0) {
		nvsState = NVS_STATE_FAILED;
		return 8;
	} else if ((flags & 0x08) != 0) {
		nvsState = NVS_STATE_FULL;
		return 0;
	} else if ((flags & 0x08) != 0) {
		nvsState = NVS_STATE_MOVED;
		nvsClean(0x10);
		return 0;
	} else if ((flags & 0x06) == 0x06) {
		nvsState = NVS_STATE_FULL;
		nvsClean(0x10);
		return 0;
	} else {
		nvsState = 1;
	}
	return 0;
}
#endif

#if FW_SINCE(0x00090804)
/* the data's header mark, the spares missing and the moves made */
unsigned char nvsGetInfo(unsigned char *mark, unsigned char *missing, unsigned int *moves)
{
	struct NvsHeader header;
	unsigned char i;

	switch (nvsState) {
	case NVS_STATE_FAILED:
		return 8;
	case 0:
		return 9;
	}
	NVS_HEADER_COPY(header, ((struct NvsBlock *)NVS_ADDRESS(nvsDataBlocks[0]))->header);
	*mark = header.mark;
	*moves = 0xffff - header.generation;
	*missing = 0;
	for (i = 0; i < NVS_SPARE_BLOCKS; i++) {
		if (nvsSpareBlocks[i] == NVS_NO_BLOCK)
			(*missing)++;
	}
	return 0;
}
#endif

/* the state and the blocks, to the trace (no one calls it; from 0x01000104 its prints are
   left out) */
void nvsPrintStatus(void)
{
#if !FW_SINCE(0x01000104)
	int i;
#if FW_SINCE(0x00090804)
	struct NvsHeader header;
#endif

#if FW_SINCE(0x00090804)
	NVS_HEADER_COPY(header, ((struct NvsBlock *)NVS_ADDRESS(nvsDataBlocks[0]))->header);
#endif
	tracePrintf("NVS Status\n");
	tracePrintf(" status = %02X\n", nvsState);
#if FW_SINCE(0x00090804)
	tracePrintf(" ver=%02X movingCount=%d\n", header.mark, 0xffff - header.generation);
#endif
	for (i = 0; i < NVS_DATA_BLOCKS; i++)
		tracePrintf(" basePhy   [%d] = %d\n", i, nvsDataBlocks[i]);
	for (i = 0; i < NVS_DATA_BLOCKS; i++)
		tracePrintf(" basersvPhy[%d] = %d\n", i, nvsCopyBlocks[i]);
	for (i = 0; i < NVS_SPARE_BLOCKS; i++)
		tracePrintf(" modifyPhy [%d] = %d\n", i, nvsSpareBlocks[i]);
#endif
}

/* marks a block as data (the first block of the data, or another) and writes its header */
#if FW_SINCE(0x01000306)
unsigned char nvsMarkData(unsigned char block, unsigned char first)
{
	unsigned char result;
	union {
		unsigned long	status;
		struct NvsHeader header;
	} value;
	struct NvsBlock *flashBlock;

	flashBlock = (struct NvsBlock *)NVS_ADDRESS(block);
	value.status = first ? 0x55555500 : 0x55555555;
	result = nvsFlashWrite((unsigned char *)&flashBlock->mark, &value, 1, 0);
	if (result == 0) {
		value.status = 0xffffffff;
		value.header.mark = 2;
		nvsFlashWrite((unsigned char *)&flashBlock->header, &value, 1, 0);
	}
	return result;
}
#elif FW_SINCE(0x00090804)
unsigned char nvsMarkData(unsigned char block, unsigned char first)
{
	unsigned char result;
	union {
		unsigned long	status;
		struct NvsHeader header;
	} value;
	struct NvsBlock *flashBlock;

	flashBlock = (struct NvsBlock *)NVS_ADDRESS(block);
	value.status = first ? 0x55555500 : 0x55555555;
	result = nvsFlashWrite((unsigned char *)&flashBlock->mark, &value, 1, 0);
	value.status = 0xffffffff;
	value.header.mark = 1;
	nvsFlashWrite((unsigned char *)&flashBlock->header, &value, 1, 0);
	return result;
}
#else
/* 0x00090206 writes no header */
unsigned char nvsMarkData(unsigned char block, unsigned char first)
{
	unsigned char result;
	unsigned long status;
	struct NvsMark *mark;

	mark = (struct NvsMark *)NVS_ADDRESS(block);
	status = first ? 0x55555500 : 0x55555555;
	result = nvsFlashWrite((unsigned char *)&mark->status, &status, 1, 0);
	return result;
}
#endif

/* marks an erased block as a spare */
unsigned char nvsMarkSpare(unsigned char block, unsigned char first)
{
	unsigned char result;
	unsigned long status;
	struct NvsMark *mark;

	mark = (struct NvsMark *)NVS_ADDRESS(block);
	status = 0xaaaaaaaa;
	result = nvsFlashWrite((unsigned char *)&mark->status, &status, 1, 0);
	return result;
}

#if FW_SINCE(0x01000306)
/* writes the second word of a block's mark */
unsigned char nvsSetMark(unsigned char block, unsigned long extra)
{
	unsigned char result;
	struct NvsMark *mark;

	if (block < NVS_FIRST_BLOCK)
		return 0x23;
	mark = (struct NvsMark *)NVS_ADDRESS(block);
	result = nvsFlashWrite((unsigned char *)&mark->extra, &extra, 1, 0);
	return result;
}
#endif

#if FW_SINCE(0x00090804)
/* clears a block's mark: the block is bad */
unsigned char nvsMarkBad(unsigned char block)
{
	unsigned char result;
	unsigned long value;
	struct NvsMark *mark;

	mark = (struct NvsMark *)NVS_ADDRESS(block);
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
#endif

#if FW_SINCE(0x00090804)
/* takes the blocks after the last data block that hold no data as the copy blocks and spares */
unsigned char nvsAssignBlocks(void)
{
	unsigned char i;
	unsigned char n;
	unsigned char block;

	block = nvsDataBlocks[NVS_DATA_BLOCKS - 1];
	memset(nvsCopyBlocks, NVS_NO_BLOCK, sizeof(nvsCopyBlocks));
	memset(nvsSpareBlocks, NVS_NO_BLOCK, sizeof(nvsSpareBlocks));
	for (i = 0, n = 0; i < NVS_BLOCKS - NVS_DATA_BLOCKS; i++) {
		block = ((block - NVS_FIRST_BLOCK + 1) & 0x0f) + NVS_FIRST_BLOCK;
		if (nvsBlockRole(block) != 0)
			continue;
		if (((struct NvsMark *)NVS_ADDRESS(block))->extra == 0)
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
#endif

#if FW_SINCE(0x00090804)
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
#endif

/* writes a record into its place in a block */
unsigned char nvsWriteBlockRecord(unsigned char *record, unsigned int index, unsigned char block)
{
	unsigned char *address;
	struct NvsBlock *flashBlock;
	unsigned char result;

	flashBlock = (struct NvsBlock *)NVS_ADDRESS(block);
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
#if BOARD_DOL1002
	if (index >= NVS_HIDDEN_FIRST && index < NVS_HIDDEN_END) {
		memset(record, 0xff, NVS_RECORD_SIZE);
		return 0;
	}
	if (index >= NVS_MOVED_FIRST && index < NVS_MOVED_END)
		index -= NVS_MOVED_SHIFT;
#endif
	while (i-- != 0) {
		block = nvsSpareBlocks[i];
		if (block == NVS_NO_BLOCK)
			continue;
		spare = (struct NvsSpareBlock *)NVS_ADDRESS(block);
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
	flashBlock = (struct NvsBlock *)NVS_ADDRESS(block);
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
#if BOARD_DOL1002
	if (index >= NVS_HIDDEN_FIRST && index < NVS_HIDDEN_END)
		return 0x32;
	if (index >= NVS_MOVED_FIRST && index < NVS_MOVED_END)
		index -= NVS_MOVED_SHIFT;
#endif
	if (index > NVS_DATA_BLOCKS * NVS_RECORDS)
		return 2;
#if FW_SINCE(0x01000306)
	nvsState = 2;
#endif
	for (i = 0; i < NVS_SPARE_BLOCKS; i++) {
		block = nvsSpareBlocks[i];
		if (block == NVS_NO_BLOCK)
			continue;
		spare = (struct NvsSpareBlock *)NVS_ADDRESS(block);
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
#if FW_SINCE(0x01000306)
			nvsState = 1;
#endif
			return 0;
		}
	}
	nvsState = NVS_STATE_FULL;
	return 7;
}
#endif	/* !BOARD_USS1001 */

#if BOARD_USS1001
unsigned char nvsInit(void)
{
	unsigned char result;

	result = 0;
	nvs.state = 0;
	result = nvsMount(0);
	if (result == 8)
		result = nvsClean(3);
	return result;
}

static unsigned char nvsReadRecords(unsigned char *buffer, unsigned int index,
				    unsigned char count)
{
	unsigned char result;

	result = 0;
	switch (nvs.state) {
	case 0xf0:
		return 8;
	case 2:
	case NVS_STATE_COLLECTING:
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
#if !SOURCE_0104	/* USS-1002's nvsGetRecord checks the index */
	if (index >= NVS_RECORDS)
		return 6;
#endif
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
	switch (nvs.state) {
	case 3:
		return 7;
	case 0x80:
		return 0x21;
	case 0xf0:
		return 8;
	case 2:
	case NVS_STATE_COLLECTING:
		return 0x22;
	case 0:
		return 9;
	case 1:
		break;
	default:
		return 9;
	}
#if !SOURCE_0104	/* USS-1002's nvsPutRecord checks the index */
	if (index >= NVS_RECORDS)
		return 6;
#endif
	if (nvs.freeList.count < NVS_FREE_MIN)
		nvsCollect(NVS_FREE_MIN - nvs.freeList.count);
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

	state = nvs.state;
	return state;
}

/* erases the NVS's blocks; the first good one is the first data block */
unsigned char nvsFormat(void)
{
	unsigned char used;
	unsigned char result;
	unsigned char retry;
	unsigned char block;

	nvs.state = 0;
	memset(nvs.blocks, 0, sizeof(nvs.blocks));
	memset(&nvs.dataList, 0, sizeof(nvs.dataList));
	memset(&nvs.freeList, 0, sizeof(nvs.freeList));
	retry = 3;
	used = 0;
	block = 0;
	while (block < NVS_BLOCKS) {
		result = nvsErase(block);
		switch (result) {
		case 0:
			if (used == 0 && nvsMarkData(block) != 0 && retry-- != 0)
				continue;
			used++;
			break;
		case 0x1a:
			nvsMarkBad(block);
			break;
		default:
			if (retry-- != 0)
				continue;
			break;
		}
		block++;
		retry = 3;
	}
	return nvsMount(0);
}

/* nothing is moved: nvsWriteRecords collects as it goes */
unsigned char nvsMove(void)
{
	return 0x0a;
}

/* copies the live records of the oldest data block to the newer ones and retires it, count
   times */
unsigned char nvsCollect(unsigned char count)
{
#if !SOURCE_0104	/* counted, never read: USS-1002 drops it */
	unsigned char moved;
#endif
	unsigned char j;
	unsigned char i;
	unsigned char k;
	unsigned char result;
	unsigned char head[3];
	struct NvsEntry *entry;
	struct NvsEntry *slot;
	struct NvsBlock *from;
	struct NvsBlock *to;
	struct NvsBlockInfo *oldest;
	struct NvsBlockInfo *next;

	if (nvs.dataList.count < NVS_COLLECT_MIN)
		return 0;
	nvs.state = NVS_STATE_COLLECTING;
	entry = &nvs.entry;
#if !SOURCE_0104
	moved = 0;
#endif
	for (; count != 0; count--) {
		oldest = nvs.dataList.head;
		if (oldest == NULL) {
			nvs.state = 0;
			return 9;
		}
		memset(nvs.moved, 0xff, sizeof(nvs.moved));
		from = NVS_BLOCK(oldest->block);
		j = NVS_ENTRIES;
		while (j-- != 0) {
			result = flashRead(entry, (unsigned char *)&from->entries[j],
					   sizeof(struct NvsEntry));
			if (result != 0)
				continue;
			for (k = j; k < NVS_ENTRIES; k++) {
				if (nvs.moved[k] == entry->index)
					goto seen;
			}
			for (next = oldest->next; next != NULL; ) {
				to = NVS_BLOCK(next->block);
				for (i = 0; i < NVS_ENTRIES; i++) {
					slot = &to->entries[i];
					result = flashRead(head, (unsigned char *)slot, 3);
					if (result != 0)
						continue;
					if (entry->index == head[0] && head[1] == NVS_ENTRY_VALID &&
					    head[2] == 0xff)
						goto seen;
					if (head[0] == 0xff && head[1] == 0xff && head[2] == 0xff) {
						result = flashWrite(&slot->index, &entry->index, 1, 1);
						if (result != 0)
							continue;
						result = flashWrite(&slot->data[0], &entry->data[0],
								    NVS_RECORD_SIZE, 1);
						if (result != 0)
							continue;
						result = flashWrite(&slot->valid, &entry->valid, 1, 1);
						if (result != 0)
							continue;
#if !SOURCE_0104
						moved++;
#endif
						goto seen;
					}
				}
				next = next->next;
				if (next == NULL) {
					next = nvsAllocBlock();
					if (next == NULL) {
						nvs.state = 3;
						return 7;
					}
				}
			}
seen:
			nvs.moved[j] = entry->index;
		}
		nvsRetire(oldest->block);
	}
	nvs.state = 1;
	return 0;
}

/* erases up to count retired blocks, collecting first while too few are free */
unsigned char nvsClean(unsigned char count)
{
	struct NvsBlockInfo *info;
	unsigned char need;
	unsigned char result;
	unsigned char retry;

	result = 0;
	switch (nvs.state) {
	case 0xf0:
		return 8;
	case 2:
		return 0x22;
	case 0:
	default:
		return 9;
	case 1:
	case 3:
		break;
	}
	if (nvs.freeList.count < NVS_FREE_MIN) {
		need = NVS_FREE_MIN - nvs.freeList.count;
		result = nvsCollect(need);
		if (result != 0) {
			/* cleaned all the same */
		}
	}
#if FW_SINCE(0x01030511)
	nvs.state = 2;
#endif
	retry = 3;
	info = nvs.freeList.head;
	while (info != NULL && count != 0) {
		if (info->role == NVS_BLOCK_USED) {
			result = nvsErase(info->block);
			if (result == 0) {
				count--;
				info->role = NVS_BLOCK_ERASED;
			} else if (result == 0x1a) {
				count--;
				result = nvsMarkBad(info->block);
			} else if (retry-- != 0) {
				continue;
			}
		}
		info = info->next;
		retry = 3;
	}
	if (nvs.freeList.count != 0)
		nvs.state = 1;
#if FW_SINCE(0x01030511)
	else
		nvs.state = 3;
#endif
#if NVS_FLASH_CLOSE
	flashClose();
#endif
	return result;
}

/* reads the blocks' headers into their lists, the data blocks from the oldest */
unsigned char nvsMount(unsigned char mode)
{
	unsigned char i;
	unsigned char result;
	unsigned long oldest;
	unsigned char first;
	struct NvsBlockInfo *info;
	struct NvsBlock *block;
	struct NvsHeader *header;

	nvs.state = 0;
	header = &nvs.header;
	memset(nvs.blocks, 0, sizeof(nvs.blocks));
	memset(&nvs.dataList, 0, sizeof(nvs.dataList));
	memset(&nvs.freeList, 0, sizeof(nvs.freeList));
	memset(&nvs.badList, 0, sizeof(nvs.badList));
	oldest = 0xffffffff;
#if !SOURCE_0104	/* USS-1002 leaves it to the first data block */
	first = 0xff;
#endif
	for (i = 0; i < NVS_BLOCKS; i++) {
		info = &nvs.blocks[i];
		block = NVS_BLOCK(i);
		info->block = i;
		result = flashRead(header, (unsigned char *)&block->header, sizeof(struct NvsHeader));
		if (result != 0)
			continue;
		switch (header->status.value) {
		case NVS_STATUS_DATA:
			info->sequence = header->sequence;
			info->role = NVS_BLOCK_DATA;
			if (header->sequence < oldest) {
				oldest = header->sequence;
				first = i;
			}
			break;
		case NVS_STATUS_USED:
			info->sequence = header->sequence;
			info->role = NVS_BLOCK_USED;
			break;
		case NVS_STATUS_ERASED:
			info->role = NVS_BLOCK_ERASED;
			break;
		default:
#if FW_SINCE(0x01030511)
			if (header->status.bytes[0] == 0xff && header->status.bytes[3] == 0xff)
				info->role = NVS_BLOCK_USED;
			else
				info->role = NVS_BLOCK_BAD;
#else
			info->role = NVS_BLOCK_BAD;	/* 0x01030505: any other status */
#endif
			break;
		}
	}
#if NVS_FLASH_CLOSE
	flashClose();
#endif
	if (oldest == 0xffffffff) {
		nvs.state = 0xf0;
		return 8;
	}
	for (i = 0; i < NVS_BLOCKS; i++) {
		info = &nvs.blocks[(first + i) & (NVS_BLOCKS - 1)];
		switch (info->role) {
		case NVS_BLOCK_ERASED:
		case NVS_BLOCK_USED:
			nvsListAppend(&nvs.freeList, info);
			break;
		case NVS_BLOCK_DATA:
			nvsListAppend(&nvs.dataList, info);
			break;
		case NVS_BLOCK_BAD:
		default:
			nvsListAppend(&nvs.badList, info);
			break;
		}
	}
	nvs.state = 1;
	return 0;
}

#if NVS_INFO_MAP
/* the newest block's mark and sequence (moves) and, into map, which records are written */
unsigned char nvsGetInfo(unsigned char *mark, unsigned char *missing, unsigned long *moves,
			 struct NvsMap *map)
{
	struct NvsBlockInfo *info;
	struct NvsBlock *block;
	struct NvsHeader *header;
	struct NvsEntry *entry;
	unsigned char *bits;
	unsigned char i;
	unsigned char slot;
	unsigned char result;
	unsigned char state;

	switch (nvs.state) {
	case 2:
	case NVS_STATE_COLLECTING:
		return 0x22;
	case 0:
	default:
		return 9;
	case 1:
	case 3:
		break;
	}
	header = &nvs.header;
	entry = &nvs.entry;
	bits = nvs.moved;
	info = nvs.dataList.tail;
	if (info == NULL) {
		*mark = 0;
		*missing = 0;
		*moves = 0;
		return 8;
	}
	state = nvs.state;
	nvs.state = 2;
	*missing = nvs.badList.count;
	block = NVS_BLOCK(info->block);
	result = flashRead(header, (unsigned char *)&block->header, sizeof(struct NvsHeader));
	if (result == 0) {
		*mark = header->mark;
		*moves = header->sequence / 8;
		if (map != NULL) {
			memset(map, 0, sizeof(*map));
			memset(bits, 0, sizeof(map->bits));
			map->size = sizeof(map->bits);
			for (; info != NULL; info = info->prev) {
				block = NVS_BLOCK(info->block);
				for (slot = 0; slot < NVS_ENTRIES; slot++) {
					result = flashRead(entry, (unsigned char *)&block->entries[slot], 3);
					if (result != 0)
						continue;
					if (entry->index < NVS_RECORDS && entry->valid == NVS_ENTRY_VALID &&
					    entry->erased == 0xff)
						bits[entry->index >> 3] |= (unsigned char)0x80 >> (entry->index & 7);
				}
			}
			for (i = 0, map->count = 0; i < NVS_RECORDS; i++) {
				if ((bits[i >> 3] & 0x80 >> (i & 7)) != 0)
					map->count++;
			}
			memcpy(map->bits, bits, sizeof(map->bits));
		}
	}
	nvs.state = state;
#if NVS_FLASH_CLOSE
	flashClose();
#endif
	return result;
}
#else
/* 0x01030505: the newest block's mark and sequence (moves), the bad blocks (missing) */
unsigned char nvsGetInfo(unsigned char *mark, unsigned char *missing, unsigned int *moves)
{
	struct NvsBlockInfo *info;
	struct NvsBlock *block;
	struct NvsHeader *header;
	unsigned char result;

	info = nvs.dataList.tail;
	if (info == NULL) {
		*mark = 0;
		*missing = 0;
		*moves = 0;
		return 8;
	}
	header = &nvs.header;
	block = NVS_BLOCK(info->block);
	result = flashRead(header, (unsigned char *)&block->header, sizeof(struct NvsHeader));
	if (result == 0) {
		*mark = header->mark;
		*missing = nvs.badList.count;
		*moves = (unsigned int)(header->sequence / 8 & 0xffff);
	}
	return result;
}
#endif

/* the state and the blocks, to the trace: their prints are left out */
void nvsPrintStatus(void)
{
}

void nvsPrintBlock(unsigned int block)
{
}

/* erases a block and makes it the newest data block */
#if !FW_SINCE(0x01030511)
/* 0x01030505: the block erased (three tries), then its whole header written at once */
unsigned char nvsMarkData(unsigned char block)
{
	unsigned char result;
	unsigned char retry;
	struct NvsBlockInfo *info;
	struct NvsBlock *flashBlock;
	struct NvsHeader *header;
	unsigned long sequence;

	header = &nvs.header;
	info = &nvs.blocks[block];
	flashBlock = NVS_BLOCK(block);
	retry = 3;
	while (retry-- != 0) {
		result = nvsErase(block);
		if (result == 0)
			break;
		if (result == 0x1a)
			return result;
	}
	if (nvs.dataList.tail != NULL) {
		sequence = nvs.dataList.tail->sequence + 1;
		if (sequence == 0)
			return 0x21;
	} else {
		sequence = 0;
	}
	memset(header, 0xff, sizeof(*header));
	header->status.value = NVS_STATUS_DATA;
	header->sequence = sequence;
	header->mark = NVS_HEADER_MARK;
	result = flashWrite((unsigned char *)&flashBlock->header, header, sizeof(*header), 1);
	if (result == 0) {
		info->role = NVS_BLOCK_DATA;
		info->sequence = sequence;
		info->block = block;
		nvsListAppend(&nvs.dataList, info);
	}
	return result;
}
#else
unsigned char nvsMarkData(unsigned char block)
{
	unsigned char result;
	unsigned char retry;
	struct NvsBlockInfo *info;
	struct NvsBlock *flashBlock;
	struct NvsHeader *header;
	unsigned long sequence;

	header = &nvs.header;
	info = &nvs.blocks[block];
	flashBlock = NVS_BLOCK(block);
	retry = 3;
	while (retry-- != 0) {
		result = nvsErase(block);
		if (result == 0x1a)
			return result;
		if (result != 0)
			continue;
		if (nvs.dataList.tail != NULL) {
			sequence = nvs.dataList.tail->sequence + 1;
			if (sequence == 0)
				return 0x21;
		} else {
			sequence = 0;
		}
		memset(header, 0xff, sizeof(*header));
		header->status.value = NVS_STATUS_DATA;
		header->sequence = sequence;
		header->mark = NVS_HEADER_MARK;
		result = flashWrite((unsigned char *)&flashBlock->header.sequence, &header->sequence,
				    sizeof(*header) - 4, 1);
		if (result != 0)
			continue;
		result = flashWrite(&flashBlock->header.status.bytes[0], &header->status, 4, 1);
		if (result != 0)
			continue;
		info->role = NVS_BLOCK_DATA;
		info->sequence = sequence;
		info->block = block;
		nvsListAppend(&nvs.dataList, info);
		return result;
	}
	return result;
}
#endif

/* marks a free block bad (its header's bytes 0 and 3 cleared) */
unsigned char nvsMarkBad(unsigned char block)
{
	unsigned char result;
	struct NvsBlockInfo *info;
	struct NvsBlock *flashBlock;
	struct NvsHeader *header;

	header = &nvs.header;
	info = &nvs.blocks[block];
	flashBlock = NVS_BLOCK(block);
	memset(header, 0xff, sizeof(*header));
	header->status.bytes[0] = 0;
	header->status.bytes[3] = 0;
	result = flashWrite(&flashBlock->header.status.bytes[0], &header->status.bytes[0], 1, 0);
	result = flashWrite(&flashBlock->header.status.bytes[3], &header->status.bytes[3], 1, 0);
	info->role = NVS_BLOCK_BAD;
	nvsListRemove(&nvs.freeList, info);
	nvsListAppend(&nvs.badList, info);
	return 0;
}

/* retires a data block (its header's byte 2 written), to be erased */
unsigned char nvsRetire(unsigned char block)
{
	unsigned char result;
	struct NvsBlockInfo *info;
	struct NvsBlock *flashBlock;
	struct NvsHeader *header;

	header = &nvs.header;
	info = &nvs.blocks[block];
	flashBlock = NVS_BLOCK(block);
	memset(header, 0xff, sizeof(*header));
	header->status.bytes[2] = 0x55;
	result = flashWrite(&flashBlock->header.status.bytes[2], &header->status.bytes[2], 1, 0);
	info->role = NVS_BLOCK_USED;
	nvsListRemove(&nvs.dataList, info);
	nvsListAppend(&nvs.freeList, info);
	return 0;
}

/* a free block made the newest data block, NULL when none is left */
struct NvsBlockInfo *nvsAllocBlock(void)
{
	struct NvsBlockInfo *info;

	while (nvs.freeList.count != 0) {
		info = nvsListPopFront(&nvs.freeList);
		if (info == NULL)
			continue;
		if (nvsMarkData(info->block) == 0)
			return info;
		nvsMarkBad(info->block);
	}
	return NULL;
}

/* a record from its newest entry, all ones when none was written */
#if SOURCE_0104
/* USS-1002's: the remapped records, read into nvs.entry */
unsigned char nvsGetRecord(unsigned char *record, unsigned int index)
{
	struct NvsBlock *flashBlock;
	struct NvsBlockInfo *info;
	struct NvsEntry *entry;
	unsigned char slot;
	unsigned char result;

	if (index >= NVS_BLANK_FIRST && index < NVS_MOVED_FIRST) {
		memset(record, 0xff, NVS_RECORD_SIZE);
		return 0;
	}
	if (index >= NVS_REMAP_FIRST && index < NVS_REMAP_END)
		index -= NVS_REMAP_FIRST - NVS_BLANK_FIRST;
	if (index >= NVS_RECORDS)
		return 6;
	nvs.state = 2;
	info = nvs.dataList.tail;
	entry = &nvs.entry;
	for (; info != NULL; info = info->prev) {
		flashBlock = NVS_BLOCK(info->block);
		if (info->role != NVS_BLOCK_DATA)
			continue;
		slot = NVS_ENTRIES;
		while (slot-- != 0) {
			result = flashRead(entry, (unsigned char *)&flashBlock->entries[slot], 3);
			if (result != 0)
				continue;
			if (entry->index == (index & 0xff) && entry->valid == NVS_ENTRY_VALID &&
			    entry->erased == 0xff) {
				result = flashRead(record, &flashBlock->entries[slot].data[0],
						   NVS_RECORD_SIZE);
				nvs.state = 1;
				return result;
			}
		}
	}
	memset(record, 0xff, NVS_RECORD_SIZE);
	nvs.state = 1;
	return 0;
}
#else
unsigned char nvsGetRecord(unsigned char *record, unsigned int index)
{
	struct NvsBlock *flashBlock;
	struct NvsBlockInfo *info;
	struct NvsEntry *flashEntry;
	struct NvsEntry entry;
	unsigned char slot;
	unsigned char result;

	nvs.state = 2;
	for (info = nvs.dataList.tail; info != NULL; info = info->prev) {
		flashBlock = NVS_BLOCK(info->block);
		if (info->role != NVS_BLOCK_DATA)
			continue;
		slot = NVS_ENTRIES;
		while (slot-- != 0) {
			flashEntry = &flashBlock->entries[slot];
			result = flashRead(&entry, (unsigned char *)flashEntry, 3);
			if (result != 0)
				continue;
			if (entry.index == (index & 0xff) && entry.valid == NVS_ENTRY_VALID &&
			    entry.erased == 0xff) {
				result = flashRead(record, &flashEntry->data[0], NVS_RECORD_SIZE);
				nvs.state = 1;
				return result;
			}
		}
	}
	memset(record, 0xff, NVS_RECORD_SIZE);
	nvs.state = 1;
	return 0;
}
#endif

/* a record to the first free entry of the newest block, a new block when it is full */
unsigned char nvsPutRecord(unsigned char *record, unsigned int index)
{
	struct NvsBlock *flashBlock;
	struct NvsBlockInfo *info;
	struct NvsEntry *flashEntry;
	struct NvsEntry *entry;
	unsigned char slot;
	unsigned char result;

	result = 7;
#if SOURCE_0104
	if (index >= NVS_BLANK_FIRST && index < NVS_MOVED_FIRST)
		return 0x32;
	if (index >= NVS_REMAP_FIRST && index < NVS_REMAP_END)
		index -= NVS_REMAP_FIRST - NVS_BLANK_FIRST;
	if (index >= NVS_RECORDS)
		return 6;
#endif
	nvs.state = 2;
	info = nvs.dataList.tail;
	entry = &nvs.entry;
	while (info != NULL) {
		flashBlock = NVS_BLOCK(info->block);
		if (info->role == NVS_BLOCK_DATA) {
			for (slot = 0; slot < NVS_ENTRIES; slot++) {
				flashEntry = &flashBlock->entries[slot];
				result = flashRead(entry, (unsigned char *)flashEntry, 3);
				if (result != 0)
					continue;
				if (entry->index == 0xff && entry->valid == 0xff && entry->erased == 0xff) {
					entry->index = (unsigned char)(index & 0xff);
					memcpy(entry->data, record, NVS_RECORD_SIZE);
					entry->valid = NVS_ENTRY_VALID;
					entry->end = 0xff;
					result = flashWrite(&flashEntry->index, &entry->index, 1, 1);
					if (result != 0)
						continue;
					result = flashWrite(&flashEntry->data[0], &entry->data[0],
							    NVS_RECORD_SIZE, 1);
					if (result != 0)
						continue;
					result = flashWrite(&flashEntry->valid, &entry->valid, 1, 1);
					if (result != 0)
						continue;
					nvs.state = 1;
					return 0;
				}
			}
		}
		info = info->next;
		if (info == NULL)
			info = nvsAllocBlock();
	}
	nvs.state = 3;
	return 7;
}

unsigned char nvsListPushFront(struct NvsList *list, struct NvsBlockInfo *info)
{
	struct NvsBlockInfo *head;

	if (NVS_LIST_NULL(list, info))
		return 1;
	head = list->head;
	list->head = info;
	info->prev = NULL;
	info->next = head;
	if (head != NULL)
		head->prev = info;
	if (list->tail == NULL)
		list->tail = info;
	list->count++;
	return 0;
}

unsigned char nvsListAppend(struct NvsList *list, struct NvsBlockInfo *info)
{
	struct NvsBlockInfo *tail;

	if (NVS_LIST_NULL(list, info))
		return 1;
	tail = list->tail;
	if (tail != NULL)
		tail->next = info;
	info->prev = tail;
	info->next = NULL;
	list->tail = info;
	if (list->head == NULL)
		list->head = info;
	list->count++;
	return 0;
}

unsigned char nvsListRemove(struct NvsList *list, struct NvsBlockInfo *info)
{
	struct NvsBlockInfo *p;

	if (NVS_LIST_NULL(list, info))
		return 1;
	for (p = list->head; p != NULL; p = p->next) {
		if (p != info)
			continue;
		if (p->prev != NULL)
			p->prev->next = p->next;
		else
			list->head = p->next;
		if (p->next != NULL)
			p->next->prev = p->prev;
		else
			list->tail = p->prev;
		list->count--;
		return 0;
	}
	return 2;
}

struct NvsBlockInfo *nvsListPopFront(struct NvsList *list)
{
	struct NvsBlockInfo *info;

	if (list->head != NULL) {
		info = list->head;
		nvsListRemove(list, info);
		return info;
	}
	return NULL;
}
#endif	/* BOARD_USS1001 */
