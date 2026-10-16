/*
 * SceDeci4pPamp: performance analyser control and trace transport.
 * Only PampModuleStart and scePerfArmPmonOpen have recovered Sony names;
 * the remaining internal names describe their observed responsibilities.
 */
#include <psp2kern/kernel/dipsw.h>
#include <psp2kern/kernel/dmac.h>
#include <psp2kern/kernel/sysclib.h>
#include "sce/sysmem.h"
#include <psp2kern/kernel/sysmem/heap.h>
#include "sce/debug.h"
#include "sce/error.h"
#include "sce/processmgr.h"
#include "sce/sysroot.h"
#include "sce/threadmgr.h"
#include "pamp.h"

static PampContext s_pamp;

#define PAMP_MEMTYPE		SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_TOOL_RW	/* kernel read/write: heap and thread stacks */

#if FW_DEBUG
#define PAMP_OPEN_PRINTF(_line, _fmt, ...) \
	ksceKernelPrintfLevelWithInfo(1, 0, \
		SCE_DBGINFO_FUNC(_line, "scePerfArmPmonOpen"), _fmt, ##__VA_ARGS__)
#else
#define PAMP_OPEN_PRINTF(_line, _fmt, ...) do { } while (0)
#endif

/* name unknown */
static void pampZfree(void *opaque, void *address)
{
	ksceKernelFreeHeapMemory((SceUID)opaque, address);
}

/* name unknown */
static void *pampZalloc(void *opaque, SceUInt32 items, SceUInt32 size)
{
	return ksceKernelAllocHeapMemory((SceUID)opaque, items * size);
}

/* name unknown; overlap is rejected even though the operation is a copy. */
static int pampDmacMemcpy(void *dst, const void *src, SceUInt32 size)
{
	SceKernelDmaOpDirectParam param;
	int op, ret;

	if ((SceUInt32)((SceUIntPtr)dst - (SceUIntPtr)src) < size ||
	    (SceUInt32)((SceUIntPtr)src - (SceUIntPtr)dst) < size)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	op = ksceKernelDmaOpAlloc("pampDmacMemcpy");
	if (op < 0)
		return op;
	ret = ksceKernelDmaOpAssign(op, SCE_KERNEL_DMAC_ID_DMAC01, 0);
	if (ret >= 0) {
		param.src = src;
		param.dst = dst;
		param.len = size;
		param.cmd = SCE_KERNEL_DMAC_CMD_COHERENT_DST | 0x3000 | SCE_KERNEL_DMAC_CMD_OP_COPY;
		param.blockSize = 0;
		param.coherencyMask = SCE_KERNEL_DMAC_COHERENCY_MSK_SRC_DST_MASK;
		ret = ksceKernelDmaOpSetupDirect(op, &param, SCE_KERNEL_DMA_OP_VIRTUAL_ADDR);
		if (ret >= 0)
			ret = ksceKernelDmaOpEnQueue(op);
		if (ret >= 0)
			ret = ksceKernelDmaOpSync(op, SCE_KERNEL_DMA_OP_SYNC_WAIT, 0, NULL);
		if (ret >= 0) {
			ret = ksceKernelDmaOpFree(op);
			return ret < 0 ? ret : (int)size;
		}
	}
	ksceKernelDmaOpFree(op);
	return ret;
}

/* name unknown; DMA errors are deliberately ignored by the original. */
static int pampReadTrace(PampContext *ctx, void *dst, int unit,
			 SceUInt32 offset, SceUInt32 size)
{
	PampBuffer *buffer = &ctx->trace[unit];
	SceUInt32 wrapped = ctx->flags & (unit ? 0x10 : 8);
	SceUInt32 cursor = unit ? ScePamgrForDriver_80CA3B39() :
		ScePamgrForDriver_FB46C3CD();
	SceUInt32 physical = buffer->physical;
	SceUInt32 available, position, first;

	if (cursor < physical)
		return PAMP_STATUS_OUT_OF_RANGE;
	available = buffer->size;
	if (cursor >= physical + available)
		return PAMP_STATUS_OUT_OF_RANGE;
	if (buffer->circular == 1) {
		if (!wrapped) {
			if (offset + size > cursor - buffer->physical)
				return PAMP_STATUS_OUT_OF_RANGE;
			if (size)
				pampDmacMemcpy(dst, buffer->base + offset, size);
		} else {
			if (offset + size > available)
				return PAMP_STATUS_OUT_OF_RANGE;
			position = cursor - buffer->physical + offset;
			if (position < available && position + size > available) {
				first = available - position;
				if (first)
					pampDmacMemcpy(dst, buffer->base + position, first);
				if (size != first)
					pampDmacMemcpy((SceUInt8 *)dst + first, buffer->base,
						       size - first);
			} else if (size) {
				if (position >= available)
					position -= available;
				pampDmacMemcpy(dst, buffer->base + position, size);
			}
		}
	} else {
		if (!wrapped)
			available = cursor - buffer->physical;
		if (offset + size > available)
			return PAMP_STATUS_OUT_OF_RANGE;
		if (size)
			pampDmacMemcpy(dst, buffer->base + offset, size);
	}
	return 0;
}

/* name unknown */
static int pampProtocolError(int ret)
{
	switch ((SceUInt32)ret) {
	case 0: return 0;
	case SCE_KERNEL_ERROR_PA_NOT_AVAILABLE: return PAMP_STATUS_NOT_AVAILABLE;
	case SCE_KERNEL_ERROR_INVALID_ARGUMENT: return PAMP_STATUS_INVALID_ARGUMENT;
	case SCE_KERNEL_ERROR_NOT_INITIALIZED: return PAMP_STATUS_NOT_OPEN;
	default: return PAMP_STATUS_ERROR;
	}
}

/* name unknown */
static inline __attribute__((always_inline)) PampPacket *pampReserveBuffer(PampContext *ctx)
{
	unsigned index, bit;

	if (ksceKernelWaitSema(ctx->bufferSema, 1, NULL) < 0 ||
	    ksceKernelLockFastMutex(&ctx->controlLock) < 0)
		return NULL;
	if (!(ctx->used & 1)) {
		index = 0;
		bit = 1;
	} else {
		if (ctx->used & 2)
			return NULL;
		index = 1;
		bit = 2;
	}
	ctx->used |= bit;
	if (ksceKernelUnlockFastMutex(&ctx->controlLock) < 0)
		return NULL;
	return &ctx->buffers[index];
}

/* name unknown */
static inline __attribute__((always_inline)) void pampReleaseBuffer(PampContext *ctx, PampPacket *packet)
{
	unsigned mask;

	if (ksceKernelLockFastMutex(&ctx->controlLock) < 0)
		return;
	if (packet == &ctx->buffers[0])
		mask = ~1u;
	else if (packet == &ctx->buffers[1])
		mask = ~2u;
	else {
		ksceKernelUnlockFastMutex(&ctx->controlLock);
		return;
	}
	ctx->used &= mask;
	if (ksceKernelUnlockFastMutex(&ctx->controlLock) >= 0)
		ksceKernelSignalSema(ctx->bufferSema, 1);
}

/* name unknown */
static void pampPaCallback(int flags, PampContext *ctx)
{
	if (ctx && (flags & 1))
		ksceKernelSetEventFlag(ctx->event, 4);
}

/* name unknown */
static int pampNotify(PampContext *ctx, SceUInt32 command, SceUInt32 value)
{
	PampPacket *packet;
	int ret;

	if (ksceKernelLockFastMutex(&ctx->controlLock) < 0)
		return PAMP_STATUS_INTERNAL_ERROR;
	if (command == PAMP_NOTIFY_STARTED) {
		if (ctx->state == PAMP_STATE_TRACING) {
			ret = PAMP_STATUS_ALREADY_TRACING;
			goto out;
		}
		if (ctx->state != PAMP_STATE_OPEN && ctx->state != PAMP_STATE_STOPPED) {
			ret = PAMP_STATUS_WRONG_STATE;
			goto out;
		}
		ctx->state = PAMP_STATE_TRACING;
		if (ctx->traced) {
			ctx->flags = 0;
			ScePamgrForDriver_D9EFF812(ctx->key, ctx->trace[0].physical);
			ScePamgrForDriver_95F4079E(ctx->key, ctx->trace[1].physical);
			ctx->traced = 0;
		}
	} else {
		if (ctx->state != PAMP_STATE_TRACING) {
			ret = PAMP_STATUS_NOT_TRACING;
			goto out;
		}
		ctx->state = PAMP_STATE_STOPPED;
	}
	packet = pampReserveBuffer(ctx);
	ctx->notification = packet;
	packet->data[0] = 0;
	packet->data[1] = 0;
	packet->data[2] = 0;
	packet->data[3] = 0;
	packet->command = command;
	packet->sequence = ++ctx->sequence;
	packet->length = 4;
	packet->status = 0;
	packet->words[0] = value;
	SceDeci4pDfMgrForDebugger_6D26CC56(&packet->hdr, 0x24);
	ret = ksceKernelWaitSema(ctx->sendSema, 1, NULL);
	if (ret >= 0) {
		ctx->sendPacket = ctx->notification;
		ret = sceDfMgrReqSend(ctx->handle);
		if (ret >= 0)
			return ksceKernelUnlockFastMutex(&ctx->controlLock) < 0 ? PAMP_STATUS_INTERNAL_ERROR : 0;
	}
out:
	ksceKernelUnlockFastMutex(&ctx->controlLock);
	return ret;
}

/* name unknown */
static int pampConfigure(PampContext *ctx)
{
	AddressSpaceInfo process;
	SceUInt32 config[18];
	SceUInt32 *data = ctx->request.words;
	SceUInt32 type = ctx->request.status;
	int uid, ret;

	if (ctx->state == PAMP_STATE_TRACING)
		return PAMP_STATUS_WRONG_STATE;
	if (type == 1) {
		if (ctx->request.length < 0x1c)
			return PAMP_STATUS_SHORT_REQUEST;
		config[0] = data[0];
		config[1] = data[1];
		config[2] = data[2];
		config[3] = data[3];
		config[4] = 0;
		config[5] = 0;
		if (data[4])
			return PAMP_STATUS_INVALID_ARGUMENT;
		config[5] = data[5];
		ret = ScePamgrForDriver_17C81A92(ctx->key, config);
	} else if (type == 0) {
		if (ctx->request.length < 0x14)
			return PAMP_STATUS_SHORT_REQUEST;
		if (data[1] - 5u >= 2)
			return PAMP_STATUS_INVALID_ARGUMENT;
		uid = SceProcessmgrForKernel_9BC44974(data[3]);
		if (uid < 0)
			return PAMP_STATUS_INVALID_PROCESS;
		process.size = sizeof(process);
		if (SceSysmemForKernel_C3EF4055(uid, &process))
			return PAMP_STATUS_INVALID_PROCESS;
		memset(config, 0, 0xc);
		config[2] = data[2];
		config[0] = data[0];
		config[1] = data[1] | ((SceUInt32)process.asid << 24);
		ret = ScePamgrForDriver_5D3D46E4(ctx->key, config);
	} else if (type == 2) {
		if (ctx->request.length < 0x4c)
			return PAMP_STATUS_SHORT_REQUEST;
		memset(config, 0, sizeof(config));
		for (unsigned i = 0; i < 18; i++)
			config[i] = data[i];
		ret = ScePamgrForDriver_49A43754(ctx->key, config);
	} else {
		return PAMP_STATUS_INVALID_ARGUMENT;
	}
	return pampProtocolError(ret);
}

/* name unknown */
static inline __attribute__((always_inline)) int pampCompress(PampContext *ctx, PampPacket *packet, int initial)
{
	SceUInt32 produced = 0, size;
	int ret, unit = ctx->traceUnit != 0;

	ctx->zstream.next_out = packet->chunk.bytes;
	ctx->zstream.avail_out = 0x10000;
	while (!ctx->zfinished) {
		if (!ctx->zstream.avail_in) {
			size = ctx->traceRemaining;
			if (size > 0x10000)
				size = 0x10000;
			pampReadTrace(ctx, ctx->scratch, unit, ctx->traceOffset, size);
			ctx->zstream.next_in = ctx->scratch;
			ctx->zstream.avail_in = size;
			ctx->traceOffset += size;
			ctx->traceRemaining -= size;
		}
		ret = SceZlibForDriver_5B718E55(&ctx->zstream,
				ctx->traceRemaining ? Z_NO_FLUSH : Z_FINISH);
		if ((unsigned)ret > 1) {
			if (initial)
				return PAMP_STATUS_INTERNAL_ERROR;
			break;
		}
		if (ret == Z_STREAM_END)
			ctx->zfinished = 1;
		produced = 0x10000 - ctx->zstream.avail_out;
		if (produced >= 0x10000)
			break;
	}
	packet->length = produced + 4;
	if (ctx->zfinished) {
		packet->chunk.more = 0;
		SceZlibForDriver_5492B3F2(&ctx->zstream);
	} else {
		packet->chunk.more = 1;
	}
	return 0;
}

/* name unknown */
static int pampOpen(PampContext *ctx)
{
	uintptr_t windowBase;
	SceSize windowSize;
	int pids[16], count = 0, ret, key;

	memset(pids, 0, sizeof(pids));
	ret = SceProcessmgrForKernel_EFFF2975(3, pids, sizeof(pids), &count);
	if (ret < 0)
		return PAMP_STATUS_NOT_AVAILABLE;
	for (int i = 0; i < count; i++) {
		if (SceSysrootForDriver_39F1FD07(pids[i]) != 1)
			continue;
		ret = SceProcessmgrForDriver_61B9B6FA(pids[i], 0x11);
		if (ret) {
			PAMP_OPEN_PRINTF(1280,
				"[Error] sceKernelSetInitialPMCR (ret = 0x%08x)\n", ret);
			return PAMP_STATUS_NOT_AVAILABLE;
		}
		ret = sceKernelPMonSetControlRegister(pids[i], 0x11);
		if (ret) {
			PAMP_OPEN_PRINTF(1286,
				"[Error] sceKernelPMonSetControlRegister (ret = 0x%08x)\n", ret);
			return PAMP_STATUS_NOT_AVAILABLE;
		}
		ret = SceProcessmgrForDriver_B1C3EFCA(pids[i], 1);
		if (ret) {
			PAMP_OPEN_PRINTF(1292,
				"[Error] sceKernelSetInitialPMUSERENR (ret = 0x%08x)\n", ret);
			return PAMP_STATUS_NOT_AVAILABLE;
		}
		ret = sceKernelPMonSetUserEnableRegister(pids[i], 1);
		if (ret) {
			PAMP_OPEN_PRINTF(1299,
				"[Error] sceKernelPMonSetUserEnableRegister (ret = 0x%08x)\n", ret);
			return PAMP_STATUS_NOT_AVAILABLE;
		}
	}
	key = sceKernelPaRegister();
	if (key < 0)
		return pampProtocolError(key);
	ctx->key = key;
	if (ksceKernelLockFastMutex(&ctx->controlLock) < 0) {
		ret = PAMP_STATUS_INTERNAL_ERROR;
		goto unregister;
	}
	if (ctx->state != PAMP_STATE_CLOSED && ctx->state != PAMP_STATE_STOPPED) {
		ret = ksceKernelUnlockFastMutex(&ctx->controlLock) < 0 ? PAMP_STATUS_INTERNAL_ERROR : PAMP_STATUS_WRONG_STATE;
		goto unregister;
	}
	ctx->state = PAMP_STATE_OPEN;
	if (ksceKernelUnlockFastMutex(&ctx->controlLock) < 0) {
		ret = PAMP_STATUS_INTERNAL_ERROR;
		goto unregister;
	}
	ctx->traceBase = (void *)0xc1000000;
	SceSysmemForDriver_C9928F5E(&windowBase, &windowSize);
	if (!windowSize)
		return PAMP_STATUS_ERROR;
	ctx->flags = 0;
	ctx->traceSize = windowSize - 0x1000000;
	ctx->trace[0].allocated = 0;
	ctx->trace[0].base = ctx->traceBase;
	ret = ksceKernelVAtoPA(ctx->traceBase, &ctx->trace[0].physical);
	if (ret < 0)
		return PAMP_STATUS_ERROR;
	ctx->trace[0].size = 0;
	ctx->trace[1].allocated = 0;
	ctx->trace[1].base = ctx->traceBase;
	ret = ksceKernelVAtoPA(ctx->traceBase, &ctx->trace[1].physical);
	if (ret < 0)
		return PAMP_STATUS_ERROR;
	ctx->trace[1].size = 0;
	ctx->paConfig[0] = 0x1000c;
	ctx->paConfig[1] = ctx->trace[0].physical;
	ctx->paConfig[2] = 0;
	ctx->paConfig[3] = 0;
	ScePamgrForDriver_F2D5105A(ctx->key, ctx->paConfig);
	ctx->pmuSize = 0;
	ctx->pmuConfig[0] = 0;
	ctx->pmuAddress = ctx->trace[1].physical;
	ctx->pmuConfig[1] = 0;
	ScePamgrForDriver_E8FF4812(ctx->key, &ctx->pmuAddress);
	ScePamgrForDriver_D9EFF812(ctx->key, ctx->trace[0].physical);
	ScePamgrForDriver_95F4079E(ctx->key, ctx->trace[1].physical);
	ScePamgrForDriver_0B7F6F53(ctx->key);
	ScePamgrForDriver_B9EE1620(ctx->key);
	ScePamgrForDriver_81764B21(ctx->key);
	ScePamgrForDriver_DB62FEFE(ctx->key);
	ScePamgrForDriver_9935D8EF(ctx->key);
	ctx->streaming = 0;
	ctx->finalPacket = 0;
	return ScePamgrForDriver_3C12A0CD(ctx->key, (void (*)(int, void *))pampPaCallback, ctx) ? PAMP_STATUS_WRONG_STATE : 0;
unregister:
	ctx->key = -1;
	sceKernelPaUnregister(key);
	return ret;
}

/* name unknown */
static int pampClose(PampContext *ctx)
{
	if (ScePamgrForDriver_FB04BBEF(ctx->key))
		return PAMP_STATUS_WRONG_STATE;
	if (ksceKernelLockFastMutex(&ctx->controlLock) < 0)
		return PAMP_STATUS_INTERNAL_ERROR;
	if (ctx->state - 1u < 3) {
		ctx->state = PAMP_STATE_CLOSED;
		if (ksceKernelUnlockFastMutex(&ctx->controlLock) < 0)
			return PAMP_STATUS_INTERNAL_ERROR;
		ScePamgrForDriver_1A7733E0(ctx->key);
		sceKernelPaUnregister(ctx->key);
		ctx->key = -1;
		return 0;
	}
	return ksceKernelUnlockFastMutex(&ctx->controlLock) < 0 ? PAMP_STATUS_INTERNAL_ERROR : PAMP_STATUS_WRONG_STATE;
}

/* name unknown */
static int pampAllocateTrace(PampContext *ctx)
{
	SceUInt32 *data = ctx->request.words;
	SceUInt32 unit = ctx->request.status, available, size = data[1];
	PampBuffer *buffer, *other;
	int ret;

	if (ctx->state == PAMP_STATE_TRACING)
		return PAMP_STATUS_WRONG_STATE;
	if (unit >= 2)
		return PAMP_STATUS_INVALID_UNIT;
	if (data[0] >= 2 || (unit == 1 && data[0] != 1))
		return PAMP_STATUS_INVALID_MODE;
	other = &ctx->trace[unit ^ 1];
	available = ctx->traceSize;
	if (other->allocated)
		available -= other->size;
	if (available < size)
		return PAMP_STATUS_NO_SPACE;
	if (size & 0x7f)
		return PAMP_STATUS_INVALID_ALIGNMENT;
	if (data[3] - 0x10000u < 3)
		return PAMP_STATUS_UNSUPPORTED;
	if (data[3])
		return PAMP_STATUS_INVALID_DESTINATION;
	if (ctx->request.length < 0x14)
		return PAMP_STATUS_SHORT_REQUEST;
	buffer = &ctx->trace[unit];
	buffer->allocated = 1;
	buffer->base = ctx->traceBase;
	buffer->circular = data[0];
	buffer->size = size;
	buffer->available = 0;
	if (other->allocated)
		other->base = ctx->traceBase + size;
	if (ctx->trace[0].allocated) {
		ret = ksceKernelVAtoPA(ctx->trace[0].base, &ctx->trace[0].physical);
		if (ret < 0)
			return PAMP_STATUS_INVALID_ARGUMENT;
		ctx->paConfig[4] = data[2];
		ctx->paConfig[0] = ctx->trace[0].circular & 1 ?
			(ctx->paConfig[0] & ~2u) | 1 : (ctx->paConfig[0] & ~1u) | 2;
		ctx->paConfig[1] = ctx->trace[0].physical;
		ctx->paConfig[2] = ctx->trace[0].size;
		ctx->paConfig[3] = 0;
		ret = ScePamgrForDriver_F2D5105A(ctx->key, ctx->paConfig);
		if (ret)
			return pampProtocolError(ret);
	}
	if (ctx->trace[1].allocated) {
		ret = ksceKernelVAtoPA(ctx->trace[1].base, &ctx->trace[1].physical);
		if (ret < 0)
			return PAMP_STATUS_INVALID_ARGUMENT;
		ctx->pmuAddress = ctx->trace[1].physical;
		ctx->pmuSize = ctx->trace[1].size;
		return pampProtocolError(ScePamgrForDriver_E8FF4812(ctx->key, &ctx->pmuAddress));
	}
	return 0;
}

/* name unknown */
static int pampStop(PampContext *ctx)
{
	PampPacket *packet;
	int ret;

	if (ctx->state != PAMP_STATE_TRACING)
		return PAMP_STATUS_NOT_TRACING;
	if (ksceKernelLockFastMutex(&ctx->controlLock) < 0)
		return PAMP_STATUS_INTERNAL_ERROR;
	if (ctx->state != PAMP_STATE_TRACING) {
		ret = PAMP_STATUS_NOT_TRACING;
		goto out;
	}
	ctx->state = PAMP_STATE_STOPPED;
	packet = pampReserveBuffer(ctx);
	ctx->notification = packet;
	memset(packet->data, 0, 4);
	packet->command = PAMP_NOTIFY_STOPPED;
	packet->sequence = ++ctx->sequence;
	packet->length = 4;
	packet->status = 0;
	packet->words[0] = 0;
	SceDeci4pDfMgrForDebugger_6D26CC56(&packet->hdr, 0x24);
	ret = ksceKernelWaitSema(ctx->sendSema, 1, NULL);
	if (ret >= 0) {
		ctx->sendPacket = ctx->notification;
		ret = sceDfMgrReqSend(ctx->handle);
		if (ret >= 0) {
			if (ksceKernelUnlockFastMutex(&ctx->controlLock) < 0)
				return PAMP_STATUS_INTERNAL_ERROR;
			ret = ScePamgrForDriver_1A7733E0(ctx->key);
			if (!ret) {
				SceUInt32 flags = ctx->flags;

				ctx->flags = flags | ScePamgrForDriver_3ACF4316();
			}
			return pampProtocolError(ret);
		}
	}
out:
	ksceKernelUnlockFastMutex(&ctx->controlLock);
	return ret;
}

/* name unknown */
static int pampTraceStatus(PampContext *ctx, PampPacket *packet)
{
	SceUInt32 *data = packet->words, flags, cursor;

	if (ctx->state == PAMP_STATE_TRACING)
		return PAMP_STATUS_WRONG_STATE;
	for (int i = 0; i < 12; i++)
		packet->data[i] = 0;
	flags = ctx->flags;
	ctx->flags = flags | ScePamgrForDriver_3ACF4316();
	if (ctx->trace[0].allocated) {
		if (!ctx->trace[0].circular && (ctx->flags & 8))
			data[0] |= 1;
		cursor = ScePamgrForDriver_FB46C3CD();
		ctx->trace[0].available = ctx->flags & 8 ? ctx->trace[0].size :
			cursor - ctx->trace[0].physical;
		data[1] = ctx->trace[0].available;
	}
	if (ctx->trace[1].allocated) {
		cursor = ScePamgrForDriver_80CA3B39();
		ctx->trace[1].available = ctx->flags & 0x10 ? ctx->trace[1].size :
			cursor - ctx->trace[1].physical;
		data[2] = ctx->trace[1].available;
	}
	packet->length = 12;
	return 0;
}

/* name unknown */
static int pampStreamTrace(PampContext *ctx, PampPacket *packet)
{
	SceUInt32 *data = ctx->request.words;
	SceUInt32 unit = ctx->request.status & 0xff, size;
	int ret;

	if (ctx->state == PAMP_STATE_TRACING)
		return PAMP_STATUS_WRONG_STATE;
	if (ctx->request.length < 12)
		return ctx->request.command; /* Sony preserves the command as status here. */
	if (unit >= 2)
		return PAMP_STATUS_INVALID_UNIT;
	if (ctx->trace[unit].available < data[0] ||
	    ctx->trace[unit].available < data[0] + data[1])
		return PAMP_STATUS_OUT_OF_RANGE;
	if (ctx->streaming)
		return PAMP_STATUS_STREAMING;
	ctx->traceFlags = ctx->request.status;
	ctx->traceOffset = data[0];
	ctx->traceRemaining = data[1];
	if (ctx->traceCompression & 1) {
		memset(&ctx->zstream, 0, sizeof(ctx->zstream));
		ctx->zstream.zalloc = pampZalloc;
		ctx->zstream.zfree = pampZfree;
		ctx->zstream.opaque = (void *)ctx->heap;
		ctx->zstream.next_in = NULL;
		SceZlibForDriver_25F28DA7(&ctx->zstream, 1, SCE_ZLIB_VERSION, sizeof(ctx->zstream));
		ctx->zfinished = 0;
		ret = pampCompress(ctx, packet, 1);
		if (ret)
			return ret;
		if (!ctx->zfinished) {
			ctx->streaming = 1;
			ctx->kickWorker = 1;
			ctx->traceSequence = ctx->request.sequence;
		}
	} else {
		size = data[1] > 0x10000 ? 0x10000 : data[1];
		ret = pampReadTrace(ctx, packet->chunk.bytes, unit, data[0], size);
		if (ret)
			return ret;
		if (data[1] <= 0x10000) {
			packet->chunk.more = 0;
			packet->length = data[1] + 4;
			ctx->traceRemaining = 0;
		} else {
			ctx->streaming = 1;
			packet->chunk.more = 1;
			packet->length = 0x10004;
			ctx->kickWorker = 1;
			ctx->traceOffset += 0x10000;
			ctx->traceRemaining -= 0x10000;
			ctx->traceSequence = ctx->request.sequence;
		}
	}
	ctx->traced = 1;
	return 0;
}

/* name unknown */
static int pampSetPmu(PampContext *ctx)
{
	SceUInt32 *data = ctx->request.words;
	int ret;

	if (ctx->state == PAMP_STATE_TRACING)
		return PAMP_STATUS_WRONG_STATE;
	if (data[1] >= 16 || data[2] > 0x80 || data[3] > 0x80)
		return PAMP_STATUS_INVALID_ARGUMENT;
	if (ctx->request.length < 0x14)
		return PAMP_STATUS_SHORT_REQUEST;
	ret = ScePamgrForDriver_0BCD4966(ctx->key, 0, ctx->request.status & 0xffff);
	if (!ret)
		ret = ScePamgrForDriver_0BCD4966(ctx->key, 1, data[0] & 0xffff);
	if (ret)
		return pampProtocolError(ret);
	ctx->pmuConfig[0] = ctx->request.status >> 16;
	ctx->pmuConfig[1] = data[0] >> 16;
	ret = ScePamgrForDriver_E8FF4812(ctx->key, &ctx->pmuAddress);
	if (ret)
		return pampProtocolError(ret);
	ctx->paConfig[0] = data[1] & 1 ? ctx->paConfig[0] & ~4u : ctx->paConfig[0] | 4;
	ctx->paConfig[0] = data[1] & 2 ? ctx->paConfig[0] & ~8u : ctx->paConfig[0] | 8;
	ret = ScePamgrForDriver_F2D5105A(ctx->key, ctx->paConfig);
	if (ret)
		return pampProtocolError(ret);
	ctx->sampleConfig[0] = data[2];
	ctx->sampleConfig[1] = data[3];
	ctx->sampleConfig[10] = data[1];
	return pampProtocolError(ScePamgrForDriver_A1E448E7(ctx->key, ctx->sampleConfig));
}

/* name unknown */
static int pampGetPmu(PampContext *ctx, PampPacket *packet)
{
	SceUInt32 *data = packet->words;
	SceUInt16 *half = packet->halves;
	int value, ret;

	if (ctx->state == PAMP_STATE_TRACING)
		return PAMP_STATUS_WRONG_STATE;
	memset(packet->data, 0, 0x14);
	ret = ScePamgrForDriver_6005FDAA(0, &value);
	if (ret)
		return pampProtocolError(ret);
	half[1] = ctx->pmuConfig[0];
	half[0] = value;
	ret = ScePamgrForDriver_6005FDAA(1, &value);
	if (ret)
		return pampProtocolError(ret);
	packet->length = 0x14;
	half[3] = ctx->pmuConfig[1];
	data[4] = ctx->sampleConfig[1];
	data[2] = ctx->sampleConfig[10];
	data[3] = ctx->sampleConfig[0];
	half[2] = value;
	return 0;
}

/* Sony debug records name the protocol handler scePerfArmPmonOpen. */
static int scePerfArmPmonOpen(int event, void *arg, void *opaque)
{
	PampContext *ctx = opaque;
	PampPacket *packet;
	SceUInt32 *data = ctx->request.words;
	SceUInt32 received, size, available, command, sequence;
	SceUInt8 *dst;
	int ret, status;

	switch (event) {
	case SCE_DFMGR_EVENT_RECV:
		dst = NULL;
		if (!ctx->receiving && ksceKernelLockFastMutex(&ctx->controlLock) >= 0) {
			if (!ctx->receiving) {
				ctx->receiving = 1;
				dst = (void *)ctx;
			}
			ksceKernelUnlockFastMutex(&ctx->controlLock);
		}
		received = 0;
		size = (SceUIntPtr)arg;
		while (received < size) {
			ret = sceDfMgrRecv(ctx->handle, dst, size);
			if (ret < 0)
				return 0;
			received += ret;
			dst += ret;
		}
		ctx->received = size;
		break;
	case SCE_DFMGR_EVENT_REQ_DONE:
		if (ksceKernelLockFastMutex(&ctx->commandLock) < 0)
			break;
		packet = pampReserveBuffer(ctx);
		ctx->reply = packet;
		command = ctx->request.command;
		packet->length = 0;
		if (ctx->state == PAMP_STATE_CLOSED) {
			if (command != PAMP_CMD_OPEN) {
				status = PAMP_STATUS_NOT_OPEN;
				goto prepareReply;
			}
			status = pampOpen(ctx);
		} else {
			switch (command) {
			case PAMP_CMD_OPEN:
				status = PAMP_STATUS_ALREADY_OPEN;
				break;
			case PAMP_CMD_CLOSE:
				status = pampClose(ctx);
				break;
			case PAMP_CMD_GET_FREE_SIZE:
				available = ctx->traceSize;
				for (int i = 0; i < 4; i++)
					packet->data[i] = 0;
				for (int i = 0; i < 2; i++)
					if (ctx->trace[i].allocated)
						available -= ctx->trace[i].size;
				packet->words[0] = available;
				packet->length = 4;
				status = PAMP_STATUS_OK;
				break;
			case PAMP_CMD_ALLOCATE_TRACE:
				status = pampAllocateTrace(ctx);
				break;
			case PAMP_CMD_CONFIGURE:
				status = pampConfigure(ctx);
				break;
			case PAMP_CMD_CONTROL:
				if (ctx->state == PAMP_STATE_TRACING)
					status = PAMP_STATUS_WRONG_STATE;
				else if (ctx->request.length < 4)
					status = PAMP_STATUS_SHORT_REQUEST;
				else if (ctx->request.status == 1)
					status = pampProtocolError(ScePamgrForDriver_0B7F6F53(ctx->key));
				else if (ctx->request.status == 0)
					status = pampProtocolError(ScePamgrForDriver_B9EE1620(ctx->key));
				else if (ctx->request.status == 2)
					status = pampProtocolError(ScePamgrForDriver_9935D8EF(ctx->key));
				else
					status = PAMP_STATUS_INVALID_ARGUMENT;
				break;
			case PAMP_CMD_START:
				if (ctx->streaming)
					status = PAMP_STATUS_STREAMING;
				else if (!ctx->trace[0].allocated && !ctx->trace[1].allocated)
					status = PAMP_STATUS_NO_TRACE_BUFFER;
				else {
					status = pampNotify(ctx, PAMP_NOTIFY_STARTED, 0);
					if (!status)
						status = pampProtocolError(ScePamgrForDriver_A708D62A(ctx->key));
				}
				break;
			case PAMP_CMD_STOP:
				status = pampStop(ctx);
				break;
			case PAMP_CMD_TRACE_STATUS:
				status = pampTraceStatus(ctx, packet);
				break;
			case PAMP_CMD_STREAM_TRACE:
				status = pampStreamTrace(ctx, packet);
				break;
			case PAMP_CMD_SET_PMU:
				status = pampSetPmu(ctx);
				break;
			case PAMP_CMD_GET_PMU:
				status = pampGetPmu(ctx, packet);
				break;
			case PAMP_CMD_GET_PA_INFO: {
				SceUInt32 info[4];

				if (ctx->state == PAMP_STATE_TRACING) {
					status = PAMP_STATUS_WRONG_STATE;
					break;
				}
				memset(packet->data, 0, 0x10);
				ScePamgrForDriver_7F3814A4(info);
				memmove(packet->data, info, 0x10);
				packet->length = 0x10;
				status = PAMP_STATUS_OK;
				break;
			}
			case PAMP_CMD_SET_COUNTER:
				if (ctx->state != PAMP_STATE_TRACING)
					status = PAMP_STATUS_WRONG_STATE;
				else if (ctx->request.status >= 2 || data[0] >= 0x10)
					status = PAMP_STATUS_INVALID_ARGUMENT;
				else if (ctx->request.length < 12)
					status = PAMP_STATUS_SHORT_REQUEST;
				else
					status = pampProtocolError(ScePamgrForDriver_16A24732(
						ctx->request.status, data[0], data[1]));
				break;
			case PAMP_CMD_GET_PA_VERSION:
				for (int i = 0; i < 4; i++)
					packet->data[i] = 0;
				packet->words[0] = ScePamgrForDriver_EB70556B();
				packet->length = 4;
				status = PAMP_STATUS_OK;
				break;
			default:
				status = PAMP_STATUS_UNSUPPORTED;
				break;
			}
		}
		command = ctx->request.command;
	prepareReply:
		sequence = ctx->request.sequence;
		packet->command = command;
		packet->status = status;
		packet->sequence = sequence;
		SceDeci4pDfMgrForDebugger_6D26CC56(&packet->hdr, packet->length + 0x20);
		if (ksceKernelLockFastMutex(&ctx->controlLock) >= 0) {
			ctx->receiving = 0;
			ksceKernelUnlockFastMutex(&ctx->controlLock);
		}
		if (ksceKernelWaitSema(ctx->sendSema, 1, NULL) >= 0) {
			ctx->sendPacket = ctx->reply;
			if (ctx->kickWorker) {
				ctx->kickWorker = 0;
				if (ksceKernelSetEventFlag(ctx->event, 1) < 0)
					goto unlock;
			}
			sceDfMgrReqSend(ctx->handle);
		}
	unlock:
		ksceKernelUnlockFastMutex(&ctx->commandLock);
		break;
	case SCE_DFMGR_EVENT_SEND:
		packet = ctx->sendPacket;
		size = SceDeci4pDfMgrForDebugger_CACAB5F9(&packet->hdr);
		sceDfMgrSend(ctx->handle, packet, size);
		break;
	case SCE_DFMGR_EVENT_SENT:
		if (ctx->finalPacket) {
			ctx->streaming = 0;
			ctx->finalPacket = 0;
		}
		pampReleaseBuffer(ctx, ctx->sendPacket);
		ksceKernelSignalSema(ctx->sendSema, 1);
		break;
	case SCE_DFMGR_EVENT_STATUS: {
		SceDfMgrStatus *info = arg;

		if (info->kind == 1 && !info->enabled && ctx->state &&
		    !ScePamgrForDriver_FB04BBEF(ctx->key) &&
		    ksceKernelLockFastMutex(&ctx->controlLock) >= 0) {
			if (ctx->state - 1u < 3) {
				ctx->state = PAMP_STATE_CLOSED;
				if (ksceKernelUnlockFastMutex(&ctx->controlLock) >= 0) {
					ScePamgrForDriver_1A7733E0(ctx->key);
					sceKernelPaUnregister(ctx->key);
					ctx->key = -1;
				}
			} else {
				ksceKernelUnlockFastMutex(&ctx->controlLock);
			}
		}
		break;
	}
	}
	return 0;
}

/* name unknown */
static int pampSendThread(SceSize arglen, void *argp)
{
	SceUInt32 bits;
	int ret;

	for (;;) {
		if ((ret = ksceKernelWaitEventFlag(s_pamp.event, 2, SCE_EVENT_WAITOR, &bits, NULL)) < 0)
			break;
		if (!(bits & 2))
			continue;
		if ((ret = ksceKernelClearEventFlag(s_pamp.event, ~2u)) < 0)
			break;
		ret = sceDfMgrReqSend(s_pamp.handle);
		if (ret < 0 && (SceUInt32)ret != DFMGR_ERROR_NOT_CONNECTED)
			return ret;
	}
	sceKernelExitThread(ret);
	return 0;
}

/* name unknown */
static int pampMainThread(SceSize arglen, void *argp)
{
	PampContext *ctx = &s_pamp;
	PampPacket *packet;
	SceUInt32 bits, size, flags;
	int ret;

	for (;;) {
		if ((ret = ksceKernelWaitEventFlag(ctx->event, 5, SCE_EVENT_WAITOR, &bits, NULL)) < 0)
			break;
		if (bits & 1) {
			if (ctx->kickWorker)
				continue;
			packet = pampReserveBuffer(ctx);
			ctx->workerPacket = packet;
			if (!(ctx->traceCompression & 1)) {
				size = ctx->traceRemaining;
				if (size <= 0x10000) {
					packet->length = size + 4;
					packet->chunk.more = 0;
					pampReadTrace(ctx, packet->chunk.bytes,
						ctx->traceUnit != 0, ctx->traceOffset, size);
					ctx->traceRemaining = 0;
					if ((ret = ksceKernelClearEventFlag(ctx->event, ~1u)) < 0)
						break;
				} else {
					packet->length = 0x10004;
					pampReadTrace(ctx, packet->chunk.bytes,
						ctx->traceUnit != 0, ctx->traceOffset, 0x10000);
					ctx->traceOffset += 0x10000;
					ctx->traceRemaining -= 0x10000;
					packet->chunk.more = 1;
				}
			} else {
				pampCompress(ctx, packet, 0);
				if (ctx->zfinished && (ret = ksceKernelClearEventFlag(ctx->event, ~1u)) < 0)
					break;
			}
			packet->command = PAMP_CMD_STREAM_TRACE;
			packet->status = 0;
			packet->sequence = ctx->traceSequence;
			SceDeci4pDfMgrForDebugger_6D26CC56(&packet->hdr, packet->length + 0x20);
			if ((ret = ksceKernelWaitSema(ctx->sendSema, 1, NULL)) < 0)
				return ret;
			if (!packet->chunk.more)
				ctx->finalPacket = 1;
			ctx->sendPacket = ctx->workerPacket;
			if (ctx->state == PAMP_STATE_CLOSED) {
				if ((ret = ksceKernelClearEventFlag(ctx->event, ~1u)) < 0)
					break;
				pampReleaseBuffer(ctx, ctx->sendPacket);
				ctx->streaming = 0;
				if ((ret = ksceKernelSignalSema(ctx->sendSema, 1)) < 0)
					return ret;
			} else if (ksceKernelSetEventFlag(ctx->event, 2) < 0) {
				return PAMP_STATUS_INTERNAL_ERROR;
			}
		} else if (bits & 4) {
			if ((ret = ksceKernelClearEventFlag(ctx->event, ~4u)) < 0 ||
			    (ret = ksceKernelLockFastMutex(&ctx->commandLock)) < 0)
				break;
			if (ctx->state == PAMP_STATE_TRACING && !pampNotify(ctx, PAMP_NOTIFY_STOPPED, 3) &&
			    !ScePamgrForDriver_1A7733E0(ctx->key)) {
				flags = ctx->flags;
				ctx->flags = flags | ScePamgrForDriver_3ACF4316();
			}
			if ((ret = ksceKernelUnlockFastMutex(&ctx->commandLock)) < 0)
				break;
		}
	}
	sceKernelExitThread(ret);
	return 0;
}

int PampModuleStart(SceSize arglen, void *argp)
{
	PampContext *ctx = &s_pamp;
	SceKernelHeapCreateOpt heapopt;
	SceKernelThreadOptParam threadopt;
	SceDfMgrProtocol protocol;
	int ret;

	if (!ksceKernelCheckDipsw(0xc2)) {
		SCE_DBG_PRINTF(0xb, 271, "disabled by dipsw\n");
		return 1;
	}
	if (!ksceKernelCheckDipsw(0xd4))
		return 1;
	memset(ctx, 0, sizeof(*ctx));
	if (ksceKernelInitializeFastMutex(&ctx->controlLock, "SceDeci4pPampControlLock", 2, NULL) < 0 ||
	    ksceKernelInitializeFastMutex(&ctx->commandLock, "SceDeci4pPampProcessCommandLock", 0, NULL) < 0)
		return 2;
	ret = ksceKernelCreateSema("SceDeci4pPampBuffer", 0, 2, 2, NULL);
	if (ret < 0)
		return 2;
	ctx->bufferSema = ret;
	ret = ksceKernelCreateSema("SceDeci4pPampSend", 0, 1, 1, NULL);
	if (ret < 0)
		return 2;
	ctx->sendSema = ret;
	ret = ksceKernelCreateEventFlag("SceDeci4pPamp", SCE_EVENT_WAITMULTIPLE, 0, NULL);
	if (ret < 0)
		return 2;
	ctx->event = ret;
	heapopt.size = sizeof(heapopt);
	heapopt.flags = HEAP_HAS_MEMTYPE;
	heapopt.maxSize = 0;
	heapopt.memBlock = 0;
	heapopt.memtype = PAMP_MEMTYPE;
	heapopt.pid = 0;
	heapopt.heap = NULL;
	ret = ksceKernelCreateHeap("SceDeci4pPamp", 0x100000, &heapopt);
	if (ret < 0)
		return 2;
	ctx->heap = ret;
	ctx->state = PAMP_STATE_CLOSED;
	threadopt.size = sizeof(threadopt);
	threadopt.attr = 1;
	threadopt.kStackMemType = PAMP_MEMTYPE;
	threadopt.uStackMemType = 0;
	threadopt.uTLSMemType = 0;
	threadopt.uStackMemid = 0;
	threadopt.data_0x18 = 0;
	ret = ksceKernelCreateThread("SceDeci4pPampMain", pampMainThread, 0x1e, 0x4000, 0, 0xf, &threadopt);
	if (ret < 0)
		return 2;
	ctx->mainThread = ret;
	if (ksceKernelStartThread(ret, 0, NULL) < 0)
		return 2;
	ret = ksceKernelCreateThread("SceDeci4pPampSend", pampSendThread, 0x1e, 0x4000, 0, 0xf, &threadopt);
	if (ret < 0)
		return 2;
	ctx->sendThread = ret;
	if (ksceKernelStartThread(ret, 0, NULL) < 0)
		return 2;
	protocol.size = sizeof(protocol);
	protocol.protocol = PAMP_PROTOCOL;
	protocol.alias = 0;
	protocol.version = 0x01020001;
	protocol.subVersion = 0x01010000;
	protocol.destination = 0;
	protocol.flags = DFMGR_PROTOCOL_ENABLE;
	strncpy(protocol.name, "PAMP", sizeof(protocol.name));
	protocol.flags |= ksceKernelCheckDipsw(0xc3) ? DFMGR_PROTOCOL_SDIO : DFMGR_PROTOCOL_USB;
	ret = sceDfMgrOpen(scePerfArmPmonOpen, &protocol, ctx);
	if (ret < 0)
		return 2;
	ctx->handle = ret;
	return 0;
}
