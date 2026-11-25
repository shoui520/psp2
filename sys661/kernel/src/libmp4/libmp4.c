#include <common/types.h>

#define NOINLINE __attribute__((noinline))

/* Error codes */
#define SCE_MP4_ERROR_NOT_INITIALIZED   ((s32)0x80617001)
#define SCE_MP4_ERROR_ALREADY_INITED    ((s32)0x80617002)
#define SCE_MP4_ERROR_INVALID_VALUE     ((s32)0x80617003)
#define SCE_MP4_ERROR_NO_RESOURCE       ((s32)0x80617004)
#define SCE_MP4_ERROR_BAD_DATA          ((s32)0x80617005)
#define SCE_MP4_ERROR_END_OF_TRACK      ((s32)0x80617006)
#define SCE_MP4_ERROR_IO_FAILED         ((s32)0x80617007)
#define SCE_MP4_ERROR_NO_MEMORY         ((s32)0x80617008)
#define SCE_MP4_ERROR_INTERNAL          ((s32)0x80617009)
#define SCE_MP4_ERROR_BUFFER_EMPTY      ((s32)0x8061700a)

#define SCE_MP4_ERROR_AAC_INVALID_ID    ((s32)0x80617101)
#define SCE_MP4_ERROR_AAC_NOT_READY     ((s32)0x80617113)
#define SCE_MP4_ERROR_AAC_NO_SLOT       ((s32)0x80617121)
#define SCE_MP4_ERROR_AAC_DECODE_FAIL   ((s32)0x80617141)
#define SCE_MP4_ERROR_AAC_ALLOC_FAIL    ((s32)0x80617151)
#define SCE_MP4_ERROR_AAC_FREE_FAIL     ((s32)0x80617152)
#define SCE_MP4_ERROR_AAC_NOT_INIT      ((s32)0x80617153)
#define SCE_MP4_ERROR_AAC_ALREADY_INIT  ((s32)0x80617154)

#define SCE_MP4_ERROR_STACK_OVERFLOW    ((s32)0x8061800b)

#define MP4_MAGIC_LIB                   0x2034504DU /* "MP4 " */
#define MP4_MAGIC_HANDLE                0x5444344DU /* "M4DT" */
#define MP4_AAC_SLOT_SIZE               102336

/* Kernel / User Imports */
extern s32 sceKernelCpuSuspendIntr(void);
extern void sceKernelCpuResumeIntr(s32 state);
extern void *sceKernelMemset(void *dst, s32 c, u32 size);
extern void *sceKernelMemcpy(void *dst, const void *src, u32 size);
extern s32 _sceKernelCheckThreadStack(void);
extern s32 sceKernelLockLwMutex(void *work, s32 count, u32 *timeout);
extern s32 sceKernelUnlockLwMutex(void *work, s32 count);
extern s32 sceKernelGetModuleGPByAddress(const void *addr, u32 *gp);
extern s32 sceKernelCreateFpl(const char *name, s32 part, s32 attr, u32 block_size, u32 num_blocks, void *opt);
extern s32 sceKernelAllocateFpl(s32 uid, void **data, u32 *timeout);
extern s32 sceKernelTryAllocateFpl(s32 uid, void **data);
extern s32 sceKernelFreeFpl(s32 uid, void *data);
extern s32 sceKernelDeleteFpl(s32 uid);
extern s32 sceKernelCreateLwMutex(void *work, const char *name, u32 attr, s32 init_count, void *opt);
extern s32 sceKernelDeleteLwMutex(void *work);
extern s32 sceKernelDcacheInvalidateRangeForUser(const void *p, u32 size);
extern void sceKernelDcacheWritebackInvalidateRange(const void *p, u32 size);
extern void sceKernelDcacheWritebackRange(const void *p, u32 size);
extern s32 sceAudiocodecInit(u32 *work, s32 type);
extern s32 sceAudiocodecDecode(u32 *work, s32 type);
extern s32 sceMpegBasePESpacketCopy(void *pkt);

/* mp4msv Imports */
extern s32 mp4msv_089364A9(u32 track, u32 sample, s32 dir, s32 *out_sample, void *out_info);
extern s32 mp4msv_08F96BE2(u32 a0, s32 *out_val);
extern s32 mp4msv_0C06DACB(u32 ctx, u32 track_idx, u32 *out_track);
extern s32 mp4msv_0C0843B0(u32 track, u32 ts, s32 *out_sample, s32 mode, void *out1, void *out2);
extern s32 mp4msv_0D32271B(u32 ctx, void *out_brand);
extern s32 mp4msv_0DF0B9AD(u32 ctx, u32 idx, void *out_info);
extern s32 mp4msv_182FF541(u32 ctx, u32 *out_duration);
extern s32 mp4msv_21BD7274(u32 track, u32 sample, u32 *out_dts, s32 *out_flag);
extern s32 mp4msv_2B1B8D23(u32 desc, u32 idx, u16 *out_len);
extern s32 mp4msv_2BAF7C25(u32 desc, void *out_info);
extern s32 mp4msv_35144B9C(u32 desc, void *out_avcc, char *out_a, char *out_b);
extern s32 mp4msv_3C2183C7(u32 flag, const void *cfg);
extern s32 mp4msv_3D57BA52(u32 ctx, u32 *out_tracks);
extern s32 mp4msv_3D8D41A0(u32 ctx, void *io_ctx);
extern s32 mp4msv_3F6B6C59(u32 track, u32 *out_handler);
extern s32 mp4msv_46186C26(u32 ctx);
extern s32 mp4msv_4C677A7E(u32 ctx, u32 track, u32 idx, u32 *out_handle, void *out_type, void *out_lang, void *out_enc, u16 *out_len);
extern s32 mp4msv_54410730(u32 track, s32 *out_count);
extern s32 mp4msv_5EE48079(u32 track, u32 *out_samples);
extern s32 mp4msv_67AF9E0F(u32 ctx, void *hdr, u32 mode);
extern s32 mp4msv_6D619263(u32 track, u32 idx, u32 *out_desc);
extern s32 mp4msv_9632797F(u32 handle, u32 off, u32 flags, void *out_buf);
extern s32 mp4msv_9BB5E338(u32 track, u32 *out_timescale);
extern s32 mp4msv_9CA13D1A(u32 flag, const void *cbs);
extern s32 mp4msv_9D56A7CC(u32 ctx);
extern s32 mp4msv_A09878FC(u32 desc, u32 idx, u16 *out_len);
extern s32 mp4msv_A418509D(u32 desc, void *out_info);
extern s32 mp4msv_A55AF94C(u32 track, void *out_info);
extern s32 mp4msv_A824645B(u32 val, u32 src_scale, u32 dst_scale, u32 *out_val);
extern s32 mp4msv_A9FD62E2(u32 track, u32 *out_count);
extern s32 mp4msv_AD3AF34E(u32 ctx, u32 idx, void *out_info);
extern s32 mp4msv_BD23D34A(u32 ctx, u32 tag, void *out_val);
extern s32 mp4msv_BE558D75(u32 desc, u32 idx, u32 a2, u32 a3, void *out_buf);
extern s32 mp4msv_CFF25FAC(u32 ctx, void *out_hdr);
extern s32 mp4msv_E042AB3C(u32 ctx, u32 *out_timescale);
extern s32 mp4msv_E5C33D61(u32 track, u32 sample, u32 flags, void *out_entry);
extern s32 mp4msv_E641D779(u32 desc, u32 idx, u32 a2, u32 a3, void *out_buf);
extern s32 mp4msv_EE62F96D(u32 track, u32 idx, s32 *out_dur, s32 *out_time, s32 *out_rate);
extern s32 mp4msv_EF6D76BC(u32 ctx, u32 track, s32 *out_count);
extern s32 mp4msv_F595F917(u32 *out_ctx);
extern s32 mp4msv_FF9E6F58(u32 track, u32 *out_dur, s32 flags);

/* Globals */
static const u32 g_mp4HeapSizes[2] = { 0x10000U, 0x40000U };
static const u32 g_annexBStartCode = 0x01000000U;

static s32 g_aacFplId = -1;
static u32 g_mp4LibMagic;
static u32 g_mp4LibVersion;
static u32 *g_mp4HeapHandle;
static u32 g_mp4HeapSize;
static u32 g_mp4MutexFlags;
static u32 g_mp4MainMutex[8];
static u32 g_mp4AacMutex[8];

static void *(*g_mp4CopyFn)(void *, const void *, u32);
static u32 g_pesLliCount;
static u32 g_pesLliBuf[400];

static void *g_aacPoolBase;
static s32 g_aacSlotCount;
static s32 g_heapInstanceCount;

/* Forward declarations */
static NOINLINE s32 mp4ValidateHandle(u32 **mp4);
static NOINLINE s32 mp4CreateInternal(u32 **mp4, const u32 *cbs, u32 buf, s32 buf_size);
static NOINLINE s32 mp4DeleteInternal(u32 **mp4);
static NOINLINE s32 mp4GetMovieInfoInternal(u32 **mp4, u32 *out_info);
static NOINLINE s32 mp4SampleBufConstructInternal(u32 **mp4, u32 *track, u32 buf, s32 buf_size, s32 max_samples, s32 avg_size, s32 max_size, s32 cache_size);
static NOINLINE s32 mp4SampleBufAvailableInternal(u32 **mp4, u32 *track, s32 *out_samples, s32 *out_bytes);
static NOINLINE s32 mp4GetSampleNumWithTsInternal(u32 **mp4, u32 *track, const u32 *ts);
static NOINLINE s32 mp4SearchSyncSampleInternal(u32 **mp4, u32 *track, s32 dir, s32 sample);
static NOINLINE s32 mp4GetEditListInternal(u32 **mp4, u32 *track, u32 *out_list, s32 max_entries);
static NOINLINE s32 mp4GetAvcTrackInfoInternal(u32 **mp4, u32 *track, u32 *out_info);
static NOINLINE s32 mp4GetAacTrackInfoInternal(u32 **mp4, u32 *track, u32 *out_info);
static NOINLINE s32 mp4RegistTrackInternal(u32 **mp4, u32 type, s32 idx, const u32 *cbs, u32 *track);
static NOINLINE s32 mp4SampleBufPutInternal(u32 **mp4, u32 *track);
static NOINLINE s32 mp4GetAvcAuInternal(u32 **mp4, u32 *track, u32 *au, u32 *out_info);
static NOINLINE s32 mp4GetAacAuInternal(u32 **mp4, u32 *track, u32 *au, u32 *out_info);
static NOINLINE s32 mp4ConvertSampleInfo(const u32 *raw, u32 *out);
static NOINLINE s32 mp4CheckSampleIndex(const u32 *track, u32 idx);
static NOINLINE s32 mp4AdvanceSampleIndex(u32 **mp4, u32 *track, s32 force);
static NOINLINE s32 mp4GetSampleInfoInternal(u32 **mp4, u32 *track, s32 sample, u32 *out_info);
static NOINLINE s32 mp4FetchSampleMeta(const u32 *track, s32 sample, u32 *out_raw);
static NOINLINE s32 mp4AssembleAvcAu(u32 **mp4, u32 *track, const u32 *raw_info, u32 *avc_pkt, u32 *sg_list, u32 *au);
static NOINLINE s32 mp4AssembleAacAu(u32 **mp4, u32 *track, const u32 *raw_info, const u32 *sg_list, u32 *au);
static NOINLINE s32 mp4GetAvcParamSetInternal(u32 **mp4, u32 *track, s32 sample, u32 *out_param);
static NOINLINE s32 mp4GetAvcAuDirectInternal(u32 **mp4, u32 *track, s32 sample, u32 es_buf, u32 es_size, u32 *au);
static NOINLINE u32 *mp4GetDataMutex(u32 **mp4);
static NOINLINE u32 *mp4GetSampleBufMutex(u32 **mp4);
static NOINLINE s32 mp4InitAuInternal(u32 **mp4, u32 buf, u32 *au);
static NOINLINE s32 mp4CountTracksInternal(u32 **mp4, u32 type);
static NOINLINE s32 mp4QuerySampleBufSizeInternal(u32 **mp4, s32 max_samples, s32 avg_size, s32 max_size, s32 cache_size);
static NOINLINE s32 mp4SampleBufDestructInternal(u32 **mp4, u32 *track);
static NOINLINE s32 mp4SampleBufFlushInternal(u32 **mp4, u32 *track);
static NOINLINE s32 mp4GetEditListCountInternal(u32 **mp4, u32 *track);
static NOINLINE s32 mp4UnregistTrackInternal(u32 **mp4, u32 *track);
static NOINLINE s32 mp4PutSampleNumInternal(u32 **mp4, u32 *track, u32 sample);
static NOINLINE s32 mp4GetSampleNumInternal(u32 **mp4, u32 *track);
static NOINLINE s32 mp4GetAacAuDirectInternal(u32 **mp4, u32 *track, s32 sample, u32 es_buf, u32 es_size, u32 *au);
static NOINLINE s32 mp4FindTrackByType(u32 **mp4, u32 type, s32 idx, u32 *out_count, u32 *out_track);
static NOINLINE s32 mp4ParseAacDescriptors(u32 **mp4, u32 *track);
static NOINLINE s32 mp4ParseAvcDescriptors(u32 **mp4, u32 *track);
static NOINLINE s32 mp4LookupAvcParamSet(u32 **mp4, u32 *track, const u32 *raw_info, u32 *out_pkt);
static NOINLINE s32 mp4InitCallbackBundle(u32 *dst, const u32 *src);
static NOINLINE s32 mp4IoSeek(u32 *io, s64 off, s16 whence);
static NOINLINE s32 mp4IoReadBE(u8 *dst, u32 size, u32 *io);
static NOINLINE void *mp4Alloc(u32 size);
static NOINLINE s32 mp4Free(void *ptr);
static NOINLINE void *mp4GetGlobalCopyCtx(void);
static NOINLINE s64 mp4CbGetCurrentPos(u32 *cb);
static NOINLINE s64 mp4CbSeek(u32 *cb, s32 unused, s32 off_lo, s32 off_hi, s32 whence);
static NOINLINE s32 mp4CbRead(u32 *cb, void *dst, s32 size);
static NOINLINE void *mp4MemcpyWrap(void *dst, const void *src, u32 size);
static NOINLINE s32 mp4IoOpen(void);
static NOINLINE s32 mp4IoClose(void);
static NOINLINE s64 mp4IoTell(const u32 *io);
static NOINLINE s64 mp4IoGetSize(const u32 *io);
static NOINLINE s32 mp4IoRead(void *dst, s32 size, u32 *io);
static NOINLINE void *mp4MemAllocCb(u32 size);
static NOINLINE void *mp4MemCallocCb(u32 size);
static NOINLINE s32 mp4MemFreeCb(void *ptr);
static NOINLINE void *mp4MemCopyCb(void *dst, u32 dst_size, const void *src, u32 src_size);
static NOINLINE void *mp4MemSetCb(void *dst, s32 val, u32 size);
static NOINLINE s32 mp4StrCmpWrap(const char *s1, const char *s2);
static NOINLINE s32 mp4StrLenCb(const u8 *s);
static NOINLINE s32 mp4SgListTotalSize(const u32 *sg);
static NOINLINE s32 mp4PacketizeAvcNal(const u32 *avc_pkt, u32 *au);
static NOINLINE s32 mp4SgListInit(u32 *sg, s32 count);
static NOINLINE s32 mp4SetCopyFn(void *(*fn)(void *, const void *, u32));
static NOINLINE s32 mp4SgListTraverse(const u32 *sg, s32 offset, s32 len, const u32 *ops);
static NOINLINE void mp4SgBuildEmit(u32 addr, u32 len, u32 **ctx);
static NOINLINE void mp4SgBuildFinish(u32 **ctx);
static NOINLINE void mp4SgBuildNext(u32 **ctx);
static NOINLINE void *mp4SgCopyEmit(const void *src, u32 len, u8 **dst_ptr);
static NOINLINE void mp4SgNoop1(void);
static NOINLINE void mp4SgNoop2(void);
static NOINLINE s32 mp4PesFlushLli(u32 *pes_ctx, u32 *au);
static NOINLINE s32 mp4PesAppendData(u32 *pes_ctx, const void *src, s32 len, s32 max_len, u32 *au);
static NOINLINE s32 mp4PesFlushDcacheChain(const u32 *lli);
static NOINLINE s32 mp4PesQueueLliEntries(u32 *pes_ctx);
static NOINLINE s32 mp4CacheInit(u32 *cache, const u32 *cbs, u32 buf, u32 buf_size, u8 enable);
static NOINLINE s32 mp4CacheTerm(u32 *cache);
static NOINLINE s32 mp4CacheReadDispatch(void *dst, s32 dst_size, u32 off_lo, u32 off_hi, s32 len, u32 *cache);
static NOINLINE s32 mp4IoCtxInit(u32 *io, const u32 *cbs, u32 buf, u32 buf_size, u8 enable);
static NOINLINE s32 mp4IoCtxTerm(u32 *io);
static NOINLINE s32 mp4CacheReadBuffered(u8 *dst, s64 off, s32 len, u32 *cache);
static NOINLINE s32 mp4CbGetFileSize(u32 *cb);
static NOINLINE s32 mp4RingPeekSg(u32 *ring, s32 count, u32 *sg);
static NOINLINE s32 mp4RingRead(u32 *ring, s32 count, u8 *dst, s32 advance);
static NOINLINE s32 mp4RingFillFromCb(u32 *ring, s64 off, s32 count);
static NOINLINE s32 mp4RingWrite(u32 *ring, s32 count, const u8 *src);
static NOINLINE s32 mp4RingInit(u32 **mp4, u32 *ring, s32 elem_size, s32 cap, u32 buf, void *read_cb, void *cb_arg);
static NOINLINE s32 mp4RingTerm(u32 *ring);
static NOINLINE s32 mp4RingGetUsed(const u32 *ring);
static NOINLINE s32 mp4RingReset(u32 *ring);
static NOINLINE s32 mp4RingGetFree(const u32 *ring);
static NOINLINE s32 mp4RingConsume(u32 *ring, s32 count, u8 *dst, s32 advance);
static NOINLINE s32 mp4AacTermPool(void);
static NOINLINE s32 mp4AacReleaseSlot(s32 slot);
static NOINLINE s32 mp4AacInitResourceWrap(s32 count);
static NOINLINE s32 mp4AacTermResourceWrap(void);
static NOINLINE s32 mp4AacOpenSlot(s32 *out_slot, s32 sample_rate);
static NOINLINE s32 mp4AacCloseSlot(const s32 *slot);
static NOINLINE s32 mp4AacDecodeSlotWrap(s32 *slot, const u32 *au, u32 pcm_out, s32 reinit, s32 sample_rate);
static NOINLINE s32 mp4AacInitPool(s32 count);
static NOINLINE s32 mp4AacAcquireSlot(s32 sample_rate);
static NOINLINE s32 mp4AacDecodeFrame(s32 slot, const u32 *au, u32 pcm_out);
static NOINLINE s32 mp4AacInitCodecSlot(s32 slot, s32 sample_rate);
static NOINLINE s32 mp4DeleteGlobalMutexes(void);
static NOINLINE s32 mp4DeleteLwMutex(void *work);
static NOINLINE s32 mp4CreateGlobalMutexes(void);
static NOINLINE s32 mp4CreateLwMutex(void *work, const char *name, s32 init_cnt);
static NOINLINE s32 mp4LockLwMutex(void *work);
static NOINLINE s32 mp4UnlockLwMutex(void *work);
static NOINLINE s32 mp4IsInitialized(void);
static NOINLINE s32 mp4InitHeap(u32 size);
static NOINLINE s32 mp4TermHeap(void);
static NOINLINE void *mp4HeapAllocWrap(u32 *heap, u32 size);
static NOINLINE s32 mp4HeapFreeWrap(u32 *heap, void *ptr);
static NOINLINE s32 mp4GetMetaCountInternal(u32 **mp4, const u32 *track);
static NOINLINE s32 mp4GetMetaInfoInternal(u32 **mp4, const u32 *track, s32 idx, u32 *out_handle, u8 *out_info);
static NOINLINE s32 mp4GetMetaDataInternal(u32 **mp4, const u32 *track, s32 idx, void *out_buf, u32 buf_size);
static NOINLINE s32 mp4StrCmp(const char *s1, const char *s2);
static NOINLINE u32 mp4ReadBeUint(const u8 *p, s32 len);
static NOINLINE u32 *mp4UserHeapCreate(const char *name, u32 size, u16 attr);
static NOINLINE s32 mp4UserHeapDelete(u32 *heap);
static NOINLINE void *mp4UserHeapAllocOpt(u32 *heap, u32 size, const u32 *opt);
static NOINLINE s32 mp4UserHeapFree(u32 *heap, void *ptr);
static NOINLINE void *mp4UserHeapAlloc(u32 *heap, u32 size);
static NOINLINE void *mp4BlockAlloc(u32 *arena, u32 size, u32 align);
static NOINLINE s32 mp4BlockFree(u32 *arena, void *ptr);
static NOINLINE s32 mp4BlockInit(u32 *arena, u32 size);
static NOINLINE s32 mp4BlockIsEmpty(const u32 *arena);

static NOINLINE s32 mp4ValidateHandle(u32 **mp4)
{
    if (mp4 == NULL)
        return SCE_MP4_ERROR_INVALID_VALUE;
    u32 *p = *mp4;
    if (p == NULL)
        return SCE_MP4_ERROR_NOT_INITIALIZED;
    if ((u32)p < (u32)g_mp4HeapHandle || (u32)p >= (u32)g_mp4HeapHandle + g_mp4HeapSize)
        return SCE_MP4_ERROR_NOT_INITIALIZED;
    if (*p != MP4_MAGIC_HANDLE)
        return SCE_MP4_ERROR_NOT_INITIALIZED;
    return 0;
}

static NOINLINE s32 mp4CreateInternal(u32 **mp4, const u32 *cbs, u32 buf, s32 buf_size)
{
    u32 cb_bundle[8];
    u8 hdr_buf[4];
    u8 brand_buf[12];
    s32 ret;

    mp4GetGlobalCopyCtx();
    if (buf_size < 1024)
        return SCE_MP4_ERROR_INVALID_VALUE;

    u32 *ctx = (u32 *)mp4Alloc(264);
    *mp4 = ctx;
    if (ctx == NULL)
        return SCE_MP4_ERROR_NO_MEMORY;

    ctx[47] = 0;
    ctx[48] = 0;
    ret = mp4CreateLwMutex(&ctx[49], "SceMp4Data", 0);
    if (ret < 0)
        goto fail_mutex;
    ctx[47] = (u32)&ctx[49];

    ret = mp4CreateLwMutex(&ctx[57], "SceMp4SampleBuf", 0);
    if (ret < 0)
        goto fail_mutex;
    ctx[48] = (u32)&ctx[57];

    ctx[22] = buf;
    if (buf == 0) {
        ret = SCE_MP4_ERROR_INVALID_VALUE;
        goto fail_mutex;
    }

    mp4LockLwMutex((void *)(*mp4)[47]);
    cb_bundle[0] = cbs[0];
    cb_bundle[1] = cbs[1];
    cb_bundle[2] = cbs[2];
    cb_bundle[3] = cbs[3];
    cb_bundle[4] = (u32)mp4;
    mp4IoCtxInit(&ctx[2], cb_bundle, ctx[22], (u32)buf_size, 1);

    if (mp4msv_F595F917(&ctx[1]) != 0) {
        ret = SCE_MP4_ERROR_NO_MEMORY;
        goto fail_io;
    }

    ret = SCE_MP4_ERROR_INVALID_VALUE;
    if (mp4msv_3D8D41A0(ctx[1], &ctx[2]) == 0) {
        s32 r;
        mp4msv_CFF25FAC(ctx[1], hdr_buf);
        mp4msv_BD23D34A(ctx[1], 0x4D534E56U, &ctx[23]);

        r = mp4msv_0D32271B(ctx[1], brand_buf);
        if (r != 0 && (r < 0 || (u32)(r - 0x2002) >= 2U))
            goto fail_ctx;

        r = mp4msv_AD3AF34E(ctx[1], 1, &ctx[34]);
        if (r != 0) {
            if (r < 0 || (u32)(r - 0x2002) >= 2U)
                goto fail_ctx;
            ctx[25] = 0;
        } else {
            ctx[25] = (ctx[35] == 0x61766331U) ? 1 : 0;
        }

        r = mp4msv_0DF0B9AD(ctx[1], 1, &ctx[26]);
        if (r != 0) {
            if (r < 0 || (u32)(r - 0x2002) >= 2U)
                goto fail_ctx;
            ctx[24] = 0;
        } else {
            ctx[24] = (ctx[27] == 0x6D703461U) ? 1 : 0;
        }

        r = mp4msv_67AF9E0F(ctx[1], hdr_buf, 3);
        if (r != 0) {
            ret = (r == 0x2003) ? SCE_MP4_ERROR_BAD_DATA : SCE_MP4_ERROR_INVALID_VALUE;
            goto fail_ctx;
        }

        r = mp4msv_46186C26(ctx[1]);
        if (r == 1) {
            ret = SCE_MP4_ERROR_NO_MEMORY;
            goto fail_ctx;
        }
        if (r != 0) {
            ret = (r == 0x2003) ? SCE_MP4_ERROR_BAD_DATA : SCE_MP4_ERROR_INVALID_VALUE;
            goto fail_ctx;
        }

        r = mp4msv_3D57BA52(ctx[1], &ctx[44]);
        if (r == 0)
            r = mp4msv_E042AB3C(ctx[1], &ctx[45]);
        if (r != 0) {
            ret = (r == 0x2003) ? SCE_MP4_ERROR_BAD_DATA : SCE_MP4_ERROR_INVALID_VALUE;
            goto fail_ctx;
        }

        r = mp4msv_182FF541(ctx[1], &ctx[46]);
        if (r == 0) {
            ctx[0] = MP4_MAGIC_HANDLE;
            mp4UnlockLwMutex((void *)(*mp4)[47]);
            return 0;
        } else if (r == 1) {
            ret = SCE_MP4_ERROR_NO_MEMORY;
        } else if (r == 2) {
            ret = SCE_MP4_ERROR_IO_FAILED;
        } else if (r == 8) {
            ret = SCE_MP4_ERROR_END_OF_TRACK;
        } else if (r == 0x2003) {
            ret = SCE_MP4_ERROR_BAD_DATA;
        } else {
            ret = SCE_MP4_ERROR_INVALID_VALUE;
        }
    }

fail_ctx:
    mp4msv_9D56A7CC(ctx[1]);
fail_io:
    mp4UnlockLwMutex((void *)(*mp4)[47]);
    mp4IoCtxTerm(&ctx[2]);
fail_mutex:
    if (ctx[48] != 0) {
        mp4DeleteLwMutex((void *)ctx[48]);
        ctx[48] = 0;
    }
    if (ctx[47] != 0) {
        mp4DeleteLwMutex((void *)ctx[47]);
        ctx[47] = 0;
    }
    mp4Free(*mp4);
    *mp4 = NULL;
    return ret;
}

static NOINLINE s32 mp4DeleteInternal(u32 **mp4)
{
    s32 ret = SCE_MP4_ERROR_INVALID_VALUE;
    u32 *ctx = *mp4;
    mp4GetGlobalCopyCtx();
    ctx[0] = 0;
    mp4IoCtxTerm(&ctx[2]);
    if (mp4msv_9D56A7CC(ctx[1]) == 0)
        ret = 0;
    if (ctx[48] != 0) {
        mp4DeleteLwMutex((void *)ctx[48]);
        ctx[48] = 0;
    }
    if (ctx[47] != 0) {
        mp4DeleteLwMutex((void *)ctx[47]);
        ctx[47] = 0;
    }
    ctx[0] = 0;
    mp4Free(*mp4);
    *mp4 = NULL;
    return ret;
}

static NOINLINE s32 mp4GetMovieInfoInternal(u32 **mp4, u32 *out_info)
{
    u32 *ctx = *mp4;
    u32 scaled = 0;
    if (out_info == NULL)
        return SCE_MP4_ERROR_INVALID_VALUE;
    s32 r = mp4msv_A824645B(ctx[46], ctx[45], 90000, &scaled);
    out_info[0] = *(const u16 *)&ctx[44];
    out_info[1] = 0;
    out_info[2] = scaled;
    return (r == 0) ? 0 : SCE_MP4_ERROR_END_OF_TRACK;
}

static NOINLINE s32 mp4SampleBufConstructInternal(u32 **mp4, u32 *track, u32 buf, s32 buf_size, s32 max_samples, s32 avg_size, s32 max_size, s32 cache_size)
{
    mp4GetGlobalCopyCtx();
    u32 aligned = (buf + 63U) & ~63U;
    u32 data_buf = aligned + (u32)(max_samples << 6);
    s32 prod = max_samples * avg_size;
    s32 two_max = 2 * max_size;
    s32 data_cap = (prod > two_max) ? prod : two_max;

    if (buf_size < (max_samples << 6) + data_cap + cache_size + 256)
        return SCE_MP4_ERROR_INTERNAL;

    track[17] = buf;
    track[18] = (u32)max_samples;
    track[19] = (u32)avg_size;
    track[20] = (u32)max_size;
    track[23] = aligned;
    track[24] = (u32)(max_samples << 6);
    track[45] = (u32)&track[46];

    s32 ret = mp4RingInit(mp4, &track[35], 64, max_samples, aligned, NULL, NULL);
    if (ret < 0)
        return ret;

    s32 prod2 = (s32)track[18] * (s32)track[19];
    s32 two_max2 = 2 * (s32)track[20];
    s32 cap2 = (prod2 > two_max2) ? prod2 : two_max2;
    track[21] = data_buf;
    track[22] = (u32)cap2;

    if (cache_size < 1024)
        return SCE_MP4_ERROR_INVALID_VALUE;

    mp4CacheInit((u32 *)track[45], track, data_buf + (((u32)cap2 + 63U) & ~63U), (u32)cache_size, cache_size != 0);
    ret = mp4RingInit(mp4, &track[25], 1, (s32)track[22], track[21], (void *)mp4CacheReadDispatch, (void *)track[45]);
    if (ret < 0) {
        mp4RingTerm(&track[35]);
        return ret;
    }
    track[14] = (u32)&track[18];
    return ret;
}

static NOINLINE s32 mp4SampleBufAvailableInternal(u32 **mp4, u32 *track, s32 *out_samples, s32 *out_bytes)
{
    (void)mp4;
    u32 *sb = (u32 *)track[14];
    s32 sfree = mp4RingGetFree(&sb[17]);
    s32 bfree = mp4RingGetFree(&sb[7]);
    s32 ret = (bfree >= 0 && sfree >= 0) ? 0 : SCE_MP4_ERROR_INTERNAL;
    if (out_samples != NULL)
        *out_samples = sfree;
    if (out_bytes != NULL)
        *out_bytes = bfree;
    return ret;
}

static NOINLINE s32 mp4GetSampleNumWithTsInternal(u32 **mp4, u32 *track, const u32 *ts)
{
    u32 scaled_ts = 0;
    s32 sample = 0;
    u32 tmp1 = 0, tmp2 = 0;
    if (mp4 == NULL || track == NULL || ts == NULL)
        return SCE_MP4_ERROR_INVALID_VALUE;
    if (mp4msv_A824645B(ts[1], 90000, track[10], &scaled_ts) != 0)
        return SCE_MP4_ERROR_END_OF_TRACK;
    s32 r = mp4msv_0C0843B0(track[8], scaled_ts, &sample, 1, &tmp1, &tmp2);
    if (sample == 0)
        r = 8;
    if (r == 0)
        return sample - 1;
    if (r == 1)
        return SCE_MP4_ERROR_NO_MEMORY;
    if (r == 2)
        return SCE_MP4_ERROR_IO_FAILED;
    if (r == 8 || r == 8197)
        return SCE_MP4_ERROR_END_OF_TRACK;
    if (r == 8195)
        return SCE_MP4_ERROR_BAD_DATA;
    if (r == 8201)
        return SCE_MP4_ERROR_INTERNAL;
    return SCE_MP4_ERROR_INVALID_VALUE;
}

static NOINLINE s32 mp4SearchSyncSampleInternal(u32 **mp4, u32 *track, s32 dir, s32 sample)
{
    s32 out_sample = 0;
    u8 info[12];
    if (mp4 == NULL || track == NULL)
        return SCE_MP4_ERROR_INVALID_VALUE;
    if (dir != 0 && dir != 1)
        return SCE_MP4_ERROR_INVALID_VALUE;
    s32 r = mp4msv_089364A9(track[8], (u32)(sample + 1), dir, &out_sample, info);
    if (out_sample == 0)
        r = 8;
    if (r == 0)
        return out_sample - 1;
    if (r == 1)
        return SCE_MP4_ERROR_NO_MEMORY;
    if (r == 2)
        return SCE_MP4_ERROR_IO_FAILED;
    if (r == 8 || r == 8201)
        return SCE_MP4_ERROR_END_OF_TRACK;
    if (r == 8195)
        return SCE_MP4_ERROR_BAD_DATA;
    if (r == 8200)
        return SCE_MP4_ERROR_BUFFER_EMPTY;
    return SCE_MP4_ERROR_INVALID_VALUE;
}

static NOINLINE s32 mp4GetEditListInternal(u32 **mp4, u32 *track, u32 *out_list, s32 max_entries)
{
    u32 *ctx = *mp4;
    s32 count = 0;
    if (out_list == NULL)
        return SCE_MP4_ERROR_INVALID_VALUE;
    s32 r = mp4msv_54410730(track[8], &count);
    if (r != 0)
        return (r == 0x2002) ? SCE_MP4_ERROR_BAD_DATA : SCE_MP4_ERROR_INVALID_VALUE;
    if (count < 0)
        return count;
    if (max_entries < count)
        return SCE_MP4_ERROR_INVALID_VALUE;
    for (s32 i = 1; i <= count; i++) {
        s32 seg_dur = 0, media_time = 0, rate = 0;
        u32 scaled_dur = 0, scaled_time = 0;
        r = mp4msv_EE62F96D(track[8], (u32)i, &seg_dur, &media_time, &rate);
        if (r != 0)
            return (r == 8194) ? SCE_MP4_ERROR_BAD_DATA : SCE_MP4_ERROR_INVALID_VALUE;
        if (mp4msv_A824645B((u32)seg_dur, ctx[45], 90000, &scaled_dur) != 0)
            return SCE_MP4_ERROR_END_OF_TRACK;
        out_list[0] = 0;
        out_list[1] = scaled_dur;
        if (media_time == -1) {
            out_list[2] = (u32)-1;
            out_list[3] = (u32)-1;
        } else {
            if (mp4msv_A824645B((u32)media_time, track[10], 90000, &scaled_time) != 0)
                return SCE_MP4_ERROR_END_OF_TRACK;
            out_list[2] = 0;
            out_list[3] = scaled_time;
        }
        out_list[4] = (u32)rate;
        out_list += 5;
    }
    return 0;
}

static NOINLINE s32 mp4GetAvcTrackInfoInternal(u32 **mp4, u32 *track, u32 *out_info)
{
    (void)mp4;
    u32 scaled = 0;
    if ((track[15] & 0x10) == 0 || out_info == NULL)
        return SCE_MP4_ERROR_INVALID_VALUE;
    out_info[2] = track[12];
    if (mp4msv_A824645B(track[11], track[10], 90000, &scaled) == 0) {
        out_info[0] = 0;
        out_info[1] = scaled;
    }
    return 0;
}

static NOINLINE s32 mp4GetAacTrackInfoInternal(u32 **mp4, u32 *track, u32 *out_info)
{
    (void)mp4;
    u32 scaled = 0;
    if ((track[15] & 0x20) == 0)
        return SCE_MP4_ERROR_INVALID_VALUE;
    u32 *desc = *(u32 **)track[13];
    if (desc == NULL || out_info == NULL)
        return SCE_MP4_ERROR_INVALID_VALUE;
    out_info[2] = track[12];
    out_info[3] = desc[0];
    out_info[4] = *((const u16 *)desc + 2);
    if (mp4msv_A824645B(track[11], track[10], 90000, &scaled) == 0) {
        out_info[0] = 0;
        out_info[1] = scaled;
    }
    return 0;
}

static NOINLINE s32 mp4RegistTrackInternal(u32 **mp4, u32 type, s32 idx, const u32 *cbs, u32 *track)
{
    u32 count = 0;
    s32 ret = mp4FindTrackByType(mp4, type, idx, &count, track);
    if (ret != 0 || (s32)count - 1 < idx)
        return SCE_MP4_ERROR_END_OF_TRACK;

    s32 r = mp4msv_9BB5E338(track[8], &track[10]);
    if (r != 0)
        return (r == 0x2003) ? SCE_MP4_ERROR_BAD_DATA : SCE_MP4_ERROR_INVALID_VALUE;

    r = mp4msv_FF9E6F58(track[8], &track[11], (s32)0x80610000);
    if (r == 0)
        r = mp4msv_5EE48079(track[8], &track[12]);

    if (r != 0) {
        if (r == 1) return SCE_MP4_ERROR_NO_MEMORY;
        if (r == 2) return SCE_MP4_ERROR_IO_FAILED;
        if (r == 8) return SCE_MP4_ERROR_END_OF_TRACK;
        if (r == 0x2003) return SCE_MP4_ERROR_BAD_DATA;
        return SCE_MP4_ERROR_INVALID_VALUE;
    }

    if ((type & 0x10) != 0)
        ret = mp4ParseAvcDescriptors(mp4, track);
    if (ret == 0 && (type & 0x20) != 0)
        ret = mp4ParseAacDescriptors(mp4, track);

    track[15] = type;
    track[9] = 0;
    track[0] = cbs[0];
    track[1] = cbs[1];
    track[2] = cbs[2];
    track[3] = cbs[3];
    track[4] = (u32)mp4;
    track[14] = 0;
    return ret;
}

static NOINLINE s32 mp4SampleBufPutInternal(u32 **mp4, u32 *track)
{
    u32 raw[16];
    u32 info[12];
    u32 *sb = (u32 *)track[14];
    if (sb == NULL)
        return SCE_MP4_ERROR_INVALID_VALUE;

    s32 free_slots = mp4RingGetFree(&sb[17]);
    s32 ret = mp4CheckSampleIndex(track, track[9]);
    if (ret < 0)
        return ret;

    s32 rem = (s32)track[12] - (s32)track[9];
    s32 to_put = (free_slots < rem) ? free_slots : rem;
    s32 put_count = 0;

    while (put_count < to_put) {
        ret = mp4FetchSampleMeta(track, (s32)track[9], raw);
        if (ret < 0)
            return ret;
        ret = mp4ConvertSampleInfo(raw, info);
        if (ret < 0)
            return ret;
        if (mp4RingGetFree(&sb[7]) < (s32)info[1])
            break;
        put_count++;
        ret = mp4CheckSampleIndex(track, track[9]);
        if (ret < 0)
            return ret;
        s64 off = ((s64)info[3] << 32) | (u64)info[2];
        if (mp4RingFillFromCb(&sb[7], off, (s32)info[1]) != (s32)info[1])
            return SCE_MP4_ERROR_IO_FAILED;
        if (mp4RingWrite(&sb[17], 1, (const u8 *)raw) != 1)
            return SCE_MP4_ERROR_INTERNAL;
        ret = 0;
        mp4AdvanceSampleIndex(mp4, track, 1);
    }
    return (ret == 0) ? put_count : ret;
}

static NOINLINE s32 mp4GetAvcAuInternal(u32 **mp4, u32 *track, u32 *au, u32 *out_info)
{
    u32 info[12];
    u32 raw[16];
    u32 avc_pkt[8];
    u32 sg[8];
    u32 *sb = (u32 *)track[14];
    if (sb == NULL)
        return SCE_MP4_ERROR_INVALID_VALUE;
    if (mp4RingGetUsed(&sb[17]) == 0)
        return SCE_MP4_ERROR_BUFFER_EMPTY;
    if (mp4RingConsume(&sb[17], 1, (u8 *)raw, 0) != 1)
        return SCE_MP4_ERROR_INTERNAL;
    if (mp4ConvertSampleInfo(raw, info) < 0)
        return SCE_MP4_ERROR_INVALID_VALUE;
    mp4SgListInit(sg, 2);
    mp4RingPeekSg(&sb[7], (s32)info[1], sg);
    s32 ret = mp4AssembleAvcAu(mp4, track, raw, avc_pkt, sg, au);
    if (out_info != NULL)
        mp4ConvertSampleInfo(raw, out_info);
    if (mp4RingConsume(&sb[17], 1, NULL, 1) != 1)
        return SCE_MP4_ERROR_INTERNAL;
    return ret;
}

static NOINLINE s32 mp4GetAacAuInternal(u32 **mp4, u32 *track, u32 *au, u32 *out_info)
{
    u32 info[12];
    u32 raw[16];
    u32 sg[8];
    u32 *sb = (u32 *)track[14];
    if (sb == NULL)
        return SCE_MP4_ERROR_INVALID_VALUE;
    if (mp4RingGetUsed(&sb[17]) == 0)
        return SCE_MP4_ERROR_BUFFER_EMPTY;
    if (mp4RingConsume(&sb[17], 1, (u8 *)raw, 0) != 1)
        return SCE_MP4_ERROR_INTERNAL;
    if (mp4ConvertSampleInfo(raw, info) < 0)
        return SCE_MP4_ERROR_INVALID_VALUE;
    mp4SgListInit(sg, 2);
    mp4RingPeekSg(&sb[7], (s32)info[1], sg);
    s32 ret = mp4AssembleAacAu(mp4, track, raw, sg, au);
    if (out_info != NULL)
        mp4ConvertSampleInfo(raw, out_info);
    if (mp4RingConsume(&sb[17], 1, NULL, 1) != 1)
        return SCE_MP4_ERROR_INTERNAL;
    return ret;
}

static NOINLINE s32 mp4ConvertSampleInfo(const u32 *raw, u32 *out)
{
    out[0] = raw[0];
    out[1] = raw[6];
    out[2] = raw[8];
    out[3] = raw[9];
    out[4] = raw[3];
    out[5] = raw[10];
    out[6] = 0;
    out[7] = raw[1];
    out[8] = 0;
    out[9] = raw[2];
    return 0;
}

static NOINLINE s32 mp4CheckSampleIndex(const u32 *track, u32 idx)
{
    return (idx < track[12]) ? 0 : SCE_MP4_ERROR_END_OF_TRACK;
}

static NOINLINE s32 mp4AdvanceSampleIndex(u32 **mp4, u32 *track, s32 force)
{
    (void)mp4;
    u32 cur = track[9];
    if (force == 0 && cur >= track[12] - 1)
        return SCE_MP4_ERROR_END_OF_TRACK;
    track[9] = cur + 1;
    return 0;
}

static NOINLINE s32 mp4GetSampleInfoInternal(u32 **mp4, u32 *track, s32 sample, u32 *out_info)
{
    (void)mp4;
    u32 raw[16];
    if (sample == -1)
        sample = (s32)track[9];
    s32 ret = mp4FetchSampleMeta(track, sample, raw);
    if (ret >= 0)
        return mp4ConvertSampleInfo(raw, out_info);
    return ret;
}

static NOINLINE s32 mp4FetchSampleMeta(const u32 *track, s32 sample, u32 *out_raw)
{
    s32 flag = 0;
    out_raw[0] = (u32)sample;
    u32 idx = (u32)(sample + 1);
    s32 r = mp4msv_E5C33D61(track[8], idx, 0, &out_raw[6]);
    if (r == 0)
        r = mp4msv_21BD7274(track[8], idx, &out_raw[4], &flag);
    if (r != 0) {
        if (r == 1) return SCE_MP4_ERROR_NO_MEMORY;
        if (r == 2) return SCE_MP4_ERROR_IO_FAILED;
        if (r == 8 || r == 8201) return SCE_MP4_ERROR_END_OF_TRACK;
        if (r == 8195) return SCE_MP4_ERROR_BAD_DATA;
        return SCE_MP4_ERROR_INVALID_VALUE;
    }
    out_raw[5] = out_raw[4] + out_raw[11];
    if (mp4msv_A824645B(out_raw[4], track[10], 90000, &out_raw[2]) != 0)
        return SCE_MP4_ERROR_END_OF_TRACK;
    if (mp4msv_A824645B(out_raw[5], track[10], 90000, &out_raw[1]) != 0)
        return SCE_MP4_ERROR_END_OF_TRACK;
    if (mp4msv_A824645B(out_raw[7], track[10], 90000, &out_raw[3]) != 0)
        return SCE_MP4_ERROR_END_OF_TRACK;
    return 0;
}

static NOINLINE s32 mp4AssembleAvcAu(u32 **mp4, u32 *track, const u32 *raw_info, u32 *avc_pkt, u32 *sg_list, u32 *au)
{
    u32 info[12];
    s32 ret = mp4LookupAvcParamSet(mp4, track, raw_info, avc_pkt);
    if (ret != 0)
        return ret;
    avc_pkt[5] = (u32)sg_list;
    ret = mp4PacketizeAvcNal(avc_pkt, au);
    if (ret != 0)
        return ret;
    ret = mp4ConvertSampleInfo(raw_info, info);
    if (ret == 0) {
        au[0] = info[6];
        au[1] = info[7];
        au[2] = info[8];
        au[3] = info[9];
    }
    return ret;
}

static NOINLINE s32 mp4AssembleAacAu(u32 **mp4, u32 *track, const u32 *raw_info, const u32 *sg_list, u32 *au)
{
    (void)mp4;
    (void)track;
    u32 info[12];
    s32 total = mp4SgListTotalSize(sg_list);
    if (total >= 8193)
        return SCE_MP4_ERROR_BAD_DATA;
    const u32 *cur = sg_list;
    for (s32 off = 0; off < total; cur = (const u32 *)cur[2]) {
        u32 len = cur[3];
        u32 dst = au[4] + (u32)off;
        sceKernelMemcpy((void *)dst, (const void *)cur[0], len);
        sceKernelDcacheWritebackRange((const void *)dst, len);
        off += (s32)len;
    }
    au[5] = (u32)total;
    s32 ret = mp4ConvertSampleInfo(raw_info, info);
    if (ret == 0) {
        au[0] = info[6];
        au[1] = info[7];
        au[2] = info[8];
        au[3] = info[9];
    }
    return ret;
}

static NOINLINE s32 mp4GetAvcParamSetInternal(u32 **mp4, u32 *track, s32 sample, u32 *out_param)
{
    u32 raw[16];
    u32 pkt[8];
    if ((track[15] & 0x10) == 0 || out_param == NULL)
        return SCE_MP4_ERROR_INVALID_VALUE;
    s32 ret = mp4FetchSampleMeta(track, sample, raw);
    if (ret != 0)
        return ret;
    ret = mp4LookupAvcParamSet(mp4, track, raw, pkt);
    if (ret == 0) {
        out_param[0] = pkt[0];
        out_param[1] = pkt[1];
        out_param[2] = pkt[2];
        out_param[3] = pkt[3];
        out_param[4] = pkt[4];
        out_param[7] = pkt[6];
    }
    return ret;
}

static NOINLINE s32 mp4GetAvcAuDirectInternal(u32 **mp4, u32 *track, s32 sample, u32 es_buf, u32 es_size, u32 *au)
{
    u32 raw[16];
    u32 avc_pkt[8];
    u32 sg[4];
    s32 ret = mp4FetchSampleMeta(track, sample, raw);
    if (ret != 0)
        return ret;
    sg[0] = es_buf;
    sg[1] = 0;
    sg[2] = 0;
    sg[3] = es_size;
    return mp4AssembleAvcAu(mp4, track, raw, avc_pkt, sg, au);
}

static NOINLINE u32 *mp4GetDataMutex(u32 **mp4)
{
    return (u32 *)(*mp4)[47];
}

static NOINLINE u32 *mp4GetSampleBufMutex(u32 **mp4)
{
    return (u32 *)(*mp4)[48];
}

static NOINLINE s32 mp4InitAuInternal(u32 **mp4, u32 buf, u32 *au)
{
    (void)mp4;
    if (au == NULL)
        return SCE_MP4_ERROR_INVALID_VALUE;
    au[4] = buf;
    au[5] = 0;
    return 0;
}

static NOINLINE s32 mp4CountTracksInternal(u32 **mp4, u32 type)
{
    u32 count = 0;
    s32 ret = mp4FindTrackByType(mp4, type, 655535, &count, NULL);
    return (ret == 0) ? (s32)count : ret;
}

static NOINLINE s32 mp4QuerySampleBufSizeInternal(u32 **mp4, s32 max_samples, s32 avg_size, s32 max_size, s32 cache_size)
{
    (void)mp4;
    s32 prod = max_samples * avg_size;
    s32 two_max = 2 * max_size;
    return (max_samples << 6) + ((prod > two_max) ? prod : two_max) + cache_size + 256;
}

static NOINLINE s32 mp4SampleBufDestructInternal(u32 **mp4, u32 *track)
{
    (void)mp4;
    mp4GetGlobalCopyCtx();
    u32 *sb = (u32 *)track[14];
    if (sb == NULL)
        return SCE_MP4_ERROR_INVALID_VALUE;
    mp4RingTerm(&sb[7]);
    sb[3] = 0;
    mp4CacheTerm((u32 *)sb[27]);
    mp4RingTerm(&sb[17]);
    sb[5] = 0;
    sb[27] = 0;
    track[17] = 0;
    track[13] = 0;
    track[14] = 0;
    return 0;
}

static NOINLINE s32 mp4SampleBufFlushInternal(u32 **mp4, u32 *track)
{
    (void)mp4;
    u32 *sb = (u32 *)track[14];
    mp4RingReset(&sb[17]);
    mp4RingReset(&sb[7]);
    return 0;
}

static NOINLINE s32 mp4GetEditListCountInternal(u32 **mp4, u32 *track)
{
    (void)mp4;
    s32 count = 0;
    s32 r = mp4msv_54410730(track[8], &count);
    if (r == 0)
        return count;
    return (r == 0x2002) ? SCE_MP4_ERROR_BAD_DATA : SCE_MP4_ERROR_INVALID_VALUE;
}

static NOINLINE s32 mp4UnregistTrackInternal(u32 **mp4, u32 *track)
{
    (void)mp4;
    (void)track;
    return 0;
}

static NOINLINE s32 mp4PutSampleNumInternal(u32 **mp4, u32 *track, u32 sample)
{
    (void)mp4;
    s32 ret = mp4CheckSampleIndex(track, sample);
    if (ret >= 0) {
        track[9] = sample;
        return 0;
    }
    return ret;
}

static NOINLINE s32 mp4GetSampleNumInternal(u32 **mp4, u32 *track)
{
    (void)mp4;
    return (s32)track[9];
}

static NOINLINE s32 mp4GetAacAuDirectInternal(u32 **mp4, u32 *track, s32 sample, u32 es_buf, u32 es_size, u32 *au)
{
    u32 raw[16];
    u32 sg[4];
    sg[0] = es_buf;
    sg[1] = 0;
    sg[2] = 0;
    sg[3] = es_size;
    s32 ret = mp4FetchSampleMeta(track, sample, raw);
    if (ret == 0)
        return mp4AssembleAacAu(mp4, track, raw, sg, au);
    return ret;
}

static NOINLINE s32 mp4FindTrackByType(u32 **mp4, u32 type, s32 idx, u32 *out_count, u32 *out_track)
{
    u32 *ctx = *mp4;
    u32 num_tracks = *(const u16 *)&ctx[44];
    u32 matched = 0;
    s32 found = 0;
    u32 trk_info[8];
    u32 vid_info[8];
    u32 aud_info[4];
    u8 avcc[16];
    char c1 = 0, c2[3];
    u32 trk = 0, handler = 0, desc = 0;
    s32 enc = 0;

    for (u32 i = 1; i <= num_tracks; i++) {
        s32 r = mp4msv_0C06DACB(ctx[1], i, &trk);
        if (r != 0) {
            if (r == 8195) return SCE_MP4_ERROR_BAD_DATA;
            if (r == 8203) return SCE_MP4_ERROR_END_OF_TRACK;
            return SCE_MP4_ERROR_INVALID_VALUE;
        }
        r = mp4msv_A55AF94C(trk, trk_info);
        if (r != 0)
            return (r == 0x2003) ? SCE_MP4_ERROR_BAD_DATA : SCE_MP4_ERROR_INVALID_VALUE;
        if (trk_info[2] != 1)
            continue;
        r = mp4msv_3F6B6C59(trk, &handler);
        if (r != 0)
            return (r == 0x2003) ? SCE_MP4_ERROR_BAD_DATA : SCE_MP4_ERROR_INVALID_VALUE;
        r = mp4msv_6D619263(trk, 1, &desc);
        if (r != 0) {
            if (r == 8194) return SCE_MP4_ERROR_END_OF_TRACK;
            if (r == 8195) return SCE_MP4_ERROR_BAD_DATA;
            return SCE_MP4_ERROR_INVALID_VALUE;
        }
        if (handler == 0x736F756EU) { /* "soun" */
            if (mp4msv_A418509D(desc, aud_info) != 0)
                return SCE_MP4_ERROR_INVALID_VALUE;
            if (aud_info[0] != 0x6D703461U)
                continue;
            if (mp4msv_08F96BE2(aud_info[1], &enc) != 0)
                return SCE_MP4_ERROR_INVALID_VALUE;
            if (enc != 0)
                continue;
            if (type == 0x20)
                matched++;
        } else if (handler == 0x76696465U) { /* "vide" */
            if (mp4msv_2BAF7C25(desc, vid_info) != 0)
                return SCE_MP4_ERROR_INVALID_VALUE;
            if (vid_info[0] != 0x61766331U)
                continue;
            if (mp4msv_35144B9C(desc, avcc, &c1, c2) != 0)
                continue;
            if (mp4msv_08F96BE2(vid_info[1], &enc) != 0)
                return SCE_MP4_ERROR_INVALID_VALUE;
            if (enc != 0)
                continue;
            if (avcc[0] == 66) {
                if (type == 0x10 || type == 0x12)
                    matched++;
            } else if (avcc[0] == 77) {
                if (type == 0x10 || type == 0x11)
                    matched++;
            } else {
                if (type == 0x10)
                    matched++;
            }
        }
        if (!found && idx == (s32)matched - 1) {
            found = 1;
            if (out_track != NULL)
                out_track[8] = trk;
        }
    }
    if (out_count != NULL)
        *out_count = matched;
    return 0;
}

static NOINLINE s32 mp4ParseAacDescriptors(u32 **mp4, u32 *track)
{
    (void)mp4;
    u32 *dst = &track[60];
    u32 count = 0;
    u32 desc = 0;
    u32 info[4];

    track[13] = (u32)dst;
    dst[0] = 0;
    mp4MemSetCb(&dst[2], 0, 8);

    s32 r = mp4msv_A9FD62E2(track[8], &count);
    if (r != 0)
        return (r == 0x2003) ? SCE_MP4_ERROR_BAD_DATA : SCE_MP4_ERROR_INVALID_VALUE;
    if (count == 0)
        return SCE_MP4_ERROR_NO_RESOURCE;

    dst[1] = count;
    r = mp4msv_6D619263(track[8], 1, &desc);
    if (r != 0) {
        if (r == 8194) return SCE_MP4_ERROR_END_OF_TRACK;
        if (r == 8195) return SCE_MP4_ERROR_BAD_DATA;
        return SCE_MP4_ERROR_INVALID_VALUE;
    }
    if (mp4msv_A418509D(desc, info) != 0)
        return SCE_MP4_ERROR_INVALID_VALUE;
    if (info[0] != 0x6D703461U)
        return SCE_MP4_ERROR_BAD_DATA;
    dst[2] = *((const u16 *)info + 7);
    *((u16 *)dst + 6) = *((const u16 *)info + 4);
    *((u16 *)dst + 7) = 0;
    dst[0] = (u32)&dst[2];
    return 0;
}

static NOINLINE s32 mp4ParseAvcDescriptors(u32 **mp4, u32 *track)
{
    (void)mp4;
    u32 *dst = &track[60];
    u8 *param_buf = (u8 *)&track[662];
    u32 rem_buf = 4096;
    u32 count = 0;
    u32 desc = 0;
    u32 info[8];
    u8 avcc[16];
    char c1 = 0, c2[15];
    u16 len = 0;

    track[13] = (u32)dst;
    dst[0] = 0;
    mp4MemSetCb(&dst[2], 0, 2400);
    mp4MemSetCb(param_buf, 0, 4096);

    s32 r = mp4msv_A9FD62E2(track[8], &count);
    if (r != 0)
        return (r == 0x2003) ? SCE_MP4_ERROR_BAD_DATA : SCE_MP4_ERROR_INVALID_VALUE;
    if (count == 0 || count > 100)
        return SCE_MP4_ERROR_NO_RESOURCE;

    dst[1] = count;
    for (u32 i = 0; i < count; i++) {
        c1 = 0;
        c2[0] = 0;
        r = mp4msv_6D619263(track[8], i + 1, &desc);
        if (r != 0) {
            if (r == 8194) return SCE_MP4_ERROR_END_OF_TRACK;
            if (r == 8195) return SCE_MP4_ERROR_BAD_DATA;
            return SCE_MP4_ERROR_INVALID_VALUE;
        }
        if (mp4msv_2BAF7C25(desc, info) != 0)
            return SCE_MP4_ERROR_INVALID_VALUE;
        if (info[0] == 0x61766331U) {
            if (mp4msv_35144B9C(desc, avcc, &c1, c2) != 0)
                return SCE_MP4_ERROR_INVALID_VALUE;
            u32 *ent = &dst[6 * i + 2];
            ent[5] = avcc[3];
            if (mp4msv_2B1B8D23(desc, 1, &len) != 0)
                return SCE_MP4_ERROR_INVALID_VALUE;
            ent[3] = len;
            if (rem_buf < len)
                return SCE_MP4_ERROR_NO_RESOURCE;
            if (mp4msv_E641D779(desc, 1, 0, 0, param_buf) != 0)
                return SCE_MP4_ERROR_INVALID_VALUE;
            ent[1] = (u32)param_buf;
            param_buf += ent[3];
            rem_buf -= ent[3];

            if (mp4msv_A09878FC(desc, 1, &len) != 0)
                return SCE_MP4_ERROR_INVALID_VALUE;
            ent[4] = len;
            if (rem_buf < len)
                return SCE_MP4_ERROR_NO_RESOURCE;
            if (mp4msv_BE558D75(desc, 1, 0, 0, param_buf) != 0)
                return SCE_MP4_ERROR_INVALID_VALUE;
            ent[2] = (u32)param_buf;
            param_buf += ent[4];
            rem_buf -= ent[4];
            ent[0] = desc;
        }
    }
    dst[0] = (u32)&dst[2];
    return 0;
}

static NOINLINE s32 mp4LookupAvcParamSet(u32 **mp4, u32 *track, const u32 *raw_info, u32 *out_pkt)
{
    (void)mp4;
    u32 *tbl = (u32 *)track[13];
    if (tbl == NULL || tbl[0] == 0)
        return SCE_MP4_ERROR_INVALID_VALUE;
    u32 target = raw_info[15];
    u32 *ent = (u32 *)tbl[0];
    if (ent[0] != target) {
        u32 count = tbl[1];
        u32 i;
        for (i = 0; i < count; i++) {
            if (tbl[6 * i + 2] == target)
                break;
        }
        if (i >= count)
            return SCE_MP4_ERROR_BAD_DATA;
        ent = &tbl[6 * i + 2];
        tbl[0] = (u32)ent;
    }
    out_pkt[0] = ent[1];
    out_pkt[1] = ent[3];
    out_pkt[2] = ent[2];
    out_pkt[3] = ent[4];
    out_pkt[4] = ent[5] + 1;
    out_pkt[6] = 3;
    return 0;
}

static NOINLINE s32 mp4InitCallbackBundle(u32 *dst, const u32 *src)
{
    u32 gp;
    __asm__ volatile ("move %0, $gp" : "=r"(gp));
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
    dst[3] = src[3];
    dst[4] = src[4];
    if (sceKernelGetModuleGPByAddress((const void *)dst[1], &dst[5]) < 0)
        dst[5] = gp;
    if (sceKernelGetModuleGPByAddress((const void *)dst[2], &dst[6]) < 0)
        dst[6] = gp;
    s32 ret = sceKernelGetModuleGPByAddress((const void *)dst[3], &dst[7]);
    if (ret < 0)
        dst[7] = gp;
    return ret;
}

/* 0x2928: sceMp4Init */
s32 sceMp4_68651CBC(s32 mode, s32 version)
{
    static const void *const msv_cbs[32] = {
        (const void *)mp4IoOpen,
        (const void *)mp4IoClose,
        (const void *)mp4IoTell,
        (const void *)mp4IoSeek,
        (const void *)mp4IoGetSize,
        (const void *)mp4IoRead,
        (const void *)mp4IoReadBE,
        (const void *)mp4MemAllocCb,
        (const void *)mp4MemCallocCb,
        (const void *)mp4MemFreeCb,
        (const void *)mp4MemCopyCb,
        (const void *)mp4MemSetCb,
        NULL,
        NULL,
        (const void *)mp4StrCmpWrap,
        NULL,
        (const void *)mp4StrLenCb,
    };
    static const u32 msv_cfg[2][32] = {
        { 0x64, 1, 1, 3 },
        { 0x1f4, 1, 1, 3 },
    };
    const void *cbs[32];
    u32 cfg[2][32];

    if (_sceKernelCheckThreadStack() < 4080)
        return SCE_MP4_ERROR_STACK_OVERFLOW;
    if ((u32)(mode - 1) >= 2U || version != 1)
        return SCE_MP4_ERROR_INVALID_VALUE;
    if (mp4IsInitialized() == 0)
        return SCE_MP4_ERROR_ALREADY_INITED;

    mp4CreateGlobalMutexes();
    s32 ret = mp4InitHeap(g_mp4HeapSizes[mode - 1]);
    if (ret < 0) {
        mp4DeleteGlobalMutexes();
        return ret;
    }

    mp4SetCopyFn(sceKernelMemcpy);
    sceKernelMemcpy(cbs, msv_cbs, sizeof(cbs));
    sceKernelMemcpy(cfg, msv_cfg, sizeof(cfg));
    mp4msv_9CA13D1A(1, cbs);
    mp4msv_3C2183C7(1, cfg[mode - 1]);
    return ret;
}

static NOINLINE s32 mp4IoSeek(u32 *io, s64 off, s16 whence)
{
    s64 pos;
    if (whence == 0) {
        pos = off;
    } else if (whence == 1) {
        pos = *(const s64 *)io + off;
    } else if (whence == 2) {
        pos = (s64)io[3] - off;
    } else {
        return -1;
    }
    if (pos < 0)
        pos = 0;
    else if (pos > (s64)io[3])
        pos = (s64)io[3];
    io[0] = (u32)pos;
    io[1] = 0;
    return 0;
}

static NOINLINE s32 mp4IoReadBE(u8 *dst, u32 size, u32 *io)
{
    u8 tmp[16];
    u32 req = (size >= 9U) ? 8U : size;
    s32 r = mp4CacheReadDispatch(tmp, 8, io[0], io[1], (s32)req, (u32 *)io[4]);
    if (r > 0) {
        *(s64 *)io += r;
    }
    if (r == (s32)req) {
        for (u32 i = 0; i < (u32)r; i++)
            dst[r - 1 - i] = tmp[i];
    }
    return r;
}

static NOINLINE void *mp4Alloc(u32 size)
{
    return mp4HeapAllocWrap(g_mp4HeapHandle, size);
}

static NOINLINE s32 mp4Free(void *ptr)
{
    return mp4HeapFreeWrap(g_mp4HeapHandle, ptr);
}

/* 0x2c78: sceMp4Finish */
s32 sceMp4_9042B257(void)
{
    if (_sceKernelCheckThreadStack() < 4080)
        return SCE_MP4_ERROR_STACK_OVERFLOW;
    if (mp4IsInitialized() != 0)
        return SCE_MP4_ERROR_NOT_INITIALIZED;
    s32 ret = mp4TermHeap();
    mp4DeleteGlobalMutexes();
    return ret;
}

static NOINLINE void *mp4GetGlobalCopyCtx(void)
{
    return (void *)&g_mp4CopyFn;
}

static NOINLINE s64 mp4CbGetCurrentPos(u32 *cb)
{
    u32 **mp4 = (u32 **)cb[4];
    u32 old_gp;
    __asm__ volatile ("move %0, $gp" : "=r"(old_gp));
    mp4UnlockLwMutex(mp4GetDataMutex(mp4));
    __asm__ volatile ("move $gp, %0" : : "r"(cb[5]));
    s64 ret = ((s64 (*)(u32))cb[1])(cb[0]);
    __asm__ volatile ("move $gp, %0" : : "r"(old_gp));
    mp4LockLwMutex(mp4GetDataMutex(mp4));
    return ret;
}

static NOINLINE s64 mp4CbSeek(u32 *cb, s32 unused, s32 off_lo, s32 off_hi, s32 whence)
{
    (void)unused;
    u32 **mp4 = (u32 **)cb[4];
    s64 off = ((s64)off_hi << 32) | (u64)(u32)off_lo;
    u32 old_gp;
    __asm__ volatile ("move %0, $gp" : "=r"(old_gp));
    mp4UnlockLwMutex(mp4GetDataMutex(mp4));
    __asm__ volatile ("move $gp, %0" : : "r"(cb[6]));
    s64 ret = ((s64 (*)(u32, s64, s32))cb[2])(cb[0], off, whence);
    __asm__ volatile ("move $gp, %0" : : "r"(old_gp));
    mp4LockLwMutex(mp4GetDataMutex(mp4));
    return ret;
}

static NOINLINE s32 mp4CbRead(u32 *cb, void *dst, s32 size)
{
    u32 **mp4 = (u32 **)cb[4];
    u32 old_gp;
    __asm__ volatile ("move %0, $gp" : "=r"(old_gp));
    mp4UnlockLwMutex(mp4GetDataMutex(mp4));
    __asm__ volatile ("move $gp, %0" : : "r"(cb[7]));
    s32 ret = ((s32 (*)(u32, void *, s32))cb[3])(cb[0], dst, size);
    __asm__ volatile ("move $gp, %0" : : "r"(old_gp));
    mp4LockLwMutex(mp4GetDataMutex(mp4));
    return ret;
}

static NOINLINE void *mp4MemcpyWrap(void *dst, const void *src, u32 size)
{
    return sceKernelMemcpy(dst, src, size);
}

static NOINLINE s32 mp4IoOpen(void) { return 0; }
static NOINLINE s32 mp4IoClose(void) { return 0; }
static NOINLINE s64 mp4IoTell(const u32 *io) { return *(const s64 *)io; }
static NOINLINE s64 mp4IoGetSize(const u32 *io) { return (s64)io[3]; }

static NOINLINE s32 mp4IoRead(void *dst, s32 size, u32 *io)
{
    s32 r = mp4CacheReadDispatch(dst, size, io[0], io[1], size, (u32 *)io[4]);
    if (r > 0)
        *(s64 *)io += r;
    return r;
}

static NOINLINE void *mp4MemAllocCb(u32 size) { return mp4HeapAllocWrap(g_mp4HeapHandle, size); }

static NOINLINE void *mp4MemCallocCb(u32 size)
{
    void *p = mp4HeapAllocWrap(g_mp4HeapHandle, size);
    if (p != NULL)
        sceKernelMemset(p, 0, size);
    return p;
}

static NOINLINE s32 mp4MemFreeCb(void *ptr) { return mp4HeapFreeWrap(g_mp4HeapHandle, ptr); }

static NOINLINE void *mp4MemCopyCb(void *dst, u32 dst_size, const void *src, u32 src_size)
{
    if (dst_size < src_size)
        src_size = dst_size;
    return sceKernelMemcpy(dst, src, src_size);
}

static NOINLINE void *mp4MemSetCb(void *dst, s32 val, u32 size) { return sceKernelMemset(dst, val, size); }
static NOINLINE s32 mp4StrCmpWrap(const char *s1, const char *s2) { return mp4StrCmp(s1, s2); }

static NOINLINE s32 mp4StrLenCb(const u8 *s)
{
    s32 n = 0;
    while (*s++ != 0)
        n++;
    return n;
}

static NOINLINE s32 mp4SgListTotalSize(const u32 *sg)
{
    s32 total = 0;
    s32 cnt = 0;
    while (1) {
        total += (s32)sg[3];
        sg = (const u32 *)sg[2];
        if (sg == NULL)
            break;
        if (++cnt >= 2)
            return -1;
    }
    return total;
}

static NOINLINE s32 mp4PacketizeAvcNal(const u32 *avc_pkt, u32 *au)
{
    u32 sg_a[4], sg_b[4];
    u32 pes_ctx[7];
    u32 ops[4];
    u32 nal_len = 0;
    u8 *nal_ptr = NULL;
    u32 build_ctx[2];
    s32 wrote_any = 0;
    s32 pos = 0;

    g_pesLliCount = 100;
    au[5] = 0;
    u32 len_sz = avc_pkt[4];
    if (len_sz == 0 || (len_sz >= 3 && len_sz != 4))
        return SCE_MP4_ERROR_INVALID_VALUE;

    pes_ctx[4] = 0;
    pes_ctx[5] = g_pesLliCount;
    pes_ctx[6] = (u32)&g_pesLliCount;

    if ((avc_pkt[6] & 1) != 0 && avc_pkt[0] != 0) {
        s32 r = mp4PesAppendData(pes_ctx, &g_annexBStartCode, 4, 99328, au);
        if (r != 0) return r;
        r = mp4PesAppendData(pes_ctx, (const void *)avc_pkt[0], (s32)avc_pkt[1], 99328, au);
        if (r != 0) return r;
        wrote_any = 1;
    }
    if ((avc_pkt[6] & 2) != 0 && avc_pkt[2] != 0) {
        s32 r = mp4PesAppendData(pes_ctx, &g_annexBStartCode, 4, 99328, au);
        if (r != 0) return r;
        r = mp4PesAppendData(pes_ctx, (const void *)avc_pkt[2], (s32)avc_pkt[3], 99328, au);
        if (r != 0) return r;
        wrote_any = 1;
    }

    s32 total = mp4SgListTotalSize((const u32 *)avc_pkt[5]);
    if (total < 0)
        return SCE_MP4_ERROR_INTERNAL;

    while (pos < total) {
        nal_len = 0;
        nal_ptr = (u8 *)&nal_len;
        ops[0] = (u32)mp4SgCopyEmit;
        ops[1] = (u32)mp4SgNoop1;
        ops[2] = (u32)mp4SgNoop2;
        ops[3] = (u32)&nal_ptr;
        if (mp4SgListTraverse((const u32 *)avc_pkt[5], pos, (s32)len_sz, ops) < 0)
            return SCE_MP4_ERROR_INTERNAL;
        s32 nlen = (s32)mp4ReadBeUint((const u8 *)&nal_len, (s32)len_sz);
        if (nlen <= 0)
            return SCE_MP4_ERROR_BAD_DATA;

        sg_a[2] = (u32)sg_b;
        sg_b[2] = 0;
        build_ctx[0] = (u32)sg_a;
        build_ctx[1] = (u32)pes_ctx;
        ops[0] = (u32)mp4SgBuildEmit;
        ops[1] = (u32)mp4SgBuildFinish;
        ops[2] = (u32)mp4SgBuildNext;
        ops[3] = (u32)build_ctx;
        s32 next_pos = pos + (s32)len_sz;
        if (mp4SgListTraverse((const u32 *)avc_pkt[5], next_pos, nlen, ops) < 0)
            return SCE_MP4_ERROR_BAD_DATA;
        pos = next_pos + nlen;

        s32 skip = 1;
        if (!wrote_any || ((*(const u8 *)sg_a[0] & 0x1FU) - 7U) < 2U)
            skip = 0;
        s32 r = mp4PesAppendData(pes_ctx, (const u8 *)&g_annexBStartCode + skip, 4 - skip, 99328, au);
        if (r != 0)
            return r;
        wrote_any = 1;

        const u32 *seg = sg_a;
        for (s32 seg_i = 0; ; seg_i++) {
            r = mp4PesAppendData(pes_ctx, (const void *)seg[0], (s32)seg[3], 99328, au);
            if (r != 0)
                return r;
            if (seg[2] == 0)
                break;
            if (seg_i >= 1)
                return SCE_MP4_ERROR_INTERNAL;
            seg = (const u32 *)seg[2];
        }
    }

    if (total < pos)
        return SCE_MP4_ERROR_BAD_DATA;
    s32 r = mp4PesFlushLli(pes_ctx, au);
    if (r == 0) {
        pes_ctx[4] = 0;
        pes_ctx[5] = *(const u32 *)pes_ctx[6];
    }
    return r;
}

static NOINLINE s32 mp4SgListInit(u32 *sg, s32 count)
{
    for (s32 i = 0; i < count - 1; i++)
        sg[4 * i + 2] = (u32)&sg[4 * (i + 1)];
    sg[4 * (count - 1) + 2] = 0;
    return 0;
}

static NOINLINE s32 mp4SetCopyFn(void *(*fn)(void *, const void *, u32))
{
    g_mp4CopyFn = fn;
    return 0;
}

static NOINLINE s32 mp4SgListTraverse(const u32 *sg, s32 offset, s32 len, const u32 *ops)
{
    s32 total = mp4SgListTotalSize(sg);
    if (total < 0)
        return total;
    if (offset < 0 || offset + len <= 0 || total < offset + len)
        return -2;

    s32 cur_off = 0;
    s32 idx = 0;
    while (idx < 2) {
        s32 seg_len = (s32)sg[3];
        if (offset >= cur_off && offset < cur_off + seg_len)
            break;
        cur_off += seg_len;
        sg = (const u32 *)sg[2];
        idx++;
    }
    if (idx >= 2)
        return -1;

    s32 seg_pos = offset - cur_off;
    s32 copied = 0;
    while (1) {
        s32 rem = len - copied;
        s32 avail = (s32)sg[3] - seg_pos;
        s32 chunk = (rem < avail) ? rem : avail;
        ((void (*)(u32, s32, u32))ops[0])(sg[0] + (u32)seg_pos, chunk, ops[3]);
        copied += chunk;
        idx++;
        seg_pos = 0;
        if (copied >= len)
            break;
        ((void (*)(u32))ops[2])(ops[3]);
        sg = (const u32 *)sg[2];
        if (idx >= 2)
            return -1;
    }
    ((void (*)(u32))ops[1])(ops[3]);
    return (copied > len) ? -3 : 0;
}

static NOINLINE void mp4SgBuildEmit(u32 addr, u32 len, u32 **ctx)
{
    u32 *cur = *ctx;
    cur[0] = addr;
    cur[1] = 0;
    cur[3] = len;
}

static NOINLINE void mp4SgBuildFinish(u32 **ctx) { (*ctx)[2] = 0; }
static NOINLINE void mp4SgBuildNext(u32 **ctx) { *ctx = (u32 *)(*ctx)[2]; }

static NOINLINE void *mp4SgCopyEmit(const void *src, u32 len, u8 **dst_ptr)
{
    u8 *dst = *dst_ptr;
    void *r = g_mp4CopyFn(dst, src, len);
    *dst_ptr = dst + len;
    return r;
}

static NOINLINE void mp4SgNoop1(void) {}
static NOINLINE void mp4SgNoop2(void) {}

static NOINLINE s32 mp4PesFlushLli(u32 *pes_ctx, u32 *au)
{
    u32 count = pes_ctx[4];
    if (count == 0)
        return 0;
    u32 *lli = (u32 *)(pes_ctx[6] + 4);
    lli[4 * count - 2] = 0;
    u32 dst = au[4];
    mp4PesFlushDcacheChain(lli);
    if ((dst & 0x1FFFFFFFU) > 0x3FFFFFU)
        sceKernelDcacheWritebackInvalidateRange((const void *)dst, au[5]);
    return sceMpegBasePESpacketCopy(lli);
}

static NOINLINE s32 mp4PesAppendData(u32 *pes_ctx, const void *src, s32 len, s32 max_len, u32 *au)
{
    if (max_len != 0 && (s32)au[5] + len > max_len)
        return 0;
    pes_ctx[0] = (u32)src;
    pes_ctx[1] = au[4] + au[5];
    pes_ctx[2] = (u32)len;
    pes_ctx[3] = 0;
    while (1) {
        s32 r = mp4PesQueueLliEntries(pes_ctx);
        if (r != 0)
            return r;
        u32 done = pes_ctx[3];
        pes_ctx[2] -= done;
        pes_ctx[0] += done;
        pes_ctx[1] += done;
        au[5] += done;
        if (pes_ctx[4] < pes_ctx[5] || (s32)done >= (s32)pes_ctx[2])
            return 0;
        r = mp4PesFlushLli(pes_ctx, au);
        if (r != 0)
            return r;
        pes_ctx[4] = 0;
        pes_ctx[5] = *(const u32 *)pes_ctx[6];
    }
}

static NOINLINE s32 mp4PesFlushDcacheChain(const u32 *lli)
{
    s32 i = 0;
    while (lli != NULL && i < 100) {
        sceKernelDcacheWritebackInvalidateRange((const void *)lli[0], lli[3] & 0xFFFU);
        lli = (const u32 *)lli[2];
        i++;
    }
    return (i < 100) ? 1 : 0;
}

static NOINLINE s32 mp4PesQueueLliEntries(u32 *pes_ctx)
{
    u32 *lli = (u32 *)(pes_ctx[6] + 4);
    pes_ctx[3] = 0;
    u32 src = pes_ctx[0];
    u32 dst = pes_ctx[1];
    while ((s32)pes_ctx[4] < (s32)pes_ctx[5] && (s32)pes_ctx[2] > (s32)pes_ctx[3]) {
        u32 idx = pes_ctx[4];
        u32 rem = pes_ctx[2] - pes_ctx[3];
        u32 chunk = (rem < 4080U) ? rem : 4080U;
        u32 *ent = &lli[4 * idx];
        ent[0] = src;
        ent[1] = dst;
        ent[2] = (u32)&lli[4 * (idx + 1)];
        if (chunk >= 0x1000U) {
            ent[3] = 0;
            return SCE_MP4_ERROR_INVALID_VALUE;
        }
        ent[3] = chunk;
        src += 4080U;
        dst += 4080U;
        pes_ctx[3] += chunk;
        pes_ctx[4] = idx + 1;
    }
    return 0;
}

static NOINLINE s32 mp4CacheInit(u32 *cache, const u32 *cbs, u32 buf, u32 buf_size, u8 enable)
{
    mp4InitCallbackBundle(&cache[2], cbs);
    cache[0] = (u32)-1;
    cache[1] = (u32)-1;
    if ((enable & 1) != 0) {
        cache[13] = 1;
        cache[10] = buf;
        cache[11] = buf_size;
    } else {
        cache[13] = 0;
        cache[10] = 0;
        cache[11] = 0;
    }
    cache[12] = 0;
    return 0;
}

static NOINLINE s32 mp4CacheTerm(u32 *cache)
{
    cache[10] = 0;
    return 0;
}

static NOINLINE s32 mp4CacheReadDispatch(void *dst, s32 dst_size, u32 off_lo, u32 off_hi, s32 len, u32 *cache)
{
    (void)dst_size;
    if ((cache[13] & 1) != 0) {
        s64 off = ((s64)off_hi << 32) | (u64)off_lo;
        return mp4CacheReadBuffered((u8 *)dst, off, len, cache);
    }
    s64 pos = mp4CbSeek(&cache[2], 0, (s32)off_lo, (s32)off_hi, 0);
    if (pos < 0)
        return SCE_MP4_ERROR_IO_FAILED;
    return mp4CbRead(&cache[2], dst, len);
}

static NOINLINE s32 mp4IoCtxInit(u32 *io, const u32 *cbs, u32 buf, u32 buf_size, u8 enable)
{
    u32 *cache = &io[6];
    io[4] = (u32)cache;
    mp4InitCallbackBundle(&io[8], cbs);
    io[6] = (u32)-1;
    io[7] = (u32)-1;
    if ((enable & 1) != 0) {
        cache[13] = 1;
        cache[10] = buf;
        cache[11] = buf_size;
    } else {
        cache[13] = 0;
        cache[10] = 0;
        cache[11] = 0;
    }
    cache[12] = 0;
    io[0] = 0;
    io[1] = 0;
    io[2] = 0;
    io[3] = (u32)mp4CbGetFileSize(&cache[2]);
    return 0;
}

static NOINLINE s32 mp4IoCtxTerm(u32 *io)
{
    u32 *cache = (u32 *)io[4];
    io[4] = 0;
    cache[10] = 0;
    return 0;
}

static NOINLINE s32 mp4CacheReadBuffered(u8 *dst, s64 off, s32 len, u32 *cache)
{
    s32 copied = 0;
    while (copied < len) {
        s64 cur_pos = off + copied;
        s64 c_start = *(const s64 *)cache;
        s64 c_end = c_start + (s32)cache[12];
        if (cur_pos >= c_start && cur_pos < c_end) {
            s32 c_off = (s32)(cur_pos - c_start);
            s32 rem = len - copied;
            s32 avail = (s32)cache[12] - c_off;
            s32 chunk = (rem < avail) ? rem : avail;
            if (mp4MemcpyWrap(dst + copied, (const u8 *)cache[10] + c_off, (u32)chunk) == NULL)
                return SCE_MP4_ERROR_INTERNAL;
            copied += chunk;
        } else {
            s64 bsz = (s32)cache[11];
            s64 aligned = (cur_pos / bsz) * bsz;
            *(s64 *)cache = aligned;
            if (mp4CbSeek(&cache[2], 0, (s32)aligned, (s32)(aligned >> 32), 0) < 0)
                return SCE_MP4_ERROR_IO_FAILED;
            s32 r = mp4CbRead(&cache[2], (void *)cache[10], (s32)cache[11]);
            cache[12] = (u32)r;
            if (r < 0)
                return SCE_MP4_ERROR_IO_FAILED;
        }
    }
    return (copied > len) ? SCE_MP4_ERROR_INTERNAL : len;
}

static NOINLINE s32 mp4CbGetFileSize(u32 *cb)
{
    s64 orig = mp4CbGetCurrentPos(cb);
    if (mp4CbSeek(cb, 0, 0, 0, 2) < 0)
        return 0;
    s64 end = mp4CbGetCurrentPos(cb);
    s32 size = (end == -1LL) ? 0 : (s32)end;
    mp4CbSeek(cb, 0, (s32)orig, (s32)(orig >> 32), 0);
    return size;
}

static NOINLINE s32 mp4RingPeekSg(u32 *ring, s32 count, u32 *sg)
{
    mp4GetGlobalCopyCtx();
    s32 take = (count < (s32)ring[3]) ? count : (s32)ring[3];
    s32 rem = take;
    while (1) {
        u32 cap = ring[0];
        u32 rpos = ring[1];
        s32 chunk = ((s32)(cap - rpos) < rem) ? (s32)(cap - rpos) : rem;
        rem -= chunk;
        sg[0] = ring[5] + rpos * ring[4];
        sg[3] = (u32)chunk * ring[4];
        rpos += (u32)chunk;
        if (rpos == cap)
            rpos = 0;
        ring[1] = rpos;
        if (rem <= 0)
            break;
        sg = (u32 *)sg[2];
    }
    sg[2] = 0;
    ring[3] -= (u32)take;
    return take;
}

static NOINLINE s32 mp4RingRead(u32 *ring, s32 count, u8 *dst, s32 advance)
{
    u32 saved_rpos = 0, saved_used = 0;
    mp4GetGlobalCopyCtx();
    if (!advance) {
        saved_used = ring[3];
        saved_rpos = ring[1];
    }
    s32 take = (count < (s32)ring[3]) ? count : (s32)ring[3];
    s32 rem = take;
    s32 copied = 0;
    while (rem > 0) {
        u32 cap = ring[0];
        u32 rpos = ring[1];
        s32 chunk = ((s32)(cap - rpos) < rem) ? (s32)(cap - rpos) : rem;
        rem -= chunk;
        if (dst != NULL)
            mp4MemcpyWrap(dst + (u32)copied * ring[4], (const u8 *)ring[5] + rpos * ring[4], (u32)chunk * ring[4]);
        copied += chunk;
        rpos += (u32)chunk;
        if (rpos == cap)
            rpos = 0;
        ring[1] = rpos;
    }
    ring[3] -= (u32)take;
    if (!advance) {
        ring[3] = saved_used;
        ring[1] = saved_rpos;
    }
    return take;
}

static NOINLINE s32 mp4RingFillFromCb(u32 *ring, s64 off, s32 count)
{
    s32 free_sp = (s32)(ring[0] - ring[3]);
    s32 rem = (count < free_sp) ? count : free_sp;
    s32 (*read_cb)(void *, s32, u32, u32, s32, u32) = (s32 (*)(void *, s32, u32, u32, s32, u32))ring[6];
    s32 done = 0;

    while (rem > 0) {
        u32 cap = ring[0];
        u32 wpos = ring[2];
        s32 chunk = ((s32)(cap - wpos) < rem) ? (s32)(cap - wpos) : rem;
        if (read_cb == NULL)
            return SCE_MP4_ERROR_INVALID_VALUE;
        u32 esz = ring[4];
        s64 cur_off = off + (s64)done * (s64)esz;
        s32 r = read_cb((void *)(ring[5] + wpos * esz), (s32)(wpos * esz), (u32)cur_off, (u32)(cur_off >> 32), (s32)((u32)chunk * esz), ring[7]);
        if (r < 0)
            return r;
        s32 elems = r / (s32)esz;
        wpos += (u32)elems;
        if (wpos == cap)
            wpos = 0;
        ring[2] = wpos;
        ring[3] += (u32)elems;
        rem -= elems;
        done += elems;
    }
    return done;
}

static NOINLINE s32 mp4RingWrite(u32 *ring, s32 count, const u8 *src)
{
    mp4GetGlobalCopyCtx();
    s32 free_sp = (s32)(ring[0] - ring[3]);
    s32 rem = (count < free_sp) ? count : free_sp;
    s32 done = 0;
    while (rem > 0) {
        u32 cap = ring[0];
        u32 wpos = ring[2];
        u32 esz = ring[4];
        s32 chunk = ((s32)(cap - wpos) < rem) ? (s32)(cap - wpos) : rem;
        mp4MemcpyWrap((u8 *)ring[5] + wpos * esz, src + (u32)done * esz, (u32)chunk * esz);
        wpos += (u32)chunk;
        if (wpos == cap)
            wpos = 0;
        ring[2] = wpos;
        ring[3] += (u32)chunk;
        rem -= chunk;
        done += chunk;
    }
    return done;
}

static NOINLINE s32 mp4RingInit(u32 **mp4, u32 *ring, s32 elem_size, s32 cap, u32 buf, void *read_cb, void *cb_arg)
{
    ring[0] = (u32)cap;
    ring[1] = 0;
    ring[2] = 0;
    ring[3] = 0;
    ring[4] = (u32)elem_size;
    ring[5] = buf;
    ring[6] = (u32)read_cb;
    ring[7] = (u32)cb_arg;
    ring[9] = (u32)mp4;
    return 0;
}

static NOINLINE s32 mp4RingTerm(u32 *ring) { (void)ring; return 0; }
static NOINLINE s32 mp4RingGetUsed(const u32 *ring) { return (s32)ring[3]; }
static NOINLINE s32 mp4RingReset(u32 *ring) { ring[1] = 0; ring[2] = 0; ring[3] = 0; return 0; }
static NOINLINE s32 mp4RingGetFree(const u32 *ring) { return (s32)(ring[0] - ring[3]); }
static NOINLINE s32 mp4RingConsume(u32 *ring, s32 count, u8 *dst, s32 advance) { return mp4RingRead(ring, count, dst, advance); }

static NOINLINE s32 mp4AacTermPool(void)
{
    if (g_aacPoolBase == NULL || g_aacSlotCount == 0 || g_aacFplId == -1)
        return 0;
    for (s32 i = 0; i < g_aacSlotCount; i++) {
        u32 *slot = (u32 *)((u8 *)g_aacPoolBase + i * MP4_AAC_SLOT_SIZE);
        if (slot[32] == 1)
            return SCE_MP4_ERROR_AAC_FREE_FAIL;
    }
    if (sceKernelFreeFpl(g_aacFplId, g_aacPoolBase) < 0)
        return SCE_MP4_ERROR_AAC_FREE_FAIL;
    g_aacPoolBase = NULL;
    if (sceKernelDeleteFpl(g_aacFplId) < 0)
        return SCE_MP4_ERROR_AAC_FREE_FAIL;
    g_aacFplId = -1;
    return 0;
}

static NOINLINE s32 mp4AacReleaseSlot(s32 slot_idx)
{
    if (g_aacPoolBase == NULL || g_aacSlotCount == 0 || slot_idx < 0 || slot_idx >= g_aacSlotCount)
        return SCE_MP4_ERROR_AAC_NOT_INIT;
    u32 *slot = (u32 *)((u8 *)g_aacPoolBase + slot_idx * MP4_AAC_SLOT_SIZE);
    s32 intr = sceKernelCpuSuspendIntr();
    if ((s32)slot[32] > 0)
        slot[32] = 0;
    sceKernelCpuResumeIntr(intr);
    return 0;
}

static NOINLINE s32 mp4AacInitResourceWrap(s32 count) { return mp4AacInitPool(count); }
static NOINLINE s32 mp4AacTermResourceWrap(void) { return mp4AacTermPool(); }

static NOINLINE s32 mp4AacOpenSlot(s32 *out_slot, s32 sample_rate)
{
    s32 s = mp4AacAcquireSlot(sample_rate);
    if (s < 0)
        return s;
    *out_slot = s;
    return 0;
}

static NOINLINE s32 mp4AacCloseSlot(const s32 *slot) { return mp4AacReleaseSlot(*slot); }

static NOINLINE s32 mp4AacDecodeSlotWrap(s32 *slot, const u32 *au, u32 pcm_out, s32 reinit, s32 sample_rate)
{
    if (reinit == 1) {
        mp4AacReleaseSlot(*slot);
        s32 s = mp4AacAcquireSlot(sample_rate);
        if (s < 0)
            return s;
        *slot = s;
    }
    return mp4AacDecodeFrame(*slot, au, pcm_out);
}

static NOINLINE s32 mp4AacInitPool(s32 count)
{
    if (g_aacPoolBase != NULL)
        return SCE_MP4_ERROR_AAC_ALREADY_INIT;
    s32 total = count * MP4_AAC_SLOT_SIZE;
    s32 fpl = sceKernelCreateFpl("SceMp4AacResource", 2, 0, (u32)total, 1, NULL);
    g_aacFplId = fpl;
    if (fpl < 0)
        return SCE_MP4_ERROR_AAC_ALLOC_FAIL;
    if (sceKernelAllocateFpl(fpl, &g_aacPoolBase, NULL) < 0) {
        sceKernelDeleteFpl(g_aacFplId);
        g_aacFplId = -1;
        g_aacPoolBase = NULL;
        return SCE_MP4_ERROR_AAC_ALLOC_FAIL;
    }
    u8 *p = (u8 *)g_aacPoolBase;
    for (s32 i = 0; i < total; i++)
        p[i] = 0;
    g_aacSlotCount = count;
    return 0;
}

static NOINLINE s32 mp4AacAcquireSlot(s32 sample_rate)
{
    if (sample_rate != 24000 && sample_rate != 32000 && sample_rate != 44100 && sample_rate != 48000)
        return SCE_MP4_ERROR_INVALID_VALUE;
    s32 intr = sceKernelCpuSuspendIntr();
    s32 found = -1;
    u32 *slot = NULL;
    for (s32 i = 0; i < g_aacSlotCount; i++) {
        u32 *cur = (u32 *)((u8 *)g_aacPoolBase + i * MP4_AAC_SLOT_SIZE);
        if (cur[32] == 0) {
            found = i;
            slot = cur;
            cur[32] = 1;
            break;
        }
    }
    sceKernelCpuResumeIntr(intr);
    if (found < 0)
        return SCE_MP4_ERROR_AAC_NO_SLOT;
    slot[32] = 2;
    s32 r = mp4AacInitCodecSlot(found, sample_rate);
    if (r < 0) {
        mp4AacReleaseSlot(found);
        return r;
    }
    slot[32] = 3;
    return found;
}

static NOINLINE s32 mp4AacDecodeFrame(s32 slot_idx, const u32 *au, u32 pcm_out)
{
    if (slot_idx < 0 || slot_idx >= g_aacSlotCount)
        return SCE_MP4_ERROR_AAC_INVALID_ID;
    if (g_aacPoolBase == NULL || g_aacSlotCount == 0)
        return SCE_MP4_ERROR_AAC_NOT_INIT;
    u32 *slot = (u32 *)((u8 *)g_aacPoolBase + slot_idx * MP4_AAC_SLOT_SIZE);
    if ((s32)slot[32] < 3)
        return SCE_MP4_ERROR_AAC_NOT_READY;
    slot[6] = au[4];
    slot[8] = pcm_out;
    if (sceAudiocodecDecode(slot, 0x1003) < 0)
        return SCE_MP4_ERROR_AAC_DECODE_FAIL;
    if (slot[9] == 0)
        sceKernelMemset((void *)slot[8], 0, 4096);
    return 0;
}

static NOINLINE s32 mp4AacInitCodecSlot(s32 slot_idx, s32 sample_rate)
{
    if (g_aacPoolBase == NULL || g_aacSlotCount == 0 || slot_idx < 0 || slot_idx >= g_aacSlotCount)
        return SCE_MP4_ERROR_AAC_NOT_INIT;
    u32 *slot = (u32 *)((u8 *)g_aacPoolBase + slot_idx * MP4_AAC_SLOT_SIZE);
    slot[3] = (u32)((u8 *)slot + 136);
    slot[4] = 102176;
    slot[5] = 1;
    slot[10] = (u32)sample_rate;
    ((u8 *)slot)[44] = 0;
    ((u8 *)slot)[45] = 0;
    s32 r = sceAudiocodecInit(slot, 0x1003);
    return (r < 0) ? r : 0;
}

static NOINLINE s32 mp4DeleteGlobalMutexes(void)
{
    s32 ret = 0;
    if ((g_mp4MutexFlags & 2) != 0) {
        s32 r = mp4DeleteLwMutex(g_mp4AacMutex);
        g_mp4MutexFlags &= ~2U;
        if (r < 0)
            ret = r;
    }
    if ((g_mp4MutexFlags & 1) != 0) {
        s32 r = mp4DeleteLwMutex(g_mp4MainMutex);
        g_mp4MutexFlags &= ~1U;
        if (r < 0)
            ret = r;
    }
    return ret;
}

static NOINLINE s32 mp4DeleteLwMutex(void *work) { return sceKernelDeleteLwMutex(work); }

static NOINLINE s32 mp4CreateGlobalMutexes(void)
{
    s32 r = sceKernelCreateLwMutex(g_mp4MainMutex, "SceMp4", 0x300, 0, NULL);
    if (r >= 0) {
        g_mp4MutexFlags |= 1U;
        r = sceKernelCreateLwMutex(g_mp4AacMutex, "SceMp4AacDec", 0x300, 0, NULL);
        if (r >= 0)
            g_mp4MutexFlags |= 2U;
    }
    return r;
}

static NOINLINE s32 mp4CreateLwMutex(void *work, const char *name, s32 init_cnt)
{
    return sceKernelCreateLwMutex(work, name, 0x300, init_cnt, NULL);
}

static NOINLINE s32 mp4LockLwMutex(void *work) { return sceKernelLockLwMutex(work, 1, NULL); }
static NOINLINE s32 mp4UnlockLwMutex(void *work) { return sceKernelUnlockLwMutex(work, 1); }

static NOINLINE s32 mp4IsInitialized(void)
{
    return (g_mp4LibMagic == MP4_MAGIC_LIB) ? 0 : SCE_MP4_ERROR_NOT_INITIALIZED;
}

static NOINLINE s32 mp4InitHeap(u32 size)
{
    if (g_mp4LibMagic == MP4_MAGIC_LIB)
        return SCE_MP4_ERROR_ALREADY_INITED;
    g_mp4HeapHandle = mp4UserHeapCreate("SceMp4DemuxRes", size, 0);
    if (g_mp4HeapHandle == NULL)
        return SCE_MP4_ERROR_NOT_INITIALIZED;
    g_mp4LibMagic = MP4_MAGIC_LIB;
    g_mp4LibVersion = 0x31303030U;
    g_mp4HeapSize = size;
    return 0;
}

static NOINLINE s32 mp4TermHeap(void)
{
    if (g_mp4LibMagic != MP4_MAGIC_LIB)
        return SCE_MP4_ERROR_NOT_INITIALIZED;
    s32 r = mp4UserHeapDelete(g_mp4HeapHandle);
    g_mp4LibMagic = 0;
    g_mp4LibVersion = 0;
    g_mp4HeapHandle = NULL;
    g_mp4HeapSize = 0;
    return r;
}

static NOINLINE void *mp4HeapAllocWrap(u32 *heap, u32 size) { return mp4UserHeapAlloc(heap, size); }
static NOINLINE s32 mp4HeapFreeWrap(u32 *heap, void *ptr) { return mp4UserHeapFree(heap, ptr); }

/* Public sceMp4 exports */
s32 sceMp4_B1221EE7(u32 **mp4, const u32 *cbs, u32 buf, s32 buf_size)
{
    if (_sceKernelCheckThreadStack() < 4064) return SCE_MP4_ERROR_STACK_OVERFLOW;
    s32 ret = mp4IsInitialized();
    if (ret < 0) return ret;
    if (mp4 == NULL) return SCE_MP4_ERROR_INVALID_VALUE;
    if (mp4ValidateHandle(mp4) == 0) return SCE_MP4_ERROR_ALREADY_INITED;
    mp4LockLwMutex(g_mp4MainMutex);
    ret = mp4CreateInternal(mp4, cbs, buf, buf_size);
    mp4UnlockLwMutex(g_mp4MainMutex);
    return ret;
}

s32 sceMp4_538C2057(u32 **mp4)
{
    if (_sceKernelCheckThreadStack() < 4080) return SCE_MP4_ERROR_STACK_OVERFLOW;
    s32 ret = mp4IsInitialized();
    if (ret < 0) return ret;
    ret = mp4ValidateHandle(mp4);
    if (ret < 0) return ret;
    mp4LockLwMutex(g_mp4MainMutex);
    mp4LockLwMutex(mp4GetSampleBufMutex(mp4));
    mp4LockLwMutex(mp4GetDataMutex(mp4));
    ret = mp4DeleteInternal(mp4);
    mp4UnlockLwMutex(g_mp4MainMutex);
    return ret;
}

s32 sceMp4_131BDE57(u32 **mp4, u32 buf, u32 *au)
{
    if (_sceKernelCheckThreadStack() < 4080) return SCE_MP4_ERROR_STACK_OVERFLOW;
    s32 ret = mp4IsInitialized();
    if (ret < 0) return ret;
    ret = mp4ValidateHandle(mp4);
    if (ret < 0) return ret;
    mp4LockLwMutex(mp4GetDataMutex(mp4));
    ret = mp4InitAuInternal(mp4, buf, au);
    mp4UnlockLwMutex(mp4GetDataMutex(mp4));
    return ret;
}

s32 sceMp4_7443AF1D(u32 **mp4, u32 *out_info)
{
    if (_sceKernelCheckThreadStack() < 4080) return SCE_MP4_ERROR_STACK_OVERFLOW;
    s32 ret = mp4IsInitialized();
    if (ret < 0) return ret;
    ret = mp4ValidateHandle(mp4);
    if (ret < 0) return ret;
    mp4LockLwMutex(mp4GetDataMutex(mp4));
    ret = mp4GetMovieInfoInternal(mp4, out_info);
    mp4UnlockLwMutex(mp4GetDataMutex(mp4));
    return ret;
}

s32 sceMp4_5EB65F26(u32 **mp4, u32 type)
{
    if (_sceKernelCheckThreadStack() < 4080) return SCE_MP4_ERROR_STACK_OVERFLOW;
    s32 ret = mp4IsInitialized();
    if (ret < 0) return ret;
    ret = mp4ValidateHandle(mp4);
    if (ret < 0) return ret;
    mp4LockLwMutex(mp4GetDataMutex(mp4));
    ret = mp4CountTracksInternal(mp4, type);
    mp4UnlockLwMutex(mp4GetDataMutex(mp4));
    return ret;
}

s32 sceMp4_7ADFD01C(u32 **mp4, u32 type, s32 idx, const u32 *cbs, u32 *track)
{
    if (_sceKernelCheckThreadStack() < 4064) return SCE_MP4_ERROR_STACK_OVERFLOW;
    s32 ret = mp4IsInitialized();
    if (ret < 0) return ret;
    ret = mp4ValidateHandle(mp4);
    if (ret < 0) return ret;
    mp4LockLwMutex(mp4GetDataMutex(mp4));
    ret = mp4RegistTrackInternal(mp4, type, idx, cbs, track);
    mp4UnlockLwMutex(mp4GetDataMutex(mp4));
    return ret;
}

s32 sceMp4_6710FE77(u32 **mp4, u32 *track)
{
    if (_sceKernelCheckThreadStack() < 4080) return SCE_MP4_ERROR_STACK_OVERFLOW;
    s32 ret = mp4IsInitialized();
    if (ret < 0) return ret;
    ret = mp4ValidateHandle(mp4);
    if (ret < 0) return ret;
    mp4LockLwMutex(mp4GetDataMutex(mp4));
    ret = mp4UnregistTrackInternal(mp4, track);
    mp4UnlockLwMutex(mp4GetDataMutex(mp4));
    return ret;
}

s32 sceMp4_0F0187D2(u32 **mp4, u32 *track, u32 *out_info)
{
    if (_sceKernelCheckThreadStack() < 4080) return SCE_MP4_ERROR_STACK_OVERFLOW;
    s32 ret = mp4IsInitialized();
    if (ret < 0) return ret;
    ret = mp4ValidateHandle(mp4);
    if (ret < 0) return ret;
    mp4LockLwMutex(mp4GetDataMutex(mp4));
    ret = mp4GetAvcTrackInfoInternal(mp4, track, out_info);
    mp4UnlockLwMutex(mp4GetDataMutex(mp4));
    return ret;
}

s32 sceMp4_9CE6F5CF(u32 **mp4, u32 *track, u32 *out_info)
{
    if (_sceKernelCheckThreadStack() < 4080) return SCE_MP4_ERROR_STACK_OVERFLOW;
    s32 ret = mp4IsInitialized();
    if (ret < 0) return ret;
    ret = mp4ValidateHandle(mp4);
    if (ret < 0) return ret;
    mp4LockLwMutex(mp4GetDataMutex(mp4));
    ret = mp4GetAacTrackInfoInternal(mp4, track, out_info);
    mp4UnlockLwMutex(mp4GetDataMutex(mp4));
    return ret;
}

s32 sceMp4_427BEF7F(u32 **mp4, u32 *track)
{
    if (_sceKernelCheckThreadStack() < 4080) return SCE_MP4_ERROR_STACK_OVERFLOW;
    s32 ret = mp4IsInitialized();
    if (ret < 0) return ret;
    ret = mp4ValidateHandle(mp4);
    if (ret < 0) return ret;
    mp4LockLwMutex(mp4GetDataMutex(mp4));
    ret = mp4GetEditListCountInternal(mp4, track);
    mp4UnlockLwMutex(mp4GetDataMutex(mp4));
    return ret;
}

s32 sceMp4_28CCB940(u32 **mp4, u32 *track, u32 *out_list, s32 max_entries)
{
    if (_sceKernelCheckThreadStack() < 4064) return SCE_MP4_ERROR_STACK_OVERFLOW;
    s32 ret = mp4IsInitialized();
    if (ret < 0) return ret;
    ret = mp4ValidateHandle(mp4);
    if (ret < 0) return ret;
    mp4LockLwMutex(mp4GetDataMutex(mp4));
    ret = mp4GetEditListInternal(mp4, track, out_list, max_entries);
    mp4UnlockLwMutex(mp4GetDataMutex(mp4));
    return ret;
}

s32 sceMp4_113E9E7B(u32 **mp4, const u32 *track)
{
    if (_sceKernelCheckThreadStack() < 4080) return SCE_MP4_ERROR_STACK_OVERFLOW;
    s32 ret = mp4IsInitialized();
    if (ret < 0) return ret;
    ret = mp4ValidateHandle(mp4);
    if (ret < 0) return ret;
    mp4LockLwMutex(mp4GetDataMutex(mp4));
    ret = mp4GetMetaCountInternal(mp4, track);
    mp4UnlockLwMutex(mp4GetDataMutex(mp4));
    return ret;
}

s32 sceMp4_4FB5B756(u32 **mp4, const u32 *track, s32 idx, u8 *out_info)
{
    u32 handle = 0;
    if (_sceKernelCheckThreadStack() < 4064) return SCE_MP4_ERROR_STACK_OVERFLOW;
    s32 ret = mp4IsInitialized();
    if (ret < 0) return ret;
    ret = mp4ValidateHandle(mp4);
    if (ret < 0) return ret;
    mp4LockLwMutex(mp4GetDataMutex(mp4));
    ret = mp4GetMetaInfoInternal(mp4, track, idx, &handle, out_info);
    mp4UnlockLwMutex(mp4GetDataMutex(mp4));
    return ret;
}

s32 sceMp4_D2AC9A7E(u32 **mp4, const u32 *track, s32 idx, void *out_buf, u32 buf_size)
{
    if (_sceKernelCheckThreadStack() < 4064) return SCE_MP4_ERROR_STACK_OVERFLOW;
    s32 ret = mp4IsInitialized();
    if (ret < 0) return ret;
    ret = mp4ValidateHandle(mp4);
    if (ret < 0) return ret;
    mp4LockLwMutex(mp4GetDataMutex(mp4));
    ret = mp4GetMetaDataInternal(mp4, track, idx, out_buf, buf_size);
    mp4UnlockLwMutex(mp4GetDataMutex(mp4));
    return ret;
}

s32 sceMp4_B4B400D1(u32 **mp4, u32 *track, const u32 *ts)
{
    if (_sceKernelCheckThreadStack() < 4080) return SCE_MP4_ERROR_STACK_OVERFLOW;
    s32 ret = mp4IsInitialized();
    if (ret < 0) return ret;
    ret = mp4ValidateHandle(mp4);
    if (ret < 0) return ret;
    mp4LockLwMutex(mp4GetDataMutex(mp4));
    ret = mp4GetSampleNumWithTsInternal(mp4, track, ts);
    mp4UnlockLwMutex(mp4GetDataMutex(mp4));
    return ret;
}

s32 sceMp4_74A1CA3E(u32 **mp4, u32 *track, s32 dir, s32 sample)
{
    if (_sceKernelCheckThreadStack() < 4064) return SCE_MP4_ERROR_STACK_OVERFLOW;
    s32 ret = mp4IsInitialized();
    if (ret < 0) return ret;
    ret = mp4ValidateHandle(mp4);
    if (ret < 0) return ret;
    mp4LockLwMutex(mp4GetDataMutex(mp4));
    ret = mp4SearchSyncSampleInternal(mp4, track, dir, sample);
    mp4UnlockLwMutex(mp4GetDataMutex(mp4));
    return ret;
}

s32 sceMp4_9C8F4FC1(u32 **mp4, u32 *track, u32 buf, s32 buf_size, s32 max_samples, s32 avg_size, s32 max_size, s32 cache_size)
{
    if (_sceKernelCheckThreadStack() < 4048) return SCE_MP4_ERROR_STACK_OVERFLOW;
    s32 ret = mp4IsInitialized();
    if (ret < 0) return ret;
    ret = mp4ValidateHandle(mp4);
    if (ret < 0) return ret;
    mp4LockLwMutex(mp4GetDataMutex(mp4));
    ret = mp4SampleBufConstructInternal(mp4, track, buf, buf_size, max_samples, avg_size, max_size, cache_size);
    mp4UnlockLwMutex(mp4GetDataMutex(mp4));
    return ret;
}

s32 sceMp4_01C76489(u32 **mp4, u32 *track)
{
    if (_sceKernelCheckThreadStack() < 4080) return SCE_MP4_ERROR_STACK_OVERFLOW;
    s32 ret = mp4IsInitialized();
    if (ret < 0) return ret;
    ret = mp4ValidateHandle(mp4);
    if (ret < 0) return ret;
    mp4LockLwMutex(mp4GetDataMutex(mp4));
    ret = mp4SampleBufDestructInternal(mp4, track);
    mp4UnlockLwMutex(mp4GetDataMutex(mp4));
    return ret;
}

s32 sceMp4_496E8A65(u32 **mp4, u32 *track)
{
    if (_sceKernelCheckThreadStack() < 4080) return SCE_MP4_ERROR_STACK_OVERFLOW;
    s32 ret = mp4IsInitialized();
    if (ret < 0) return ret;
    ret = mp4ValidateHandle(mp4);
    if (ret < 0) return ret;
    mp4LockLwMutex(mp4GetDataMutex(mp4));
    ret = mp4SampleBufFlushInternal(mp4, track);
    mp4UnlockLwMutex(mp4GetDataMutex(mp4));
    return ret;
}

s32 sceMp4_8754ECB8(u32 **mp4, u32 *track, s32 *out_samples, s32 *out_bytes)
{
    if (_sceKernelCheckThreadStack() < 4064) return SCE_MP4_ERROR_STACK_OVERFLOW;
    s32 ret = mp4IsInitialized();
    if (ret < 0) return ret;
    ret = mp4ValidateHandle(mp4);
    if (ret < 0) return ret;
    mp4LockLwMutex(mp4GetDataMutex(mp4));
    ret = mp4SampleBufAvailableInternal(mp4, track, out_samples, out_bytes);
    mp4UnlockLwMutex(mp4GetDataMutex(mp4));
    return ret;
}

s32 sceMp4_31BCD7E0(u32 **mp4, u32 *track)
{
    if (_sceKernelCheckThreadStack() < 4080) return SCE_MP4_ERROR_STACK_OVERFLOW;
    s32 ret = mp4IsInitialized();
    if (ret < 0) return ret;
    ret = mp4ValidateHandle(mp4);
    if (ret < 0) return ret;
    mp4LockLwMutex(mp4GetSampleBufMutex(mp4));
    mp4LockLwMutex(mp4GetDataMutex(mp4));
    ret = mp4SampleBufPutInternal(mp4, track);
    mp4UnlockLwMutex(mp4GetDataMutex(mp4));
    mp4UnlockLwMutex(mp4GetSampleBufMutex(mp4));
    return ret;
}

s32 sceMp4_503A3CBA(u32 **mp4, u32 *track, u32 *au, u32 *out_info)
{
    if (_sceKernelCheckThreadStack() < 4064) return SCE_MP4_ERROR_STACK_OVERFLOW;
    s32 ret = mp4IsInitialized();
    if (ret < 0) return ret;
    ret = mp4ValidateHandle(mp4);
    if (ret < 0) return ret;
    mp4LockLwMutex(mp4GetDataMutex(mp4));
    ret = mp4GetAvcAuInternal(mp4, track, au, out_info);
    mp4UnlockLwMutex(mp4GetDataMutex(mp4));
    return ret;
}

s32 sceMp4_5601A6F0(u32 **mp4, u32 *track, u32 *au, u32 *out_info)
{
    if (_sceKernelCheckThreadStack() < 4064) return SCE_MP4_ERROR_STACK_OVERFLOW;
    s32 ret = mp4IsInitialized();
    if (ret < 0) return ret;
    ret = mp4ValidateHandle(mp4);
    if (ret < 0) return ret;
    mp4LockLwMutex(mp4GetDataMutex(mp4));
    ret = mp4GetAacAuInternal(mp4, track, au, out_info);
    mp4UnlockLwMutex(mp4GetDataMutex(mp4));
    return ret;
}

s32 sceMp4_D8250B75(u32 **mp4, u32 *track, u32 sample)
{
    if (_sceKernelCheckThreadStack() < 4080) return SCE_MP4_ERROR_STACK_OVERFLOW;
    s32 ret = mp4IsInitialized();
    if (ret < 0) return ret;
    ret = mp4ValidateHandle(mp4);
    if (ret < 0) return ret;
    mp4LockLwMutex(mp4GetDataMutex(mp4));
    ret = mp4PutSampleNumInternal(mp4, track, sample);
    mp4UnlockLwMutex(mp4GetDataMutex(mp4));
    return ret;
}

s32 sceMp4_A6C724DC(u32 **mp4, u32 *track)
{
    if (_sceKernelCheckThreadStack() < 4080) return SCE_MP4_ERROR_STACK_OVERFLOW;
    s32 ret = mp4IsInitialized();
    if (ret < 0) return ret;
    ret = mp4ValidateHandle(mp4);
    if (ret < 0) return ret;
    mp4LockLwMutex(mp4GetDataMutex(mp4));
    ret = mp4GetSampleNumInternal(mp4, track);
    mp4UnlockLwMutex(mp4GetDataMutex(mp4));
    return ret;
}

s32 sceMp4_F7C51EC1(u32 **mp4, u32 *track, s32 sample, u32 *out_info)
{
    if (_sceKernelCheckThreadStack() < 4064) return SCE_MP4_ERROR_STACK_OVERFLOW;
    s32 ret = mp4IsInitialized();
    if (ret < 0) return ret;
    ret = mp4ValidateHandle(mp4);
    if (ret < 0) return ret;
    mp4LockLwMutex(mp4GetDataMutex(mp4));
    ret = mp4GetSampleInfoInternal(mp4, track, sample, out_info);
    mp4UnlockLwMutex(mp4GetDataMutex(mp4));
    return ret;
}

s32 sceMp4_3069C2B5(u32 **mp4, u32 *track, s32 sample, u32 *out_param)
{
    if (_sceKernelCheckThreadStack() < 4064) return SCE_MP4_ERROR_STACK_OVERFLOW;
    s32 ret = mp4IsInitialized();
    if (ret < 0) return ret;
    ret = mp4ValidateHandle(mp4);
    if (ret < 0) return ret;
    mp4LockLwMutex(mp4GetDataMutex(mp4));
    ret = mp4GetAvcParamSetInternal(mp4, track, sample, out_param);
    mp4UnlockLwMutex(mp4GetDataMutex(mp4));
    return ret;
}

s32 sceMp4_17EAA97D(u32 **mp4, u32 *track, s32 sample, u32 es_buf, u32 es_size, u32 *au)
{
    if (_sceKernelCheckThreadStack() < 4064) return SCE_MP4_ERROR_STACK_OVERFLOW;
    s32 ret = mp4IsInitialized();
    if (ret < 0) return ret;
    ret = mp4ValidateHandle(mp4);
    if (ret < 0) return ret;
    mp4LockLwMutex(mp4GetDataMutex(mp4));
    ret = mp4GetAvcAuDirectInternal(mp4, track, sample, es_buf, es_size, au);
    mp4UnlockLwMutex(mp4GetDataMutex(mp4));
    return ret;
}

s32 sceMp4_532029B8(u32 **mp4, u32 *track, s32 sample, u32 es_buf, u32 es_size, u32 *au)
{
    if (_sceKernelCheckThreadStack() < 4064) return SCE_MP4_ERROR_STACK_OVERFLOW;
    s32 ret = mp4IsInitialized();
    if (ret < 0) return ret;
    ret = mp4ValidateHandle(mp4);
    if (ret < 0) return ret;
    mp4LockLwMutex(mp4GetDataMutex(mp4));
    ret = mp4GetAacAuDirectInternal(mp4, track, sample, es_buf, es_size, au);
    mp4UnlockLwMutex(mp4GetDataMutex(mp4));
    return ret;
}

s32 sceMp4_4ED4AB1E(s32 count)
{
    if (_sceKernelCheckThreadStack() < 4080) return SCE_MP4_ERROR_STACK_OVERFLOW;
    s32 ret = mp4IsInitialized();
    if (ret < 0) return ret;
    mp4LockLwMutex(g_mp4AacMutex);
    ret = mp4AacInitResourceWrap(count);
    mp4UnlockLwMutex(g_mp4AacMutex);
    return ret;
}

s32 sceMp4_7D332394(void)
{
    if (_sceKernelCheckThreadStack() < 4080) return SCE_MP4_ERROR_STACK_OVERFLOW;
    s32 ret = mp4IsInitialized();
    if (ret < 0) return ret;
    mp4LockLwMutex(g_mp4AacMutex);
    ret = mp4AacTermResourceWrap();
    mp4UnlockLwMutex(g_mp4AacMutex);
    return ret;
}

s32 sceMp4_10EE0D2C(s32 *out_slot)
{
    if (_sceKernelCheckThreadStack() < 4080) return SCE_MP4_ERROR_STACK_OVERFLOW;
    s32 ret = mp4IsInitialized();
    if (ret < 0) return ret;
    mp4LockLwMutex(g_mp4AacMutex);
    ret = mp4AacOpenSlot(out_slot, 48000);
    mp4UnlockLwMutex(g_mp4AacMutex);
    return ret;
}

s32 sceMp4_5D72B333(const s32 *slot)
{
    if (_sceKernelCheckThreadStack() < 4080) return SCE_MP4_ERROR_STACK_OVERFLOW;
    s32 ret = mp4IsInitialized();
    if (ret < 0) return ret;
    mp4LockLwMutex(g_mp4AacMutex);
    ret = mp4AacCloseSlot(slot);
    mp4UnlockLwMutex(g_mp4AacMutex);
    return ret;
}

s32 sceMp4_7663CB5C(s32 *slot, const u32 *au, u32 pcm_out, s32 reinit, s32 sample_rate)
{
    if (_sceKernelCheckThreadStack() < 4064) return SCE_MP4_ERROR_STACK_OVERFLOW;
    s32 ret = mp4IsInitialized();
    if (ret < 0) return ret;
    mp4LockLwMutex(g_mp4AacMutex);
    ret = mp4AacDecodeSlotWrap(slot, au, pcm_out, reinit, sample_rate);
    mp4UnlockLwMutex(g_mp4AacMutex);
    return ret;
}

s32 sceMp4_BCA9389C(u32 **mp4, s32 max_samples, s32 avg_size, s32 max_size, s32 cache_size)
{
    if (_sceKernelCheckThreadStack() < 4064) return SCE_MP4_ERROR_STACK_OVERFLOW;
    s32 ret = mp4IsInitialized();
    if (ret < 0) return ret;
    return mp4QuerySampleBufSizeInternal(mp4, max_samples, avg_size, max_size, cache_size);
}

static NOINLINE s32 mp4GetMetaCountInternal(u32 **mp4, const u32 *track)
{
    u32 trk = (track != NULL) ? track[8] : 0;
    s32 count = 0;
    s32 r = mp4msv_EF6D76BC((*mp4)[1], trk, &count);
    if (r == 0)
        return count;
    return (r == 8195) ? SCE_MP4_ERROR_BAD_DATA : SCE_MP4_ERROR_INVALID_VALUE;
}

static NOINLINE s32 mp4GetMetaInfoInternal(u32 **mp4, const u32 *track, s32 idx, u32 *out_handle, u8 *out_info)
{
    u32 trk = (track != NULL) ? track[8] : 0;
    u16 len = 0;
    s32 count = mp4GetMetaCountInternal(mp4, track);
    if (count < 0)
        return count;
    if (idx >= count || out_info == NULL)
        return SCE_MP4_ERROR_INVALID_VALUE;
    s32 r = mp4msv_4C677A7E((*mp4)[1], trk, (u32)(idx + 1), out_handle, out_info + 4, out_info + 12, out_info + 14, &len);
    *(u32 *)(out_info + 16) = len;
    if (r != 0)
        return (r == 8195) ? SCE_MP4_ERROR_BAD_DATA : SCE_MP4_ERROR_INVALID_VALUE;
    u16 lang = *(const u16 *)(out_info + 12);
    *(u32 *)out_info = (u32)idx;
    out_info[8] = (u8)(lang >> 15);
    *(u16 *)(out_info + 12) = (u16)(lang & 0x7FFFU);
    out_info[9] = 0;
    out_info[10] = 0;
    out_info[11] = 0;
    return 0;
}

static NOINLINE s32 mp4GetMetaDataInternal(u32 **mp4, const u32 *track, s32 idx, void *out_buf, u32 buf_size)
{
    u8 info[32];
    u32 handle = 0;
    if (out_buf == NULL || (s32)buf_size < 0)
        return SCE_MP4_ERROR_INVALID_VALUE;
    s32 ret = mp4GetMetaInfoInternal(mp4, track, idx, &handle, info);
    if (ret < 0)
        return ret;
    if (*(const u32 *)(info + 16) < buf_size)
        return SCE_MP4_ERROR_INVALID_VALUE;
    s32 r = mp4msv_9632797F(handle, 0, 0, out_buf);
    if (r != 0)
        return (r == 8195) ? SCE_MP4_ERROR_BAD_DATA : SCE_MP4_ERROR_INVALID_VALUE;
    return 0;
}

static NOINLINE s32 mp4StrCmp(const char *s1, const char *s2)
{
    if (s1 == NULL || s2 == NULL) {
        if (s1 == s2) return 0;
        return (s1 != NULL) ? 1 : -1;
    }
    while (*s1 == *s2) {
        if (*s1 == '\0')
            return 0;
        s1++;
        s2++;
    }
    return (s32)*s1 - (s32)*s2;
}

static NOINLINE u32 mp4ReadBeUint(const u8 *p, s32 len)
{
    switch (len) {
    case 1: return p[0];
    case 2: return ((u32)p[0] << 8) | p[1];
    case 3: return ((u32)p[0] << 16) | ((u32)p[1] << 8) | p[2];
    case 4: return ((u32)p[0] << 24) | ((u32)p[1] << 16) | ((u32)p[2] << 8) | p[3];
    default: return 0;
    }
}

/* Userland FPL-backed heap allocator */
static NOINLINE u32 *mp4UserHeapCreate(const char *name, u32 size, u16 attr)
{
    u32 aligned = (size + 255U) & ~255U;
    s32 fpl_attr = (attr & 0x4000) ? 0x4000 : 0;
    s32 fpl = sceKernelCreateFpl(name, 2, fpl_attr, aligned, 1, NULL);
    if (fpl < 0)
        return NULL;
    void *mem = NULL;
    if (sceKernelTryAllocateFpl(fpl, &mem) < 0) {
        sceKernelDeleteFpl(fpl);
        return NULL;
    }
    u32 *heap = (u32 *)mem;
    heap[1] = (u32)fpl;
    heap[2] = ((attr & 1) ? aligned : 0U) | ((attr & 0x4000) ? 1U : 0U);
    char *dst_name = (char *)&heap[3];
    s32 i = 0;
    while (name[i] != '\0' && i < 31) {
        dst_name[i] = name[i];
        i++;
    }
    dst_name[i] = '\0';
    heap[11] = aligned;
    heap[12] = 0;
    heap[13] = 1;
    if (sceKernelCreateLwMutex(&heap[14], name, 0x200, 0, NULL) < 0) {
        sceKernelDeleteFpl(fpl);
        return NULL;
    }
    heap[22] = (u32)&heap[22];
    heap[23] = (u32)&heap[22];
    mp4BlockInit(&heap[24], aligned - 96U);
    s32 intr = sceKernelCpuSuspendIntr();
    heap[0] = (u32)((u8 *)heap + 1);
    g_heapInstanceCount++;
    sceKernelCpuResumeIntr(intr);
    return heap;
}

static NOINLINE s32 mp4UserHeapDelete(u32 *heap)
{
    if (heap == NULL || heap[0] != (u32)((u8 *)heap + 1))
        return (s32)0x80000100;
    s32 r = sceKernelLockLwMutex(&heap[14], 1, NULL);
    if (r < 0)
        return r;
    heap[0] = 0;
    sceKernelUnlockLwMutex(&heap[14], 1);
    sceKernelDeleteLwMutex(&heap[14]);
    u32 *cur = (u32 *)heap[22];
    while (1) {
        cur[2] = 0;
        sceKernelDeleteFpl((s32)cur[3]);
        if (cur == &heap[22])
            break;
        cur = (u32 *)cur[0];
    }
    s32 main_fpl = (s32)heap[1];
    heap[24] = 0;
    sceKernelDeleteFpl(main_fpl);
    s32 intr = sceKernelCpuSuspendIntr();
    g_heapInstanceCount--;
    sceKernelCpuResumeIntr(intr);
    return 0;
}

static NOINLINE void *mp4UserHeapAllocOpt(u32 *heap, u32 size, const u32 *opt)
{
    u32 align = 0;
    if (opt != NULL) {
        if (opt[0] < 8U)
            return NULL;
        align = opt[1];
        if (align == 0 || (align & 3U) != 0 || (align & (align - 1U)) != 0)
            return NULL;
    }
    if (heap == NULL || heap[0] != (u32)((u8 *)heap + 1))
        return NULL;
    if (sceKernelLockLwMutex(&heap[14], 1, NULL) < 0)
        return NULL;

    void *ptr = NULL;
    u32 *cur = (u32 *)heap[22];
    while (1) {
        ptr = mp4BlockAlloc(&cur[2], size, align);
        if (ptr != NULL) {
            heap[12]++;
            sceKernelUnlockLwMutex(&heap[14], 1);
            return ptr;
        }
        if (cur == &heap[22])
            break;
        cur = (u32 *)cur[0];
    }

    if ((s32)heap[2] < 4) {
        heap[12]++;
        sceKernelUnlockLwMutex(&heap[14], 1);
        return NULL;
    }

    u32 grow = heap[2] & 0x7FFFFF00U;
    u32 need = (size + 7U) & ~7U;
    if (align != 0) {
        u32 rem = (align - 1U) & 0x28U;
        u32 pad = (rem != 0) ? (align - rem) : 0U;
        if (grow - 48U < need + pad)
            grow = (need + pad + 303U) & ~255U;
    } else {
        if (grow - 48U < size)
            grow = (need + 303U) & ~255U;
    }

    s32 fpl = sceKernelCreateFpl((const char *)&heap[3], 2, (heap[2] & 1U) ? 0x4000 : 0, grow, 1, NULL);
    if (fpl >= 0) {
        void *mem = NULL;
        if (sceKernelTryAllocateFpl(fpl, &mem) >= 0) {
            u32 *seg = (u32 *)mem;
            heap[11] += grow;
            heap[13]++;
            if (seg != NULL) {
                u32 *head = (u32 *)heap[22];
                seg[3] = (u32)fpl;
                seg[0] = (u32)head;
                seg[1] = head[1];
                head[1] = (u32)seg;
                *(u32 *)seg[1] = (u32)seg;
                mp4BlockInit(&seg[2], grow - 8U);
                ptr = mp4BlockAlloc(&seg[2], size, align);
            }
            heap[12]++;
            sceKernelUnlockLwMutex(&heap[14], 1);
            return ptr;
        }
        sceKernelDeleteFpl(fpl);
    }
    sceKernelUnlockLwMutex(&heap[14], 1);
    return NULL;
}

static NOINLINE s32 mp4UserHeapFree(u32 *heap, void *ptr)
{
    if (heap == NULL || heap[0] != (u32)((u8 *)heap + 1))
        return (s32)0x80000100;
    s32 r = sceKernelLockLwMutex(&heap[14], 1, NULL);
    if (r < 0)
        return r;
    if (ptr != NULL) {
        u32 *cur = (u32 *)heap[22];
        while (mp4BlockFree(&cur[2], ptr) != 0) {
            if (cur == &heap[22]) {
                sceKernelUnlockLwMutex(&heap[14], 1);
                return (s32)0x80000103;
            }
            cur = (u32 *)cur[0];
        }
        if (cur != &heap[22] && mp4BlockIsEmpty(&cur[2])) {
            cur[2] = 0;
            u32 *next = (u32 *)cur[0];
            next[1] = cur[1];
            *(u32 *)cur[1] = (u32)next;
            heap[13]--;
            heap[11] -= (cur[4] + 255U) & ~255U;
            sceKernelDeleteFpl((s32)cur[3]);
        }
        heap[12]--;
    }
    sceKernelUnlockLwMutex(&heap[14], 1);
    return 0;
}

static NOINLINE void *mp4UserHeapAlloc(u32 *heap, u32 size)
{
    return mp4UserHeapAllocOpt(heap, size, NULL);
}

static NOINLINE void *mp4BlockAlloc(u32 *arena, u32 size, u32 align)
{
    if (arena == NULL || arena[0] != (u32)((u8 *)arena - 1))
        return NULL;
    if (size < 8U)
        size = 8U;
    s32 units = (s32)(((size + 7U) >> 3) + 1U);
    u32 *start = (u32 *)arena[5];
    u32 *prev = start;
    u32 *cur = (u32 *)prev[0];

    while (1) {
        s32 cur_u = (s32)cur[1];
        if (cur_u >= units) {
            if (cur_u == units) {
                u32 ret_addr = (u32)&cur[2];
                if (align == 0 || (ret_addr % align) == 0) {
                    prev[0] = cur[0];
                    arena[5] = (u32)prev;
                    cur[0] = (u32)arena;
                    arena[3] += cur[1];
                    return (void *)ret_addr;
                }
            } else {
                u32 tail_u = 0;
                s32 need_u = units;
                u32 *link_prev = NULL;
                if (align != 0) {
                    u32 mis = (u32)&cur[2 * (cur_u - units) + 2] % align;
                    if (mis != 0) {
                        tail_u = mis >> 3;
                        need_u = units + (s32)tail_u;
                        if (cur_u < need_u)
                            goto next_blk;
                        link_prev = cur;
                        if (cur_u == need_u) {
                            link_prev = prev;
                            prev[0] = cur[0];
                        }
                    }
                }
                cur[1] = (u32)(cur_u - need_u);
                cur += 2 * (cur_u - need_u);
                u32 ret_addr = (u32)&cur[2];
                cur[1] = (u32)units;
                if (tail_u != 0) {
                    u32 *tail = &cur[2 * units];
                    tail[0] = link_prev[0];
                    tail[1] = tail_u;
                    link_prev[0] = (u32)tail;
                }
                arena[5] = (u32)prev;
                cur[0] = (u32)arena;
                arena[3] += cur[1];
                return (void *)ret_addr;
            }
        }
next_blk:
        if (cur == start)
            return NULL;
        prev = cur;
        cur = (u32 *)cur[0];
    }
}

static NOINLINE s32 mp4BlockFree(u32 *arena, void *ptr)
{
    if (arena == NULL || arena[0] != (u32)((u8 *)arena - 1))
        return -4;
    if (ptr == NULL)
        return -1;
    u32 *blk = (u32 *)ptr - 2;
    if (blk[0] != (u32)arena || (s32)blk[1] <= 0)
        return -1;
    if (blk < &arena[6] || (u8 *)blk >= (u8 *)arena + arena[2])
        return -2;

    u32 *p = (u32 *)arena[5];
    while (!(blk > p && blk < (u32 *)p[0])) {
        if (blk == p)
            return -3;
        if (p >= (u32 *)p[0] && (blk > p || blk < (u32 *)p[0]))
            break;
        p = (u32 *)p[0];
    }
    u32 *next = (u32 *)p[0];
    if (blk < next && next < &blk[2 * blk[1]])
        return -3;
    if (p < blk && blk < &p[2 * p[1]])
        return -3;

    if ((s32)blk[1] >= 2) {
        blk[2] = 0xDEADBEEFU;
        blk[3] = 0xDEADBEEFU;
        if ((s32)p[1] >= 3) {
            u32 *end = &blk[2 * blk[1]];
            end[-1] = 0xDEADBEEFU;
            end[-2] = 0xDEADBEEFU;
        }
    }
    arena[3] -= blk[1];
    if (&blk[2 * blk[1]] == next && (s32)next[1] > 0) {
        blk[1] += next[1];
        blk[0] = next[0];
        next[0] = 0;
        next[1] = 0;
    } else {
        blk[0] = (u32)next;
    }
    if (&p[2 * p[1]] == blk) {
        p[1] += blk[1];
        p[0] = blk[0];
        blk[0] = 0;
        blk[1] = 0;
    } else {
        p[0] = (u32)blk;
    }
    arena[5] = (u32)p;
    return 0;
}

static NOINLINE s32 mp4BlockInit(u32 *arena, u32 size)
{
    if (arena == NULL || size < 0x31U)
        return 1;
    u32 units = (size - 24U) >> 3;
    u32 *first = &arena[6];
    u32 *sent = &arena[6 + 2 * units - 2];
    arena[0] = (u32)((u8 *)arena - 1);
    arena[2] = size;
    arena[3] = 0;
    arena[5] = (u32)first;
    first[0] = (u32)sent;
    first[1] = units - 1U;
    sent[0] = (u32)first;
    sent[1] = 0;
    return 0;
}

static NOINLINE s32 mp4BlockIsEmpty(const u32 *arena)
{
    if (arena != NULL && arena[0] == (u32)((u8 *)arena - 1))
        return (arena[3] == 0) ? 1 : 0;
    return 0;
}
