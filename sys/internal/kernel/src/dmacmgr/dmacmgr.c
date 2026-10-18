/*
 * SceDmacMgr: DMA operations, tag chains, controller queues and callbacks.
 *
 * An operation (DmaOp) is assigned to a controller, given a chain of tags
 * and queued; the controller's interrupt hands each finished operation its
 * completion and starts the next queued one on the freed channel.
 */

#include <sysmem/sysmem.h>
#include <threadmgr/threadmgr.h>
#include <sysmem/cpu.h>
#include <sysmem/dipsw.h>
#include <psp2kern/kernel/sysclib.h>
#include <sysmem/barrier.h>
#include <sysmem/debug.h>
#include <kernel/error.h>
#include <excpmgr/excpmgr_cpu.h>
#include <intrmgr/intrmgr.h>
#include <sysmem/suspend.h>
#include <sysmem/sysroot.h>
#include "dmacmgr.h"

/* SceSysmem's fixed heaps (names from sysmem) */
struct FixedHeap;

/* the SHA command modes of DMAC5 (henkaku wiki) */
#define DMAC_CMD_HASH_MASK	0x00000c00
#define DMAC_CMD_HASH_DIGEST	0x00000000
#define DMAC_CMD_HASH_INIT	0x00000400
#define DMAC_CMD_HASH_RESULT	0x00000800
#define DMAC_CMD_HASH_UPDATE	0x00000c00

/* the length bits that select modulo addressing for the source and destination */
#define DMAC_LEN_MOD_SRC_MASK	0x0f000000
#define DMAC_LEN_MOD_DST_MASK	0xf0000000
#define DMAC_LEN_MASK		0x00ffffff

#define DMA_OP_MAGIC	((SceUInt32)&s_dmacMgr | 1)

/* a channel interrupt: DMAC4's gates have no channel of their own */
typedef struct DmacIntr {
	int intrCode;
	const char *name;
	DmaChannel *channel;
} DmacIntr;

static DmaTag s_tagCache[64] __attribute__((aligned(64)));
static DmacMgr s_dmacMgr;

int ReleaseDmaTagChain(DmaTag *first, DmaTag *last);
void StartQueuedDmaOperation(DmaController *dmac, DmaOp *op);

static inline DmaOp *getDmaOp(SceKernelDmaOpId opid)
{
	return (DmaOp *)(((SceUInt32)opid >> 1) * 4);
}

static inline SceKernelDmaOpId getDmaOpId(const DmaOp *op)
{
	return ((SceUInt32)op >> 1) | 1;
}

/* channel n's bit in the controller's masks is bit 31 - n */
static inline SceUInt32 channelBit(const DmaController *dmac, const DmaChannel *channel)
{
	return 1 << (&dmac->channel[31] - channel);
}

void _CancelDmaTransfer(DmaOp *op)
{
	DmaChannel *channel = op->channel;
	DmaController *dmac = op->dmac;
	volatile SceDmacChannelReg *reg;

	SCE_DBG_ASSERT(channel != NULL, 230);
	SCE_DBG_ASSERT(channel->op == op, 231);
	if (op->status & DMA_OP_CANCELING)
		return;
	reg = channel->reg;
	op->status |= DMA_OP_CANCELING;
	reg->ctrl = 0;
	dsb();
	while (reg->stat & SCE_KERNEL_DMAC_STAT_BUSY)
		;
	if (reg->intrStatus != 0)
		return;
	channel->op = NULL;
	op->channel = NULL;
	dmac->freeChannels |= channelBit(dmac, channel);
	op->status &= ~DMA_OP_CANCELING;
}

/* stop a DMAC4 channel and close its interrupt gate */
void ReleaseDmac4Channel(int index)
{
	Dmac4Channel *dmac4 = &s_dmacMgr.dmac4[index];
	volatile SceDmac4Reg *reg4 = s_dmacMgr.dmac4Reg;
	volatile SceDmacChannelReg *reg;
	SceKernelIntrStatus intr;
	SceUInt32 mask;

	intr = SceCpuForDriver_D32ACE9E(&dmac4->dmac.lock);
	if (!(dmac4->dmac.intrChannels & 0x80000000)) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&dmac4->dmac.lock, intr);
		return;
	}
	mask = ~(1 << index);
	reg = dmac4->channel.reg;
	reg4->gate[0].intrMask &= mask;
	reg4->gate[1].intrMask &= mask;
	reg4->gate[2].intrMask &= mask;
	reg4->gate[3].intrMask &= mask;
	reg->ctrl = 0;
	dsb();
	while (reg->stat & SCE_KERNEL_DMAC_STAT_BUSY)
		;
	reg->intrStatus = DMAC_INTR_COMPLETE | DMAC_INTR_ERROR;
	dsb();
	dmac4->channel.op = NULL;
	dmac4->dmac.freeChannels = 0;
	dmac4->dmac.intrChannels = 0;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&dmac4->dmac.lock, intr);
}

void RemoveQueuedDmaOperation(DmaOp *op)
{
	DmaController *dmac = op->dmac;

	SceCpuForDriver_BF82DEB2(&dmac->lock);
	if (dmac->head == op)
		dmac->head = op->next;
	if (dmac->tail == op)
		dmac->tail = op->prev;
	if (op->prev != NULL)
		op->prev->next = op->next;
	if (op->next != NULL)
		op->next->prev = op->prev;
	op->prev = NULL;
	op->next = NULL;
	sceKernelSpinlockLowUnlock(&dmac->lock);
	op->status &= ~DMA_OP_QUEUED;
}

DmaTag *AllocateDmaTagData(void)
{
	DmaTag *tag;

	while ((tag = s_dmacMgr.freeTag) != NULL) {
		if (SceCpuForDriver_CDA96E81((SceInt32 *)&s_dmacMgr.freeTag, (SceInt32)tag,
						    (SceInt32)tag->next) == (SceInt32)tag)
			return tag;
	}
	tag = SceSysmemForKernel_ABA5313C(s_dmacMgr.objectHeap);
	if (tag == NULL) {
		SCE_DBG_PRINTF(6, 378, "allocate tag failed.\n");
		return NULL;
	}
	SceSysmemForDriver_8D160E65(&tag->tag, &tag->pa);
	tag->cached = 0;
	tag->pa |= 1;
	return tag;
}

/* stop a channel and release its interrupt */
void ReleaseDmacChannel(int intrCode, DmaController *dmac, DmaChannel *channel)
{
	SceUInt32 bit = channelBit(dmac, channel);
	volatile SceDmacChannelReg *reg;

	if (!(dmac->intrChannels & bit))
		return;
	sceKernelDisableIntr(intrCode);
	sceKernelReleaseIntrHandler(intrCode);
	reg = channel->reg;
	reg->ctrl = 0;
	dsb();
	while (reg->stat & SCE_KERNEL_DMAC_STAT_BUSY)
		;
	reg->intrStatus = DMAC_INTR_COMPLETE | DMAC_INTR_ERROR;
	dsb();
	channel->op = NULL;
	dmac->freeChannels &= ~bit;
	dmac->intrChannels &= ~bit;
}

int SuspendDmacmgr(void *args)
{
	sceKernelCpuId();
	return 0;
}

/*
 * Give the tags first..last back. Runs of tags from the tag cache go back
 * onto its free list in one push each; the others go back to the fixed heap.
 */
int ReleaseDmaTagChain(DmaTag *first, DmaTag *last)
{
	DmaTag *tag = first, *run = first, *next = first->next;
	DmaTag *head;

	if (!tag->cached)
		goto release;
	for (;;) {
		if (tag == last || !next->cached) {
			do {
				head = s_dmacMgr.freeTag;
				tag->next = head;
			} while (SceCpuForDriver_CDA96E81((SceInt32 *)&s_dmacMgr.freeTag,
								 (SceInt32)head, (SceInt32)run) !=
				 (SceInt32)head);
			if (tag == last)
				return 0;
		}
		for (;;) {
			tag = next;
			next = next->next;
			if (tag->cached)
				break;
release:
			SceSysmemForKernel_93344755(s_dmacMgr.objectHeap, tag);
			run = next;
			if (tag == last)
				return 0;
		}
	}
}

/* drop an operation's transfer, as on resume */
int ResetDmacChannel(DmaOp *op)
{
	if (op == NULL || op->channel == NULL)
		return 0;
	_CancelDmaTransfer(op);
	op->status |= DMA_OP_DONE;
	if (op->head != NULL) {
		ReleaseDmaTagChain(op->head, op->tail);
		op->head = NULL;
		op->tail = NULL;
	}
	return 0;
}

/* DMAC5's SHA modes: a chain of several tags starts, continues and ends the hash */
static SceUInt32 chainHashCmd(SceUInt32 cmd, SceUInt32 index)
{
	switch (cmd & DMAC_CMD_HASH_MASK) {
	case DMAC_CMD_HASH_INIT:
		if (index != 0)
			cmd |= DMAC_CMD_HASH_UPDATE;
		break;
	case DMAC_CMD_HASH_RESULT:
		cmd |= DMAC_CMD_HASH_UPDATE;
		break;
	case DMAC_CMD_HASH_DIGEST:
		if (index != 0)
			cmd |= DMAC_CMD_HASH_UPDATE;
		else
			cmd = (cmd & ~DMAC_CMD_HASH_MASK) | DMAC_CMD_HASH_INIT;
		break;
	}
	return cmd;
}

/* one tag per stretch where both the source and the destination are contiguous */
static int CreateDmaTagChain(int dmacId, SceKernelPAVector *src, SceKernelPAVector *dst,
			     const SceKernelDmaOpTag *param, DmaTag **head, DmaTag **tail)
{
	SceKernelPARange *srcRange, *dstRange, *srcEnd, *dstEnd;
	SceUInt32 srcBlockMask, dstBlockMask, blockMask, cmd, count;
	SceSize size, offset;
	DmaTag *tag, *prev;

	if (src->capacity == 0 || dst->capacity == 0)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	srcRange = src->pRanges;
	dstRange = dst->pRanges;
	srcEnd = srcRange + src->capacity;
	dstEnd = dstRange + dst->capacity;
	srcBlockMask = param->blockSize & 0xffff;
	if (srcBlockMask != 0)
		srcBlockMask--;
	dstBlockMask = param->blockSize >> 16;
	if (dstBlockMask != 0)
		dstBlockMask--;
	blockMask = dstBlockMask | srcBlockMask;

	prev = NULL;
	count = 0;
	offset = 0;
	tag = AllocateDmaTagData();
	if (tag == NULL)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	for (;;) {
		tag->offset = offset;
		tag->tag.src = (const void *)srcRange->addr;
		tag->tag.dst = (void *)dstRange->addr;
		if (srcRange->size < dstRange->size) {
			size = srcRange->size;
			if (!(param->len & DMAC_LEN_MOD_DST_MASK)) {
				dstRange->addr += size;
				dstRange->size -= size;
			}
			srcRange++;
		} else if (srcRange->size == dstRange->size) {
			size = srcRange->size;
			srcRange++;
			dstRange++;
		} else {
			size = dstRange->size;
			if (!(param->len & DMAC_LEN_MOD_SRC_MASK)) {
				srcRange->addr += size;
				srcRange->size -= size;
			}
			dstRange++;
		}
		offset += size;
		cmd = param->cmd;
		if (dmacId == SCE_KERNEL_DMAC_ID_DMAC5) {
			tag->tag.len = size;
			tag->tag.keyring = param->keyring;
			tag->tag.iv = param->iv;
			if ((cmd & 7) == SCE_KERNEL_DMAC_CMD_OP_HASH_SHA1)
				cmd = chainHashCmd(cmd, count);
		} else {
			if (size & blockMask) {
				SCE_DBG_PRINTF(0xb, 1026, "physical memory size is not aligned with blockmask. psize 0x%08x, blockmask 0x%08x\n",
					       size, blockMask);
				return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
			}
			tag->tag.len = size | (param->len & 0xff000000);
		}
		tag->tag.cmd = cmd & ~(DMAC_CMD_INTERRUPT | 0x2000);
		tag->prev = prev;
		tag->tag.blockSize = param->blockSize;
		if (prev == NULL) {
			*head = tag;
		} else {
			prev->next = tag;
			prev->tag.pNext = (SceKernelDmaOpTag *)tag->pa;
		}
		tag->param = param;
		count++;
		if (dstRange >= dstEnd || srcRange >= srcEnd)
			break;
		prev = tag;
		tag = AllocateDmaTagData();
		if (tag == NULL)
			return SCE_KERNEL_ERROR_NO_MEMORY;
	}

	cmd = param->cmd;
	if (dmacId == SCE_KERNEL_DMAC_ID_DMAC5 && (cmd & 7) == SCE_KERNEL_DMAC_CMD_OP_HASH_SHA1) {
		if ((cmd & DMAC_CMD_HASH_MASK) == DMAC_CMD_HASH_INIT) {
			if (count > 1)
				cmd |= DMAC_CMD_HASH_UPDATE;
		} else if ((cmd & DMAC_CMD_HASH_MASK) == DMAC_CMD_HASH_RESULT ||
			   ((cmd & DMAC_CMD_HASH_MASK) == DMAC_CMD_HASH_DIGEST && count > 1)) {
			cmd = (cmd & ~DMAC_CMD_HASH_MASK) | DMAC_CMD_HASH_RESULT;
		}
	}
	tag->tag.pNext = SCE_KERNEL_DMAC_CHAIN_END;
	tag->tag.cmd = cmd;
	tag->next = NULL;
	*tail = tag;
	return 0;
}

/* start the first queued operation on a free channel of its controller */
void StartQueuedDmaOperation(DmaController *dmac, DmaOp *op)
{
	SceUInt32 freeChannels = dmac->freeChannels;
	volatile SceDmacChannelReg *reg;
	DmaChannel *channel;
	DmaTag *head;

	if (freeChannels == 0)
		return;
	channel = &dmac->channel[__builtin_clz(freeChannels)];
	reg = channel->reg;
	dmac->freeChannels = freeChannels - channelBit(dmac, channel);
	channel->op = op;
	op->channel = channel;
	reg->coherencyMask = op->coherencyMask;
	if (op->dmacId == SCE_KERNEL_DMAC_ID_DMAC5) {
		volatile SceDmac5ChannelReg *reg5 = (volatile SceDmac5ChannelReg *)reg;
		const SceKernelDmaOpEncDec *encDec = op->encDec;
		const SceUInt32 *key = (const SceUInt32 *)encDec->key;
		int i;

		reg->srcKeyringId = encDec->keyring;
		reg->iv = (SceUInt32)encDec->iv;
		reg->ivCoherencyMask = encDec->ivCoherencyMask;
		for (i = 0; i < 16; i++)
			reg5->key[i] = key[i];
		head = op->head;
	} else {
		head = op->head;
		if ((head->tag.cmd & 0xc) == SCE_KERNEL_DMAC_CMD_OP_SET)
			reg->setValue = op->setValue;
	}
	reg->chainStart = head->pa;
	dsb();
	dmac->head = op->next;
	if (op->next == NULL)
		dmac->tail = NULL;
	op->prev = NULL;
	op->next = NULL;
	op->status &= ~DMA_OP_QUEUED;
}

/* hand the channel back to its controller and start the next queued operation */
static inline void releaseOpChannel(DmaOp *op, DmaChannel *channel)
{
	DmaController *dmac = op->dmac;
	SceKernelIntrStatus intr;

	channel->op = NULL;
	op->channel = NULL;
	dmac->freeChannels |= channelBit(dmac, channel);
	intr = SceCpuForDriver_D32ACE9E(&dmac->lock);
	if (dmac->head != NULL)
		StartQueuedDmaOperation(dmac, dmac->head);
	sceKernelSpinlockLowUnlockCpuResumeIntr(&dmac->lock, intr);
	op->status |= DMA_OP_DONE;
}

/* completion or error of one channel */
void DmacIntrHandler(DmaChannel *channel)
{
	volatile SceDmacChannelReg *reg = channel->reg;
	DmaOp *op = channel->op;
	DmaTag *head, *tag, *prev, *next;
	SceUInt32 intrStatus, stat, error, busy, current;
	SceSize completed;
	DmaOpCallback callback;
	void *pUserData;
	const SceKernelDmaOpTag *errorTag;

	if (op == NULL)
		return;
	SceCpuForDriver_BF82DEB2(&op->lock);
	SCE_DBG_ASSERT(op->channel == channel, 1897);
	intrStatus = reg->intrStatus;
	reg->intrStatus = intrStatus;
	dsb();
	stat = reg->stat;
	if (!(op->status & DMA_OP_ALLOCATED) ||
	    !(intrStatus & (DMAC_INTR_COMPLETE | DMAC_INTR_ERROR))) {
		sceKernelSpinlockLowUnlock(&op->lock);
		return;
	}
	if (op->status & DMA_OP_CANCELING) {
		DmaController *dmac = op->dmac;

		reg->ctrl = 0;
		dsb();
		while (reg->stat & SCE_KERNEL_DMAC_STAT_BUSY)
			;
		reg->intrStatus = DMAC_INTR_COMPLETE | DMAC_INTR_ERROR;
		dsb();
		channel->op = NULL;
		op->channel = NULL;
		dmac->freeChannels |= channelBit(dmac, channel);
		op->status &= ~DMA_OP_CANCELING;
		sceKernelSpinlockLowUnlock(&op->lock);
		return;
	}

	head = op->head;
	error = intrStatus & DMAC_INTR_ERROR;
	if (error) {
		/* the tags before the failing one are done, the failing one partly */
		current = reg->currentTag;
		completed = 0;
		prev = NULL;
		for (tag = head; tag != NULL; prev = tag, tag = tag->next) {
			completed += tag->tag.len & DMAC_LEN_MASK;
			if (tag->pa == current) {
				completed -= reg->errorLen;
				op->errorTag = tag->param;
				break;
			}
		}
		if (prev != NULL)
			ReleaseDmaTagChain(head, prev);
		next = tag;
		SCE_DBG_PRINTF(0, 1949, "DMAC transfer error ! stat = 0x%08x, curchain %p, opname %s\n",
			       stat, op->errorTag, op->name);
		if (stat & SCE_KERNEL_DMAC_STAT_ABORTED)
			SCE_DBG_PRINTF(0, 1952, "DMAC transfer is aborted\n");
		if (stat & SCE_KERNEL_DMAC_STAT_ERROR_READ)
			SCE_DBG_PRINTF(0, 1955, "DMAC transfer error: read error\n");
		if (stat & SCE_KERNEL_DMAC_STAT_ERROR_WRITE)
			SCE_DBG_PRINTF(0, 1958, "DMAC transfer error: write error\n");
		if (stat & SCE_KERNEL_DMAC_STAT_ERROR_ILLEGAL_CONFIG)
			SCE_DBG_PRINTF(0, 1961, "DMAC transfer error: illegal configuration\n");
		if (stat & SCE_KERNEL_DMAC_STAT_ERROR_TAG)
			SCE_DBG_PRINTF(0, 1964, "DMAC transfer error: tag error\n");
		if (stat & SCE_KERNEL_DMAC_STAT_ERROR_ZERO_BYTE)
			SCE_DBG_PRINTF(0, 1967, "DMAC transfer error: zero byte error\n");
		busy = stat & SCE_KERNEL_DMAC_STAT_BUSY;
	} else if (head != NULL) {
		busy = reg->stat & SCE_KERNEL_DMAC_STAT_BUSY;
		tag = head;
		if (stat & SCE_KERNEL_DMAC_STAT_BUSY) {
			/* still running: done up to the tag that raised the interrupt */
			completed = 0;
			for (;;) {
				SCE_DBG_ASSERT(tag != NULL, 1977);
				completed += tag->tag.len & DMAC_LEN_MASK;
				if (tag->tag.cmd & DMAC_CMD_INTERRUPT)
					break;
				tag = tag->next;
			}
		} else {
			/* stopped: done up to the current tag */
			current = reg->currentTag;
			completed = tag->tag.len & DMAC_LEN_MASK;
			while (tag->pa != current) {
				tag = tag->next;
				completed += tag->tag.len & DMAC_LEN_MASK;
			}
		}
		next = tag->next;
		ReleaseDmaTagChain(op->head, tag);
	} else {
		next = NULL;
		busy = reg->stat & SCE_KERNEL_DMAC_STAT_BUSY;
		completed = 0;
	}

	op->head = next;
	sceKernelSetEventFlag(op->evfId, 1);
	if (!busy) {
		reg->intrStatus = DMAC_INTR_COMPLETE;
		dsb();
		if (op->head != NULL && !error) {
			/* continue with the rest of the chain */
			op->head->prev = NULL;
			reg->chainStart = op->head->pa;
			dsb();
		} else {
			releaseOpChannel(op, channel);
		}
	}

	callback = op->callback;
	if (callback == NULL) {
		sceKernelSpinlockLowUnlock(&op->lock);
		return;
	}
	pUserData = op->pUserData;
	errorTag = op->errorTag;
	sceKernelSpinlockLowUnlock(&op->lock);
	callback(getDmaOpId(op), stat, pUserData, errorTag, completed);
}

/* the interrupt of four DMAC4 channels */
int Dmac4IntrHandler(int intrCode, void *activeIntr)
{
	SceUInt32 active = *(SceUInt32 *)activeIntr;
	int i;

	for (i = 0; i < 16; i++) {
		if (active & (1 << i))
			DmacIntrHandler(&s_dmacMgr.dmac4[i].channel);
	}
	return -1;
}

int DmacChannelIntrHandler(int intrCode, void *channel)
{
	DmacIntrHandler(channel);
	return -1;
}

/* channel interrupts, in their registration order */
static const SceUInt8 s_intrCode[14] = {
	0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x7c, 0x7d, 0x78, 0x79, 0x7a, 0x7b,
};

int ResumeDmacmgr(void *args)
{
	volatile SceDmac4Reg *reg4;
	int i;

	if (sceKernelCpuId() != 0)
		return 0;
	ResetDmacChannel(s_dmacMgr.dmac01Channel[0].op);
	ResetDmacChannel(s_dmacMgr.dmac01Channel[1].op);
	ResetDmacChannel(s_dmacMgr.dmac23Channel[0].op);
	ResetDmacChannel(s_dmacMgr.dmac23Channel[1].op);
	ResetDmacChannel(s_dmacMgr.dmac5Channel[0].op);
	ResetDmacChannel(s_dmacMgr.dmac5Channel[1].op);
	for (i = 0; i < 16; i++)
		ResetDmacChannel(s_dmacMgr.dmac4[i].channel.op);
	if (s_dmacMgr.dmac6Reg != NULL) {
		ResetDmacChannel(s_dmacMgr.dmac6Channel[0].op);
		ResetDmacChannel(s_dmacMgr.dmac6Channel[1].op);
	}
	for (i = 0; i < 14; i++)
		sceKernelEnableIntr(s_intrCode[i]);
	reg4 = s_dmacMgr.dmac4Reg;
	reg4->gate[0].intrMask = 0xffff;
	reg4->gate[1].intrMask = 0;
	reg4->gate[2].intrMask = 0;
	reg4->gate[3].intrMask = 0;
	if (s_dmacMgr.dmac6Reg != NULL) {
		sceKernelEnableIntr(0x7e);
		sceKernelEnableIntr(0x7f);
	}
	return 0;
}

void *mapDmac(const char *name, SceUInt32 pa)
{
	void *base;
	SceKernelAllocMemBlockKernelOpt opt;
	SceUID memBlockId;

	memset(&opt, 0, sizeof(opt));
	opt.size = sizeof(opt);
	opt.attr = SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_PADDR;
	opt.paddr = pa;
	memBlockId = SceSysmemForDriver_C94850C9(name, SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_IO_RW,
					   DMAC_REG_SIZE, &opt);
	if (memBlockId < 0) {
		SCE_DBG_PRINTF(0xb, 2285, "map %s failed. 0x%08x\n", name, memBlockId);
		SCE_DBG_ASSERT_FAIL(2286);
		return NULL;
	}
	SceSysmemForDriver_A841EDDA(memBlockId, &base);
	return base;
}

/* the physical ranges of addr..addr+size; more than one go into the range heap */
int AllocPARanges(SceUInt32 addr, SceSize size, SceKernelPAVector *vector)
{
	SceKernelVARange range;
	SceKernelPARange *ranges;
	int res;

	range.size = size & DMAC_LEN_MASK;
	vector->size = sizeof(*vector);
	range.addr = addr;
	res = sceKernelVARangeToPAVector(&range, vector);
	if (res < 0) {
		SCE_DBG_PRINTF(0xf, 917, "VA to PA failed. 0x%08x\n", res);
		return res;
	}
	if (vector->total == 1)
		return 0;
	ranges = SceSysmemForDriver_7B4CB60A(s_dmacMgr.rangeHeapId, vector->total * sizeof(*ranges));
	if (ranges == NULL)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	vector->pRanges = ranges;
	vector->capacity = vector->total;
	res = sceKernelVARangeToPAVector(&range, vector);
	if (res < 0) {
		SCE_DBG_PRINTF(0xf, 938, "VA to PA failed. 0x%08x\n", res);
		return res;
	}
	SCE_DBG_ASSERT(vector->nDataInVector == vector->total, 942);
	return 0;
}

/* turn the caller's tags into a chain and append it to the operation's */
int _concatenateChain(DmaOp *op, SceKernelDmaOpFlag flag, const SceKernelDmaOpTag *param)
{
	DmaTag *first, *last, *head, *tail;
	SceKernelPAVector src, dst;
	SceKernelPARange range, physRange;
	SceUInt32 cmd;
	int res;

	if (param == NULL || param == SCE_KERNEL_DMAC_CHAIN_END) {
		SCE_DBG_PRINTF(0xb, 1132, "error: pTag is NULL\n");
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	}
	head = NULL;
	tail = NULL;
	switch (flag) {
	case SCE_KERNEL_DMA_OP_PHYSICAL_ADDR:
		do {
			DmaTag *tag = AllocateDmaTagData();

			if (tag == NULL)
				return SCE_KERNEL_ERROR_NO_MEMORY;
			tag->tag.dst = param->dst;
			tag->tag.len = param->len;
			tag->tag.cmd = param->cmd;
			tag->tag.blockSize = param->blockSize;
			tag->tag.src = param->src;
			tag->offset = 0;
			tag->next = NULL;
			tag->prev = tail;
			tag->param = param;
			if (head != NULL) {
				tail->next = tag;
				tail->tag.pNext = (SceKernelDmaOpTag *)tag->pa;
			} else {
				head = tag;
			}
			param = param->pNext;
			tail = tag;
		} while (param != SCE_KERNEL_DMAC_CHAIN_END);
		tail->tag.pNext = SCE_KERNEL_DMAC_CHAIN_END;
		cmd = tail->tag.cmd;
		break;
	case SCE_KERNEL_DMA_OP_VIRTUAL_SRC_ADDR:
		dst.capacity = 1;
		dst.pRanges = &physRange;
		do {
			src.capacity = 1;
			src.pRanges = &range;
			physRange.addr = (SceUInt32)param->dst;
			physRange.size = param->len & DMAC_LEN_MASK;
			res = AllocPARanges((SceUInt32)param->src, param->len, &src);
			if (res < 0)
				return res;
			res = CreateDmaTagChain(op->dmacId, &src, &dst, param, &first, &last);
			if (src.capacity > 1)
				sceKernelFreeHeapMemory(s_dmacMgr.rangeHeapId, src.pRanges);
			if (res < 0)
				return res;
			if (head != NULL) {
				tail->next = first;
				tail->tag.pNext = (SceKernelDmaOpTag *)first->pa;
				first->prev = tail;
			} else {
				head = first;
			}
			tail = last;
			param = param->pNext;
		} while (param != SCE_KERNEL_DMAC_CHAIN_END);
		cmd = tail->tag.cmd;
		break;
	case SCE_KERNEL_DMA_OP_VIRTUAL_DST_ADDR:
		src.capacity = 1;
		src.pRanges = &physRange;
		do {
			dst.capacity = 1;
			dst.pRanges = &range;
			physRange.addr = (SceUInt32)param->src;
			physRange.size = param->len & DMAC_LEN_MASK;
			res = AllocPARanges((SceUInt32)param->dst, param->len, &dst);
			if (res < 0)
				return res;
			res = CreateDmaTagChain(op->dmacId, &src, &dst, param, &first, &last);
			if (dst.capacity > 1)
				sceKernelFreeHeapMemory(s_dmacMgr.rangeHeapId, dst.pRanges);
			if (res < 0)
				return res;
			if (head != NULL) {
				tail->next = first;
				tail->tag.pNext = (SceKernelDmaOpTag *)first->pa;
				first->prev = tail;
			} else {
				head = first;
			}
			tail = last;
			param = param->pNext;
		} while (param != SCE_KERNEL_DMAC_CHAIN_END);
		cmd = tail->tag.cmd;
		break;
	case SCE_KERNEL_DMA_OP_VIRTUAL_ADDR:
		do {
			src.capacity = 1;
			src.pRanges = &range;
			res = AllocPARanges((SceUInt32)param->src, param->len, &src);
			if (res < 0)
				return res;
			dst.capacity = 1;
			dst.pRanges = &physRange;
			res = AllocPARanges((SceUInt32)param->dst, param->len, &dst);
			if (res < 0) {
				sceKernelFreeHeapMemory(s_dmacMgr.rangeHeapId, src.pRanges);
				return res;
			}
			res = CreateDmaTagChain(op->dmacId, &src, &dst, param, &first, &last);
			if (src.capacity > 1)
				sceKernelFreeHeapMemory(s_dmacMgr.rangeHeapId, src.pRanges);
			if (dst.capacity > 1)
				sceKernelFreeHeapMemory(s_dmacMgr.rangeHeapId, dst.pRanges);
			if (res < 0)
				return res;
			if (head != NULL) {
				tail->next = first;
				tail->tag.pNext = (SceKernelDmaOpTag *)first->pa;
				first->prev = tail;
			} else {
				head = first;
			}
			tail = last;
			param = param->pNext;
		} while (param != SCE_KERNEL_DMAC_CHAIN_END);
		cmd = tail->tag.cmd;
		break;
	case SCE_KERNEL_DMA_OP_COMPLETE_CHAIN:
		/* param[0] starts a chain the caller built, param[1] ends it */
		head = AllocateDmaTagData();
		if (head == NULL)
			return SCE_KERNEL_ERROR_NO_MEMORY;
		tail = AllocateDmaTagData();
		if (tail == NULL) {
			ReleaseDmaTagChain(head, head);
			return SCE_KERNEL_ERROR_NO_MEMORY;
		}
		head->tag.cmd = param[0].cmd;
		head->tag.blockSize = param[0].blockSize;
		head->tag.pNext = param[0].pNext;
		head->tag.len = param[0].len;
		head->tag.src = param[0].src;
		head->tag.dst = param[0].dst;
		head->param = &param[0];
		head->next = tail;
		head->offset = 0;
		head->prev = NULL;
		param[1].pNext->pNext = (SceKernelDmaOpTag *)tail->pa;
		tail->tag.src = param[1].src;
		tail->tag.dst = param[1].dst;
		tail->tag.len = param[1].len;
		tail->tag.cmd = param[1].cmd;
		tail->tag.blockSize = param[1].blockSize;
		tail->offset = 0;
		tail->next = NULL;
		tail->prev = head;
		tail->tag.pNext = SCE_KERNEL_DMAC_CHAIN_END;
		tail->param = &param[1];
		cmd = param[1].cmd;
		break;
	default:
		SCE_DBG_PRINTF(0xb, 1335, "Unknown flag\n");
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	}

	tail->tag.cmd = cmd | DMAC_CMD_INTERRUPT;
	if (op->head != NULL) {
		op->tail->next = head;
		op->tail->tag.pNext = (SceKernelDmaOpTag *)head->pa;
	} else {
		op->head = head;
	}
	op->tail = tail;
	return 0;
}

SceKernelDmaOpId sceKernelDmaOpAlloc(const char *name)
{
	SceKernelDmaOpId opid;
	DmaOp *op;
	SceUID evfId;

	if (SceIntrmgrForDriver_182EE3E3())
		return SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
	if (name == NULL) {
		name = "SceDmacDefault";
		SCE_DBG_PRINTF_FUNC(0xb, 690, "sceKernelDmaOpAlloc",
				    "dmac op name is null from %p!\n",
			       __builtin_return_address(0));
	}
	op = SceSysmemForKernel_ABA5313C(s_dmacMgr.objectHeap);
	if (op == NULL)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	opid = getDmaOpId(op);
	memset(op, 0, __builtin_offsetof(DmaOp, magic));
	op->magic = DMA_OP_MAGIC;
	evfId = sceKernelCreateEventFlag(name, 0, 0, NULL);
	if (evfId < 0) {
		SCE_DBG_PRINTF_FUNC(0xb, 708, "sceKernelDmaOpAlloc",
				    "Open event flag failed. 0x%08x\n", evfId);
		SceSysmemForKernel_93344755(s_dmacMgr.objectHeap, op);
		return evfId;
	}
	op->evfId = evfId;
	op->name = SceSysmemForDriver_E655852F(evfId);
	op->status = DMA_OP_ALLOCATED;
	return opid;
}

int sceKernelDmaOpFree(SceKernelDmaOpId opid)
{
	DmaOp *op = getDmaOp(opid);
	SceKernelIntrStatus intr;
	int retry;

	if (SceIntrmgrForDriver_182EE3E3())
		return SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
	if (op->magic != DMA_OP_MAGIC) {
		SCE_DBG_PRINTF_FUNC(0xb, 732, "sceKernelDmaOpFree",
				    "invalid magic number. opid 0x%08x, magic 0x%08x\n",
			       opid, op->magic);
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	}
	intr = SceCpuForDriver_D32ACE9E(&op->lock);
	if (!(op->status & DMA_OP_ALLOCATED)) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&op->lock, intr);
		return SCE_KERNEL_ERROR_NOT_INITIALIZED;
	}
	if (op->status & DMA_OP_CANCELING) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&op->lock, intr);
		return SCE_KERNEL_ERROR_CANCELING;
	}
	if (op->channel != NULL) {
		_CancelDmaTransfer(op);
		if (op->status & DMA_OP_CANCELING) {
			/* the interrupt handler finishes the cancel */
			sceKernelSpinlockLowUnlockCpuResumeIntr(&op->lock, intr);
			for (retry = 5; ; ) {
				sceKernelDelayThread(10);
				intr = SceCpuForDriver_D32ACE9E(&op->lock);
				if (!(op->status & DMA_OP_CANCELING))
					break;
				sceKernelSpinlockLowUnlockCpuResumeIntr(&op->lock, intr);
				if (--retry == 0)
					return SCE_ERROR_ERRNO_EBUSY;
			}
		}
	}
	if (op->status & DMA_OP_QUEUED)
		RemoveQueuedDmaOperation(op);
	sceKernelDeleteEventFlag(op->evfId);
	op->status = 0;
	op->magic = 0;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&op->lock, intr);
	SceSysmemForKernel_93344755(s_dmacMgr.objectHeap, op);
	return 0;
}

int sceKernelDmaOpAssign(SceKernelDmaOpId opid, SceKernelDmacId dmacId, SceUInt32 channel)
{
	DmaOp *op = getDmaOp(opid);
	DmaController *dmac;
	SceKernelIntrStatus intr;

	if (op->magic != DMA_OP_MAGIC) {
		SCE_DBG_PRINTF_FUNC(0xb, 793, "sceKernelDmaOpAssign",
				    "invalid magic number. opid 0x%08x, magic 0x%08x\n",
			       opid, op->magic);
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	}
	intr = SceCpuForDriver_D32ACE9E(&op->lock);
	if (!(op->status & DMA_OP_ALLOCATED)) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&op->lock, intr);
		return SCE_KERNEL_ERROR_NOT_INITIALIZED;
	}
	if ((op->status & DMA_OP_QUEUED) || op->channel != NULL) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&op->lock, intr);
		return SCE_KERNEL_ERROR_ALREADY_QUEUED;
	}
	if (op->status & DMA_OP_CANCELING) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&op->lock, intr);
		return SCE_KERNEL_ERROR_CANCELING;
	}
	switch (dmacId) {
	case SCE_KERNEL_DMAC_ID_DMAC01:
		dmac = op->dmac = &s_dmacMgr.dmac01;
		break;
	case SCE_KERNEL_DMAC_ID_DMAC23:
		dmac = op->dmac = &s_dmacMgr.dmac23;
		break;
	case SCE_KERNEL_DMAC_ID_DMAC4:
		if (channel > 15)
			goto invalid;
		dmac = op->dmac = &s_dmacMgr.dmac4[channel].dmac;
		break;
	case SCE_KERNEL_DMAC_ID_DMAC5:
		dmac = op->dmac = &s_dmacMgr.dmac5;
		break;
	case SCE_KERNEL_DMAC_ID_DMAC6:
		if (s_dmacMgr.dmac6Reg == NULL)
			goto invalid;
		dmac = op->dmac = &s_dmacMgr.dmac6;
		break;
	default:
invalid:
		sceKernelSpinlockLowUnlockCpuResumeIntr(&op->lock, intr);
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	}
	if (dmac->intrChannels == 0) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&op->lock, intr);
		return SCE_KERNEL_ERROR_NOT_UNDER_CONTROL;
	}
	op->dmacId = dmacId;
	op->status &= ~DMA_OP_DONE;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&op->lock, intr);
	return 0;
}

int sceKernelDmaOpSetCallback(SceKernelDmaOpId opid, DmaOpCallback callback, void *pUserData)
{
	DmaOp *op;
	SceKernelIntrStatus intr;

	if (callback == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	op = getDmaOp(opid);
	if (op->magic != DMA_OP_MAGIC) {
		SCE_DBG_PRINTF_FUNC(0xb, 872, "sceKernelDmaOpSetCallback",
				    "invalid magic number. opid 0x%08x, magic 0x%08x\n",
			       opid, op->magic);
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	}
	intr = SceCpuForDriver_D32ACE9E(&op->lock);
	if (!(op->status & DMA_OP_ALLOCATED)) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&op->lock, intr);
		return SCE_KERNEL_ERROR_NOT_INITIALIZED;
	}
	if ((op->status & DMA_OP_QUEUED) || op->channel != NULL) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&op->lock, intr);
		return SCE_KERNEL_ERROR_ALREADY_QUEUED;
	}
	if (op->status & DMA_OP_CANCELING) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&op->lock, intr);
		return SCE_KERNEL_ERROR_CANCELING;
	}
	op->callback = callback;
	op->pUserData = pUserData;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&op->lock, intr);
	return 0;
}

int sceKernelDmaOpSetupChain(SceKernelDmaOpId opid, SceKernelDmaOpTag *pTag,
			      SceKernelDmaOpChainParam *pParam, SceKernelDmaOpFlag flag)
{
	DmaOp *op = getDmaOp(opid);
	SceKernelIntrStatus intr;
	int res;

	if (op->magic != DMA_OP_MAGIC) {
		SCE_DBG_PRINTF_FUNC(0xb, 1362, "sceKernelDmaOpSetupChain",
				    "invalid magic number. opid 0x%08x, magic 0x%08x\n",
			       opid, op->magic);
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	}
	intr = SceCpuForDriver_D32ACE9E(&op->lock);
	if (pParam != NULL) {
		if (op->dmacId == SCE_KERNEL_DMAC_ID_DMAC5) {
			if (pParam->size != sizeof(SceKernelDmaOpEncDecChainParam))
				goto invalidSize;
		} else if (pParam->size != sizeof(SceKernelDmaOpChainParam)) {
invalidSize:
			sceKernelSpinlockLowUnlockCpuResumeIntr(&op->lock, intr);
			return SCE_KERNEL_ERROR_INVALID_ARGUMENT_SIZE;
		}
	}
	if (!(op->status & DMA_OP_ALLOCATED)) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&op->lock, intr);
		return SCE_KERNEL_ERROR_NOT_INITIALIZED;
	}
	if ((op->status & DMA_OP_QUEUED) || op->channel != NULL) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&op->lock, intr);
		return SCE_KERNEL_ERROR_ALREADY_QUEUED;
	}
	if (op->status & DMA_OP_CANCELING) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&op->lock, intr);
		return SCE_KERNEL_ERROR_CANCELING;
	}
	if (op->head != NULL)
		ReleaseDmaTagChain(op->head, op->tail);
	op->tail = NULL;
	op->coherencyMask = 0x7fc0000;
	op->head = NULL;
	if (pParam != NULL) {
		op->coherencyMask = pParam->coherencyMask | 0x7fc0000;
		if (op->dmacId == SCE_KERNEL_DMAC_ID_DMAC5)
			op->encDec = &((SceKernelDmaOpEncDecChainParam *)pParam)->encDec;
		else
			op->setValue = pParam->setValue;
	}
	res = _concatenateChain(op, flag, pTag);
	if (res < 0) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&op->lock, intr);
		return res;
	}
	op->status &= ~DMA_OP_DONE;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&op->lock, intr);
	return 0;
}

int _DmaOpSetupDirect(SceKernelDmaOpId opid, SceKernelDmaOpDirectParam *pParam,
			       SceKernelDmaOpFlag flag)
{
	SceKernelDmaOpTag tag;
	DmaOp *op;
	int res;

	if (pParam == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	op = getDmaOp(opid);
	if (op->magic != DMA_OP_MAGIC) {
		SCE_DBG_PRINTF_FUNC(0xb, 1076, "_DmaOpSetupDirect",
				    "invalid magic number. opid 0x%08x, magic 0x%08x\n",
				    opid, op->magic);
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	}
	if (op->dmacId == SCE_KERNEL_DMAC_ID_DMAC5) {
		tag.keyring = pParam->encDec.keyring;
		tag.iv = pParam->encDec.iv;
	}
	tag.src = pParam->src;
	tag.dst = pParam->dst;
	tag.len = pParam->len;
	tag.cmd = pParam->cmd;
	tag.blockSize = pParam->blockSize;
	tag.pNext = SCE_KERNEL_DMAC_CHAIN_END;
	res = sceKernelDmaOpSetupChain(opid, &tag, NULL, flag);
	if (res < 0)
		return res;
	if (op->dmacId == SCE_KERNEL_DMAC_ID_DMAC5)
		op->encDec = &pParam->encDec;
	else
		op->setValue = pParam->setValue;
	op->coherencyMask |= pParam->coherencyMask & ~SCE_KERNEL_DMAC_COHERENCY_MSK_UNK_MASK;
	return 0;
}

/* queue an operation, or start it when its controller has nothing queued */
static inline void enqueueDmaOp(DmaController *dmac, DmaOp *op)
{
	DmaOp *tail;

	op->status |= DMA_OP_QUEUED;
	if (dmac->head == NULL) {
		dmac->tail = op;
		dmac->head = op;
		StartQueuedDmaOperation(dmac, op);
		return;
	}
	tail = dmac->tail;
	if (tail->next != NULL) {
		tail->next->prev = op;
		op->next = tail->next;
	}
	tail->next = op;
	op->prev = tail;
	dmac->tail = op;
}

int sceKernelDmaOpConcatenate(SceKernelDmaOpId opid, SceKernelDmaOpTag *pTag,
			       SceKernelDmaOpFlag flag)
{
	DmaOp *op = getDmaOp(opid);
	DmaController *dmac;
	SceKernelIntrStatus intr;
	SceUInt16 done;
	int res;

	if (op->magic != DMA_OP_MAGIC) {
		SCE_DBG_PRINTF_FUNC(0xb, 1441, "sceKernelDmaOpConcatenate",
				    "invalid magic number. opid 0x%08x, magic 0x%08x\n",
			       opid, op->magic);
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	}
	intr = SceCpuForDriver_D32ACE9E(&op->lock);
	if (!(op->status & DMA_OP_ALLOCATED)) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&op->lock, intr);
		return SCE_KERNEL_ERROR_NOT_INITIALIZED;
	}
	done = op->status & DMA_OP_DONE;
	if (!done && op->head == NULL) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&op->lock, intr);
		return SCE_KERNEL_ERROR_NOT_SETUP;
	}
	if (op->status & DMA_OP_CANCELING) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&op->lock, intr);
		return SCE_KERNEL_ERROR_CANCELING;
	}
	op->status -= done;
	res = _concatenateChain(op, flag, pTag);
	if (res >= 0 && done) {
		/* the operation had finished: queue it again */
		dmac = op->dmac;
		SceCpuForDriver_BF82DEB2(&dmac->lock);
		if (dmac->intrChannels == 0) {
			sceKernelSpinlockLowUnlock(&dmac->lock);
			res = SCE_KERNEL_ERROR_NOT_UNDER_CONTROL;
		} else {
			enqueueDmaOp(dmac, op);
			res = 0;
			sceKernelSpinlockLowUnlock(&dmac->lock);
		}
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&op->lock, intr);
	return res;
}

int sceKernelDmaOpEnQueue(SceKernelDmaOpId opid)
{
	DmaOp *op = getDmaOp(opid);
	DmaController *dmac;
	SceKernelIntrStatus intr;
	int res;

	if (op->magic != DMA_OP_MAGIC) {
		SCE_DBG_PRINTF_FUNC(0xb, 1489, "sceKernelDmaOpEnQueue",
				    "invalid magic number. opid 0x%08x, magic 0x%08x\n",
			       opid, op->magic);
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	}
	intr = SceCpuForDriver_D32ACE9E(&op->lock);
	if (!(op->status & DMA_OP_ALLOCATED)) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&op->lock, intr);
		return SCE_KERNEL_ERROR_NOT_INITIALIZED;
	}
	if (op->head == NULL) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&op->lock, intr);
		return SCE_KERNEL_ERROR_NOT_SETUP;
	}
	if ((op->status & DMA_OP_QUEUED) || op->channel != NULL) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&op->lock, intr);
		return SCE_KERNEL_ERROR_ALREADY_QUEUED;
	}
	if (op->status & DMA_OP_DONE) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&op->lock, intr);
		return SCE_KERNEL_ERROR_TRANSFERRED;
	}
	if (op->status & DMA_OP_CANCELING) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&op->lock, intr);
		return SCE_KERNEL_ERROR_CANCELING;
	}
	dmac = op->dmac;
	SceCpuForDriver_BF82DEB2(&dmac->lock);
	if (dmac->intrChannels == 0) {
		sceKernelSpinlockLowUnlock(&dmac->lock);
		res = SCE_KERNEL_ERROR_NOT_UNDER_CONTROL;
	} else {
		enqueueDmaOp(dmac, op);
		res = 0;
		sceKernelSpinlockLowUnlock(&dmac->lock);
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&op->lock, intr);
	return res;
}

/* DMA access to an address is not cache coherent when it is mapped uncached */
static inline SceBool isUncacheAddress(void *addr)
{
	SceUInt32 va = (SceUInt32)addr;

	if (va - 0x18000000 < 0x08000000)
		return sceKernelIsUncacheAddressInTmpFsGame(addr);
	return va - 0x20000000 < 0x20000000 || va - 0x60000000 < 0x21000000;
}

/* the DMAC on the bus of an address: DMAC2/3 for CDRAM, DMAC0/1 for the rest */
static inline SceKernelDmacId dmacForAddress(const void *addr)
{
	if (((SceUInt32)addr & ~0x40000000) - 0x20000000 < 0x08000000)
		return SCE_KERNEL_DMAC_ID_DMAC23;
	return SCE_KERNEL_DMAC_ID_DMAC01;
}

static const SceKernelDmaOpChainParam s_memcpyParam = {
	.size = sizeof(SceKernelDmaOpChainParam),
	.coherencyMask = 0x3e7f3,
	.setValue = 0,
};

/* copy in pieces of at most 16 MiB - 1, the most a tag can hold */
static inline int dmacMemcpySub(DmaOp *op, void *dst, const void *src, SceSize size)
{
	SceKernelDmaOpId opid = getDmaOpId(op);
	SceKernelDmaOpTag tag;
	SceKernelDmacId dmacId;
	SceUInt32 cmd;
	SceSize n;
	int res;

	cmd = SCE_KERNEL_DMAC_CMD_COHERENT_DST | 0x3000;
	if (isUncacheAddress(dst))
		cmd = 0x3000;
	if (isUncacheAddress((void *)src))
		dmacId = dmacForAddress(src);
	else {
		cmd |= SCE_KERNEL_DMAC_CMD_COHERENT_SRC;
		dmacId = dmacForAddress(dst);
	}
	tag.cmd = cmd;
	tag.blockSize = 0;
	tag.pNext = SCE_KERNEL_DMAC_CHAIN_END;
	if (cmd & (SCE_KERNEL_DMAC_CMD_COHERENT_SRC | SCE_KERNEL_DMAC_CMD_COHERENT_DST))
		sceKernelSysrootDisableAutoClockDown();
	for (;;) {
		n = size > DMAC_LEN_MASK - 1 ? DMAC_LEN_MASK : size;
		tag.src = src;
		tag.dst = dst;
		tag.len = n;
		res = sceKernelDmaOpAssign(opid, dmacId, 0);
		if (res < 0) {
			SCE_DBG_PRINTF_FUNC(0xb, 461, "dmacMemcpySub",
					    "Assign dmac operation failed. 0x%08x\n", res);
			break;
		}
		res = sceKernelDmaOpSetupChain(opid, &tag,
						(SceKernelDmaOpChainParam *)&s_memcpyParam,
						SCE_KERNEL_DMA_OP_VIRTUAL_ADDR);
		if (res < 0) {
			SCE_DBG_PRINTF_FUNC(0xb, 467, "dmacMemcpySub",
					    "Setup chain failed. 0x%08x\n", res);
			break;
		}
		res = sceKernelDmaOpEnQueue(opid);
		if (res < 0) {
			SCE_DBG_PRINTF_FUNC(0xb, 473, "dmacMemcpySub",
					    "Enqueue operation failed. 0x%08x\n", res);
			break;
		}
		res = sceKernelWaitEventFlag(op->evfId, 1,
					      SCE_KERNEL_EVF_WAITMODE_OR |
					      SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT,
					      NULL, NULL);
		if (res < 0)
			break;
		size -= n;
		if (size == 0)
			break;
		dst = (char *)dst + n;
		src = (const char *)src + n;
	}
	sceKernelDeleteEventFlag(op->evfId);
	/* wait until the interrupt handler is done with the operation */
	SceCpuForDriver_BF82DEB2(&op->lock);
	if (cmd & (SCE_KERNEL_DMAC_CMD_COHERENT_SRC | SCE_KERNEL_DMAC_CMD_COHERENT_DST))
		sceKernelSysrootEnableAutoClockDown();
	return res;
}

int sceKernelDmacMemcpy(void *dst, const void *src, SceSize size)
{
	DmaOp op;
	SceUID evfId;

	if (SceIntrmgrForDriver_182EE3E3())
		return SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
	if (size == 0)
		return 0;
	if ((SceUInt32)dst - (SceUInt32)src < size || (SceUInt32)src - (SceUInt32)dst < size)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	memset(&op, 0, __builtin_offsetof(DmaOp, lock));
	op.lock = 0;
	op.magic = DMA_OP_MAGIC;
	evfId = sceKernelCreateEventFlag("SceDmacMemcpy", 0, 0, NULL);
	op.evfId = evfId;
	if (evfId < 0) {
		SCE_DBG_PRINTF_FUNC(0xb, 515, "sceKernelDmacMemcpy",
				    "Open event flag failed. 0x%08x\n", evfId);
		return evfId;
	}
	op.status = DMA_OP_ALLOCATED;
	op.name = "SceDmacMemcpy";
	op.lock = 0;
	return dmacMemcpySub(&op, dst, src, size);
}

/* fill in pieces of at most 16 MiB - 1, the most a tag can hold */
static inline int dmacMemsetSub(DmaOp *op, SceKernelDmaOpChainParam *param,
				SceKernelDmaOpTag *tag, SceKernelDmacId dmacId, void *dst,
				SceSize size)
{
	SceKernelDmaOpId opid = getDmaOpId(op);
	SceSize n;
	int res;

	if (tag->cmd & SCE_KERNEL_DMAC_CMD_COHERENT_DST)
		sceKernelSysrootDisableAutoClockDown();
	for (;;) {
		n = size > DMAC_LEN_MASK - 1 ? DMAC_LEN_MASK : size;
		tag->dst = dst;
		tag->len = n;
		res = sceKernelDmaOpAssign(opid, dmacId, 0);
		if (res < 0) {
			SCE_DBG_PRINTF_FUNC(0xb, 576, "dmacMemsetSub",
					    "Assign dmac operation failed. 0x%08x\n", res);
			break;
		}
		res = sceKernelDmaOpSetupChain(opid, tag, param, SCE_KERNEL_DMA_OP_VIRTUAL_DST_ADDR);
		if (res < 0) {
			SCE_DBG_PRINTF_FUNC(0xb, 582, "dmacMemsetSub",
					    "Setup chain failed. 0x%08x\n", res);
			break;
		}
		res = sceKernelDmaOpEnQueue(opid);
		if (res < 0) {
			SCE_DBG_PRINTF_FUNC(0xb, 588, "dmacMemsetSub",
					    "Enqueue operation failed. 0x%08x\n", res);
			break;
		}
		res = sceKernelWaitEventFlag(op->evfId, 1,
					      SCE_KERNEL_EVF_WAITMODE_OR |
					      SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT,
					      NULL, NULL);
		if (res < 0)
			break;
		size -= n;
		if (size == 0)
			break;
		dst = (char *)dst + n;
	}
	sceKernelDeleteEventFlag(op->evfId);
	/* wait until the interrupt handler is done with the operation */
	SceCpuForDriver_BF82DEB2(&op->lock);
	if (tag->cmd & SCE_KERNEL_DMAC_CMD_COHERENT_DST)
		sceKernelSysrootEnableAutoClockDown();
	return res;
}

int sceKernelDmacMemset(void *dst, int c, SceSize size)
{
	SceKernelDmaOpChainParam param;
	DmaOp op;
	SceKernelDmaOpTag tag;
	SceKernelDmacId dmacId;
	SceUID evfId;

	if (SceIntrmgrForDriver_182EE3E3())
		return SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
	if (size == 0)
		return 0;
	dmacId = dmacForAddress(dst);
	param.setValue = (c & 0xff) * 0x01010101;
	param.coherencyMask = 0x3e600;
	tag.cmd = SCE_KERNEL_DMAC_CMD_OP_SET | 0x3000;
	if (!isUncacheAddress(dst))
		tag.cmd |= SCE_KERNEL_DMAC_CMD_COHERENT_DST;
	memset(&op, 0, __builtin_offsetof(DmaOp, magic));
	op.magic = DMA_OP_MAGIC;
	tag.blockSize = 0;
	tag.pNext = SCE_KERNEL_DMAC_CHAIN_END;
	param.size = sizeof(param);
	evfId = sceKernelCreateEventFlag("SceDmacMemset", 0, 0, NULL);
	op.evfId = evfId;
	if (evfId < 0) {
		SCE_DBG_PRINTF_FUNC(0xb, 644, "sceKernelDmacMemset",
				    "Open event flag failed. 0x%08x\n", evfId);
		return evfId;
	}
	op.name = "SceDmacMemset";
	op.status = DMA_OP_ALLOCATED;
	op.lock = 0;
	return dmacMemsetSub(&op, &param, &tag, dmacId, dst, size);
}

int sceKernelDmaOpDeQueue(SceKernelDmaOpId opid)
{
	DmaOp *op = getDmaOp(opid);
	SceKernelIntrStatus intr;

	if (op->magic != DMA_OP_MAGIC) {
		SCE_DBG_PRINTF_FUNC(0xb, 1539, "sceKernelDmaOpDeQueue",
				    "invalid magic number. opid 0x%08x, magic 0x%08x\n",
			       opid, op->magic);
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	}
	intr = SceCpuForDriver_D32ACE9E(&op->lock);
	if (!(op->status & DMA_OP_ALLOCATED)) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&op->lock, intr);
		return SCE_KERNEL_ERROR_NOT_INITIALIZED;
	}
	if (!(op->status & DMA_OP_QUEUED)) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&op->lock, intr);
		return SCE_KERNEL_ERROR_NOT_QUEUED;
	}
	if (op->channel != NULL) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&op->lock, intr);
		return SCE_KERNEL_ERROR_ON_TRANSFERRING;
	}
	RemoveQueuedDmaOperation(op);
	sceKernelSpinlockLowUnlockCpuResumeIntr(&op->lock, intr);
	return 0;
}

int SceDmacmgrForDriver_893FCB80(void)
{
	return SCE_KERNEL_ERROR_NOT_IMPLEMENTED;
}

int sceKernelDmaOpSync(SceKernelDmaOpId opid, SceKernelDmaOpSyncMode syncMode,
			SceUInt32 timeout, const SceKernelDmaOpTag **ppErrorTag)
{
	DmaOp *op = getDmaOp(opid);
	SceKernelIntrStatus intr;
	SceUInt16 status;
	SceUID evfId;
	int res;

	if (op->magic != DMA_OP_MAGIC) {
		SCE_DBG_PRINTF_FUNC(0xb, 1586, "sceKernelDmaOpSync",
				    "invalid magic number. opid 0x%08x, magic 0x%08x\n",
			       opid, op->magic);
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	}
	intr = SceCpuForDriver_D32ACE9E(&op->lock);
	if (!(op->status & DMA_OP_ALLOCATED)) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&op->lock, intr);
		return SCE_KERNEL_ERROR_NOT_INITIALIZED;
	}
	if (op->head == NULL && op->channel == NULL && !(op->status & DMA_OP_DONE)) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&op->lock, intr);
		return SCE_KERNEL_ERROR_NOT_SETUP;
	}
	if (op->status & DMA_OP_CANCELING) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&op->lock, intr);
		return SCE_KERNEL_ERROR_CANCELING;
	}
	status = op->status;
	evfId = op->evfId;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&op->lock, intr);
	switch (syncMode) {
	case SCE_KERNEL_DMA_OP_SYNC_POLL:
		if (!(status & DMA_OP_DONE))
			return 1;
		res = sceKernelPollEventFlag(evfId, 1,
					      SCE_KERNEL_EVF_WAITMODE_OR |
					      SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT,
					      NULL);
		break;
	case SCE_KERNEL_DMA_OP_SYNC_WAIT:
		res = sceKernelWaitEventFlag(evfId, 1,
					      SCE_KERNEL_EVF_WAITMODE_OR |
					      SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT,
					      NULL, NULL);
		break;
	case SCE_KERNEL_DMA_OP_SYNC_TIMED_WAIT:
		res = sceKernelWaitEventFlag(evfId, 1,
					      SCE_KERNEL_EVF_WAITMODE_OR |
					      SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT,
					      NULL, &timeout);
		break;
	default:
		SCE_DBG_PRINTF_FUNC(0xb, 1648, "sceKernelDmaOpSync",
				    "Unknown op sync mode. 0x%x\n", syncMode);
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	}
	if (res < 0)
		return res;
	if (ppErrorTag != NULL) {
		intr = SceCpuForDriver_D32ACE9E(&op->lock);
		if (op->magic == DMA_OP_MAGIC)
			*ppErrorTag = op->errorTag;
		sceKernelSpinlockLowUnlockCpuResumeIntr(&op->lock, intr);
	}
	return 0;
}

int sceKernelDmaOpQuit(SceKernelDmaOpId opid)
{
	DmaOp *op = getDmaOp(opid);
	SceKernelIntrStatus intr;
	int retry;

	if (op->magic != DMA_OP_MAGIC) {
		SCE_DBG_PRINTF_FUNC(0xb, 1671, "sceKernelDmaOpQuit",
				    "invalid magic number. opid 0x%08x, magic 0x%08x\n",
			       opid, op->magic);
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	}
	intr = SceCpuForDriver_D32ACE9E(&op->lock);
	if (!(op->status & DMA_OP_ALLOCATED)) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&op->lock, intr);
		return SCE_KERNEL_ERROR_NOT_INITIALIZED;
	}
	if (op->status & DMA_OP_CANCELING) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&op->lock, intr);
		return SCE_KERNEL_ERROR_CANCELING;
	}
	if (op->channel != NULL) {
		op->head->tag.pNext = SCE_KERNEL_DMAC_CHAIN_END;
		_CancelDmaTransfer(op);
		if (op->status & DMA_OP_CANCELING) {
			/* the interrupt handler finishes the cancel */
			sceKernelSpinlockLowUnlockCpuResumeIntr(&op->lock, intr);
			for (retry = 5; ; ) {
				if (!SceIntrmgrForDriver_182EE3E3() && intr != 0)
					sceKernelDelayThread(10);
				intr = SceCpuForDriver_D32ACE9E(&op->lock);
				if (!(op->status & DMA_OP_CANCELING))
					break;
				sceKernelSpinlockLowUnlockCpuResumeIntr(&op->lock, intr);
				if (--retry == 0)
					return SCE_ERROR_ERRNO_EBUSY;
			}
		}
		op->status |= DMA_OP_DONE;
	}
	if (op->head != NULL) {
		ReleaseDmaTagChain(op->head, op->tail);
		op->head = NULL;
		op->tail = NULL;
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&op->lock, intr);
	return 0;
}

/* release the interrupts of a DMAC's channels (0xffff: all of them) */
int SceDmacmgrForDriver_BCBB5E85(int dmac, SceUInt32 channel)
{
	DmaController *ctrl;
	DmaChannel *first;
	SceKernelIntrStatus intr;
	int intrCode;
	int i;

	switch (dmac) {
	case 0:
		intrCode = 0x70;
		first = &s_dmacMgr.dmac01Channel[0];
		ctrl = &s_dmacMgr.dmac01;
		break;
	case 1:
		intrCode = 0x72;
		first = &s_dmacMgr.dmac01Channel[1];
		ctrl = &s_dmacMgr.dmac01;
		break;
	case 2:
		intrCode = 0x74;
		first = &s_dmacMgr.dmac23Channel[0];
		ctrl = &s_dmacMgr.dmac23;
		break;
	case 3:
		intrCode = 0x76;
		first = &s_dmacMgr.dmac23Channel[1];
		ctrl = &s_dmacMgr.dmac23;
		break;
	case 4:
		if (channel != 0xffff && channel > 15)
			return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
		if (channel == 0xffff) {
			for (i = 0; i < 16; i++)
				ReleaseDmac4Channel(i);
		} else {
			ReleaseDmac4Channel(channel);
		}
		return 0;
	case 5:
		intrCode = 0x7c;
		first = &s_dmacMgr.dmac5Channel[0];
		ctrl = &s_dmacMgr.dmac5;
		break;
	case 6:
		if (s_dmacMgr.dmac6Reg == NULL)
			return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
		first = &s_dmacMgr.dmac6Channel[0];
		intrCode = 0x7e;
		ctrl = &s_dmacMgr.dmac6;
		break;
	default:
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	}
	if (channel > 1 && channel != 0xffff)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	intr = SceCpuForDriver_D32ACE9E(&ctrl->lock);
	if (channel == 0xffff) {
		ReleaseDmacChannel(intrCode, ctrl, first);
		ReleaseDmacChannel(intrCode + 1, ctrl, first + 2);
	} else {
		ReleaseDmacChannel(intrCode + channel, ctrl, first + channel * 2);
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&ctrl->lock, intr);
	return 0;
}

static const DmacIntr s_dmacIntr[16] = {
	{ 0x70, "SceDmacmgrDmac0Ch0", &s_dmacMgr.dmac01Channel[0] },
	{ 0x71, "SceDmacmgrDmac0Ch1", &s_dmacMgr.dmac01Channel[2] },
	{ 0x72, "SceDmacmgrDmac1Ch0", &s_dmacMgr.dmac01Channel[1] },
	{ 0x73, "SceDmacmgrDmac1Ch1", &s_dmacMgr.dmac01Channel[3] },
	{ 0x74, "SceDmacmgrDmac2Ch0", &s_dmacMgr.dmac23Channel[0] },
	{ 0x75, "SceDmacmgrDmac2Ch1", &s_dmacMgr.dmac23Channel[2] },
	{ 0x76, "SceDmacmgrDmac3Ch0", &s_dmacMgr.dmac23Channel[1] },
	{ 0x77, "SceDmacmgrDmac3Ch1", &s_dmacMgr.dmac23Channel[3] },
	{ 0x7c, "SceDmacmgrDmac5Ch0", &s_dmacMgr.dmac5Channel[0] },
	{ 0x7d, "SceDmacmgrDmac5Ch1", &s_dmacMgr.dmac5Channel[1] },
	{ 0x7e, "SceDmacmgrDmac6Ch0", &s_dmacMgr.dmac6Channel[0] },
	{ 0x7f, "SceDmacmgrDmac6Ch1", &s_dmacMgr.dmac6Channel[1] },
	{ 0x78, "SceDmacmgrDmac4Gate0", NULL },
	{ 0x79, "SceDmacmgrDmac4Gate1", NULL },
	{ 0x7a, "SceDmacmgrDmac4Gate2", NULL },
	{ 0x7b, "SceDmacmgrDmac4Gate3", NULL },
};

/* DMAC6 exists only on TOOL units */
static inline int isDmac6Intr(int intrCode)
{
	return intrCode == 0x7e || intrCode == 0x7f;
}

static inline void InitializeTagCache(void)
{
	DmaTag *tag;

	for (tag = s_tagCache; tag != &s_tagCache[64]; tag++) {
		int res = SceSysmemForDriver_8D160E65(&tag->tag, &tag->pa);

		tag->pa |= 1;
		if (res < 0)
			SCE_PANIC_FUNC(2083, "InitializeTagCache");
		tag->cached = 1;
		tag->next = tag + 1;
	}
	s_tagCache[63].next = NULL;
	s_dmacMgr.freeTag = s_tagCache;
}

int module_start(SceSize args, void *argp)
{
	SceKernelHeapCreateOpt heapOpt;
	volatile SceDmac4Reg *reg4;
	SceKernelIntrHandler handler;
	void *arg;
	int intrCode, targetCpu, res, i;
	char *name;

	s_dmacMgr.dmac0Reg = mapDmac("SceDmacmgrDmac0Reg", DMAC0_REG_BASE);
	s_dmacMgr.dmac1Reg = mapDmac("SceDmacmgrDmac1Reg", DMAC1_REG_BASE);
	s_dmacMgr.dmac2Reg = mapDmac("SceDmacmgrDmac2Reg", DMAC2_REG_BASE);
	s_dmacMgr.dmac3Reg = mapDmac("SceDmacmgrDmac3Reg", DMAC3_REG_BASE);
	s_dmacMgr.dmac4Reg = mapDmac("SceDmacmgrDmac4Reg", DMAC4_REG_BASE);
	s_dmacMgr.dmac5Reg = mapDmac("SceDmacmgrDmac5Reg", DMAC5_REG_BASE);
	if (SceDipswForDriver_A98FC2FD(0x9f) || SceDipswForDriver_A98FC2FD(0xc0) ||
	    SceDipswForDriver_A98FC2FD(0xc1) || SceDipswForDriver_A98FC2FD(0xc2))
		s_dmacMgr.dmac6Reg = mapDmac("SceDmacmgrDmac6Reg", DMAC6_REG_BASE);
	else
		s_dmacMgr.dmac6Reg = NULL;
	SceKernelSuspendForDriver_CE7A2207(0x12, SuspendDmacmgr, NULL);
	SceKernelSuspendForDriver_105C5752(0x12, ResumeDmacmgr, NULL);

	heapOpt.size = sizeof(heapOpt);
	heapOpt.flags = HEAP_AUTO_EXTEND;
	res = SceSysmemForDriver_9328E0E8("SceKernelDmacMgr", 0x400, &heapOpt);
	if (res < 0) {
		SCE_DBG_PRINTF_FUNC(0xb, 2134, "StartDmacmgr", "Create Heap failed. 0x%08x\n", res);
		goto out;
	}
	s_dmacMgr.rangeHeapId = res;
	InitializeTagCache();
	s_dmacMgr.objectHeap = SceSysmemForKernel_3717C322(0x40);
	SCE_DBG_ASSERT_CHECK_FUNC(s_dmacMgr.objectHeap != NULL, 2148, "StartDmacmgr");

	s_dmacMgr.dmac01.lock = 0;
	s_dmacMgr.dmac01.head = NULL;
	s_dmacMgr.dmac01.tail = NULL;
	s_dmacMgr.dmac23.head = NULL;
	s_dmacMgr.dmac23.tail = NULL;
	s_dmacMgr.dmac23.lock = 0;
	for (i = 0; i < 16; i++) {
		s_dmacMgr.dmac4[i].dmac.head = NULL;
		s_dmacMgr.dmac4[i].dmac.tail = NULL;
		s_dmacMgr.dmac4[i].dmac.lock = 0;
	}
	s_dmacMgr.dmac5.lock = 0;
	s_dmacMgr.dmac5.head = NULL;
	s_dmacMgr.dmac5.tail = NULL;
	s_dmacMgr.dmac6.head = NULL;
	s_dmacMgr.dmac6.tail = NULL;
	s_dmacMgr.dmac6.lock = 0;
	s_dmacMgr.dmac01Channel[0].op = NULL;
	s_dmacMgr.dmac01Channel[1].op = NULL;
	s_dmacMgr.dmac23Channel[0].op = NULL;
	s_dmacMgr.dmac23Channel[1].op = NULL;
	s_dmacMgr.dmac5Channel[0].op = NULL;
	s_dmacMgr.dmac01Channel[0].reg = s_dmacMgr.dmac0Reg;
	s_dmacMgr.dmac01Channel[1].reg = s_dmacMgr.dmac1Reg;
	s_dmacMgr.dmac23Channel[0].reg = s_dmacMgr.dmac2Reg;
	s_dmacMgr.dmac23Channel[1].reg = s_dmacMgr.dmac3Reg;
	s_dmacMgr.dmac5Channel[0].reg = s_dmacMgr.dmac5Reg;
	if (s_dmacMgr.dmac6Reg != NULL) {
		s_dmacMgr.dmac6Channel[0].reg = s_dmacMgr.dmac6Reg;
		s_dmacMgr.dmac6Channel[0].op = NULL;
	}
	s_dmacMgr.dmac01Channel[2].reg = s_dmacMgr.dmac0Reg + 1;
	s_dmacMgr.dmac01Channel[3].reg = s_dmacMgr.dmac1Reg + 1;
	s_dmacMgr.dmac23Channel[2].reg = s_dmacMgr.dmac2Reg + 1;
	s_dmacMgr.dmac23Channel[3].reg = s_dmacMgr.dmac3Reg + 1;
	s_dmacMgr.dmac5Channel[1].reg = s_dmacMgr.dmac5Reg + 1;
	s_dmacMgr.dmac01Channel[2].op = NULL;
	s_dmacMgr.dmac01Channel[3].op = NULL;
	s_dmacMgr.dmac23Channel[2].op = NULL;
	s_dmacMgr.dmac23Channel[3].op = NULL;
	s_dmacMgr.dmac5Channel[1].op = NULL;
	if (s_dmacMgr.dmac6Reg != NULL) {
		s_dmacMgr.dmac6Channel[1].reg = s_dmacMgr.dmac6Reg + 1;
		s_dmacMgr.dmac6Channel[1].op = NULL;
	}
	s_dmacMgr.dmac01.intrChannels = 0xf0000000;
	s_dmacMgr.dmac01.freeChannels = 0xf0000000;
	s_dmacMgr.dmac23.freeChannels = 0xf0000000;
	s_dmacMgr.dmac23.intrChannels = 0xf0000000;
	s_dmacMgr.dmac5.freeChannels = 0xc0000000;
	s_dmacMgr.dmac5.intrChannels = 0xc0000000;
	s_dmacMgr.dmac6.freeChannels = 0xc0000000;
	s_dmacMgr.dmac6.intrChannels = 0xc0000000;
	for (i = 0; i < 16; i++) {
		s_dmacMgr.dmac4[i].channel.reg = &s_dmacMgr.dmac4Reg->ch[i];
		s_dmacMgr.dmac4[i].channel.op = NULL;
		s_dmacMgr.dmac4[i].dmac.freeChannels = 0x80000000;
		s_dmacMgr.dmac4[i].dmac.intrChannels = 0x80000000;
	}

	for (i = 0; i < 16; i++) {
		intrCode = s_dmacIntr[i].intrCode;
		if (s_dmacIntr[i].channel != NULL) {
			handler = DmacChannelIntrHandler;
			arg = s_dmacIntr[i].channel;
			targetCpu = 0xf;
		} else {
			handler = Dmac4IntrHandler;
			arg = (void *)&s_dmacMgr.dmac4Reg->gate[intrCode - 0x78].activeIntr;
			targetCpu = 0x80000;
		}
		if (s_dmacMgr.dmac6Reg == NULL && isDmac6Intr(intrCode))
			continue;
		res = sceKernelRegisterIntrHandler(intrCode, s_dmacIntr[i].name, 0, handler, arg,
						    0x80, targetCpu, NULL);
		if (res < 0) {
			SCE_DBG_PRINTF_FUNC(0xb, 2244, "StartDmacmgr",
					    "register interrupt handler failed. 0x%08x\n", res);
			while (i-- != 0) {
				intrCode = s_dmacIntr[i].intrCode;
				if (s_dmacMgr.dmac6Reg != NULL || !isDmac6Intr(intrCode))
					sceKernelReleaseIntrHandler(intrCode);
			}
			sceKernelDeleteHeap(s_dmacMgr.rangeHeapId);
			goto out;
		}
		sceKernelEnableIntr(intrCode);
	}
	reg4 = s_dmacMgr.dmac4Reg;
	reg4->gate[0].intrMask = 0xffff;
	reg4->gate[1].intrMask = 0;
	reg4->gate[2].intrMask = 0;
	reg4->gate[3].intrMask = 0;
out:
	SceSysmemForKernel_B628B46B("SceDmacMemset", 0, &name);
	SceSysmemForKernel_B628B46B("SceDmacMemcpy", 0, &name);
	return 0;
}

/* user entry: lock the range for writing in the caller's context, then fill */
int sceDmacMemset(void *dst, int c, SceSize size)
{
	SceUInt32 context = readTpidruro();
	int res;

	writeTpidruro(context << 16);
	res = sceKernelLockRangeWithMode(SCE_KERNEL_MEMORY_REF_PERM_KERN_W, dst, size);
	if (res >= 0) {
		res = sceKernelDmacMemset(dst, c, size);
		SceSysmemForDriver_22CBE925(SCE_KERNEL_MEMORY_REF_PERM_KERN_W, dst, size);
	}
	writeTpidruro(context);
	return res;
}

/* user entry: lock the source for reading and the destination for writing, then copy */
int sceDmacMemcpy(void *dst, const void *src, SceSize size)
{
	SceUInt32 context = readTpidruro();
	int res;

	writeTpidruro(context << 16);
	res = sceKernelLockRangeWithMode(SCE_KERNEL_MEMORY_REF_PERM_KERN_R, (void *)src, size);
	if (res >= 0) {
		res = sceKernelLockRangeWithMode(SCE_KERNEL_MEMORY_REF_PERM_KERN_W, dst, size);
		if (res >= 0) {
			res = sceKernelDmacMemcpy(dst, src, size);
			SceSysmemForDriver_22CBE925(SCE_KERNEL_MEMORY_REF_PERM_KERN_W, dst, size);
		}
		SceSysmemForDriver_22CBE925(SCE_KERNEL_MEMORY_REF_PERM_KERN_R, (void *)src, size);
	}
	writeTpidruro(context);
	return res;
}
