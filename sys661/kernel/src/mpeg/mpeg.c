#include <common/types.h>

typedef struct SceMpegCtx {
    u8 reserved00[0x10];    /* 0x000..0x00f */
    u32 magic_flags;        /* 0x010 */
    u32 stream_list;        /* 0x014 */
    u32 ringbuf_ptr;        /* 0x018 */
    u32 frame_width;        /* 0x01c */
    u32 ddr_top;            /* 0x020 */
    u32 active_slot;        /* 0x024 */
    u8 slots[0x684];        /* 0x028..0x6ab */
    s32 avc_mode;           /* 0x6ac */
    u32 avc_es_buf;         /* 0x6b0 */
    u8 avc_hdr_b0;          /* 0x6b4 */
    u8 avc_hdr_b1;          /* 0x6b5 */
    u8 reserved6b6[2];      /* 0x6b6..0x6b7 */
    u32 avc_param_6b8;      /* 0x6b8 */
    u32 avc_param_6bc;      /* 0x6bc */
    u32 avc_work_ptr;       /* 0x6c0 */
    u32 avc_mem1;           /* 0x6c4 */
    u32 avc_mem2;           /* 0x6c8 */
    u32 avc_mem3;           /* 0x6cc */
    u32 avc_arg4;           /* 0x6d0 */
    u32 reserved6d4;        /* 0x6d4 */
    u32 avc_arg5;           /* 0x6d8 */
    u32 avc_arg6;           /* 0x6dc */
    u32 avc_arg7;           /* 0x6e0 */
    u32 avc_arg8;           /* 0x6e4 */
    u32 avc_arg9;           /* 0x6e8 */
    u32 avc_arg10;          /* 0x6ec */
    u32 reserved6f0;        /* 0x6f0 */
    u32 atrac_edram;        /* 0x6f4 */
    u32 atrac_work_mem;     /* 0x6f8 */
    u32 atrac_work_ptr;     /* 0x6fc */
} SceMpegCtx;

typedef struct SceMpegStreamNode {
    u8 data[0x24];          /* 0x00..0x23 */
    u32 field24;            /* 0x24 */
    u32 field28;            /* 0x28 */
} SceMpegStreamNode;


/* Imports from Kernel_Library */
extern void *sceKernelMemset(void *dst, int c, u32 size);
extern s32 sceKernelLockLwMutex(void *work, int count, u32 *timeout);
extern s32 _sceKernelCheckThreadStack(void);
extern s32 sceKernelUnlockLwMutex(void *work, int count);
extern void *sceKernelMemcpy(void *dst, const void *src, u32 size);

/* Imports from ThreadManForUser */
extern s32 sceKernelCreateFpl(const char *name, u32 part, u32 attr, u32 block_size, u32 num_blocks, void *opt);
extern s32 sceKernelAllocateFpl(s32 uid, void **data, u32 *timeout);
extern s32 sceKernelDeleteFpl(s32 uid);
extern s32 sceKernelFreeFpl(s32 uid, void *data);
extern s32 sceKernelDeleteLwMutex(void *work);
extern s32 sceKernelCreateLwMutex(void *work, const char *name, u32 attr, int init_count, void *opt);

/* Imports from UtilsForUser */
extern void sceKernelDcacheWritebackInvalidateRange(const void *p, u32 size);
extern void sceKernelDcacheWritebackRange(const void *p, u32 size);

/* Imports from sceAudiocodec */
extern s32 sceAudiocodecReleaseEDRAM(u32 *work);
extern s32 sceAudiocodecGetEDRAM(u32 *work, int type);
extern s32 sceAudiocodecInit(u32 *work, int type);
extern s32 sceAudiocodecDecode(u32 *work, int type);
extern s32 sceAudiocodecCheckNeedMem(u32 *work, int type);

/* Imports from sceGe_user */
extern s32 sceGeListEnQueue(const void *list, void *stall, int cbid, void *arg);

/* Imports from sceMpegbase */
extern s32 sceMpegbase_0530BE4E(u32 mode);
extern s32 sceMpegbase_119D5DDA(u32 a0, u32 a1, u32 a2, u32 a3);
extern s32 sceMpegBaseCscAvcRange(void *dst, u32 mode, const u32 *range, int width, void *src);
extern s32 sceMpegbase_31007FF8(void *mp);
extern s32 sceMpegBaseCscInit(int width);
extern s32 sceMpegBaseYCrCbCopy(void *dst, const void *src, int flags);
extern s32 sceMpegBaseCscAvc(void *dst, u32 mode, int width, void *src);
extern s32 sceMpegBasePESpacketCopy(void *list);

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
extern s32 sceVideocodec_D95C24D5(u32 *work, int type);
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
static s32 mpegCreateInternal(u32 *mp_ptr, void *data, s32 size, u32 *rb, s32 frameWidth, s32 mode, u32 ddrtop);
static s32 mpegDeleteInternal(u32 *mp_ptr);
static s32 mpegInitAuInternal(u32 *mp_ptr, u32 buf, u32 *au);
static u32 *mpegGetAvcResPtr(void);
static u32 mpegGetInitFlags(void);
static u32 mpegSetInitFlags(u32 val);
static s32 mpegQueryStreamOffsetAndInit(u32 *mp_ptr, s32 type, s32 ch, u32 buf, u32 *s0);
static s32 mpegAvcDecodeModeInternal(u32 *mp_ptr, const s32 *mode);
static s32 mpegAvcQueryYCbCrSizeInternal(u32 *mp_ptr, s32 idx, u32 *out);
static s32 mpegAvcInitYCbCrInternal(u32 *mp_ptr, u32 *out);
static s32 mpegAvcDecodeInternal(u32 *mp_ptr, u32 *au, s32 frameWidth, u32 *buf_list, u32 *num_pic);
static s32 mpegAvcDecodeStopInternal(u32 *mp_ptr, s32 frameWidth, u32 *buf_list, u32 *num_pic);
static s32 mpegAvcFlushInternal(u32 *mp_ptr);
static s32 mpegAvcQueryYCbCrSizeDefault(u32 *mp_ptr, u32 *out);
static s32 mpegAvcInitYCbCrWithDims(u32 *mp_ptr, s32 mode, s32 width, s32 height, u32 *buf);
static s32 mpegAvcQueryYCbCrMemSize(u32 *mp_ptr, s32 mode, s32 width, s32 height, s32 *out_size);
static s32 mpegAvcCscRangeInternal(u32 *mp_ptr, u32 *src, const u32 *range, s32 frameWidth, void *dst);
static s32 mpegAvcCopyYCbCrInternal(u32 *mp_ptr, u32 *dst, u32 *src);
static s32 mpegAvcCscInternal(u32 *mp_ptr, void *dst_buf, u32 *src, s32 flags);
static s32 mpegAvcDecodeYCbCrInternal(u32 *mp_ptr, u32 *au, u32 *buf_list, u32 *num_pic);
static s32 mpegAvcDecodeStopYCbCrInternal(u32 *mp_ptr, u32 *buf_list, u32 *num_pic);
static s32 mpegAvcCalcBufferSizes(s32 width, s32 height, u32 *out1, u32 *out2);
static s32 mpegAvcConvertYCbCrInternal(u32 *mp_ptr, u32 *dst, u32 *src, s32 idx);
static s32 mpegAvcDecodeDetailInternal(u32 *mp_ptr, u32 *buf_list, u32 *num_pic);
static s32 mpegAvcFillDetailInfo(u32 *dst, s32 idx, s32 w_mb, s32 h_mb, u32 *src_info);
static s32 mpegAtracDecodeInternal(u32 *mp_ptr, u32 *au, void *buf, s32 init_flag);
static s32 mpegQueryStreamOffsetInternal(u32 *mp_ptr, const u8 *header, u32 *out_offset);
static s32 mpegValidateHandle(u32 *mp_ptr);
static s32 mpegQueryStreamSizeInternal(const u8 *header, u32 *out_size);
static s32 mpegCheckPsmfVersion(const u8 *header);
static u32 *mpegFindStreamSlot(u32 code, u32 mp, s32 mode);
static s32 mpegPeekStreamPackets(u32 *slot, s32 skip, s32 max_count, void *dst, s32 *out_count);
static s32 mpegPopStreamPackets(u32 *slot, s32 count, s32 *out_popped);
static s32 mpegRingbufferPutDemux(u32 *mp_ptr, u32 *rb, s32 num_packets, s32 *out_parsed);
static void mpegBitstreamSkip(u32 *bs, u32 num_bits);
static s32 mpegParsePackHeader(u32 *bs, u32 *pack_info);
static s32 mpegParsePesHeader(u32 *bs, u32 *pes_info);
static s32 mpegRingbufferPutInternal(u32 *rb, s32 num_packets, s32 available);
static s32 mpegRingbufferWritePackets(u32 *rb, s32 num_packets, s32 available);
static s32 mpegRingbufferResetInternal(u32 *rb);
static s32 mpegRingbufferQueryMemSizeInternal(s32 packets);
static s32 mpegRingbufferQueryPackNumInternal(u32 mem_size);
static s32 mpegRingbufferConstructInternal(u32 *rb, s32 packets, void *data, s32 size, void *cb, void *arg);
static s32 mpegRingbufferDestructInternal(u32 *rb);
static s32 mpegRingbufferAvailableSizeInternal(u32 *rb);
static s32 mpegRingbufferConsume(u32 *rb, s32 consumed);
static s32 mpegRingbufferCheckAddr(u32 *rb, s32 *min_pkt, u32 addr);
static s32 mpegGetAuInternal(u32 *mp_ptr, u32 *stream, s32 type, u32 *au);
static s32 mpegGetAtracAuInternal(u32 *mp_ptr, u32 *stream, u32 *au, u32 *attr);
static s32 mpegGetAvcAuInternal(u32 *mp_ptr, u32 *stream, s32 type, u32 *au, void *header_out);
static s32 mpegGetUserdataAuInternal(u32 *mp_ptr, u32 *stream, u32 *au, void **es_out);
static s32 mpegRingbufferSetReadPos(u32 *rb, u32 p1, u32 p2);
static s32 mpegGetPcmAuInternal(u32 *mp_ptr, u32 *stream, u32 *au, u32 *attr);
static s32 mpegAvcCopyEsData(u32 *mp_ptr, const void *src, s32 len, u32 *au);
static s32 mpegAvcAssembleNal(u32 *mp_ptr, u32 *nal, u32 *au);
static s32 mpegEsNodeInit(u32 *node, u32 a1, u32 a2, u32 a3, u32 t0);
static s32 mpegEsNodeSetNext(u32 *node, u32 next);
static s32 mpegGetAvcNalAuInternal(u32 *mp_ptr, u32 *stream, u32 *au, void *header_out);
static s32 mpegEsNodeFlushCache(u32 *node);
static s32 mpegEsFinalizeChain(u32 *mp_ptr, u32 *ctx, u32 *au);
static s32 mpegEsAppendChunk(u32 *mp_ptr, u32 *ctx, const void *src, s32 len, s32 type, u32 *au);
static s32 mpegEsResetTable(u32 *ctx);
static s32 mpegMallocAvcEsBufInternal(u32 *mp_ptr);
static s32 mpegFreeAvcEsBufInternal(u32 *mp_ptr, s32 buf_id);
static u32 mpegGetAvcEsBufPtr(u32 *mp_ptr, s32 buf_id);
static s32 mpegQueryAtracEsSizeInternal(u32 *mp_ptr, u32 *es_size, u32 *out_size);
static s32 mpegQueryPcmEsSizeInternal(u32 *mp_ptr, u32 *es_size, u32 *out_size);
static s32 mpegQueryUserdataEsSizeInternal(u32 *mp_ptr, u32 *es_size, u32 *out_size);
static u32 psmfGetStreamSize(const u8 *p);
static u32 psmfGetStreamType(const u8 *p);
static u32 psmfGetEpMapFlag(const u8 *p);
static u32 psmfGetStreamOffset(const u8 *p);
static u32 psmfGetStreamCount(const u8 *p);
static const u8 *psmfGetStreamEntry(const u8 *p, s32 idx);
static const u8 *psmfGetPayloadPtr(const u8 *p);
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
static s32 mpegVcodecSetup(u32 *vwork, void *mem1, void *mem2, void *mem3, u32 a4, u32 *a5, u32 a6, u32 a7, u32 a8, u32 a9, u32 a10);
static s32 mpegVcodecReleaseEdram(void);
static u32 mpegVcodecGetEdramAddr(void);
static s32 mpegVcodecDecode(u32 *vwork);
static s32 mpegVcodecStop(u32 *vwork);
static s32 mpegVcodecDelete(u32 *vwork);
static s32 mpegVcodecSetOption(u32 *vwork, u32 a1);
static s32 mpegAcodecInit(s32 mode);
static s32 mpegAcodecQueryMem(u32 *out, s32 mode, u32 *flag_out);
static s32 mpegAcodecSetup(u32 *awork, u32 edram, u32 work_mem, const u8 *hdr2, const u8 *hdr4);
static s32 mpegAcodecReleaseEdram(void);
static s32 mpegAcodecQueryMemLocked(u32 *out, s32 mode);
static u32 mpegAcodecGetFlag(u32 *flag_out);
static s32 mpegAcodecDecode(u32 *awork);
static s32 mpegStrncmp(const char *s1, const char *s2, s32 n);
static s32 mpegStrcmp(const char *s1, const char *s2);
static s32 mpegAbs(s32 x);
static s32 mpegStreamTypeToId(s32 type, s32 ch);
static s32 mpegStreamIdToType(s32 *type_out, s32 *ch_out, u32 stream_id);
static s32 mpegMatchStreamId(s32 idx, u32 stream_id);
static u16 mpegReadBe16(const u8 *p);
static u32 mpegReadBe24(const u8 *p);
static u32 mpegReadBe32(const u8 *p);
static void mpegReadBits64(const u8 *p, s32 bit_offset, u32 *hi_out, u32 *lo_out);

/* 0: 0x0000 (300B) */
static s32 mpegFlushStreamInternal(u32 *mp_ptr, u32 *stream)
{
	s32 ret = mpegValidateHandle(mp_ptr);
	if (ret != 0)
		return ret;
	if (stream == 0)
		return 0x806101fe;
	u32 mp = *mp_ptr;
	mpegLockLwMutex((void *)(mp + 0x66c));
	u32 *slot = mpegFindStreamSlot(stream[0], mp, 1);
	if (slot == (u32 *)-1) {
		mpegUnlockLwMutex((void *)(mp + 0x66c));
		return 0x806101fe;
	}
	if (slot[2] != (u32)-1) {
		slot[3] = 0;
		slot[0] = (u32)-1;
		slot[1] = (u32)-1;
		if (stream[2] != (u32)-1) {
			u32 *s2 = (u32 *)stream[2];
			s2[3] = 0;
			s2[1] = (u32)-1;
			s2[0] = (u32)-1;
		}
		stream[4] = 0x7fff;
		stream[11] = 0;
		s32 type = 0, ch = 0;
		ret = mpegStreamIdToType(&type, &ch, stream[0]);
		const u32 *def = g_stream_defaults[type];
		u32 d0 = def[0];
		u32 d1 = def[1];
		stream[6] = 1;
		u32 flags = stream[12];
		stream[10] = d0;
		stream[5] = d1;
		stream[12] = flags & ~4u;
		stream[16] = 0;
		stream[8] = 0;
		stream[13] = 0;
		stream[14] = 0;
	}
	mpegUnlockLwMutex((void *)(mp + 0x66c));
	return ret;
}

/* 1: 0x012c (144B) */
static s32 mpegFlushAllStreamInternal(u32 *mp_ptr)
{
	s32 ret = mpegValidateHandle(mp_ptr);
	if (ret != 0)
		return ret;
	u32 mp = *mp_ptr;
	for (s32 i = 0; i < 0x70; i++) {
		u32 *slot = (u32 *)(((SceMpegCtx *)mp)->ringbuf_ptr + (i << 4));
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
	s32 ret = mpegValidateHandle(mp_ptr);
	if (ret != 0)
		return ret;
	if (stream == 0)
		return 0x806101fe;
	u32 mp = *mp_ptr;
	mpegLockLwMutex((void *)(mp + 0x66c));
	u32 *slot = mpegFindStreamSlot(stream[0], mp, 1);
	if (slot == (u32 *)-1) {
		mpegUnlockLwMutex((void *)(mp + 0x66c));
		return 0x806101fe;
	}
	if (slot[2] != (u32)-1) {
		slot[3] = 0;
		slot[2] = (u32)-1;
		slot[0] = (u32)-1;
		slot[1] = (u32)-1;
		if (stream[2] != (u32)-1) {
			u32 *s2 = (u32 *)stream[2];
			s2[1] = (u32)-1;
			s2[3] = 0;
			s2[2] = (u32)-1;
			s2[0] = (u32)-1;
		}
	}
	mpegUnlockLwMutex((void *)(mp + 0x66c));
	return 0;
}

/* 3: 0x0284 (248B) */
static u32 *mpegRegistStreamInternal(u32 *mp_ptr, s32 type, s32 ch)
{
	if (mpegValidateHandle(mp_ptr) != 0)
		return 0;
	u32 mp = *mp_ptr;
	mpegLockLwMutex((void *)(mp + 0x66c));
	s32 code = mpegStreamTypeToId(type, ch);
	u32 *s0 = mpegFindStreamSlot(code, mp, 2);
	if (s0 == (u32 *)-1) {
		mpegUnlockLwMutex((void *)(mp + 0x66c));
		return 0;
	}
	u32 buf = (u32)-1;
	if (type == 0)
		buf = ((SceMpegCtx *)mp)->ddr_top + (ch << 11);
	if (mpegQueryStreamOffsetAndInit(mp_ptr, type, ch, buf, s0) != 0) {
		mpegUnlockLwMutex((void *)(mp + 0x66c));
		return 0;
	}
	mpegUnlockLwMutex((void *)(mp + 0x66c));
	return s0;
}

/* 4: 0x037c (188B) */
static s32 mpegInitInternal(void)
{
	if (g_init_flags != 0)
		return 0x80618005;
	g_init_flags = 1;
	if (mpegCreateLwMutex(0, "SceMpegLibmpeg", g_libmpeg_mutex) < 0)
		return 0x80618006;
	if (mpegCreateLwMutex(0, "SceMpegAvc", g_avc_mutex) < 0)
		return 0x80618006;
	if (mpegCreateLwMutex(0, "SceMpegAtrac", g_atrac_mutex) < 0)
		return 0x80618006;
	s32 ret = mpegVcodecInit();
	if (ret != 0)
		return ret;
	ret = mpegAcodecInit(0);
	if (ret != 0)
		return ret;
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
	if (mpegDeleteLwMutex(g_libmpeg_mutex) < 0)
		return 0x80618006;
	if (mpegDeleteLwMutex(g_avc_mutex) < 0)
		return 0x80618006;
	s32 r = mpegDeleteLwMutex(g_atrac_mutex);
	return (r < 0) ? (s32)0x80618006 : 0;
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
static s32 mpegCreateInternal(u32 *mp_ptr, void *data, s32 size, u32 *rb, s32 frameWidth, s32 mode, u32 ddrtop)
{
	s32 r = sceMpegbase_31007FF8(mp_ptr);
	if (r == 1)
		sceGeListEnQueue(g_ge_list, 0, -1, 0);
	if ((g_init_flags & 1) == 0)
		return 0x80618009;
	if ((g_init_flags & 2) != 0)
		return 0x80618005;
	g_init_flags |= 2;
	if (size < mpegQueryMemSizeInternal(mode))
		return 0x80610022;
	mpegLockLwMutex(g_libmpeg_mutex);
	sceKernelMemset(data, 0, size);
	u32 base = ((u32)data + 0x3f) & ~0x3fu;
	*mp_ptr = base;
	sceKernelMemcpy((void *)base, "LIBMPEG", 8);
	*(u32 *)(base + 8) = 0x00313030;
	*(u32 *)(base + 12) = (u32)-1;
	((SceMpegCtx *)base)->avc_mode = mode;
	if (mpegCreateLwMutex(0, "SceMpegRingBufferPut", (void *)(base + 0x68c)) != 0) {
		mpegUnlockLwMutex(g_libmpeg_mutex);
		return 0x80618006;
	}
	if (mpegCreateLwMutex(0, "SceMpegMpegData", (void *)(base + 0x66c)) != 0) {
		mpegUnlockLwMutex(g_libmpeg_mutex);
		return 0x80618006;
	}
	if (((SceMpegCtx *)base)->avc_mode == 1) {
		((SceMpegCtx *)base)->magic_flags = 0;
		((SceMpegCtx *)base)->stream_list = 0;
	} else {
		rb[10] = (u32)mp_ptr;
		((SceMpegCtx *)base)->stream_list = rb[8];
		((SceMpegCtx *)base)->magic_flags = (u32)rb;
	}
	u32 p_slots = ((base + 0x704) + 0x3f) & ~0x3fu;
	((SceMpegCtx *)base)->ringbuf_ptr = p_slots;
	for (s32 i = 0; i < 0x70; i++) {
		u32 *slot = (u32 *)(p_slots + (i << 4));
		slot[0] = (u32)-1;
		slot[1] = (u32)-1;
		slot[2] = (u32)-1;
		slot[3] = 0;
	}
	u32 p_streams = (p_slots + 0x704 + 0x3f) & ~0x3fu;
	((SceMpegCtx *)base)->frame_width = p_streams;
	u32 p_buf32k = (p_streams + 0x1dc4 + 0x3f) & ~0x3fu;
	((SceMpegCtx *)base)->ddr_top = p_buf32k;
	sceKernelMemset((void *)p_buf32k, 0, 0x8000);
	((SceMpegCtx *)base)->active_slot = 0;
	u32 p_cur = p_buf32k + 0x8644;
	for (s32 j = 0; j < 2; j++) {
		u32 tbl = base + j * 0x644 + 0x28;
		*(u32 *)tbl = 0x63;
		for (s32 i = 0; i < (s32)*(u32 *)tbl; i++) {
			u32 node = tbl + 4 + (i << 4);
			mpegEsNodeInit((u32 *)node, 0, 0, node + 16, 0);
		}
		u32 last = tbl + 4 + ((*(u32 *)tbl - 1) << 4);
		mpegEsNodeSetNext((u32 *)last, 0);
	}
	u32 p_vwork = (p_cur + 0x3f) & ~0x3fu;
	u32 p_vmem1 = p_vwork + 0x80;
	u32 p_vmem2 = p_vmem1 + 0x40;
	u32 p_vmem3 = p_vmem2 + 0xc0;
	p_cur = p_vmem3 + 0x80;
	((SceMpegCtx *)base)->avc_work_ptr = p_vwork;
	((SceMpegCtx *)base)->avc_mem1 = p_vmem1;
	((SceMpegCtx *)base)->avc_mem2 = p_vmem2;
	((SceMpegCtx *)base)->avc_mem3 = p_vmem3;
	((SceMpegCtx *)base)->avc_arg4 = 0;
	u32 edram = mpegVcodecGetEdramAddr();
	((SceMpegCtx *)base)->avc_arg5 = edram;
	if (edram == 0) {
		mpegUnlockLwMutex(g_libmpeg_mutex);
		return 0x80610022;
	}
	if (mode == 1) {
		((SceMpegCtx *)base)->avc_es_buf = 0;
		((SceMpegCtx *)base)->avc_hdr_b0 = (u8)mode;
		((SceMpegCtx *)base)->avc_hdr_b1 = (u8)mode;
		((SceMpegCtx *)base)->avc_param_6b8 = 0;
		((SceMpegCtx *)base)->avc_param_6bc = 0;
	} else {
		((SceMpegCtx *)base)->avc_hdr_b0 = 0;
		((SceMpegCtx *)base)->avc_hdr_b1 = 0;
		((SceMpegCtx *)base)->avc_es_buf = 2;
		((SceMpegCtx *)base)->avc_param_6b8 = 0x4a000;
		((SceMpegCtx *)base)->avc_param_6bc = 0x62400;
		if (ddrtop == 0)
			ddrtop = 0x09000000;
	}
	if (mode == 1) {
		((SceMpegCtx *)base)->avc_arg7 = 0x1e0;
		((SceMpegCtx *)base)->avc_arg8 = 0x110;
		((SceMpegCtx *)base)->avc_arg9 = 3;
		((SceMpegCtx *)base)->avc_arg6 = ddrtop;
	} else {
		((SceMpegCtx *)base)->avc_arg7 = 0x1e0;
		((SceMpegCtx *)base)->avc_arg8 = 0x110;
		((SceMpegCtx *)base)->avc_arg9 = 2;
		((SceMpegCtx *)base)->avc_arg6 = ddrtop;
	}
	((SceMpegCtx *)base)->avc_arg10 = 0;
	r = mpegVcodecSetup((u32 *)p_vwork, (void *)p_vmem1, (void *)p_vmem2, (void *)p_vmem3,
	             ((SceMpegCtx *)base)->avc_arg4, (u32 *)(base + 0x6d8), ((SceMpegCtx *)base)->avc_arg6,
	             ((SceMpegCtx *)base)->avc_arg7, ((SceMpegCtx *)base)->avc_arg8, ((SceMpegCtx *)base)->avc_arg9,
	             ((SceMpegCtx *)base)->avc_arg10);
	if (r != 0) {
		mpegUnlockLwMutex(g_libmpeg_mutex);
		return r;
	}
	if (mode == 1)
		mpegVcodecSetOption((*(u32 **)&((SceMpegCtx *)base)->avc_work_ptr), 1);
	r = sceMpegBaseCscInit(frameWidth);
	if (r != 0) {
		mpegUnlockLwMutex(g_libmpeg_mutex);
		return r;
	}
	if (mode == 1) {
		((SceMpegCtx *)base)->atrac_work_ptr = 0;
		((SceMpegCtx *)base)->atrac_work_mem = 0;
	} else {
		((SceMpegCtx *)base)->atrac_work_ptr = p_cur;
		u32 need_mem = 0;
		r = mpegAcodecQueryMemLocked(&need_mem, mode);
		if (r != 0) {
			mpegUnlockLwMutex(g_libmpeg_mutex);
			return r;
		}
		p_cur += 0x80;
		((SceMpegCtx *)base)->atrac_work_mem = need_mem;
	}
	if (mode == 1) {
		((SceMpegCtx *)base)->atrac_edram = 0;
	} else {
		u32 a_edram = mpegAcodecGetFlag(0);
		((SceMpegCtx *)base)->atrac_edram = a_edram;
		if (a_edram == 0) {
			mpegUnlockLwMutex(g_libmpeg_mutex);
			return 0x80610022;
		}
	}
	if ((u32)data + (u32)size <= p_cur)
		r = 0x80610022;
	mpegUnlockLwMutex(g_libmpeg_mutex);
	return r;
}

/* 8: 0x0a28 (208B) */
static s32 mpegDeleteInternal(u32 *mp_ptr)
{
	s32 ret = mpegValidateHandle(mp_ptr);
	if (ret != 0)
		return ret;
	u32 base = *mp_ptr;
	g_init_flags &= ~2u;
	s32 mode = ((SceMpegCtx *)base)->avc_mode;
	if (mode != 2 && mode != 3) {
		ret = mpegVcodecDelete((*(u32 **)&((SceMpegCtx *)base)->avc_work_ptr));
		if (ret != 0)
			return ret;
	}
	mpegLockLwMutex(g_libmpeg_mutex);
	for (s32 i = 0; i < 0x70; i++) {
		u32 *slot = (u32 *)(((SceMpegCtx *)base)->ringbuf_ptr + (i << 4));
		slot[0] = (u32)-1;
		slot[1] = (u32)-1;
		slot[2] = (u32)-1;
		slot[3] = 0;
	}
	if (mpegDeleteLwMutex((void *)(base + 0x66c)) < 0)
		return 0x80618006;
	if (((SceMpegCtx *)base)->stream_list != 0) {
		if (mpegDeleteLwMutex((void *)(base + 0x68c)) < 0)
			return 0x80618006;
	}
	mpegUnlockLwMutex(g_libmpeg_mutex);
	return 0;
}

/* 9: 0x0af8 (72B) */
static s32 mpegInitAuInternal(u32 *mp_ptr, u32 buf, u32 *au)
{
	s32 ret = mpegValidateHandle(mp_ptr);
	if (ret != 0)
		return ret;
	u32 phys = mpegGetAvcEsBufPtr(mp_ptr, (s32)buf);
	au[4] = phys;
	au[5] = 0;
	return ret;
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
static s32 mpegQueryStreamOffsetAndInit(u32 *mp_ptr, s32 type, s32 ch, u32 buf, u32 *s0)
{
	s32 ret = mpegValidateHandle(mp_ptr);
	if (ret != 0)
		return ret;
	if (s0 == 0)
		return 0x806101fe;
	u32 mp = *mp_ptr;
	if ((u32)type >= 7)
		return 0x806101fe;
	const u32 *def = g_stream_defaults[type];
	u32 d0 = def[0];
	u32 d1 = def[1];
	s0[4] = 0x7fff;
	s0[10] = d0;
	s0[5] = d1;
	s0[6] = 1;
	s0[3] = buf;
	s0[7] = (buf != (u32)-1) ? 0x800 : 0;
	s32 code = mpegStreamTypeToId(type, ch);
	s0[0] = code;
	s0[12] = 0;
	s0[11] = 0;
	s0[8] = 0;
	s0[13] = 0;
	s0[14] = 0;
	s0[15] = 0;
	s0[16] = 0;
	u32 *slot = mpegFindStreamSlot(code, mp, 1);
	if (slot == (u32 *)-1)
		return 0x806101fe;
	slot[0] = (u32)-1;
	slot[1] = (u32)-1;
	slot[2] = (u32)s0;
	slot[3] = 0;
	s0[1] = (u32)slot;
	s0[2] = (u32)-1;
	if (type == 0) {
		s32 code6 = mpegStreamTypeToId(6, ch);
		u32 *slot6 = mpegFindStreamSlot(code6, mp, 1);
		if (slot6 == (u32 *)-1)
			return 0x806101fe;
		slot6[0] = (u32)-1;
		slot6[1] = (u32)-1;
		slot6[2] = (u32)s0;
		slot6[3] = 0;
		s0[2] = (u32)slot6;
	}
	return 0;
}

/* 14: 0x0cf4 (172B) */
static s32 mpegAvcDecodeModeInternal(u32 *mp_ptr, const s32 *mode)
{
	s32 ret = mpegValidateHandle(mp_ptr);
	if (ret != 0)
		return ret;
	if (mode == 0)
		return 0x806101fe;
	u32 mp = *mp_ptr;
	s32 m0 = mode[0];
	s32 m1 = mode[1];
	u32 *vwork = (*(u32 **)&((SceMpegCtx *)mp)->avc_work_ptr);
	if (m0 < -1 && m1 < -1)
		return 0x806101fe;
	if (m0 >= -1) {
		if (m0 >= 3)
			return 0x806101fe;
		vwork[13] = m0;
	}
	if (m1 >= -1) {
		if (m1 >= 4)
			return 0x806101fe;
		sceMpegbase_0530BE4E(g_csc_modes[m1]);
	}
	return ret;
}

/* 15: 0x0da0 (272B) */
static s32 mpegAvcQueryYCbCrSizeInternal(u32 *mp_ptr, s32 idx, u32 *out)
{
	s32 ret = mpegValidateHandle(mp_ptr);
	if (ret != 0)
		return ret;
	if (out == 0)
		return 0x806101fe;
	u32 vwork_addr = 0;
	ret = mpegAvcInitYCbCrInternal(mp_ptr, &vwork_addr);
	if (ret != 0)
		return ret;
	u32 *vwork = (u32 *)vwork_addr;
	u32 v0 = vwork[2];
	u32 a0 = vwork[11];
	u32 a1 = vwork[12];
	u32 *a3 = (u32 *)vwork[4];
	out[0] = v0;
	if (idx < 0 || idx >= (s32)a3[8])
		return 0x806201fe;
	u32 *pic = (u32 *)(a0 + idx * 0x2c);
	out[10] = a1;
	out[1] = pic[8];
	out[11] = pic[9];
	out[2] = a3[2];
	out[12] = pic[10];
	out[3] = a3[3];
	out[4] = a3[4];
	out[5] = a3[5];
	out[6] = a3[6];
	out[7] = a3[7];
	out[8] = a3[8];
	out[9] = a3[9];
	return 0;
}

/* 16: 0x0eb0 (76B) */
static s32 mpegAvcInitYCbCrInternal(u32 *mp_ptr, u32 *out)
{
	s32 ret = mpegValidateHandle(mp_ptr);
	if (ret != 0)
		return ret;
	if (out == 0)
		return 0x806101fe;
	*out = ((SceMpegCtx *)(*mp_ptr))->avc_work_ptr;
	return ret;
}

/* 17: 0x0efc (436B) */
static s32 mpegAvcDecodeInternal(u32 *mp_ptr, u32 *au, s32 frameWidth, u32 *buf_list, u32 *num_pic)
{
	s32 ret = mpegValidateHandle(mp_ptr);
	if (ret != 0)
		return ret;
	if (au == 0 || buf_list == 0 || num_pic == 0)
		return 0x806101fe;
	if ((s32)au[5] <= 0)
		return 0x80610001;
	u32 mp = *mp_ptr;
	*num_pic = 0;
	u32 *vwork = (*(u32 **)&((SceMpegCtx *)mp)->avc_work_ptr);
	u32 es_buf = mpegGetAvcEsBufPtr(mp_ptr, (s32)au[4]) & 0x0fffffffu;
	vwork[9] = es_buf;
	vwork[10] = au[5];
	ret = mpegVcodecDecode(vwork);
	au[5] = 0;
	if (ret != 0)
		return ret;
	for (u32 i = 0; i < vwork[11]; i++) {
		u32 dst = buf_list[i] + g_csc_offsets[i];
		u32 csc_src[8];
		for (s32 k = 0; k < 4; k++) {
			csc_src[k] = *(u32 *)(vwork[40] + (i << 5) + (k << 2));
			csc_src[k + 4] = *(u32 *)(vwork[40] + (i << 5) + (k << 2) + 16);
		}
		sceMpegBaseCscAvc((void *)dst, 0, frameWidth, csc_src);
		*num_pic += 1;
	}
	return ret;
}

/* 18: 0x10b0 (336B) */
static s32 mpegAvcDecodeStopInternal(u32 *mp_ptr, s32 frameWidth, u32 *buf_list, u32 *num_pic)
{
	s32 ret = mpegValidateHandle(mp_ptr);
	if (ret != 0)
		return ret;
	if (buf_list == 0 || num_pic == 0)
		return 0x806101fe;
	u32 mp = *mp_ptr;
	*num_pic = 0;
	u32 *vwork = (*(u32 **)&((SceMpegCtx *)mp)->avc_work_ptr);
	ret = mpegVcodecStop(vwork);
	if (ret != 0)
		return ret;
	for (u32 i = 0; i < vwork[11]; i++) {
		u32 dst = buf_list[i] + g_csc_offsets[i];
		u32 csc_src[8];
		for (s32 k = 0; k < 4; k++) {
			csc_src[k] = *(u32 *)(vwork[40] + (i << 5) + (k << 2));
			csc_src[k + 4] = *(u32 *)(vwork[40] + (i << 5) + (k << 2) + 16);
		}
		sceMpegBaseCscAvc((void *)dst, 0, frameWidth, csc_src);
		*num_pic += 1;
	}
	return ret;
}

/* 19: 0x1200 (68B) */
static s32 mpegAvcFlushInternal(u32 *mp_ptr)
{
	s32 ret = mpegValidateHandle(mp_ptr);
	if (ret != 0)
		return ret;
	return mpegVcodecStop((u32 *)((SceMpegCtx *)(*mp_ptr))->avc_work_ptr);
}

/* 20: 0x1244 (32B) */
static s32 mpegAvcQueryYCbCrSizeDefault(u32 *mp_ptr, u32 *out)
{
	return mpegAvcQueryYCbCrSizeInternal(mp_ptr, 0, out);
}

/* 21: 0x1264 (460B) */
static s32 mpegAvcInitYCbCrWithDims(u32 *mp_ptr, s32 mode, s32 width, s32 height, u32 *buf)
{
	s32 ret = mpegValidateHandle(mp_ptr);
	if (ret != 0)
		return ret;
	if (mode == 3) {
		s32 y_size = width * height;
		s32 c_size = (u32)y_size >> 2;
		if (((width / 16) & 7) == 7) {
			y_size += height << 4;
			c_size += height << 2;
		}
		u32 u_ptr = (u32)buf + y_size;
		sceKernelMemset(buf, 0x10, y_size);
		u32 v_ptr = u_ptr + c_size;
		sceKernelMemset((void *)u_ptr, 0x80, c_size);
		sceKernelMemset((void *)v_ptr, 0x80, c_size);
		sceKernelDcacheWritebackRange(buf, y_size);
		sceKernelDcacheWritebackRange((void *)u_ptr, c_size);
		sceKernelDcacheWritebackRange((void *)v_ptr, c_size);
		return 0;
	}
	sceKernelMemset(buf, 0, 0x80);
	if (mode == 2) {
		buf[11] = mode;
		buf[4] = mode;
		buf[6] = mode;
		buf[8] = mode;
		buf[10] = mode;
		buf[5] = mode;
		buf[7] = mode;
		buf[9] = mode;
		return ret;
	}
	if (mode != 1)
		return 0x806101fe;
	u32 s1 = 0, s2 = 0;
	ret = mpegAvcCalcBufferSizes(width, height, &s1, &s2);
	if (ret != 0)
		return ret;
	u32 p0 = (u32)buf + 0x80;
	buf[4] = p0;
	u32 p1 = p0 + s1;
	u32 p2 = p1 + s1;
	u32 p3 = p2 + (s1 >> 1);
	u32 p4 = p3 + (s1 >> 1);
	u32 p5 = p4 + s2;
	u32 p6 = p5 + s2;
	u32 p7 = p6 + (s2 >> 1);
	buf[11] = p7;
	buf[6] = p1;
	buf[8] = p2;
	buf[10] = p3;
	buf[5] = p4;
	buf[7] = p5;
	buf[9] = p6;
	return ret;
}

/* 22: 0x1430 (272B) */
static s32 mpegAvcQueryYCbCrMemSize(u32 *mp_ptr, s32 mode, s32 width, s32 height, s32 *out_size)
{
	*out_size = 0;
	s32 ret = mpegValidateHandle(mp_ptr);
	if (ret != 0)
		return ret;
	if (mode == 2) {
		*out_size = 0x80;
		return ret;
	}
	if (mode == 1) {
		u32 s1 = 0, s2 = 0;
		ret = mpegAvcCalcBufferSizes(width, height, &s1, &s2);
		if (ret != 0)
			return ret;
		*out_size = (s1 + s2) * 3 + 0x80;
		return ret;
	}
	if (mode != 3)
		return 0x806101fe;
	s32 sz = (width * height * 3) / 2;
	if (((width / 16) & 7) == 7)
		sz += height * 24;
	*out_size = sz;
	return ret;
}

/* 23: 0x1540 (540B) */
static s32 mpegAvcCscRangeInternal(u32 *mp_ptr, u32 *src, const u32 *range, s32 frameWidth, void *dst)
{
	s32 ret = mpegValidateHandle(mp_ptr);
	if (ret != 0)
		return ret;
	if (src == 0 || range == 0)
		return 0x806101fe;
	u32 mp = *mp_ptr;
	u32 x = range[0];
	u32 y = range[1];
	u32 w = range[2];
	u32 h = range[3];
	if ((x & 15) != 0 || (y & 15) != 0 || (w & 15) != 0 || (h & 15) != 0)
		return 0x806101fe;
	u32 x_mb = x >> 4;
	u32 y_mb = y >> 4;
	u32 w_mb = w >> 4;
	u32 h_mb = h >> 4;
	if (x_mb + w_mb > src[9] || y_mb + h_mb > src[10])
		return 0x806101fe;
	if (src[8] == 1) {
		if (x_mb == 0 && y_mb == 0 && w_mb == src[9] && h_mb == src[10]) {
			u32 csc_arg[10];
			for (s32 i = 0; i < 8; i++)
				csc_arg[i] = src[i];
			csc_arg[8] = w_mb;
			csc_arg[9] = h_mb;
			ret = sceMpegBaseCscAvc(dst, 0, frameWidth, csc_arg);
		} else {
			u32 r_arg[4];
			r_arg[0] = x_mb;
			r_arg[1] = y_mb;
			r_arg[2] = w_mb;
			r_arg[3] = h_mb;
			ret = sceMpegBaseCscAvcRange(dst, 0, r_arg, frameWidth, src);
		}
		return ret;
	}
	if (src[8] != 2)
		return 0x806101fe;
	if (((SceMpegCtx *)mp)->avc_mode < 4)
		return 0x806101fe;
	u32 *vwork = (*(u32 **)&((SceMpegCtx *)mp)->avc_work_ptr);
	u32 *vc = (u32 *)vwork[0];
	u32 *vsub = (u32 *)vc[12];
	for (s32 i = 0; i < 8; i++)
		vsub[i + 5] = src[i];
	vsub[2] = x;
	vsub[3] = y;
	vsub[0] = w;
	vsub[1] = h;
	vsub[13] = (u32)dst;
	vsub[4] = frameWidth;
	vsub[14] = vwork[13];
	vsub[15] = 0;
	if (sceVideocodec_893B32B1(vc, 0) < 0)
		return 0x80618007;
	return ret;
}

/* 24: 0x175c (144B) */
static s32 mpegAvcCopyYCbCrInternal(u32 *mp_ptr, u32 *dst, u32 *src)
{
	s32 ret = mpegValidateHandle(mp_ptr);
	if (ret != 0)
		return ret;
	if (dst == 0 || src == 0)
		return 0x806101fe;
	if (dst[4] == 2)
		return 0x806101fe;
	return mpegAvcConvertYCbCrInternal(mp_ptr, dst, src, -1);
}

/* 25: 0x17ec (148B) */
static s32 mpegAvcCscInternal(u32 *mp_ptr, void *dst_buf, u32 *src, s32 flags)
{
	s32 ret = mpegValidateHandle(mp_ptr);
	if (ret != 0)
		return ret;
	if (dst_buf == 0 || src == 0)
		return 0x806101fe;
	u32 tmp_dst[8];
	tmp_dst[4] = 3;
	tmp_dst[5] = (u32)dst_buf;
	tmp_dst[3] = (u32)flags;
	return mpegAvcConvertYCbCrInternal(mp_ptr, tmp_dst, src, -1);
}

/* 26: 0x1880 (208B) */
static s32 mpegAvcDecodeYCbCrInternal(u32 *mp_ptr, u32 *au, u32 *buf_list, u32 *num_pic)
{
	s32 ret = mpegValidateHandle(mp_ptr);
	if (ret != 0)
		return ret;
	u32 *vwork = (u32 *)((SceMpegCtx *)(*mp_ptr))->avc_work_ptr;
	vwork[9] = mpegGetAvcEsBufPtr(mp_ptr, (s32)au[4]) & 0x0fffffffu;
	vwork[10] = au[5];
	ret = sceMpegbase_119D5DDA(au[0], au[1], au[2], au[3]);
	if (ret != 0)
		return ret;
	ret = mpegVcodecDecode(vwork);
	au[5] = 0;
	if (ret != 0)
		return ret;
	return mpegAvcDecodeDetailInternal(mp_ptr, buf_list, num_pic);
}

/* 27: 0x1950 (100B) */
static s32 mpegAvcDecodeStopYCbCrInternal(u32 *mp_ptr, u32 *buf_list, u32 *num_pic)
{
	s32 ret = mpegAvcFlushInternal(mp_ptr);
	if (ret != 0)
		return ret;
	return mpegAvcDecodeDetailInternal(mp_ptr, buf_list, num_pic);
}

/* 28: 0x19b4 (164B) */
static s32 mpegAvcCalcBufferSizes(s32 width, s32 height, u32 *out1, u32 *out2)
{
	*out1 = 0;
	*out2 = 0;
	if ((width & 15) != 0 || (height & 15) != 0)
		return 0x806101fe;
	if (width > 480 || width <= 0 || height > 272 || height <= 0)
		return 0x806101fe;
	s32 w_half = width >> 1;
	s32 w_aln = (w_half + 31) & ~31;
	*out1 = w_aln * height;
	*out2 = (w_half - w_aln) * height;
	return 0;
}

/* 29: 0x1a58 (176B) */
static s32 mpegAvcConvertYCbCrInternal(u32 *mp_ptr, u32 *dst, u32 *src, s32 idx)
{
	s32 ret = mpegValidateHandle(mp_ptr);
	if (ret != 0)
		return ret;
	u32 *vwork = (u32 *)((SceMpegCtx *)(*mp_ptr))->avc_work_ptr;
	u32 w_mb = (vwork[5] + 15) >> 4;
	u32 h_mb = (vwork[6] + 15) >> 4;
	u32 info[12];
	for (s32 k = 0; k < 4; k++) {
		info[k] = src[k];
		info[k + 4] = src[k + 4];
	}
	info[8] = dst[8];
	info[9] = w_mb;
	info[10] = h_mb;
	info[11] = vwork[idx + 27];
	return mpegAvcFillDetailInfo(dst, idx, w_mb, h_mb, info);
}

/* 30: 0x1b08 (168B) */
static s32 mpegAvcDecodeDetailInternal(u32 *mp_ptr, u32 *buf_list, u32 *num_pic)
{
	s32 ret = mpegValidateHandle(mp_ptr);
	if (ret != 0)
		return ret;
	*num_pic = 0;
	u32 *vwork = (u32 *)((SceMpegCtx *)(*mp_ptr))->avc_work_ptr;
	for (u32 i = 0; i < vwork[11]; i++) {
		ret = mpegAvcConvertYCbCrInternal(mp_ptr, (u32 *)buf_list[i], (u32 *)(vwork[40] + (i << 5)), i);
		if (ret != 0)
			break;
		*num_pic += 1;
	}
	return ret;
}

/* 31: 0x1bb0 (468B) */
static s32 mpegAvcFillDetailInfo(u32 *dst, s32 idx, s32 w_mb, s32 h_mb, u32 *src_info)
{
	if (dst[8] == 1) {
		dst[9] = w_mb;
		dst[10] = h_mb;
		dst[11] = src_info[11];
		u32 flags = (src_info[11] == 0) ? 3 : 2;
		return sceMpegBaseYCrCbCopy(dst, src_info, flags);
	}
	if (dst[8] != 2)
		return 0x806101fe;
	u32 *vc = g_vcodec_ptr;
	u32 *vsub = g_vcodec_sub_ptr;
	vc[12] = (u32)vsub;
	vsub[9] = src_info[11];
	u32 *vinfo = (u32 *)vc[9];
	u32 y_sz = *(u32 *)(vinfo[43] + (idx << 2));
	u32 c_sz = *(u32 *)(vinfo[44] + (idx << 2));
	vsub[5] = y_sz;
	vsub[7] = y_sz;
	vsub[6] = c_sz;
	vsub[8] = c_sz;
	sceKernelDcacheWritebackInvalidateRange((void *)src_info[0], y_sz);
	sceKernelDcacheWritebackInvalidateRange((void *)src_info[2], y_sz);
	sceKernelDcacheWritebackInvalidateRange((void *)src_info[4], c_sz);
	sceKernelDcacheWritebackInvalidateRange((void *)src_info[6], c_sz);
	vsub[1] = src_info[0];
	vsub[3] = src_info[2];
	vsub[2] = src_info[4];
	vsub[4] = src_info[6];
	s32 ret = sceVideocodec_D95C24D5(vc, 0);
	if (ret != 0)
		return ret;
	for (s32 k = 0; k < 4; k++) {
		dst[k] = src_info[k];
		dst[k + 4] = src_info[k + 4];
	}
	dst[9] = w_mb;
	dst[10] = h_mb;
	dst[11] = src_info[11];
	return 0;
}

/* 32: 0x1d84 (168B) */
static s32 mpegAtracDecodeInternal(u32 *mp_ptr, u32 *au, void *buf, s32 init_flag)
{
	s32 ret = mpegValidateHandle(mp_ptr);
	if (ret != 0)
		return ret;
	u32 mp = *mp_ptr;
	u32 *awork = (*(u32 **)&((SceMpegCtx *)mp)->atrac_work_ptr);
	if (init_flag == 1) {
		u32 es = au[4];
		ret = mpegAcodecSetup(awork, ((SceMpegCtx *)mp)->atrac_edram, ((SceMpegCtx *)mp)->atrac_work_mem,
		               (const u8 *)(es + 2), (const u8 *)(es + 4));
		if (ret != 0)
			return ret;
	}
	awork[8] = (u32)buf;
	awork[6] = au[4];
	return mpegAcodecDecode(awork);
}

/* 33: 0x1e2c (232B) */
static s32 mpegQueryStreamOffsetInternal(u32 *mp_ptr, const u8 *header, u32 *out_offset)
{
	s32 ret = mpegValidateHandle(mp_ptr);
	if (ret != 0)
		return ret;
	u32 mp = *mp_ptr;
	*out_offset = 0;
	if (mpegStrncmp((const char *)header, "PSMF", 4) != 0)
		return 0x806101fe;
	s32 ver = mpegCheckPsmfVersion(header);
	*(s32 *)(mp + 12) = ver;
	if (ver < 0)
		return 0x806101fe;
	u32 off = mpegReadBe32(header + 8);
	if ((off & 0x7ff) != 0) {
		*out_offset = 0;
		return 0x806101fe;
	}
	*out_offset = off;
	return 0;
}

/* 34: 0x1f14 (92B) */
static s32 mpegValidateHandle(u32 *mp_ptr)
{
	if ((mpegGetInitFlags() & 3) != 3)
		return 0x80618009;
	if (mp_ptr == 0)
		return 0x806101fe;
	return (mpegStrcmp((const char *)(*mp_ptr), "LIBMPEG") == 0) ? 0 : (s32)0x806101fe;
}

/* 35: 0x1f70 (68B) */
static s32 mpegQueryStreamSizeInternal(const u8 *header, u32 *out_size)
{
	*out_size = 0;
	if (mpegStrncmp((const char *)header, "PSMF", 4) != 0)
		return 0x806101fe;
	*out_size = mpegReadBe32(header + 12);
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
	s32 type = 0, ch = 0;
	if (mpegStreamIdToType(&type, &ch, code) != 0)
		return (u32 *)-1;
	if (mode == 1) {
		u32 base = ((SceMpegCtx *)mp)->ringbuf_ptr;
		switch (type) {
		case 0:
			return (ch < 16) ? (u32 *)(base + (ch << 4)) : (u32 *)-1;
		case 1:
			return (ch < 16) ? (u32 *)(base + ((ch + 16) << 4)) : (u32 *)-1;
		case 2:
			return (ch < 32) ? (u32 *)(base + ((ch + 32) << 4)) : (u32 *)-1;
		case 3:
			return (ch < 16) ? (u32 *)(base + ((ch + 64) << 4)) : (u32 *)-1;
		case 4:
			return (ch < 32) ? (u32 *)(base + ((ch + 80) << 4)) : (u32 *)-1;
		case 5:
			return (ch < 16) ? (u32 *)(base + (ch << 4)) : (u32 *)-1;
		case 6:
			return (ch < 1) ? (u32 *)(base + ((ch + 0x6f) << 4)) : (u32 *)-1;
		default:
			return (u32 *)-1;
		}
	} else {
		u32 base = ((SceMpegCtx *)mp)->frame_width;
		switch (type) {
		case 0:
			return (ch < 16) ? (u32 *)(base + (ch << 6)) : (u32 *)-1;
		case 1:
			return (ch < 16) ? (u32 *)(base + ((ch + 16) << 6)) : (u32 *)-1;
		case 2:
			return (ch < 32) ? (u32 *)(base + ((ch + 32) << 6)) : (u32 *)-1;
		case 3:
			return (ch < 16) ? (u32 *)(base + ((ch + 64) << 6)) : (u32 *)-1;
		case 4:
			return (ch < 32) ? (u32 *)(base + ((ch + 80) << 6)) : (u32 *)-1;
		case 5:
			return (ch < 16) ? (u32 *)(base + (ch << 6)) : (u32 *)-1;
		default:
			return (u32 *)-1;
		}
	}
}

/* 38: 0x2228 (192B) */
static s32 mpegPeekStreamPackets(u32 *slot, s32 skip, s32 max_count, void *dst, s32 *out_count)
{
	if ((s32)slot[3] <= 0 || skip < 0 || max_count <= 0) {
		*out_count = 0;
		return 0x806101fe;
	}
	u32 node = slot[0];
	for (s32 i = 0; i < skip; i++) {
		if (node == (u32)-1) {
			*out_count = 0;
			return 0;
		}
		node = ((SceMpegStreamNode *)node)->field28;
	}
	s32 cnt = 0;
	u8 *p = (u8 *)dst;
	while (cnt < max_count && node != (u32)-1) {
		sceKernelMemcpy(p, (const void *)node, 0x2c);
		node = ((SceMpegStreamNode *)node)->field28;
		cnt++;
		p += 0x2c;
	}
	*out_count = cnt;
	return 0;
}

/* 39: 0x22e8 (128B) */
static s32 mpegPopStreamPackets(u32 *slot, s32 count, s32 *out_popped)
{
	if (count <= 0) {
		*out_popped = 0;
		return 0x806101fe;
	}
	u32 node = slot[0];
	s32 popped = 0;
	if ((s32)slot[3] > 0) {
		while (popped < count && node != (u32)-1) {
			u32 next = ((SceMpegStreamNode *)node)->field28;
			((SceMpegStreamNode *)node)->field24 = 0;
			slot[3] -= 1;
			node = next;
			popped++;
			slot[0] = next;
		}
	}
	if (slot[0] == (u32)-1)
		slot[1] = (u32)-1;
	*out_popped = popped;
	return 0;
}

/* 40: 0x2368 (828B) */
static s32 mpegRingbufferPutDemux(u32 *mp_ptr, u32 *rb, s32 num_packets, s32 *out_parsed)
{
	s32 ret = mpegValidateHandle(mp_ptr);
	if (ret != 0)
		return ret;
	if (num_packets <= 0)
		return 0x806101fe;
	u32 mp = *mp_ptr;
	*out_parsed = 0;
	u32 bs[5];
	u32 pack_info[3];
	u32 pes_info[14];
	bs[2] = rb[5];
	u32 end_addr = rb[5] + rb[0] * 0x800;
	bs[3] = end_addr;
	u32 start_addr = rb[5] + rb[1] * 0x800;
	bs[0] = start_addr;
	bs[1] = 0;
	s32 parsed = 0;
	while (parsed < num_packets) {
		u32 cur_pkt_addr = bs[0];
		bs[4] = 0x4000;
		s32 r = mpegParsePackHeader(bs, pack_info);
		if (r != 0) {
			if (r == 2 || r == 4) {
				*out_parsed += 1;
				return 0;
			}
			if (r != 1)
				return 0x80611001;
		} else {
			u32 pkt_node = rb[5] + rb[0] * 0x800 + ((cur_pkt_addr - bs[2]) >> 11) * 0x68;
			sceKernelMemset((void *)pkt_node, 0, 0x68);
			*(u32 *)(pkt_node + 0) = pack_info[0];
			*(u32 *)(pkt_node + 4) = pack_info[1];
			*(u32 *)(pkt_node + 8) = pack_info[2];
			*(u32 *)(pkt_node + 12) = 0;
			s32 sub_idx = 0;
			while ((s32)bs[4] > 0) {
				r = mpegParsePesHeader(bs, pes_info);
				if (r < 0)
					return r;
				if (r == 2 || r == 4) {
					*out_parsed += 1;
					return 0;
				}
				if (r == 1) {
					if (sub_idx >= 2)
						return 0x80611001;
					mpegLockLwMutex((void *)(mp + 0x66c));
					u32 *slot = mpegFindStreamSlot(pes_info[0], mp, 1);
					if (slot != (u32 *)-1 && slot[2] != (u32)-1) {
						u32 entry = pkt_node + sub_idx * 0x2c + 0x10;
						sceKernelMemcpy((void *)entry, pes_info, 0x2c);
						((SceMpegStreamNode *)entry)->field24 = 1;
						((SceMpegStreamNode *)entry)->field28 = (u32)-1;
						if (slot[1] == (u32)-1) {
							slot[0] = entry;
							slot[1] = entry;
							slot[3] = 1;
						} else {
							((SceMpegStreamNode *)slot[1])->field28 = entry;
							slot[1] = entry;
							slot[3] += 1;
						}
						sub_idx++;
						*(u32 *)(pkt_node + 12) = sub_idx;
					}
					mpegUnlockLwMutex((void *)(mp + 0x66c));
				}
			}
		}
		u32 next_addr = cur_pkt_addr + 0x800;
		bs[0] = next_addr;
		if (next_addr == end_addr)
			bs[0] = bs[2];
		bs[1] = 0;
		parsed++;
		*out_parsed = parsed;
	}
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
static s32 mpegParsePesHeader(u32 *bs, u32 *pes_info)
{
	u32 start_ptr = bs[2] + (bs[1] >> 3);
	if (start_ptr >= bs[6])
		start_ptr -= bs[7];
	pes_info[9] = start_ptr;
	mpegBitstreamSkip(bs, 24);
	u32 stream_id = ((u8 *)bs)[3];
	mpegBitstreamSkip(bs, 8);
	pes_info[0] = stream_id << 24;
	u32 pes_len = ((u16 *)bs)[1];
	mpegBitstreamSkip(bs, 16);
	u32 sid = pes_info[0];
	pes_info[1] = pes_len;
	pes_info[4] = (u32)-1;
	pes_info[5] = (u32)-1;
	pes_info[3] = (u32)-1;
	pes_info[6] = (u32)-1;
	if (sid != 0xbc000000 && sid != 0xbe000000 && sid != 0xbf000000 &&
	    sid != 0xf0000000 && sid != 0xf1000000 && sid != 0xff000000 &&
	    sid != 0xf2000000 && sid != 0xf8000000) {
		mpegBitstreamSkip(bs, 2);
		u32 scrambling = bs[0] >> 30;
		mpegBitstreamSkip(bs, 2);
		pes_info[2] = scrambling;
		mpegBitstreamSkip(bs, 4);
		u32 pts_dts_flags = bs[0] >> 30;
		mpegBitstreamSkip(bs, 2);
		u32 escr_flag = bs[0] >> 31;
		mpegBitstreamSkip(bs, 1);
		u32 rate_dsm_copy_crc = bs[0] >> 28;
		mpegBitstreamSkip(bs, 4);
		u32 ext_flag = bs[0] >> 31;
		mpegBitstreamSkip(bs, 1);
		u32 hdr_data_len = ((u8 *)bs)[3];
		mpegBitstreamSkip(bs, 8);
		u32 hdr_start_bit = bs[1];
		if ((pts_dts_flags & 2) != 0) {
			mpegBitstreamSkip(bs, 4);
			u32 p32 = bs[0];
			mpegBitstreamSkip(bs, 3);
			mpegBitstreamSkip(bs, 1);
			u32 p29 = bs[0];
			mpegBitstreamSkip(bs, 15);
			mpegBitstreamSkip(bs, 1);
			u32 p14 = bs[0];
			mpegBitstreamSkip(bs, 15);
			mpegBitstreamSkip(bs, 1);
			pes_info[4] = ((p32 >> 29) << 30) | ((p29 >> 17) << 15) | (p14 >> 17);
			pes_info[3] = p32 >> 31;
		}
		if (pts_dts_flags == 3) {
			mpegBitstreamSkip(bs, 4);
			u32 d32 = bs[0];
			mpegBitstreamSkip(bs, 3);
			mpegBitstreamSkip(bs, 1);
			u32 d29 = bs[0];
			mpegBitstreamSkip(bs, 15);
			mpegBitstreamSkip(bs, 1);
			u32 d14 = bs[0];
			mpegBitstreamSkip(bs, 15);
			mpegBitstreamSkip(bs, 1);
			pes_info[6] = ((d32 >> 29) << 30) | ((d29 >> 17) << 15) | (d14 >> 17);
			pes_info[5] = d32 >> 31;
		}
		if (escr_flag == 1)
			mpegBitstreamSkip(bs, 48);
		if (rate_dsm_copy_crc != 0)
			mpegBitstreamSkip(bs, g_pes_ext_bits[rate_dsm_copy_crc]);
		if (ext_flag == 1) {
			u32 pes_priv_flag = bs[0] >> 31;
			mpegBitstreamSkip(bs, 1);
			u32 pack_hdr_flag = bs[0] >> 31;
			mpegBitstreamSkip(bs, 1);
			u32 seq_cnt_flag = bs[0] >> 31;
			mpegBitstreamSkip(bs, 1);
			u32 std_buf_flag = bs[0] >> 31;
			mpegBitstreamSkip(bs, 1);
			mpegBitstreamSkip(bs, 3);
			u32 ext2_flag = bs[0] >> 31;
			mpegBitstreamSkip(bs, 1);
			if (pes_priv_flag == 1) {
				mpegBitstreamSkip(bs, 48);
				mpegBitstreamSkip(bs, 48);
				mpegBitstreamSkip(bs, 32);
			}
			if (pack_hdr_flag == 1)
				return 0x80618007;
			if (seq_cnt_flag == 1)
				mpegBitstreamSkip(bs, 16);
			if (std_buf_flag == 1)
				mpegBitstreamSkip(bs, 16);
			if (ext2_flag == 1) {
				mpegBitstreamSkip(bs, 1);
				u32 ext2_len = bs[0] >> 25;
				mpegBitstreamSkip(bs, 7);
				for (u32 i = 0; i < ext2_len; i++)
					mpegBitstreamSkip(bs, 8);
			}
		}
		u32 rem_hdr_bytes = hdr_data_len - ((bs[1] - hdr_start_bit) >> 3);
		if (rem_hdr_bytes != 0) {
			u32 new_bit = bs[1] + (rem_hdr_bytes << 3);
			u32 new_ptr = bs[2] + (new_bit >> 3);
			bs[0] = 0;
			bs[4] = 0;
			bs[1] = new_bit;
			if (new_ptr < bs[6])
				bs[3] = new_ptr;
			else
				bs[3] = new_ptr - bs[7];
			mpegBitstreamSkip(bs, 0);
		}
		u32 s1 = pes_info[1] - hdr_data_len;
		u32 data_len = s1 - 3;
		pes_info[8] = data_len;
		u32 data_ptr = bs[2] + (bs[1] >> 3);
		if (data_ptr >= bs[6])
			data_ptr -= bs[7];
		pes_info[7] = data_ptr;
		if (pes_info[0] == 0xbd000000) {
			u32 sub_hdr = bs[0] >> 8;
			mpegBitstreamSkip(bs, 24);
			pes_info[0] |= sub_hdr;
			data_len = s1 - 6;
		}
		if (data_len != 0) {
			u32 new_bit = bs[1] + (data_len << 3);
			u32 new_ptr = bs[2] + (new_bit >> 3);
			bs[0] = 0;
			bs[4] = 0;
			bs[1] = new_bit;
			if (new_ptr < bs[6])
				bs[3] = new_ptr;
			else
				bs[3] = new_ptr - bs[7];
			mpegBitstreamSkip(bs, 0);
		}
		return 1;
	} else if (sid == 0xbc000000 || sid == 0xbf000000 || sid == 0xf0000000 ||
	           sid == 0xf1000000 || sid == 0xff000000 || sid == 0xf2000000 || sid == 0xf8000000) {
		u32 data_len = pes_info[1];
		if (sid == 0xbf000000) {
			u32 data_ptr = bs[2] + (bs[1] >> 3);
			if (data_ptr >= bs[6])
				data_ptr -= bs[7];
			pes_info[8] = data_len;
			pes_info[7] = data_ptr;
			u32 sub_hdr = bs[0] >> 8;
			mpegBitstreamSkip(bs, 24);
			pes_info[0] |= sub_hdr;
			data_len -= 3;
		}
		if (data_len != 0) {
			u32 new_bit = bs[1] + (data_len << 3);
			u32 new_ptr = bs[2] + (new_bit >> 3);
			bs[0] = 0;
			bs[4] = 0;
			bs[1] = new_bit;
			if (new_ptr < bs[6])
				bs[3] = new_ptr;
			else
				bs[3] = new_ptr - bs[7];
			mpegBitstreamSkip(bs, 0);
		}
		return 1;
	} else if (sid == 0xbe000000) {
		u32 data_len = pes_info[1];
		if (data_len != 0) {
			u32 new_bit = bs[1] + (data_len << 3);
			u32 new_ptr = bs[2] + (new_bit >> 3);
			bs[0] = 0;
			bs[4] = 0;
			bs[1] = new_bit;
			if (new_ptr < bs[6])
				bs[3] = new_ptr;
			else
				bs[3] = new_ptr - bs[7];
			mpegBitstreamSkip(bs, 0);
		}
		return 1;
	}
	return 1;
}

/* 44: 0x2fdc (108B) */
static s32 mpegRingbufferPutInternal(u32 *rb, s32 num_packets, s32 available)
{
	s32 n = mpegRingbufferWritePackets(rb, num_packets, available);
	if (n <= 0)
		return n;
	s32 parsed = 0;
	s32 ret = mpegRingbufferPutDemux((u32 *)rb[8], rb, n, &parsed);
	if (ret < 0)
		return ret;
	mpegRingbufferConsume(rb, parsed);
	return parsed;
}

/* 45: 0x3048 (440B) */
static s32 mpegRingbufferWritePackets(u32 *rb, s32 num_packets, s32 available)
{
	if (rb == 0)
		return 0x806101fe;
	if (num_packets <= 0 || available <= 0)
		return 0;
	u32 *mp_ptr = (u32 *)rb[8];
	if (mpegValidateHandle(mp_ptr) != 0)
		return 0x806101fe;
	u32 mp = *mp_ptr;
	if (((SceMpegCtx *)mp)->magic_flags != rb[9])
		return 0x806101fe;
	s32 req = (num_packets < available) ? num_packets : available;
	s32 total = rb[0];
	s32 w_idx = rb[2];
	s32 chunk1 = req;
	s32 chunk2 = 0;
	if (w_idx + req > total) {
		chunk1 = total - w_idx;
		chunk2 = req - chunk1;
	}
	typedef s32 (*rb_cb_t)(void *data, s32 num_packets, void *arg);
	rb_cb_t cb = (rb_cb_t)rb[6];
	s32 got1 = cb((void *)(rb[5] + w_idx * 0x800), chunk1, (void *)rb[7]);
	if (got1 < 0)
		return got1;
	if (got1 > chunk1)
		return 0x806101fe;
	if (got1 < chunk1)
		return got1;
	s32 got2 = 0;
	if (chunk2 > 0) {
		got2 = cb((void *)rb[5], chunk2, (void *)rb[7]);
		if (got2 < 0)
			return got2;
		if (got2 > chunk2)
			return 0x806101fe;
	}
	return got1 + got2;
}

/* 46: 0x3200 (328B) */
static s32 mpegRingbufferResetInternal(u32 *rb)
{
	if (rb == 0)
		return 0x806101fe;
	u32 *mp_ptr = (u32 *)rb[8];
	if (mpegValidateHandle(mp_ptr) != 0)
		return 0x806101fe;
	u32 mp = *mp_ptr;
	if (((SceMpegCtx *)mp)->magic_flags != rb[9])
		return 0x806101fe;
	mpegLockLwMutex((void *)rb[10]);
	mpegLockLwMutex((void *)(mp + 0x66c));
	s32 total = rb[0];
	s32 min_pkt = total;
	u32 pkt_tbl = rb[5] + total * 0x800;
	for (s32 i = 0; i < total; i++) {
		u32 pkt = pkt_tbl + i * 0x68;
		for (u32 j = 0; j < *(u32 *)(pkt + 12); j++) {
			u32 entry = pkt + j * 0x2c + 0x10;
			if (((SceMpegStreamNode *)entry)->field24 == 1)
				mpegRingbufferCheckAddr(rb, &min_pkt, i * 0x800 + rb[5]);
		}
	}
	for (s32 i = 0; i < 0x70; i++) {
		u32 *slot = (u32 *)(((SceMpegCtx *)mp)->ringbuf_ptr + (i << 4));
		if (slot[2] != (u32)-1) {
			u32 *stream = (u32 *)slot[2];
			if (stream[12] != 0)
				mpegRingbufferCheckAddr(rb, &min_pkt, stream[8]);
		}
	}
	mpegUnlockLwMutex((void *)(mp + 0x66c));
	mpegUnlockLwMutex((void *)rb[10]);
	return min_pkt - mpegRingbufferAvailableSizeInternal(rb);
}

/* 47: 0x3348 (24B) */
static s32 mpegRingbufferQueryMemSizeInternal(s32 packets)
{
	return (packets << 11) + packets * 0x68;
}

/* 48: 0x3360 (28B) */
static s32 mpegRingbufferQueryPackNumInternal(u32 mem_size)
{
	return mem_size / 0x868u;
}

/* 49: 0x337c (220B) */
static s32 mpegRingbufferConstructInternal(u32 *rb, s32 packets, void *data, s32 size, void *cb, void *arg)
{
	if (size < mpegRingbufferQueryMemSizeInternal(packets))
		return 0x80610022;
	rb[0] = packets;
	rb[1] = 0;
	rb[2] = 0;
	rb[3] = 0;
	rb[4] = 0;
	rb[5] = (u32)data;
	rb[6] = (u32)cb;
	rb[7] = (u32)arg;
	rb[8] = 0;
	rb[9] = 0;
	rb[10] = 0;
	u32 pkt_tbl = (u32)data + packets * 0x800;
	for (s32 i = 0; i < packets; i++) {
		u32 pkt = pkt_tbl + i * 0x68;
		*(u32 *)(pkt + 12) = 0;
		for (s32 j = 0; j < 2; j++) {
			u32 entry = pkt + j * 0x2c + 0x10;
			((SceMpegStreamNode *)entry)->field24 = 0;
			((SceMpegStreamNode *)entry)->field28 = (u32)-1;
		}
	}
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
	if (rb == 0)
		return 0x806101fe;
	u32 *mp_ptr = (u32 *)rb[8];
	if (mpegValidateHandle(mp_ptr) != 0)
		return 0x806101fe;
	u32 mp = *mp_ptr;
	if (((SceMpegCtx *)mp)->magic_flags != rb[9])
		return 0x806101fe;
	mpegLockLwMutex((void *)rb[10]);
	s32 count = rb[3];
	mpegUnlockLwMutex((void *)rb[10]);
	return count;
}

/* 52: 0x34d0 (196B) */
static s32 mpegRingbufferConsume(u32 *rb, s32 consumed)
{
	if (rb == 0)
		return 0x806101fe;
	u32 *mp_ptr = (u32 *)rb[8];
	if (mpegValidateHandle(mp_ptr) != 0)
		return 0x806101fe;
	u32 mp = *mp_ptr;
	if (((SceMpegCtx *)mp)->magic_flags != rb[9])
		return 0x806101fe;
	mpegLockLwMutex((void *)rb[10]);
	s32 total = rb[0];
	s32 r_idx = rb[1];
	s32 w_idx = rb[2];
	if (r_idx + consumed < total) {
		r_idx += consumed;
		w_idx += consumed;
	} else {
		r_idx = consumed - (total - r_idx);
		w_idx = r_idx;
	}
	rb[1] = r_idx;
	rb[2] = w_idx;
	rb[4] += consumed;
	mpegUnlockLwMutex((void *)rb[10]);
	return 0;
}

/* 53: 0x3594 (172B) */
static s32 mpegRingbufferCheckAddr(u32 *rb, s32 *min_pkt, u32 addr)
{
	if (rb == 0)
		return 0x806101fe;
	u32 *mp_ptr = (u32 *)rb[8];
	if (mpegValidateHandle(mp_ptr) != 0)
		return 0x806101fe;
	u32 mp = *mp_ptr;
	if (((SceMpegCtx *)mp)->magic_flags != rb[9])
		return 0x806101fe;
	s32 total = rb[0];
	s32 idx = (s32)(addr - rb[5]) >> 11;
	s32 w_idx = rb[2];
	s32 dist = (w_idx <= idx) ? (idx - w_idx) : (total - (w_idx - idx));
	if (dist < *min_pkt)
		*min_pkt = dist;
	return 0;
}

/* 54: 0x3644 (1080B) */
static s32 mpegGetAuInternal(u32 *mp_ptr, u32 *stream, s32 type, u32 *au)
{
	s32 ret = mpegValidateHandle(mp_ptr);
	if (ret != 0)
		return ret;
	u32 mp = *mp_ptr;
	u32 *rb = (*(u32 **)&((SceMpegCtx *)mp)->stream_list);
	u32 rb_start = rb[5];
	u32 rb_end = rb_start + rb[0] * 0x800;
	u32 max_au_sz = g_au_max_sizes[type];
	u32 ctx[7];
	ctx[0] = (u32)mpegGetActiveSlotPtr(mp_ptr);
	ctx[5] = au[4];
	if (ctx[5] != 0) {
		ctx[5] = mpegGetAvcEsBufPtr(mp_ptr, (s32)ctx[5]);
		if (type == 0)
			ctx[5] += 0x200;
	}
	au[5] = 0;
	ctx[6] = 0;
	if (stream[11] > 0) {
		ret = mpegEsAppendChunk(mp_ptr, ctx, (const void *)stream[3], stream[11], type, au);
		if (ret < 0)
			return ret;
		stream[11] = 0;
	}
	u32 rem_au = stream[16];
	u32 pes_node[11];
	s32 got = 0;
	s32 popped = 0;
	while ((s32)rem_au > 0) {
		if ((stream[12] & 1) == 0) {
			ret = mpegPeekStreamPackets((u32 *)stream[1], 0, 1, pes_node, &got);
			if (ret != 0)
				return ret;
			if (got == 0)
				return 0x80618001;
			stream[8] = pes_node[2];
			stream[12] |= 1;
			stream[14] = pes_node[3];
		}
		if (stream[14] <= rem_au) {
			u32 p_end = stream[8] + stream[14];
			if (p_end <= rb_end) {
				ret = mpegEsAppendChunk(mp_ptr, ctx, (const void *)stream[8], stream[14], type, au);
				if (ret < 0)
					return ret;
			} else {
				u32 len1 = rb_end - stream[8];
				ret = mpegEsAppendChunk(mp_ptr, ctx, (const void *)stream[8], len1, type, au);
				if (ret < 0)
					return ret;
				ret = mpegEsAppendChunk(mp_ptr, ctx, (const void *)rb_start, p_end - rb_end, type, au);
				if (ret < 0)
					return ret;
			}
			rem_au -= stream[14];
			stream[14] = 0;
			stream[12] &= ~1u;
			ret = mpegPopStreamPackets((u32 *)stream[1], 1, &popped);
			if (ret != 0)
				return ret;
		} else {
			u32 p_end = stream[8] + rem_au;
			if (p_end <= rb_end) {
				ret = mpegEsAppendChunk(mp_ptr, ctx, (const void *)stream[8], rem_au, type, au);
				if (ret < 0)
					return ret;
				stream[8] = p_end;
				if (p_end == rb_end)
					stream[8] = rb_start;
			} else {
				u32 len1 = rb_end - stream[8];
				ret = mpegEsAppendChunk(mp_ptr, ctx, (const void *)stream[8], len1, type, au);
				if (ret < 0)
					return ret;
				u32 len2 = p_end - rb_end;
				ret = mpegEsAppendChunk(mp_ptr, ctx, (const void *)rb_start, len2, type, au);
				if (ret < 0)
					return ret;
				stream[8] = rb_start + len2;
			}
			stream[14] -= rem_au;
			rem_au = 0;
		}
	}
	if ((s32)stream[13] > 0 && ctx[5] != 0) {
		s32 pad_rem = stream[13];
		while (pad_rem > 0) {
			s32 chunk = (pad_rem < 0x80) ? pad_rem : 0x80;
			if (au[5] + chunk > max_au_sz) {
				stream[16] = 0;
				return 0x80611001;
			}
			ret = mpegEsAppendChunk(mp_ptr, ctx, g_zero_pad, chunk, type, au);
			if (ret < 0)
				return ret;
			pad_rem -= chunk;
		}
		stream[13] = 0;
	}
	if (ctx[5] != 0 && (s32)ctx[6] > 0) {
		ret = mpegEsFinalizeChain(mp_ptr, ctx, au);
		if (ret < 0)
			return ret;
	}
	stream[16] = 0;
	return 0;
}

/* 55: 0x3a7c (556B) */
static s32 mpegGetAtracAuInternal(u32 *mp_ptr, u32 *stream, u32 *au, u32 *attr)
{
	s32 ret = mpegValidateHandle(mp_ptr);
	if (ret != 0)
		return ret;
	u32 mp = *mp_ptr;
	if (attr != 0)
		*attr = 0;
	mpegLockLwMutex((void *)(mp + 0x66c));
	u32 *slot = mpegFindStreamSlot(stream[0], mp, 1);
	if (slot == (u32 *)-1 || slot[2] == (u32)-1) {
		mpegUnlockLwMutex((void *)(mp + 0x66c));
		return 0x806101fe;
	}
	s32 type = 0, ch = 0;
	ret = mpegStreamIdToType(&type, &ch, stream[0]);
	if (ret != 0 || type != 2) {
		mpegUnlockLwMutex((void *)(mp + 0x66c));
		return 0x806101fe;
	}
	u32 pes_node[11];
	s32 got = 0;
	ret = mpegPeekStreamPackets((u32 *)stream[1], 0, 1, pes_node, &got);
	if (ret != 0) {
		mpegUnlockLwMutex((void *)(mp + 0x66c));
		return ret;
	}
	if (got == 0) {
		au[0] = (u32)-1;
		au[1] = (u32)-1;
		au[2] = (u32)-1;
		au[3] = (u32)-1;
		au[5] = 0;
		mpegUnlockLwMutex((void *)(mp + 0x66c));
		return 0x80618001;
	}
	if ((stream[12] & 1) == 0) {
		au[0] = pes_node[4];
		au[1] = pes_node[5];
		au[2] = (u32)-1;
		au[3] = (u32)-1;
		if (pes_node[4] == (u32)-1 && pes_node[5] == (u32)-1) {
			au[0] = 0;
			au[1] = 0;
		}
		stream[9] = pes_node[0];
	} else {
		au[0] = 0;
		au[1] = 0;
		au[2] = (u32)-1;
		au[3] = (u32)-1;
	}
	if ((stream[12] & 4) != 0)
		au[4] = 0;
	if (attr != 0)
		*attr = (stream[9] >> 8) & 0xff;
	stream[16] = 0x140;
	ret = mpegGetAuInternal(mp_ptr, stream, 2, au);
	mpegUnlockLwMutex((void *)(mp + 0x66c));
	return ret;
}

/* 56: 0x3ca8 (1016B) */
static s32 mpegGetAvcAuInternal(u32 *mp_ptr, u32 *stream, s32 type, u32 *au, void *header_out)
{
	s32 ret = mpegValidateHandle(mp_ptr);
	if (ret != 0)
		return ret;
	u32 mp = *mp_ptr;
	mpegLockLwMutex((void *)(mp + 0x66c));
	u32 *rb = (*(u32 **)&((SceMpegCtx *)mp)->stream_list);
	u32 *slot = mpegFindStreamSlot(stream[0], mp, 1);
	if (slot == (u32 *)-1 || slot[2] == (u32)-1) {
		mpegUnlockLwMutex((void *)(mp + 0x66c));
		return 0x806101fe;
	}
	s32 st_type = 0, st_ch = 0;
	ret = mpegStreamIdToType(&st_type, &st_ch, stream[0]);
	if (ret != 0 || st_type != type) {
		mpegUnlockLwMutex((void *)(mp + 0x66c));
		return 0x806101fe;
	}
	u32 rb_start = rb[5];
	u32 rb_end = rb_start + rb[0] * 0x800;
	s32 skip = 0;
	u32 pes_node[11];
	s32 got = 0;
	s32 popped = 0;
	u8 hdr_buf[16];
	while (1) {
		ret = mpegPeekStreamPackets((u32 *)stream[1], skip, 1, pes_node, &got);
		if (ret != 0) {
			mpegUnlockLwMutex((void *)(mp + 0x66c));
			return ret;
		}
		if (got == 0) {
			au[0] = (u32)-1;
			au[1] = (u32)-1;
			au[2] = (u32)-1;
			au[3] = (u32)-1;
			au[5] = 0;
			mpegUnlockLwMutex((void *)(mp + 0x66c));
			return 0x80618001;
		}
		if ((stream[12] & 1) == 0) {
			au[0] = pes_node[4];
			au[1] = pes_node[5];
			au[2] = pes_node[6];
			au[3] = pes_node[7];
			u32 p_cur = pes_node[2];
			u32 rem_pkt = pes_node[3];
			u32 p_after_off = p_cur + stream[5];
			if (p_after_off > rb_end)
				p_after_off = rb_start + (p_after_off - rb_end);
			u32 rem_after_off = rem_pkt - stream[5];
			stream[8] = p_after_off;
			stream[14] = rem_after_off;
			if (stream[4] != (u32)-1) {
				stream[15] = stream[4] & 0xffff;
				stream[10] = stream[4] >> 16;
			} else {
				stream[15] = pes_node[0] & 0xff;
				stream[10] = (pes_node[0] >> 8) & 0xff;
			}
			if (stream[15] == 0) {
				if (stream[10] <= 1) {
					ret = mpegPopStreamPackets((u32 *)stream[1], 1, &popped);
					if (ret != 0) {
						mpegUnlockLwMutex((void *)(mp + 0x66c));
						return ret;
					}
					skip = 0;
					continue;
				}
				u32 next_off = stream[10];
				if (next_off < rem_after_off) {
					stream[12] |= 1;
					stream[5] = next_off;
				} else {
					stream[5] = next_off - rem_after_off;
					skip = 1;
				}
				continue;
			}
		} else {
			au[0] = (u32)-1;
			au[1] = (u32)-1;
			au[2] = (u32)-1;
			au[3] = (u32)-1;
		}
		u32 p_au = stream[8];
		if (p_au + 4 <= rb_end) {
			sceKernelMemcpy(hdr_buf, (const void *)p_au, 4);
		} else {
			u32 len1 = rb_end - p_au;
			sceKernelMemcpy(hdr_buf, (const void *)p_au, len1);
			sceKernelMemcpy(hdr_buf + len1, (const void *)rb_start, 4 - len1);
		}
		if (type == 5) {
			u32 hdr_sz = psmfGetStreamSize(hdr_buf);
			if (p_au + hdr_sz <= rb_end) {
				sceKernelMemcpy(hdr_buf, (const void *)p_au, hdr_sz);
			} else {
				u32 len1 = rb_end - p_au;
				sceKernelMemcpy(hdr_buf, (const void *)p_au, len1);
				sceKernelMemcpy(hdr_buf + len1, (const void *)rb_start, hdr_sz - len1);
			}
			stream[8] = p_au + hdr_sz;
			if (stream[8] >= rb_end)
				stream[8] = rb_start + (stream[8] - rb_end);
			u32 au_sz = psmfGetStreamOffset(hdr_buf);
			stream[14] -= hdr_sz;
			stream[16] = au_sz;
			stream[13] = ((au_sz + 0x7ff) & ~0x7ffu) - au_sz + 0x800;
			if (header_out != 0) {
				u32 *ho = (u32 *)header_out;
				ho[0] = psmfGetStreamType(hdr_buf);
				ho[1] = psmfGetEpMapFlag(hdr_buf);
				u32 cnt = psmfGetStreamCount(hdr_buf);
				ho[2] = cnt;
				for (u32 i = 0; i < cnt; i++) {
					const u8 *ent = psmfGetStreamEntry(hdr_buf, i);
					sceKernelMemcpy(&ho[3 + i], ent, 4);
				}
			}
		} else {
			if (header_out != 0)
				sceKernelMemcpy(header_out, hdr_buf, 4);
			stream[8] = p_au + 4;
			if (stream[8] >= rb_end)
				stream[8] = rb_start + (stream[8] - rb_end);
			stream[14] -= 4;
			stream[16] = mpegReadBe32(hdr_buf);
		}
		break;
	}
	if ((stream[12] & 4) != 0)
		au[4] = 0;
	ret = mpegGetAuInternal(mp_ptr, stream, type, au);
	if (ret < 0) {
		mpegUnlockLwMutex((void *)(mp + 0x66c));
		return ret;
	}
	stream[15] -= 1;
	stream[10] = 0;
	if ((s32)stream[15] <= 0) {
		if ((stream[12] & 1) != 0) {
			stream[12] &= ~1u;
			ret = mpegPopStreamPackets((u32 *)stream[1], 1, &popped);
			if (ret != 0) {
				mpegUnlockLwMutex((void *)(mp + 0x66c));
				return ret;
			}
		}
		stream[5] = 2;
		stream[4] = (u32)-1;
	} else {
		stream[4] = stream[15] & 0xffff;
	}
	mpegUnlockLwMutex((void *)(mp + 0x66c));
	return 0;
}

/* 57: 0x40a0 (764B) */
static s32 mpegGetUserdataAuInternal(u32 *mp_ptr, u32 *stream, u32 *au, void **es_out)
{
	s32 ret = mpegValidateHandle(mp_ptr);
	if (ret != 0)
		return ret;
	u32 mp = *mp_ptr;
	if (es_out != 0)
		*es_out = 0;
	mpegLockLwMutex((void *)(mp + 0x66c));
	u32 *rb = (*(u32 **)&((SceMpegCtx *)mp)->stream_list);
	u32 *slot = mpegFindStreamSlot(stream[0], mp, 1);
	if (slot == (u32 *)-1 || slot[2] == (u32)-1) {
		mpegUnlockLwMutex((void *)(mp + 0x66c));
		return 0x806101fe;
	}
	s32 type = 0, ch = 0;
	ret = mpegStreamIdToType(&type, &ch, stream[0]);
	if (ret != 0 || type != 1) {
		mpegUnlockLwMutex((void *)(mp + 0x66c));
		return 0x806101fe;
	}
	u32 rb_start = rb[5];
	u32 rb_end = rb_start + rb[0] * 0x800;
	s32 skip = 0;
	u32 pes_node[11];
	s32 got = 0;
	s32 popped = 0;
	while (1) {
		ret = mpegPeekStreamPackets((u32 *)stream[1], skip, 1, pes_node, &got);
		if (ret != 0) {
			mpegUnlockLwMutex((void *)(mp + 0x66c));
			return ret;
		}
		if (got == 0) {
			au[0] = (u32)-1;
			au[1] = (u32)-1;
			au[2] = (u32)-1;
			au[3] = (u32)-1;
			au[5] = 0;
			mpegUnlockLwMutex((void *)(mp + 0x66c));
			return 0x80618001;
		}
		if ((stream[12] & 1) == 0) {
			au[0] = pes_node[4];
			au[1] = pes_node[5];
			au[2] = (u32)-1;
			au[3] = (u32)-1;
			if (pes_node[4] == (u32)-1 && pes_node[5] == (u32)-1) {
				au[0] = 0;
				au[1] = 0;
			}
			u32 p_cur = pes_node[2];
			u32 rem_pkt = pes_node[3];
			u32 p_after_off = p_cur + stream[5];
			if (p_after_off > rb_end)
				p_after_off = rb_start + (p_after_off - rb_end);
			u32 rem_after_off = rem_pkt - stream[5];
			stream[8] = p_after_off;
			stream[14] = rem_after_off;
			if (stream[4] != 0x7fff) {
				stream[15] = stream[4] & 0xff;
				stream[10] = (stream[4] >> 8) & 0xff;
			} else {
				stream[15] = (pes_node[0] >> 8) & 0xff;
				stream[10] = pes_node[0] & 0xff;
			}
			if (stream[15] == 0) {
				if (stream[10] <= 1) {
					ret = mpegPopStreamPackets((u32 *)stream[1], 1, &popped);
					if (ret != 0) {
						mpegUnlockLwMutex((void *)(mp + 0x66c));
						return ret;
					}
					skip = 0;
					continue;
				}
				u32 next_off = stream[10];
				if (next_off < rem_after_off) {
					stream[12] |= 1;
					stream[5] = next_off;
				} else {
					stream[5] = next_off - rem_after_off;
					skip = 1;
				}
				continue;
			}
		} else {
			au[0] = (u32)-1;
			au[1] = (u32)-1;
			au[2] = (u32)-1;
			au[3] = (u32)-1;
		}
		break;
	}
	u8 hdr_buf[8];
	u32 p_au = stream[8];
	if (p_au + 8 <= rb_end) {
		sceKernelMemcpy(hdr_buf, (const void *)p_au, 8);
	} else {
		u32 len1 = rb_end - p_au;
		sceKernelMemcpy(hdr_buf, (const void *)p_au, len1);
		sceKernelMemcpy(hdr_buf + len1, (const void *)rb_start, 8 - len1);
	}
	stream[8] = p_au + 8;
	if (stream[8] >= rb_end)
		stream[8] = rb_start + (stream[8] - rb_end);
	stream[14] -= 8;
	stream[16] = ((mpegReadBe16(hdr_buf + 2) & 0x3ffu) << 3) + 8;
	if ((stream[12] & 4) != 0)
		au[4] = 0;
	ret = mpegGetAuInternal(mp_ptr, stream, 1, au);
	if (ret < 0) {
		mpegUnlockLwMutex((void *)(mp + 0x66c));
		return ret;
	}
	stream[15] -= 1;
	stream[10] = 0;
	if ((s32)stream[15] <= 0) {
		if ((stream[12] & 1) != 0) {
			stream[12] &= ~1u;
			ret = mpegPopStreamPackets((u32 *)stream[1], 1, &popped);
			if (ret != 0) {
				mpegUnlockLwMutex((void *)(mp + 0x66c));
				return ret;
			}
		}
		stream[5] = 4;
		stream[4] = 0x7fff;
	} else {
		stream[4] = stream[15] & 0xff;
	}
	if (es_out != 0)
		*es_out = (void *)au[4];
	mpegUnlockLwMutex((void *)(mp + 0x66c));
	return 0;
}

/* 58: 0x439c (260B) */
static s32 mpegRingbufferSetReadPos(u32 *rb, u32 p1, u32 p2)
{
	if (rb == 0)
		return 0x806101fe;
	u32 *mp_ptr = (u32 *)rb[8];
	if (mpegValidateHandle(mp_ptr) != 0)
		return 0x806101fe;
	u32 mp = *mp_ptr;
	if (((SceMpegCtx *)mp)->magic_flags != rb[9])
		return 0x806101fe;
	u32 rb_start = rb[5];
	u32 rb_end = rb_start + rb[0] * 0x800;
	if (p1 < rb_start || p1 >= rb_end)
		return 0x806101fe;
	if (p2 < rb_start || p2 >= rb_end)
		return 0x806101fe;
	if (p1 == p2)
		return 0;
	u32 w_ptr = rb_start + rb[2] * 0x800;
	if (p1 >= w_ptr) {
		if (p2 >= w_ptr && p2 < p1)
			return 1;
		return -1;
	} else {
		if (p2 >= w_ptr || p1 < p2)
			return -1;
		return 1;
	}
}

/* 59: 0x44a0 (872B) */
static s32 mpegGetPcmAuInternal(u32 *mp_ptr, u32 *stream, u32 *au, u32 *attr)
{
	s32 ret = mpegValidateHandle(mp_ptr);
	if (ret != 0)
		return ret;
	u32 mp = *mp_ptr;
	if (attr != 0)
		*attr = 0;
	mpegLockLwMutex((void *)(mp + 0x66c));
	u32 *rb = (*(u32 **)&((SceMpegCtx *)mp)->stream_list);
	u32 *slot = mpegFindStreamSlot(stream[0], mp, 1);
	if (slot == (u32 *)-1 || slot[2] == (u32)-1) {
		mpegUnlockLwMutex((void *)(mp + 0x66c));
		return 0x806101fe;
	}
	s32 type = 0, ch = 0;
	ret = mpegStreamIdToType(&type, &ch, stream[0]);
	if (ret != 0 || type != 0) {
		mpegUnlockLwMutex((void *)(mp + 0x66c));
		return 0x806101fe;
	}
	if ((stream[12] & 2) != 0) {
		ret = mpegChangeGetAvcAuModeInternal(mp_ptr, stream);
		if (ret != 0) {
			au[0] = (u32)-1;
			au[1] = (u32)-1;
			au[2] = (u32)-1;
			au[3] = (u32)-1;
			au[5] = 0;
			mpegUnlockLwMutex((void *)(mp + 0x66c));
			return ret;
		}
	}
	u32 pes_node[11];
	s32 got = 0;
	ret = mpegPeekStreamPackets((u32 *)stream[1], 0, 1, pes_node, &got);
	if (ret != 0) {
		mpegUnlockLwMutex((void *)(mp + 0x66c));
		return ret;
	}
	if (got == 0) {
		au[0] = (u32)-1;
		au[1] = (u32)-1;
		au[2] = (u32)-1;
		au[3] = (u32)-1;
		au[5] = 0;
		mpegUnlockLwMutex((void *)(mp + 0x66c));
		return 0x80618001;
	}
	u32 first_pkt_ptr = pes_node[2];
	stream[16] = pes_node[3];
	s32 skip = 1;
	while (1) {
		ret = mpegPeekStreamPackets((u32 *)stream[1], skip, 1, pes_node, &got);
		if (ret != 0) {
			mpegUnlockLwMutex((void *)(mp + 0x66c));
			return ret;
		}
		if (got == 0) {
			au[0] = (u32)-1;
			au[1] = (u32)-1;
			au[2] = (u32)-1;
			au[3] = (u32)-1;
			au[5] = 0;
			mpegUnlockLwMutex((void *)(mp + 0x66c));
			return 0x80618001;
		}
		if (pes_node[4] != (u32)-1 || pes_node[5] != (u32)-1)
			break;
		stream[16] += pes_node[3];
		skip++;
	}
	ret = mpegPeekStreamPackets((u32 *)stream[1], 0, 1, pes_node, &got);
	if (ret != 0) {
		mpegUnlockLwMutex((void *)(mp + 0x66c));
		return ret;
	}
	au[0] = pes_node[4];
	au[1] = pes_node[5];
	au[2] = pes_node[6];
	au[3] = pes_node[7];
	if (pes_node[6] == (u32)-1 && pes_node[7] == (u32)-1) {
		au[2] = pes_node[4];
		au[3] = pes_node[5];
	}
	s32 popped = 0;
	while (1) {
		ret = mpegPeekStreamPackets((u32 *)stream[2], 0, 1, pes_node, &got);
		if (ret != 0) {
			mpegUnlockLwMutex((void *)(mp + 0x66c));
			return ret;
		}
		if (got == 0)
			break;
		s32 cmp = mpegRingbufferSetReadPos(rb, pes_node[2], first_pkt_ptr);
		if (cmp >= 0)
			break;
		u32 p_priv = pes_node[2];
		u32 priv_hdr = mpegReadBe32((const u8 *)p_priv);
		if ((u8)(priv_hdr & 0xff) == (u8)(stream[0] & 0xff) && ((priv_hdr >> 8) & 0xff) == 1) {
			u32 priv_len = ((priv_hdr >> 16) & 0xff) << 4;
			if (priv_len > stream[7]) {
				mpegUnlockLwMutex((void *)(mp + 0x66c));
				return 0x80611001;
			}
			sceKernelMemcpy((void *)stream[3], (const void *)(p_priv + 4), priv_len);
			stream[11] = priv_len;
			ret = mpegPopStreamPackets((u32 *)stream[2], 1, &popped);
			if (ret != 0) {
				mpegUnlockLwMutex((void *)(mp + 0x66c));
				return ret;
			}
			break;
		}
		ret = mpegPopStreamPackets((u32 *)stream[2], 1, &popped);
		if (ret != 0) {
			mpegUnlockLwMutex((void *)(mp + 0x66c));
			return ret;
		}
	}
	if ((stream[12] & 4) != 0)
		au[4] = 0;
	ret = mpegGetAuInternal(mp_ptr, stream, 0, au);
	mpegUnlockLwMutex((void *)(mp + 0x66c));
	return ret;
}

/* 60: 0x4808 (224B) */
static s32 mpegAvcCopyEsData(u32 *mp_ptr, const void *src, s32 len, u32 *au)
{
	s32 ret = mpegValidateHandle(mp_ptr);
	if (ret != 0)
		return ret;
	if (src == 0 || au == 0 || len <= 0)
		return 0x806101fe;
	u32 mp = *mp_ptr;
	mpegLockLwMutex((void *)(mp + 0x66c));
	u32 ctx[7];
	ctx[0] = (u32)mpegGetActiveSlotPtr(mp_ptr);
	ctx[5] = mpegGetAvcEsBufPtr(mp_ptr, (s32)au[4]) + 0x200;
	ctx[6] = 0;
	ret = mpegEsAppendChunk(mp_ptr, ctx, src, len, 0, au);
	if (ret >= 0 && (s32)ctx[6] > 0)
		ret = mpegEsFinalizeChain(mp_ptr, ctx, au);
	mpegUnlockLwMutex((void *)(mp + 0x66c));
	return ret;
}

/* 61: 0x48e8 (668B) */
static s32 mpegAvcAssembleNal(u32 *mp_ptr, u32 *nal, u32 *au)
{
	s32 ret = mpegValidateHandle(mp_ptr);
	if (ret != 0)
		return ret;
	if (nal == 0 || au == 0)
		return 0x806101fe;
	u32 mp = *mp_ptr;
	mpegLockLwMutex((void *)(mp + 0x66c));
	if (nal[4] == 0 || (s32)nal[5] <= 0) {
		mpegUnlockLwMutex((void *)(mp + 0x66c));
		return 0x806101fe;
	}
	if (nal[6] != 0) {
		if (nal[0] == 0 || (s32)nal[1] <= 0 || nal[2] == 0 || (s32)nal[3] <= 0) {
			mpegUnlockLwMutex((void *)(mp + 0x66c));
			return 0x806101fe;
		}
	}
	u32 ctx[7];
	ctx[0] = (u32)mpegGetActiveSlotPtr(mp_ptr);
	ctx[5] = mpegGetAvcEsBufPtr(mp_ptr, (s32)au[4]) + 0x200;
	au[5] = 0;
	ctx[6] = 0;
	if (nal[6] != 0) {
		s32 sps_rem = (s32)nal[1];
		const u8 *sps_p = (const u8 *)nal[0];
		while (sps_rem > 0) {
			u32 n_len = mpegReadBe16(sps_p);
			sps_rem -= n_len + 2;
			if (sps_rem < 0) {
				mpegUnlockLwMutex((void *)(mp + 0x66c));
				return 0x806101fe;
			}
			ret = mpegEsAppendChunk(mp_ptr, ctx, g_nal_prefix, 4, 0, au);
			if (ret < 0) {
				mpegUnlockLwMutex((void *)(mp + 0x66c));
				return ret;
			}
			ret = mpegEsAppendChunk(mp_ptr, ctx, sps_p + 2, n_len, 0, au);
			if (ret < 0) {
				mpegUnlockLwMutex((void *)(mp + 0x66c));
				return ret;
			}
			sps_p += n_len + 2;
		}
		s32 pps_rem = (s32)nal[3];
		const u8 *pps_p = (const u8 *)nal[2];
		while (pps_rem > 0) {
			u32 n_len = mpegReadBe16(pps_p);
			pps_rem -= n_len + 2;
			if (pps_rem < 0) {
				mpegUnlockLwMutex((void *)(mp + 0x66c));
				return 0x806101fe;
			}
			ret = mpegEsAppendChunk(mp_ptr, ctx, g_nal_prefix, 4, 0, au);
			if (ret < 0) {
				mpegUnlockLwMutex((void *)(mp + 0x66c));
				return ret;
			}
			ret = mpegEsAppendChunk(mp_ptr, ctx, pps_p + 2, n_len, 0, au);
			if (ret < 0) {
				mpegUnlockLwMutex((void *)(mp + 0x66c));
				return ret;
			}
			pps_p += n_len + 2;
		}
	}
	s32 nal_rem = (s32)nal[5];
	const u8 *nal_p = (const u8 *)nal[4];
	while (nal_rem > 0) {
		ret = mpegEsAppendChunk(mp_ptr, ctx, g_nal_prefix, 4, 0, au);
		if (ret < 0) {
			mpegUnlockLwMutex((void *)(mp + 0x66c));
			return ret;
		}
		u32 n_len = mpegReadBe32(nal_p);
		nal_rem -= n_len + 4;
		if (nal_rem < 0) {
			mpegUnlockLwMutex((void *)(mp + 0x66c));
			return 0x806101fe;
		}
		ret = mpegEsAppendChunk(mp_ptr, ctx, nal_p + 4, n_len, 0, au);
		if (ret < 0) {
			mpegUnlockLwMutex((void *)(mp + 0x66c));
			return ret;
		}
		nal_p += n_len + 4;
	}
	if ((s32)ctx[6] > 0) {
		ret = mpegEsFinalizeChain(mp_ptr, ctx, au);
		if (ret < 0) {
			mpegUnlockLwMutex((void *)(mp + 0x66c));
			return ret;
		}
	}
	mpegUnlockLwMutex((void *)(mp + 0x66c));
	return 0;
}

/* 62: 0x4b84 (52B) */
static s32 mpegEsNodeInit(u32 *node, u32 a1, u32 a2, u32 a3, u32 t0)
{
	node[0] = a1;
	node[1] = a2;
	node[2] = t0;
	node[3] = (a3 & 0xfffu) | 0x314e0000u;
	return 0;
}

/* 63: 0x4bb8 (12B) */
static s32 mpegEsNodeSetNext(u32 *node, u32 next)
{
	node[2] = next;
	return 0;
}

/* 64: 0x4bc4 (36B) */
static s32 mpegGetAvcNalAuInternal(u32 *mp_ptr, u32 *stream, u32 *au, void *header_out)
{
	return mpegGetAvcAuInternal(mp_ptr, stream, 3, au, header_out);
}

/* 65: 0x4be8 (88B) */
static s32 mpegEsNodeFlushCache(u32 *node)
{
	for (s32 i = 0; i < 100 && node != 0; i++) {
		sceKernelDcacheWritebackInvalidateRange((const void *)node[0], node[3] & 0xfffu);
		node = (u32 *)node[2];
	}
	return 0;
}

/* 66: 0x4c40 (176B) */
static s32 mpegEsFinalizeChain(u32 *mp_ptr, u32 *ctx, u32 *au)
{
	s32 idx = (s32)ctx[6] - 1;
	mpegEsNodeSetNext((u32 *)(ctx[0] + (idx << 4)), 0);
	mpegEsNodeFlushCache((u32 *)ctx[0]);
	sceKernelDcacheWritebackInvalidateRange((const void *)ctx[0], 0x640);
	s32 ret = sceMpegBasePESpacketCopy((void *)ctx[0]);
	if (ret < 0) {
		au[5] = 0;
		return ret;
	}
	if (idx < 0x63)
		mpegEsNodeSetNext((u32 *)(ctx[0] + (idx << 4)), ctx[0] + ((idx + 1) << 4));
	mpegNoopHandle(mp_ptr);
	ctx[0] = (u32)mpegGetActiveSlotPtr(mp_ptr);
	return 0;
}

/* 67: 0x4cf4 (344B) */
static s32 mpegEsAppendChunk(u32 *mp_ptr, u32 *ctx, const void *src, s32 len, s32 type, u32 *au)
{
	if (src == 0)
		return 0x806101fe;
	if (len == 0)
		return 0;
	u32 max_sz = g_au_max_sizes[type];
	if (ctx[5] != 0) {
		if (au[5] + len > max_sz) {
			au[5] = 0;
			return 0x80611001;
		}
		ctx[1] = (u32)src;
		ctx[2] = len;
		while ((s32)ctx[2] > 0) {
			if (ctx[6] == 100) {
				s32 ret = mpegEsFinalizeChain(mp_ptr, ctx, au);
				if (ret < 0)
					return ret;
				ctx[6] = 0;
			}
			s32 added = mpegEsResetTable(ctx);
			if (added < 0) {
				au[5] = 0;
				return added;
			}
			ctx[6] += added;
		}
	}
	au[5] += len;
	return 0;
}

/* 68: 0x4e4c (204B) */
static s32 mpegEsResetTable(u32 *ctx)
{
	u32 tbl = ctx[0];
	s32 rem = (s32)ctx[2];
	u32 dst = ctx[5];
	u32 src = ctx[1];
	s32 idx = (s32)ctx[6];
	s32 count = 0;
	if (rem > 0) {
		while (idx < 100) {
			s32 chunk = (rem >= 0xff0) ? 0xff0 : rem;
			ctx[4] = chunk;
			ctx[3] = dst;
			rem -= chunk;
			idx++;
			u32 node = tbl + ((idx - 1) << 4);
			count++;
			mpegEsNodeInit((u32 *)node, src, dst, chunk, node + 16);
			dst = ctx[3] + ctx[4];
			src += ctx[4];
			ctx[2] = rem;
			ctx[5] = dst;
			ctx[1] = src;
			if (rem <= 0)
				break;
		}
	}
	return count;
}

/* 69: 0x4f18 (156B) */
static s32 mpegMallocAvcEsBufInternal(u32 *mp_ptr)
{
	if (mpegValidateHandle(mp_ptr) != 0)
		return 0;
	u32 mp = *mp_ptr;
	mpegLockLwMutex((void *)(mp + 0x66c));
	s32 mode = ((SceMpegCtx *)mp)->avc_mode;
	if (mode == 2 || mode == 3) {
		mpegUnlockLwMutex((void *)(mp + 0x66c));
		return 0;
	}
	for (s32 i = 0; i < 2; i++) {
		if (*(s32 *)(mp + (i << 2) + 0x6b0) == 0) {
			*(s32 *)(mp + (i << 2) + 0x6b0) = 1;
			mpegUnlockLwMutex((void *)(mp + 0x66c));
			return i + 1;
		}
	}
	mpegUnlockLwMutex((void *)(mp + 0x66c));
	return 0;
}

/* 70: 0x4fb4 (140B) */
static s32 mpegFreeAvcEsBufInternal(u32 *mp_ptr, s32 buf_id)
{
	s32 ret = mpegValidateHandle(mp_ptr);
	if (ret != 0)
		return ret;
	if ((u32)(buf_id - 1) >= 2u)
		return 0x806101fe;
	u32 mp = *mp_ptr;
	mpegLockLwMutex((void *)(mp + 0x66c));
	s32 mode = ((SceMpegCtx *)mp)->avc_mode;
	if (mode == 2 || mode == 3) {
		mpegUnlockLwMutex((void *)(mp + 0x66c));
		return 0x806101fe;
	}
	*(u32 *)(mp + ((buf_id - 1) << 2) + 0x6b0) = 0;
	mpegUnlockLwMutex((void *)(mp + 0x66c));
	return 0;
}

/* 71: 0x5040 (60B) */
static u32 mpegGetAvcEsBufPtr(u32 *mp_ptr, s32 buf_id)
{
	if (mpegValidateHandle(mp_ptr) != 0)
		return 0;
	u32 mp = *mp_ptr;
	if ((u32)(buf_id - 1) < 2u)
		return *(u32 *)(((SceMpegCtx *)mp)->avc_work_ptr + 0x10) + (buf_id - 1) * 0x18600;
	return (u32)buf_id;
}

/* 72: 0x507c (80B) */
static s32 mpegQueryAtracEsSizeInternal(u32 *mp_ptr, u32 *es_size, u32 *out_size)
{
	s32 ret = mpegValidateHandle(mp_ptr);
	if (ret != 0) {
		*es_size = 0;
		*out_size = 0;
		return ret;
	}
	*es_size = 0x840;
	*out_size = 0x2000;
	return 0;
}

/* 73: 0x50cc (76B) */
static s32 mpegQueryPcmEsSizeInternal(u32 *mp_ptr, u32 *es_size, u32 *out_size)
{
	s32 ret = mpegValidateHandle(mp_ptr);
	if (ret != 0) {
		*es_size = 0;
		*out_size = 0;
		return ret;
	}
	*es_size = 0x140;
	*out_size = 0x140;
	return 0;
}

/* 74: 0x5118 (76B) */
static s32 mpegQueryUserdataEsSizeInternal(u32 *mp_ptr, u32 *es_size, u32 *out_size)
{
	s32 ret = mpegValidateHandle(mp_ptr);
	if (ret != 0) {
		*es_size = 0;
		*out_size = 0;
		return ret;
	}
	*es_size = 0xa0000;
	*out_size = 0xa0000;
	return 0;
}

/* 75: 0x5164 (40B) */
static u32 psmfGetStreamSize(const u8 *p)
{
	return ((mpegReadBe16(p + 2) & 0x3ffu) << 3) + 8;
}

/* 76: 0x518c (12B) */
static u32 psmfGetStreamType(const u8 *p)
{
	return p[0] & 0xfu;
}

/* 77: 0x5198 (32B) */
static u32 psmfGetEpMapFlag(const u8 *p)
{
	return mpegReadBe24(p + 1) >> 23;
}

/* 78: 0x51b8 (32B) */
static u32 psmfGetStreamOffset(const u8 *p)
{
	return mpegReadBe24(p + 1) & 0x1fffffu;
}

/* 79: 0x51d8 (8B) */
static u32 psmfGetStreamCount(const u8 *p)
{
	return p[3];
}

/* 80: 0x51e0 (32B) */
static const u8 *psmfGetStreamEntry(const u8 *p, s32 idx)
{
	if (idx < (s32)p[3])
		return p + (idx << 2) + 4;
	return 0;
}

/* 81: 0x5204 (8B) */
static const u8 *psmfGetPayloadPtr(const u8 *p)
{
	return p + 0x0e;
}

/* 82: 0x520c (32B) */
static void *mpegGetActiveSlotPtr(u32 *mp_ptr)
{
	u32 mp = *mp_ptr;
	return (void *)(mp + ((SceMpegCtx *)mp)->active_slot * 0x644 + 0x28);
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
	s32 ret = mpegValidateHandle(mp_ptr);
	if (ret != 0)
		return ret;
	u32 mp = *mp_ptr;
	mpegLockLwMutex((void *)(mp + 0x66c));
	u32 *rb = (*(u32 **)&((SceMpegCtx *)mp)->stream_list);
	u32 *slot = mpegFindStreamSlot(stream[0], mp, 1);
	if (slot == (u32 *)-1 || slot[2] == (u32)-1) {
		mpegUnlockLwMutex((void *)(mp + 0x66c));
		return 0x806101fe;
	}
	s32 type = 0, ch = 0;
	ret = mpegStreamIdToType(&type, &ch, stream[0]);
	if (ret != 0 || type != 0) {
		mpegUnlockLwMutex((void *)(mp + 0x66c));
		return 0x806101fe;
	}
	u32 pes_node[11];
	s32 got = 0;
	s32 popped = 0;
	ret = mpegPeekStreamPackets((u32 *)stream[2], 0, 1, pes_node, &got);
	if (ret != 0) {
		mpegUnlockLwMutex((void *)(mp + 0x66c));
		return ret;
	}
	if (got == 0) {
		ret = mpegPeekStreamPackets((u32 *)stream[1], 0, 1, pes_node, &got);
		if (ret != 0) {
			mpegUnlockLwMutex((void *)(mp + 0x66c));
			return ret;
		}
		if (got != 0) {
			ret = mpegPopStreamPackets((u32 *)stream[1], *(u32 *)(stream[1] + 12) - 1, &popped);
			if (ret != 0) {
				mpegUnlockLwMutex((void *)(mp + 0x66c));
				return ret;
			}
		}
		mpegUnlockLwMutex((void *)(mp + 0x66c));
		return 0x80618001;
	}
	u32 priv_ptr = pes_node[2];
	while (1) {
		ret = mpegPeekStreamPackets((u32 *)stream[1], 0, 1, pes_node, &got);
		if (ret != 0) {
			mpegUnlockLwMutex((void *)(mp + 0x66c));
			return ret;
		}
		if (got == 0) {
			mpegUnlockLwMutex((void *)(mp + 0x66c));
			return 0x80618001;
		}
		if (mpegRingbufferSetReadPos(rb, pes_node[2], priv_ptr) > 0)
			break;
		ret = mpegPopStreamPackets((u32 *)stream[1], 1, &popped);
		if (ret != 0) {
			mpegUnlockLwMutex((void *)(mp + 0x66c));
			return ret;
		}
	}
	mpegUnlockLwMutex((void *)(mp + 0x66c));
	return 0;
}

/* 85: 0x53c4 (224B) */
static s32 mpegChangeGetAuModeInternal(u32 *mp_ptr, u32 *stream, s32 mode)
{
	s32 ret = mpegValidateHandle(mp_ptr);
	if (ret != 0)
		return ret;
	u32 mp = *mp_ptr;
	mpegLockLwMutex((void *)(mp + 0x66c));
	u32 *slot = mpegFindStreamSlot(stream[0], mp, 1);
	if (slot == (u32 *)-1) {
		mpegUnlockLwMutex((void *)(mp + 0x66c));
		return 0x806101fe;
	}
	s32 type = 0, ch = 0;
	ret = mpegStreamIdToType(&type, &ch, stream[0]);
	if (ret != 0) {
		mpegUnlockLwMutex((void *)(mp + 0x66c));
		return ret;
	}
	if (type == 0) {
		if (mode == 0) {
			stream[12] &= ~2u;
			mpegUnlockLwMutex((void *)(mp + 0x66c));
			return 0;
		}
		if (mode == 2) {
			stream[12] |= 2u;
			mpegUnlockLwMutex((void *)(mp + 0x66c));
			return 0;
		}
	}
	mpegUnlockLwMutex((void *)(mp + 0x66c));
	return 0x806101fe;
}

/* 86: 0x54a4 (212B) */
static s32 mpegSetStreamModeInternal(u32 *mp_ptr, u32 *stream, s32 mode)
{
	s32 ret = mpegValidateHandle(mp_ptr);
	if (ret != 0)
		return ret;
	u32 mp = *mp_ptr;
	mpegLockLwMutex((void *)(mp + 0x66c));
	u32 *slot = mpegFindStreamSlot(stream[0], mp, 1);
	if (slot == (u32 *)-1) {
		mpegUnlockLwMutex((void *)(mp + 0x66c));
		return 0x806101fe;
	}
	s32 type = 0, ch = 0;
	ret = mpegStreamIdToType(&type, &ch, stream[0]);
	if (ret != 0) {
		mpegUnlockLwMutex((void *)(mp + 0x66c));
		return ret;
	}
	if (mode == 0) {
		stream[12] &= ~4u;
		mpegUnlockLwMutex((void *)(mp + 0x66c));
		return 0;
	}
	if (mode == 1) {
		stream[12] |= 4u;
		mpegUnlockLwMutex((void *)(mp + 0x66c));
		return 0;
	}
	mpegUnlockLwMutex((void *)(mp + 0x66c));
	return 0x806101fe;
}

/* 87: 0x5578 (104B) - sceMpegQueryStreamOffset (0x21ff80e4) */
s32 sceMpegQueryStreamOffset(u32 *mp_ptr, const u8 *header, u32 *out_offset)
{
	if (mpegCheckThreadStack() < 0x3f0)
		return 0x80618008;
	if (mp_ptr == 0 || header == 0 || out_offset == 0)
		return 0x806101fe;
	return mpegQueryStreamOffsetInternal(mp_ptr, header, out_offset);
}

/* 88: 0x55e0 (88B) - sceMpegQueryStreamSize (0x611e9e11) */
s32 sceMpegQueryStreamSize(const u8 *header, u32 *out_size)
{
	if (mpegCheckThreadStack() < 0x3f0)
		return 0x80618008;
	if (header == 0 || out_size == 0)
		return 0x806101fe;
	return mpegQueryStreamSizeInternal(header, out_size);
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
	if (mpegCheckThreadStack() < 0x3f0)
		return 0x80618008;
	if (mode != 0)
		return 0x806101fe;
	return mpegQueryMemSizeInternal(0);
}

/* 92: 0x5700 (72B) - sceMpeg_42C679F6 (0x42c679f6) */
s32 sceMpeg_42C679F6(s32 mode)
{
	if (mpegCheckThreadStack() < 0x3f0)
		return 0x80618008;
	if ((u32)mode >= 6u)
		return 0x806101fe;
	return mpegQueryMemSizeInternal(mode);
}

/* 93: 0x5748 (176B) - sceMpegCreate (0xd8c5f121) */
s32 sceMpegCreate(u32 *mp_ptr, void *data, s32 size, u32 *rb, s32 frameWidth, s32 mode, u32 ddrtop)
{
	if (mpegCheckThreadStack() < 0x3e0)
		return 0x80618008;
	if (mp_ptr == 0 || data == 0 || size <= 0)
		return 0x806101fe;
	if (rb != 0 && rb[0] == 0)
		return 0x806101fe;
	if ((frameWidth & 63) != 0 || frameWidth <= 0 || frameWidth > 0x800)
		return 0x806101fe;
	if (mode != 0 || ddrtop != 0)
		return 0x806101fe;
	return mpegCreateInternal(mp_ptr, data, size, rb, frameWidth, 0, 0);
}

/* 94: 0x57f8 (168B) - sceMpeg_75E21135 (0x75e21135) */
s32 sceMpeg_75E21135(u32 *mp_ptr, void *data, s32 size, u32 *rb, s32 frameWidth, s32 mode, u32 ddrtop)
{
	if (mpegCheckThreadStack() < 0x3e0)
		return 0x80618008;
	if (mp_ptr == 0 || data == 0 || size <= 0)
		return 0x806101fe;
	if (rb != 0 && rb[0] == 0)
		return 0x806101fe;
	if ((frameWidth & 63) != 0 || frameWidth <= 0 || frameWidth > 0x800)
		return 0x806101fe;
	return mpegCreateInternal(mp_ptr, data, size, rb, frameWidth, mode, ddrtop);
}

/* 95: 0x58a0 (72B) - sceMpegDelete (0x606a4649) */
s32 sceMpegDelete(u32 *mp_ptr)
{
	if (mpegCheckThreadStack() < 0x3f0)
		return 0x80618008;
	if (mp_ptr == 0)
		return 0x806101fe;
	return mpegDeleteInternal(mp_ptr);
}

/* 96: 0x58e8 (100B) - sceMpegRegistStream (0x42560f23) */
u32 *sceMpegRegistStream(u32 *mp_ptr, s32 type, s32 ch)
{
	if (mpegCheckThreadStack() < 0x3f0)
		return 0;
	if (mp_ptr == 0 || (u32)type >= 6u || ch < 0)
		return 0;
	return mpegRegistStreamInternal(mp_ptr, type, ch);
}

/* 97: 0x594c (88B) - sceMpegUnRegistStream (0x591a4aa2) */
s32 sceMpegUnRegistStream(u32 *mp_ptr, u32 *stream)
{
	if (mpegCheckThreadStack() < 0x3f0)
		return 0x80618008;
	if (mp_ptr == 0 || stream == 0)
		return 0x806101fe;
	return mpegUnRegistStreamInternal(mp_ptr, stream);
}

/* 98: 0x59a4 (68B) - sceMpegMallocAvcEsBuf (0xa780cf7e) */
s32 sceMpegMallocAvcEsBuf(u32 *mp_ptr)
{
	if (mpegCheckThreadStack() < 0x3f0)
		return 0;
	if (mp_ptr == 0)
		return 0;
	return mpegMallocAvcEsBufInternal(mp_ptr);
}

/* 99: 0x59e8 (88B) - sceMpegFreeAvcEsBuf (0xceb870b1) */
s32 sceMpegFreeAvcEsBuf(u32 *mp_ptr, s32 buf_id)
{
	if (mpegCheckThreadStack() < 0x3f0)
		return 0x80618008;
	if (mp_ptr == 0 || buf_id == 0)
		return 0x806101fe;
	return mpegFreeAvcEsBufInternal(mp_ptr, buf_id);
}

/* 100: 0x5a40 (104B) - sceMpegQueryAtracEsSize (0xf8dcb679) */
s32 sceMpegQueryAtracEsSize(u32 *mp_ptr, u32 *es_size, u32 *out_size)
{
	if (mpegCheckThreadStack() < 0x3f0)
		return 0x80618008;
	if (mp_ptr == 0 || es_size == 0 || out_size == 0)
		return 0x806101fe;
	return mpegQueryAtracEsSizeInternal(mp_ptr, es_size, out_size);
}

/* 101: 0x5aa8 (104B) - sceMpeg_C02CF6B5 (0xc02cf6b5) */
s32 sceMpeg_C02CF6B5(u32 *mp_ptr, u32 *es_size, u32 *out_size)
{
	if (mpegCheckThreadStack() < 0x3f0)
		return 0x80618008;
	if (mp_ptr == 0 || es_size == 0 || out_size == 0)
		return 0x806101fe;
	return mpegQueryPcmEsSizeInternal(mp_ptr, es_size, out_size);
}

/* 102: 0x5b10 (104B) - sceMpegInitAu (0x167afd9e) */
s32 sceMpegInitAu(u32 *mp_ptr, u32 buf, u32 *au)
{
	if (mpegCheckThreadStack() < 0x3f0)
		return 0x80618008;
	if (mp_ptr == 0 || buf == 0 || au == 0)
		return 0x806101fe;
	return mpegInitAuInternal(mp_ptr, buf, au);
}

/* 103: 0x5b78 (104B) - sceMpeg_234586AE (0x234586ae) */
s32 sceMpeg_234586AE(u32 *mp_ptr, u32 *stream, s32 mode)
{
	if (mpegCheckThreadStack() < 0x3f0)
		return 0x80618008;
	if (mp_ptr == 0 || stream == 0 || mode < 0)
		return 0x806101fe;
	return mpegChangeGetAuModeInternal(mp_ptr, stream, mode);
}

/* 104: 0x5be0 (104B) - sceMpeg_9DCFB7EA (0x9dcfb7ea) */
s32 sceMpeg_9DCFB7EA(u32 *mp_ptr, u32 *stream, s32 mode)
{
	if (mpegCheckThreadStack() < 0x3f0)
		return 0x80618008;
	if (mp_ptr == 0 || stream == 0 || mode < 0)
		return 0x806101fe;
	return mpegSetStreamModeInternal(mp_ptr, stream, mode);
}

/* 105: 0x5c48 (88B) - sceMpeg_3C37A7A6 (0x3c37a7a6) */
s32 sceMpeg_3C37A7A6(u32 *mp_ptr, u32 *stream)
{
	if (mpegCheckThreadStack() < 0x3f0)
		return 0x80618008;
	if (mp_ptr == 0 || stream == 0)
		return 0x806101fe;
	return mpegChangeGetAvcAuModeInternal(mp_ptr, stream);
}

/* 106: 0x5ca0 (120B) - sceMpegGetAvcAu (0xfe246728) */
s32 sceMpegGetAvcAu(u32 *mp_ptr, u32 *stream, u32 *au, u32 *attr)
{
	if (mpegCheckThreadStack() < 0x3e0)
		return 0x80618008;
	if (mp_ptr == 0 || stream == 0 || au == 0)
		return 0x806101fe;
	return mpegGetPcmAuInternal(mp_ptr, stream, au, attr);
}

/* 107: 0x5d18 (120B) - sceMpeg_8C1E027D (0x8c1e027d) */
s32 sceMpeg_8C1E027D(u32 *mp_ptr, u32 *stream, u32 *au, u32 *attr)
{
	if (mpegCheckThreadStack() < 0x3e0)
		return 0x80618008;
	if (mp_ptr == 0 || stream == 0 || au == 0)
		return 0x806101fe;
	return mpegGetAtracAuInternal(mp_ptr, stream, au, attr);
}

/* 108: 0x5d90 (120B) - sceMpegGetAtracAu (0xe1ce83a7) */
s32 sceMpegGetAtracAu(u32 *mp_ptr, u32 *stream, u32 *au, void **es_out)
{
	if (mpegCheckThreadStack() < 0x3e0)
		return 0x80618008;
	if (mp_ptr == 0 || stream == 0 || au == 0)
		return 0x806101fe;
	return mpegGetUserdataAuInternal(mp_ptr, stream, au, es_out);
}

/* 109: 0x5e08 (88B) - sceMpeg_500F0429 (0x500f0429) */
s32 sceMpeg_500F0429(u32 *mp_ptr, u32 *stream)
{
	if (mpegCheckThreadStack() < 0x3f0)
		return 0x80618008;
	if (mp_ptr == 0 || stream == 0)
		return 0x806101fe;
	return mpegFlushStreamInternal(mp_ptr, stream);
}

/* 110: 0x5e60 (72B) - sceMpegFlushAllStream (0x707b7629) */
s32 sceMpegFlushAllStream(u32 *mp_ptr)
{
	if (mpegCheckThreadStack() < 0x3f0)
		return 0x80618008;
	if (mp_ptr == 0)
		return 0x806101fe;
	return mpegFlushAllStreamInternal(mp_ptr);
}

/* 111: 0x5ea8 (136B) - sceMpegAvcDecode (0x0e3c2e9d) */
s32 sceMpegAvcDecode(u32 *mp_ptr, u32 *au, s32 frameWidth, u32 *buf_list, u32 *num_pic)
{
	if (mpegCheckThreadStack() < 0x3e0)
		return 0x80618008;
	if (mp_ptr == 0 || au == 0 || buf_list == 0 || num_pic == 0)
		return 0x806101fe;
	if ((frameWidth & 63) != 0 || frameWidth <= 0 || frameWidth > 0x800)
		return 0x806101fe;
	return mpegAvcDecodeInternal(mp_ptr, au, frameWidth, buf_list, num_pic);
}

/* 112: 0x5f30 (88B) - sceMpeg_0F6C18D7 (0x0f6c18d7) */
s32 sceMpeg_0F6C18D7(u32 *mp_ptr, u32 *out)
{
	if (mpegCheckThreadStack() < 0x3f0)
		return 0x80618008;
	if (mp_ptr == 0 || out == 0)
		return 0x806101fe;
	return mpegAvcQueryYCbCrSizeDefault(mp_ptr, out);
}

/* 113: 0x5f88 (104B) - sceMpeg_AB0E9556 (0xab0e9556) */
s32 sceMpeg_AB0E9556(u32 *mp_ptr, s32 idx, u32 *out)
{
	if (mpegCheckThreadStack() < 0x3f0)
		return 0x80618008;
	if (mp_ptr == 0 || idx < 0 || out == 0)
		return 0x806101fe;
	return mpegAvcQueryYCbCrSizeInternal(mp_ptr, idx, out);
}

/* 114: 0x5ff0 (88B) - sceMpegAvcDecodeMode (0xa11c7026) */
s32 sceMpegAvcDecodeMode(u32 *mp_ptr, const s32 *mode)
{
	if (mpegCheckThreadStack() < 0x3f0)
		return 0x80618008;
	if (mp_ptr == 0 || mode == 0)
		return 0x806101fe;
	return mpegAvcDecodeModeInternal(mp_ptr, mode);
}

/* 115: 0x6048 (120B) - sceMpegAvcDecodeStop (0x740fccd1) */
s32 sceMpegAvcDecodeStop(u32 *mp_ptr, s32 frameWidth, u32 *buf_list, u32 *num_pic)
{
	if (mpegCheckThreadStack() < 0x3e0)
		return 0x80618008;
	if (mp_ptr == 0 || buf_list == 0 || num_pic == 0)
		return 0x806101fe;
	if ((frameWidth & 63) != 0 || frameWidth <= 0 || frameWidth > 0x800)
		return 0x806101fe;
	return mpegAvcDecodeStopInternal(mp_ptr, frameWidth, buf_list, num_pic);
}

/* 116: 0x60c0 (72B) - sceMpeg_4571CC64 (0x4571cc64) */
s32 sceMpeg_4571CC64(u32 *mp_ptr)
{
	if (mpegCheckThreadStack() < 0x3f0)
		return 0x80618008;
	if (mp_ptr == 0)
		return 0x806101fe;
	return mpegAvcFlushInternal(mp_ptr);
}

/* 117: 0x6108 (136B) - sceMpeg_211A057C (0x211a057c) */
s32 sceMpeg_211A057C(u32 *mp_ptr, s32 mode, s32 width, s32 height, s32 *out_size)
{
	if (mpegCheckThreadStack() < 0x3e0)
		return 0x80618008;
	if (mp_ptr == 0 || out_size == 0 || width < 0 || height < 0)
		return 0x806101fe;
	return mpegAvcQueryYCbCrMemSize(mp_ptr, mode, width, height, out_size);
}

/* 118: 0x6190 (136B) - sceMpeg_67179B1B (0x67179b1b) */
s32 sceMpeg_67179B1B(u32 *mp_ptr, s32 mode, s32 width, s32 height, u32 *buf)
{
	if (mpegCheckThreadStack() < 0x3e0)
		return 0x80618008;
	if (mp_ptr == 0 || buf == 0 || width < 0 || height < 0)
		return 0x806101fe;
	return mpegAvcInitYCbCrWithDims(mp_ptr, mode, width, height, buf);
}

/* 119: 0x6218 (120B) - sceMpeg_F0EB1125 (0xf0eb1125) */
s32 sceMpeg_F0EB1125(u32 *mp_ptr, u32 *au, u32 *buf_list, u32 *num_pic)
{
	if (mpegCheckThreadStack() < 0x3e0)
		return 0x80618008;
	if (mp_ptr == 0 || au == 0 || buf_list == 0 || num_pic == 0)
		return 0x806101fe;
	return mpegAvcDecodeYCbCrInternal(mp_ptr, au, buf_list, num_pic);
}

/* 120: 0x6290 (104B) - sceMpeg_F2930C9C (0xf2930c9c) */
s32 sceMpeg_F2930C9C(u32 *mp_ptr, u32 *buf_list, u32 *num_pic)
{
	if (mpegCheckThreadStack() < 0x3f0)
		return 0x80618008;
	if (mp_ptr == 0 || buf_list == 0 || num_pic == 0)
		return 0x806101fe;
	return mpegAvcDecodeStopYCbCrInternal(mp_ptr, buf_list, num_pic);
}

/* 121: 0x62f8 (136B) - sceMpeg_31BD0272 (0x31bd0272) */
s32 sceMpeg_31BD0272(u32 *mp_ptr, u32 *src, const u32 *range, s32 frameWidth, void *dst)
{
	if (mpegCheckThreadStack() < 0x3e0)
		return 0x80618008;
	if (mp_ptr == 0 || src == 0 || range == 0 || dst == 0)
		return 0x806101fe;
	if ((frameWidth & 63) != 0 || frameWidth <= 0 || frameWidth > 0x800)
		return 0x806101fe;
	return mpegAvcCscRangeInternal(mp_ptr, src, range, frameWidth, dst);
}

/* 122: 0x6380 (104B) - sceMpeg_0558B075 (0x0558b075) */
s32 sceMpeg_0558B075(u32 *mp_ptr, u32 *dst, u32 *src)
{
	if (mpegCheckThreadStack() < 0x3f0)
		return 0x80618008;
	if (mp_ptr == 0 || dst == 0 || src == 0)
		return 0x806101fe;
	return mpegAvcCopyYCbCrInternal(mp_ptr, dst, src);
}

/* 123: 0x63e8 (120B) - sceMpeg_F5E7EA31 (0xf5e7ea31) */
s32 sceMpeg_F5E7EA31(u32 *mp_ptr, void *dst_buf, u32 *src, s32 flags)
{
	if (mpegCheckThreadStack() < 0x3e0)
		return 0x80618008;
	if (mp_ptr == 0 || dst_buf == 0 || src == 0)
		return 0x806101fe;
	return mpegAvcCscInternal(mp_ptr, dst_buf, src, flags);
}

/* 124: 0x6460 (144B) - sceMpegAtracDecode (0x800c44df) */
s32 sceMpegAtracDecode(u32 *mp_ptr, u32 *au, void *buf, s32 init_flag)
{
	if (mpegCheckThreadStack() < 0x3e0)
		return 0x80618008;
	if (mp_ptr == 0 || au == 0)
		return 0x806101fe;
	if ((u32)init_flag >= 2u)
		return 0x806101fe;
	if (init_flag == 0 && buf == 0)
		return 0x806101fe;
	return mpegAtracDecodeInternal(mp_ptr, au, buf, init_flag);
}

/* 125: 0x64f0 (56B) - sceMpeg_AF26BB01 (0xaf26bb01) */
u32 *sceMpeg_AF26BB01(void)
{
	if (mpegCheckThreadStack() >= 0x3f0)
		return mpegGetAvcResPtr();
	return 0;
}

/* 126: 0x6528 (56B) - sceMpeg_63B9536A (0x63b9536a) */
s32 sceMpeg_63B9536A(void)
{
	if (mpegCheckThreadStack() >= 0x3f0)
		return mpegAvcResInit();
	return 0;
}

/* 127: 0x6560 (84B) - sceMpeg_FCBDB5AD (0xfcbdb5ad) */
s32 sceMpeg_FCBDB5AD(u32 flags)
{
	if (mpegCheckThreadStack() < 0x3f0)
		return 0x80618008;
	if (flags == 1)
		return mpegAvcResRelease();
	return 0x806101fe;
}

/* 128: 0x65b4 (60B) - sceMpeg_8160A2FE (0x8160a2fe) */
s32 sceMpeg_8160A2FE(void)
{
	if (mpegCheckThreadStack() < 0x3f0)
		return 0x80618008;
	return mpegAvcResTerm();
}

/* 129: 0x65f0 (104B) - sceMpeg_C45C99CC (0xc45c99cc) */
s32 sceMpeg_C45C99CC(u32 *mp_ptr, u32 *es_size, u32 *out_size)
{
	if (mpegCheckThreadStack() < 0x3f0)
		return 0x80618008;
	if (mp_ptr == 0 || es_size == 0 || out_size == 0)
		return 0x806101fe;
	return mpegQueryUserdataEsSizeInternal(mp_ptr, es_size, out_size);
}

/* 130: 0x6658 (120B) - sceMpeg_01977054 (0x01977054) */
s32 sceMpeg_01977054(u32 *mp_ptr, u32 *stream, u32 *au, void *header_out)
{
	if (mpegCheckThreadStack() < 0x3e0)
		return 0x80618008;
	if (mp_ptr == 0 || stream == 0 || au == 0)
		return 0x806101fe;
	return mpegGetAvcNalAuInternal(mp_ptr, stream, au, header_out);
}

/* 131: 0x66d0 (120B) - sceMpeg_921FCCCF (0x921fcccf) */
s32 sceMpeg_921FCCCF(u32 *mp_ptr, const void *src, s32 len, u32 *au)
{
	if (mpegCheckThreadStack() < 0x3e0)
		return 0x80618008;
	if (mp_ptr == 0 || src == 0 || len <= 0 || au == 0)
		return 0x806101fe;
	return mpegAvcCopyEsData(mp_ptr, src, len, au);
}

/* 132: 0x6748 (104B) - sceMpeg_11F95CF1 (0x11f95cf1) */
s32 sceMpeg_11F95CF1(u32 *mp_ptr, u32 *nal, u32 *au)
{
	if (mpegCheckThreadStack() < 0x3f0)
		return 0x80618008;
	if (mp_ptr == 0 || nal == 0 || au == 0)
		return 0x806101fe;
	return mpegAvcAssembleNal(mp_ptr, nal, au);
}

/* 133: 0x67b0 (72B) - sceMpegRingbufferQueryMemSize (0xd7a29f46) */
s32 sceMpegRingbufferQueryMemSize(s32 packets)
{
	if (mpegCheckThreadStack() < 0x3f0)
		return 0x80618008;
	if (packets <= 0)
		return 0x806101fe;
	return mpegRingbufferQueryMemSizeInternal(packets);
}

/* 134: 0x67f8 (72B) - sceMpeg_769BEBB6 (0x769bebb6) */
s32 sceMpeg_769BEBB6(s32 mem_size)
{
	if (mpegCheckThreadStack() < 0x3f0)
		return 0x80618008;
	if (mem_size <= 0)
		return 0x806101fe;
	return mpegRingbufferQueryPackNumInternal((u32)mem_size);
}

/* 135: 0x6840 (152B) - sceMpegRingbufferConstruct (0x37295ed8) */
s32 sceMpegRingbufferConstruct(u32 *rb, s32 packets, void *data, s32 size, void *cb, void *arg)
{
	if (mpegCheckThreadStack() < 0x3e0)
		return 0x80618008;
	if (rb == 0 || packets <= 0 || data == 0 || size <= 0 || cb == 0)
		return 0x806101fe;
	return mpegRingbufferConstructInternal(rb, packets, data, size, cb, arg);
}

/* 136: 0x68d8 (72B) - sceMpegRingbufferDestruct (0x13407f13) */
s32 sceMpegRingbufferDestruct(u32 *rb)
{
	if (mpegCheckThreadStack() < 0x3f0)
		return 0x80618008;
	if (rb == 0)
		return 0x806101fe;
	return mpegRingbufferDestructInternal(rb);
}

/* 137: 0x6920 (104B) - sceMpegRingbufferPut (0xb240a59e) */
s32 sceMpegRingbufferPut(u32 *rb, s32 num_packets, s32 available)
{
	if (mpegCheckThreadStack() < 0x3f0)
		return 0x80618008;
	if (rb == 0 || num_packets < 0 || available < 0)
		return 0x806101fe;
	return mpegRingbufferPutInternal(rb, num_packets, available);
}

/* 138: 0x6988 (72B) - sceMpegRingbufferAvailableSize (0xb5f6dc87) */
s32 sceMpegRingbufferAvailableSize(u32 *rb)
{
	if (mpegCheckThreadStack() < 0x3f0)
		return 0x80618008;
	if (rb == 0)
		return 0x806101fe;
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
	return _sceKernelCheckThreadStack();
}

/* 144: 0x6a6c (244B) */
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
			void *block = 0;
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
	u32 sp_val = 0;
	s32 ret = mpegVcodecOpen(&sp_val);
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
static s32 mpegVcodecSetup(u32 *vwork, void *mem1, void *mem2, void *mem3, u32 a4, u32 *a5, u32 a6, u32 a7, u32 a8, u32 a9, u32 a10)
{
	mpegLockLwMutex(g_avc_mutex);
	sceKernelMemset(mem1, 0, 0x28);
	sceKernelMemset(mem2, 0, 0xb0);
	sceKernelMemset(mem3, 0, 0x64);
	vwork[11] = (u32)mem2;
	vwork[4] = (u32)mem1;
	vwork[12] = (u32)mem3;
	vwork[15] = 4;
	vwork[14] = a4;
	s32 ret = sceVideocodecOpen(vwork, 0);
	if (ret == 0) {
		u32 edram = *a5;
		vwork[7] = a6;
		vwork[5] = edram;
		ret = sceVideocodecInit(vwork, 0);
		if (ret == 0) {
			ret = sceVideocodecGetVersion(vwork, 0);
			if (ret == 0) {
				vwork[16] = a7;
				vwork[17] = a8;
				vwork[18] = a9;
				vwork[19] = a10;
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
static s32 mpegVcodecSetOption(u32 *vwork, u32 a1)
{
	mpegLockLwMutex(g_avc_mutex);
	vwork[22] = a1;
	s32 ret = sceVideocodec_893B32B1(vwork, 0);
	mpegUnlockLwMutex(g_avc_mutex);
	return ret;
}

/* 157: 0x70d4 (140B) */
static s32 mpegAcodecInit(s32 mode)
{
	g_acodec_ptr = g_acodec_buf;
	sceKernelMemset(g_acodec_buf, 0, 0x6c);
	u32 sp_val = 0;
	s32 ret = mpegAcodecQueryMem(&sp_val, mode, &g_acodec_flag);
	if (ret == 0) {
		ret = sceAudiocodecGetEDRAM(g_acodec_ptr, 0x1000);
		if (ret != 0)
			g_acodec_ptr[3] = 0;
	}
	return ret;
}

/* 158: 0x7160 (188B) */
static s32 mpegAcodecQueryMem(u32 *out, s32 mode, u32 *flag_out)
{
	u32 is_m3 = (mode == 3) ? 1u : 0u;
	if (flag_out != 0)
		*flag_out = is_m3;
	g_acodec_ptr[12] = 1;
	sceKernelMemcpy(&g_acodec_ptr[10], &g_atrac_hdr[is_m3 << 1], 2);
	sceKernelMemcpy(&g_acodec_ptr[11], &g_atrac_zero[is_m3], 4);
	g_acodec_ptr[5] = 1;
	s32 ret = sceAudiocodecCheckNeedMem(g_acodec_ptr, 0x1000);
	*out = g_acodec_ptr[4];
	return ret;
}

/* 159: 0x721c (272B) */
static s32 mpegAcodecSetup(u32 *awork, u32 edram, u32 work_mem, const u8 *hdr2, const u8 *hdr4)
{
	mpegLockLwMutex(g_atrac_mutex);
	awork[3] = edram;
	awork[12] = 1;
	sceKernelMemcpy(&awork[10], hdr2, 2);
	sceKernelMemcpy(&awork[11], hdr4, 4);
	awork[5] = 1;
	s32 ret = sceAudiocodecCheckNeedMem(awork, 0x1000);
	if (ret != 0) {
		mpegUnlockLwMutex(g_atrac_mutex);
		return ret;
	}
	if (work_mem < awork[4]) {
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
static u32 mpegAcodecGetFlag(u32 *flag_out)
{
	if (flag_out != 0)
		*flag_out = g_acodec_flag;
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
static s32 mpegStrncmp(const char *s1, const char *s2, s32 n)
{
	if (n <= 0)
		return 0;
	do {
		u8 c1 = (u8)*s1++;
		u8 c2 = (u8)*s2++;
		n--;
		if (c1 != c2)
			return (c1 < c2) ? -1 : 1;
	} while (n > 0);
	return 0;
}

/* 165: 0x749c (128B) */
static s32 mpegStrcmp(const char *s1, const char *s2)
{
	if (s1 == 0 || s2 == 0) {
		if (s1 == s2)
			return 0;
		return (s1 != 0) ? 1 : -1;
	}
	s32 c1 = (s8)*(const u8 *)s1;
	s32 c2 = (s8)*s2++;
	if (c1 != c2)
		return c1 - (s8)s2[-1];
	while (1) {
		s1++;
		if (c1 == 0)
			return 0;
		c1 = (s8)*(const u8 *)s1;
		c2 = (s8)*s2++;
		if (c1 != c2)
			return c1 - (s8)s2[-1];
	}
}

/* 166: 0x751c (16B) */
static s32 mpegAbs(s32 x)
{
	s32 nx = -x;
	return (x > nx) ? x : nx;
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
static s32 mpegStreamIdToType(s32 *type_out, s32 *ch_out, u32 stream_id)
{
	for (s32 i = 0; i < 19; i++) {
		if (mpegMatchStreamId(i, stream_id) == 0) {
			switch (i) {
			case 0:
			case 9:
				*type_out = 0;
				*ch_out = (stream_id & ~g_stream_id_table[i][1]) >> 24;
				return 0;
			case 1:
				*type_out = 1;
				*ch_out = (stream_id & ~g_stream_id_table[i][1]) >> 16;
				return 0;
			case 2:
				*type_out = 2;
				*ch_out = (stream_id & ~g_stream_id_table[i][1]) >> 16;
				return 0;
			case 3:
				*type_out = 3;
				*ch_out = (stream_id & ~g_stream_id_table[i][1]) >> 16;
				return 0;
			case 4:
			case 14:
				*type_out = 4;
				*ch_out = (stream_id & ~g_stream_id_table[i][1]) >> 24;
				return 0;
			case 5:
			case 15:
				*type_out = 5;
				*ch_out = (stream_id & ~g_stream_id_table[i][1]) >> 16;
				return 0;
			case 6:
				*type_out = 6;
				*ch_out = (stream_id & ~g_stream_id_table[i][1]) >> 8;
				return 0;
			default:
				*type_out = i;
				*ch_out = 0;
				return 0;
			}
		}
	}
	return -1;
}

/* 169: 0x780c (40B) */
static s32 mpegMatchStreamId(s32 idx, u32 stream_id)
{
	return ((stream_id & g_stream_id_table[idx][1]) == g_stream_id_table[idx][0]) ? 0 : -1;
}

/* 170: 0x7838 (24B) */
static u16 mpegReadBe16(const u8 *p)
{
	return (u16)((p[0] << 8) | p[1]);
}

/* 171: 0x7850 (32B) */
static u32 mpegReadBe24(const u8 *p)
{
	return ((u32)p[0] << 16) | ((u32)p[1] << 8) | (u32)p[2];
}

/* 172: 0x7870 (44B) */
static u32 mpegReadBe32(const u8 *p)
{
	return ((u32)p[0] << 24) | ((u32)p[1] << 16) | ((u32)p[2] << 8) | (u32)p[3];
}

/* 173: 0x789c (168B) */
static void mpegReadBits64(const u8 *p, s32 bit_offset, u32 *hi_out, u32 *lo_out)
{
	const u8 *q = p + (bit_offset >> 3);
	u32 sh = bit_offset & 7;
	u32 w0 = mpegReadBe32(q);
	u32 w1 = mpegReadBe32(q + 4);
	u32 b8 = q[8];
	if (sh == 0) {
		*hi_out = w0;
		*lo_out = w1;
	} else {
		*hi_out = (w0 << sh) | (w1 >> (32 - sh));
		*lo_out = (w1 << sh) | (b8 >> (8 - sh));
	}
}
