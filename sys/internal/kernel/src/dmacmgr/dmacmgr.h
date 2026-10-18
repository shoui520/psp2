/*
 * SceDmacMgr: DMA operations, their tag chains and the controller queues.
 *
 * The register layouts are the henkaku wiki's (DMAC page); the operation and
 * tag records are what dmacmgr.c reads and writes.
 */

#pragma once

#include <scetypes.h>
#include <sysmem/spinlock.h>

#include <dmacmgr/dmacmgr.h>
#include <dmacmgr/dmac_regs.h>

/* ===== operations ===== */

/* a tag as the controller reads it, with what dmacmgr keeps about it */
typedef struct DmaTag {
	struct DmaTag *next;
	struct DmaTag *prev;
	const SceKernelDmaOpTag *param;		/* the caller's tag it was made from */
	SceSize offset;				/* bytes of the chain before this tag */
	uintptr_t pa;			/* of tag, bit 0 set */
	SceUInt32 cached;			/* 1: from the tag cache, 0: from the fixed heap */
	SceUInt32 reserved[2];
	SceKernelDmaOpTag tag;
} DmaTag;

struct DmaController;
struct DmaChannel;

/* SceKernelDmaOpId is the record's address shifted right by one, bit 0 set */
typedef struct DmaOp {
	struct DmaOp *prev;			/* in the controller's queue */
	struct DmaOp *next;
	DmaOpCallback callback;
	void *pUserData;
	struct DmaController *dmac;		/* sceKernelDmaOpAssign */
	struct DmaChannel *channel;		/* while transferring */
	DmaTag *head;				/* the chain still to transfer */
	DmaTag *tail;
	SceUInt32 coherencyMask;
	const SceKernelDmaOpTag *errorTag;	/* the caller's tag that failed */
	union {
		SceUInt32 setValue;
		const SceKernelDmaOpEncDec *encDec;	/* DMAC5 */
	};
	SceKernelSpinlock lock;
	SceUInt32 magic;			/* DMA_OP_MAGIC */
	SceUID evfId;				/* bit 0: transfer done */
	volatile SceUInt16 status;			/* DMA_OP_* */
	SceUInt16 dmacId;			/* SceKernelDmacId */
	const char *name;
} DmaOp;

#define DMA_OP_ALLOCATED	0x0001
#define DMA_OP_QUEUED		0x0008
#define DMA_OP_DONE		0x0020		/* transferred; set it up again */
#define DMA_OP_CANCELING	0x0040

_Static_assert(sizeof(DmaTag) == 0x40, "DmaTag size");
_Static_assert(sizeof(DmaOp) == 0x40, "DmaOp size");

/* ===== controllers ===== */

typedef struct DmaChannel {
	volatile SceDmacChannelReg *reg;
	DmaOp *op;
} DmaChannel;

/* bit 31 - n of the masks is channel n; the channels follow the record */
typedef struct DmaController {
	DmaOp *head;				/* queued operations */
	DmaOp *tail;
	SceKernelSpinlock lock;
	SceUInt32 freeChannels;
	SceUInt32 intrChannels;			/* the channels with an interrupt handler */
	DmaChannel channel[0];
} DmaController;

/* DMAC4: each channel is a controller of its own */
typedef struct Dmac4Channel {
	DmaController dmac;
	DmaChannel channel;
} Dmac4Channel;

typedef struct DmacMgr {
	volatile SceDmacChannelReg *dmac0Reg;
	volatile SceDmacChannelReg *dmac1Reg;
	volatile SceDmacChannelReg *dmac2Reg;
	volatile SceDmacChannelReg *dmac3Reg;
	volatile SceDmac4Reg *dmac4Reg;
	volatile SceDmacChannelReg *dmac5Reg;
	volatile SceDmacChannelReg *dmac6Reg;		/* NULL without DMAC6 */
	SceUInt32 reserved;
	struct FixedHeap *objectHeap;		/* operations and tags, 0x40 bytes each */
	SceUID rangeHeapId;			/* physical range lists */
	DmaTag *freeTag;			/* the tag cache's free list */
	DmaController dmac01;			/* DMAC0 and DMAC1 */
	DmaChannel dmac01Channel[4];
	DmaController dmac23;			/* DMAC2 and DMAC3 */
	DmaChannel dmac23Channel[4];
	Dmac4Channel dmac4[16];
	DmaController dmac5;
	DmaChannel dmac5Channel[2];
	DmaController dmac6;
	DmaChannel dmac6Channel[2];
} DmacMgr;

_Static_assert(__builtin_offsetof(DmacMgr, dmac01) == 0x2c, "DmacMgr dmac01");
_Static_assert(__builtin_offsetof(DmacMgr, dmac4) == 0x94, "DmacMgr dmac4");
_Static_assert(sizeof(DmacMgr) == 0x29c, "DmacMgr size");
