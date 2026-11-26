#include "mp4msv_internal.h"

Mp4Callbacks g_mp4_callbacks;

Mp4Config g_mp4_config = {
    100, 1, 1, 3, 0
};

const Mp4StateTrans g_mp4_state_table[18][10] = {
    /* action  0 */ { {0, -1}, {0, -1}, {0, -1}, {0, -1}, {1, -1}, {1, -1}, {1, -1}, {1, -1}, {1, -1}, {1, -1} },
    /* action  1 */ { {1,  4}, {0, -1}, {0, -1}, {0, -1}, {0, -1}, {0, -1}, {0, -1}, {0, -1}, {0, -1}, {0, -1} },
    /* action  2 */ { {0, -1}, {0, -1}, {0, -1}, {0, -1}, {1,  7}, {0, -1}, {0, -1}, {0, -1}, {0, -1}, {0, -1} },
    /* action  3 */ { {0, -1}, {1, -1}, {0, -1}, {0, -1}, {1, -1}, {1, -1}, {1, -1}, {1, -1}, {1, -1}, {1, -1} },
    /* action  4 */ { {0, -1}, {0, -1}, {0, -1}, {0, -1}, {1,  6}, {1, -1}, {1, -1}, {1, -1}, {1, -1}, {1, -1} },
    /* action  5 */ { {0, -1}, {0, -1}, {0, -1}, {0, -1}, {1,  5}, {0, -1}, {0, -1}, {0, -1}, {0, -1}, {0, -1} },
    /* action  6 */ { {0, -1}, {0, -1}, {0, -1}, {0, -1}, {0, -1}, {1, -1}, {0, -1}, {0, -1}, {0, -1}, {0, -1} },
    /* action  7 */ { {1,  2}, {1,  2}, {0, -1}, {0, -1}, {0, -1}, {0, -1}, {0, -1}, {0, -1}, {0, -1}, {0, -1} },
    /* action  8 */ { {1,  8}, {1,  8}, {0, -1}, {0, -1}, {0, -1}, {0, -1}, {0, -1}, {0, -1}, {0, -1}, {0, -1} },
    /* action  9 */ { {1,  1}, {1,  1}, {0, -1}, {0, -1}, {0, -1}, {0, -1}, {0, -1}, {0, -1}, {0, -1}, {0, -1} },
    /* action 10 */ { {1,  1}, {1,  1}, {0, -1}, {0, -1}, {0, -1}, {0, -1}, {0, -1}, {0, -1}, {0, -1}, {0, -1} },
    /* action 11 */ { {1,  0}, {1,  0}, {1,  0}, {1,  0}, {1,  0}, {1,  0}, {1,  0}, {1,  0}, {1,  0}, {1,  0} },
    /* action 12 */ { {1, -1}, {1, -1}, {1, -1}, {1, -1}, {1, -1}, {1, -1}, {1, -1}, {1, -1}, {1, -1}, {1, -1} },
    /* action 13 */ { {0, -1}, {0, -1}, {0, -1}, {1, -1}, {0, -1}, {0, -1}, {0, -1}, {0, -1}, {0, -1}, {1, -1} },
    /* action 14 */ { {0, -1}, {0, -1}, {1, -1}, {1, -1}, {0, -1}, {0, -1}, {1, -1}, {1, -1}, {1, -1}, {1, -1} },
    /* action 15 */ { {0, -1}, {0, -1}, {1, -1}, {0, -1}, {0, -1}, {0, -1}, {0, -1}, {0, -1}, {0, -1}, {0, -1} },
    /* action 16 */ { {0, -1}, {0, -1}, {1, -1}, {1, -1}, {1, -1}, {1, -1}, {1, -1}, {0, -1}, {1, -1}, {1, -1} },
    /* action 17 */ { {0, -1}, {0, -1}, {0, -1}, {0, -1}, {0, -1}, {0, -1}, {0, -1}, {0, -1}, {1, -1}, {0, -1} },
};

const int32_t g_mp4_err_table[10] = {
    -1, -1, 3, 3, 7, 7, 7, 7, 9, 9
};

static const uint8_t g_month_days[12] = {
    31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
};

int32_t mp4msv_9CA13D1A(int32_t count, const Mp4Callbacks *cbs)
{
    if (count > 0) {
        g_mp4_callbacks.open_fn = cbs->open_fn;
        g_mp4_callbacks.close_fn = cbs->close_fn;
        g_mp4_callbacks.tell_fn = cbs->tell_fn;
        g_mp4_callbacks.seek_fn = cbs->seek_fn;
        g_mp4_callbacks.get_size_fn = cbs->get_size_fn;
        g_mp4_callbacks.read_fn = cbs->read_fn;
        g_mp4_callbacks.read_be_fn = cbs->read_be_fn;
        g_mp4_callbacks.malloc_fn = cbs->malloc_fn;
        g_mp4_callbacks.calloc_fn = cbs->calloc_fn;
        g_mp4_callbacks.free_fn = cbs->free_fn;
        g_mp4_callbacks.memcpy_fn = cbs->memcpy_fn;
        g_mp4_callbacks.memmove_fn = cbs->memmove_fn;
        g_mp4_callbacks.strncpy_fn = cbs->strncpy_fn;
        g_mp4_callbacks.strcmp_fn = cbs->strcmp_fn;
        g_mp4_callbacks.strncmp_fn = cbs->strncmp_fn;
        g_mp4_callbacks.memset_fn = cbs->memset_fn;
        g_mp4_callbacks.strlen_fn = cbs->strlen_fn;
    }
    return 0;
}

int32_t mp4msv_2A2D949A(void)
{
    return 1;
}

Mp4Config *mp4msv_4A7AD789(void)
{
    return &g_mp4_config;
}

int32_t mp4msv_3C2183C7(int32_t count, const Mp4Config *cfg)
{
    if (count > 0) {
        g_mp4_config = *cfg;
    }
    return 0;
}

MP4_NOINLINE int64_t mp4CbTell(void *fh)
{
    return g_mp4_callbacks.tell_fn(fh);
}

MP4_NOINLINE int32_t mp4CbSeek(void *fh, int64_t offset, int16_t origin)
{
    return g_mp4_callbacks.seek_fn(fh, offset, origin);
}

MP4_NOINLINE int64_t mp4CbGetSize(void *fh)
{
    return g_mp4_callbacks.get_size_fn(fh);
}

MP4_NOINLINE int32_t mp4CbRead(void *dst, uint32_t size, void *fh)
{
    return g_mp4_callbacks.read_fn(dst, size, fh);
}

MP4_NOINLINE int32_t mp4CbReadBe(void *dst, uint32_t size, void *fh)
{
    return g_mp4_callbacks.read_be_fn(dst, size, fh);
}

MP4_NOINLINE void *mp4CbMalloc(uint32_t size)
{
    return g_mp4_callbacks.malloc_fn(size);
}

MP4_NOINLINE void *mp4CbCalloc(uint32_t size)
{
    return g_mp4_callbacks.calloc_fn(size);
}

MP4_NOINLINE void mp4CbFree(void *ptr)
{
    g_mp4_callbacks.free_fn(ptr);
}

MP4_NOINLINE int32_t mp4CbMemcpy(void *dst, uint32_t dst_size, const void *src, uint32_t len)
{
    return g_mp4_callbacks.memcpy_fn(dst, dst_size, src, len);
}

MP4_NOINLINE int32_t mp4CbStrncpy(char *dst, const char *src, uint32_t len)
{
    return g_mp4_callbacks.strncpy_fn(dst, src, len);
}

MP4_NOINLINE uint32_t mp4CbStrlen(const char *str)
{
    return g_mp4_callbacks.strlen_fn(str);
}

int32_t mp4msv_F595F917(Mp4Handle **out_handle)
{
    Mp4Config *cfg = mp4msv_4A7AD789();
    int32_t base_unit = (int32_t)cfg->base_unit;
    int32_t cache_cnt = base_unit * (int32_t)cfg->cache_mul;
    int32_t max_cnt = cache_cnt * (int32_t)cfg->max_mul;
    int32_t min_cnt = base_unit * (int32_t)cfg->min_mul;
    Mp4Handle *handle;
    Mp4IoStream *stream;
    int32_t ret;

    if (max_cnt < base_unit) {
        return MP4_ERR_INVALID_PARAM;
    }

    handle = (Mp4Handle *)mp4MemCalloc(sizeof(Mp4Handle));
    *out_handle = handle;
    if (handle == NULL) {
        return MP4_ERR_NOMEM;
    }

    stream = (Mp4IoStream *)mp4MemCalloc(sizeof(Mp4IoStream));
    handle->stream = stream;
    if (stream == NULL) {
        mp4MemFree(*out_handle);
        *out_handle = NULL;
        return MP4_ERR_NOMEM;
    }

    stream->io_mode = 3;
    stream->reserved_e = 0;
    stream->reserved_f = 0;
    stream->reserved_g = 0;
    stream->reserved_h = 0;
    stream->reserved_i = 0;
    stream->reserved_j = 0;
    stream->base_unit = (uint32_t)base_unit;
    stream->cache_size = (uint32_t)cache_cnt;
    stream->max_read = (uint32_t)max_cnt;
    stream->min_read = (uint32_t)((min_cnt < max_cnt) ? min_cnt : max_cnt);
    stream->is_attached = 0;
    stream->reserved_c = 0;
    stream->reserved_d = 0;

    ret = mp4FsmCreate(&stream->fsm);
    if (ret != 0) {
        mp4MemFree((*out_handle)->stream);
        mp4MemFree(*out_handle);
        *out_handle = NULL;
        return ret;
    }
    return 0;
}

MP4_NOINLINE int32_t mp4CloseMovie(Mp4Handle *handle)
{
    if (handle == NULL) {
        return MP4_ERR_INVALID_HANDLE;
    }
    if (handle->movie != NULL) {
        if (handle->movie->mfra != NULL) {
            if (mp4MfraDestroy(handle->movie->mfra) != 0) {
                return MP4_ERR_INTERNAL;
            }
            handle->movie->mfra = NULL;
        }
        mp4FreeOdsmContexts(handle);
        if (handle->movie != NULL) {
            mp4BoxFree((void **)&handle->movie, MP4_TAG_SLDM);
        }
    }
    return 0;
}

int32_t mp4msv_68FBFBE1(Mp4MoofBox *moof)
{
    Mp4Movie *movie = NULL;
    uint32_t seq_num = 0;
    Mp4MoofBox *target = moof;
    Mp4MoofBox *cur;

    if (moof == NULL) {
        return MP4_ERR_INVALID_HANDLE;
    }
    if (mp4MoofGetMovie(target, &movie) != 0) {
        return MP4_ERR_INTERNAL;
    }
    if (movie->mfra != NULL) {
        if (mp4MoofGetSeqNum(target, &seq_num) != 0) {
            return MP4_ERR_INTERNAL;
        }
        if (mp4MfraRemoveOther(movie->mfra, seq_num) != 0) {
            return MP4_ERR_INTERNAL;
        }
    }

    if (target->movie != NULL || target->next != NULL) {
        Mp4Movie *owner = target->movie;
        if (owner == NULL) {
            return 0;
        }
        cur = owner->moof_head;
        if (cur == target) {
            owner->moof_head = target->next;
        } else if (cur != NULL) {
            int found = 0;
            while (cur->next != NULL) {
                if (cur->next == target) {
                    cur->next = target->next;
                    found = 1;
                    break;
                }
                cur = cur->next;
            }
            if (!found) {
                return MP4_ERR_CORRUPT;
            }
        }
        target->next = NULL;
        target->movie->moof_count--;
    }

    mp4BoxFree((void **)&target, MP4_TAG_MOOF);
    return 0;
}

int32_t mp4msv_DEBFCBCA(Mp4Handle *handle, Mp4Track *track, uint32_t tag)
{
    Mp4UsmtBox **usmt_slot;
    Mp4MtdtBox *mtdt;
    uint16_t removed = 0;
    uint32_t idx;
    Mp4MtdtEntry *new_entries = NULL;
    uint32_t kept = 0;

    if (handle == NULL && track == NULL) {
        return MP4_ERR_INVALID_HANDLE;
    }
    if (handle != NULL && track == NULL && handle->movie == NULL) {
        return MP4_ERR_INVALID_HANDLE;
    }

    if (track != NULL) {
        if (track->usmt == NULL || track->usmt->mtdt.size == 0) {
            return MP4_ERR_NO_ENTRY;
        }
        usmt_slot = &track->usmt;
        mtdt = &track->usmt->mtdt;
    } else {
        Mp4Movie *movie = handle->movie;
        if (movie->size == 0) {
            return MP4_ERR_CORRUPT;
        }
        if (movie->usmt == NULL || movie->usmt->mtdt.size == 0) {
            return MP4_ERR_NO_ENTRY;
        }
        usmt_slot = &movie->usmt;
        mtdt = &movie->usmt->mtdt;
    }

    if (mtdt->entry_count == 0) {
        return MP4_ERR_NO_ENTRY;
    }
    for (idx = 0; idx < mtdt->entry_count; idx++) {
        if (mtdt->entries[idx].tag == tag) {
            removed++;
        }
    }
    if (removed == 0) {
        return MP4_ERR_NO_ENTRY;
    }

    if (mtdt->entry_count == removed) {
        for (idx = 0; idx < mtdt->entry_count; idx++) {
            mp4MtdtExtFree(&mtdt->entries[idx].ext);
        }
        new_entries = NULL;
        kept = 0;
    } else {
        new_entries = (Mp4MtdtEntry *)mp4MemCalloc((uint32_t)(mtdt->entry_count - removed) * sizeof(Mp4MtdtEntry));
        if (new_entries == NULL) {
            return MP4_ERR_NOMEM;
        }
        for (idx = 0; idx < mtdt->entry_count; idx++) {
            if (mtdt->entries[idx].tag == tag) {
                mp4MtdtExtFree(&mtdt->entries[idx].ext);
            } else {
                mp4SafeMemcpy(&new_entries[kept], sizeof(Mp4MtdtEntry), &mtdt->entries[idx], sizeof(Mp4MtdtEntry));
                kept++;
            }
        }
    }

    mp4MemFree(mtdt->entries);
    mtdt->capacity = kept;
    mtdt->entry_count = (uint16_t)(mtdt->entry_count - removed);
    mtdt->entries = new_entries;
    if (new_entries == NULL) {
        mtdt->tag = 0;
        mtdt->size = 0;
        if ((*usmt_slot)->mtdf.size == 0) {
            mp4MemFree(*usmt_slot);
            *usmt_slot = NULL;
        }
    }
    return 0;
}

int32_t mp4msv_576C693B(Mp4Handle *handle, Mp4Track *track, uint32_t tag)
{
    Mp4UsmtBox **usmt_slot;
    Mp4MtdfBox *mtdf;
    uint16_t removed = 0;
    uint32_t idx;
    Mp4MtdfEntry *new_entries = NULL;
    uint32_t kept = 0;

    if (handle == NULL && track == NULL) {
        return MP4_ERR_INVALID_HANDLE;
    }
    if (handle != NULL && track == NULL && handle->movie == NULL) {
        return MP4_ERR_INVALID_HANDLE;
    }

    if (track != NULL) {
        if (track->usmt == NULL || track->usmt->mtdf.size == 0) {
            return MP4_ERR_NO_ENTRY;
        }
        usmt_slot = &track->usmt;
        mtdf = &track->usmt->mtdf;
    } else {
        Mp4Movie *movie = handle->movie;
        if (movie->size == 0) {
            return MP4_ERR_CORRUPT;
        }
        if (movie->usmt == NULL || movie->usmt->mtdf.capacity == 0) {
            return MP4_ERR_NO_ENTRY;
        }
        usmt_slot = &movie->usmt;
        mtdf = &movie->usmt->mtdf;
    }

    if (mtdf->entry_count == 0) {
        return MP4_ERR_NO_ENTRY;
    }
    for (idx = 0; idx < mtdf->entry_count; idx++) {
        if (mtdf->entries[idx].tag == tag) {
            removed++;
        }
    }
    if (removed == 0) {
        return MP4_ERR_NO_ENTRY;
    }

    if (mtdf->entry_count != removed) {
        new_entries = (Mp4MtdfEntry *)mp4MemCalloc((uint32_t)(mtdf->entry_count - removed) * sizeof(Mp4MtdfEntry));
        if (new_entries == NULL) {
            return MP4_ERR_NOMEM;
        }
        for (idx = 0; idx < mtdf->entry_count; idx++) {
            if (mtdf->entries[idx].tag != tag) {
                mp4SafeMemcpy(&new_entries[kept], sizeof(Mp4MtdfEntry), &mtdf->entries[idx], sizeof(Mp4MtdfEntry));
                kept++;
            }
        }
    }

    mp4MemFree(mtdf->entries);
    mtdf->capacity = kept;
    mtdf->entry_count = (uint16_t)(mtdf->entry_count - removed);
    mtdf->entries = new_entries;
    if (new_entries == NULL) {
        mtdf->tag = 0;
        mtdf->capacity = 0;
        if ((*usmt_slot)->mtdt.size == 0) {
            mp4MemFree(*usmt_slot);
            *usmt_slot = NULL;
        }
    }
    return 0;
}

int32_t mp4msv_3D8D41A0(Mp4Handle *handle, void *user_fh)
{
    if (handle == NULL || handle->stream == NULL) {
        return MP4_ERR_INVALID_HANDLE;
    }
    handle->stream->file_handle = user_fh;
    handle->stream->is_attached = 1;
    handle->stream->user_handle = user_fh;
    return 0;
}

int32_t mp4msv_9D56A7CC(Mp4Handle *handle)
{
    if (handle == NULL) {
        return MP4_ERR_INVALID_HANDLE;
    }
    mp4FsmDestroy(handle->stream->fsm);
    if (handle->stream != NULL) {
        mp4MemFree(handle->stream);
    }
    mp4CloseMovie(handle);
    mp4MemFree(handle);
    return 0;
}

int32_t mp4msv_A824645B(uint32_t src_val, uint32_t src_scale, uint32_t dst_scale, uint32_t *out_scaled)
{
    uint32_t prod_hi;
    uint32_t prod_lo;
    uint32_t quot_hi;
    uint32_t rem;

    if (src_scale == 0) {
        return MP4_ERR_INVALID_PARAM;
    }
    mp4MulU32ToU64(src_val, dst_scale, &prod_hi, &prod_lo);
    mp4DivU64ByU32(prod_hi, prod_lo, src_scale, &quot_hi, out_scaled, &rem);
    return 0;
}

int32_t mp4msv_98ADC323(Mp4Track *track, uint32_t *out_start, uint32_t *out_count)
{
    int32_t ret;
    if (track == NULL) {
        return MP4_ERR_INVALID_HANDLE;
    }
    ret = mp4FsmBegin(track->movie->self->handle->stream->fsm, 14);
    if (ret == 0) {
        ret = mp4GetTrackCacheRange(track, out_start, out_count);
        if (ret == 0) {
            mp4FsmCommit(track->movie->self->handle->stream->fsm);
            return 0;
        }
    }
    return ret;
}

int32_t mp4msv_5087548E(Mp4Track *track)
{
    int32_t ret;
    if (track == NULL) {
        return MP4_ERR_INVALID_HANDLE;
    }
    ret = mp4FsmBegin(track->movie->self->handle->stream->fsm, 13);
    if (ret == 0) {
        ret = mp4LoadNextTrackFrag(track);
        if (ret == 0) {
            mp4FsmCommit(track->movie->self->handle->stream->fsm);
            return 0;
        }
    }
    return ret;
}

int32_t mp4msv_8A0FD93F(Mp4Track *track, uint32_t start_sample, uint32_t count, uint32_t *out_start, uint32_t *out_count)
{
    int32_t ret;
    if (track == NULL) {
        return MP4_ERR_INVALID_HANDLE;
    }
    ret = mp4FsmBegin(track->movie->self->handle->stream->fsm, 15);
    if (ret == 0) {
        ret = mp4PrefetchCacheWindow(track, start_sample, count, out_start, out_count);
        if (ret == 0) {
            mp4FsmCommit(track->movie->self->handle->stream->fsm);
            return 0;
        }
    }
    return ret;
}

int32_t mp4msv_46186C26(Mp4Handle *handle)
{
    if (handle != NULL && handle->movie != NULL) {
        return mp4CheckMoovValid(handle);
    }
    return MP4_ERR_INVALID_HANDLE;
}

int32_t mp4msv_BD23D34A(Mp4Handle *handle, uint32_t brand, uint32_t *out_found)
{
    if (handle != NULL && handle->movie != NULL) {
        return mp4HasCompatBrand(handle, brand, out_found);
    }
    return MP4_ERR_INVALID_HANDLE;
}

int32_t mp4msv_D8BA6AFE(Mp4Track *track, uint32_t sample_num, int32_t mode, uint32_t *out_val)
{
    if (track != NULL) {
        return mp4TrackCheckValid(track, sample_num, mode, out_val);
    }
    return MP4_ERR_INVALID_HANDLE;
}

int32_t mp4msv_0C0843B0(Mp4Track *track, uint32_t time_val, uint32_t *out_sample, uint32_t flags, uint32_t *out_sync, uint32_t *out_rem)
{
    if (track != NULL) {
        return mp4FindSampleByTime(track, time_val, out_sample, flags, out_sync, out_rem);
    }
    return MP4_ERR_INVALID_HANDLE;
}

int32_t mp4msv_089364A9(Mp4Track *track, uint32_t sample_num, int32_t direction, uint32_t *out_sample, uint32_t *out_delta)
{
    if (track != NULL) {
        return mp4FindSyncSample(track, sample_num, direction, out_sample, out_delta);
    }
    return MP4_ERR_INVALID_HANDLE;
}

int32_t mp4msv_E5C33D61(Mp4Track *track, uint32_t sample_num, int32_t want_chunk_rem, Mp4SampleInfo *out_info)
{
    if (track != NULL) {
        return mp4GetSampleInfo(track, sample_num, want_chunk_rem, out_info);
    }
    return MP4_ERR_INVALID_HANDLE;
}

int32_t mp4msv_21BD7274(Mp4Track *track, uint32_t sample_num, uint32_t *out_time, uint32_t *out_dur)
{
    if (track != NULL) {
        return mp4GetSampleTime(track, sample_num, out_time, out_dur);
    }
    return MP4_ERR_INVALID_HANDLE;
}

int32_t mp4msv_9501780A(Mp4Track *track, uint32_t time_val, uint32_t *out_sample, uint32_t *out_sync, uint32_t *out_rem)
{
    if (track != NULL) {
        return mp4FindFragSampleByTime(track, time_val, out_sample, out_sync, out_rem);
    }
    return MP4_ERR_INVALID_HANDLE;
}

int32_t mp4msv_21BE7D96(Mp4Track *track, uint32_t *out_time)
{
    if (track != NULL) {
        return mp4GetTrackFragBaseTime(track, out_time);
    }
    return MP4_ERR_INVALID_HANDLE;
}

int32_t mp4msv_0D6843EA(Mp4Handle *handle, int32_t opt_id, int32_t opt_val)
{
    if (handle == NULL) {
        return MP4_ERR_INVALID_HANDLE;
    }
    if (opt_id != 0) {
        return MP4_ERR_INVALID_PARAM;
    }
    if (opt_val != 0) {
        handle->options |= 1U;
    } else {
        handle->options &= ~1U;
    }
    return 0;
}

int32_t mp4msv_67AF9E0F(Mp4Handle *handle, uint32_t *out_flags, int32_t mode)
{
    int32_t ret;
    if (handle == NULL || handle->stream == NULL) {
        return MP4_ERR_INVALID_HANDLE;
    }
    ret = mp4FsmBegin(handle->stream->fsm, 7);
    if (ret == 0) {
        ret = mp4OpenMovieFull(handle, out_flags, mode);
        if (ret == 0) {
            mp4FsmCommit(handle->stream->fsm);
            return 0;
        }
    }
    return ret;
}

int32_t mp4msv_BE203EA3(Mp4Handle *handle, uint32_t *out_flags, int32_t mode)
{
    int32_t ret;
    if (handle == NULL || handle->stream == NULL) {
        return MP4_ERR_INVALID_HANDLE;
    }
    ret = mp4FsmBegin(handle->stream->fsm, 8);
    if (ret == 0) {
        ret = mp4OpenMovieFull(handle, out_flags, mode);
        if (ret == 0) {
            mp4FsmCommit(handle->stream->fsm);
            return 0;
        }
    }
    return ret;
}

int32_t mp4msv_CFF25FAC(Mp4Handle *handle, uint32_t *out_flags)
{
    int32_t ret;
    if (handle == NULL || handle->stream == NULL) {
        return MP4_ERR_INVALID_HANDLE;
    }
    ret = mp4FsmBegin(handle->stream->fsm, 9);
    if (ret == 0) {
        ret = mp4OpenMovieBasic(handle, out_flags);
        if (ret == 0) {
            mp4FsmCommit(handle->stream->fsm);
            return 0;
        }
    }
    return ret;
}

int32_t mp4msv_3149D53B(Mp4Handle *handle, uint32_t mask, uint32_t *out_flags)
{
    int32_t ret;
    if (handle == NULL || handle->stream == NULL) {
        return MP4_ERR_INVALID_HANDLE;
    }
    ret = mp4FsmBegin(handle->stream->fsm, 10);
    if (ret == 0) {
        ret = mp4OpenMoviePartial(handle, mask, out_flags);
        if (ret == 0) {
            mp4FsmCommit(handle->stream->fsm);
            return 0;
        }
    }
    return ret;
}

int32_t mp4msv_27412E93(Mp4SampleEntry *entry, int32_t has_size, uint32_t buf_size, void *out_buf)
{
    if (entry == NULL) {
        return MP4_ERR_INVALID_HANDLE;
    }
    if (has_size == 0) {
        int32_t ret = mp4GetEsdsLen(entry, &buf_size);
        if (ret != 0) {
            return ret;
        }
    }
    return mp4GetEsdsData(entry, buf_size, out_buf);
}

int32_t mp4msv_E641D779(Mp4SampleEntry *entry, uint8_t index, int32_t has_size, uint32_t buf_size, void *out_buf)
{
    if (entry == NULL) {
        return MP4_ERR_INVALID_HANDLE;
    }
    if (has_size == 0) {
        uint16_t nal_len;
        int32_t ret = mp4GetAvccSpsLen(entry, index, &nal_len);
        if (ret != 0) {
            return ret;
        }
        buf_size = nal_len;
    }
    return mp4GetAvccSpsData(entry, index, buf_size, out_buf);
}

int32_t mp4msv_BE558D75(Mp4SampleEntry *entry, uint8_t index, int32_t has_size, uint32_t buf_size, void *out_buf)
{
    if (entry == NULL) {
        return MP4_ERR_INVALID_HANDLE;
    }
    if (has_size == 0) {
        uint16_t nal_len;
        int32_t ret = mp4GetAvccPpsLen(entry, index, &nal_len);
        if (ret != 0) {
            return ret;
        }
        buf_size = nal_len;
    }
    return mp4GetAvccPpsData(entry, index, buf_size, out_buf);
}

int32_t mp4msv_CE9C6D39(Mp4DrefEntry *entry, int32_t has_size, uint32_t buf_size, char *out_buf)
{
    if (entry == NULL) {
        return MP4_ERR_INVALID_HANDLE;
    }
    if (out_buf == NULL) {
        return MP4_ERR_INVALID_PARAM;
    }
    if (has_size == 0) {
        int32_t ret = mp4GetDrefLocationLen(entry, &buf_size);
        if (ret != 0) {
            return ret;
        }
    }
    return mp4GetDrefLocation(entry, buf_size, out_buf);
}

int32_t mp4msv_5565A318(Mp4Handle *handle, Mp4Track *track, uint32_t tag, uint16_t lang, uint16_t enc, uint16_t *out_lang, Mp4MtdtEntry **out_entry, uint16_t *out_len)
{
    uint16_t found_lang;
    int32_t ret;

    if (((uintptr_t)handle | (uintptr_t)track) == 0U) {
        return MP4_ERR_INVALID_HANDLE;
    }
    if ((handle != NULL) & (track == NULL)) {
        if (handle->movie == NULL) {
            return MP4_ERR_INVALID_HANDLE;
        }
    }
    ret = mp4FindMtdtEntry(handle, track, tag, lang, enc, &found_lang, out_entry, out_len);
    if (out_lang != NULL) {
        *out_lang = found_lang;
    }
    return ret;
}

int32_t mp4msv_9076AA02(Mp4Handle *handle, Mp4Track *track, uint32_t tag, uint16_t *out_lang, uint16_t *out_enc, int32_t check_size, uint32_t buf_size, void *out_buf)
{
    if (((uintptr_t)handle | (uintptr_t)track) == 0U) {
        return MP4_ERR_INVALID_HANDLE;
    }
    if ((out_lang == NULL) | (out_enc == NULL)) {
        return MP4_ERR_INVALID_PARAM;
    }
    if (out_buf == NULL) {
        return MP4_ERR_INVALID_PARAM;
    }
    if ((handle != NULL) & (track == NULL)) {
        if (handle->movie == NULL) {
            return MP4_ERR_INVALID_HANDLE;
        }
    }
    if ((check_size != 0) & (buf_size < 32U)) {
        return MP4_ERR_INVALID_PARAM;
    }
    return mp4FindMtdfEntry(handle, track, tag, out_lang, out_enc, out_buf);
}

int32_t mp4msv_6AE9B944(Mp4Handle *handle, Mp4Track *track, uint32_t index, uint32_t *out_tag, uint16_t *out_lang, uint16_t *out_enc, int32_t check_size, uint32_t buf_size, void *out_buf)
{
    if (((uintptr_t)handle | (uintptr_t)track) == 0U) {
        return MP4_ERR_INVALID_HANDLE;
    }
    if ((out_tag == NULL) | (out_lang == NULL)) {
        return MP4_ERR_INVALID_PARAM;
    }
    if ((out_enc == NULL) | (out_buf == NULL)) {
        return MP4_ERR_INVALID_PARAM;
    }
    if ((handle != NULL) & (track == NULL)) {
        if (handle->movie == NULL) {
            return MP4_ERR_INVALID_HANDLE;
        }
    }
    if ((check_size != 0) & (buf_size < 32U)) {
        return MP4_ERR_INVALID_PARAM;
    }
    return mp4GetMtdfByIndex(handle, track, index, out_tag, out_lang, out_enc, out_buf);
}

int32_t mp4msv_AADD18E7(Mp4OdsmObject *obj, uint8_t index, int32_t has_size, uint32_t buf_size, void *out_buf)
{
    if (obj == NULL) {
        return MP4_ERR_INVALID_HANDLE;
    }
    if (has_size == 0) {
        uint16_t stream_type;
        uint32_t payload_len;
        int32_t ret = mp4GetOdsmEsHeader(obj, index, &stream_type, &payload_len);
        if (ret != 0) {
            return ret;
        }
        buf_size = payload_len;
    }
    return mp4GetOdsmEsPayload(obj, index, buf_size, out_buf);
}

int32_t mp4msv_E4AFDAD3(Mp4Handle *handle, uint64_t *out_offset)
{
    if (handle == NULL || handle->movie == NULL) {
        return MP4_ERR_INVALID_HANDLE;
    }
    if (handle->movie->mtsd_offset == 0) {
        return MP4_ERR_NOT_FOUND;
    }
    if (out_offset == NULL) {
        return MP4_ERR_INVALID_PARAM;
    }
    *out_offset = handle->movie->mtsd_offset;
    return 0;
}

int32_t mp4msv_4154992A(Mp4MtdtEntry *entry, uint32_t *out_magic, uint32_t *out_count)
{
    if (entry == NULL) {
        return MP4_ERR_INVALID_HANDLE;
    }
    if (out_magic == NULL || out_count == NULL) {
        return MP4_ERR_INVALID_PARAM;
    }
    if ((uint32_t)(entry->tag - 2564U) >= 2U) {
        return MP4_ERR_NOT_FOUND;
    }
    *out_magic = entry->ext.head_magic;
    *out_count = entry->ext.item_count;
    return 0;
}

int32_t mp4msv_C38F9765(Mp4MtdtEntry *entry, uint32_t index_one_based, Mp4MtsmBox **out_mtsm, uint32_t *out_offset, uint32_t *out_size, uint32_t *out_stsd_idx)
{
    Mp4MtsmBox *mtsm;
    Mp4MtdtExtItem *item;
    Mp4McstEntry *mcst_entry;

    if (entry == NULL) {
        return MP4_ERR_INVALID_HANDLE;
    }
    if (index_one_based == 0) {
        return MP4_ERR_INVALID_PARAM;
    }
    if (entry->ext.item_count < index_one_based) {
        return MP4_ERR_NOT_FOUND;
    }
    if (out_mtsm == NULL || out_offset == NULL || out_size == NULL || out_stsd_idx == NULL) {
        return MP4_ERR_INVALID_PARAM;
    }

    mtsm = entry->ext.mtsm;
    item = &entry->ext.items[index_one_based - 1];
    if (item->media_id != mtsm->mthd.media_id || mtsm->mcst.entry_count < item->sample_index) {
        return MP4_ERR_NOT_FOUND;
    }

    mcst_entry = &mtsm->mcst.entries[item->sample_index - 1];
    *out_mtsm = mtsm;
    *out_offset = mcst_entry->sample_offset;
    *out_size = mcst_entry->sample_size;
    *out_stsd_idx = mcst_entry->stsd_index;
    return 0;
}

int32_t mp4msv_2A296DDA(Mp4MtsmBox *mtsm, uint32_t index_one_based, Mp4DrefEntry **out_entry);

int32_t mp4msv_63752099(Mp4MtsmBox *mtsm, uint32_t index_one_based, Mp4MtsmImageInfo *out_info)
{
    Mp4SampleEntry *entry;
    uint32_t step;
    Mp4VisualSampleDescOut vis_desc;
    Mp4DrefEntry *dref_entry = NULL;

    if (mtsm == NULL) {
        return MP4_ERR_INVALID_HANDLE;
    }
    if (index_one_based == 0 || out_info == NULL) {
        return MP4_ERR_INVALID_PARAM;
    }
    if (mtsm->stsd.entry_count < index_one_based) {
        return MP4_ERR_NOT_FOUND;
    }

    entry = mtsm->stsd.head;
    for (step = 1; step < index_one_based; step++) {
        if (entry == NULL) {
            return MP4_ERR_NOT_FOUND;
        }
        entry = entry->next;
    }
    if (entry == NULL || entry->tag != MP4_TAG_PNG) {
        return MP4_ERR_NOT_FOUND;
    }
    if (mp4GetVisualSampleDesc(entry, &vis_desc) != 0) {
        return MP4_ERR_NOT_FOUND;
    }
    if (mp4msv_2A296DDA(mtsm, entry->data_ref_index, &dref_entry) != 0) {
        return MP4_ERR_NOT_FOUND;
    }

    out_info->codec_tag = vis_desc.codec_tag;
    out_info->data_ref = dref_entry;
    out_info->width = vis_desc.width;
    out_info->height = vis_desc.height;
    out_info->horiz_res = vis_desc.horiz_res;
    out_info->vert_res = vis_desc.vert_res;
    return 0;
}

int32_t mp4msv_F50268BF(Mp4MtsmBox *mtsm, uint32_t *out_count)
{
    Mp4DrefEntry *cur;
    uint32_t cnt = 0;

    if (mtsm == NULL) {
        return MP4_ERR_INVALID_HANDLE;
    }
    if (out_count == NULL) {
        return MP4_ERR_INVALID_PARAM;
    }
    cur = mtsm->dinf.dref.head;
    while (cur != NULL) {
        cnt++;
        cur = cur->next;
    }
    *out_count = cnt;
    return 0;
}

int32_t mp4msv_2A296DDA(Mp4MtsmBox *mtsm, uint32_t index_one_based, Mp4DrefEntry **out_entry)
{
    Mp4DrefEntry *cur;
    uint32_t step;

    if (mtsm == NULL) {
        return MP4_ERR_INVALID_HANDLE;
    }
    if (index_one_based == 0 || out_entry == NULL) {
        return MP4_ERR_INVALID_PARAM;
    }
    cur = mtsm->dinf.dref.head;
    for (step = 1; step < index_one_based; step++) {
        if (cur == NULL) {
            return MP4_ERR_NOT_FOUND;
        }
        cur = cur->next;
    }
    if (cur == NULL) {
        return MP4_ERR_NOT_FOUND;
    }
    *out_entry = cur;
    return 0;
}

int32_t mp4msv_A41C1F5F(Mp4Handle *handle, uint32_t *out_count)
{
    Mp4UuidNode *cur;
    uint32_t cnt = 0;

    if (handle == NULL) {
        return MP4_ERR_INVALID_HANDLE;
    }
    if (out_count == NULL) {
        return MP4_ERR_INVALID_PARAM;
    }
    if (handle->movie == NULL) {
        return MP4_ERR_INVALID_HANDLE;
    }
    cur = handle->movie->uuid_head;
    while (cur != NULL) {
        cnt++;
        cur = cur->next;
    }
    *out_count = cnt;
    return 0;
}

int32_t mp4msv_B919743C(Mp4Handle *handle, uint32_t index_one_based, uint64_t *out_offset, uint32_t *out_size, uint32_t *out_uuid)
{
    Mp4UuidNode *cur;
    uint32_t step;
    uint32_t uuid_idx;

    if (handle == NULL) {
        return MP4_ERR_INVALID_HANDLE;
    }
    if (index_one_based == 0 || out_offset == NULL || out_size == NULL || out_uuid == NULL) {
        return MP4_ERR_INVALID_PARAM;
    }
    if (handle->movie == NULL) {
        return MP4_ERR_INVALID_HANDLE;
    }
    cur = handle->movie->uuid_head;
    if (cur == NULL) {
        return MP4_ERR_NOT_FOUND;
    }
    for (step = 1; step != index_one_based; step++) {
        cur = cur->next;
        if (cur == NULL) {
            return MP4_ERR_NOT_FOUND;
        }
    }
    *out_offset = cur->file_offset;
    *out_size = cur->box_size;
    for (uuid_idx = 0; uuid_idx < 4; uuid_idx++) {
        out_uuid[uuid_idx] = cur->uuid[uuid_idx];
    }
    return 0;
}

int32_t mp4msv_032F3500(Mp4Handle *handle, const uint32_t *target_uuid, uint32_t *out_count)
{
    Mp4UuidNode *cur;
    uint32_t matches = 0;

    if (handle == NULL) {
        return MP4_ERR_INVALID_HANDLE;
    }
    if (target_uuid == NULL || out_count == NULL) {
        return MP4_ERR_INVALID_PARAM;
    }
    if (handle->movie == NULL) {
        return MP4_ERR_INVALID_HANDLE;
    }
    cur = handle->movie->uuid_head;
    while (cur != NULL) {
        if (cur->uuid[0] == target_uuid[0] &&
            cur->uuid[1] == target_uuid[1] &&
            cur->uuid[2] == target_uuid[2] &&
            cur->uuid[3] == target_uuid[3]) {
            matches++;
        }
        cur = cur->next;
    }
    *out_count = matches;
    return 0;
}

int32_t mp4msv_509B3BAC(Mp4Handle *handle, const uint32_t *target_uuid, uint32_t match_one_based, uint64_t *out_offset, uint32_t *out_size)
{
    Mp4UuidNode *cur;
    uint32_t matches = 0;

    if (handle == NULL) {
        return MP4_ERR_INVALID_HANDLE;
    }
    if (target_uuid == NULL || match_one_based == 0 || out_offset == NULL || out_size == NULL) {
        return MP4_ERR_INVALID_PARAM;
    }
    if (handle->movie == NULL) {
        return MP4_ERR_INVALID_HANDLE;
    }
    cur = handle->movie->uuid_head;
    while (cur != NULL) {
        if (cur->uuid[0] == target_uuid[0] &&
            cur->uuid[1] == target_uuid[1] &&
            cur->uuid[2] == target_uuid[2] &&
            cur->uuid[3] == target_uuid[3]) {
            matches++;
            if (matches == match_one_based) {
                *out_offset = cur->file_offset;
                *out_size = cur->box_size;
                return 0;
            }
        }
        cur = cur->next;
    }
    return MP4_ERR_NOT_FOUND;
}

int32_t mp4msv_07C60A23(Mp4Handle *handle, uint32_t *out_major, uint32_t *out_minor)
{
    if (handle != NULL && handle->movie != NULL) {
        return mp4GetFtypBrand(handle, out_major, out_minor);
    }
    return MP4_ERR_INVALID_HANDLE;
}

int32_t mp4msv_8FD8E0A4(Mp4Handle *handle, Mp4DateTime *out_ctime, Mp4DateTime *out_mtime)
{
    if (handle != NULL && handle->movie != NULL) {
        return mp4GetMovieTimes(handle, out_ctime, out_mtime);
    }
    return MP4_ERR_INVALID_HANDLE;
}

int32_t mp4msv_E042AB3C(Mp4Handle *handle, uint32_t *out_scale)
{
    if (handle != NULL && handle->movie != NULL) {
        return mp4GetMovieTimescale(handle, out_scale);
    }
    return MP4_ERR_INVALID_HANDLE;
}

int32_t mp4msv_182FF541(Mp4Handle *handle, uint32_t *out_dur)
{
    if (handle != NULL && handle->movie != NULL) {
        return mp4GetMovieDuration(handle, out_dur);
    }
    return MP4_ERR_INVALID_HANDLE;
}

int32_t mp4msv_3D57BA52(Mp4Handle *handle, uint16_t *out_count)
{
    if (handle != NULL && handle->movie != NULL) {
        return mp4GetTrackCount(handle, out_count);
    }
    return MP4_ERR_INVALID_HANDLE;
}

int32_t mp4msv_0C06DACB(Mp4Handle *handle, uint16_t index, Mp4Track **out_track)
{
    if (handle != NULL && handle->movie != NULL) {
        return mp4GetTrackByIndex(handle, index, out_track);
    }
    return MP4_ERR_INVALID_HANDLE;
}

int32_t mp4msv_8D564A92(Mp4Handle *handle, uint32_t track_id, Mp4Track **out_track)
{
    if (handle != NULL && handle->movie != NULL) {
        return mp4GetTrackById(handle, track_id, out_track);
    }
    return MP4_ERR_INVALID_HANDLE;
}

int32_t mp4msv_A55AF94C(Mp4Track *track, Mp4TrackInfo *out_info)
{
    if (track != NULL) {
        return mp4GetTrackHeaderInfo(track, out_info);
    }
    return MP4_ERR_INVALID_HANDLE;
}

int32_t mp4msv_6B94A29D(Mp4Handle *handle, Mp4Track *track, int32_t *out_matrix)
{
    if (((uintptr_t)handle | (uintptr_t)track) == 0U) {
        return MP4_ERR_INVALID_HANDLE;
    }
    if (handle != NULL && handle->movie == NULL) {
        return MP4_ERR_INVALID_HANDLE;
    }
    return mp4GetMatrix(handle, track, out_matrix);
}

int32_t mp4msv_D3505C12(Mp4Track *track, uint32_t ref_type, uint32_t *out_count)
{
    if (track != NULL) {
        return mp4GetTrefCount(track, ref_type, out_count);
    }
    return MP4_ERR_INVALID_HANDLE;
}

int32_t mp4msv_A2AE6F30(Mp4Track *track, uint32_t ref_type, uint32_t index, Mp4Track **out_track)
{
    if (track != NULL) {
        return mp4GetTrefTrack(track, ref_type, index, out_track);
    }
    return MP4_ERR_INVALID_HANDLE;
}

int32_t mp4msv_54410730(Mp4Track *track, uint32_t *out_count)
{
    if (track != NULL) {
        return mp4GetElstCount(track, out_count);
    }
    return MP4_ERR_INVALID_HANDLE;
}

int32_t mp4msv_EE62F96D(Mp4Track *track, uint32_t index, uint32_t *out_dur, uint32_t *out_time, uint32_t *out_rate)
{
    if (track != NULL) {
        return mp4GetElstEntry(track, index, out_dur, out_time, out_rate);
    }
    return MP4_ERR_INVALID_HANDLE;
}

int32_t mp4msv_9BB5E338(Mp4Track *track, uint32_t *out_scale)
{
    if (track != NULL) {
        return mp4GetMediaTimescale(track, out_scale);
    }
    return MP4_ERR_INVALID_HANDLE;
}

int32_t mp4msv_FF9E6F58(Mp4Track *track, uint32_t *out_dur)
{
    if (track != NULL) {
        return mp4GetTrackDuration(track, out_dur);
    }
    return MP4_ERR_INVALID_HANDLE;
}

int32_t mp4msv_3F6B6C59(Mp4Track *track, uint32_t *out_type)
{
    if (track != NULL) {
        return mp4GetHandlerType(track, out_type);
    }
    return MP4_ERR_INVALID_HANDLE;
}

int32_t mp4msv_5EE48079(Mp4Track *track, uint32_t *out_count)
{
    if (track != NULL) {
        return mp4GetTrackSampleCount(track, out_count);
    }
    return MP4_ERR_INVALID_HANDLE;
}

int32_t mp4msv_162503F3(Mp4Track *track, uint32_t sample_num, uint32_t *out_size)
{
    if (track != NULL) {
        return mp4GetSampleSize(track, sample_num, out_size);
    }
    return MP4_ERR_INVALID_HANDLE;
}

int32_t mp4msv_A9FD62E2(Mp4Track *track, uint32_t *out_count)
{
    if (track != NULL) {
        return mp4GetSampleDescCount(track, out_count);
    }
    return MP4_ERR_INVALID_HANDLE;
}

int32_t mp4msv_6D619263(Mp4Track *track, uint32_t index, Mp4SampleEntry **out_entry)
{
    if (track != NULL) {
        return mp4GetSampleDescByIndex(track, index, out_entry);
    }
    return MP4_ERR_INVALID_HANDLE;
}

int32_t mp4msv_2BAF7C25(Mp4SampleEntry *entry, Mp4VisualBriefOut *out_desc)
{
    if (entry != NULL) {
        return mp4GetVisualBrief(entry, out_desc);
    }
    return MP4_ERR_INVALID_HANDLE;
}

int32_t mp4msv_A418509D(Mp4SampleEntry *entry, Mp4AudioBriefOut *out_desc)
{
    if (entry != NULL) {
        return mp4GetAudioBrief(entry, out_desc);
    }
    return MP4_ERR_INVALID_HANDLE;
}

int32_t mp4msv_8C998600(Mp4SampleEntry *entry, uint32_t *out_len)
{
    if (entry != NULL) {
        return mp4GetEsdsLen(entry, out_len);
    }
    return MP4_ERR_INVALID_HANDLE;
}

int32_t mp4msv_35144B9C(Mp4SampleEntry *entry, uint8_t *out_hdr, uint8_t *out_sps_cnt, uint8_t *out_pps_cnt)
{
    if (entry != NULL) {
        return mp4GetAvccHeader(entry, out_hdr, out_sps_cnt, out_pps_cnt);
    }
    return MP4_ERR_INVALID_HANDLE;
}

int32_t mp4msv_2B1B8D23(Mp4SampleEntry *entry, uint8_t index, uint16_t *out_len)
{
    if (entry != NULL) {
        return mp4GetAvccSpsLen(entry, index, out_len);
    }
    return MP4_ERR_INVALID_HANDLE;
}

int32_t mp4msv_A09878FC(Mp4SampleEntry *entry, uint8_t index, uint16_t *out_len)
{
    if (entry != NULL) {
        return mp4GetAvccPpsLen(entry, index, out_len);
    }
    return MP4_ERR_INVALID_HANDLE;
}

int32_t mp4msv_08F96BE2(Mp4DrefEntry *entry, uint32_t *out_len)
{
    if (entry != NULL) {
        return mp4GetDrefLocationLen(entry, out_len);
    }
    return MP4_ERR_INVALID_HANDLE;
}

int32_t mp4msv_287B3FDA(Mp4Track *track, uint32_t *out_hi, uint32_t *out_lo, int32_t mode)
{
    if (track != NULL) {
        return mp4GetTrackUsmtCustom(track, out_hi, out_lo, mode);
    }
    return MP4_ERR_INVALID_HANDLE;
}

int32_t mp4msv_886CFC3D(Mp4Handle *handle, uint64_t *out_offset)
{
    if (handle != NULL) {
        return mp4GetMoovOffset(handle, out_offset);
    }
    return MP4_ERR_INVALID_HANDLE;
}

int32_t mp4msv_9632797F(Mp4MtdtEntry *entry, int32_t has_size, uint32_t buf_size, uint8_t *out_buf)
{
    if (entry == NULL) {
        return MP4_ERR_INVALID_HANDLE;
    }
    if (has_size == 0) {
        buf_size = (uint32_t)entry->size - 10U;
    }
    return mp4CopyMtdtPayload(entry, buf_size, out_buf);
}

int32_t mp4msv_EF6D76BC(Mp4Handle *handle, Mp4Track *track, uint32_t *out_count)
{
    if (((uintptr_t)handle | (uintptr_t)track) == 0U) {
        return MP4_ERR_INVALID_HANDLE;
    }
    if ((handle != NULL) & (track == NULL)) {
        if (handle->movie == NULL) {
            return MP4_ERR_INVALID_HANDLE;
        }
    }
    return mp4GetMtdtCount(handle, track, out_count);
}

int32_t mp4msv_4C677A7E(Mp4Handle *handle, Mp4Track *track, uint32_t index, Mp4MtdtEntry **out_entry, uint32_t *out_tag, uint16_t *out_lang, uint16_t *out_enc, uint16_t *out_len)
{
    if (((uintptr_t)handle | (uintptr_t)track) == 0U) {
        return MP4_ERR_INVALID_HANDLE;
    }
    if ((handle != NULL) & (track == NULL)) {
        if (handle->movie == NULL) {
            return MP4_ERR_INVALID_HANDLE;
        }
    }
    return mp4GetMtdtByIndex(handle, track, index, out_entry, out_tag, out_lang, out_enc, out_len);
}

int32_t mp4msv_0DF0B9AD(Mp4Handle *handle, uint32_t index, uint32_t *out_words)
{
    if (handle != NULL && handle->movie != NULL) {
        return mp4GetAprfEntry(handle, index, out_words);
    }
    return MP4_ERR_INVALID_HANDLE;
}

int32_t mp4msv_AD3AF34E(Mp4Handle *handle, uint32_t index, uint32_t *out_words)
{
    if (handle != NULL && handle->movie != NULL) {
        return mp4GetVprfEntry(handle, index, out_words);
    }
    return MP4_ERR_INVALID_HANDLE;
}

int32_t mp4msv_0D32271B(Mp4Handle *handle, uint32_t *out_attr)
{
    if (handle != NULL && handle->movie != NULL) {
        return mp4GetFprfAttr(handle, out_attr);
    }
    return MP4_ERR_INVALID_HANDLE;
}

int32_t mp4msv_D070C0AA(Mp4Handle *handle, uint16_t *out_count)
{
    if (handle != NULL && handle->movie != NULL) {
        return mp4GetOdsmCount(handle, out_count);
    }
    return MP4_ERR_INVALID_HANDLE;
}

int32_t mp4msv_888C1115(Mp4Handle *handle, uint16_t index, Mp4OdsmObject **out_obj)
{
    if (handle != NULL && handle->movie != NULL) {
        return mp4GetOdsmObject(handle, index, out_obj);
    }
    return MP4_ERR_INVALID_HANDLE;
}

int32_t mp4msv_E8DE0EB8(Mp4OdsmObject *obj, Mp4OdsmTrackInfo *out_info)
{
    if (obj != NULL) {
        return mp4GetOdsmTrackInfo(obj, out_info);
    }
    return MP4_ERR_INVALID_HANDLE;
}

int32_t mp4msv_CD733822(Mp4OdsmObject *obj, uint8_t index, uint16_t *out_type, uint32_t *out_size)
{
    if (obj != NULL) {
        return mp4GetOdsmEsHeader(obj, index, out_type, out_size);
    }
    return MP4_ERR_INVALID_HANDLE;
}

int32_t mp4msv_9A8B21D2(Mp4Handle *handle, uint8_t *out_profiles)
{
    if (handle != NULL && handle->movie != NULL) {
        return mp4GetIodsProfiles(handle, out_profiles);
    }
    return MP4_ERR_INVALID_HANDLE;
}

int32_t mp4msv_65C17387(Mp4SampleEntry *entry, Mp4EsConfigOut *out_cfg)
{
    if (entry != NULL) {
        return mp4GetEsConfig(entry, out_cfg);
    }
    return MP4_ERR_INVALID_HANDLE;
}

int32_t mp4msv_2BFE233D(Mp4SampleEntry *entry, int32_t check_size, uint32_t buf_size, void *out_buf)
{
    if (entry != NULL) {
        return mp4GetEsDecSpecific(entry, check_size, buf_size, out_buf);
    }
    return MP4_ERR_INVALID_HANDLE;
}

int32_t mp4msv_190E442D(Mp4SampleEntry *entry, Mp4HintSampleDescOut *out_desc)
{
    if (entry == NULL) {
        return MP4_ERR_INVALID_HANDLE;
    }
    if (out_desc == NULL) {
        return MP4_ERR_INVALID_PARAM;
    }
    return mp4GetHintSampleDesc(entry, out_desc);
}

int32_t mp4msv_9878A6B3(Mp4MtsmBox *mtsm, uint32_t *out_count)
{
    if (mtsm == NULL) {
        return MP4_ERR_INVALID_HANDLE;
    }
    if (out_count == NULL) {
        return MP4_ERR_INVALID_PARAM;
    }
    *out_count = mtsm->stsd.entry_count;
    return 0;
}

int32_t mp4msv_C9D3D397(Mp4SampleEntry *entry, uint32_t *out_size)
{
    if (entry == NULL) {
        return MP4_ERR_INVALID_HANDLE;
    }
    if (out_size == NULL) {
        return MP4_ERR_INVALID_PARAM;
    }
    return mp4GetAvcUuidSize(entry, out_size);
}

int32_t mp4msv_D5936DBD(Mp4SampleEntry *entry, uint32_t buf_size, void *out_buf)
{
    if (entry == NULL) {
        return MP4_ERR_INVALID_HANDLE;
    }
    if ((out_buf == NULL) | (buf_size == 0U)) {
        return MP4_ERR_INVALID_PARAM;
    }
    return mp4GetAvcUuidData(entry, buf_size, out_buf);
}

MP4_NOINLINE int32_t mp4FsmCreate(Mp4Fsm **out_fsm)
{
    Mp4Fsm *fsm = (Mp4Fsm *)mp4MemCalloc(sizeof(Mp4Fsm));
    *out_fsm = fsm;
    if (fsm == NULL) {
        return MP4_ERR_NOMEM;
    }
    fsm->cur_state = 0;
    return 0;
}

MP4_NOINLINE int32_t mp4FsmDestroy(Mp4Fsm *fsm)
{
    mp4MemFree(fsm);
    return 0;
}

MP4_NOINLINE int32_t mp4FsmBegin(Mp4Fsm *fsm, int32_t action)
{
    const Mp4StateTrans *tr = &g_mp4_state_table[action][fsm->cur_state];
    if (tr->allowed == 0) {
        return MP4_ERR_BAD_STATE;
    }
    fsm->pending_state = tr->next_state;
    return 0;
}

MP4_NOINLINE int32_t mp4FsmCommit(Mp4Fsm *fsm)
{
    if (fsm->pending_state != -1) {
        fsm->cur_state = fsm->pending_state;
    }
    return 0;
}

MP4_NOINLINE int32_t mp4FsmSwitchToFrag(Mp4Fsm *fsm)
{
    fsm->pending_state = g_mp4_err_table[fsm->pending_state];
    return 0;
}

MP4_NOINLINE int32_t mp4FsmIsPendingCache(const Mp4Fsm *fsm)
{
    return fsm->pending_state == 2;
}

MP4_NOINLINE int32_t mp4FsmIsFragReadOnly(const Mp4Fsm *fsm)
{
    return fsm->cur_state == 9;
}

MP4_NOINLINE int32_t mp4FsmIsCacheMode(const Mp4Fsm *fsm)
{
    return fsm->cur_state == 2;
}

MP4_NOINLINE int32_t mp4SecToDateTime(uint32_t seconds, Mp4DateTime *out_dt)
{
    uint16_t year = 1904;
    uint8_t is_leap = 0;
    uint16_t month_idx;
    uint32_t rem_sec;
    uint32_t day_sec;

    for (;;) {
        uint32_t year_sec;
        uint32_t mod400 = (uint32_t)year % 400U;
        if ((year & 3U) != 0 || mod400 == 100U || mod400 == 200U || mod400 == 300U) {
            year_sec = 31536000U;
            is_leap = 0;
        } else {
            year_sec = 31622400U;
            is_leap = 1;
        }
        if (seconds >= year_sec) {
            seconds -= year_sec;
            year++;
        } else {
            out_dt->year = year;
            break;
        }
    }

    for (month_idx = 0; month_idx < 12; month_idx++) {
        uint32_t month_sec;
        if (month_idx == 1 && is_leap != 0) {
            month_sec = 2505600U;
        } else {
            month_sec = (uint32_t)g_month_days[month_idx] * 86400U;
        }
        if (seconds >= month_sec) {
            seconds -= month_sec;
        } else {
            out_dt->month = (uint16_t)(month_idx + 1U);
            break;
        }
    }

    out_dt->day = (uint16_t)(seconds / 86400U + 1U);
    rem_sec = seconds - (uint32_t)(out_dt->day - 1U) * 86400U;
    out_dt->hour = (uint16_t)(rem_sec / 3600U);
    day_sec = rem_sec - (uint32_t)out_dt->hour * 3600U;
    out_dt->second = (uint16_t)(day_sec % 60U);
    out_dt->minute = (uint16_t)(day_sec / 60U);
    return 0;
}

MP4_NOINLINE int32_t mp4SafeMemcpy(void *dst, uint32_t dst_size, const void *src, uint32_t len)
{
    uint8_t *dst_bytes = (uint8_t *)dst;
    const uint8_t *src_bytes = (const uint8_t *)src;

    if (dst == NULL || src == NULL || dst_size < len) {
        return MP4_ERR_INVALID_PARAM;
    }
    if (dst == src || len == 0) {
        return 0;
    }

    if (src_bytes < dst_bytes && dst_bytes < src_bytes + len) {
        uint32_t step = (uint32_t)(dst_bytes - src_bytes);
        uint8_t *d_cur = dst_bytes + len;
        const uint8_t *s_cur = src_bytes + len;
        uint32_t rem = len;
        while (dst_bytes < d_cur) {
            uint32_t chunk = (rem < step) ? rem : step;
            d_cur -= chunk;
            s_cur -= chunk;
            mp4CbMemcpy(d_cur, chunk, s_cur, chunk);
            rem -= chunk;
        }
        return 0;
    }

    if (dst_bytes < src_bytes && src_bytes < dst_bytes + len) {
        uint32_t step = (uint32_t)(src_bytes - dst_bytes);
        uint8_t *d_cur = dst_bytes;
        const uint8_t *s_cur = src_bytes;
        uint8_t *d_end = dst_bytes + len;
        uint32_t rem = len;
        while (d_cur < d_end) {
            uint32_t chunk = (rem < step) ? rem : step;
            mp4CbMemcpy(d_cur, chunk, s_cur, chunk);
            d_cur += chunk;
            s_cur += chunk;
            rem -= chunk;
        }
        return 0;
    }

    mp4CbMemcpy(dst, dst_size, src, len);
    return 0;
}

MP4_NOINLINE int32_t mp4SeekAbs(void *fh, uint64_t offset)
{
    if (mp4CbSeek(fh, (int64_t)offset, 0) == 0) {
        return 0;
    }
    return MP4_ERR_IO;
}

MP4_NOINLINE int32_t mp4HasCompatBrand(Mp4Handle *handle, uint32_t brand, uint32_t *out_found)
{
    Mp4Movie *movie = handle->movie;
    uint32_t idx;

    if (movie->ftyp.size == 0) {
        return MP4_ERR_CORRUPT;
    }
    *out_found = 0;
    for (idx = 0; idx < movie->ftyp.compat_count; idx++) {
        if (movie->ftyp.compat_brands[idx] == brand) {
            *out_found = 1;
            break;
        }
    }
    return 0;
}

MP4_NOINLINE int32_t mp4FindSampleByTime(Mp4Track *track, uint32_t time_val, uint32_t *out_sample, uint32_t flags, uint32_t *out_sync, uint32_t *out_rem)
{
    uint32_t query_time;
    int32_t ret;

    if ((flags == 0U) & (time_val == 0U)) {
        return MP4_ERR_TIME_RANGE;
    }
    query_time = (flags == 0U) ? (time_val - 1U) : time_val;

    if (track->movie->self->mfra != NULL) {
        ret = mp4FindFragSampleByTime(track, query_time, out_sample, out_sync, out_rem);
        if (ret != 0) {
            return ret;
        }
    } else {
        ret = mp4StblFindSampleByTime(&track->mdia.minf.stbl, query_time, out_sample, out_sync, out_rem, 1);
        if (ret != 0) {
            if (ret == MP4_ERR_NOT_READY) {
                return MP4_ERR_CORRUPT;
            }
            if ((ret == MP4_ERR_COUNT_OVERFLOW) | ((uint32_t)(ret - MP4_ERR_FRAG_OVERFLOW) < 2U)) {
                ret = MP4_ERR_OVERFLOW;
            }
            return ret;
        }
        (*out_sample)++;
    }

    if (flags == 0U) {
        (*out_rem)++;
    }
    return 0;
}

MP4_NOINLINE int32_t mp4FindSyncSample(Mp4Track *track, uint32_t sample_num, int32_t direction, uint32_t *out_sample, uint32_t *out_delta)
{
    int32_t ret;

    if (sample_num == 0U) {
        return MP4_ERR_SAMPLE_RANGE;
    }
    if ((sample_num == 1U) & (direction == 0)) {
        return MP4_ERR_NO_SYNC;
    }
    ret = mp4StblFindSyncSample(&track->mdia.minf.stbl, sample_num - 1U, direction, out_sample, out_delta, 1);
    if (ret == MP4_ERR_NOT_READY) {
        return MP4_ERR_CORRUPT;
    }
    if (((uint32_t)(ret - MP4_ERR_FRAG_OVERFLOW) < 2U) | (ret == MP4_ERR_COUNT_OVERFLOW)) {
        return MP4_ERR_OVERFLOW;
    }
    if (ret == 0) {
        (*out_sample)++;
    }
    return ret;
}

MP4_NOINLINE int32_t mp4GetSampleInfo(Mp4Track *track, uint32_t sample_num, int32_t want_chunk_rem, Mp4SampleInfo *out_info)
{
    int32_t ret;
    Mp4SampleEntry *entry;

    if (sample_num == 0U) {
        return MP4_ERR_SAMPLE_RANGE;
    }
    ret = mp4StblGetSampleInfo(&track->mdia.minf.stbl, sample_num - 1U, want_chunk_rem, out_info, 1);
    if (ret != 0) {
        if (ret == MP4_ERR_NOT_READY) {
            return MP4_ERR_CORRUPT;
        }
        if ((ret == MP4_ERR_COUNT_OVERFLOW) | ((uint32_t)(ret - MP4_ERR_FRAG_OVERFLOW) < 2U)) {
            ret = MP4_ERR_OVERFLOW;
        }
        return ret;
    }
    if (mp4StsdGetEntry(&track->mdia.minf.stbl.stsd, out_info->sample_desc, &entry) != 0) {
        return MP4_ERR_CORRUPT;
    }
    out_info->sample_desc = (uint32_t)(uintptr_t)entry;
    return 0;
}

MP4_NOINLINE int32_t mp4GetSampleTime(Mp4Track *track, uint32_t sample_num, uint32_t *out_time, uint32_t *out_dur)
{
    int32_t ret;

    if (sample_num == 0U) {
        return MP4_ERR_SAMPLE_RANGE;
    }
    ret = mp4StblGetSampleTime(&track->mdia.minf.stbl, sample_num - 1U, out_time, out_dur, 1);
    if (ret != 0) {
        if (ret == MP4_ERR_NOT_READY) {
            return MP4_ERR_CORRUPT;
        }
        if ((ret == MP4_ERR_COUNT_OVERFLOW) | ((uint32_t)(ret - MP4_ERR_FRAG_OVERFLOW) < 2U)) {
            ret = MP4_ERR_OVERFLOW;
        }
        return ret;
    }
    return 0;
}
