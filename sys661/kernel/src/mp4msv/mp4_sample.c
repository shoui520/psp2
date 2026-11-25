#include "mp4msv_internal.h"

Mp4TableChunk *mp4ChunkAlloc(u32 capacity, u32 stride_words)
{
    if (capacity == 0 || stride_words == 0)
        return NULL;
    Mp4TableChunk *chunk = (Mp4TableChunk *)mp4Alloc(sizeof(Mp4TableChunk));
    if (chunk == NULL)
        return NULL;
    mp4MemSet(chunk, 0, sizeof(*chunk));
    chunk->entries = (u32 *)mp4Alloc(capacity * stride_words * sizeof(u32));
    if (chunk->entries == NULL) {
        mp4Free(chunk);
        return NULL;
    }
    chunk->capacity = capacity;
    chunk->stride_words = stride_words;
    return chunk;
}

void mp4ChunkListFree(Mp4TableChunk *head)
{
    while (head != NULL) {
        Mp4TableChunk *next = head->next;
        mp4Free(head->entries);
        mp4Free(head);
        head = next;
    }
}

int mp4ChunkGetWord(const Mp4TableChunk *head, u32 row, u32 col, u32 *out_val)
{
    if (head == NULL || col >= head->stride_words)
        return MP4_ERR_INTERNAL;
    const Mp4TableChunk *cur = head;
    while (cur != NULL) {
        if (row < cur->count) {
            *out_val = cur->entries[row * cur->stride_words + col];
            return MP4_OK;
        }
        row -= cur->count;
        cur = cur->next;
    }
    return MP4_ERR_INTERNAL;
}

int mp4ChunkAppendRow(Mp4TableChunk **head_ptr, u32 stride_words, const u32 *row_words)
{
    if (head_ptr == NULL || row_words == NULL || stride_words == 0)
        return MP4_ERR_INVALID_ARG;
    Mp4TableChunk *cur = *head_ptr;
    if (cur == NULL) {
        cur = mp4ChunkAlloc(64, stride_words);
        if (cur == NULL)
            return MP4_ERR_NOMEM;
        *head_ptr = cur;
    } else {
        while (cur->next != NULL)
            cur = cur->next;
        if (cur->count >= cur->capacity) {
            Mp4TableChunk *next = mp4ChunkAlloc(64, stride_words);
            if (next == NULL)
                return MP4_ERR_NOMEM;
            cur->next = next;
            cur = next;
        }
    }

    u32 base = cur->count * cur->stride_words;
    u32 idx;
    for (idx = 0; idx < stride_words; idx++)
        cur->entries[base + idx] = row_words[idx];
    cur->count++;
    return MP4_OK;
}

int mp4StscLookup(Mp4StscTable *stsc, u32 sample_num, u32 *out_chunk,
                  u32 *out_first_sample, u32 *out_desc_idx)
{
    if (stsc == NULL || stsc->valid == 0)
        return MP4_ERR_BAD_STATE;
    if (sample_num < stsc->first_sample ||
        sample_num >= stsc->first_sample + stsc->sample_count)
        return MP4_ERR_OUT_OF_RANGE;
    if (stsc->chunks == NULL || stsc->entry_count == 0)
        return MP4_ERR_INTERNAL;

    u32 cur_sample = stsc->first_sample;
    u32 idx;
    for (idx = 0; idx < stsc->entry_count; idx++) {
        u32 first_chunk = 0;
        u32 samples_per_chunk = 0;
        u32 desc_idx = 0;
        if (mp4ChunkGetWord(stsc->chunks, idx, 0, &first_chunk) != MP4_OK ||
            mp4ChunkGetWord(stsc->chunks, idx, 1, &samples_per_chunk) != MP4_OK ||
            mp4ChunkGetWord(stsc->chunks, idx, 2, &desc_idx) != MP4_OK)
            return MP4_ERR_INTERNAL;
        if (samples_per_chunk == 0)
            return MP4_ERR_INTERNAL;

        u32 next_chunk = stsc->first_chunk + stsc->total_chunks + 1U;
        if (idx + 1U < stsc->entry_count) {
            if (mp4ChunkGetWord(stsc->chunks, idx + 1U, 0, &next_chunk) != MP4_OK)
                return MP4_ERR_INTERNAL;
        }

        u32 span_samples = (next_chunk - first_chunk) * samples_per_chunk;
        if (sample_num < cur_sample + span_samples) {
            u32 chunk_offset = (sample_num - cur_sample) / samples_per_chunk;
            *out_chunk = (first_chunk - 1U) + chunk_offset;
            *out_first_sample = cur_sample + chunk_offset * samples_per_chunk;
            *out_desc_idx = desc_idx;
            stsc->cached_entry = stsc->first_entry + idx;
            stsc->cached_sample = cur_sample;
            return MP4_OK;
        }
        cur_sample += span_samples;
    }
    return MP4_ERR_BAD_STATE;
}

int mp4SampleGetInfo(Mp4Track *track, u32 sample_idx, u64 *out_offset,
                     u32 *out_size, u64 *out_dts, s32 *out_cts, u32 *out_sync)
{
    if (track == NULL)
        return MP4_ERR_BAD_HANDLE;
    Mp4SampleTable *st = &track->samples;
    if (sample_idx >= st->stsz_count)
        return MP4_ERR_OUT_OF_RANGE;

    u32 sample_size = st->stsz_fixed_size;
    if (sample_size == 0) {
        if (mp4ChunkGetWord(st->stsz_chunks, sample_idx, 0, &sample_size) != MP4_OK)
            return MP4_ERR_INTERNAL;
    }
    if (out_size != NULL)
        *out_size = sample_size;

    if (out_offset != NULL) {
        u32 chunk_idx = 0;
        u32 first_sample = 1;
        u32 desc_idx = 1;
        u64 base_off = 0;
        if (mp4StscLookup(&st->stsc, sample_idx + 1U, &chunk_idx, &first_sample, &desc_idx) == MP4_OK) {
            u32 chunk_off = 0;
            if (mp4ChunkGetWord(st->stco_chunks, chunk_idx, 0, &chunk_off) == MP4_OK)
                base_off = chunk_off;
            u32 s;
            for (s = first_sample - 1U; s < sample_idx; s++) {
                u32 prev_sz = st->stsz_fixed_size;
                if (prev_sz == 0)
                    mp4ChunkGetWord(st->stsz_chunks, s, 0, &prev_sz);
                base_off += prev_sz;
            }
        }
        *out_offset = base_off;
    }

    if (out_dts != NULL) {
        u64 dts = 0;
        u32 rem = sample_idx;
        u32 idx;
        for (idx = 0; idx < st->stts_count && rem > 0; idx++) {
            u32 cnt = 0;
            u32 delta = 0;
            mp4ChunkGetWord(st->stts_chunks, idx, 0, &cnt);
            mp4ChunkGetWord(st->stts_chunks, idx, 1, &delta);
            if (rem < cnt) {
                dts += (u64)rem * delta;
                rem = 0;
            } else {
                dts += (u64)cnt * delta;
                rem -= cnt;
            }
        }
        *out_dts = dts;
    }

    if (out_cts != NULL)
        *out_cts = 0;

    if (out_sync != NULL) {
        if (st->stss_count == 0) {
            *out_sync = 1;
        } else {
            u32 is_sync = 0;
            u32 idx;
            for (idx = 0; idx < st->stss_count; idx++) {
                u32 sync_num = 0;
                mp4ChunkGetWord(st->stss_chunks, idx, 0, &sync_num);
                if (sync_num == sample_idx + 1U) {
                    is_sync = 1;
                    break;
                }
            }
            *out_sync = is_sync;
        }
    }
    return MP4_OK;
}

int mp4SampleTimeToIndex(Mp4Track *track, u64 target_time, u32 *out_idx)
{
    if (track == NULL || out_idx == NULL)
        return MP4_ERR_INVALID_ARG;
    Mp4SampleTable *st = &track->samples;
    u64 cur_time = 0;
    u32 sample_idx = 0;
    u32 idx;
    for (idx = 0; idx < st->stts_count; idx++) {
        u32 cnt = 0;
        u32 delta = 0;
        mp4ChunkGetWord(st->stts_chunks, idx, 0, &cnt);
        mp4ChunkGetWord(st->stts_chunks, idx, 1, &delta);
        u64 span = (u64)cnt * delta;
        if (target_time < cur_time + span && delta > 0) {
            *out_idx = sample_idx + (u32)((target_time - cur_time) / delta);
            return MP4_OK;
        }
        cur_time += span;
        sample_idx += cnt;
    }
    *out_idx = (st->stsz_count > 0) ? (st->stsz_count - 1U) : 0U;
    return MP4_OK;
}

int mp4SampleFindSync(Mp4Track *track, u32 sample_idx, int forward, u32 *out_idx)
{
    if (track == NULL || out_idx == NULL)
        return MP4_ERR_INVALID_ARG;
    Mp4SampleTable *st = &track->samples;
    if (st->stss_count == 0) {
        *out_idx = sample_idx;
        return MP4_OK;
    }
    u32 target = sample_idx + 1U;
    u32 best = 1U;
    u32 idx;
    for (idx = 0; idx < st->stss_count; idx++) {
        u32 sync_num = 0;
        mp4ChunkGetWord(st->stss_chunks, idx, 0, &sync_num);
        if (forward) {
            if (sync_num >= target) {
                *out_idx = sync_num - 1U;
                return MP4_OK;
            }
        } else {
            if (sync_num <= target)
                best = sync_num;
            else
                break;
        }
    }
    *out_idx = (best > 0) ? (best - 1U) : 0U;
    return MP4_OK;
}

int mp4SampleAppend(Mp4Track *track, u64 offset, u32 size, u32 duration, s32 cts_offset, int is_sync)
{
    if (track == NULL)
        return MP4_ERR_BAD_HANDLE;
    Mp4SampleTable *st = &track->samples;
    (void)cts_offset;

    u32 off32 = (u32)offset;
    int ret = mp4ChunkAppendRow(&st->stco_chunks, 1, &off32);
    if (ret != MP4_OK)
        return ret;
    st->stco_count++;

    ret = mp4ChunkAppendRow(&st->stsz_chunks, 1, &size);
    if (ret != MP4_OK)
        return ret;
    st->stsz_count++;
    if (size > st->max_sample_size)
        st->max_sample_size = size;

    u32 stts_row[2] = { 1, duration };
    ret = mp4ChunkAppendRow(&st->stts_chunks, 2, stts_row);
    if (ret != MP4_OK)
        return ret;
    st->stts_count++;

    if (is_sync) {
        u32 sample_num = st->stsz_count;
        mp4ChunkAppendRow(&st->stss_chunks, 1, &sample_num);
        st->stss_count++;
    }
    track->duration += duration;
    return MP4_OK;
}
