/*
 * SceDmacmgr (os0:kd/dmacmgr.skprx): SceDmacmgrForDriver, the DMA operations' exports, each
 * declared once with its definition as the prototype, and their records.
 */
#pragma once

#include <psp2kern/types.h>

/*
 * The DMA operations' records and constants. No Sony header or record names them; the names are
 * vitasdk's (psp2kern/kernel/dmac.h), the layouts those dmacmgr reads.
 */

/* the end of a tag chain (SceKernelDmaOpTag.pNext) */
#define SCE_KERNEL_DMAC_CHAIN_END		((SceKernelDmaOpTag *)0xFFFFFFFF)

/* SceKernelDmaOpTag.cmd: the operation */
#define SCE_KERNEL_DMAC_CMD_OP_COPY		0x00000000
#define SCE_KERNEL_DMAC_CMD_OP_HASH_SHA1	0x00000003
#define SCE_KERNEL_DMAC_CMD_OP_SET		0x0000000C
/* SceKernelDmaOpTag.cmd: L2-coherent source and destination accesses */
#define SCE_KERNEL_DMAC_CMD_COHERENT_SRC	0x01000000
#define SCE_KERNEL_DMAC_CMD_COHERENT_DST	0x02000000

/* SceKernelDmaOpTag.blockSize */
#define SCE_KERNEL_DMAC_BLOCKSIZE_SRC_SHIFT	0
#define SCE_KERNEL_DMAC_BLOCKSIZE_DST_SHIFT	16

/* a channel's status (the callback's stat) */
#define SCE_KERNEL_DMAC_STAT_BUSY			0x00000001
#define SCE_KERNEL_DMAC_STAT_ABORTED			0x00000002
#define SCE_KERNEL_DMAC_STAT_ERROR_READ			0x00010000
#define SCE_KERNEL_DMAC_STAT_ERROR_WRITE		0x00020000
#define SCE_KERNEL_DMAC_STAT_ERROR_ILLEGAL_CONFIG	0x00040000
#define SCE_KERNEL_DMAC_STAT_ERROR_TAG			0x00080000
#define SCE_KERNEL_DMAC_STAT_ERROR_ZERO_BYTE		0x00100000

/* SceKernelDmaOpChainParam.coherencyMask: source bits 0-8, destination bits 9-17 */
#define SCE_KERNEL_DMAC_COHERENCY_MSK_DST_SHIFT		9
#define SCE_KERNEL_DMAC_COHERENCY_MSK_SRC_DST_MASK	0x0003FFFF
#define SCE_KERNEL_DMAC_COHERENCY_MSK_UNK_MASK		0x07FC0000	/* dmacmgr's own: the tags' reads */

/* an operation (sceKernelDmaOpAlloc) */
typedef SceInt32 SceKernelDmaOpId;

/* sceKernelDmaOpAssign's controllers */
typedef enum SceKernelDmacId {
	SCE_KERNEL_DMAC_ID_DMAC01	= 0x10,
	SCE_KERNEL_DMAC_ID_DMAC23	= 0x11,
	SCE_KERNEL_DMAC_ID_DMAC4	= 0x12,
	SCE_KERNEL_DMAC_ID_DMAC5	= 0x13,
	SCE_KERNEL_DMAC_ID_DMAC6	= 0x14	/* development kits */
} SceKernelDmacId;

/* the setup functions' flag: which addresses are virtual, a whole chain given */
typedef enum SceKernelDmaOpFlag {
	SCE_KERNEL_DMA_OP_PHYSICAL_ADDR		= 0x000,
	SCE_KERNEL_DMA_OP_VIRTUAL_SRC_ADDR	= 0x001,
	SCE_KERNEL_DMA_OP_VIRTUAL_DST_ADDR	= 0x010,
	SCE_KERNEL_DMA_OP_VIRTUAL_ADDR		= 0x011,
	SCE_KERNEL_DMA_OP_COMPLETE_CHAIN	= 0x100
} SceKernelDmaOpFlag;

/* sceKernelDmaOpSync's modes */
typedef enum SceKernelDmaOpSyncMode {
	SCE_KERNEL_DMA_OP_SYNC_POLL		= 0x1,
	SCE_KERNEL_DMA_OP_SYNC_WAIT		= 0x2,
	SCE_KERNEL_DMA_OP_SYNC_TIMED_WAIT	= 0x3
} SceKernelDmaOpSyncMode;

/* one transfer of a chain, as the controller reads it */
typedef struct SceKernelDmaOpTag {
	const void *src;
	void *dst;
	SceUInt32 len;
	SceUInt32 cmd;				/* SCE_KERNEL_DMAC_CMD_* */
	SceUInt32 keyring;
	void *iv;				/* physical */
	SceUInt32 blockSize;			/* SCE_KERNEL_DMAC_BLOCKSIZE_* */
	struct SceKernelDmaOpTag *pNext;	/* SCE_KERNEL_DMAC_CHAIN_END ends the chain */
} SceKernelDmaOpTag;

/* a cipher's key and IV */
typedef struct SceKernelDmaOpEncDec {
	SceUInt32 keyring;
	void *iv;				/* physical */
	SceUInt32 ivCoherencyMask;
	SceUInt32 reserved;
	SceUInt8 key[0x40];
} SceKernelDmaOpEncDec;

/* sceKernelDmaOpSetupChain's parameters */
typedef struct SceKernelDmaOpChainParam {
	SceSize size;				/* sizeof the parameters */
	SceUInt32 coherencyMask;
	SceUInt32 setValue;			/* a byte repeated four times */
} SceKernelDmaOpChainParam;

typedef struct SceKernelDmaOpEncDecChainParam {
	SceKernelDmaOpChainParam header;	/* header.size: sizeof(SceKernelDmaOpEncDecChainParam) */
	SceKernelDmaOpEncDec encDec;
} SceKernelDmaOpEncDecChainParam;

/* _DmaOpSetupDirect's parameters: one transfer */
typedef struct SceKernelDmaOpDirectParam {
	const void *src;
	void *dst;
	SceUInt32 len;
	SceUInt32 cmd;				/* SCE_KERNEL_DMAC_CMD_* */
	SceUInt32 blockSize;
	SceUInt32 coherencyMask;
	SceUInt32 setValue;			/* a byte repeated four times */
	SceKernelDmaOpEncDec encDec;
} SceKernelDmaOpDirectParam;

_Static_assert(sizeof(SceKernelDmaOpTag) == 0x20, "SceKernelDmaOpTag");
_Static_assert(sizeof(SceKernelDmaOpEncDec) == 0x50, "SceKernelDmaOpEncDec");
_Static_assert(sizeof(SceKernelDmaOpChainParam) == 0xc, "SceKernelDmaOpChainParam");
_Static_assert(sizeof(SceKernelDmaOpEncDecChainParam) == 0x5c, "SceKernelDmaOpEncDecChainParam");
_Static_assert(sizeof(SceKernelDmaOpDirectParam) == 0x6c, "SceKernelDmaOpDirectParam");

/* DMA completion callbacks, including the completed byte count. */
typedef int (*DmaOpCallback)(SceKernelDmaOpId opid, SceUInt32 stat,
	void *pUserData, const SceKernelDmaOpTag *pTag, SceSize completed);

int sceKernelDmaOpSetCallback(SceKernelDmaOpId opid, DmaOpCallback callback,
	void *pUserData);

int SceDmacmgrForDriver_893FCB80(void);
int SceDmacmgrForDriver_BCBB5E85(int dmac, SceUInt32 channel);
int _DmaOpSetupDirect(SceKernelDmaOpId opid, SceKernelDmaOpDirectParam *pParam, SceKernelDmaOpFlag flag);
int sceDmacMemcpy(void *dst, const void *src, SceSize size);
int sceDmacMemset(void *dst, int c, SceSize size);
SceKernelDmaOpId sceKernelDmaOpAlloc(const char *name);
int sceKernelDmaOpAssign(SceKernelDmaOpId opid, SceKernelDmacId dmacId, SceUInt32 channel);
int sceKernelDmaOpConcatenate(SceKernelDmaOpId opid, SceKernelDmaOpTag *pTag, SceKernelDmaOpFlag flag);
int sceKernelDmaOpDeQueue(SceKernelDmaOpId opid);
int sceKernelDmaOpEnQueue(SceKernelDmaOpId opid);
int sceKernelDmaOpFree(SceKernelDmaOpId opid);
int sceKernelDmaOpQuit(SceKernelDmaOpId opid);
int sceKernelDmaOpSetupChain(SceKernelDmaOpId opid, SceKernelDmaOpTag *pTag, SceKernelDmaOpChainParam *pParam, SceKernelDmaOpFlag flag);
int sceKernelDmaOpSync(SceKernelDmaOpId opid, SceKernelDmaOpSyncMode syncMode, SceUInt32 timeout, const SceKernelDmaOpTag **ppErrorTag);
int sceKernelDmacMemcpy(void *dst, const void *src, SceSize size);
int sceKernelDmacMemset(void *dst, int c, SceSize size);
