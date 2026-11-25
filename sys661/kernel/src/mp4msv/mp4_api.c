#include "mp4msv_internal.h"

/* ============================================================================
 * Global Configuration & OS Callback Registration
 * ============================================================================ */

int mp4msv_9CA13D1A(int enable, const Mp4OsCallbacks *cbs)
{
    if (enable > 0 && cbs != NULL)
        g_mp4_os_cbs = *cbs;
    return MP4_OK;
}

int mp4msv_2A2D949A(void)
{
    return 1;
}

Mp4Config *mp4msv_4A7AD789(void)
{
    return &g_mp4_config;
}

int mp4msv_3C2183C7(int enable, const Mp4Config *cfg)
{
    if (enable > 0 && cfg != NULL)
        g_mp4_config = *cfg;
    return MP4_OK;
}

/* ============================================================================
 * Handle Lifecycle & File Open / Close
 * ============================================================================ */

int mp4msv_F595F917(Mp4Handle **out_handle)
{
    Mp4Config *cfg = mp4msv_4A7AD789();
    u32 base = cfg->base_unit;
    u32 cache_sz = base * cfg->cache_mult;
    u32 max_rd = cache_sz * cfg->max_mult;
    u32 min_rd = base * cfg->min_mult;

    if (max_rd < base)
        return MP4_ERR_INVALID_ARG;

    Mp4Handle *handle = (Mp4Handle *)mp4Alloc(sizeof(Mp4Handle));
    if (handle == NULL)
        return MP4_ERR_NOMEM;
    mp4MemSet(handle, 0, sizeof(*handle));

    Mp4IoStream *stream = (Mp4IoStream *)mp4Alloc(sizeof(Mp4IoStream));
    if (stream == NULL) {
        mp4Free(handle);
        return MP4_ERR_NOMEM;
    }
    mp4MemSet(stream, 0, sizeof(*stream));
    stream->io_mode = 3;
    stream->base_unit = base;
    stream->cache_size = cache_sz;
    stream->max_read = max_rd;
    stream->min_read = (u32)mp4MinS32((s32)max_rd, (s32)min_rd);

    int ret = mp4StateCreate(&stream->fsm);
    if (ret != MP4_OK) {
        mp4Free(stream);
        mp4Free(handle);
        return ret;
    }

    handle->stream = stream;
    *out_handle = handle;
    return MP4_OK;
}

int mp4msv_3D8D41A0(Mp4Handle *handle, void *file_handle)
{
    if (handle == NULL || handle->stream == NULL)
        return MP4_ERR_BAD_HANDLE;
    handle->stream->file_handle = file_handle;
    handle->stream->user_handle = file_handle;
    handle->stream->is_attached = 1;
    return MP4_OK;
}

int mp4msv_9D56A7CC(Mp4Handle *handle)
{
    if (handle == NULL)
        return MP4_ERR_BAD_HANDLE;
    if (handle->stream != NULL) {
        mp4StateDestroy(handle->stream->fsm);
        mp4Free(handle->stream->cache_buf);
        mp4Free(handle->stream);
    }
    mp4MovieDestroy(handle);
    mp4Free(handle);
    return MP4_OK;
}

int mp4msv_0D6843EA(Mp4Handle *handle, u32 opt_id, u32 value)
{
    if (handle == NULL)
        return MP4_ERR_BAD_HANDLE;
    if (opt_id != 0)
        return MP4_ERR_INVALID_ARG;
    if (value != 0)
        handle->options |= 1U;
    else
        handle->options &= ~1U;
    return MP4_OK;
}

int mp4msv_68FBFBE1(Mp4Fragment *frag)
{
    if (frag == NULL || frag->movie == NULL)
        return MP4_ERR_BAD_HANDLE;
    Mp4Movie *movie = frag->movie;
    Mp4Fragment **link = &movie->fragments;
    while (*link != NULL) {
        if (*link == frag) {
            *link = frag->next;
            movie->fragment_count--;
            break;
        }
        link = &(*link)->next;
    }
    mp4Free(frag);
    return MP4_OK;
}

int mp4msv_DEBFCBCA(Mp4Handle *handle, u32 mode)
{
    if (handle == NULL || handle->stream == NULL)
        return MP4_ERR_BAD_HANDLE;
    int ret = mp4StateCheck(handle->stream->fsm, 1);
    if (ret != MP4_OK)
        return ret;
    ret = mp4MovieCreate(handle);
    if (ret != MP4_OK)
        return ret;
    handle->stream->mode = mode;
    ret = mp4ParseTopLevel(handle->movie, 3U, handle->stream);
    if (ret == MP4_OK)
        mp4StateCommit(handle->stream->fsm);
    return ret;
}

int mp4msv_576C693B(Mp4Handle *handle, u32 mode)
{
    if (handle == NULL || handle->stream == NULL)
        return MP4_ERR_BAD_HANDLE;
    int ret = mp4StateCheck(handle->stream->fsm, 2);
    if (ret != MP4_OK)
        return ret;
    ret = mp4MovieCreate(handle);
    if (ret != MP4_OK)
        return ret;
    handle->stream->mode = mode;
    mp4StateCommit(handle->stream->fsm);
    return MP4_OK;
}

int mp4msv_A824645B(u32 num, u32 den, u32 scale, u32 *out_val)
{
    if (den == 0 || out_val == NULL)
        return MP4_ERR_INVALID_ARG;
    u64 prod = (u64)num * (u64)scale;
    *out_val = (u32)(prod / den);
    return MP4_OK;
}

/* ============================================================================
 * UUID Box Queries
 * ============================================================================ */

int mp4msv_A41C1F5F(Mp4Handle *handle, u32 *out_count)
{
    if (handle == NULL || handle->movie == NULL)
        return MP4_ERR_BAD_HANDLE;
    if (out_count == NULL)
        return MP4_ERR_INVALID_ARG;
    u32 count = 0;
    Mp4UuidBox *node = handle->movie->uuid_list;
    while (node != NULL) {
        count++;
        node = node->next;
    }
    *out_count = count;
    return MP4_OK;
}

int mp4msv_B919743C(Mp4Handle *handle, u32 index, u64 *out_offset, u32 *out_size, u32 *out_uuid)
{
    if (handle == NULL || handle->movie == NULL)
        return MP4_ERR_BAD_HANDLE;
    if (index == 0 || out_offset == NULL || out_size == NULL || out_uuid == NULL)
        return MP4_ERR_INVALID_ARG;

    Mp4UuidBox *node = handle->movie->uuid_list;
    u32 cur = 1;
    while (node != NULL && cur < index) {
        node = node->next;
        cur++;
    }
    if (node == NULL)
        return MP4_ERR_NOT_FOUND;

    *out_offset = node->file_offset;
    *out_size = node->size;
    int idx;
    for (idx = 0; idx < 4; idx++)
        out_uuid[idx] = node->uuid[idx];
    return MP4_OK;
}

int mp4msv_032F3500(Mp4Handle *handle, const u32 *uuid, u32 *out_count)
{
    if (handle == NULL || handle->movie == NULL)
        return MP4_ERR_BAD_HANDLE;
    if (uuid == NULL || out_count == NULL)
        return MP4_ERR_INVALID_ARG;

    u32 count = 0;
    Mp4UuidBox *node = handle->movie->uuid_list;
    while (node != NULL) {
        if (node->uuid[0] == uuid[0] && node->uuid[1] == uuid[1] &&
            node->uuid[2] == uuid[2] && node->uuid[3] == uuid[3])
            count++;
        node = node->next;
    }
    *out_count = count;
    return MP4_OK;
}

int mp4msv_509B3BAC(Mp4Handle *handle, const u32 *uuid, u32 nth, u64 *out_offset, u32 *out_size)
{
    if (handle == NULL || handle->movie == NULL)
        return MP4_ERR_BAD_HANDLE;
    if (uuid == NULL || nth == 0 || out_offset == NULL || out_size == NULL)
        return MP4_ERR_INVALID_ARG;

    u32 match = 0;
    Mp4UuidBox *node = handle->movie->uuid_list;
    while (node != NULL) {
        if (node->uuid[0] == uuid[0] && node->uuid[1] == uuid[1] &&
            node->uuid[2] == uuid[2] && node->uuid[3] == uuid[3]) {
            match++;
            if (match == nth) {
                *out_offset = node->file_offset;
                *out_size = node->size;
                return MP4_OK;
            }
        }
        node = node->next;
    }
    return MP4_ERR_NOT_FOUND;
}

/* ============================================================================
 * Metadata, Profile, Brand, & Movie Info Queries
 * ============================================================================ */

int mp4msv_32AB9DCB(Mp4Handle *handle, u32 *out_brand)
{
    if (handle == NULL || handle->stream == NULL)
        return MP4_ERR_BAD_HANDLE;
    if (handle->movie == NULL)
        return MP4_ERR_NOT_OPEN;
    if (handle->movie->ftyp == NULL)
        return MP4_ERR_NO_BRAND;
    if (out_brand != NULL)
        *out_brand = handle->movie->ftyp->major_brand;
    return MP4_OK;
}

int mp4msv_E4AFDAD3(Mp4Handle *handle, u64 *out_mdat_size)
{
    if (handle == NULL || handle->movie == NULL)
        return MP4_ERR_BAD_HANDLE;
    if (handle->movie->mdat_size == 0)
        return MP4_ERR_NOT_FOUND;
    if (out_mdat_size == NULL)
        return MP4_ERR_INVALID_ARG;
    *out_mdat_size = handle->movie->mdat_size;
    return MP4_OK;
}

int mp4msv_4154992A(const u32 *box_hdr, u32 *out_param0, u32 *out_param1)
{
    if (box_hdr == NULL)
        return MP4_ERR_BAD_HANDLE;
    if (out_param0 == NULL || out_param1 == NULL)
        return MP4_ERR_INVALID_ARG;
    if (box_hdr[1] - 2564U >= 2U)
        return MP4_ERR_NOT_FOUND;
    *out_param0 = box_hdr[3];
    *out_param1 = box_hdr[5];
    return MP4_OK;
}

int mp4msv_C38F9765(const u32 *index_tbl, u32 entry_num, u32 *out_track,
                    u32 *out_start, u32 *out_dur, u32 *out_flags)
{
    if (index_tbl == NULL)
        return MP4_ERR_BAD_HANDLE;
    if (entry_num == 0 || out_track == NULL || out_start == NULL ||
        out_dur == NULL || out_flags == NULL)
        return MP4_ERR_INVALID_ARG;
    if (entry_num > index_tbl[5])
        return MP4_ERR_NOT_FOUND;

    const u32 *track_tbl = (const u32 *)index_tbl[7];
    const u32 *item = (const u32 *)index_tbl[6] + (entry_num - 1U) * 3U;
    if (item[0] != track_tbl[7] || item[1] > track_tbl[28])
        return MP4_ERR_NOT_FOUND;

    const u32 *rec = (const u32 *)track_tbl[29] + (item[1] - 1U) * 5U;
    *out_track = (u32)track_tbl;
    *out_start = rec[0];
    *out_dur = rec[1];
    *out_flags = rec[2];
    return MP4_OK;
}

int mp4msv_F50268BF(Mp4Movie *movie, u32 *out_count)
{
    if (movie == NULL)
        return MP4_ERR_BAD_HANDLE;
    if (out_count == NULL)
        return MP4_ERR_INVALID_ARG;
    u32 count = 0;
    Mp4MetaItem *item = movie->meta_list;
    while (item != NULL) {
        count++;
        item = item->next;
    }
    *out_count = count;
    return MP4_OK;
}

int mp4msv_2A296DDA(Mp4Movie *movie, u32 index, Mp4MetaItem **out_item)
{
    if (movie == NULL)
        return MP4_ERR_BAD_HANDLE;
    if (index == 0 || out_item == NULL)
        return MP4_ERR_INVALID_ARG;
    Mp4MetaItem *item = movie->meta_list;
    u32 cur = 1;
    while (item != NULL && cur < index) {
        item = item->next;
        cur++;
    }
    if (item == NULL)
        return MP4_ERR_NOT_FOUND;
    *out_item = item;
    return MP4_OK;
}

int mp4msv_9878A6B3(Mp4Movie *movie, u32 *out_count)
{
    if (movie == NULL)
        return MP4_ERR_BAD_HANDLE;
    if (out_count == NULL)
        return MP4_ERR_INVALID_ARG;
    *out_count = movie->profile_count;
    return MP4_OK;
}

int mp4msv_63752099(Mp4Movie *movie, u32 index, u32 *out_info, int flags)
{
    (void)flags;
    if (movie == NULL)
        return MP4_ERR_BAD_HANDLE;
    if (index == 0 || out_info == NULL)
        return MP4_ERR_INVALID_ARG;
    if (index > movie->profile_count)
        return MP4_ERR_NOT_FOUND;

    Mp4ProfileItem *prof = movie->profiles;
    u32 cur = 1;
    while (prof != NULL && cur < index) {
        prof = prof->next;
        cur++;
    }
    if (prof == NULL || prof->type != MP4_BOX_PRFT)
        return MP4_ERR_NOT_FOUND;

    Mp4MetaItem *meta = NULL;
    if (mp4msv_2A296DDA(movie, prof->ref_index, &meta) != MP4_OK)
        return MP4_ERR_NOT_FOUND;

    out_info[0] = prof->id;
    out_info[1] = meta->type;
    out_info[2] = meta->lang;
    out_info[3] = meta->reserved;
    out_info[4] = meta->size;
    out_info[5] = (u32)meta->data;
    return MP4_OK;
}

/* ============================================================================
 * Track, Sample, & Muxer / Demuxer API Wrappers
 * ============================================================================ */

int mp4msv_98ADC323(Mp4Track *track, u32 sample_size, u32 duration)
{
    if (track == NULL || track->movie == NULL || track->movie->handle == NULL)
        return MP4_ERR_BAD_HANDLE;
    Mp4StateMachine *fsm = track->movie->handle->stream->fsm;
    int ret = mp4StateCheck(fsm, 14);
    if (ret != MP4_OK)
        return ret;
    ret = mp4SampleAppend(track, 0, sample_size, duration, 0, 1);
    if (ret == MP4_OK)
        mp4StateCommit(fsm);
    return ret;
}

int mp4msv_5087548E(Mp4Track *track)
{
    if (track == NULL || track->movie == NULL || track->movie->handle == NULL)
        return MP4_ERR_BAD_HANDLE;
    Mp4StateMachine *fsm = track->movie->handle->stream->fsm;
    int ret = mp4StateCheck(fsm, 13);
    if (ret != MP4_OK)
        return ret;
    mp4StateCommit(fsm);
    return MP4_OK;
}

int mp4msv_8A0FD93F(Mp4Track *track, u64 offset, u32 size, u32 duration, int is_sync)
{
    if (track == NULL || track->movie == NULL || track->movie->handle == NULL)
        return MP4_ERR_BAD_HANDLE;
    Mp4StateMachine *fsm = track->movie->handle->stream->fsm;
    int ret = mp4StateCheck(fsm, 15);
    if (ret != MP4_OK)
        return ret;
    ret = mp4SampleAppend(track, offset, size, duration, 0, is_sync);
    if (ret == MP4_OK)
        mp4StateCommit(fsm);
    return ret;
}

int mp4msv_67AF9E0F(Mp4Handle *handle, u32 handler_type, Mp4Track **out_track)
{
    if (handle == NULL || handle->stream == NULL)
        return MP4_ERR_BAD_HANDLE;
    int ret = mp4StateCheck(handle->stream->fsm, 7);
    if (ret != MP4_OK)
        return ret;
    if (handle->movie == NULL)
        return MP4_ERR_NOT_OPEN;
    Mp4Track *tr = mp4TrackCreate(handle->movie, handler_type);
    if (tr == NULL)
        return MP4_ERR_NOMEM;
    if (out_track != NULL)
        *out_track = tr;
    mp4StateCommit(handle->stream->fsm);
    return MP4_OK;
}

int mp4msv_BE203EA3(Mp4Handle *handle, u32 handler_type, Mp4Track **out_track)
{
    if (handle == NULL || handle->stream == NULL)
        return MP4_ERR_BAD_HANDLE;
    int ret = mp4StateCheck(handle->stream->fsm, 8);
    if (ret != MP4_OK)
        return ret;
    if (handle->movie == NULL)
        return MP4_ERR_NOT_OPEN;
    Mp4Track *tr = mp4TrackCreate(handle->movie, handler_type);
    if (tr == NULL)
        return MP4_ERR_NOMEM;
    if (out_track != NULL)
        *out_track = tr;
    mp4StateCommit(handle->stream->fsm);
    return MP4_OK;
}

int mp4msv_CFF25FAC(Mp4Handle *handle, u32 flags)
{
    (void)flags;
    if (handle == NULL || handle->stream == NULL)
        return MP4_ERR_BAD_HANDLE;
    int ret = mp4StateCheck(handle->stream->fsm, 9);
    if (ret != MP4_OK)
        return ret;
    mp4IoFlush(handle->stream);
    mp4StateCommit(handle->stream->fsm);
    return MP4_OK;
}

int mp4msv_3149D53B(Mp4Handle *handle, u32 flags, u64 *out_size)
{
    (void)flags;
    if (handle == NULL || handle->stream == NULL)
        return MP4_ERR_BAD_HANDLE;
    int ret = mp4StateCheck(handle->stream->fsm, 10);
    if (ret != MP4_OK)
        return ret;
    mp4IoFlush(handle->stream);
    if (out_size != NULL)
        *out_size = handle->stream->file_pos;
    mp4StateCommit(handle->stream->fsm);
    return MP4_OK;
}

/* Movie & Track property getters/setters */
int mp4msv_46186C26(Mp4Handle *handle, u32 *out_val)
{
    if (handle == NULL || handle->movie == NULL) return MP4_ERR_BAD_HANDLE;
    if (out_val != NULL) *out_val = handle->movie->track_count;
    return MP4_OK;
}

int mp4msv_BD23D34A(Mp4Handle *handle, u32 *out_val)
{
    if (handle == NULL || handle->movie == NULL) return MP4_ERR_BAD_HANDLE;
    if (out_val != NULL) *out_val = handle->movie->timescale;
    return MP4_OK;
}

int mp4msv_D8BA6AFE(Mp4Track *track, u32 *out_val)
{
    if (track == NULL) return MP4_ERR_BAD_HANDLE;
    if (out_val != NULL) *out_val = track->track_id;
    return MP4_OK;
}

int mp4msv_0C0843B0(Mp4Track *track, u32 *out_val)
{
    if (track == NULL) return MP4_ERR_BAD_HANDLE;
    if (out_val != NULL) *out_val = track->handler_type;
    return MP4_OK;
}

int mp4msv_089364A9(Mp4Track *track, u32 *out_val)
{
    if (track == NULL) return MP4_ERR_BAD_HANDLE;
    if (out_val != NULL) *out_val = track->timescale;
    return MP4_OK;
}

int mp4msv_E5C33D61(Mp4Track *track, u64 *out_val)
{
    if (track == NULL) return MP4_ERR_BAD_HANDLE;
    if (out_val != NULL) *out_val = track->duration;
    return MP4_OK;
}

int mp4msv_21BD7274(Mp4Track *track, u32 *out_val)
{
    if (track == NULL) return MP4_ERR_BAD_HANDLE;
    if (out_val != NULL) *out_val = track->samples.stsz_count;
    return MP4_OK;
}

int mp4msv_9501780A(Mp4Track *track, u32 *out_val)
{
    if (track == NULL) return MP4_ERR_BAD_HANDLE;
    if (out_val != NULL) *out_val = track->samples.max_sample_size;
    return MP4_OK;
}

int mp4msv_21BE7D96(Mp4Track *track, u32 *out_val)
{
    if (track == NULL) return MP4_ERR_BAD_HANDLE;
    if (out_val != NULL) *out_val = track->codec.format;
    return MP4_OK;
}

int mp4msv_27412E93(Mp4Track *track, u32 use_idx, u32 idx_or_time, u32 *out_val)
{
    if (track == NULL) return MP4_ERR_BAD_HANDLE;
    u32 sample_idx = idx_or_time;
    if (use_idx == 0)
        mp4SampleTimeToIndex(track, idx_or_time, &sample_idx);
    u64 off = 0;
    u32 sz = 0;
    int ret = mp4SampleGetInfo(track, sample_idx, &off, &sz, NULL, NULL, NULL);
    if (ret == MP4_OK && out_val != NULL)
        *out_val = sz;
    return ret;
}

int mp4msv_E641D779(Mp4Track *track, u32 sample_idx, u32 mode, u32 *out_off, u32 *out_sz)
{
    (void)mode;
    if (track == NULL) return MP4_ERR_BAD_HANDLE;
    u64 off = 0;
    u32 sz = 0;
    int ret = mp4SampleGetInfo(track, sample_idx, &off, &sz, NULL, NULL, NULL);
    if (ret == MP4_OK) {
        if (out_off != NULL) *out_off = (u32)off;
        if (out_sz != NULL) *out_sz = sz;
    }
    return ret;
}

int mp4msv_BE558D75(Mp4Track *track, u32 sample_idx, u32 mode, u64 *out_off, u32 *out_sz)
{
    (void)mode;
    if (track == NULL) return MP4_ERR_BAD_HANDLE;
    return mp4SampleGetInfo(track, sample_idx, out_off, out_sz, NULL, NULL, NULL);
}

int mp4msv_CE9C6D39(Mp4Track *track, u32 mode, u32 sample_idx, u64 *out_dts)
{
    (void)mode;
    if (track == NULL) return MP4_ERR_BAD_HANDLE;
    if (out_dts == NULL) return MP4_ERR_INVALID_ARG;
    return mp4SampleGetInfo(track, sample_idx, NULL, NULL, out_dts, NULL, NULL);
}

int mp4msv_5565A318(Mp4Handle *handle, u32 track_idx, u32 sample_idx, u32 flags, u16 *out_val)
{
    (void)flags;
    if (handle == NULL || handle->movie == NULL) return MP4_ERR_BAD_HANDLE;
    Mp4Track *tr = mp4TrackGetByIndex(handle->movie, track_idx);
    if (tr == NULL) return MP4_ERR_NOT_FOUND;
    if (out_val != NULL) *out_val = (u16)(sample_idx < tr->samples.stsz_count ? 1 : 0);
    return MP4_OK;
}

int mp4msv_9076AA02(Mp4Handle *handle, u32 track_idx, void *buf, u32 *out_size,
                    void *meta_buf, u32 meta_len, u32 *out_dts)
{
    if (handle == NULL || handle->movie == NULL) return MP4_ERR_BAD_HANDLE;
    if (buf == NULL || out_size == NULL || out_dts == NULL) return MP4_ERR_INVALID_ARG;
    if (meta_buf != NULL && meta_len < 32U) return MP4_ERR_INVALID_ARG;
    Mp4Track *tr = mp4TrackGetByIndex(handle->movie, track_idx);
    if (tr == NULL) return MP4_ERR_NOT_FOUND;
    u64 off = 0;
    u32 sz = 0;
    u64 dts = 0;
    int ret = mp4SampleGetInfo(tr, tr->samples.cur_sample_idx, &off, &sz, &dts, NULL, NULL);
    if (ret != MP4_OK) return ret;
    mp4IoSeek(handle->stream, (s64)off, 0);
    mp4IoReadBytes(handle->stream, buf, sz);
    *out_size = sz;
    *out_dts = (u32)dts;
    tr->samples.cur_sample_idx++;
    return MP4_OK;
}

int mp4msv_6AE9B944(Mp4Handle *handle, u32 track_idx, void *buf, u32 *out_size,
                    u32 *out_dts, void *meta_buf, u32 meta_len, u32 *out_sync)
{
    if (handle == NULL || handle->movie == NULL) return MP4_ERR_BAD_HANDLE;
    if (buf == NULL || out_size == NULL || out_dts == NULL || out_sync == NULL)
        return MP4_ERR_INVALID_ARG;
    if (meta_buf != NULL && meta_len < 32U) return MP4_ERR_INVALID_ARG;
    Mp4Track *tr = mp4TrackGetByIndex(handle->movie, track_idx);
    if (tr == NULL) return MP4_ERR_NOT_FOUND;
    u64 off = 0;
    u32 sz = 0;
    u64 dts = 0;
    u32 sync = 0;
    int ret = mp4SampleGetInfo(tr, tr->samples.cur_sample_idx, &off, &sz, &dts, NULL, &sync);
    if (ret != MP4_OK) return ret;
    mp4IoSeek(handle->stream, (s64)off, 0);
    mp4IoReadBytes(handle->stream, buf, sz);
    *out_size = sz;
    *out_dts = (u32)dts;
    *out_sync = sync;
    tr->samples.cur_sample_idx++;
    return MP4_OK;
}

int mp4msv_AADD18E7(Mp4Track *track, u32 sample_idx, u32 mode, u32 *out_sync, u32 *out_idx)
{
    if (track == NULL) return MP4_ERR_BAD_HANDLE;
    u32 found = 0;
    int ret = mp4SampleFindSync(track, sample_idx, (mode != 0) ? 1 : 0, &found);
    if (ret == MP4_OK) {
        if (out_sync != NULL) *out_sync = found;
        if (out_idx != NULL) *out_idx = found;
    }
    return ret;
}

int mp4msv_07C60A23(Mp4Handle *handle, u32 *out_val)
{
    if (handle == NULL || handle->movie == NULL) return MP4_ERR_BAD_HANDLE;
    if (out_val != NULL) *out_val = (u32)handle->movie->duration;
    return MP4_OK;
}

int mp4msv_8FD8E0A4(Mp4Handle *handle, u64 *out_val)
{
    if (handle == NULL || handle->movie == NULL) return MP4_ERR_BAD_HANDLE;
    if (out_val != NULL) *out_val = handle->movie->duration;
    return MP4_OK;
}

int mp4msv_E042AB3C(Mp4Handle *handle, u32 *out_val)
{
    if (handle == NULL || handle->movie == NULL) return MP4_ERR_BAD_HANDLE;
    if (out_val != NULL) *out_val = handle->movie->creation_time;
    return MP4_OK;
}

int mp4msv_182FF541(Mp4Handle *handle, u32 *out_val)
{
    if (handle == NULL || handle->movie == NULL) return MP4_ERR_BAD_HANDLE;
    if (out_val != NULL) *out_val = handle->movie->modification_time;
    return MP4_OK;
}

int mp4msv_3D57BA52(Mp4Handle *handle, u32 *out_val)
{
    if (handle == NULL || handle->movie == NULL) return MP4_ERR_BAD_HANDLE;
    if (out_val != NULL) *out_val = handle->movie->next_track_id;
    return MP4_OK;
}

int mp4msv_0C06DACB(Mp4Handle *handle, u32 timescale)
{
    if (handle == NULL || handle->movie == NULL) return MP4_ERR_BAD_HANDLE;
    handle->movie->timescale = timescale;
    return MP4_OK;
}

int mp4msv_8D564A92(Mp4Handle *handle, u32 *out_val)
{
    if (handle == NULL || handle->movie == NULL) return MP4_ERR_BAD_HANDLE;
    if (out_val != NULL) *out_val = handle->movie->fragment_count;
    return MP4_OK;
}

int mp4msv_A55AF94C(Mp4Track *track, u32 timescale)
{
    if (track == NULL) return MP4_ERR_BAD_HANDLE;
    track->timescale = timescale;
    return MP4_OK;
}

int mp4msv_6B94A29D(Mp4Handle *handle, u32 index, Mp4Track **out_track)
{
    if (handle == NULL || handle->movie == NULL) return MP4_ERR_BAD_HANDLE;
    Mp4Track *tr = mp4TrackGetByIndex(handle->movie, index);
    if (tr == NULL) return MP4_ERR_NOT_FOUND;
    if (out_track != NULL) *out_track = tr;
    return MP4_OK;
}

int mp4msv_D3505C12(Mp4Track *track, u32 width_fixed)
{
    if (track == NULL) return MP4_ERR_BAD_HANDLE;
    track->width_fixed = width_fixed;
    return MP4_OK;
}

int mp4msv_A2AE6F30(Mp4Track *track, u32 height_fixed)
{
    if (track == NULL) return MP4_ERR_BAD_HANDLE;
    track->height_fixed = height_fixed;
    return MP4_OK;
}

int mp4msv_54410730(Mp4Track *track, u32 *out_width)
{
    if (track == NULL) return MP4_ERR_BAD_HANDLE;
    if (out_width != NULL) *out_width = track->width_fixed;
    return MP4_OK;
}

int mp4msv_EE62F96D(Mp4Track *track, u16 language)
{
    if (track == NULL) return MP4_ERR_BAD_HANDLE;
    track->language = language;
    return MP4_OK;
}

int mp4msv_9BB5E338(Mp4Track *track, u16 *out_language)
{
    if (track == NULL) return MP4_ERR_BAD_HANDLE;
    if (out_language != NULL) *out_language = track->language;
    return MP4_OK;
}

int mp4msv_FF9E6F58(Mp4Track *track, u16 volume)
{
    if (track == NULL) return MP4_ERR_BAD_HANDLE;
    track->volume = volume;
    return MP4_OK;
}

int mp4msv_3F6B6C59(Mp4Track *track, u16 *out_volume)
{
    if (track == NULL) return MP4_ERR_BAD_HANDLE;
    if (out_volume != NULL) *out_volume = track->volume;
    return MP4_OK;
}

int mp4msv_5EE48079(Mp4Track *track, u32 *out_height)
{
    if (track == NULL) return MP4_ERR_BAD_HANDLE;
    if (out_height != NULL) *out_height = track->height_fixed;
    return MP4_OK;
}

int mp4msv_162503F3(Mp4Track *track, u32 flags)
{
    if (track == NULL) return MP4_ERR_BAD_HANDLE;
    track->flags = flags;
    return MP4_OK;
}

int mp4msv_A9FD62E2(Mp4Track *track, u32 *out_flags)
{
    if (track == NULL) return MP4_ERR_BAD_HANDLE;
    if (out_flags != NULL) *out_flags = track->flags;
    return MP4_OK;
}

int mp4msv_6D619263(Mp4Track *track, u32 format)
{
    if (track == NULL) return MP4_ERR_BAD_HANDLE;
    track->codec.format = format;
    return MP4_OK;
}

int mp4msv_2BAF7C25(Mp4Track *track, u32 *out_val)
{
    if (track == NULL) return MP4_ERR_BAD_HANDLE;
    if (out_val != NULL) *out_val = track->codec.sample_rate;
    return MP4_OK;
}

int mp4msv_A418509D(Mp4Track *track, u32 *out_val)
{
    if (track == NULL) return MP4_ERR_BAD_HANDLE;
    if (out_val != NULL) *out_val = track->codec.channels;
    return MP4_OK;
}

int mp4msv_8C998600(Mp4Track *track, u32 *out_val)
{
    if (track == NULL) return MP4_ERR_BAD_HANDLE;
    if (out_val != NULL) *out_val = track->samples.stss_count;
    return MP4_OK;
}

int mp4msv_35144B9C(Mp4Track *track, u32 *out_val)
{
    if (track == NULL) return MP4_ERR_BAD_HANDLE;
    if (out_val != NULL) *out_val = track->samples.stts_count;
    return MP4_OK;
}

int mp4msv_2B1B8D23(Mp4Track *track, u32 sample_idx, u32 *out_size)
{
    if (track == NULL) return MP4_ERR_BAD_HANDLE;
    return mp4SampleGetInfo(track, sample_idx, NULL, out_size, NULL, NULL, NULL);
}

int mp4msv_A09878FC(Mp4Track *track, u32 sample_idx, u64 *out_offset)
{
    if (track == NULL) return MP4_ERR_BAD_HANDLE;
    return mp4SampleGetInfo(track, sample_idx, out_offset, NULL, NULL, NULL, NULL);
}

int mp4msv_08F96BE2(Mp4Track *track, u64 target_time, u32 *out_idx)
{
    if (track == NULL) return MP4_ERR_BAD_HANDLE;
    return mp4SampleTimeToIndex(track, target_time, out_idx);
}

int mp4msv_287B3FDA(Mp4Track *track, u32 sample_idx)
{
    if (track == NULL) return MP4_ERR_BAD_HANDLE;
    track->samples.cur_sample_idx = sample_idx;
    return MP4_OK;
}

int mp4msv_886CFC3D(Mp4Track *track, u32 *out_idx)
{
    if (track == NULL) return MP4_ERR_BAD_HANDLE;
    if (out_idx != NULL) *out_idx = track->samples.cur_sample_idx;
    return MP4_OK;
}

int mp4msv_9632797F(u16 *hdr, int has_len, int len, void *out_buf)
{
    if (hdr == NULL) return MP4_ERR_BAD_HANDLE;
    int copy_len = has_len ? len : ((int)*hdr - 10);
    if (copy_len > 0 && out_buf != NULL)
        mp4MemCopy(out_buf, (const u8 *)hdr + 10, (u32)copy_len);
    return MP4_OK;
}

int mp4msv_EF6D76BC(Mp4Handle *handle, u32 track_id, Mp4Track **out_track)
{
    if (handle == NULL || handle->movie == NULL) return MP4_ERR_BAD_HANDLE;
    Mp4Track *tr = mp4TrackGetById(handle->movie, track_id);
    if (tr == NULL) return MP4_ERR_NOT_FOUND;
    if (out_track != NULL) *out_track = tr;
    return MP4_OK;
}

int mp4msv_4C677A7E(Mp4Handle *handle, u32 handler_type, Mp4Track **out_track)
{
    if (handle == NULL || handle->movie == NULL) return MP4_ERR_BAD_HANDLE;
    Mp4Track *tr = handle->movie->tracks;
    while (tr != NULL) {
        if (tr->handler_type == handler_type) {
            if (out_track != NULL) *out_track = tr;
            return MP4_OK;
        }
        tr = tr->next;
    }
    return MP4_ERR_NOT_FOUND;
}

int mp4msv_0DF0B9AD(Mp4Handle *handle, u32 brand)
{
    if (handle == NULL || handle->movie == NULL) return MP4_ERR_BAD_HANDLE;
    if (handle->movie->ftyp == NULL) {
        handle->movie->ftyp = (Mp4FtypBox *)mp4Alloc(sizeof(Mp4FtypBox));
        if (handle->movie->ftyp == NULL) return MP4_ERR_NOMEM;
        mp4MemSet(handle->movie->ftyp, 0, sizeof(Mp4FtypBox));
    }
    handle->movie->ftyp->major_brand = brand;
    return MP4_OK;
}

int mp4msv_AD3AF34E(Mp4Handle *handle, u32 minor_ver)
{
    if (handle == NULL || handle->movie == NULL) return MP4_ERR_BAD_HANDLE;
    if (handle->movie->ftyp == NULL) return MP4_ERR_NO_BRAND;
    handle->movie->ftyp->minor_version = minor_ver;
    return MP4_OK;
}

int mp4msv_0D32271B(Mp4Handle *handle, u32 *out_minor)
{
    if (handle == NULL || handle->movie == NULL) return MP4_ERR_BAD_HANDLE;
    if (handle->movie->ftyp == NULL) return MP4_ERR_NO_BRAND;
    if (out_minor != NULL) *out_minor = handle->movie->ftyp->minor_version;
    return MP4_OK;
}

int mp4msv_D070C0AA(Mp4Handle *handle, u32 compat_brand)
{
    if (handle == NULL || handle->movie == NULL) return MP4_ERR_BAD_HANDLE;
    if (handle->movie->ftyp == NULL) return MP4_ERR_NO_BRAND;
    Mp4FtypBox *ftyp = handle->movie->ftyp;
    if (ftyp->compat_count < 16U)
        ftyp->compat_brands[ftyp->compat_count++] = compat_brand;
    return MP4_OK;
}

int mp4msv_888C1115(Mp4Handle *handle, u32 *out_count)
{
    if (handle == NULL || handle->movie == NULL) return MP4_ERR_BAD_HANDLE;
    if (handle->movie->ftyp == NULL) return MP4_ERR_NO_BRAND;
    if (out_count != NULL) *out_count = handle->movie->ftyp->compat_count;
    return MP4_OK;
}

int mp4msv_E8DE0EB8(Mp4Track *track, u32 *out_sps_size)
{
    if (track == NULL) return MP4_ERR_BAD_HANDLE;
    if (out_sps_size != NULL) *out_sps_size = track->codec.sps_size;
    return MP4_OK;
}

int mp4msv_CD733822(Mp4Track *track, u32 sample_idx, u32 *out_sync)
{
    if (track == NULL) return MP4_ERR_BAD_HANDLE;
    return mp4SampleFindSync(track, sample_idx, 0, out_sync);
}

int mp4msv_9A8B21D2(Mp4Handle *handle, u32 index, u32 *out_brand)
{
    if (handle == NULL || handle->movie == NULL) return MP4_ERR_BAD_HANDLE;
    if (handle->movie->ftyp == NULL || index >= handle->movie->ftyp->compat_count)
        return MP4_ERR_NOT_FOUND;
    if (out_brand != NULL) *out_brand = handle->movie->ftyp->compat_brands[index];
    return MP4_OK;
}

int mp4msv_65C17387(Mp4Track *track, const u8 *sps, u32 sps_size)
{
    if (track == NULL) return MP4_ERR_BAD_HANDLE;
    mp4Free(track->codec.sps_data);
    track->codec.sps_data = NULL;
    track->codec.sps_size = 0;
    if (sps != NULL && sps_size > 0) {
        track->codec.sps_data = (u8 *)mp4Alloc(sps_size);
        if (track->codec.sps_data == NULL) return MP4_ERR_NOMEM;
        mp4MemCopy(track->codec.sps_data, sps, sps_size);
        track->codec.sps_size = sps_size;
    }
    return MP4_OK;
}

int mp4msv_2BFE233D(Mp4Track *track, const u8 *pps, u32 pps_size)
{
    if (track == NULL) return MP4_ERR_BAD_HANDLE;
    mp4Free(track->codec.pps_data);
    track->codec.pps_data = NULL;
    track->codec.pps_size = 0;
    if (pps != NULL && pps_size > 0) {
        track->codec.pps_data = (u8 *)mp4Alloc(pps_size);
        if (track->codec.pps_data == NULL) return MP4_ERR_NOMEM;
        mp4MemCopy(track->codec.pps_data, pps, pps_size);
        track->codec.pps_size = pps_size;
    }
    return MP4_OK;
}

int mp4msv_190E442D(Mp4Track *track, const void *cfg_buf)
{
    if (track == NULL) return MP4_ERR_BAD_HANDLE;
    if (cfg_buf == NULL) return MP4_ERR_INVALID_ARG;
    const u32 *vals = (const u32 *)cfg_buf;
    track->codec.width = vals[0];
    track->codec.height = vals[1];
    return MP4_OK;
}

int mp4msv_C9D3D397(Mp4Track *track, const void *cfg_buf)
{
    if (track == NULL) return MP4_ERR_BAD_HANDLE;
    if (cfg_buf == NULL) return MP4_ERR_INVALID_ARG;
    const u32 *vals = (const u32 *)cfg_buf;
    track->codec.sample_rate = vals[0];
    track->codec.channels = vals[1];
    return MP4_OK;
}

int mp4msv_D5936DBD(Mp4Track *track, void *out_buf, u32 *io_size)
{
    if (track == NULL) return MP4_ERR_BAD_HANDLE;
    if (out_buf == NULL || io_size == NULL) return MP4_ERR_INVALID_ARG;
    u32 avail = track->codec.esds_size;
    if (*io_size < avail) avail = *io_size;
    if (avail > 0 && track->codec.esds_data != NULL)
        mp4MemCopy(out_buf, track->codec.esds_data, avail);
    *io_size = avail;
    return MP4_OK;
}
