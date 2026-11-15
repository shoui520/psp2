/*
 * The Bluetooth controller's buffer pools in SQRAM (bt/lcpool.c, 0x43724 - 0x4381x): three pools of
 * blocks and the memory they are carved from. The objects are not converted yet; they stand at the
 * addresses of the 4.00 image. The roles are inferred.
 */
#ifndef ROBIN_BTPOOL_H
#define ROBIN_BTPOOL_H

#include <stdint.h>
#include "robin/buffer.h"

/* The pools' blocks: the table pool has three sizes, the packet and event pools one. */
#define BT_TABLE_POOL_CLASSES	3
#define BT_TABLE_POOL_BLOCKS	32	/* 12 + 8 + 12 */
#define BT_PACKET_POOL_BLOCKS	8
#define BT_EVENT_POOL_BLOCKS	4
#define BT_POOL_AREA_SIZE	0x3c70	/* the table pool's blocks, then the packet pool's */
#define BT_PACKET_POOL_OFFSET	0x1910	/* (0x4c + 4) * 12 + (0x9c + 4) * 8 + (0x158 + 4) * 12 */
#define BT_EVENT_POOL_SIZE	0x11a0	/* (0x464 + 4) * 4 */
#define BT_RECORD_BUFFERS	3
#define BT_RECORD_SIZE		752

/*
 * What btLmpLcResolveEntry() writes into the radio through btLmpLcSyncEntry(): the length in bytes,
 * the words, and the word that starts the radio on them. The pools take the memory over afterwards.
 */
#define BT_RADIO_TABLE_WORDS	128
struct BtRadioTable {
	uint16_t length;
	uint16_t words[BT_RADIO_TABLE_WORDS];
	uint16_t start;
};

/* SQRAM 0xc0001154: the event pool's blocks, holding the radio's table at 0xc000 until then. */
union BtPoolFrameMemory {
	uint8_t bytes[BT_EVENT_POOL_SIZE];
	struct BtRadioTable radioTable;
};

extern uint8_t btPoolHeaderMemory[BT_POOL_AREA_SIZE];	/* SQRAM 0xc00022f4: the table pool's blocks, then the packet pool's */
#define btPoolDescriptorMemory	(&btPoolHeaderMemory[BT_PACKET_POOL_OFFSET])	/* SQRAM 0xc0003c04 */
extern union BtPoolFrameMemory btPoolFrameMemory;
extern struct OsaBufferPool btTablePool;		/* SQRAM 0xc0001118 */
extern struct OsaPoolConfig btTablePoolConfig[BT_TABLE_POOL_CLASSES];	/* SQRAM 0xc0001124 */
extern uint8_t btTablePoolControl[4 * BT_TABLE_POOL_BLOCKS];		/* SQRAM 0xc0005f64 */
extern struct OsaBufferPool btPacketPool;		/* SQRAM 0xc000113c */
extern struct OsaPoolConfig btPacketPoolConfig;			/* SQRAM 0xc0001108 */
extern uint8_t btPacketPoolControl[4 * BT_PACKET_POOL_BLOCKS];	/* SQRAM 0xc0005fe4 */
extern struct OsaBufferPool btEventPool;		/* SQRAM 0xc0001148 */
extern struct OsaPoolConfig btEventPoolConfig;			/* SQRAM 0xc0001110 */
extern uint8_t btEventPoolControl[4 * BT_EVENT_POOL_BLOCKS];		/* SQRAM 0xc0006004 */
extern uint32_t btEventPoolFailures;			/* SQRAM 0xc0001100: allocations the event pool refused */
extern uint32_t btPacketPoolFailures;			/* SQRAM 0xc0001104 */
extern uint8_t btRecordBuffers[BT_RECORD_BUFFERS][BT_RECORD_SIZE];	/* SQRAM 0xc0006014 */
extern uint8_t btContextBuffers[BT_RECORD_BUFFERS][BT_RECORD_SIZE];	/* SQRAM 0xc00068e4 */
extern int (*btLimitParam)(uint8_t *packet);		/* 0x400f154 */

/* The hooks btLmpLcQueryEntry() installs in the ROM (0x400eb88 - 0x400eb9x, ROM data). */
struct BtEntryHooks {
	uint8_t *(*config)(void);		/* 0x400eb88: an event buffer */
	int (*release)(uint8_t *packet);	/* 0x400eb8c: frees an event buffer */
	uint8_t *(*control)(void);		/* 0x400eb90: a packet buffer */
	int (*acquire)(uint8_t *packet);	/* 0x400eb94: frees a packet buffer */
	void (*node)(void);			/* 0x400eb98 */
	uint8_t *(*slot)(int index);		/* 0x400eb9c */
	uint8_t *(*window)(int index);		/* 0x400eba0 */
	uint8_t *(*counter)(int index);		/* 0x400eba4 */
	uint8_t *(*bitmap)(int index);		/* 0x400eba8 */
	uint8_t *(*rate)(int index);		/* 0x400ebac */
	uint8_t *(*key)(int index);		/* 0x400ebb0 */
};
extern struct BtEntryHooks btEntryHooks;

void btLmpLcFilterContext(void);			/* 0x4377a */
uint8_t *btLmpLcHandleEntry(void);			/* 0x4377c */
int btLmpLcProcessEntry(uint8_t *packet);		/* 0x437b2 */
uint8_t *btLmpLcUpdateEntry(void);			/* 0x437b8 */
int btLmpLcConfigureEntry(uint8_t *packet);		/* 0x437e0 */
uint8_t *btLmpLcCheckEntry(int index);			/* 0x437ea */
int btLmpLcRecordContext(void);				/* 0x43724 */
uint8_t *btLmpLcValidateEntry(int index);		/* 0x43804 */
uint8_t *btLmpLcPrepareEntry(int index);		/* 0x43818 */
uint8_t *btLmpLcDispatchEntry(int index);		/* 0x43828 */
uint8_t *btLmpLcParseEntry(int index);			/* 0x43836 */
uint8_t *btLmpLcBuildEntry(int index);			/* 0x4384e */
uint8_t *btLmpLcComputeEntry(int index);		/* 0x43860 */
uint8_t *(*btLmpLcQueryEntry(void))(int index);		/* 0x4386c */
int btLmpLcSyncEntry(uint16_t reg, const uint16_t *data, uint32_t mask);	/* 0x43940 */

#endif
