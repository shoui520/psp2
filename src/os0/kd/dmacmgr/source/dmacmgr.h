/*
 * SceDmacMgr: DMA operations, their tag chains and the controller queues.
 *
 * The register layouts are the henkaku wiki's (DMAC page); the operation and
 * tag records are what dmacmgr.c reads and writes.
 */

#pragma once

#include <psp2kern/types.h>

/* vitasdk's prototype takes the timeout by address. */
#define ksceKernelDmaOpSync		ksceKernelDmaOpSync_vitasdk
#include <sce/dmacmgr.h>
#include <sce/dmac_regs.h>
#undef ksceKernelDmaOpSync

/* ===== registers ===== */

/* one channel's registers, 0x80 bytes */
typedef struct SceDmacChannelReg {
	SceUInt32 src;
	SceUInt32 dst;
	SceSize len;
	SceUInt32 cmd;
	SceUInt32 srcKeyringId;
	SceUInt32 iv;
	SceSize blockSize;
	SceUInt32 ctrl;				/* 0x1c: 0 stops the channel */
	SceUInt32 chainStart;		/* 0x20: the first tag of a chain */
	SceUInt32 stat;				/* 0x24: SCE_KERNEL_DMAC_STAT_* */
	SceUInt32 intrStatus;			/* 0x28: DMAC_INTR_*, written back to clear */
	SceUInt32 coherencyMask;
	SceUInt32 ivCoherencyMask;
	SceUInt32 setValue;			/* 0x34: for SCE_KERNEL_DMAC_CMD_OP_SET */
	SceUInt32 reserved0;
	SceUInt32 trngValue;
	SceUInt32 errorSrc;
	SceUInt32 errorDst;
	SceSize errorLen;			/* 0x48: what the failing tag left undone */
	SceUInt32 currentTag;		/* 0x4c */
	SceUInt32 reserved1[12];
} SceDmacChannelReg;

#define DMAC_INTR_COMPLETE	0x00000001
#define DMAC_INTR_ERROR		0x00000002

/* the command flag that makes a tag raise the completion interrupt */
#define DMAC_CMD_INTERRUPT	0x00001000

/* DMAC5 channel: its external key follows the other channel's registers */
typedef struct SceDmac5ChannelReg {
	SceDmacChannelReg reg;
	SceUInt8 reserved[0x180];
	SceUInt32 key[16];			/* 0x200 */
} SceDmac5ChannelReg;

/* DMAC4's interrupt gates, one per four channels */
typedef struct SceDmac4GateReg {
	SceUInt32 activeIntr;
	SceUInt32 intrMask;
} SceDmac4GateReg;

typedef struct SceDmac4Reg {
	SceDmacChannelReg ch[16];
	SceUInt32 reserved0[2];
	SceUInt8 reserved1[8];
	SceUInt32 busErrorAddress;
	SceUInt32 busErrorAttribute;
	SceUInt32 secureBusErrorAddress;
	SceUInt32 secureBusErrorAttribute;
	SceUInt8 reserved2[0x900 - 0x820];
	SceDmac4GateReg gate[4];		/* 0x900: interrupts 0x78..0x7b */
} SceDmac4Reg;

_Static_assert(sizeof(SceDmacChannelReg) == 0x80, "SceDmacChannelReg size");
_Static_assert(__builtin_offsetof(SceDmac5ChannelReg, key) == 0x200, "DMAC5 key");
_Static_assert(__builtin_offsetof(SceDmac4Reg, gate) == 0x900, "DMAC4 gates");

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
