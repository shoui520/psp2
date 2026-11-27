#include <common/types.h>

typedef struct SceMpegCtx {
    u32 magicLo;            /* 0x000: "LIBM" */
    u32 magicHi;            /* 0x004: "PEG\0" */
    u32 versionTag;         /* 0x008: "001\0" */
    s32 psmfVersion;        /* 0x00c */
    u32 ringbufPtr;         /* 0x010 */
    u32 packetNodePool;     /* 0x014 */
    u32 streamQueueTable;   /* 0x018 */
    u32 streamDescTable;    /* 0x01c */
    u32 epMapWorkBuf;       /* 0x020 */
    u32 activeEsTableIdx;   /* 0x024 */
    u8 esDescTable[0x644];  /* 0x028..0x66b */
    u8 dataMutex[0x20];     /* 0x66c..0x68b */
    u8 ringbufPutMutex[0x20]; /* 0x68c..0x6ab */
    s32 avcMode;            /* 0x6ac */
    s32 avcEsBufCount;      /* 0x6b0 */
    u8 avcEsBufInUse[2];    /* 0x6b4..0x6b5 */
    u8 avcEsBufPad[2];      /* 0x6b6..0x6b7 */
    u32 avcEsBufAddr[2];    /* 0x6b8..0x6bf */
    u32 avcWorkPtr;         /* 0x6c0 */
    u32 avcInfoBuf;         /* 0x6c4 */
    u32 avcFrameInfoBuf;    /* 0x6c8 */
    u32 avcSeiBuf;          /* 0x6cc */
    u32 avcCodecFlags;      /* 0x6d0 */
    u32 avcUnused;          /* 0x6d4 */
    u32 avcEdramAddr;       /* 0x6d8 */
    u32 avcDdrTop;          /* 0x6dc */
    u32 avcFrameWidth;      /* 0x6e0 */
    u32 avcFrameHeight;     /* 0x6e4 */
    u32 avcFrameBufCount;   /* 0x6e8 */
    u32 avcExtraParam;      /* 0x6ec */
    u32 atracUnused;        /* 0x6f0 */
    u32 atracEdramAddr;     /* 0x6f4 */
    u32 atracWorkMemSize;   /* 0x6f8 */
    u32 atracWorkPtr;       /* 0x6fc */
} SceMpegCtx;

typedef struct SceMpegPacketNode {
    u32 nextNode;           /* 0x00 */
    u32 prevNode;           /* 0x04 */
    u32 pesInfo[11];        /* 0x08..0x33 */
} SceMpegPacketNode;

typedef struct SceMpegEsDesc {
    u32 srcAddr;            /* 0x00 */
    u32 dstAddr;            /* 0x04 */
    u32 nextDesc;           /* 0x08 */
    u32 copySize;           /* 0x0c */
} SceMpegEsDesc;


/* Imports from Kernel_Library */
extern void *sceKernelMemset(void *dst, int val, u32 size);
extern s32 sceKernelLockLwMutex(void *work, int count, u32 *timeout);
extern s32 sceKernelCheckThreadStack(void);
extern s32 sceKernelUnlockLwMutex(void *work, int count);
extern void *sceKernelMemcpy(void *dst, const void *src, u32 size);

/* Imports from ThreadManForUser */
extern s32 sceKernelCreateFpl(const char *name, u32 part, u32 attr, u32 blockSize, u32 numBlocks, void *opt);
extern s32 sceKernelAllocateFpl(s32 uid, void **blockOut, u32 *timeout);
extern s32 sceKernelDeleteFpl(s32 uid);
extern s32 sceKernelFreeFpl(s32 uid, void *block);
extern s32 sceKernelDeleteLwMutex(void *work);
extern s32 sceKernelCreateLwMutex(void *work, const char *name, u32 attr, int initCount, void *opt);

/* Imports from UtilsForUser */
extern void sceKernelDcacheWritebackInvalidateRange(const void *addr, u32 size);
extern void sceKernelDcacheWritebackRange(const void *addr, u32 size);

/* Imports from sceAudiocodec */
extern s32 sceAudiocodecReleaseEDRAM(u32 *work);
extern s32 sceAudiocodecGetEDRAM(u32 *work, int type);
extern s32 sceAudiocodecInit(u32 *work, int type);
extern s32 sceAudiocodecDecode(u32 *work, int type);
extern s32 sceAudiocodecCheckNeedMem(u32 *work, int type);

/* Imports from sceGe_user */
extern s32 sceGeListEnQueue(const void *list, void *stall, int cbId, void *arg);

/* Imports from sceMpegbase */
extern s32 sceMpegbase_0530BE4E(u32 mode);
extern s32 sceMpegbase_119D5DDA(u32 ptsHigh, u32 ptsLow, u32 dtsHigh, u32 dtsLow);
extern s32 sceMpegBaseCscAvcRange(void *dst, u32 mode, const u32 *range, int width, void *src);
extern s32 sceMpegbase_31007FF8(void *mp);
extern s32 sceMpegBaseCscInit(int width);
extern s32 sceMpegBaseYCrCbCopy(void *dst, const void *src, int flags);
extern s32 sceMpegBaseCscAvc(void *dst, u32 mode, int width, void *src);
extern s32 sceMpegBasePESpacketCopy(void *descList);

/* Imports from sceVideocodec */
extern s32 sceVideocodecInit(u32 *work, int type);
extern s32 sceVideocodecGetFrameCrop(u32 *work, int type);
extern s32 sceVideocodecGetVersion(u32 *work, int type);
extern s32 sceVideocodecGetEDRAM(u32 *work, int type);
extern s32 sceVideocodecDelete(u32 *work, int type);
extern s32 sceVideocodecReleaseEDRAM(u32 *work);
extern s32 sceVideocodecGetSEI(u32 *work, int type);
extern s32 sceVideocodecSetMemory(u32 *work, int type);
extern s32 sceVideocodec_893B32B1(u32 *work, int type);
extern s32 sceVideocodecStop(u32 *work, int type);
extern s32 sceVideocodecOpen(u32 *work, int type);
extern s32 sceVideocodec_D95C24D5(u32 *params);
extern s32 sceVideocodecDecode(u32 *work, int type);

/* .rodata constants */
static const u32 g_stream_defaults[7][2] = {
	{ 0, 0 },
	{ 0, 4 },
	{ 0, 4 },
	{ 0, 2 },
	{ 0, 0 },
	{ 0, 2 },
	{ 0, 0 }
};

static const u32 g_ge_list[4] = {
	0x1f000000, 0x17000000, 0x0f000000, 0x0c000000
};

static const u32 g_csc_modes[4] = { 1, 2, 3, 0 };

static const u8 g_pes_ext_bits[16] = {
	0x00, 0x10, 0x08, 0x18, 0x08, 0x18, 0x10, 0x20,
	0x18, 0x28, 0x20, 0x30, 0x20, 0x30, 0x28, 0x38
};

static const u8 g_zero_pad[0x80] = { 0 };

static const u32 g_au_max_sizes[7] = {
	0x18400, 0x800, 0x140, 0xa0000, 0, 0x19100, 0x800
};

static const u8 g_nal_prefix[4] = { 0x00, 0x00, 0x00, 0x01 };

static const u8 g_atrac_hdr[4] = { 0x28, 0x5c, 0x5c, 0x5c };
static const u32 g_atrac_zero[2] = { 0, 0 };

static const u32 g_stream_id_table[19][2] = {
	{ 0xe0000000, 0xff000000 },
	{ 0xbd000000, 0xffff0000 },
	{ 0xbd100000, 0xffff0000 },
	{ 0xbd200000, 0xffff0000 },
	{ 0xc0000000, 0xff000000 },
	{ 0xbd800000, 0xffff0000 },
	{ 0xbf01e000, 0xffffff00 },
	{ 0xbd300000, 0xffff0000 },
	{ 0xbd400000, 0xffff0000 },
	{ 0xe0000000, 0xff000000 },
	{ 0xbdffc000, 0xffffffff },
	{ 0xbdffa000, 0xffffffff },
	{ 0xbdffa100, 0xffffffff },
	{ 0xbdff9000, 0xffffffff },
	{ 0xc0000000, 0xff000000 },
	{ 0xbd800000, 0xffff0000 },
	{ 0xbda00000, 0xffff0000 },
	{ 0xbd880000, 0xffff0000 },
	{ 0xbd900000, 0xffff0000 }
};

/* .bss globals */
static u32 g_init_flags;                /* 0x8100 */
static u32 g_csc_offsets[4];            /* 0x8104..0x8113 */
static u32 *g_vcodec_ptr;               /* 0x8114 */
static u32 *g_vcodec_sub_ptr;           /* 0x8118 */
static u32 g_vcodec_buf[0x20];          /* 0x8140..0x81bf */
static u32 g_vcodec_sub_buf[0x10];      /* 0x81c0..0x81ff */
static u32 *g_acodec_ptr;               /* 0x8200 */
static u32 g_acodec_buf[0x20];          /* 0x8240..0x82bf */
static u32 g_acodec_flag;               /* 0x82c0 */
static u8 g_avc_mutex[0x20];            /* 0x82c4..0x82e3 */
static u8 g_atrac_mutex[0x20];          /* 0x82e4..0x8303 */
static u8 g_libmpeg_mutex[0x20];        /* 0x8304..0x8323 */
static u32 g_avc_res[2];                /* 0x8324..0x832b */

/* Forward declarations */
static s32 mpegFlushStreamInternal(u32 *mp_ptr, u32 *stream);
static s32 mpegFlushAllStreamInternal(u32 *mp_ptr);
static s32 mpegUnRegistStreamInternal(u32 *mp_ptr, u32 *stream);
static u32 *mpegRegistStreamInternal(u32 *mp_ptr, s32 type, s32 ch);
static s32 mpegInitInternal(void);
static s32 mpegFinishInternal(void);
static s32 mpegQueryMemSizeInternal(s32 mode);
static s32 mpegCreateInternal(u32 *mp_ptr, void *workMem, s32 size, u32 *rb, s32 frameWidth, s32 mode, u32 ddrtop);
static s32 mpegDeleteInternal(u32 *mp_ptr);
static s32 mpegInitAuInternal(u32 *mp_ptr, u32 esBuf, u32 *au);
static u32 *mpegGetAvcResPtr(void);
static u32 mpegGetInitFlags(void);
static u32 mpegSetInitFlags(u32 val);
static s32 mpegQueryStreamOffsetAndInit(u32 *mp_ptr, s32 type, s32 ch, u32 epBuf, u32 *streamDesc);
static s32 mpegAvcDecodeModeInternal(u32 *mp_ptr, const s32 *mode);
static s32 mpegAvcQueryYCbCrSizeInternal(u32 *mp_ptr, s32 frameIdx, u32 *outDetail);
static s32 mpegAvcInitYCbCrInternal(u32 *mp_ptr, u32 *outWork);
static s32 mpegAvcDecodeInternal(u32 *mp_ptr, u32 *au, s32 frameWidth, u32 *bufList, u32 *numPic);
static s32 mpegAvcDecodeStopInternal(u32 *mp_ptr, s32 frameWidth, u32 *bufList, u32 *numPic);
static s32 mpegAvcFlushInternal(u32 *mp_ptr);
static s32 mpegAvcQueryYCbCrSizeDefault(u32 *mp_ptr, u32 *outDetail);
static s32 mpegAvcInitYCbCrWithDims(u32 *mp_ptr, s32 mode, s32 width, s32 height, u32 *ycbcrBuf);
static s32 mpegAvcQueryYCbCrMemSize(u32 *mp_ptr, s32 mode, s32 width, s32 height, s32 *outSize);
static s32 mpegAvcCscRangeInternal(u32 *mp_ptr, u32 *srcYcbcr, const u32 *range, s32 frameWidth, void *dstRgb);
static s32 mpegAvcCopyYCbCrInternal(u32 *mp_ptr, u32 *dstYcbcr, u32 *srcYcbcr);
static s32 mpegAvcCscInternal(u32 *mp_ptr, void *dstYuv, u32 *srcYcbcr, s32 flags);
static s32 mpegAvcDecodeYCbCrInternal(u32 *mp_ptr, u32 *au, u32 *bufList, u32 *numPic);
static s32 mpegAvcDecodeStopYCbCrInternal(u32 *mp_ptr, u32 *bufList, u32 *numPic);
static s32 mpegAvcCalcBufferSizes(s32 width, s32 height, u32 *outEvenSize, u32 *outOddSize);
static s32 mpegAvcConvertYCbCrInternal(u32 *mp_ptr, u32 *dstYcbcr, u32 *srcYcbcr, s32 frameIdx);
static s32 mpegAvcDecodeDetailInternal(u32 *mp_ptr, u32 *bufList, u32 *numPic);
static s32 mpegAvcFillDetailInfo(u32 *dstYcbcr, s32 frameIdx, s32 widthMb, s32 heightMb, u32 *srcPlanes);
static s32 mpegAtracDecodeInternal(u32 *mp_ptr, u32 *au, void *pcmBuf, s32 initFlag);
static s32 mpegQueryStreamOffsetInternal(u32 *mp_ptr, const u8 *header, u32 *outOffset);
static s32 mpegValidateHandle(u32 *mp_ptr);
static s32 mpegQueryStreamSizeInternal(const u8 *header, u32 *outSize);
static s32 mpegCheckPsmfVersion(const u8 *header);
static u32 *mpegFindStreamSlot(u32 code, u32 mp, s32 mode);
static s32 mpegPeekStreamPackets(u32 *queue, s32 skip, s32 maxCount, void *dst, s32 *outCount);
static s32 mpegPopStreamPackets(u32 *queue, s32 count, s32 *outPopped);
static s32 mpegRingbufferPutDemux(u32 *mp_ptr, u32 *rb, s32 numPackets, s32 *outParsed);
static void mpegBitstreamSkip(u32 *bs, u32 numBits);
static s32 mpegParsePackHeader(u32 *bs, u32 *packInfo);
static s32 mpegParsePesHeader(u32 *bs, u32 *pesInfo);
static s32 mpegRingbufferPutInternal(u32 *rb, s32 numPackets, s32 available);
static s32 mpegRingbufferWritePackets(u32 *rb, s32 numPackets, s32 available);
static s32 mpegRingbufferResetInternal(u32 *rb);
static s32 mpegRingbufferQueryMemSizeInternal(s32 packets);
static s32 mpegRingbufferQueryPackNumInternal(u32 memSize);
static s32 mpegRingbufferConstructInternal(u32 *rb, s32 packets, void *rbData, s32 size, void *cb, void *cbArg);
static s32 mpegRingbufferDestructInternal(u32 *rb);
static s32 mpegRingbufferAvailableSizeInternal(u32 *rb);
static s32 mpegRingbufferConsume(u32 *rb, s32 consumed);
static s32 mpegRingbufferCheckAddr(u32 *rb, s32 *minPkt, u32 addr);
static s32 mpegGetAuInternal(u32 *mp_ptr, u32 *stream, u32 streamType, u32 *au);
static s32 mpegGetAtracAuInternal(u32 *mp_ptr, u32 *stream, u32 *au, u32 *attrOut);
static s32 mpegGetAvcAuInternal(u32 *mp_ptr, u32 *stream, u32 streamType, u32 *au, void *headerOut);
static s32 mpegGetUserdataAuInternal(u32 *mp_ptr, u32 *stream, u32 *au, void **esOut);
static s32 mpegRingbufferSetReadPos(u32 *rb, u32 addrA, u32 addrB);
static s32 mpegGetPcmAuInternal(u32 *mp_ptr, u32 *stream, u32 *au, u32 *attrOut);
static s32 mpegAvcCopyEsData(u32 *mp_ptr, const void *src, s32 len, u32 *au);
static s32 mpegAvcAssembleNal(u32 *mp_ptr, u32 *nal, u32 *au);
static s32 mpegEsNodeInit(u32 *desc, u32 srcAddr, u32 dstAddr, u32 nextDesc, u32 copySize);
static s32 mpegEsNodeSetNext(u32 *desc, u32 nextDesc);
static s32 mpegGetAvcNalAuInternal(u32 *mp_ptr, u32 *stream, u32 *au, void *headerOut);
static s32 mpegEsNodeFlushCache(u32 *desc);
static s32 mpegEsFinalizeChain(u32 *mp_ptr, u32 *copyCtx, u32 *au);
static s32 mpegEsAppendChunk(u32 *mp_ptr, u32 *copyCtx, const void *src, s32 len, u32 streamType, u32 *au);
static s32 mpegEsResetTable(u32 *copyCtx);
static s32 mpegMallocAvcEsBufInternal(u32 *mp_ptr);
static s32 mpegFreeAvcEsBufInternal(u32 *mp_ptr, s32 bufId);
static u32 mpegGetAvcEsBufPtr(u32 *mp_ptr, s32 bufId);
static s32 mpegQueryAtracEsSizeInternal(u32 *mp_ptr, u32 *esSize, u32 *outSize);
static s32 mpegQueryPcmEsSizeInternal(u32 *mp_ptr, u32 *esSize, u32 *outSize);
static s32 mpegQueryUserdataEsSizeInternal(u32 *mp_ptr, u32 *esSize, u32 *outSize);
static u32 psmfGetStreamSize(const u8 *hdr);
static u32 psmfGetStreamType(const u8 *hdr);
static u32 psmfGetEpMapFlag(const u8 *hdr);
static u32 psmfGetStreamOffset(const u8 *hdr);
static u32 psmfGetStreamCount(const u8 *hdr);
static const u8 *psmfGetStreamEntry(const u8 *hdr, s32 idx);
static const u8 *psmfGetPayloadPtr(const u8 *hdr);
static void *mpegGetActiveSlotPtr(u32 *mp_ptr);
static s32 mpegNoopHandle(u32 *mp_ptr);
static s32 mpegChangeGetAvcAuModeInternal(u32 *mp_ptr, u32 *stream);
static s32 mpegChangeGetAuModeInternal(u32 *mp_ptr, u32 *stream, s32 mode);
static s32 mpegSetStreamModeInternal(u32 *mp_ptr, u32 *stream, s32 mode);
static s32 mpegCreateLwMutex(s32 count, const char *name, void *work);
static s32 mpegDeleteLwMutex(void *work);
static s32 mpegLockLwMutex(void *work);
static s32 mpegUnlockLwMutex(void *work);
static s32 mpegCheckThreadStack(void);
static s32 mpegAvcResInit(void);
static u32 mpegAvcResAcquire(void);
static u32 mpegAvcResRelease(void);
static s32 mpegAvcResTerm(void);
static s32 mpegVcodecInit(void);
static s32 mpegVcodecOpen(u32 *out);
static s32 mpegVcodecSetup(u32 *vwork, void *infoBuf, void *frameInfoBuf, void *seiBuf, u32 codecFlags, u32 *edramPtr, u32 ddrTop, u32 width, u32 height, u32 frameBufCount, u32 extraParam);
static s32 mpegVcodecReleaseEdram(void);
static u32 mpegVcodecGetEdramAddr(void);
static s32 mpegVcodecDecode(u32 *vwork);
static s32 mpegVcodecStop(u32 *vwork);
static s32 mpegVcodecDelete(u32 *vwork);
static s32 mpegVcodecSetOption(u32 *vwork, u32 optVal);
static s32 mpegAcodecInit(s32 mode);
static s32 mpegAcodecQueryMem(u32 *out, s32 mode, u32 *flagOut);
static s32 mpegAcodecSetup(u32 *awork, u32 edram, u32 workMem, const u8 *hdrShort, const u8 *hdrWord);
static s32 mpegAcodecReleaseEdram(void);
static s32 mpegAcodecQueryMemLocked(u32 *out, s32 mode);
static u32 mpegAcodecGetFlag(u32 *flagOut);
static s32 mpegAcodecDecode(u32 *awork);
static s32 mpegStrncmp(const char *lhs, const char *rhs, s32 len);
static s32 mpegStrcmp(const char *lhs, const char *rhs);
static s32 mpegAbs(s32 val);
static s32 mpegStreamTypeToId(s32 type, s32 ch);
static s32 mpegStreamIdToType(s32 *typeOut, s32 *chOut, u32 streamId);
static s32 mpegMatchStreamId(s32 typeIdx);
static u16 mpegReadBe16(const u8 *src);
static u32 mpegReadBe24(const u8 *src);
static u32 mpegReadBe32(const u8 *src);
static u32 mpegReadBits64(const u8 *src, s32 byteLen);

/* 0: 0x0000 (300B) */
static s32 mpegFlushStreamInternal(u32 *mp_ptr, u32 *stream)
{
	SceMpegCtx *ctx = (SceMpegCtx *)(*mp_ptr);
	s32 ret = mpegValidateHandle(mp_ptr);
	if (ret != 0)
		return ret;
	mpegLockLwMutex(ctx->dataMutex);
	u32 *slot = mpegFindStreamSlot(stream[0], *mp_ptr, 1);
	if (slot == (u32 *)-1) {
		mpegUnlockLwMutex(ctx->dataMutex);
		return 0x806101fe;
	}
	if (slot[2] != (u32)-1) {
		u32 *privSlot = (u32 *)stream[2];
		slot[3] = 0;
		slot[0] = (u32)-1;
		slot[1] = (u32)-1;
		if (privSlot != (u32 *)-1) {
			privSlot[3] = 0;
			privSlot[1] = (u32)-1;
			privSlot[0] = (u32)-1;
		}
	}
	stream[4] = 0x7fff;
	stream[11] = 0;
	s32 type, ch;
	mpegStreamIdToType(&type, &ch, stream[0]);
	const u32 *def = g_stream_defaults[type];
	stream[10] = def[0];
	stream[5] = def[1];
	stream[6] = 1;
	stream[12] &= ~4u;
	stream[16] = 0;
	stream[8] = 0;
	stream[13] = 0;
	stream[14] = 0;
	mpegUnlockLwMutex(ctx->dataMutex);
	return 0;
}

/* 1: 0x012c (144B) */
static s32 mpegFlushAllStreamInternal(u32 *mp_ptr)
{
	s32 ret = mpegValidateHandle(mp_ptr);
	if (ret != 0)
		return ret;
	SceMpegCtx *ctx = (SceMpegCtx *)(*mp_ptr);
	for (s32 i = 0; i < 0x70; i++) {
		u32 *slot = (u32 *)(ctx->streamQueueTable + (i << 4));
		if (slot[2] != (u32)-1) {
			ret = mpegFlushStreamInternal(mp_ptr, (u32 *)slot[2]);
			if (ret != 0)
				break;
		}
	}
	return ret;
}

/* 2: 0x01bc (200B) */
static s32 mpegUnRegistStreamInternal(u32 *mp_ptr, u32 *stream)
{
	SceMpegCtx *ctx = (SceMpegCtx *)(*mp_ptr);
	s32 ret = mpegValidateHandle(mp_ptr);
	if (ret != 0)
		return ret;
	mpegLockLwMutex(ctx->dataMutex);
	u32 *slot = mpegFindStreamSlot(stream[0], (u32)ctx, 1);
	if (slot == (u32 *)-1) {
		mpegUnlockLwMutex(ctx->dataMutex);
		return 0x806101fe;
	}
	if (slot[2] != (u32)-1) {
		u32 *privSlot = (u32 *)stream[2];
		slot[3] = 0;
		slot[2] = (u32)-1;
		slot[0] = (u32)-1;
		slot[1] = (u32)-1;
		if (privSlot != (u32 *)-1) {
			privSlot[1] = (u32)-1;
			privSlot[3] = 0;
			privSlot[2] = (u32)-1;
			privSlot[0] = (u32)-1;
		}
	}
	mpegUnlockLwMutex(ctx->dataMutex);
	return 0;
}

/* 3: 0x0284 (248B) */
static u32 *mpegRegistStreamInternal(u32 *mp_ptr, s32 type, s32 ch)
{
	SceMpegCtx *ctx = (SceMpegCtx *)(*mp_ptr);
	if (mpegValidateHandle(mp_ptr) != 0)
		return 0;
	mpegLockLwMutex(ctx->dataMutex);
	s32 code = mpegStreamTypeToId(type, ch);
	u32 *streamDesc = mpegFindStreamSlot(code, (u32)ctx, 2);
	if (streamDesc == (u32 *)-1) {
		mpegUnlockLwMutex(ctx->dataMutex);
		return 0;
	}
	u32 epBuf = (u32)-1;
	if (type == 0)
		epBuf = ctx->epMapWorkBuf + (ch << 11);
	if (mpegQueryStreamOffsetAndInit(mp_ptr, type, ch, epBuf, streamDesc) != 0) {
		mpegUnlockLwMutex(ctx->dataMutex);
		return 0;
	}
	mpegUnlockLwMutex(ctx->dataMutex);
	return streamDesc;
}

/* 4: 0x037c (188B) */
static s32 mpegInitInternal(void)
{
	if (g_init_flags != 0)
		return 0x80618005;
	g_init_flags = 1;
	if (mpegCreateLwMutex(0, "SceMpegLibmpeg", g_libmpeg_mutex) != 0)
		return 0x80618006;
	if (mpegCreateLwMutex(0, "SceMpegAvc", g_avc_mutex) != 0)
		return 0x80618006;
	if (mpegCreateLwMutex(0, "SceMpegAtrac", g_atrac_mutex) != 0)
		return 0x80618006;
	mpegVcodecInit();
	mpegAcodecInit(0);
	return 0;
}

/* 5: 0x0438 (164B) */
static s32 mpegFinishInternal(void)
{
	if ((g_init_flags & 1) == 0)
		return 0x80618009;
	if ((g_init_flags & 2) != 0)
		return 0x80618009;
	g_init_flags &= ~1u;
	mpegAcodecReleaseEdram();
	mpegVcodecReleaseEdram();
	if (mpegDeleteLwMutex(g_libmpeg_mutex) != 0)
		return 0x80618006;
	if (mpegDeleteLwMutex(g_avc_mutex) != 0)
		return 0x80618006;
	if (mpegDeleteLwMutex(g_atrac_mutex) != 0)
		return 0x80618006;
	return 0;
}

/* 6: 0x04dc (72B) */
static s32 mpegQueryMemSizeInternal(s32 mode)
{
	if (mode == 1) {
		if (mpegAcodecGetFlag(0) != 0)
			mpegAcodecReleaseEdram();
	}
	return 0x10000;
}

/* 7: 0x0524 (1284B) */
static s32 mpegCreateInternal(u32 *mp_ptr, void *workMem, s32 size, u32 *rb, s32 frameWidth, s32 mode, u32 ddrtop)
{
	s32 ret = sceMpegbase_31007FF8(mp_ptr);
	if (ret == 1)
		sceGeListEnQueue(g_ge_list, 0, -1, 0);
	if ((g_init_flags & 1) == 0)
		return 0x80618009;
	if ((g_init_flags & 2) != 0)
		return 0x80618005;
	g_init_flags |= 2;
	if (size < mpegQueryMemSizeInternal(mode))
		return 0x80610022;
	mpegLockLwMutex(g_libmpeg_mutex);
	sceKernelMemset(workMem, 0, size);
	u32 base = ((u32)workMem + 0x3f) & ~0x3fu;
	SceMpegCtx *ctx = (SceMpegCtx *)base;
	*mp_ptr = base;
	ctx->psmfVersion = -1;
	ctx->magicLo = 0x4d42494cu; /* "LIBM" */
	ctx->magicHi = 0x00474550u; /* "PEG\0" */
	ctx->versionTag = 0x00313030u; /* "001\0" */
	ctx->avcMode = mode;
	if (mpegCreateLwMutex(0, "SceMpegRingBufferPut", ctx->ringbufPutMutex) != 0 ||
	    mpegCreateLwMutex(0, "SceMpegMpegData", ctx->dataMutex) != 0) {
		mpegUnlockLwMutex(g_libmpeg_mutex);
		return 0x80618006;
	}
	if (ctx->avcMode == 1) {
		ctx->ringbufPtr = 0;
		ctx->packetNodePool = 0;
	} else {
		rb[10] = (u32)mp_ptr;
		ctx->packetNodePool = rb[8];
		ctx->ringbufPtr = (u32)rb;
	}
	u32 pQueues = ((base + 0x704) + 0x3f) & ~0x3fu;
	ctx->streamQueueTable = pQueues;
	for (s32 i = 0; i < 0x70; i++) {
		u32 *slot = (u32 *)(pQueues + (i << 4));
		slot[0] = (u32)-1;
		slot[1] = (u32)-1;
		slot[2] = (u32)-1;
		slot[3] = 0;
	}
	u32 pStreams = (pQueues + 0x704 + 0x3f) & ~0x3fu;
	ctx->streamDescTable = pStreams;
	u32 pEpBuf = (pStreams + 0x1dc4 + 0x3f) & ~0x3fu;
	ctx->epMapWorkBuf = pEpBuf;
	sceKernelMemset((void *)pEpBuf, 0, 0x8000);
	ctx->activeEsTableIdx = 0;
	u32 pCur = pEpBuf + 0x8644;
	{
		u32 tbl = (u32)ctx->esDescTable;
		u32 *tblCount = (u32 *)tbl;
		*tblCount = 99;
		for (s32 i = 0; i < (s32)(*tblCount); i++) {
			u32 descAddr = tbl + 4 + (i << 4);
			mpegEsNodeInit((u32 *)descAddr, 0, 0, descAddr + 16, 0);
		}
		u32 lastDesc = tbl + 4 + ((*tblCount - 1) << 4);
		mpegEsNodeSetNext((u32 *)lastDesc, 0);
	}
	u32 pVwork = (pCur + 0x3f) & ~0x3fu;
	u32 pVinfo = pVwork + 0x80;
	u32 pVframe = pVinfo + 0x40;
	u32 pVsei = pVframe + 0xc0;
	pCur = pVsei + 0x80;
	ctx->avcWorkPtr = pVwork;
	ctx->avcInfoBuf = pVinfo;
	ctx->avcFrameInfoBuf = pVframe;
	ctx->avcSeiBuf = pVsei;
	ctx->avcCodecFlags = 0;
	u32 edram = mpegVcodecGetEdramAddr();
	ctx->avcEdramAddr = edram;
	if (edram == 0) {
		mpegUnlockLwMutex(g_libmpeg_mutex);
		return 0x80610022;
	}
	if (mode == 1) {
		ctx->avcEsBufCount = 0;
		ctx->avcEsBufInUse[0] = 1;
		ctx->avcEsBufInUse[1] = 1;
		ctx->avcEsBufAddr[0] = 0;
		ctx->avcEsBufAddr[1] = 0;
	} else {
		ctx->avcEsBufInUse[0] = 0;
		ctx->avcEsBufInUse[1] = 0;
		ctx->avcEsBufCount = 2;
		ctx->avcEsBufAddr[0] = 0x4a000;
		ctx->avcEsBufAddr[1] = 0x62400;
		if (ddrtop == 0)
			ddrtop = 0x09000000;
	}
	ctx->avcFrameWidth = 480;
	ctx->avcFrameHeight = 272;
	ctx->avcFrameBufCount = (mode == 1) ? 3 : 2;
	ctx->avcDdrTop = ddrtop;
	ctx->avcExtraParam = 0;
	ret = mpegVcodecSetup((u32 *)pVwork, (void *)pVinfo, (void *)pVframe, (void *)pVsei,
	                      ctx->avcCodecFlags, &ctx->avcEdramAddr, ctx->avcDdrTop,
	                      ctx->avcFrameWidth, ctx->avcFrameHeight, ctx->avcFrameBufCount,
	                      ctx->avcExtraParam);
	if (ret != 0) {
		mpegUnlockLwMutex(g_libmpeg_mutex);
		return ret;
	}
	if (mode == 1)
		mpegVcodecSetOption((u32 *)ctx->avcWorkPtr, 1);
	ret = sceMpegBaseCscInit(frameWidth);
	if (ret != 0) {
		mpegUnlockLwMutex(g_libmpeg_mutex);
		return ret;
	}
	if (mode == 1) {
		ctx->atracWorkPtr = 0;
		ctx->atracWorkMemSize = 0;
		ctx->atracEdramAddr = 0;
	} else {
		ctx->atracWorkPtr = pCur;
		u32 needMem = 0;
		ret = mpegAcodecQueryMemLocked(&needMem, mode);
		pCur += 0x80;
		if (ret != 0) {
			mpegUnlockLwMutex(g_libmpeg_mutex);
			return ret;
		}
		ctx->atracWorkMemSize = needMem;
		u32 aEdram = mpegAcodecGetFlag(0);
		ctx->atracEdramAddr = aEdram;
		if (aEdram == 0) {
			mpegUnlockLwMutex(g_libmpeg_mutex);
			return 0x80610022;
		}
	}
	if (pCur >= (u32)workMem + (u32)size)
		ret = 0x80610022;
	mpegUnlockLwMutex(g_libmpeg_mutex);
	return ret;
}

/* 8: 0x0a28 (208B) */
static s32 mpegDeleteInternal(u32 *mp_ptr)
{
	s32 ret = mpegValidateHandle(mp_ptr);
	if (ret != 0)
		return ret;
	SceMpegCtx *ctx = (SceMpegCtx *)(*mp_ptr);
	g_init_flags &= ~2u;
	mpegVcodecDelete((u32 *)ctx->avcWorkPtr);
	mpegLockLwMutex(g_libmpeg_mutex);
	for (s32 i = 0; i < 0x70; i++) {
		u32 *slot = (u32 *)(ctx->streamQueueTable + (i << 4));
		slot[0] = (u32)-1;
		slot[1] = (u32)-1;
		slot[2] = (u32)-1;
		slot[3] = 0;
	}
	if (mpegDeleteLwMutex(ctx->dataMutex) != 0 ||
	    mpegDeleteLwMutex(ctx->ringbufPutMutex) != 0) {
		mpegUnlockLwMutex(g_libmpeg_mutex);
		return 0x80618006;
	}
	mpegUnlockLwMutex(g_libmpeg_mutex);
	return 0;
}

/* 9: 0x0af8 (72B) */
static s32 mpegInitAuInternal(u32 *mp_ptr, u32 esBuf, u32 *au)
{
	s32 ret = mpegValidateHandle(mp_ptr);
	if (ret != 0)
		return ret;
	au[4] = esBuf;
	au[5] = 0;
	return 0;
}

/* 10: 0x0b40 (12B) */
static u32 *mpegGetAvcResPtr(void)
{
	return g_avc_res;
}

/* 11: 0x0b4c (12B) */
static u32 mpegGetInitFlags(void)
{
	return g_init_flags;
}

/* 12: 0x0b58 (16B) */
static u32 mpegSetInitFlags(u32 val)
{
	u32 old = g_init_flags;
	g_init_flags = val;
	return old;
}

/* 13: 0x0b68 (396B) */
static s32 mpegQueryStreamOffsetAndInit(u32 *mp_ptr, s32 type, s32 ch, u32 epBuf, u32 *streamDesc)
{
	s32 ret = mpegValidateHandle(mp_ptr);
	if (ret != 0)
		return ret;
	if (type >= 7)
		return 0x806101fe;
	u32 mp = *mp_ptr;
	s32 code = mpegStreamTypeToId(type, ch);
	streamDesc[0] = code;
	streamDesc[1] = (u32)mpegMatchStreamId(type);
	const u32 *def = g_stream_defaults[type];
	streamDesc[3] = (type == 0) ? epBuf : 0;
	streamDesc[4] = 0x7fff;
	streamDesc[6] = 1;
	streamDesc[10] = def[0];
	streamDesc[5] = def[1];
	streamDesc[11] = 0;
	streamDesc[8] = 0;
	streamDesc[13] = 0;
	streamDesc[14] = 0;
	streamDesc[12] = 0;
	streamDesc[16] = 0;
	u32 *slot = mpegFindStreamSlot(streamDesc[0], mp, 1);
	if (slot == (u32 *)-1)
		return 0x806101fe;
	u32 emptyVal = slot[2];
	if (emptyVal != (u32)-1)
		return 0x80618005;
	slot[1] = emptyVal;
	slot[2] = (u32)streamDesc;
	slot[3] = 0;
	slot[0] = emptyVal;
	if (type != 0) {
		streamDesc[2] = emptyVal;
		return 0;
	}
	s32 privCode = mpegStreamTypeToId(6, ch);
	u32 *privSlot = mpegFindStreamSlot(privCode, mp, 1);
	if (privSlot == (u32 *)-1)
		return 0x806101fe;
	if (privSlot[2] != emptyVal)
		return 0x80618005;
	privSlot[1] = emptyVal;
	streamDesc[2] = (u32)privSlot;
	privSlot[2] = (u32)streamDesc;
	privSlot[3] = 0;
	privSlot[0] = emptyVal;
	return 0;
}

/* 14: 0x0cf4 (172B) */
static s32 mpegAvcDecodeModeInternal(u32 *mp_ptr, const s32 *mode)
{
	s32 ret = mpegValidateHandle(mp_ptr);
	if (ret != 0)
		return ret;
	s32 decodeMode = mode[0];
	if (decodeMode != -1) {
		if (decodeMode < -1 || decodeMode >= 3)
			return 0x806101fe;
		u32 *vwork = (u32 *)((SceMpegCtx *)(*mp_ptr))->avcWorkPtr;
		vwork[13] = (u32)decodeMode;
	}
	s32 pixelMode = mode[1];
	if (pixelMode != -1) {
		if (pixelMode < -1 || pixelMode >= 4)
			return 0x806101fe;
		sceMpegbase_0530BE4E(g_csc_modes[pixelMode]);
	}
	return 0;
}

/* 15: 0x0da0 (272B) */
static s32 mpegAvcQueryYCbCrSizeInternal(u32 *mp_ptr, s32 frameIdx, u32 *outDetail)
{
	s32 ret = mpegValidateHandle(mp_ptr);
	if (ret != 0)
		return ret;
	if (outDetail == 0)
		return 0x806101fe;
	u32 vworkAddr;
	ret = mpegAvcInitYCbCrInternal(mp_ptr, &vworkAddr);
	if (ret != 0)
		return ret;
	u32 *vwork = (u32 *)vworkAddr;
	u32 frameListAddr = vwork[11];
	u32 seiAddr = vwork[12];
	u32 *avcInfo = (u32 *)vwork[4];
	outDetail[0] = vwork[2];
	if (frameIdx < 0 || frameIdx >= (s32)avcInfo[8])
		return 0x806201fe;
	u32 *picInfo = (u32 *)(frameListAddr + frameIdx * 44);
	outDetail[10] = seiAddr;
	outDetail[1] = picInfo[8];
	outDetail[11] = picInfo[9];
	outDetail[2] = avcInfo[2];
	outDetail[12] = picInfo[10];
	outDetail[3] = avcInfo[3];
	outDetail[4] = avcInfo[4];
	outDetail[5] = avcInfo[5];
	outDetail[6] = avcInfo[6];
	outDetail[7] = avcInfo[7];
	outDetail[8] = avcInfo[8];
	outDetail[9] = avcInfo[9];
	return 0;
}

/* 16: 0x0eb0 (76B) */
static s32 mpegAvcInitYCbCrInternal(u32 *mp_ptr, u32 *outWork)
{
	s32 ret = mpegValidateHandle(mp_ptr);
	if (ret != 0)
		return ret;
	*outWork = ((SceMpegCtx *)(*mp_ptr))->avcWorkPtr;
	return 0;
}

/* 17: 0x0efc (436B) */
static s32 mpegAvcDecodeInternal(u32 *mp_ptr, u32 *au, s32 frameWidth, u32 *bufList, u32 *numPic)
{
	s32 ret = mpegValidateHandle(mp_ptr);
	if (ret != 0)
		return ret;
	SceMpegCtx *ctx = (SceMpegCtx *)(*mp_ptr);
	u32 *frameInfo = (u32 *)ctx->avcFrameInfoBuf;
	u32 *vwork = (u32 *)ctx->avcWorkPtr;
	u32 *avcInfo = (u32 *)ctx->avcInfoBuf;
	u32 esAddr = mpegGetAvcEsBufPtr(mp_ptr, (s32)au[4]);
	vwork[9] = esAddr & 0x0fffffffu;
	vwork[10] = au[5];
	ret = sceMpegbase_119D5DDA(au[0], au[1], au[2], au[3]);
	if (ret != 0)
		return ret;
	ret = mpegVcodecDecode(vwork);
	au[5] = 0;
	if (ret != 0)
		return ret;
	s32 picCount = (s32)avcInfo[8];
	*numPic = (u32)picCount;
	if ((u32)(ctx->avcMode - 3) < 4u)
		return 0;
	if (picCount <= 0)
		return 0;
	for (s32 i = 0; ; ) {
		u32 dstAddr = bufList[i];
		i++;
		if (dstAddr == 0)
			return 0x806101fe;
		u32 cscSrc[12];
		cscSrc[0] = avcInfo[3] >> 4;
		cscSrc[1] = avcInfo[2] >> 4;
		cscSrc[2] = 0;
		cscSrc[3] = 0;
		cscSrc[4] = frameInfo[0];
		cscSrc[5] = frameInfo[1];
		cscSrc[6] = frameInfo[2];
		cscSrc[7] = frameInfo[3];
		cscSrc[8] = frameInfo[4];
		cscSrc[9] = frameInfo[5];
		cscSrc[10] = frameInfo[6];
		cscSrc[11] = frameInfo[7];
		frameInfo += 11;
		ret = sceMpegBaseCscAvc((void *)dstAddr, 0, frameWidth, cscSrc);
		if (ret != 0 || i >= (s32)(*numPic))
			return ret;
	}
}

/* 18: 0x10b0 (336B) */
static s32 mpegAvcDecodeStopInternal(u32 *mp_ptr, s32 frameWidth, u32 *bufList, u32 *numPic)
{
	SceMpegCtx *ctx = (SceMpegCtx *)(*mp_ptr);
	u32 *frameInfo = (u32 *)ctx->avcFrameInfoBuf;
	u32 *avcInfo = (u32 *)ctx->avcInfoBuf;
	s32 ret = mpegAvcFlushInternal(mp_ptr);
	if (ret != 0)
		return ret;
	s32 picCount = (s32)avcInfo[8];
	*numPic = (u32)picCount;
	if ((u32)(ctx->avcMode - 3) < 4u)
		return 0;
	if (picCount <= 0)
		return 0;
	for (s32 i = 0; ; ) {
		u32 dstAddr = bufList[i];
		i++;
		if (dstAddr == 0)
			return 0x806101fe;
		u32 cscSrc[12];
		cscSrc[0] = avcInfo[3] >> 4;
		cscSrc[1] = avcInfo[2] >> 4;
		cscSrc[2] = 0;
		cscSrc[3] = 0;
		cscSrc[4] = frameInfo[0];
		cscSrc[5] = frameInfo[1];
		cscSrc[6] = frameInfo[2];
		cscSrc[7] = frameInfo[3];
		cscSrc[8] = frameInfo[4];
		cscSrc[9] = frameInfo[5];
		cscSrc[10] = frameInfo[6];
		cscSrc[11] = frameInfo[7];
		frameInfo += 11;
		ret = sceMpegBaseCscAvc((void *)dstAddr, 0, frameWidth, cscSrc);
		if (ret != 0 || i >= (s32)(*numPic))
			return ret;
	}
}

/* 19: 0x1200 (68B) */
static s32 mpegAvcFlushInternal(u32 *mp_ptr)
{
	s32 ret = mpegValidateHandle(mp_ptr);
	if (ret != 0)
		return ret;
	return mpegVcodecStop((u32 *)((SceMpegCtx *)(*mp_ptr))->avcWorkPtr);
}

/* 20: 0x1244 (32B) */
static s32 mpegAvcQueryYCbCrSizeDefault(u32 *mp_ptr, u32 *outDetail)
{
	return mpegAvcQueryYCbCrSizeInternal(mp_ptr, 0, outDetail);
}

/* 21: 0x1264 (460B) */
static s32 mpegAvcInitYCbCrWithDims(u32 *mp_ptr, s32 mode, s32 width, s32 height, u32 *ycbcrBuf)
{
	s32 ret = mpegValidateHandle(mp_ptr);
	if (ret != 0)
		return ret;
	if (mode == 3) {
		s32 ySize = width * height;
		u32 cSize = (u32)ySize >> 2;
		if (((width / 16) & 7) == 7) {
			ySize += height * 16;
			cSize += (u32)(height * 4);
		}
		u8 *base = (u8 *)ycbcrBuf;
		sceKernelMemset(base, 0x10, (u32)ySize);
		sceKernelMemset(base + ySize, 0x80, cSize);
		sceKernelMemset(base + ySize + cSize, 0x80, cSize);
		sceKernelDcacheWritebackRange(base, (u32)ySize);
		sceKernelDcacheWritebackRange(base + ySize, cSize);
		sceKernelDcacheWritebackRange(base + ySize + cSize, cSize);
		return 0;
	}
	sceKernelMemset(ycbcrBuf, 0, 128);
	if (mode == 2) {
		ycbcrBuf[11] = 2;
		ycbcrBuf[4] = 2;
		ycbcrBuf[6] = 2;
		ycbcrBuf[8] = 2;
		ycbcrBuf[10] = 2;
		ycbcrBuf[5] = 2;
		ycbcrBuf[7] = 2;
		ycbcrBuf[9] = 2;
		return 0;
	}
	if (mode != 1)
		return 0x806101fe;
	u32 evenSize, oddSize;
	ret = mpegAvcCalcBufferSizes(width, height, &evenSize, &oddSize);
	if (ret != 0)
		return ret;
	u32 planeBase = (u32)(ycbcrBuf + 32);
	u32 planeCbEven = planeBase + evenSize + evenSize;
	u32 planeCrEven = planeCbEven + (evenSize >> 1);
	u32 planeYOddTop = planeCrEven + (evenSize >> 1);
	u32 planeCbOdd = planeYOddTop + oddSize + oddSize;
	ycbcrBuf[4] = planeBase;
	ycbcrBuf[11] = planeCbOdd + (oddSize >> 1);
	ycbcrBuf[6] = planeBase + evenSize;
	ycbcrBuf[8] = planeCbEven;
	ycbcrBuf[10] = planeCrEven;
	ycbcrBuf[5] = planeYOddTop;
	ycbcrBuf[7] = planeYOddTop + oddSize;
	ycbcrBuf[9] = planeCbOdd;
	return 0;
}

/* 22: 0x1430 (272B) */
static s32 mpegAvcQueryYCbCrMemSize(u32 *mp_ptr, s32 mode, s32 width, s32 height, s32 *outSize)
{
	*outSize = 0;
	s32 ret = mpegValidateHandle(mp_ptr);
	if (ret != 0)
		return ret;
	if (mode == 2) {
		*outSize = 128;
		return 0;
	}
	if (mode == 1) {
		u32 evenSize, oddSize;
		ret = mpegAvcCalcBufferSizes(width, height, &evenSize, &oddSize);
		if (ret != 0)
			return ret;
		*outSize = (s32)((evenSize + oddSize) * 3 + 128);
		return 0;
	}
	if (mode != 3)
		return 0x806101fe;
	s32 sz = (width * height * 3) / 2;
	*outSize = sz;
	if (((width / 16) & 7) != 7)
		return 0;
	*outSize = sz + height * 24;
	return 0;
}

/* 23: 0x1540 (540B) */
static s32 mpegAvcCscRangeInternal(u32 *mp_ptr, u32 *srcYcbcr, const u32 *range, s32 frameWidth, void *dstRgb)
{
	s32 ret = mpegValidateHandle(mp_ptr);
	if (ret != 0)
		return ret;
	SceMpegCtx *ctx = (SceMpegCtx *)(*mp_ptr);
	u32 cscSrc[12];
	cscSrc[0] = srcYcbcr[0];
	cscSrc[1] = srcYcbcr[1];
	cscSrc[2] = srcYcbcr[2];
	cscSrc[3] = srcYcbcr[3];
	if (srcYcbcr[4] == 2) {
		u32 *frameInfo = (u32 *)(ctx->avcFrameInfoBuf + 44 * srcYcbcr[12]);
		cscSrc[4] = frameInfo[0] + g_csc_offsets[0];
		cscSrc[5] = frameInfo[1] + g_csc_offsets[1];
		cscSrc[6] = frameInfo[2] + g_csc_offsets[0];
		cscSrc[7] = frameInfo[3] + g_csc_offsets[1];
		cscSrc[8] = frameInfo[4] + g_csc_offsets[2];
		cscSrc[9] = frameInfo[5] + g_csc_offsets[3];
		cscSrc[10] = frameInfo[6] + g_csc_offsets[2];
		cscSrc[11] = frameInfo[7] + g_csc_offsets[3];
	} else {
		cscSrc[4] = srcYcbcr[4] + g_csc_offsets[0];
		cscSrc[5] = srcYcbcr[5] + g_csc_offsets[1];
		cscSrc[6] = srcYcbcr[6] + g_csc_offsets[0];
		cscSrc[7] = srcYcbcr[7] + g_csc_offsets[1];
		cscSrc[8] = srcYcbcr[8] + g_csc_offsets[2];
		cscSrc[9] = srcYcbcr[9] + g_csc_offsets[3];
		cscSrc[10] = srcYcbcr[10] + g_csc_offsets[2];
		cscSrc[11] = srcYcbcr[11] + g_csc_offsets[3];
	}
	if ((range[0] & 0xf) != 0 || (range[1] & 0xf) != 0 ||
	    (range[2] & 0xf) != 0 || (range[3] & 0xf) != 0)
		return 0x806101fe;
	u32 scaledRange[4];
	scaledRange[0] = range[0] >> 4;
	scaledRange[1] = range[1] >> 4;
	scaledRange[2] = range[2] >> 4;
	scaledRange[3] = range[3] >> 4;
	return sceMpegBaseCscAvcRange(dstRgb, 0, scaledRange, frameWidth, cscSrc);
}

/* 24: 0x175c (144B) */
static s32 mpegAvcCopyYCbCrInternal(u32 *mp_ptr, u32 *dstYcbcr, u32 *srcYcbcr)
{
	s32 ret = mpegValidateHandle(mp_ptr);
	if (ret != 0)
		return ret;
	if (dstYcbcr == 0 || srcYcbcr == 0)
		return 0x806101fe;
	if (dstYcbcr[4] == 2)
		return 0x806101fe;
	return mpegAvcConvertYCbCrInternal(mp_ptr, dstYcbcr, srcYcbcr, -1);
}

/* 25: 0x17ec (148B) */
static s32 mpegAvcCscInternal(u32 *mp_ptr, void *dstYuv, u32 *srcYcbcr, s32 flags)
{
	s32 ret = mpegValidateHandle(mp_ptr);
	if (ret != 0)
		return ret;
	if (dstYuv == 0 || srcYcbcr == 0)
		return 0x806101fe;
	u32 tmpDst[12];
	tmpDst[4] = 3;
	tmpDst[5] = (u32)dstYuv;
	tmpDst[3] = (u32)flags;
	return mpegAvcConvertYCbCrInternal(mp_ptr, tmpDst, srcYcbcr, -1);
}

/* 26: 0x1880 (208B) */
static s32 mpegAvcDecodeYCbCrInternal(u32 *mp_ptr, u32 *au, u32 *bufList, u32 *numPic)
{
	s32 ret = mpegValidateHandle(mp_ptr);
	if (ret != 0)
		return ret;
	u32 *vwork = (u32 *)((SceMpegCtx *)(*mp_ptr))->avcWorkPtr;
	vwork[9] = mpegGetAvcEsBufPtr(mp_ptr, (s32)au[4]) & 0x0fffffffu;
	vwork[10] = au[5];
	ret = sceMpegbase_119D5DDA(au[0], au[1], au[2], au[3]);
	if (ret != 0)
		return ret;
	ret = mpegVcodecDecode(vwork);
	au[5] = 0;
	if (ret != 0)
		return ret;
	return mpegAvcDecodeDetailInternal(mp_ptr, bufList, numPic);
}

/* 27: 0x1950 (100B) */
static s32 mpegAvcDecodeStopYCbCrInternal(u32 *mp_ptr, u32 *bufList, u32 *numPic)
{
	s32 ret = mpegAvcFlushInternal(mp_ptr);
	if (ret != 0)
		return ret;
	return mpegAvcDecodeDetailInternal(mp_ptr, bufList, numPic);
}

/* 28: 0x19b4 (164B) */
static s32 mpegAvcCalcBufferSizes(s32 width, s32 height, u32 *outEvenSize, u32 *outOddSize)
{
	u32 uWidth = (u32)width;
	u32 uHeight = (u32)height;
	u32 hMb = uHeight >> 4;
	if (((uWidth | uHeight) & 0xfu) != 0)
		return 0x806101fe;
	if ((uWidth >> 4) - 1 >= 30u || hMb == 0 || hMb >= 18u)
		return 0x806101fe;
	u32 rowBytes = hMb * 8u;
	if ((uWidth & 0x10u) != 0) {
		*outEvenSize = (((uWidth & ~0x1fu) + 32u) >> 1) * rowBytes;
		*outOddSize = ((uWidth & ~0x1fu) >> 1) * rowBytes;
	} else {
		u32 halfAligned = (uWidth & ~0x1fu) >> 1;
		*outEvenSize = halfAligned * rowBytes;
		*outOddSize = halfAligned * rowBytes;
	}
	return 0;
}

/* 29: 0x1a58 (176B) */
static s32 mpegAvcConvertYCbCrInternal(u32 *mp_ptr, u32 *dstYcbcr, u32 *srcYcbcr, s32 frameIdx)
{
	SceMpegCtx *ctx = (SceMpegCtx *)(*mp_ptr);
	u32 *avcInfo = (u32 *)ctx->avcInfoBuf;
	if (frameIdx == -1 && srcYcbcr[4] == 2)
		frameIdx = (s32)srcYcbcr[12];
	s32 widthMb, heightMb;
	u32 *srcPlanes;
	if (frameIdx == -1) {
		if (dstYcbcr[4] == 2)
			return 0x806101fe;
		heightMb = (s32)srcYcbcr[0];
		widthMb = (s32)srcYcbcr[1];
		srcPlanes = srcYcbcr + 4;
	} else {
		heightMb = (s32)(avcInfo[3] >> 4);
		widthMb = (s32)(avcInfo[2] >> 4);
		srcPlanes = (u32 *)(ctx->avcFrameInfoBuf + 44 * frameIdx);
	}
	return mpegAvcFillDetailInfo(dstYcbcr, frameIdx, widthMb, heightMb, srcPlanes);
}

/* 30: 0x1b08 (168B) */
static s32 mpegAvcDecodeDetailInternal(u32 *mp_ptr, u32 *bufList, u32 *numPic)
{
	SceMpegCtx *ctx = (SceMpegCtx *)(*mp_ptr);
	u32 *avcInfo = (u32 *)ctx->avcInfoBuf;
	s32 picCount = (s32)avcInfo[8];
	*numPic = (u32)picCount;
	s32 ret = 0;
	if (picCount > 0) {
		for (s32 i = 0; ; ) {
			u32 *dst = (u32 *)bufList[i];
			s32 curIdx = i++;
			if (dst == 0)
				return 0x806101fe;
			ret = mpegAvcConvertYCbCrInternal(mp_ptr, dst, 0, curIdx);
			if (ret != 0 || i >= (s32)(*numPic))
				break;
		}
	}
	return ret;
}

/* 31: 0x1bb0 (468B) */
static s32 mpegAvcFillDetailInfo(u32 *dstYcbcr, s32 frameIdx, s32 widthMb, s32 heightMb, u32 *srcPlanes)
{
	if (dstYcbcr[4] == 3) {
		s32 width = widthMb * 16;
		s32 height = heightMb * 16;
		u32 ySize = (u32)(width * height);
		u32 cSize = ySize >> 2;
		s32 retDim = (widthMb << 20) | height;
		u32 convParams[14];
		convParams[0] = (u32)width;
		convParams[1] = (u32)height;
		convParams[2] = dstYcbcr[3];
		convParams[3] = srcPlanes[0];
		convParams[4] = srcPlanes[2];
		convParams[5] = srcPlanes[4];
		convParams[6] = srcPlanes[6];
		convParams[7] = srcPlanes[1];
		convParams[8] = srcPlanes[3];
		convParams[9] = srcPlanes[5];
		convParams[10] = srcPlanes[7];
		if ((widthMb & 7) == 7) {
			retDim = ((width + 16) << 16) | height;
			cSize += (u32)(heightMb << 8) >> 2;
			ySize += (u32)(heightMb << 8);
		}
		u32 dstBase = dstYcbcr[5];
		convParams[11] = dstBase;
		convParams[12] = dstBase + ySize;
		convParams[13] = dstBase + ySize + cSize;
		s32 ret = sceVideocodec_D95C24D5(convParams);
		if (ret >= 0)
			return retDim;
		return ret;
	}
	dstYcbcr[1] = (u32)widthMb;
	dstYcbcr[0] = (u32)heightMb;
	dstYcbcr[2] = 0;
	dstYcbcr[3] = 0;
	if (dstYcbcr[4] == 2) {
		dstYcbcr[12] = (u32)frameIdx;
		return 0;
	}
	u32 srcDesc[12];
	srcDesc[0] = (u32)(heightMb * 16);
	srcDesc[1] = (u32)(widthMb * 16);
	srcDesc[2] = 0;
	srcDesc[3] = 0;
	srcDesc[4] = srcPlanes[0] + g_csc_offsets[0];
	srcDesc[5] = srcPlanes[1] + g_csc_offsets[1];
	srcDesc[6] = srcPlanes[2] + g_csc_offsets[0];
	srcDesc[7] = srcPlanes[3] + g_csc_offsets[1];
	srcDesc[8] = srcPlanes[4] + g_csc_offsets[2];
	srcDesc[9] = srcPlanes[5] + g_csc_offsets[3];
	srcDesc[10] = srcPlanes[6] + g_csc_offsets[2];
	srcDesc[11] = srcPlanes[7] + g_csc_offsets[3];
	return sceMpegBaseYCrCbCopy(dstYcbcr, srcDesc, 3);
}

/* 32: 0x1d84 (168B) */
static s32 mpegAtracDecodeInternal(u32 *mp_ptr, u32 *au, void *pcmBuf, s32 initFlag)
{
	s32 ret = mpegValidateHandle(mp_ptr);
	if (ret != 0)
		return ret;
	SceMpegCtx *ctx = (SceMpegCtx *)(*mp_ptr);
	u32 *awork = (u32 *)ctx->atracWorkPtr;
	if (initFlag == 1) {
		u32 es = au[4];
		ret = mpegAcodecSetup(awork, ctx->atracEdramAddr, ctx->atracWorkMemSize,
		                      (const u8 *)(es + 2), (const u8 *)(es + 4));
		if (ret != 0)
			return ret;
	}
	awork[8] = (u32)pcmBuf;
	awork[6] = au[4];
	return mpegAcodecDecode(awork);
}

/* 33: 0x1e2c (232B) */
static s32 mpegQueryStreamOffsetInternal(u32 *mp_ptr, const u8 *header, u32 *outOffset)
{
	SceMpegCtx *ctx = (SceMpegCtx *)(*mp_ptr);
	if ((mpegGetInitFlags() & 3) != 3) {
		*outOffset = 0;
		return 0x80618009;
	}
	if (mpegStrcmp((const char *)ctx, "LIBMPEG") != 0 ||
	    mpegStrncmp((const char *)header, "PSMF", 4) != 0) {
		*outOffset = 0;
		return 0x806101fe;
	}
	s32 ver = mpegCheckPsmfVersion(header);
	ctx->psmfVersion = ver;
	if (ver < 0) {
		*outOffset = 0;
		return 0x80610002;
	}
	u32 off = mpegReadBe32(header + 8);
	*outOffset = off;
	if ((off & 0x7ffu) != 0 || off == 0) {
		*outOffset = 0;
		return 0x806101fe;
	}
	return 0;
}

/* 34: 0x1f14 (92B) */
static s32 mpegValidateHandle(u32 *mp_ptr)
{
	u32 mp = *mp_ptr;
	if ((mpegGetInitFlags() & 3) != 3)
		return 0x80618009;
	return (mpegStrcmp((const char *)mp, "LIBMPEG") == 0) ? 0 : (s32)0x806101fe;
}

/* 35: 0x1f70 (68B) */
static s32 mpegQueryStreamSizeInternal(const u8 *header, u32 *outSize)
{
	u32 sz = mpegReadBe32(header + 12);
	*outSize = sz;
	if ((sz & 0x7ffu) != 0) {
		*outSize = 0;
		return 0x806101fe;
	}
	return 0;
}

/* 36: 0x1fb4 (168B) */
static s32 mpegCheckPsmfVersion(const u8 *header)
{
	const char *ver = (const char *)(header + 4);
	if (mpegStrncmp(ver, "0012", 4) == 0)
		return 0;
	if (mpegStrncmp(ver, "0013", 4) == 0)
		return 1;
	if (mpegStrncmp(ver, "0014", 4) == 0)
		return 2;
	if (mpegStrncmp(ver, "0015", 4) == 0)
		return 3;
	return -1;
}

/* 37: 0x205c (460B) */
static u32 *mpegFindStreamSlot(u32 code, u32 mp, s32 mode)
{
	SceMpegCtx *ctx = (SceMpegCtx *)mp;
	if ((code & 0xffffe000u) == 0xbf01e000u) {
		u32 ch = (code >> 8) & 0x1fu;
		if (mode == 1)
			return (u32 *)(ctx->streamQueueTable + ch * 16);
		return (u32 *)(ctx->streamDescTable + ch * 68);
	}
	if ((code & 0xe0000000u) == 0xe0000000u) {
		u32 ch = (code >> 24) & 0x1fu;
		if (mode == 1)
			return (u32 *)(ctx->streamQueueTable + ch * 16 + 256);
		return (u32 *)(ctx->streamDescTable + ch * 68 + 1088);
	}
	u32 hi = code & 0xfff00000u;
	if (hi == 0xbd000000u) {
		u32 ch = (code >> 16) & 0x42ffu;
		if (mode == 1)
			return (u32 *)(ctx->streamQueueTable + ch * 16 + 512);
		return (u32 *)(ctx->streamDescTable + ch * 68 + 2176);
	}
	if ((code & 0xffe00000u) == 0xbd800000u) {
		u32 ch = (code >> 16) & 0x427fu;
		if (mode == 1)
			return (u32 *)(ctx->streamQueueTable + ch * 16 + 1024);
		return (u32 *)(ctx->streamDescTable + ch * 68 + 4352);
	}
	if (hi == 0xbd100000u) {
		u32 ch = (code >> 16) & 0x42efu;
		if (mode == 1)
			return (u32 *)(ctx->streamQueueTable + ch * 16 + 768);
		return (u32 *)(ctx->streamDescTable + ch * 68 + 3264);
	}
	if (hi == 0xbd200000u) {
		u32 ch = (code >> 16) & 0x42dfu;
		if (mode == 1)
			return (u32 *)(ctx->streamQueueTable + ch * 16 + 1536);
		return (u32 *)(ctx->streamDescTable + ch * 68 + 6528);
	}
	return (u32 *)-1;
}

/* 38: 0x2228 (192B) */
static s32 mpegPeekStreamPackets(u32 *queue, s32 skip, s32 maxCount, void *dst, s32 *outCount)
{
	u32 head = queue[1];
	*outCount = 0;
	if (head == (u32)-1)
		return 0x806101fe;
	s32 total = (s32)queue[3];
	if (skip >= total)
		return 0;
	SceMpegPacketNode *node = (SceMpegPacketNode *)head;
	for (s32 i = 0; i < skip; i++)
		node = (SceMpegPacketNode *)node->nextNode;
	s32 count = total - skip;
	if (count > maxCount)
		count = maxCount;
	u8 *outPtr = (u8 *)dst;
	for (s32 i = 0; i < count; i++) {
		sceKernelMemcpy(outPtr, node->pesInfo, 44);
		outPtr += 44;
		node = (SceMpegPacketNode *)node->nextNode;
	}
	*outCount = count;
	return 0;
}

/* 39: 0x22e8 (128B) */
static s32 mpegPopStreamPackets(u32 *queue, s32 count, s32 *outPopped)
{
	u32 head = queue[1];
	*outPopped = 0;
	if (head == (u32)-1)
		return 0x806101fe;
	SceMpegPacketNode *node = (SceMpegPacketNode *)head;
	s32 toPop = (s32)queue[3];
	if (count < toPop)
		toPop = count;
	for (s32 i = 0; i < toPop; i++) {
		SceMpegPacketNode *next = (SceMpegPacketNode *)node->nextNode;
		node->nextNode = (u32)-1;
		node->prevNode = (u32)-1;
		node = next;
		queue[3] -= 1;
	}
	*outPopped = toPop;
	queue[1] = (u32)node;
	if (queue[3] == 0) {
		queue[1] = (u32)-1;
		queue[0] = (u32)-1;
	}
	return 0;
}

/* 40: 0x2368 (828B) */
static s32 mpegRingbufferPutDemux(u32 *mp_ptr, u32 *rb, s32 numPackets, s32 *outParsed)
{
	SceMpegCtx *ctx = (SceMpegCtx *)(*mp_ptr);
	s32 ret = mpegValidateHandle(mp_ptr);
	if (ret != 0)
		return ret;
	*outParsed = 0;
	if (rb[4] != 2048)
		return 0x806101fe;
	s32 avail = mpegRingbufferAvailableSizeInternal(rb);
	s32 pktCnt = (numPackets < avail) ? numPackets : avail;
	if (pktCnt * (s32)rb[4] == 0)
		return 0;
	s32 totalBits = pktCnt * (s32)rb[4] * 8;
	mpegLockLwMutex(ctx->dataMutex);
	u32 bs[8];
	u32 packInfo[6];
	u32 pesInfo[14];
	u32 pktSize = rb[4];
	u32 readIdx = rb[1];
	u32 totalPkts = rb[0];
	u32 rbBase = rb[5];
	bs[0] = 0;
	bs[1] = 0;
	bs[2] = rbBase + readIdx * pktSize;
	bs[3] = bs[2];
	bs[4] = 0;
	bs[5] = rbBase;
	bs[6] = rbBase + totalPkts * pktSize;
	bs[7] = totalPkts * pktSize;
	mpegBitstreamSkip(bs, 0);
	while (1) {
		if (bs[0] == 0x000001ba) {
			mpegParsePackHeader(bs, packInfo);
		}
		s32 pesCount = 0;
		u32 pesOffset = 0;
		while ((bs[0] >> 8) == 1 && bs[0] != 0x000001ba && bs[0] != 0x000001b9 && (s32)bs[1] < totalBits) {
			if (mpegParsePesHeader(bs, pesInfo) < 0) {
				mpegUnlockLwMutex(ctx->dataMutex);
				return 0x80618007;
			}
			if ((s32)bs[1] <= totalBits) {
				u32 *slot = mpegFindStreamSlot(pesInfo[0], (u32)ctx, 1);
				if (slot != (u32 *)-1 && slot[2] != (u32)-1) {
					u32 curPkt = rb[1] + ((*outParsed * (s32)rb[4]) / (s32)rb[4]);
					if (curPkt >= rb[0])
						curPkt -= rb[0];
					if (pesCount >= 2) {
						mpegUnlockLwMutex(ctx->dataMutex);
						return 0x80618007;
					}
					u32 oldTail = slot[0];
					SceMpegPacketNode *node = (SceMpegPacketNode *)(ctx->packetNodePool + 104 * curPkt + pesOffset);
					node->nextNode = (u32)-1;
					node->prevNode = oldTail;
					if (oldTail != (u32)-1)
						((SceMpegPacketNode *)oldTail)->nextNode = (u32)node;
					slot[0] = (u32)node;
					sceKernelMemcpy(node->pesInfo, pesInfo, 44);
					if (slot[1] == (u32)-1)
						slot[1] = slot[0];
					slot[3] += 1;
				}
				*outParsed = (s32)((bs[1] >> 3) / rb[4]);
			}
			pesCount++;
			pesOffset += 52;
		}
		if ((s32)bs[1] >= totalBits)
			break;
		if (bs[0] != 0x000001ba) {
			mpegUnlockLwMutex(ctx->dataMutex);
			if (((bs[1] >> 3) % rb[4]) != 0)
				return 0x80618007;
			return 0;
		}
	}
	if (((bs[1] >> 3) % rb[4]) != 0) {
		mpegUnlockLwMutex(ctx->dataMutex);
		return 0x80618007;
	}
	mpegUnlockLwMutex(ctx->dataMutex);
	return 0;
}

/* 41: 0x26a4 (276B) */
static void mpegBitstreamSkip(u32 *bs, u32 num_bits)
{
	while (num_bits >= 25) {
		s32 rem = (s32)bs[4] - 24;
		bs[0] <<= 24;
		bs[4] = rem;
		if (rem < 25) {
			u32 rb_end = bs[6];
			do {
				u8 *p = (u8 *)bs[3];
				s32 vbits = (s32)bs[4];
				u32 cur = bs[0];
				u8 b = *p;
				u8 *next = p + 1;
				s32 next_vbits = vbits + 8;
				bs[0] = cur | ((u32)b << (24 - vbits));
				if ((u32)next < rb_end)
					bs[3] = (u32)next;
				else
					bs[3] = bs[5];
				bs[4] = next_vbits;
			} while ((s32)bs[4] < 25);
		}
		num_bits -= 24;
		bs[1] += 24;
	}
	s32 rem = (s32)bs[4] - (s32)num_bits;
	bs[0] <<= num_bits;
	bs[4] = rem;
	if (rem < 25) {
		u32 rb_end = bs[6];
		do {
			u8 *p = (u8 *)bs[3];
			s32 vbits = (s32)bs[4];
			u32 cur = bs[0];
			u8 b = *p;
			u8 *next = p + 1;
			s32 next_vbits = vbits + 8;
			bs[0] = cur | ((u32)b << (24 - vbits));
			if ((u32)next < rb_end)
				bs[3] = (u32)next;
			else
				bs[3] = bs[5];
			bs[4] = next_vbits;
		} while ((s32)bs[4] < 25);
	}
	bs[1] += num_bits;
}

/* 42: 0x27b8 (420B) */
static s32 mpegParsePackHeader(u32 *bs, u32 *pack_info)
{
	mpegBitstreamSkip(bs, 34);
	u32 scr_32_30 = bs[0];
	mpegBitstreamSkip(bs, 3);
	mpegBitstreamSkip(bs, 1);
	u32 scr_29_15 = (bs[0] >> 17);
	mpegBitstreamSkip(bs, 15);
	mpegBitstreamSkip(bs, 1);
	u32 scr_14_0 = (bs[0] >> 17);
	mpegBitstreamSkip(bs, 15);
	mpegBitstreamSkip(bs, 1);
	u32 scr_ext = (bs[0] >> 23);
	mpegBitstreamSkip(bs, 9);
	pack_info[0] = scr_ext;
	mpegBitstreamSkip(bs, 30);
	u32 stuffing = bs[0] >> 29;
	mpegBitstreamSkip(bs, 3);
	pack_info[1] = ((scr_32_30 >> 29) << 30) | (scr_29_15 << 15) | scr_14_0;
	pack_info[2] = scr_32_30 >> 31;
	for (u32 i = 0; i < stuffing; i++)
		mpegBitstreamSkip(bs, 8);
	if (bs[0] == 0x000001bb) {
		pack_info[3] = 1;
		mpegBitstreamSkip(bs, 56);
		mpegBitstreamSkip(bs, 40);
		while ((bs[0] >> 31) == 1)
			mpegBitstreamSkip(bs, 24);
	} else {
		pack_info[3] = 0;
	}
	return 1;
}

/* 43: 0x295c (1664B) */
static s32 mpegParsePesHeader(u32 *bs, u32 *pesInfo)
{
	u32 startPtr = bs[2] + (bs[1] >> 3);
	if (startPtr >= bs[6])
		startPtr -= bs[7];
	pesInfo[9] = startPtr;
	mpegBitstreamSkip(bs, 24);
	u32 streamId = bs[0] >> 24;
	mpegBitstreamSkip(bs, 8);
	pesInfo[0] = streamId << 24;
	u32 pesLen = bs[0] >> 16;
	mpegBitstreamSkip(bs, 16);
	u32 sid = pesInfo[0];
	pesInfo[1] = pesLen;
	pesInfo[4] = (u32)-1;
	pesInfo[5] = (u32)-1;
	pesInfo[3] = (u32)-1;
	pesInfo[6] = (u32)-1;
	if (sid != 0xbc000000 && sid != 0xbe000000 && sid != 0xbf000000 &&
	    sid != 0xf0000000 && sid != 0xf1000000 && sid != 0xff000000 &&
	    sid != 0xf2000000 && sid != 0xf8000000) {
		mpegBitstreamSkip(bs, 2);
		u32 scrambling = bs[0] >> 30;
		mpegBitstreamSkip(bs, 2);
		pesInfo[2] = scrambling;
		mpegBitstreamSkip(bs, 4);
		u32 ptsDtsFlags = bs[0] >> 30;
		mpegBitstreamSkip(bs, 2);
		u32 escrFlag = bs[0] >> 31;
		mpegBitstreamSkip(bs, 1);
		u32 rateDsmCopyCrc = bs[0] >> 28;
		mpegBitstreamSkip(bs, 4);
		u32 extFlag = bs[0] >> 31;
		mpegBitstreamSkip(bs, 1);
		u32 hdrDataLen = bs[0] >> 24;
		mpegBitstreamSkip(bs, 8);
		u32 hdrStartBit = bs[1];
		if ((ptsDtsFlags & 2) != 0) {
			mpegBitstreamSkip(bs, 4);
			u32 ptsTop = bs[0];
			mpegBitstreamSkip(bs, 3);
			mpegBitstreamSkip(bs, 1);
			u32 ptsMid = bs[0];
			mpegBitstreamSkip(bs, 15);
			mpegBitstreamSkip(bs, 1);
			u32 ptsLow = bs[0];
			mpegBitstreamSkip(bs, 15);
			mpegBitstreamSkip(bs, 1);
			pesInfo[4] = ((ptsTop >> 29) << 30) | ((ptsMid >> 17) << 15) | (ptsLow >> 17);
			pesInfo[3] = ptsTop >> 31;
		}
		if (ptsDtsFlags == 3) {
			mpegBitstreamSkip(bs, 4);
			u32 dtsTop = bs[0];
			mpegBitstreamSkip(bs, 3);
			mpegBitstreamSkip(bs, 1);
			u32 dtsMid = bs[0];
			mpegBitstreamSkip(bs, 15);
			mpegBitstreamSkip(bs, 1);
			u32 dtsLow = bs[0];
			mpegBitstreamSkip(bs, 15);
			mpegBitstreamSkip(bs, 1);
			pesInfo[6] = ((dtsTop >> 29) << 30) | ((dtsMid >> 17) << 15) | (dtsLow >> 17);
			pesInfo[5] = dtsTop >> 31;
		}
		if (escrFlag == 1)
			mpegBitstreamSkip(bs, 48);
		if (rateDsmCopyCrc != 0)
			mpegBitstreamSkip(bs, g_pes_ext_bits[rateDsmCopyCrc]);
		if (extFlag == 1) {
			u32 pesPrivFlag = bs[0] >> 31;
			mpegBitstreamSkip(bs, 1);
			u32 packHdrFlag = bs[0] >> 31;
			mpegBitstreamSkip(bs, 1);
			u32 seqCntFlag = bs[0] >> 31;
			mpegBitstreamSkip(bs, 1);
			u32 stdBufFlag = bs[0] >> 31;
			mpegBitstreamSkip(bs, 1);
			mpegBitstreamSkip(bs, 3);
			u32 extTwoFlag = bs[0] >> 31;
			mpegBitstreamSkip(bs, 1);
			if (pesPrivFlag == 1) {
				mpegBitstreamSkip(bs, 48);
				mpegBitstreamSkip(bs, 48);
				mpegBitstreamSkip(bs, 32);
			}
			if (packHdrFlag == 1)
				return 0x80618007;
			if (seqCntFlag == 1)
				mpegBitstreamSkip(bs, 16);
			if (stdBufFlag == 1)
				mpegBitstreamSkip(bs, 16);
			if (extTwoFlag == 1) {
				mpegBitstreamSkip(bs, 1);
				u32 extTwoLen = bs[0] >> 25;
				mpegBitstreamSkip(bs, 7);
				for (u32 i = 0; i < extTwoLen; i++)
					mpegBitstreamSkip(bs, 8);
			}
		}
		u32 remHdrBytes = hdrDataLen - ((bs[1] - hdrStartBit) >> 3);
		if (remHdrBytes != 0) {
			u32 newBit = bs[1] + (remHdrBytes << 3);
			u32 newPtr = bs[2] + (newBit >> 3);
			bs[0] = 0;
			bs[4] = 0;
			bs[1] = newBit;
			if (newPtr < bs[6])
				bs[3] = newPtr;
			else
				bs[3] = newPtr - bs[7];
			mpegBitstreamSkip(bs, 0);
		}
		u32 remAfterHdr = pesInfo[1] - hdrDataLen;
		u32 dataLen = remAfterHdr - 3;
		pesInfo[8] = dataLen;
		u32 dataPtr = bs[2] + (bs[1] >> 3);
		if (dataPtr >= bs[6])
			dataPtr -= bs[7];
		pesInfo[7] = dataPtr;
		if (pesInfo[0] == 0xbd000000) {
			u32 privSubHdr = bs[0] >> 8;
			mpegBitstreamSkip(bs, 24);
			pesInfo[0] |= privSubHdr;
			dataLen = remAfterHdr - 6;
		}
		if (dataLen != 0) {
			u32 newBit = bs[1] + (dataLen << 3);
			u32 newPtr = bs[2] + (newBit >> 3);
			bs[0] = 0;
			bs[4] = 0;
			bs[1] = newBit;
			if (newPtr < bs[6])
				bs[3] = newPtr;
			else
				bs[3] = newPtr - bs[7];
			mpegBitstreamSkip(bs, 0);
		}
		return 1;
	} else if (sid == 0xbc000000 || sid == 0xbf000000 || sid == 0xf0000000 ||
	           sid == 0xf1000000 || sid == 0xff000000 || sid == 0xf2000000 || sid == 0xf8000000) {
		u32 dataLen = pesInfo[1];
		if (sid == 0xbf000000) {
			u32 dataPtr = bs[2] + (bs[1] >> 3);
			if (dataPtr >= bs[6])
				dataPtr -= bs[7];
			pesInfo[8] = dataLen;
			pesInfo[7] = dataPtr;
			u32 privSubHdr = bs[0] >> 8;
			mpegBitstreamSkip(bs, 24);
			pesInfo[0] |= privSubHdr;
			dataLen -= 3;
		}
		if (dataLen != 0) {
			u32 newBit = bs[1] + (dataLen << 3);
			u32 newPtr = bs[2] + (newBit >> 3);
			bs[0] = 0;
			bs[4] = 0;
			bs[1] = newBit;
			if (newPtr < bs[6])
				bs[3] = newPtr;
			else
				bs[3] = newPtr - bs[7];
			mpegBitstreamSkip(bs, 0);
		}
		return 1;
	} else if (sid == 0xbe000000) {
		u32 dataLen = pesInfo[1];
		if (dataLen != 0) {
			u32 newBit = bs[1] + (dataLen << 3);
			u32 newPtr = bs[2] + (newBit >> 3);
			bs[0] = 0;
			bs[4] = 0;
			bs[1] = newBit;
			if (newPtr < bs[6])
				bs[3] = newPtr;
			else
				bs[3] = newPtr - bs[7];
			mpegBitstreamSkip(bs, 0);
		}
		return 1;
	}
	return 1;
}

/* 44: 0x2fdc (108B) */
static s32 mpegRingbufferPutInternal(u32 *rb, s32 numPackets, s32 available)
{
	s32 written = mpegRingbufferWritePackets(rb, numPackets, available);
	if (written < 0)
		return written;
	s32 parsed = 0;
	s32 ret = mpegRingbufferPutDemux((u32 *)rb[10], rb, written, &parsed);
	s32 consumed = mpegRingbufferConsume(rb, written);
	if (ret == 0)
		return consumed;
	return ret;
}

/* 45: 0x3048 (440B) */
static s32 mpegRingbufferWritePackets(u32 *rb, s32 numPackets, s32 available)
{
	u32 *mp_ptr = (u32 *)rb[10];
	SceMpegCtx *ctx = (SceMpegCtx *)(*mp_ptr);
	s32 ret = mpegValidateHandle(mp_ptr);
	if (ret != 0)
		return ret;
	typedef s32 (*SceMpegRbCallback)(void *dst, s32 numPackets, void *cbArg);
	SceMpegRbCallback cb = (SceMpegRbCallback)rb[6];
	s32 rem = (numPackets < available) ? numPackets : available;
	mpegLockLwMutex(ctx->ringbufPutMutex);
	mpegLockLwMutex(ctx->dataMutex);
	s32 totalWritten = 0;
	while (rem > 0) {
		s32 totalPkts = (s32)rb[0];
		s32 writeIdx = (s32)rb[2];
		s32 toWrite = rem;
		if (totalPkts < writeIdx + rem)
			toWrite = totalPkts - writeIdx;
		if (cb == 0) {
			mpegUnlockLwMutex(ctx->dataMutex);
			mpegUnlockLwMutex(ctx->ringbufPutMutex);
			return 0x80610103;
		}
		mpegUnlockLwMutex(ctx->dataMutex);
		s32 got = cb((void *)(rb[5] + rb[2] * rb[4]), toWrite, (void *)rb[7]);
		mpegLockLwMutex(ctx->dataMutex);
		if (got < 0) {
			if (totalWritten <= 0) {
				mpegUnlockLwMutex(ctx->dataMutex);
				mpegUnlockLwMutex(ctx->ringbufPutMutex);
				return got;
			}
			got = 0;
		}
		totalWritten += got;
		s32 nextWrite = (s32)rb[2] + got;
		rem -= got;
		rb[2] = (u32)nextWrite;
		if (nextWrite == (s32)rb[0])
			rb[2] = 0;
		rb[3] += (u32)got;
		if (got == 0)
			break;
	}
	mpegUnlockLwMutex(ctx->dataMutex);
	mpegUnlockLwMutex(ctx->ringbufPutMutex);
	return totalWritten;
}

/* 46: 0x3200 (328B) */
static s32 mpegRingbufferResetInternal(u32 *rb)
{
	u32 *mp_ptr = (u32 *)rb[10];
	SceMpegCtx *ctx = (SceMpegCtx *)(*mp_ptr);
	s32 ret = mpegValidateHandle(mp_ptr);
	if (ret != 0)
		return ret;
	mpegLockLwMutex(ctx->dataMutex);
	if (rb[3] == rb[0]) {
		mpegUnlockLwMutex(ctx->dataMutex);
		return 0;
	}
	s32 minPkt = (s32)rb[1];
	for (s32 i = 0; i < 112; i++) {
		u32 *slot = (u32 *)(ctx->streamQueueTable + (i << 4));
		if (slot[2] != (u32)-1 && slot[1] != (u32)-1) {
			SceMpegPacketNode *headNode = (SceMpegPacketNode *)slot[1];
			mpegRingbufferCheckAddr(rb, &minPkt, headNode->pesInfo[7]);
			if (minPkt == -1)
				break;
		}
	}
	s32 inUse;
	s32 totalPkts = (s32)rb[0];
	s32 unconsumed = (s32)rb[3];
	if (minPkt == -1) {
		inUse = totalPkts - unconsumed;
	} else {
		s32 readIdx = (s32)rb[1];
		if ((s32)rb[2] >= readIdx && readIdx < minPkt)
			inUse = totalPkts - minPkt + readIdx;
		else
			inUse = readIdx - minPkt;
	}
	s32 avail = totalPkts - unconsumed - inUse;
	mpegUnlockLwMutex(ctx->dataMutex);
	return avail;
}

/* 47: 0x3348 (24B) */
static s32 mpegRingbufferQueryMemSizeInternal(s32 packets)
{
	return (packets << 11) + packets * 0x68;
}

/* 48: 0x3360 (28B) */
static s32 mpegRingbufferQueryPackNumInternal(u32 memSize)
{
	return memSize / 0x868u;
}

/* 49: 0x337c (220B) */
static s32 mpegRingbufferConstructInternal(u32 *rb, s32 packets, void *rbData, s32 size, void *cb, void *cbArg)
{
	if (size < packets * 2152)
		return 0x80610022;
	mpegLockLwMutex(g_libmpeg_mutex);
	rb[4] = 2048;
	rb[5] = (u32)rbData;
	rb[8] = (u32)rbData + (packets << 11);
	rb[0] = (u32)packets;
	rb[6] = (u32)cb;
	rb[7] = (u32)cbArg;
	rb[3] = 0;
	rb[2] = 0;
	rb[1] = 0;
	rb[10] = 0;
	u32 gpVal;
	__asm__ __volatile__("move %0, $gp" : "=r"(gpVal));
	rb[11] = gpVal;
	mpegUnlockLwMutex(g_libmpeg_mutex);
	return 0;
}

/* 50: 0x3458 (8B) */
static s32 mpegRingbufferDestructInternal(u32 *rb)
{
	(void)rb;
	return 0;
}

/* 51: 0x3460 (112B) */
static s32 mpegRingbufferAvailableSizeInternal(u32 *rb)
{
	u32 *mp_ptr = (u32 *)rb[10];
	SceMpegCtx *ctx = (SceMpegCtx *)(*mp_ptr);
	s32 ret = mpegValidateHandle(mp_ptr);
	if (ret != 0)
		return ret;
	mpegLockLwMutex(ctx->dataMutex);
	s32 count = (s32)rb[3];
	mpegUnlockLwMutex(ctx->dataMutex);
	return count;
}

/* 52: 0x34d0 (196B) */
static s32 mpegRingbufferConsume(u32 *rb, s32 consumed)
{
	u32 *mp_ptr = (u32 *)rb[10];
	SceMpegCtx *ctx = (SceMpegCtx *)(*mp_ptr);
	s32 ret = mpegValidateHandle(mp_ptr);
	if (ret != 0)
		return ret;
	mpegLockLwMutex(ctx->dataMutex);
	s32 cur = (s32)rb[3];
	s32 take = (consumed < cur) ? consumed : cur;
	s32 rem = take;
	while (rem > 0) {
		s32 total = (s32)rb[0];
		s32 readIdx = (s32)rb[1];
		s32 step = (total >= readIdx + rem) ? rem : (total - readIdx);
		rem -= step;
		rb[1] = (u32)(readIdx + step);
		if (readIdx + step == total)
			rb[1] = 0;
	}
	rb[3] = (u32)(cur - take);
	mpegUnlockLwMutex(ctx->dataMutex);
	return take;
}

/* 53: 0x3594 (176B) */
static s32 mpegRingbufferCheckAddr(u32 *rb, s32 *minPkt, u32 addr)
{
	s32 rbStart = (s32)rb[5];
	s32 pktIdx = ((s32)addr - rbStart) / (s32)rb[4];
	s32 writeIdx = (s32)rb[2];
	if (pktIdx == writeIdx && (s32)addr - rbStart > 0) {
		*minPkt = -1;
		return rbStart;
	}
	s32 readIdx = (s32)rb[1];
	if (writeIdx >= readIdx) {
		s32 curMin = *minPkt;
		if (writeIdx >= pktIdx) {
			if (readIdx >= curMin && pktIdx < readIdx && pktIdx < curMin)
				*minPkt = pktIdx;
			return rbStart;
		}
		if (readIdx >= curMin || pktIdx < curMin)
			*minPkt = pktIdx;
	} else {
		if (pktIdx < *minPkt)
			*minPkt = pktIdx;
	}
	return rbStart;
}

/* 54: 0x3644 (1080B) */
static s32 mpegGetAuInternal(u32 *mp_ptr, u32 *stream, u32 streamType, u32 *au)
{
	u32 *slot = mpegFindStreamSlot(stream[0], *mp_ptr, 1);
	if (slot == (u32 *)-1)
		return 0x806101fe;
	s32 status = (s32)0x80618001;
	while (1) {
		s32 totalCopied = 0;
		u32 *tblHdr = (u32 *)mpegGetActiveSlotPtr(mp_ptr);
		SceMpegEsDesc *descs = (SceMpegEsDesc *)(tblHdr + 1);
		s32 remBytes = (s32)stream[9];
		s32 maxPkts = (s32)slot[3];
		if (maxPkts > 99)
			maxPkts = 99;
		u32 esBase = mpegGetAvcEsBufPtr(mp_ptr, (s32)au[4]);
		s32 descIdx = 0;
		s32 popCount = 0;
		s32 peekCnt = 0;
		u32 pesInfo[12];
		s32 copyOverflow = 0;
		while (descIdx < maxPkts && remBytes >= 1) {
			mpegPeekStreamPackets(slot, descIdx, 1, pesInfo, &peekCnt);
			s16 curOff = *(s16 *)&stream[7];
			s32 hdrSkip = (s32)stream[5];
			u32 copyLen = pesInfo[8] - (u32)(hdrSkip + curOff);
			remBytes -= (s32)copyLen;
			u32 dstAddr = esBase + au[5] + (u32)totalCopied;
			u32 srcAddr = pesInfo[7] + (u32)hdrSkip + (u32)curOff;
			totalCopied += (s32)copyLen;
			if (remBytes <= 0) {
				if (remBytes != 0) {
					s32 excess = mpegAbs(remBytes);
					copyLen -= (u32)excess;
					*(s16 *)&stream[7] = (s16)((s32)copyLen + curOff);
					totalCopied -= mpegAbs(remBytes);
				} else {
					*(s16 *)&stream[7] = 0;
					popCount += 1;
				}
				if (popCount > 0 || (s32)stream[8] > 0 || stream[13] == 1) {
					stream[13] = 0;
					stream[6] |= 1u;
				}
			}
			SceMpegEsDesc *desc = &descs[descIdx];
			desc->srcAddr = srcAddr;
			desc->nextDesc = (u32)(desc + 1);
			desc->dstAddr = dstAddr;
			if (copyLen >= 0x1000u) {
				status = (s32)0x80618007;
				desc->copySize = 0;
				copyOverflow = 1;
				break;
			}
			desc->copySize = copyLen;
			if (remBytes <= 0) {
				u32 align = stream[10];
				descIdx++;
				if (align != 0) {
					u32 mod = (u32)totalCopied % align;
					if (mod != 0) {
						u32 padLen = align - mod;
						SceMpegEsDesc *padDesc = &descs[descIdx];
						if (padLen >= 0x1000u)
							padLen = 0;
						padDesc->copySize = padLen;
						descIdx++;
						padDesc->srcAddr = (u32)g_zero_pad;
						padDesc->dstAddr = dstAddr + copyLen;
						padDesc->nextDesc = (u32)(padDesc + 1);
						totalCopied += (s32)(align - ((u32)totalCopied % align));
					}
				}
				status = 0;
				stream[11] = 0;
				break;
			}
			descIdx++;
			*(s16 *)&stream[7] = 0;
			popCount += 1;
		}
		if (!copyOverflow) {
			if (descIdx >= maxPkts && remBytes >= 1) {
				stream[9] = (u32)remBytes;
				stream[11] += (u32)totalCopied;
				stream[8] += (u32)popCount;
			} else {
				stream[8] = 0;
			}
			descs[descIdx - 1].nextDesc = 0;
			if (streamType >= 7u || g_au_max_sizes[streamType] < au[5] + (u32)totalCopied) {
				status = (s32)0x80618007;
			} else {
				u32 modeFlags = stream[12];
				if ((modeFlags & 5u) == 0 && ((modeFlags & 2u) == 0 || (s32)modeFlags < 0)) {
					mpegEsNodeFlushCache((u32 *)descs);
					u32 totalSize = au[5] + (u32)totalCopied;
					if ((esBase & 0x1fffffffu) > 0x3fffffu)
						sceKernelDcacheWritebackInvalidateRange((const void *)esBase, totalSize);
					s32 copyRet = sceMpegBasePESpacketCopy(descs);
					if (copyRet != 0)
						status = copyRet;
				}
			}
		}
		mpegPopStreamPackets(slot, popCount, &peekCnt);
		mpegNoopHandle(mp_ptr);
		au[5] += (u32)totalCopied;
		if (!(descIdx >= 99 && status == (s32)0x80618001))
			return status;
	}
}

/* 55: 0x3a7c (556B) */
static s32 mpegGetAtracAuInternal(u32 *mp_ptr, u32 *stream, u32 *au, u32 *attrOut)
{
	SceMpegCtx *ctx = (SceMpegCtx *)(*mp_ptr);
	s32 ret = mpegValidateHandle(mp_ptr);
	if (ret != 0)
		return ret;
	mpegLockLwMutex(ctx->dataMutex);
	u32 *slot = mpegFindStreamSlot(stream[0], *mp_ptr, 1);
	if (slot == (u32 *)-1) {
		mpegUnlockLwMutex(ctx->dataMutex);
		return 0x806101fe;
	}
	u32 pesInfo[12];
	s32 got = 0;
	mpegPeekStreamPackets(slot, 0, 1, pesInfo, &got);
	if (got != 1) {
		mpegUnlockLwMutex(ctx->dataMutex);
		return 0x80618001;
	}
	if (stream[11] == 0) {
		au[5] = 0;
		while (1) {
			mpegPeekStreamPackets(slot, 0, 1, pesInfo, &got);
			if (got != 1) {
				mpegUnlockLwMutex(ctx->dataMutex);
				return 0x80618001;
			}
			const u8 *payload = (const u8 *)pesInfo[7];
			if ((stream[6] & 1u) == 0) {
				au[0] = (u32)-1;
				au[1] = (u32)-1;
				au[2] = (u32)-1;
				au[3] = (u32)-1;
				stream[16] = payload[1];
				stream[9] = (u32)(160 * (payload[1] & 0xf));
				break;
			}
			s16 hdrOff = (s16)mpegReadBe16(payload + 2);
			*(s16 *)&stream[7] = hdrOff;
			if (hdrOff != -1) {
				au[0] = pesInfo[3];
				au[1] = pesInfo[4];
				au[2] = pesInfo[5];
				au[3] = pesInfo[6];
				stream[6] = 0;
				stream[16] = payload[1];
				stream[9] = (u32)(160 * (payload[1] & 0xf));
				break;
			}
			mpegPopStreamPackets(slot, 1, &got);
		}
	}
	if (attrOut != 0)
		*attrOut = stream[16];
	ret = mpegGetAuInternal(mp_ptr, stream, 2, au);
	mpegUnlockLwMutex(ctx->dataMutex);
	if ((stream[12] & 4u) != 0 && ret == 0) {
		stream[12] &= ~4u;
		return mpegGetAtracAuInternal(mp_ptr, stream, au, attrOut);
	}
	return ret;
}

/* 56: 0x3ca8 (1016B) */
static s32 mpegGetAvcAuInternal(u32 *mp_ptr, u32 *stream, u32 streamType, u32 *au, void *headerOut)
{
	SceMpegCtx *ctx = (SceMpegCtx *)(*mp_ptr);
	s32 ret = mpegValidateHandle(mp_ptr);
	if (ret != 0)
		return ret;
	mpegLockLwMutex(ctx->dataMutex);
	u32 *slot = mpegFindStreamSlot(stream[0], *mp_ptr, 1);
	if (slot == (u32 *)-1) {
		mpegUnlockLwMutex(ctx->dataMutex);
		return 0x806101fe;
	}
	u32 pesInfo[12];
	s32 got = 0;
	mpegPeekStreamPackets(slot, 0, 1, pesInfo, &got);
	if (got != 1) {
		mpegUnlockLwMutex(ctx->dataMutex);
		return 0x80618001;
	}
	if (stream[14] != 0 || (stream[11] == 0 && stream[13] != 1)) {
		if (stream[11] == 0) {
			au[5] = 0;
			while (1) {
				mpegPeekStreamPackets(slot, 0, 1, pesInfo, &got);
				if (got != 1) {
					mpegUnlockLwMutex(ctx->dataMutex);
					return 0x80618001;
				}
				if (pesInfo[3] != (u32)-1 || pesInfo[4] != (u32)-1 ||
				    pesInfo[5] != (u32)-1 || pesInfo[6] != (u32)-1) {
					if ((stream[6] & 1u) != 0) {
						*(s16 *)&stream[7] = 0;
						au[0] = pesInfo[3];
						au[1] = pesInfo[4];
						au[2] = pesInfo[5];
						au[3] = pesInfo[6];
						stream[6] = 0;
					}
					break;
				}
				mpegPopStreamPackets(slot, 1, &got);
			}
		}
		mpegPeekStreamPackets(slot, 0, 1, pesInfo, &got);
		if (got != 1) {
			mpegUnlockLwMutex(ctx->dataMutex);
			return 0x80618001;
		}
		s32 needHdr = (s32)stream[14];
		s32 pktOff = (s32)(*(s16 *)&stream[7]) + 2;
		s32 pktRem = (s32)pesInfo[8] - pktOff;
		if (needHdr == 0) {
			stream[14] = 8;
			needHdr = 8;
		}
		u8 hdrBuf[8];
		s32 startPos = 8 - needHdr;
		u8 *dstHdr = &hdrBuf[startPos];
		while (pktRem < needHdr) {
			sceKernelMemcpy(dstHdr, (const void *)(pesInfo[7] + pktOff), (u32)pktRem);
			dstHdr += pktRem;
			stream[14] -= (u32)pktRem;
			mpegPeekStreamPackets(slot, 1, 1, pesInfo, &got);
			pktOff = 2;
			if (got != 1) {
				stream[13] = 1;
				break;
			}
			stream[11] += (u32)pktRem;
			pktRem = (s32)pesInfo[8] - 2;
			mpegPopStreamPackets(slot, 1, &got);
			needHdr = (s32)stream[14];
		}
		if (pktRem >= needHdr) {
			sceKernelMemcpy(dstHdr, (const void *)(pesInfo[7] + pktOff), (u32)needHdr);
			*(s16 *)&stream[7] += (s16)stream[14];
			stream[11] += stream[14];
			stream[14] = 0;
		}
		s32 copied = 8 - (s32)stream[14];
		if (startPos < 4) {
			s32 lenLo = (copied >= 5) ? (4 - startPos) : (copied - startPos);
			sceKernelMemcpy((u8 *)&stream[15] + startPos, &hdrBuf[startPos], (u32)lenLo);
		}
		if (copied >= 5 && headerOut != 0) {
			s32 hiStart = (startPos > 4) ? startPos : 4;
			sceKernelMemcpy((u8 *)headerOut + hiStart, &hdrBuf[hiStart], (u32)(copied - hiStart));
		}
		if (stream[14] == 0) {
			stream[9] = mpegReadBe32((const u8 *)&stream[15]) - 4;
			if (headerOut != 0) {
				sceKernelMemcpy(headerOut, &stream[15], 4);
				sceKernelMemcpy((u8 *)headerOut + 4, &hdrBuf[4], 4);
			}
		}
	}
	ret = mpegGetAuInternal(mp_ptr, stream, streamType, au);
	mpegUnlockLwMutex(ctx->dataMutex);
	if ((stream[12] & 4u) != 0 && ret == 0) {
		stream[12] &= ~4u;
		return mpegGetAvcAuInternal(mp_ptr, stream, streamType, au, headerOut);
	}
	return ret;
}

/* 57: 0x40a0 (764B) */
static s32 mpegGetUserdataAuInternal(u32 *mp_ptr, u32 *stream, u32 *au, void **esOut)
{
	SceMpegCtx *ctx = (SceMpegCtx *)(*mp_ptr);
	s32 ret = mpegValidateHandle(mp_ptr);
	if (ret != 0)
		return ret;
	mpegLockLwMutex(ctx->dataMutex);
	u32 *slot = mpegFindStreamSlot(stream[0], *mp_ptr, 1);
	if (slot == (u32 *)-1) {
		mpegUnlockLwMutex(ctx->dataMutex);
		return 0x806101fe;
	}
	u32 pesInfo[12];
	s32 got = 0;
	mpegPeekStreamPackets(slot, 0, 1, pesInfo, &got);
	if (got != 1) {
		mpegUnlockLwMutex(ctx->dataMutex);
		return 0x80618001;
	}
	if (stream[11] == 0 || stream[14] != 0) {
		if (stream[11] == 0)
			au[5] = 0;
		while (1) {
			mpegPeekStreamPackets(slot, 0, 1, pesInfo, &got);
			if (got != 1) {
				mpegUnlockLwMutex(ctx->dataMutex);
				return 0x80618001;
			}
			const u8 *payload = (const u8 *)pesInfo[7];
			if ((stream[6] & 1u) == 0) {
				au[0] = (u32)-1;
				au[1] = (u32)-1;
				au[2] = (u32)-1;
				au[3] = (u32)-1;
				break;
			}
			s16 hdrOff = (s16)mpegReadBe16(payload + 2);
			*(s16 *)&stream[7] = hdrOff;
			if (hdrOff != -1) {
				au[0] = pesInfo[3];
				au[1] = pesInfo[4];
				au[2] = pesInfo[5];
				au[3] = pesInfo[6];
				stream[6] = 0;
				break;
			}
			mpegPopStreamPackets(slot, 1, &got);
		}
		s32 needHdr = (s32)stream[14];
		s32 pktOff = (s32)(*(s16 *)&stream[7]) + 4;
		s32 pktRem = (s32)pesInfo[8] - pktOff;
		if (needHdr == 0) {
			stream[14] = 4;
			needHdr = 4;
		}
		u8 *dstHdr = (u8 *)&stream[15] + (4 - needHdr);
		s32 skipIdx = 1;
		while (pktRem < needHdr) {
			sceKernelMemcpy(dstHdr, (const void *)(pesInfo[7] + pktOff), (u32)pktRem);
			dstHdr += pktRem;
			stream[14] -= (u32)pktRem;
			mpegPeekStreamPackets(slot, skipIdx, 1, pesInfo, &got);
			pktOff = 4;
			skipIdx++;
			if (got != 1) {
				stream[9] = 4;
				stream[13] = 1;
				break;
			}
			needHdr = (s32)stream[14];
			pktRem = (s32)pesInfo[8] - 4;
		}
		if (pktRem >= needHdr) {
			sceKernelMemcpy(dstHdr, (const void *)(pesInfo[7] + pktOff), (u32)needHdr);
			stream[14] = 0;
			stream[9] = psmfGetStreamSize((const u8 *)&stream[15]) - stream[11] + 8;
		}
	}
	ret = mpegGetAuInternal(mp_ptr, stream, 1, au);
	mpegUnlockLwMutex(ctx->dataMutex);
	if (ret == 0 && esOut != 0)
		*esOut = (void *)(au[4] + 8);
	if ((stream[12] & 4u) != 0 && ret == 0) {
		stream[12] &= ~4u;
		return mpegGetUserdataAuInternal(mp_ptr, stream, au, esOut);
	}
	return ret;
}

/* 58: 0x439c (260B) */
static s32 mpegRingbufferSetReadPos(u32 *rb, u32 addrA, u32 addrB)
{
	u32 pktSize = rb[4];
	u32 rbStart = rb[5];
	u32 rbEnd = rbStart + pktSize * rb[0];
	u32 writePtr = rbStart + pktSize * rb[2];
	u32 readPtr = rbStart + pktSize * rb[1];
	if (addrA < rbStart || addrA > rbEnd)
		return 0x80618007;
	if (addrB < rbStart || addrB > rbEnd)
		return 0x80618007;
	if (writePtr < readPtr) {
		if (addrA < addrB)
			return 1;
		return (addrA == addrB) ? 2 : 3;
	}
	if (addrA <= readPtr && addrB <= readPtr) {
		if (addrA < addrB)
			return 1;
		return (addrA == addrB) ? 2 : 3;
	}
	if (addrA >= writePtr && addrB >= writePtr) {
		if (addrA < addrB)
			return 1;
		return (addrA == addrB) ? 2 : 3;
	}
	return (addrA >= addrB) ? 1 : 3;
}

/* 59: 0x44a0 (872B) */
static s32 mpegGetPcmAuInternal(u32 *mp_ptr, u32 *stream, u32 *au, u32 *attrOut)
{
	SceMpegCtx *ctx = (SceMpegCtx *)(*mp_ptr);
	s32 ret = mpegValidateHandle(mp_ptr);
	if (ret != 0)
		return ret;
	if (stream[2] == (u32)-1)
		return 0x806101fe;
	mpegLockLwMutex(ctx->dataMutex);
	u32 *slot = mpegFindStreamSlot(stream[0], *mp_ptr, 1);
	if (slot == (u32 *)-1) {
		mpegUnlockLwMutex(ctx->dataMutex);
		return 0x80618001;
	}
	u32 pesInfo[12];
	u32 privPes[12];
	s32 got = 0;
	u8 *epBuf = (u8 *)stream[3];
	while (1) {
		mpegPeekStreamPackets(slot, 0, 1, pesInfo, &got);
		if (got != 1) {
			mpegUnlockLwMutex(ctx->dataMutex);
			return 0x80618001;
		}
		if (stream[11] != 0)
			break;
		au[5] = 0;
		if (stream[4] != 0x7fff) {
			au[0] = (u32)-1;
			au[1] = (u32)-1;
			au[2] = (u32)-1;
			au[3] = (u32)-1;
			const u8 *payload = psmfGetPayloadPtr(epBuf);
			const u8 *entry = psmfGetStreamEntry(payload, (s32)stream[4]);
			stream[9] = psmfGetStreamOffset(entry);
			break;
		}
		au[0] = pesInfo[3];
		au[1] = pesInfo[4];
		au[2] = pesInfo[5];
		au[3] = pesInfo[6];
		u32 *privSlot = (u32 *)stream[2];
		mpegPeekStreamPackets(privSlot, 0, 1, privPes, &got);
		if (got != 1) {
			mpegUnlockLwMutex(ctx->dataMutex);
			return 0x80618001;
		}
		if (mpegRingbufferSetReadPos((u32 *)ctx->ringbufPtr, pesInfo[7], privPes[7]) != 1) {
			mpegPopStreamPackets(privSlot, 1, &got);
			if (privPes[8] >= 0x801u) {
				mpegUnlockLwMutex(ctx->dataMutex);
				return 0x80618007;
			}
			sceKernelMemcpy(epBuf, (const void *)privPes[7], privPes[8]);
			stream[4] = 0;
			*(s16 *)&stream[7] = 0;
			const u8 *payload = psmfGetPayloadPtr(epBuf);
			const u8 *entry = psmfGetStreamEntry(payload, (s32)stream[4]);
			stream[9] = psmfGetStreamOffset(entry);
			break;
		}
		mpegUnlockLwMutex(ctx->dataMutex);
		s32 syncRet = mpegChangeGetAvcAuModeInternal(mp_ptr, stream);
		if (syncRet != 0)
			return (syncRet == (s32)0x80618004) ? (s32)0x80618001 : syncRet;
		mpegLockLwMutex(ctx->dataMutex);
	}
	const u8 *payload = psmfGetPayloadPtr(epBuf);
	const u8 *entry = psmfGetStreamEntry(payload, (s32)stream[4]);
	if (psmfGetEpMapFlag(entry) == 0 ||
	    (ctx->psmfVersion == 0 && (s32)stream[4] == (s32)psmfGetStreamCount(psmfGetPayloadPtr(epBuf)) - 1)) {
		stream[12] &= 0x7fffffffu;
	} else {
		stream[12] |= 0x80000000u;
	}
	if (attrOut != 0) {
		*attrOut = stream[12] >> 31;
		*attrOut |= psmfGetStreamType(entry) * 2;
	}
	ret = mpegGetAuInternal(mp_ptr, stream, 0, au);
	if (ret == 0) {
		s32 entryCount = (s32)psmfGetStreamCount(psmfGetPayloadPtr(epBuf));
		s32 nextIdx = (s32)stream[4] + 1;
		if ((s32)stream[4] >= entryCount - 1)
			nextIdx = 0x7fff;
		stream[4] = (u32)nextIdx;
	}
	mpegUnlockLwMutex(ctx->dataMutex);
	if ((stream[12] & 4u) != 0 && ret == 0) {
		stream[12] &= ~4u;
		return mpegGetPcmAuInternal(mp_ptr, stream, au, attrOut);
	}
	return ret;
}

/* 60: 0x4808 (224B) */
static s32 mpegAvcCopyEsData(u32 *mp_ptr, const void *src, s32 len, u32 *au)
{
	s32 ret = mpegValidateHandle(mp_ptr);
	if (ret != 0)
		return ret;
	au[5] = 0;
	u32 *tblHdr = (u32 *)mpegGetActiveSlotPtr(mp_ptr);
	u32 copyCtx[7];
	copyCtx[4] = 0;
	copyCtx[5] = *tblHdr;
	copyCtx[6] = (u32)tblHdr;
	ret = mpegEsAppendChunk(mp_ptr, copyCtx, src, len, 0, au);
	if (ret == 0) {
		ret = mpegEsFinalizeChain(mp_ptr, copyCtx, au);
		if (ret == 0) {
			copyCtx[4] = 0;
			copyCtx[5] = *(u32 *)copyCtx[6];
		}
	}
	mpegNoopHandle(mp_ptr);
	return ret;
}

/* 61: 0x48e8 (668B) */
static s32 mpegAvcAssembleNal(u32 *mp_ptr, u32 *nal, u32 *au)
{
	s32 ret = mpegValidateHandle(mp_ptr);
	if (ret != 0)
		return ret;
	au[5] = 0;
	u32 lenSize = nal[4];
	if (lenSize == 0 || (lenSize >= 3u && lenSize != 4u))
		return 0x806101fe;
	u32 *tblHdr = (u32 *)mpegGetActiveSlotPtr(mp_ptr);
	u32 copyCtx[7];
	copyCtx[4] = 0;
	copyCtx[5] = *tblHdr;
	copyCtx[6] = (u32)tblHdr;
	s32 hasPrefix = 0;
	if ((nal[7] & 1u) != 0 && nal[0] != 0) {
		ret = mpegEsAppendChunk(mp_ptr, copyCtx, g_nal_prefix, 4, 0, au);
		if (ret != 0) {
			mpegNoopHandle(mp_ptr);
			return ret;
		}
		ret = mpegEsAppendChunk(mp_ptr, copyCtx, (const void *)nal[0], (s32)nal[1], 0, au);
		if (ret != 0) {
			mpegNoopHandle(mp_ptr);
			return ret;
		}
		hasPrefix = 1;
	}
	if ((nal[7] & 2u) != 0 && nal[2] != 0) {
		ret = mpegEsAppendChunk(mp_ptr, copyCtx, g_nal_prefix, 4, 0, au);
		if (ret != 0) {
			mpegNoopHandle(mp_ptr);
			return ret;
		}
		ret = mpegEsAppendChunk(mp_ptr, copyCtx, (const void *)nal[2], (s32)nal[3], 0, au);
		if (ret != 0) {
			mpegNoopHandle(mp_ptr);
			return ret;
		}
		hasPrefix = 1;
	}
	s32 remLen = (s32)nal[6];
	const u8 *nalPtr = (const u8 *)nal[5];
	while (remLen > 0) {
		s32 unitLen = (s32)mpegReadBits64(nalPtr, (s32)nal[4]);
		s32 hdrLen = (s32)nal[4];
		remLen -= hdrLen + unitLen;
		const u8 *unitData = nalPtr + hdrLen;
		if (unitLen < 1 || remLen < 0) {
			mpegNoopHandle(mp_ptr);
			return 0x80618007;
		}
		s32 skipZero = 0;
		if (hasPrefix != 0)
			skipZero = (((u32)(unitData[0] & 0x1fu) - 7u) >= 2u) ? 1 : 0;
		ret = mpegEsAppendChunk(mp_ptr, copyCtx, g_nal_prefix + skipZero, 4 - skipZero, 0, au);
		hasPrefix = 1;
		nalPtr = unitData + unitLen;
		if (ret != 0) {
			mpegNoopHandle(mp_ptr);
			return ret;
		}
		ret = mpegEsAppendChunk(mp_ptr, copyCtx, unitData, unitLen, 0, au);
		if (ret != 0) {
			mpegNoopHandle(mp_ptr);
			return ret;
		}
	}
	ret = mpegEsFinalizeChain(mp_ptr, copyCtx, au);
	if (ret == 0) {
		copyCtx[4] = 0;
		copyCtx[5] = *(u32 *)copyCtx[6];
	}
	mpegNoopHandle(mp_ptr);
	return ret;
}

/* 62: 0x4b84 (52B) */
static s32 mpegEsNodeInit(u32 *desc, u32 srcAddr, u32 dstAddr, u32 nextDesc, u32 copySize)
{
	desc[0] = srcAddr;
	desc[1] = dstAddr;
	desc[2] = nextDesc;
	if (copySize >= 0x1000u) {
		desc[3] = 0;
		return 0x806101fe;
	}
	desc[3] = copySize;
	return 0;
}

/* 63: 0x4bb8 (12B) */
static s32 mpegEsNodeSetNext(u32 *desc, u32 nextDesc)
{
	desc[2] = nextDesc;
	return 0;
}

/* 64: 0x4bc4 (36B) */
static s32 mpegGetAvcNalAuInternal(u32 *mp_ptr, u32 *stream, u32 *au, void *headerOut)
{
	return mpegGetAvcAuInternal(mp_ptr, stream, 3, au, headerOut);
}

/* 65: 0x4be8 (88B) */
static s32 mpegEsNodeFlushCache(u32 *desc)
{
	s32 count = 0;
	u32 *cur = desc;
	while (1) {
		count++;
		sceKernelDcacheWritebackInvalidateRange((const void *)cur[0], cur[3] & 0xfffu);
		cur = (u32 *)cur[2];
		if (cur == 0 || count >= 100)
			break;
	}
	return (count < 100) ? 1 : 0;
}

/* 66: 0x4c40 (180B) */
static s32 mpegEsFinalizeChain(u32 *mp_ptr, u32 *copyCtx, u32 *au)
{
	u32 *descs = (u32 *)(copyCtx[6] + 4);
	if (copyCtx[4] == 0)
		return 0;
	u32 esBase = mpegGetAvcEsBufPtr(mp_ptr, (s32)au[4]);
	descs[4 * copyCtx[4] - 2] = 0;
	mpegEsNodeFlushCache(descs);
	u32 totalSize = au[5];
	if ((esBase & 0x1fffffffu) > 0x3fffffu)
		sceKernelDcacheWritebackInvalidateRange((const void *)esBase, totalSize);
	return sceMpegBasePESpacketCopy(descs);
}

/* 67: 0x4cf4 (344B) */
static s32 mpegEsAppendChunk(u32 *mp_ptr, u32 *copyCtx, const void *src, s32 len, u32 streamType, u32 *au)
{
	if (streamType >= 7u)
		return 0x80618007;
	if (g_au_max_sizes[streamType] < au[5] + (u32)len)
		return 0x80618007;
	u32 esBase = mpegGetAvcEsBufPtr(mp_ptr, (s32)au[4]);
	copyCtx[0] = (u32)src;
	copyCtx[1] = esBase + au[5];
	copyCtx[2] = (u32)len;
	copyCtx[3] = 0;
	s32 ret;
	do {
		ret = mpegEsResetTable(copyCtx);
		if (ret != 0)
			break;
		u32 copied = copyCtx[3];
		copyCtx[2] -= copied;
		copyCtx[0] += copied;
		au[5] += copied;
		copyCtx[1] += copied;
		if ((s32)copyCtx[4] < (s32)copyCtx[5] || (s32)copyCtx[3] >= (s32)copyCtx[2])
			break;
		ret = mpegEsFinalizeChain(mp_ptr, copyCtx, au);
		if (ret == 0) {
			copyCtx[4] = 0;
			copyCtx[5] = *(u32 *)copyCtx[6];
		}
	} while (ret == 0);
	return ret;
}

/* 68: 0x4e4c (204B) */
static s32 mpegEsResetTable(u32 *copyCtx)
{
	s32 idx = (s32)copyCtx[4];
	s32 maxDesc = (s32)copyCtx[5];
	u32 descBase = copyCtx[6] + 4;
	copyCtx[3] = 0;
	u32 srcAddr = copyCtx[0];
	u32 dstAddr = copyCtx[1];
	if (idx >= maxDesc || (s32)copyCtx[2] <= 0)
		return 0;
	while (1) {
		s32 rem = (s32)copyCtx[2] - (s32)copyCtx[3];
		u32 chunk = (rem < 4080) ? (u32)rem : 4080u;
		SceMpegEsDesc *desc = (SceMpegEsDesc *)(descBase + (u32)(idx * 16));
		desc->nextDesc = descBase + (u32)(idx * 16 + 16);
		desc->srcAddr = srcAddr;
		srcAddr += 4080u;
		desc->dstAddr = dstAddr;
		if (chunk >= 0x1000u) {
			desc->copySize = 0;
			return 0x806101fe;
		}
		desc->copySize = chunk;
		dstAddr += 4080u;
		idx += 1;
		copyCtx[3] += chunk;
		copyCtx[4] = (u32)idx;
		if (idx >= (s32)copyCtx[5] || (s32)copyCtx[3] >= (s32)copyCtx[2])
			return 0;
	}
}

/* 69: 0x4f18 (156B) */
static s32 mpegMallocAvcEsBufInternal(u32 *mp_ptr)
{
	SceMpegCtx *ctx = (SceMpegCtx *)(*mp_ptr);
	if (mpegValidateHandle(mp_ptr) != 0)
		return 0;
	mpegLockLwMutex(ctx->dataMutex);
	s32 found = 0;
	for (s32 i = 0; i < ctx->avcEsBufCount; i++) {
		if (ctx->avcEsBufInUse[i] == 0) {
			ctx->avcEsBufInUse[i] = 1;
			found = i + 1;
			break;
		}
	}
	mpegUnlockLwMutex(ctx->dataMutex);
	return found;
}

/* 70: 0x4fb4 (140B) */
static s32 mpegFreeAvcEsBufInternal(u32 *mp_ptr, s32 bufId)
{
	SceMpegCtx *ctx = (SceMpegCtx *)(*mp_ptr);
	s32 ret = mpegValidateHandle(mp_ptr);
	if (ret != 0)
		return ret;
	mpegLockLwMutex(ctx->dataMutex);
	if (bufId <= 0 || bufId > ctx->avcEsBufCount) {
		ret = 0x806101fe;
	} else {
		ctx->avcEsBufInUse[bufId - 1] = 0;
	}
	mpegUnlockLwMutex(ctx->dataMutex);
	return ret;
}

/* 71: 0x5040 (60B) */
static u32 mpegGetAvcEsBufPtr(u32 *mp_ptr, s32 bufId)
{
	SceMpegCtx *ctx = (SceMpegCtx *)(*mp_ptr);
	if (bufId > 0 && bufId <= ctx->avcEsBufCount && ctx->avcEsBufInUse[bufId - 1] == 1)
		return ctx->avcEsBufAddr[bufId - 1];
	return (u32)bufId;
}

/* 72: 0x507c (80B) */
static s32 mpegQueryAtracEsSizeInternal(u32 *mp_ptr, u32 *esSize, u32 *outSize)
{
	s32 ret = mpegValidateHandle(mp_ptr);
	if (ret != 0)
		return ret;
	*esSize = 0x840;
	*outSize = 0x2000;
	return 0;
}

/* 73: 0x50cc (76B) */
static s32 mpegQueryPcmEsSizeInternal(u32 *mp_ptr, u32 *esSize, u32 *outSize)
{
	s32 ret = mpegValidateHandle(mp_ptr);
	if (ret != 0)
		return ret;
	*esSize = 0x140;
	*outSize = 0x140;
	return 0;
}

/* 74: 0x5118 (76B) */
static s32 mpegQueryUserdataEsSizeInternal(u32 *mp_ptr, u32 *esSize, u32 *outSize)
{
	s32 ret = mpegValidateHandle(mp_ptr);
	if (ret != 0)
		return ret;
	*esSize = 0xa0000;
	*outSize = 0xa0000;
	return 0;
}

/* 75: 0x5164 (40B) */
static u32 psmfGetStreamSize(const u8 *hdr)
{
	return ((mpegReadBe16(hdr + 2) & 0x3ffu) << 3) + 8;
}

/* 76: 0x518c (12B) */
static u32 psmfGetStreamType(const u8 *hdr)
{
	return hdr[0] & 0xfu;
}

/* 77: 0x5198 (32B) */
static u32 psmfGetEpMapFlag(const u8 *hdr)
{
	return mpegReadBe24(hdr + 1) >> 23;
}

/* 78: 0x51b8 (32B) */
static u32 psmfGetStreamOffset(const u8 *hdr)
{
	return mpegReadBe24(hdr + 1) & 0x1fffffu;
}

/* 79: 0x51d8 (8B) */
static u32 psmfGetStreamCount(const u8 *hdr)
{
	return hdr[3];
}

/* 80: 0x51e0 (36B) */
static const u8 *psmfGetStreamEntry(const u8 *hdr, s32 idx)
{
	if (idx < (s32)hdr[3])
		return hdr + (idx << 2) + 4;
	return 0;
}

/* 81: 0x5204 (8B) */
static const u8 *psmfGetPayloadPtr(const u8 *hdr)
{
	return hdr + 0x0e;
}

/* 82: 0x520c (32B) */
static void *mpegGetActiveSlotPtr(u32 *mp_ptr)
{
	SceMpegCtx *ctx = (SceMpegCtx *)(*mp_ptr);
	return (void *)((u32)ctx->esDescTable + ctx->activeEsTableIdx * 0x644);
}

/* 83: 0x522c (8B) */
static s32 mpegNoopHandle(u32 *mp_ptr)
{
	(void)mp_ptr;
	return 0;
}

/* 84: 0x5234 (400B) */
static s32 mpegChangeGetAvcAuModeInternal(u32 *mp_ptr, u32 *stream)
{
	SceMpegCtx *ctx = (SceMpegCtx *)(*mp_ptr);
	s32 ret = mpegValidateHandle(mp_ptr);
	if (ret != 0)
		return ret;
	if (stream[2] == (u32)-1)
		return 0x806101fe;
	mpegLockLwMutex(ctx->dataMutex);
	u32 *slot = mpegFindStreamSlot(stream[0], *mp_ptr, 1);
	if (slot == (u32 *)-1) {
		mpegUnlockLwMutex(ctx->dataMutex);
		return 0x806101fe;
	}
	u32 pesInfo[12];
	s32 got = 0;
	mpegPeekStreamPackets((u32 *)stream[2], 0, 1, pesInfo, &got);
	u32 privAddr;
	if (got == 1) {
		privAddr = pesInfo[7];
	} else {
		ret = (s32)0x80618004;
		privAddr = 0xffff0000u;
	}
	s32 total = (s32)slot[3];
	for (s32 i = 0; i < total; i++) {
		mpegPeekStreamPackets(slot, 0, 1, pesInfo, &got);
		if (privAddr != 0xffff0000u &&
		    mpegRingbufferSetReadPos((u32 *)ctx->ringbufPtr, privAddr, pesInfo[7]) == 1)
			break;
		mpegPopStreamPackets(slot, 1, &got);
	}
	stream[4] = 0x7fff;
	stream[12] &= ~4u;
	stream[11] = 0;
	mpegUnlockLwMutex(ctx->dataMutex);
	return ret;
}

/* 85: 0x53c4 (224B) */
static s32 mpegChangeGetAuModeInternal(u32 *mp_ptr, u32 *stream, s32 mode)
{
	SceMpegCtx *ctx = (SceMpegCtx *)(*mp_ptr);
	s32 ret = mpegValidateHandle(mp_ptr);
	if (ret != 0)
		return ret;
	mpegLockLwMutex(ctx->dataMutex);
	if (mpegFindStreamSlot(stream[0], *mp_ptr, 1) == (u32 *)-1 || stream[2] == (u32)-1) {
		mpegUnlockLwMutex(ctx->dataMutex);
		return 0x806101fe;
	}
	if (mode == 0) {
		stream[12] &= ~2u;
	} else if (mode == 2) {
		stream[12] |= 2u;
	} else {
		ret = 0x806101fe;
	}
	mpegUnlockLwMutex(ctx->dataMutex);
	return ret;
}

/* 86: 0x54a4 (212B) */
static s32 mpegSetStreamModeInternal(u32 *mp_ptr, u32 *stream, s32 mode)
{
	SceMpegCtx *ctx = (SceMpegCtx *)(*mp_ptr);
	s32 ret = mpegValidateHandle(mp_ptr);
	if (ret != 0)
		return ret;
	mpegLockLwMutex(ctx->dataMutex);
	if (mpegFindStreamSlot(stream[0], *mp_ptr, 1) == (u32 *)-1) {
		mpegUnlockLwMutex(ctx->dataMutex);
		return 0x806101fe;
	}
	if (mode == 0) {
		stream[12] &= ~1u;
	} else if (mode == 1) {
		stream[12] |= 1u;
	} else {
		ret = 0x806101fe;
	}
	mpegUnlockLwMutex(ctx->dataMutex);
	return ret;
}

/* 87: 0x5578 (104B) - sceMpegQueryStreamOffset (0x21ff80e4) */
s32 sceMpegQueryStreamOffset(u32 *mp_ptr, const u8 *header, u32 *outOffset)
{
	if (mpegCheckThreadStack() < 0x3f0)
		return 0x80618008;
	return mpegQueryStreamOffsetInternal(mp_ptr, header, outOffset);
}

/* 88: 0x55e0 (88B) - sceMpegQueryStreamSize (0x611e9e11) */
s32 sceMpegQueryStreamSize(const u8 *header, u32 *outSize)
{
	if (mpegCheckThreadStack() < 0x3f0)
		return 0x80618008;
	return mpegQueryStreamSizeInternal(header, outSize);
}

/* 89: 0x5638 (60B) - sceMpegInit (0x682a619b) */
s32 sceMpegInit(void)
{
	if (mpegCheckThreadStack() < 0x3f0)
		return 0x80618008;
	return mpegInitInternal();
}

/* 90: 0x5674 (60B) - sceMpegFinish (0x874624d6) */
s32 sceMpegFinish(void)
{
	if (mpegCheckThreadStack() < 0x3f0)
		return 0x80618008;
	return mpegFinishInternal();
}

/* 91: 0x56b0 (80B) - sceMpegQueryMemSize (0xc132e22f) */
s32 sceMpegQueryMemSize(s32 mode)
{
	s32 stackFree = mpegCheckThreadStack();
	if (mode != 1)
		mode = 0;
	if (stackFree < 0x3f0)
		return 0x80618008;
	return mpegQueryMemSizeInternal(mode);
}

/* 92: 0x5700 (72B) - sceMpeg_42C679F6 (0x42c679f6) */
s32 sceMpeg_42C679F6(s32 mode)
{
	if (mpegCheckThreadStack() < 0x3f0)
		return 0x80618008;
	return mpegQueryMemSizeInternal(mode);
}

/* 93: 0x5748 (176B) - sceMpegCreate (0xd8c5f121) */
s32 sceMpegCreate(u32 *mp_ptr, void *workMem, s32 size, u32 *rb, s32 frameWidth, s32 mode, u32 ddrtop)
{
	s32 stackFree = mpegCheckThreadStack();
	if (mode != 1)
		mode = 0;
	if (stackFree < 0x3e0)
		return 0x80618008;
	return mpegCreateInternal(mp_ptr, workMem, size, rb, frameWidth, mode, ddrtop);
}

/* 94: 0x57f8 (168B) - sceMpeg_75E21135 (0x75e21135) */
s32 sceMpeg_75E21135(u32 *mp_ptr, void *workMem, s32 size, u32 *rb, s32 frameWidth, s32 mode, u32 ddrtop)
{
	if (mpegCheckThreadStack() < 0x3e0)
		return 0x80618008;
	return mpegCreateInternal(mp_ptr, workMem, size, rb, frameWidth, mode, ddrtop);
}

/* 95: 0x58a0 (72B) - sceMpegDelete (0x606a4649) */
s32 sceMpegDelete(u32 *mp_ptr)
{
	if (mpegCheckThreadStack() < 0x3f0)
		return 0x80618008;
	return mpegDeleteInternal(mp_ptr);
}

/* 96: 0x58e8 (100B) - sceMpegRegistStream (0x42560f23) */
u32 *sceMpegRegistStream(u32 *mp_ptr, s32 type, s32 ch)
{
	if (mpegCheckThreadStack() < 0x3f0)
		return 0;
	return mpegRegistStreamInternal(mp_ptr, type, ch);
}

/* 97: 0x594c (88B) - sceMpegUnRegistStream (0x591a4aa2) */
s32 sceMpegUnRegistStream(u32 *mp_ptr, u32 *stream)
{
	if (mpegCheckThreadStack() < 0x3f0)
		return 0x80618008;
	return mpegUnRegistStreamInternal(mp_ptr, stream);
}

/* 98: 0x59a4 (68B) - sceMpegMallocAvcEsBuf (0xa780cf7e) */
s32 sceMpegMallocAvcEsBuf(u32 *mp_ptr)
{
	if (mpegCheckThreadStack() < 0x3f0)
		return 0;
	return mpegMallocAvcEsBufInternal(mp_ptr);
}

/* 99: 0x59e8 (88B) - sceMpegFreeAvcEsBuf (0xceb870b1) */
s32 sceMpegFreeAvcEsBuf(u32 *mp_ptr, s32 bufId)
{
	if (mpegCheckThreadStack() < 0x3f0)
		return 0x80618008;
	return mpegFreeAvcEsBufInternal(mp_ptr, bufId);
}

/* 100: 0x5a40 (104B) - sceMpegQueryAtracEsSize (0xf8dcb679) */
s32 sceMpegQueryAtracEsSize(u32 *mp_ptr, u32 *esSize, u32 *outSize)
{
	if (mpegCheckThreadStack() < 0x3f0)
		return 0x80618008;
	return mpegQueryAtracEsSizeInternal(mp_ptr, esSize, outSize);
}

/* 101: 0x5aa8 (104B) - sceMpegQueryPcmEsSize (0xc02cf6b5) */
s32 sceMpegQueryPcmEsSize(u32 *mp_ptr, u32 *esSize, u32 *outSize)
{
	if (mpegCheckThreadStack() < 0x3f0)
		return 0x80618008;
	return mpegQueryPcmEsSizeInternal(mp_ptr, esSize, outSize);
}

/* 102: 0x5b10 (104B) - sceMpegInitAu (0x167afd9e) */
s32 sceMpegInitAu(u32 *mp_ptr, u32 esBuf, u32 *au)
{
	if (mpegCheckThreadStack() < 0x3f0)
		return 0x80618008;
	return mpegInitAuInternal(mp_ptr, esBuf, au);
}

/* 103: 0x5b78 (104B) - sceMpegChangeGetAvcAuMode (0x234586ae) */
s32 sceMpegChangeGetAvcAuMode(u32 *mp_ptr, u32 *stream, s32 mode)
{
	if (mpegCheckThreadStack() < 0x3f0)
		return 0x80618008;
	return mpegChangeGetAuModeInternal(mp_ptr, stream, mode);
}

/* 104: 0x5be0 (104B) - sceMpegChangeGetAuMode (0x9dcfb7ea) */
s32 sceMpegChangeGetAuMode(u32 *mp_ptr, u32 *stream, s32 mode)
{
	if (mpegCheckThreadStack() < 0x3f0)
		return 0x80618008;
	return mpegSetStreamModeInternal(mp_ptr, stream, mode);
}

/* 105: 0x5c48 (88B) - sceMpegNextAvcRpAu (0x3c37a7a6) */
s32 sceMpegNextAvcRpAu(u32 *mp_ptr, u32 *stream)
{
	if (mpegCheckThreadStack() < 0x3f0)
		return 0x80618008;
	return mpegChangeGetAvcAuModeInternal(mp_ptr, stream);
}

/* 106: 0x5ca0 (120B) - sceMpegGetAvcAu (0xfe246728) */
s32 sceMpegGetAvcAu(u32 *mp_ptr, u32 *stream, u32 *au, u32 *attrOut)
{
	if (mpegCheckThreadStack() < 0x3e0)
		return 0x80618008;
	return mpegGetPcmAuInternal(mp_ptr, stream, au, attrOut);
}

/* 107: 0x5d18 (120B) - sceMpegGetPcmAu (0x8c1e027d) */
s32 sceMpegGetPcmAu(u32 *mp_ptr, u32 *stream, u32 *au, u32 *attrOut)
{
	if (mpegCheckThreadStack() < 0x3e0)
		return 0x80618008;
	return mpegGetAtracAuInternal(mp_ptr, stream, au, attrOut);
}

/* 108: 0x5d90 (120B) - sceMpegGetAtracAu (0xe1ce83a7) */
s32 sceMpegGetAtracAu(u32 *mp_ptr, u32 *stream, u32 *au, void **esOut)
{
	if (mpegCheckThreadStack() < 0x3e0)
		return 0x80618008;
	return mpegGetUserdataAuInternal(mp_ptr, stream, au, esOut);
}

/* 109: 0x5e08 (88B) - sceMpegFlushStream (0x500f0429) */
s32 sceMpegFlushStream(u32 *mp_ptr, u32 *stream)
{
	if (mpegCheckThreadStack() < 0x3f0)
		return 0x80618008;
	return mpegFlushStreamInternal(mp_ptr, stream);
}

/* 110: 0x5e60 (72B) - sceMpegFlushAllStream (0x707b7629) */
s32 sceMpegFlushAllStream(u32 *mp_ptr)
{
	if (mpegCheckThreadStack() < 0x3f0)
		return 0x80618008;
	return mpegFlushAllStreamInternal(mp_ptr);
}

/* 111: 0x5ea8 (136B) - sceMpegAvcDecode (0x0e3c2e9d) */
s32 sceMpegAvcDecode(u32 *mp_ptr, u32 *au, s32 frameWidth, u32 *bufList, u32 *numPic)
{
	if (mpegCheckThreadStack() < 0x3e0)
		return 0x80618008;
	return mpegAvcDecodeInternal(mp_ptr, au, frameWidth, bufList, numPic);
}

/* 112: 0x5f30 (88B) - sceMpegAvcDecodeDetail (0x0f6c18d7) */
s32 sceMpegAvcDecodeDetail(u32 *mp_ptr, u32 *outDetail)
{
	if (mpegCheckThreadStack() < 0x3f0)
		return 0x80618008;
	return mpegAvcQueryYCbCrSizeDefault(mp_ptr, outDetail);
}

/* 113: 0x5f88 (104B) - sceMpegAvcDecodeDetailIndex (0xab0e9556) */
s32 sceMpegAvcDecodeDetailIndex(u32 *mp_ptr, s32 frameIdx, u32 *outDetail)
{
	if (mpegCheckThreadStack() < 0x3f0)
		return 0x80618008;
	return mpegAvcQueryYCbCrSizeInternal(mp_ptr, frameIdx, outDetail);
}

/* 114: 0x5ff0 (88B) - sceMpegAvcDecodeMode (0xa11c7026) */
s32 sceMpegAvcDecodeMode(u32 *mp_ptr, const s32 *mode)
{
	if (mpegCheckThreadStack() < 0x3f0)
		return 0x80618008;
	return mpegAvcDecodeModeInternal(mp_ptr, mode);
}

/* 115: 0x6048 (120B) - sceMpegAvcDecodeStop (0x740fccd1) */
s32 sceMpegAvcDecodeStop(u32 *mp_ptr, s32 frameWidth, u32 *bufList, u32 *numPic)
{
	if (mpegCheckThreadStack() < 0x3e0)
		return 0x80618008;
	return mpegAvcDecodeStopInternal(mp_ptr, frameWidth, bufList, numPic);
}

/* 116: 0x60c0 (72B) - sceMpegAvcDecodeFlush (0x4571cc64) */
s32 sceMpegAvcDecodeFlush(u32 *mp_ptr)
{
	if (mpegCheckThreadStack() < 0x3f0)
		return 0x80618008;
	return mpegAvcFlushInternal(mp_ptr);
}

/* 117: 0x6108 (136B) - sceMpegAvcQueryYCbCrSize (0x211a057c) */
s32 sceMpegAvcQueryYCbCrSize(u32 *mp_ptr, s32 mode, s32 width, s32 height, s32 *outSize)
{
	if (mpegCheckThreadStack() < 0x3e0)
		return 0x80618008;
	return mpegAvcQueryYCbCrMemSize(mp_ptr, mode, width, height, outSize);
}

/* 118: 0x6190 (136B) - sceMpegAvcInitYCbCr (0x67179b1b) */
s32 sceMpegAvcInitYCbCr(u32 *mp_ptr, s32 mode, s32 width, s32 height, u32 *ycbcrBuf)
{
	if (mpegCheckThreadStack() < 0x3e0)
		return 0x80618008;
	return mpegAvcInitYCbCrWithDims(mp_ptr, mode, width, height, ycbcrBuf);
}

/* 119: 0x6218 (120B) - sceMpegAvcDecodeYCbCr (0xf0eb1125) */
s32 sceMpegAvcDecodeYCbCr(u32 *mp_ptr, u32 *au, u32 *bufList, u32 *numPic)
{
	if (mpegCheckThreadStack() < 0x3e0)
		return 0x80618008;
	return mpegAvcDecodeYCbCrInternal(mp_ptr, au, bufList, numPic);
}

/* 120: 0x6290 (104B) - sceMpegAvcDecodeStopYCbCr (0xf2930c9c) */
s32 sceMpegAvcDecodeStopYCbCr(u32 *mp_ptr, u32 *bufList, u32 *numPic)
{
	if (mpegCheckThreadStack() < 0x3f0)
		return 0x80618008;
	return mpegAvcDecodeStopYCbCrInternal(mp_ptr, bufList, numPic);
}

/* 121: 0x62f8 (136B) - sceMpegAvcCsc (0x31bd0272) */
s32 sceMpegAvcCsc(u32 *mp_ptr, u32 *srcYcbcr, const u32 *range, s32 frameWidth, void *dstRgb)
{
	if (mpegCheckThreadStack() < 0x3e0)
		return 0x80618008;
	return mpegAvcCscRangeInternal(mp_ptr, srcYcbcr, range, frameWidth, dstRgb);
}

/* 122: 0x6380 (104B) - sceMpegAvcCopyYCbCr (0x0558b075) */
s32 sceMpegAvcCopyYCbCr(u32 *mp_ptr, u32 *dstYcbcr, u32 *srcYcbcr)
{
	if (mpegCheckThreadStack() < 0x3f0)
		return 0x80618008;
	return mpegAvcCopyYCbCrInternal(mp_ptr, dstYcbcr, srcYcbcr);
}

/* 123: 0x63e8 (120B) - sceMpegAvcConvertToYuv420 (0xf5e7ea31) */
s32 sceMpegAvcConvertToYuv420(u32 *mp_ptr, void *dstYuv, u32 *srcYcbcr, s32 flags)
{
	if (mpegCheckThreadStack() < 0x3e0)
		return 0x80618008;
	return mpegAvcCscInternal(mp_ptr, dstYuv, srcYcbcr, flags);
}

/* 124: 0x6460 (144B) - sceMpegAtracDecode (0x800c44df) */
s32 sceMpegAtracDecode(u32 *mp_ptr, u32 *au, void *pcmBuf, s32 initFlag)
{
	if (mpegCheckThreadStack() < 0x3e0)
		return 0x80618008;
	if (((SceMpegCtx *)(*mp_ptr))->avcMode == 1)
		return 0x806101fe;
	return mpegAtracDecodeInternal(mp_ptr, au, pcmBuf, initFlag);
}

/* 125: 0x64f0 (56B) - sceMpegAvcResourceGetAvcEsBuf (0xaf26bb01) */
u32 sceMpegAvcResourceGetAvcEsBuf(void)
{
	if (mpegCheckThreadStack() >= 0x3f0)
		return mpegAvcResAcquire();
	return 0;
}

/* 126: 0x6528 (56B) - sceMpegAvcResourceGetAvcDecTopAddr (0x63b9536a) */
u32 sceMpegAvcResourceGetAvcDecTopAddr(void)
{
	if (mpegCheckThreadStack() >= 0x3f0)
		return mpegAvcResRelease();
	return 0;
}

/* 127: 0x6560 (84B) - sceMpegAvcResourceInit (0xfcbdb5ad) */
s32 sceMpegAvcResourceInit(u32 flags)
{
	if (mpegCheckThreadStack() < 0x3f0)
		return 0x80618008;
	if (flags == 1)
		return mpegAvcResInit();
	return 0x806101fe;
}

/* 128: 0x65b4 (60B) - sceMpegAvcResourceFinish (0x8160a2fe) */
s32 sceMpegAvcResourceFinish(void)
{
	if (mpegCheckThreadStack() < 0x3f0)
		return 0x80618008;
	return mpegAvcResTerm();
}

/* 129: 0x65f0 (104B) - sceMpegQueryUserdataEsSize (0xc45c99cc) */
s32 sceMpegQueryUserdataEsSize(u32 *mp_ptr, u32 *esSize, u32 *outSize)
{
	if (mpegCheckThreadStack() < 0x3f0)
		return 0x80618008;
	return mpegQueryUserdataEsSizeInternal(mp_ptr, esSize, outSize);
}

/* 130: 0x6658 (120B) - sceMpegGetUserdataAu (0x01977054) */
s32 sceMpegGetUserdataAu(u32 *mp_ptr, u32 *stream, u32 *au, void *headerOut)
{
	if (mpegCheckThreadStack() < 0x3e0)
		return 0x80618008;
	return mpegGetAvcNalAuInternal(mp_ptr, stream, au, headerOut);
}

/* 131: 0x66d0 (120B) - sceMpegGetAvcEsAu (0x921fcccf) */
s32 sceMpegGetAvcEsAu(u32 *mp_ptr, const void *src, s32 len, u32 *au)
{
	if (mpegCheckThreadStack() < 0x3e0)
		return 0x80618008;
	return mpegAvcCopyEsData(mp_ptr, src, len, au);
}

/* 132: 0x6748 (104B) - sceMpegGetAvcNalAu (0x11f95cf1) */
s32 sceMpegGetAvcNalAu(u32 *mp_ptr, u32 *nal, u32 *au)
{
	if (mpegCheckThreadStack() < 0x3f0)
		return 0x80618008;
	return mpegAvcAssembleNal(mp_ptr, nal, au);
}

/* 133: 0x67b0 (72B) - sceMpegRingbufferQueryMemSize (0xd7a29f46) */
s32 sceMpegRingbufferQueryMemSize(s32 packets)
{
	if (mpegCheckThreadStack() < 0x3f0)
		return 0x80618008;
	return mpegRingbufferQueryMemSizeInternal(packets);
}

/* 134: 0x67f8 (72B) - sceMpegRingbufferQueryPackNum (0x769bebb6) */
s32 sceMpegRingbufferQueryPackNum(s32 memSize)
{
	if (mpegCheckThreadStack() < 0x3f0)
		return 0x80618008;
	return mpegRingbufferQueryPackNumInternal((u32)memSize);
}

/* 135: 0x6840 (152B) - sceMpegRingbufferConstruct (0x37295ed8) */
s32 sceMpegRingbufferConstruct(u32 *rb, s32 packets, void *rbData, s32 size, void *cb, void *cbArg)
{
	if (mpegCheckThreadStack() < 0x3e0)
		return 0x80618008;
	return mpegRingbufferConstructInternal(rb, packets, rbData, size, cb, cbArg);
}

/* 136: 0x68d8 (72B) - sceMpegRingbufferDestruct (0x13407f13) */
s32 sceMpegRingbufferDestruct(u32 *rb)
{
	if (mpegCheckThreadStack() < 0x3f0)
		return 0x80618008;
	return mpegRingbufferDestructInternal(rb);
}

/* 137: 0x6920 (104B) - sceMpegRingbufferPut (0xb240a59e) */
s32 sceMpegRingbufferPut(u32 *rb, s32 numPackets, s32 available)
{
	if (mpegCheckThreadStack() < 0x3f0)
		return 0x80618008;
	return mpegRingbufferPutInternal(rb, numPackets, available);
}

/* 138: 0x6988 (72B) - sceMpegRingbufferAvailableSize (0xb5f6dc87) */
s32 sceMpegRingbufferAvailableSize(u32 *rb)
{
	if (mpegCheckThreadStack() < 0x3f0)
		return 0x80618008;
	return mpegRingbufferResetInternal(rb);
}

/* 139: 0x69d0 (40B) */
static s32 mpegCreateLwMutex(s32 count, const char *name, void *work)
{
	return sceKernelCreateLwMutex(work, name, 0x300, count, 0);
}

/* 140: 0x69f8 (28B) */
static s32 mpegDeleteLwMutex(void *work)
{
	return sceKernelDeleteLwMutex(work);
}

/* 141: 0x6a14 (32B) */
static s32 mpegLockLwMutex(void *work)
{
	return sceKernelLockLwMutex(work, 1, 0);
}

/* 142: 0x6a34 (28B) */
static s32 mpegUnlockLwMutex(void *work)
{
	return sceKernelUnlockLwMutex(work, 1);
}

/* 143: 0x6a50 (28B) */
static s32 mpegCheckThreadStack(void)
{
	return sceKernelCheckThreadStack();
}

/* 144: 0x6a6c (248B) */
static s32 mpegAvcResInit(void)
{
	u32 *res = mpegGetAvcResPtr();
	u32 opt[2];
	opt[0] = 8;
	opt[1] = 0x20000;
	mpegLockLwMutex(g_libmpeg_mutex);
	s32 ret = (s32)0x80618005;
	if ((mpegGetInitFlags() & 4) == 0) {
		s32 fpl = sceKernelCreateFpl("SceMpegAvcResource", 2, 0, 0x20000, 1, opt);
		ret = fpl;
		if (fpl >= 0) {
			void *block;
			ret = sceKernelAllocateFpl(fpl, &block, 0);
			if (ret < 0) {
				sceKernelDeleteFpl(fpl);
			} else {
				res[0] = (u32)fpl;
				res[1] = (u32)block;
				mpegSetInitFlags(mpegGetInitFlags() | 4u);
			}
		}
	}
	mpegUnlockLwMutex(g_libmpeg_mutex);
	return ret;
}

/* 145: 0x6b64 (100B) */
static u32 mpegAvcResAcquire(void)
{
	u32 *res = mpegGetAvcResPtr();
	mpegLockLwMutex(g_libmpeg_mutex);
	u32 val = 0;
	if ((mpegGetInitFlags() & 4) != 0)
		val = res[1];
	mpegUnlockLwMutex(g_libmpeg_mutex);
	return val;
}

/* 146: 0x6bc8 (104B) */
static u32 mpegAvcResRelease(void)
{
	u32 *res = mpegGetAvcResPtr();
	mpegLockLwMutex(g_libmpeg_mutex);
	u32 val = 0;
	if ((mpegGetInitFlags() & 4) != 0)
		val = res[1] & ~0x3fffffu;
	mpegUnlockLwMutex(g_libmpeg_mutex);
	return val;
}

/* 147: 0x6c30 (148B) */
static s32 mpegAvcResTerm(void)
{
	u32 *res = mpegGetAvcResPtr();
	mpegLockLwMutex(g_libmpeg_mutex);
	s32 ret = (s32)0x80618009;
	if ((mpegGetInitFlags() & 4) != 0) {
		sceKernelFreeFpl((s32)res[0], (void *)res[1]);
		ret = sceKernelDeleteFpl((s32)res[0]);
		res[1] = 0;
		res[0] = 0;
		mpegSetInitFlags(mpegGetInitFlags() & ~4u);
	}
	mpegLockLwMutex(g_libmpeg_mutex);
	return ret;
}

/* 148: 0x6cc4 (168B) */
static s32 mpegVcodecInit(void)
{
	g_vcodec_ptr = g_vcodec_buf;
	sceKernelMemset(g_vcodec_buf, 0, 0x60);
	g_vcodec_sub_ptr = g_vcodec_sub_buf;
	sceKernelMemset(g_vcodec_sub_buf, 0, 0x28);
	g_vcodec_ptr[4] = (u32)g_vcodec_sub_ptr;
	u32 spVal = 0;
	s32 ret = mpegVcodecOpen(&spVal);
	if (ret == 0) {
		ret = sceVideocodecGetEDRAM(g_vcodec_ptr, 0);
		if (ret != 0)
			g_vcodec_ptr[5] = 0;
	}
	return ret;
}

/* 149: 0x6d6c (68B) */
static s32 mpegVcodecOpen(u32 *out)
{
	s32 ret = sceVideocodecOpen(g_vcodec_ptr, 0);
	*out = g_vcodec_ptr[6];
	return ret;
}

/* 150: 0x6db0 (376B) */
static s32 mpegVcodecSetup(u32 *vwork, void *infoBuf, void *frameInfoBuf, void *seiBuf, u32 codecFlags, u32 *edramPtr, u32 ddrTop, u32 width, u32 height, u32 frameBufCount, u32 extraParam)
{
	mpegLockLwMutex(g_avc_mutex);
	sceKernelMemset(infoBuf, 0, 0x28);
	sceKernelMemset(frameInfoBuf, 0, 0xb0);
	sceKernelMemset(seiBuf, 0, 0x64);
	vwork[11] = (u32)frameInfoBuf;
	vwork[4] = (u32)infoBuf;
	vwork[12] = (u32)seiBuf;
	vwork[15] = 4;
	vwork[14] = codecFlags;
	s32 ret = sceVideocodecOpen(vwork, 0);
	if (ret == 0) {
		u32 edram = *edramPtr;
		vwork[7] = ddrTop;
		vwork[5] = edram;
		ret = sceVideocodecInit(vwork, 0);
		if (ret == 0) {
			ret = sceVideocodecGetVersion(vwork, 0);
			if (ret == 0) {
				vwork[16] = width;
				vwork[17] = height;
				vwork[18] = frameBufCount;
				vwork[19] = extraParam;
				ret = sceVideocodecSetMemory(vwork, 0);
				if (ret == 0) {
					mpegUnlockLwMutex(g_avc_mutex);
					return 0;
				}
			}
		}
	}
	mpegUnlockLwMutex(g_avc_mutex);
	return ret;
}

/* 151: 0x6f28 (36B) */
static s32 mpegVcodecReleaseEdram(void)
{
	return sceVideocodecReleaseEDRAM(g_vcodec_ptr);
}

/* 152: 0x6f4c (16B) */
static u32 mpegVcodecGetEdramAddr(void)
{
	return g_vcodec_ptr[5];
}

/* 153: 0x6f5c (84B) */
static s32 mpegVcodecDecode(u32 *vwork)
{
	mpegLockLwMutex(g_avc_mutex);
	s32 ret = sceVideocodecDecode(vwork, 0);
	mpegUnlockLwMutex(g_avc_mutex);
	return ret;
}

/* 154: 0x6fb0 (84B) */
static s32 mpegVcodecStop(u32 *vwork)
{
	mpegLockLwMutex(g_avc_mutex);
	s32 ret = sceVideocodecStop(vwork, 0);
	mpegUnlockLwMutex(g_avc_mutex);
	return ret;
}

/* 155: 0x7004 (108B) */
static s32 mpegVcodecDelete(u32 *vwork)
{
	mpegLockLwMutex(g_avc_mutex);
	s32 ret = sceVideocodecDelete(vwork, 0);
	if (ret == 0) {
		mpegUnlockLwMutex(g_avc_mutex);
		return 0;
	}
	mpegUnlockLwMutex(g_avc_mutex);
	return ret;
}

/* 156: 0x7070 (100B) */
static s32 mpegVcodecSetOption(u32 *vwork, u32 optVal)
{
	mpegLockLwMutex(g_avc_mutex);
	vwork[22] = optVal;
	s32 ret = sceVideocodec_893B32B1(vwork, 0);
	mpegUnlockLwMutex(g_avc_mutex);
	return ret;
}

/* 157: 0x70d4 (140B) */
static s32 mpegAcodecInit(s32 mode)
{
	g_acodec_ptr = g_acodec_buf;
	sceKernelMemset(g_acodec_buf, 0, 0x6c);
	u32 spVal = 0;
	s32 ret = mpegAcodecQueryMem(&spVal, mode, &g_acodec_flag);
	if (ret == 0) {
		ret = sceAudiocodecGetEDRAM(g_acodec_ptr, 0x1000);
		if (ret != 0)
			g_acodec_ptr[3] = 0;
	}
	return ret;
}

/* 158: 0x7160 (188B) */
static s32 mpegAcodecQueryMem(u32 *out, s32 mode, u32 *flagOut)
{
	u32 isModeThree = (mode == 3) ? 1u : 0u;
	if (flagOut != 0)
		*flagOut = isModeThree;
	g_acodec_ptr[12] = 1;
	sceKernelMemcpy(&g_acodec_ptr[10], &g_atrac_hdr[isModeThree << 1], 2);
	sceKernelMemcpy(&g_acodec_ptr[11], &g_atrac_zero[isModeThree], 4);
	g_acodec_ptr[5] = 1;
	s32 ret = sceAudiocodecCheckNeedMem(g_acodec_ptr, 0x1000);
	*out = g_acodec_ptr[4];
	return ret;
}

/* 159: 0x721c (272B) */
static s32 mpegAcodecSetup(u32 *awork, u32 edram, u32 workMem, const u8 *hdrShort, const u8 *hdrWord)
{
	mpegLockLwMutex(g_atrac_mutex);
	awork[3] = edram;
	awork[12] = 1;
	sceKernelMemcpy(&awork[10], hdrShort, 2);
	sceKernelMemcpy(&awork[11], hdrWord, 4);
	awork[5] = 1;
	s32 ret = sceAudiocodecCheckNeedMem(awork, 0x1000);
	if (ret != 0) {
		mpegUnlockLwMutex(g_atrac_mutex);
		return ret;
	}
	if (workMem < awork[4]) {
		mpegUnlockLwMutex(g_atrac_mutex);
		return 0x807f00ff;
	}
	ret = sceAudiocodecInit(awork, 0x1000);
	if (ret == 0) {
		mpegUnlockLwMutex(g_atrac_mutex);
		return 0;
	}
	mpegUnlockLwMutex(g_atrac_mutex);
	return ret;
}

/* 160: 0x732c (56B) */
static s32 mpegAcodecReleaseEdram(void)
{
	if (g_acodec_ptr[3] != 0)
		return sceAudiocodecReleaseEDRAM(g_acodec_ptr);
	return 0;
}

/* 161: 0x7364 (100B) */
static s32 mpegAcodecQueryMemLocked(u32 *out, s32 mode)
{
	mpegLockLwMutex(g_atrac_mutex);
	s32 ret = mpegAcodecQueryMem(out, mode, 0);
	mpegUnlockLwMutex(g_atrac_mutex);
	return ret;
}

/* 162: 0x73c8 (32B) */
static u32 mpegAcodecGetFlag(u32 *flagOut)
{
	if (flagOut != 0)
		*flagOut = g_acodec_flag;
	return g_acodec_ptr[3];
}

/* 163: 0x73e8 (108B) */
static s32 mpegAcodecDecode(u32 *awork)
{
	mpegLockLwMutex(g_atrac_mutex);
	s32 ret = sceAudiocodecDecode(awork, 0x1000);
	if (ret == 0) {
		mpegUnlockLwMutex(g_atrac_mutex);
		return 0;
	}
	mpegUnlockLwMutex(g_atrac_mutex);
	return ret;
}

/* 164: 0x7454 (72B) */
static s32 mpegStrncmp(const char *lhs, const char *rhs, s32 len)
{
	if (len <= 0)
		return 0;
	do {
		u8 chL = (u8)*lhs++;
		u8 chR = (u8)*rhs++;
		len--;
		if (chL != chR)
			return (chL < chR) ? -1 : 1;
	} while (len > 0);
	return 0;
}

/* 165: 0x749c (128B) */
static s32 mpegStrcmp(const char *lhs, const char *rhs)
{
	if (lhs == 0 || rhs == 0) {
		if (lhs == rhs)
			return 0;
		return (lhs != 0) ? 1 : -1;
	}
	s32 chL = (s8)*(const u8 *)lhs;
	s32 chR = (s8)*rhs++;
	if (chL != chR)
		return chL - (s8)rhs[-1];
	while (1) {
		lhs++;
		if (chL == 0)
			return 0;
		chL = (s8)*(const u8 *)lhs;
		chR = (s8)*rhs++;
		if (chL != chR)
			return chL - (s8)rhs[-1];
	}
}

/* 166: 0x751c (16B) */
static s32 mpegAbs(s32 val)
{
	s32 neg = -val;
	return (val > neg) ? val : neg;
}

/* 167: 0x752c (148B) */
static s32 mpegStreamTypeToId(s32 type, s32 ch)
{
	switch (type) {
	case 0:
	case 5:
		return g_stream_id_table[0][0] | (ch << 24);
	case 1:
		return g_stream_id_table[1][0] | (ch << 16);
	case 2:
		return g_stream_id_table[2][0] | (ch << 16);
	case 3:
		return g_stream_id_table[3][0] | (ch << 16);
	case 4:
		return g_stream_id_table[4][0] | (ch << 24);
	case 6:
		return g_stream_id_table[6][0] | (ch << 8);
	default:
		return -1;
	}
}

/* 168: 0x75c0 (588B) */
static s32 mpegStreamIdToType(s32 *typeOut, s32 *chOut, u32 streamId)
{
	s32 found = 0;
	u32 masked = 0;
	u32 chMask = 0;
	for (s32 idx = 0; idx < 19 && found == 0; idx++) {
		u32 tblBase = g_stream_id_table[idx][0];
		u32 tblMask = g_stream_id_table[idx][1];
		if (tblMask == 0xffff0000u) {
			if (tblBase == 0xbd000000u || tblBase == 0xbd100000u ||
			    tblBase == 0xbd200000u || tblBase == 0xbd300000u ||
			    tblBase == 0xbd400000u) {
				masked = streamId & 0xfff00000u;
				chMask = 15u;
			} else if (tblBase == 0xbd800000u) {
				masked = streamId & 0xffe00000u;
				chMask = 31u;
			} else if (tblBase == 0xbd880000u || tblBase == 0xbd900000u ||
			           tblBase == 0xbda00000u) {
				masked = streamId & 0xfff80000u;
				chMask = 7u;
			} else {
				masked = streamId & 0xffff0000u;
				chMask = 0u;
			}
			if (masked == tblBase) {
				found = 1;
				*typeOut = idx;
				*chOut = (s32)((streamId >> 16) & chMask);
			}
		} else if (tblMask == 0xffffff00u) {
			if (tblBase == 0xbf01e000u) {
				masked = streamId & 0xffffe000u;
				chMask = 31u;
			}
			if (masked == tblBase) {
				found = 1;
				*typeOut = idx;
				*chOut = (s32)((streamId >> 8) & chMask);
			}
		} else if (tblMask == 0xffffffffu) {
			masked = streamId & 0xffffff00u;
			if (masked == tblBase) {
				found = 1;
				*typeOut = idx;
				*chOut = (s32)(streamId & 0xffffu);
			}
		} else if (tblMask == 0xff000000u) {
			if (tblBase == 0xe0000000u) {
				masked = streamId & 0xf0000000u;
				chMask = 15u;
			} else if (tblBase == 0xc0000000u) {
				masked = streamId & 0xe0000000u;
				chMask = 31u;
			} else {
				masked = streamId & 0xff000000u;
				chMask = 0u;
			}
			if (masked == tblBase) {
				found = 1;
				*typeOut = idx;
				*chOut = (s32)((streamId >> 24) & chMask);
			}
		}
	}
	return found;
}

/* 169: 0x780c (44B) */
static s32 mpegMatchStreamId(s32 typeIdx)
{
	if (typeIdx < 19)
		return (s32)g_stream_id_table[typeIdx][1];
	return 0x806101fe;
}

/* 170: 0x7838 (24B) */
static u16 mpegReadBe16(const u8 *src)
{
	return (u16)((src[0] << 8) + src[1]);
}

/* 171: 0x7850 (32B) */
static u32 mpegReadBe24(const u8 *src)
{
	return ((u32)src[0] << 16) + ((u32)src[1] << 8) + (u32)src[2];
}

/* 172: 0x7870 (44B) */
static u32 mpegReadBe32(const u8 *src)
{
	return ((u32)src[0] << 24) + ((u32)src[1] << 16) + ((u32)src[2] << 8) + (u32)src[3];
}

/* 173: 0x789c (168B) */
static u32 mpegReadBits64(const u8 *src, s32 byteLen)
{
	switch (byteLen) {
	case 1:
		return (u32)src[0];
	case 2:
		return (u16)(((u32)src[0] << 8) + (u32)src[1]);
	case 3:
		return ((u32)src[0] << 16) + ((u32)src[1] << 8) + (u32)src[2];
	case 4:
		return ((u32)src[0] << 24) + ((u32)src[1] << 16) + ((u32)src[2] << 8) + (u32)src[3];
	default:
		return 0;
	}
}
