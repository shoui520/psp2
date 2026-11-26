#include "mp4msv_internal.h"

#define MP4_ERR_DYNARRAY_RANGE 12

/* 0x02040c */
MP4_NOINLINE int32_t mp4StblGetFragMoofOffset(Mp4Stbl *stbl, uint64_t *out_offset)
{
    *out_offset = stbl->frag_moof_offset;
    return MP4_OK;
}

/* 0x020424 */
MP4_NOINLINE int32_t mp4StblAdvanceOneFrag(Mp4Stbl *stbl)
{
    return mp4StblLoadNextFrag(stbl);
}

/* 0x020440 */
MP4_NOINLINE int32_t mp4StblGetLoadedRange(Mp4Stbl *stbl, uint32_t *out_start, uint32_t *out_count)
{
    if (mp4FsmIsCacheMode(mp4StblGetFsm(stbl)) == 1) {
        return mp4CacheGetSampleWindow(stbl->cache, out_start, out_count);
    }
    if (stbl->frag_ctx == NULL) {
        return MP4_ERR_NOT_READY;
    }
    *out_start = stbl->frag_ctx->base_sample;
    *out_count = stbl->stsz.sample_count;
    return MP4_OK;
}

/* 0x0204c8 */
MP4_NOINLINE int32_t mp4StblGetFragRange(Mp4Stbl *stbl, uint32_t *out_has_stss, uint32_t *out_count)
{
    if (stbl->stss.size != 0U) {
        *out_has_stss = 1U;
        *out_count = stbl->stss.entry_count;
    } else {
        *out_has_stss = 0U;
        *out_count = 0U;
    }
    return MP4_OK;
}

/* 0x0204f4 */
MP4_NOINLINE int32_t mp4StblEnsureTime(Mp4Stbl *stbl, uint32_t time_val, int32_t allow_io)
{
    Mp4StblFragCtx *frag = stbl->frag_ctx;

    if (frag == NULL) {
        return MP4_ERR_NOT_READY;
    }
    if (frag->is_complete == 1U && time_val >= frag->total_duration) {
        return MP4_ERR_TIME_RANGE;
    }
    if (time_val >= frag->base_time && time_val < frag->base_time + frag->loaded_duration) {
        return MP4_OK;
    }
    if (allow_io == 0) {
        return MP4_ERR_IO;
    }

    if (time_val < frag->base_time) {
        if (mp4StblResetFrags(stbl) != MP4_OK) {
            return MP4_ERR_INTERNAL;
        }
    }

    for (;;) {
        int32_t err;
        frag = stbl->frag_ctx;
        if (time_val >= frag->base_time && time_val < frag->base_time + frag->loaded_duration) {
            return MP4_OK;
        }
        err = mp4StblLoadNextFrag(stbl);
        if (err != MP4_OK) {
            if (err == MP4_ERR_EOF_FRAG) {
                return MP4_ERR_TIME_RANGE;
            }
            return err;
        }
    }
}

/* 0x0205ec */
MP4_NOINLINE int32_t mp4StblLookupTime(
    Mp4Stbl *stbl,
    uint32_t time_val,
    uint32_t *out_sample,
    uint32_t *out_rem)
{
    Mp4StblFragCtx *frag;
    Mp4SttsBox *stts;
    uint32_t cur_entry;
    uint32_t cur_time;

    if (stbl->stts.size == 0U) {
        return MP4_ERR_CORRUPT;
    }
    frag = stbl->frag_ctx;
    if (frag->base_time + frag->loaded_duration < time_val || time_val < frag->base_time) {
        return MP4_ERR_TIME_RANGE;
    }

    stts = &stbl->stts;
    cur_entry = stts->cursor_entry;
    if (cur_entry == 0U) {
        stts->cursor_sample = frag->base_sample;
        stts->cursor_time = frag->base_time;
    }

    cur_time = stts->cursor_time;
    if (time_val < cur_time) {
        if (time_val - frag->base_time < cur_time - time_val) {
            cur_entry = 0U;
            cur_time = frag->base_time;
            stts->cursor_entry = 0U;
            stts->cursor_sample = frag->base_sample;
            stts->cursor_time = cur_time;
        }
    } else if (cur_time < time_val) {
        uint32_t last_entry = stts->entry_count - 1U;
        if (cur_entry != last_entry &&
            frag->base_time + frag->loaded_duration - time_val < time_val - cur_time) {
            uint32_t last_cnt = mp4SttsGetWord(stts, last_entry, 0U);
            uint32_t last_delta = mp4SttsGetWord(stts, last_entry, 1U);
            stts->cursor_entry = last_entry;
            cur_entry = last_entry;
            stts->cursor_sample = frag->base_sample + stbl->stsz.sample_count - last_cnt;
            cur_time = stbl->frag_ctx->base_time + stbl->frag_ctx->loaded_duration - last_cnt * last_delta;
            stts->cursor_time = cur_time;
        }
    }

    if (time_val < cur_time) {
        uint32_t scan = cur_entry - 1U;
        uint32_t acc_time = cur_time;
        uint32_t acc_samples = 0U;

        if (scan >= stts->entry_count) {
            return MP4_ERR_CORRUPT;
        }
        for (;;) {
            uint32_t run_samples = mp4SttsGetWord(stts, scan, 0U);
            uint32_t run_delta = mp4SttsGetWord(stts, scan, 1U);
            acc_time -= run_samples * run_delta;
            acc_samples += run_samples;
            if (time_val >= acc_time) {
                uint32_t step = (time_val - acc_time) / run_delta;
                *out_sample = stts->cursor_sample - acc_samples + step;
                *out_rem = time_val - (acc_time + step * run_delta);
                stts->cursor_time = acc_time;
                stts->cursor_entry = scan;
                stts->cursor_sample -= acc_samples;
                return MP4_OK;
            }
            if (scan == 0U) {
                return MP4_ERR_CORRUPT;
            }
            scan--;
        }
    }

    if (cur_entry >= stts->entry_count) {
        return MP4_ERR_CORRUPT;
    }

    {
        uint32_t scan = cur_entry;
        uint32_t acc_time = cur_time;
        uint32_t acc_samples = 0U;

        while (scan < stts->entry_count) {
            uint32_t run_samples = mp4SttsGetWord(stts, scan, 0U);
            uint32_t run_delta = mp4SttsGetWord(stts, scan, 1U);
            if (time_val < acc_time + run_samples * run_delta) {
                uint32_t step = (time_val - acc_time) / run_delta;
                *out_sample = stts->cursor_sample + acc_samples + step;
                *out_rem = time_val - (acc_time + step * run_delta);
                stts->cursor_time = acc_time;
                stts->cursor_entry = scan;
                stts->cursor_sample += acc_samples;
                return MP4_OK;
            }
            acc_time += run_samples * run_delta;
            acc_samples += run_samples;
            scan++;
        }
    }

    return MP4_ERR_CORRUPT;
}

/* 0x020960 */
MP4_NOINLINE int32_t mp4StblGetSampleOffset(
    Mp4Stbl *stbl,
    uint32_t sample_idx,
    uint64_t *out_offset,
    uint32_t *out_chunk)
{
    uint32_t base_sample = stbl->frag_ctx->base_sample;
    uint32_t chunk_first_sample;
    uint64_t chunk_off;

    if (sample_idx >= base_sample + stbl->stsz.sample_count || sample_idx < base_sample) {
        return MP4_ERR_SAMPLE_RANGE;
    }
    if (mp4StblFindChunkForSample(stbl, sample_idx, out_chunk, &chunk_first_sample) != MP4_OK) {
        return MP4_ERR_CORRUPT;
    }

    if (chunk_first_sample < sample_idx) {
        uint32_t delta_bytes;
        if (mp4StblSumSampleSizes(stbl, chunk_first_sample, sample_idx - 1U, &delta_bytes) != MP4_OK) {
            return MP4_ERR_CORRUPT;
        }
        if (mp4ChunkArrayGetU64(stbl->stco.entries, *out_chunk - stbl->frag_ctx->base_chunk, 0U, &chunk_off) != MP4_OK) {
            chunk_off = 0xFFFFFFFFFFFFFFFFULL;
        }
        *out_offset = chunk_off + (uint64_t)delta_bytes;
        return MP4_OK;
    }

    if (mp4ChunkArrayGetU64(stbl->stco.entries, *out_chunk - stbl->frag_ctx->base_chunk, 0U, &chunk_off) != MP4_OK) {
        chunk_off = 0xFFFFFFFFFFFFFFFFULL;
    }
    *out_offset = chunk_off;
    return MP4_OK;
}

/* 0x020ad0 - Export NID 0x32AB9DCB */
MP4_NOINLINE int32_t mp4msv_32AB9DCB(Mp4Handle *handle, uint32_t *out_val)
{
    if (handle == NULL || handle->stream == NULL) {
        return MP4_ERR_INVALID_HANDLE;
    }
    if (handle->movie == NULL) {
        return MP4_ERR_NOT_READY;
    }
    if (handle->movie->mvex == NULL) {
        return MP4_ERR_NO_ENTRY;
    }
    *out_val = handle->movie->mvex->mehd.fragment_duration;
    return MP4_OK;
}

/* 0x020b10 */
MP4_NOINLINE int32_t mp4FreeOdsmContexts(Mp4Handle *handle)
{
    Mp4Movie *movie = handle->movie;
    uint32_t idx;

    for (idx = 0U; idx < movie->track_count; idx++) {
        Mp4Track *trk = mp4MovieGetTrackAt(movie, idx);
        if (trk->odsm_ctx != NULL) {
            mp4OdsmFreeCtx(trk->odsm_ctx);
        }
    }
    return MP4_OK;
}

/* 0x020b8c */
MP4_NOINLINE int32_t mp4DynArrayPush(Mp4DynArray *arr, const void *elem)
{
    uint32_t idx = arr->count;
    Mp4DynBlock *blk;

    if (idx == arr->capacity) {
        int32_t err = mp4DynArrayGrow(arr);
        if (err != MP4_OK) {
            return err;
        }
        idx = arr->count;
    }

    arr->count = idx + 1U;
    if (idx >= idx + 1U) {
        return MP4_ERR_DYNARRAY_RANGE;
    }

    blk = mp4DynArrayGetBlock(arr, idx / arr->block_cap);
    {
        int32_t wr_err = mp4DynBlockWrite(blk, (idx % arr->block_cap) * arr->elem_size, arr->elem_size,
                                          (const uint8_t *)elem);
        if (wr_err != MP4_OK) {
            return wr_err;
        }
    }
    return MP4_OK;
}

/* 0x020c64 */
MP4_NOINLINE int32_t mp4DynArraySet(Mp4DynArray *arr, uint32_t index, const void *elem)
{
    uint32_t cur_cnt = arr->count;
    Mp4DynBlock *target_blk;
    Mp4DynBlock *walk_blk;
    int32_t err;

    if (cur_cnt < index) {
        return MP4_ERR_DYNARRAY_RANGE;
    }

    if (cur_cnt == arr->capacity) {
        err = mp4DynArrayGrow(arr);
        if (err != MP4_OK) {
            return err;
        }
        cur_cnt = arr->count;
    }

    arr->count = cur_cnt + 1U;
    target_blk = mp4DynArrayGetBlock(arr, index / arr->block_cap);
    walk_blk = arr->tail;

    while (walk_blk != target_blk) {
        Mp4DynBlock *prev_blk = mp4DynBlockGetPrev(walk_blk);
        void *carry_ptr = NULL;

        err = mp4DynBlockGetPtr(prev_blk, (arr->block_cap - 1U) * arr->elem_size, &carry_ptr);
        if (err != MP4_OK) {
            return err;
        }
        err = mp4DynBlockInsertBytes(walk_blk, 0U, arr->elem_size, (const uint8_t *)carry_ptr);
        if (err != MP4_OK) {
            return err;
        }
        walk_blk = prev_blk;
    }

    return mp4DynBlockInsertBytes(target_blk, (index % arr->block_cap) * arr->elem_size,
                                  arr->elem_size, (const uint8_t *)elem);
}

/* 0x020db4 */
MP4_NOINLINE int32_t mp4DynArrayErase(Mp4DynArray *arr, uint32_t index)
{
    Mp4DynBlock *cur_blk;
    int32_t err;

    if (index >= arr->count) {
        return MP4_ERR_DYNARRAY_RANGE;
    }

    arr->count--;
    cur_blk = mp4DynArrayGetBlock(arr, index / arr->block_cap);
    err = mp4DynBlockEraseBytes(cur_blk, (index % arr->block_cap) * arr->elem_size, arr->elem_size);
    if (err != MP4_OK) {
        return err;
    }

    while (cur_blk != arr->tail) {
        Mp4DynBlock *next_blk = mp4DynBlockGetNext(cur_blk);
        void *first_ptr = NULL;

        err = mp4DynBlockGetPtr(next_blk, 0U, &first_ptr);
        if (err != MP4_OK) {
            return err;
        }
        err = mp4DynBlockWrite(cur_blk, (arr->block_cap - 1U) * arr->elem_size,
                               arr->elem_size, (const uint8_t *)first_ptr);
        if (err != MP4_OK) {
            return err;
        }
        err = mp4DynBlockEraseBytes(next_blk, 0U, arr->elem_size);
        if (err != MP4_OK) {
            return err;
        }
        cur_blk = next_blk;
    }

    return mp4DynArrayShrink(arr);
}

/* 0x020ee8 */
MP4_NOINLINE Mp4DynBlock *mp4DynArrayGetBlock(Mp4DynArray *arr, uint32_t block_idx)
{
    uint32_t total = arr->block_count;
    Mp4DynBlock *blk;

    if (block_idx >= (total + 1U) >> 1) {
        uint32_t steps = total - block_idx - 1U;
        blk = arr->tail;
        while (steps != 0U) {
            steps--;
            blk = mp4DynBlockGetPrev(blk);
        }
    } else {
        uint32_t steps = block_idx;
        blk = arr->head;
        while (steps != 0U) {
            steps--;
            blk = mp4DynBlockGetNext(blk);
        }
    }

    return blk;
}

/* 0x020f74 */
MP4_NOINLINE int32_t mp4DynArrayGrow(Mp4DynArray *arr)
{
    Mp4DynBlock *blk;
    int32_t err;

    err = mp4DynBlockCreate(arr->elem_size * arr->block_cap, &blk);
    if (err != MP4_OK) {
        return err;
    }

    if (arr->block_count != 0U) {
        mp4DynBlockSetNext(arr->tail, blk);
        mp4DynBlockSetPrev(blk, arr->tail);
        arr->tail = blk;
    } else {
        arr->tail = blk;
        arr->head = blk;
    }

    arr->block_count++;
    arr->capacity += arr->block_cap;
    return MP4_OK;
}

/* 0x021010 */
MP4_NOINLINE int32_t mp4DynArrayShrink(Mp4DynArray *arr)
{
    if (arr->block_count >= 2U && (arr->capacity - arr->count) >= arr->block_cap) {
        do {
            Mp4DynBlock *old_tail = arr->tail;
            Mp4DynBlock *prev_blk = mp4DynBlockGetPrev(old_tail);

            arr->tail = prev_blk;
            mp4DynBlockSetNext(prev_blk, NULL);
            mp4DynBlockFree(old_tail);
            arr->block_count--;
            arr->capacity -= arr->block_cap;
        } while ((arr->capacity - arr->count) >= arr->block_cap);
    }

    return MP4_OK;
}

/* 0x0210bc */
MP4_NOINLINE int32_t mp4DynBlockCreate(uint32_t alloc_bytes, Mp4DynBlock **out_blk)
{
    Mp4DynBlock *blk;

    if (alloc_bytes == 0U) {
        return MP4_ERR_INVALID_PARAM;
    }

    blk = (Mp4DynBlock *)mp4CbMalloc(sizeof(Mp4DynBlock));
    if (blk == NULL) {
        return MP4_ERR_NOMEM;
    }

    blk->alloc_bytes = alloc_bytes;
    blk->buf = (uint8_t *)mp4CbMalloc(alloc_bytes);
    if (blk->buf == NULL) {
        mp4CbFree(blk);
        return MP4_ERR_NOMEM;
    }

    blk->prev = NULL;
    blk->next = NULL;
    *out_blk = blk;
    return MP4_OK;
}

/* 0x021148 */
MP4_NOINLINE void mp4DynBlockFree(Mp4DynBlock *blk)
{
    if (blk->buf != NULL) {
        mp4CbFree(blk->buf);
    }
    if (blk != NULL) {
        mp4CbFree(blk);
    }
}

/* 0x021194 */
MP4_NOINLINE Mp4DynBlock *mp4DynBlockGetPrev(Mp4DynBlock *blk)
{
    return blk->prev;
}

/* 0x02119c */
MP4_NOINLINE void mp4DynBlockSetPrev(Mp4DynBlock *blk, Mp4DynBlock *prev_blk)
{
    blk->prev = prev_blk;
}

/* 0x0211a4 */
MP4_NOINLINE Mp4DynBlock *mp4DynBlockGetNext(Mp4DynBlock *blk)
{
    return blk->next;
}

/* 0x0211ac */
MP4_NOINLINE void mp4DynBlockSetNext(Mp4DynBlock *blk, Mp4DynBlock *next_blk)
{
    blk->next = next_blk;
}

/* 0x0211b4 */
MP4_NOINLINE int32_t mp4DynBlockGetPtr(Mp4DynBlock *blk, uint32_t elem_offset, void **out_ptr)
{
    if (elem_offset >= blk->alloc_bytes) {
        return MP4_ERR_DYNARRAY_RANGE;
    }
    *out_ptr = blk->buf + elem_offset;
    return MP4_OK;
}

/* 0x0211dc */
MP4_NOINLINE int32_t mp4DynBlockWrite(
    Mp4DynBlock *blk,
    uint32_t elem_offset,
    uint32_t len,
    const uint8_t *src)
{
    uint32_t idx;

    if (blk->alloc_bytes < elem_offset + len) {
        return MP4_ERR_DYNARRAY_RANGE;
    }
    for (idx = 0U; idx < len; idx++) {
        blk->buf[elem_offset + idx] = src[idx];
    }
    return MP4_OK;
}

/* 0x02122c */
MP4_NOINLINE int32_t mp4DynBlockInsertBytes(
    Mp4DynBlock *blk,
    uint32_t elem_offset,
    uint32_t len,
    const uint8_t *src)
{
    uint32_t total = blk->alloc_bytes;
    uint32_t tail_bytes;

    if (total < elem_offset + len) {
        return MP4_ERR_DYNARRAY_RANGE;
    }

    tail_bytes = total - (elem_offset + len);
    if (tail_bytes != 0U) {
        uint8_t *dst = blk->buf + total - 1U;
        const uint8_t *mv_src = dst - len;
        while (tail_bytes != 0U) {
            *dst-- = *mv_src--;
            tail_bytes--;
        }
    }

    return mp4DynBlockWrite(blk, elem_offset, len, src);
}

/* 0x0212a0 */
MP4_NOINLINE int32_t mp4DynBlockEraseBytes(Mp4DynBlock *blk, uint32_t elem_offset, uint32_t len)
{
    uint32_t tail_bytes;

    if (blk->alloc_bytes < elem_offset + len) {
        return MP4_ERR_DYNARRAY_RANGE;
    }

    tail_bytes = blk->alloc_bytes - (elem_offset + len);
    if (tail_bytes != 0U) {
        uint8_t *dst = blk->buf + elem_offset;
        const uint8_t *src = dst + len;
        while (tail_bytes != 0U) {
            *dst++ = *src++;
            tail_bytes--;
        }
    }
    return MP4_OK;
}

/* 0x0212fc */
MP4_NOINLINE int32_t mp4DynArrayCreate(uint32_t elem_size, uint32_t block_cap, Mp4DynArray **out_arr)
{
    Mp4DynArray *arr;

    if ((elem_size == 0U) | (block_cap == 0U)) {
        return MP4_ERR_INVALID_PARAM;
    }

    arr = (Mp4DynArray *)mp4CbMalloc(sizeof(Mp4DynArray));
    if (arr == NULL) {
        return MP4_ERR_NOMEM;
    }

    arr->elem_size = elem_size;
    arr->block_cap = block_cap;
    arr->count = 0U;
    arr->capacity = 0U;
    arr->block_count = 0U;
    arr->head = NULL;
    arr->tail = NULL;
    *out_arr = arr;
    return MP4_OK;
}

/* 0x021384 */
MP4_NOINLINE void mp4DynArrayDestroy(Mp4DynArray *arr)
{
    Mp4DynBlock *blk = arr->head;

    while (blk != NULL) {
        Mp4DynBlock *next = mp4DynBlockGetNext(blk);
        mp4DynBlockFree(blk);
        blk = next;
    }
    if (arr != NULL) {
        mp4CbFree(arr);
    }
}

/* 0x0213e8 */
MP4_NOINLINE int32_t mp4DynArrayCount(Mp4DynArray *arr, uint32_t *out_count)
{
    *out_count = arr->count;
    return MP4_OK;
}

/* 0x0213f8 */
MP4_NOINLINE int32_t mp4DynArrayGet(Mp4DynArray *arr, uint32_t index, void *out_elem)
{
    Mp4DynBlock *blk;
    uint32_t elem_offset;
    uint32_t len;
    uint32_t idx;
    uint8_t *dst = (uint8_t *)out_elem;

    if (index >= arr->count) {
        return MP4_ERR_DYNARRAY_RANGE;
    }

    blk = mp4DynArrayGetBlock(arr, index / arr->block_cap);
    len = arr->elem_size;
    elem_offset = (index % arr->block_cap) * len;
    if (blk->alloc_bytes < elem_offset + len) {
        return MP4_ERR_DYNARRAY_RANGE;
    }

    for (idx = 0U; idx < len; idx++) {
        dst[idx] = blk->buf[elem_offset + idx];
    }
    return MP4_OK;
}

/* 0x0214d4 */
MP4_NOINLINE int32_t mp4OdsmGetObjectAt(void *odsm_ctx, uint16_t index, Mp4OdsmObject **out_obj)
{
    Mp4OdsmCtx *ctx = (Mp4OdsmCtx *)odsm_ctx;
    uint32_t idx;
    uint16_t rem = index;

    if (rem == 0U) {
        return MP4_ERR_INVALID_PARAM;
    }

    for (idx = 0U; idx < ctx->count; idx++) {
        uint8_t tag = ctx->entries[idx].tag;
        if (tag == 1U || tag == 17U) {
            rem--;
        }
        if (rem == 0U) {
            *out_obj = (Mp4OdsmObject *)ctx->entries[idx].body;
            return MP4_OK;
        }
    }

    return MP4_ERR_NO_ENTRY;
}

/* 0x02154c */
MP4_NOINLINE int32_t mp4OdsmFindEsItem(void *odsm_ctx, uint8_t es_index, Mp4OdsmEsItem **out_item)
{
    Mp4OdsmCtx *ctx = (Mp4OdsmCtx *)odsm_ctx;
    uint32_t idx;

    for (idx = 0U; idx < ctx->count; idx++) {
        if (ctx->entries[idx].tag == 11U) {
            Mp4OdsmEsItem *item = (Mp4OdsmEsItem *)ctx->entries[idx].body;
            if (item->es_index == es_index) {
                *out_item = item;
                return MP4_OK;
            }
        }
    }

    return MP4_ERR_NO_ENTRY;
}

/* 0x02159c */
MP4_NOINLINE int32_t mp4OdsmFreeCtx(void *odsm_ctx)
{
    Mp4OdsmCtx *ctx = (Mp4OdsmCtx *)odsm_ctx;
    uint32_t idx;

    for (idx = 0U; idx < ctx->count; idx++) {
        mp4DescBodyFree(ctx->entries[idx].body, ctx->entries[idx].tag);
    }
    mp4CbFree(ctx->entries);
    mp4CbFree(ctx);
    return MP4_OK;
}

/* 0x021618 */
MP4_NOINLINE int32_t mp4DescFree(Mp4DescNode *node)
{
    mp4DescBodyFree(node->body, node->tag);
    mp4MemFree(node);
    return MP4_OK;
}

/* 0x021650 */
MP4_NOINLINE int32_t mp4OdsmGetObjectCount(void *odsm_ctx, uint16_t *out_count)
{
    Mp4OdsmCtx *ctx = (Mp4OdsmCtx *)odsm_ctx;
    uint32_t idx;

    *out_count = 0U;
    for (idx = 0U; idx < ctx->count; idx++) {
        uint8_t tag = ctx->entries[idx].tag;
        if (tag == 1U || tag == 17U) {
            (*out_count)++;
        }
    }
    return MP4_OK;
}

/* 0x0216a8 */
MP4_NOINLINE int32_t mp4OdsmAppendEntry(Mp4OdsmCtx *ctx, uint8_t tag, void *body)
{
    uint32_t cur_cnt = ctx->count;

    if (mp4MemRealloc((void **)&ctx->entries,
                      cur_cnt * sizeof(Mp4OdsmDescEntry),
                      (cur_cnt + 1U) * sizeof(Mp4OdsmDescEntry)) != MP4_OK) {
        return MP4_ERR_NOMEM;
    }

    ctx->entries[ctx->count].tag = tag;
    ctx->entries[ctx->count].body = body;
    ctx->count++;
    return MP4_OK;
}

/* 0x021740 */
MP4_NOINLINE int32_t mp4DescBodyFree(void *body, uint8_t tag)
{
    switch (tag) {
        case 2U:
        case 16U: {
            Mp4IodsBody *iods = (Mp4IodsBody *)body;
            if (iods->es_track_ids != NULL) {
                mp4MemFree(iods->es_track_ids);
            }
            if (iods->url_str != NULL) {
                mp4MemFree(iods->url_str);
            }
            mp4MemFree(iods);
            break;
        }
        case 3U: {
            Mp4EsDescBody *es = (Mp4EsDescBody *)body;
            if (es->url_str != NULL) {
                mp4MemFree(es->url_str);
            }
            if (es->dec_specific_info != NULL) {
                mp4MemFree(es->dec_specific_info);
            }
            mp4MemFree(es);
            break;
        }
        case 11U: {
            Mp4OdsmEsItem *item = (Mp4OdsmEsItem *)body;
            if (item->payload != NULL) {
                mp4MemFree(item->payload);
            }
            mp4MemFree(item);
            break;
        }
        case 17U: {
            Mp4OdsmObject *obj = (Mp4OdsmObject *)body;
            if (obj->es_ids != NULL) {
                mp4MemFree(obj->es_ids);
            }
            if (obj->es_indices != NULL) {
                mp4MemFree(obj->es_indices);
            }
            if (obj->url_str != NULL) {
                mp4MemFree(obj->url_str);
            }
            mp4MemFree(obj);
            break;
        }
        default:
            break;
    }

    return MP4_OK;
}

/* 0x02181c */
MP4_NOINLINE int32_t mp4DescParseIods(uint32_t size, const uint8_t *buf, Mp4DescNode **out_node)
{
    Mp4BitReader br;
    uint8_t tag = 0U;
    uint8_t hdr_bytes = 0U;
    uint32_t desc_len = 0U;
    uint32_t rem;
    const uint8_t *cursor;
    Mp4DescNode *node;
    Mp4IodsBody *iods;
    int32_t err;

    for (;;) {
        err = mp4DescReadTagAndLen(buf, size, &tag, &desc_len, &hdr_bytes);
        if (err != MP4_OK) {
            return err;
        }
        rem = size - hdr_bytes;
        cursor = buf + hdr_bytes;
        if (rem < desc_len) {
            return MP4_ERR_CORRUPT;
        }
        if (tag == 16U) {
            break;
        }
        size = rem - desc_len;
        buf = cursor + desc_len;
    }

    mp4BitReaderInit(&br, cursor);
    node = (Mp4DescNode *)mp4MemCalloc(sizeof(Mp4DescNode));
    if (node == NULL) {
        return MP4_ERR_NOMEM;
    }
    node->tag = 16U;

    iods = (Mp4IodsBody *)mp4MemCalloc(sizeof(Mp4IodsBody));
    node->body = iods;
    if (iods == NULL) {
        mp4MemFree(node);
        return MP4_ERR_NOMEM;
    }

    iods->tag = 16U;
    if (rem < 2U) {
        return MP4_ERR_CORRUPT;
    }

    iods->object_id = (uint16_t)mp4BitReaderRead(&br, 10U);
    iods->url_flag = mp4BitReaderRead(&br, 1U);
    iods->include_inline = mp4BitReaderRead(&br, 1U);
    mp4BitReaderRead(&br, 4U);
    cursor += 2;
    rem -= 2U;

    if (iods->url_flag == 1U) {
        uint8_t url_len = cursor[0];
        iods->url_len = url_len;
        iods->url_str = (uint8_t *)mp4MemCalloc(url_len);
        if (iods->url_str == NULL) {
            mp4DescFree(node);
            return MP4_ERR_NOMEM;
        }
        mp4SafeMemcpy(iods->url_str, iods->url_len, cursor + 1, iods->url_len);
        return MP4_OK;
    }

    if (rem < 5U) {
        return MP4_ERR_CORRUPT;
    }

    iods->od_profile = cursor[0];
    iods->scene_profile = cursor[1];
    iods->audio_profile = cursor[2];
    iods->visual_profile = cursor[3];
    iods->graphics_profile = cursor[4];
    cursor += 5;
    rem -= 5U;

    while (rem >= 2U) {
        err = mp4DescReadTagAndLen(cursor, rem, &tag, &desc_len, &hdr_bytes);
        if (err != MP4_OK) {
            return err;
        }
        rem -= hdr_bytes;
        cursor += hdr_bytes;
        if (rem < desc_len) {
            return MP4_ERR_CORRUPT;
        }
        if (tag == 14U) {
            err = mp4DescParseIodsEsTrack(iods, desc_len, cursor);
            if (err != MP4_OK) {
                return err;
            }
        }
        rem -= desc_len;
        cursor += desc_len;
    }

    if (rem != 0U) {
        return MP4_ERR_CORRUPT;
    }

    *out_node = node;
    return MP4_OK;
}

/* 0x021ab4 */
MP4_NOINLINE int32_t mp4DescParseEsds(uint32_t size, const uint8_t *buf, Mp4DescNode **out_node)
{
    Mp4BitReader br;
    uint8_t tag = 0U;
    uint8_t hdr_bytes = 0U;
    uint32_t desc_len = 0U;
    uint32_t rem;
    const uint8_t *cursor;
    Mp4DescNode *node;
    Mp4EsDescBody *es;
    int32_t err;

    for (;;) {
        err = mp4DescReadTagAndLen(buf, size, &tag, &desc_len, &hdr_bytes);
        if (err != MP4_OK) {
            return err;
        }
        rem = size - hdr_bytes;
        cursor = buf + hdr_bytes;
        if (rem < desc_len) {
            return MP4_ERR_CORRUPT;
        }
        if (tag == 3U) {
            break;
        }
        size = rem - desc_len;
        buf = cursor + desc_len;
    }

    mp4BitReaderInit(&br, cursor);
    node = (Mp4DescNode *)mp4MemCalloc(sizeof(Mp4DescNode));
    if (node == NULL) {
        return MP4_ERR_NOMEM;
    }
    node->tag = 3U;

    es = (Mp4EsDescBody *)mp4MemCalloc(sizeof(Mp4EsDescBody));
    node->body = es;
    if (es == NULL) {
        mp4MemFree(node);
        return MP4_ERR_NOMEM;
    }

    es->tag = 3U;
    if (rem < 3U) {
        return MP4_ERR_CORRUPT;
    }

    es->es_id = (uint16_t)mp4BitReaderRead(&br, 16U);
    es->stream_dep_flag = mp4BitReaderRead(&br, 1U);
    es->url_flag = mp4BitReaderRead(&br, 1U);
    es->ocr_stream_flag = mp4BitReaderRead(&br, 1U);
    mp4BitReaderRead(&br, 5U);
    cursor += 3;
    rem -= 3U;

    if (es->stream_dep_flag == 1U) {
        es->depends_on_es_id = (uint16_t)(((uint32_t)cursor[0] << 8) + cursor[1]);
        cursor += 2;
        rem -= 2U;
    }

    if (es->url_flag == 1U) {
        uint8_t url_len = cursor[0];
        cursor++;
        rem--;
        es->url_len = url_len;
        es->url_str = (uint8_t *)mp4MemCalloc(url_len);
        if (es->url_str == NULL) {
            mp4DescFree(node);
            return MP4_ERR_NOMEM;
        }
        mp4SafeMemcpy(es->url_str, es->url_len, cursor, es->url_len);
        rem -= es->url_len;
    }

    if (es->ocr_stream_flag == 1U) {
        es->ocr_es_id = (uint16_t)(((uint32_t)cursor[0] << 8) + cursor[1]);
        cursor += 2;
        rem -= 2U;
    }

    while (rem >= 2U) {
        err = mp4DescReadTagAndLen(cursor, rem, &tag, &desc_len, &hdr_bytes);
        if (err != MP4_OK) {
            return err;
        }
        rem -= hdr_bytes;
        cursor += hdr_bytes;
        if (rem < desc_len) {
            return MP4_ERR_CORRUPT;
        }
        if (tag == 4U) {
            err = mp4DescParseDecConfig(es, desc_len, cursor);
            if (err != MP4_OK) {
                return err;
            }
        } else if (tag == 6U) {
            err = mp4DescParseSlConfig(es, desc_len, cursor);
            if (err != MP4_OK) {
                return err;
            }
        }
        rem -= desc_len;
        cursor += desc_len;
    }

    if (rem != 0U) {
        return MP4_ERR_CORRUPT;
    }

    *out_node = node;
    return MP4_OK;
}

/* 0x021d8c */
MP4_NOINLINE int32_t mp4DescParseOdUpdate(Mp4OdsmCtx *ctx, uint32_t size, const uint8_t *buf)
{
    uint32_t rem = size;
    const uint8_t *cursor = buf;
    uint8_t tag = 0U;
    uint8_t hdr_bytes = 0U;
    uint32_t desc_len = 0U;

    while (rem >= 2U) {
        int32_t err = mp4DescReadTagAndLen(cursor, rem, &tag, &desc_len, &hdr_bytes);
        if (err != MP4_OK) {
            return err;
        }
        rem -= hdr_bytes;
        cursor += hdr_bytes;
        if (rem < desc_len) {
            return MP4_ERR_CORRUPT;
        }
        if (tag == 17U) {
            err = mp4DescParseOdEntry(ctx, desc_len, cursor);
            if (err != MP4_OK) {
                return err;
            }
        }
        rem -= desc_len;
        cursor += desc_len;
    }

    return (rem == 0U) ? MP4_OK : MP4_ERR_CORRUPT;
}

/* 0x021e70 */
MP4_NOINLINE int32_t mp4DescParseEsUpdate(Mp4OdsmCtx *ctx, uint32_t size, const uint8_t *buf)
{
    uint32_t rem = size;
    const uint8_t *cursor = buf;
    uint8_t tag = 0U;
    uint8_t hdr_bytes = 0U;
    uint32_t desc_len = 0U;

    while (rem >= 2U) {
        int32_t err = mp4DescReadTagAndLen(cursor, rem, &tag, &desc_len, &hdr_bytes);
        if (err != MP4_OK) {
            return err;
        }
        rem -= hdr_bytes;
        cursor += hdr_bytes;
        if (rem < desc_len) {
            return MP4_ERR_CORRUPT;
        }
        if (tag == 11U) {
            err = mp4DescParseEsItem(ctx, desc_len, cursor);
            if (err != MP4_OK) {
                return err;
            }
        }
        rem -= desc_len;
        cursor += desc_len;
    }

    return (rem == 0U) ? MP4_OK : MP4_ERR_CORRUPT;
}

/* 0x021f54 */
MP4_NOINLINE int32_t mp4DescParseOdEntry(Mp4OdsmCtx *ctx, uint32_t size, const uint8_t *buf)
{
    Mp4BitReader br;
    Mp4OdsmObject *obj;
    const uint8_t *cursor;
    uint32_t rem;
    uint8_t tag = 0U;
    uint8_t hdr_bytes = 0U;
    uint32_t desc_len = 0U;
    int32_t err;

    mp4BitReaderInit(&br, buf);
    obj = (Mp4OdsmObject *)mp4MemCalloc(sizeof(Mp4OdsmObject));
    if (obj == NULL) {
        return MP4_ERR_NOMEM;
    }

    err = mp4OdsmAppendEntry(ctx, 17U, obj);
    if (err != MP4_OK) {
        return err;
    }

    obj->tag = 17U;
    if (size < 2U) {
        return MP4_ERR_CORRUPT;
    }

    obj->object_id = (uint16_t)mp4BitReaderRead(&br, 10U);
    obj->url_flag = mp4BitReaderRead(&br, 1U);
    mp4BitReaderRead(&br, 5U);
    cursor = buf + 2;
    rem = size - 2U;

    if (obj->url_flag == 1U) {
        uint8_t url_len = cursor[0];
        obj->url_len = url_len;
        obj->url_str = (uint8_t *)mp4MemCalloc(url_len);
        if (obj->url_str == NULL) {
            return MP4_ERR_NOMEM;
        }
        mp4SafeMemcpy(obj->url_str, obj->url_len, cursor + 1, obj->url_len);
        return MP4_OK;
    }

    while (rem >= 2U) {
        err = mp4DescReadTagAndLen(cursor, rem, &tag, &desc_len, &hdr_bytes);
        if (err != MP4_OK) {
            return err;
        }
        rem -= hdr_bytes;
        cursor += hdr_bytes;
        if (rem < desc_len) {
            return MP4_ERR_CORRUPT;
        }
        if (tag == 10U) {
            err = mp4DescParseEsIdInc(obj, desc_len, cursor);
            if (err != MP4_OK) {
                return err;
            }
        } else if (tag == 15U) {
            err = mp4DescParseEsIdRef(obj, desc_len, cursor);
            if (err != MP4_OK) {
                return err;
            }
        }
        rem -= desc_len;
        cursor += desc_len;
    }

    return (rem == 0U) ? MP4_OK : MP4_ERR_CORRUPT;
}

/* 0x022120 */
MP4_NOINLINE int32_t mp4DescParseEsIdRef(Mp4OdsmObject *obj, uint32_t size, const uint8_t *buf)
{
    if (size != 2U) {
        return MP4_ERR_CORRUPT;
    }

    if (obj->es_ids != NULL) {
        uint16_t *new_arr = (uint16_t *)mp4MemCalloc(2U * obj->es_id_count + 2U);
        if (new_arr == NULL) {
            return MP4_ERR_NOMEM;
        }
        mp4SafeMemcpy(new_arr, 2U * obj->es_id_count + 2U, obj->es_ids, 2U * obj->es_id_count);
        mp4CbFree(obj->es_ids);
        obj->es_ids = new_arr;
    } else {
        obj->es_ids = (uint16_t *)mp4MemCalloc(2U);
        if (obj->es_ids == NULL) {
            return MP4_ERR_NOMEM;
        }
    }

    obj->es_ids[obj->es_id_count] = (uint16_t)(((uint32_t)buf[0] << 8) + buf[1]);
    obj->es_id_count++;
    return MP4_OK;
}

/* 0x0221fc */
MP4_NOINLINE int32_t mp4DescParseEsIdInc(Mp4OdsmObject *obj, uint32_t size, const uint8_t *buf)
{
    if (size != 1U) {
        return MP4_ERR_CORRUPT;
    }

    if (obj->es_indices != NULL) {
        uint8_t *new_arr = (uint8_t *)mp4MemCalloc((uint32_t)obj->es_index_count + 1U);
        if (new_arr == NULL) {
            return MP4_ERR_NOMEM;
        }
        mp4SafeMemcpy(new_arr, (uint32_t)obj->es_index_count + 1U, obj->es_indices, obj->es_index_count);
        mp4CbFree(obj->es_indices);
        obj->es_indices = new_arr;
    } else {
        obj->es_indices = (uint8_t *)mp4MemCalloc(1U);
        if (obj->es_indices == NULL) {
            return MP4_ERR_NOMEM;
        }
    }

    obj->es_indices[obj->es_index_count] = buf[0];
    obj->es_index_count++;
    return MP4_OK;
}

/* 0x0222c0 */
MP4_NOINLINE int32_t mp4DescParseEsItem(Mp4OdsmCtx *ctx, uint32_t size, const uint8_t *buf)
{
    Mp4OdsmEsItem *item;
    int32_t err;

    item = (Mp4OdsmEsItem *)mp4MemCalloc(sizeof(Mp4OdsmEsItem));
    if (item == NULL) {
        return MP4_ERR_NOMEM;
    }

    err = mp4OdsmAppendEntry(ctx, 11U, item);
    if (err != MP4_OK) {
        return err;
    }

    item->tag = 11U;
    if (size < 3U) {
        return MP4_ERR_CORRUPT;
    }

    item->es_index = buf[0];
    item->stream_type = (uint16_t)(((uint32_t)buf[1] << 8) + buf[2]);
    {
        uint32_t payload_len = size - 3U;
        if (payload_len != 0U) {
            item->payload = (uint8_t *)mp4MemCalloc(payload_len);
            if (item->payload == NULL) {
                return MP4_ERR_NOMEM;
            }
            mp4SafeMemcpy(item->payload, payload_len, buf + 3, payload_len);
            item->payload_size = payload_len;
        }
    }

    return MP4_OK;
}

/* 0x0223a8 */
MP4_NOINLINE int32_t mp4DescParseDecConfig(Mp4EsDescBody *body, uint32_t size, const uint8_t *buf)
{
    Mp4BitReader br;
    const uint8_t *cursor;
    uint32_t rem;
    uint8_t tag = 0U;
    uint8_t hdr_bytes = 0U;
    uint32_t desc_len = 0U;

    mp4BitReaderInit(&br, buf);
    body->has_dec_cfg = 4U;
    if (size < 13U) {
        return MP4_ERR_CORRUPT;
    }

    body->object_type_ind = (uint8_t)mp4BitReaderRead(&br, 8U);
    body->stream_type_byte = (uint8_t)mp4BitReaderRead(&br, 6U);
    body->up_stream = mp4BitReaderRead(&br, 1U);
    mp4BitReaderRead(&br, 1U);
    body->buffer_size_db = mp4BitReaderRead(&br, 24U);
    body->max_bitrate = mp4BitReaderRead(&br, 32U);
    body->avg_bitrate = mp4BitReaderRead(&br, 32U);

    rem = size - 13U;
    cursor = buf + 13;

    while (rem >= 2U) {
        int32_t err = mp4DescReadTagAndLen(cursor, rem, &tag, &desc_len, &hdr_bytes);
        if (err != MP4_OK) {
            return err;
        }
        rem -= hdr_bytes;
        cursor += hdr_bytes;
        if (rem < desc_len) {
            return MP4_ERR_CORRUPT;
        }
        if (tag == 5U) {
            err = mp4DescParseDecSpecific(body, desc_len, cursor);
            if (err != MP4_OK) {
                return err;
            }
        }
        rem -= desc_len;
        cursor += desc_len;
    }

    return (rem == 0U) ? MP4_OK : MP4_ERR_CORRUPT;
}

/* 0x022528 */
MP4_NOINLINE int32_t mp4DescParseDecSpecific(Mp4EsDescBody *body, uint32_t size, const uint8_t *buf)
{
    if (body->dec_specific_info != NULL) {
        return MP4_OK;
    }

    body->dec_specific_info = (uint8_t *)mp4MemCalloc(size);
    if (body->dec_specific_info == NULL) {
        return MP4_ERR_NOMEM;
    }

    mp4SafeMemcpy(body->dec_specific_info, size, buf, size);
    body->dec_specific_size = size;
    return MP4_OK;
}

/* 0x0225ac */
MP4_NOINLINE int32_t mp4DescParseSlConfig(Mp4EsDescBody *body, uint32_t size, const uint8_t *buf)
{
    Mp4BitReader br;
    uint32_t rem;
    uint8_t predefined;

    mp4BitReaderInit(&br, buf);
    body->sl_cfg_tag = 6U;
    if (size == 0U) {
        return MP4_ERR_CORRUPT;
    }

    predefined = (uint8_t)mp4BitReaderRead(&br, 8U);
    body->sl_predefined = predefined;
    rem = size - 1U;

    if (predefined == 0U) {
        if (rem < 15U) {
            return MP4_ERR_CORRUPT;
        }
        body->sl_use_au_start = mp4BitReaderRead(&br, 1U);
        body->sl_use_au_end = mp4BitReaderRead(&br, 1U);
        body->sl_use_rap = mp4BitReaderRead(&br, 1U);
        body->sl_has_random_pad = mp4BitReaderRead(&br, 1U);
        body->sl_use_padding = mp4BitReaderRead(&br, 1U);
        body->sl_use_timestamps = mp4BitReaderRead(&br, 1U);
        body->sl_use_idle = mp4BitReaderRead(&br, 1U);
        body->sl_duration_flag = mp4BitReaderRead(&br, 1U);
        body->sl_ts_resolution = mp4BitReaderRead(&br, 32U);
        body->sl_ocr_resolution = mp4BitReaderRead(&br, 32U);
        body->sl_ts_len = (uint8_t)mp4BitReaderRead(&br, 8U);
        body->sl_ocr_len = (uint8_t)mp4BitReaderRead(&br, 8U);
        body->sl_au_len = (uint8_t)mp4BitReaderRead(&br, 8U);
        body->sl_inst_bitrate_len = (uint8_t)mp4BitReaderRead(&br, 8U);
        body->sl_degrad_prio_len = (uint8_t)mp4BitReaderRead(&br, 4U);
        body->sl_au_seq_len = (uint8_t)mp4BitReaderRead(&br, 5U);
        body->sl_pkt_seq_len = (uint8_t)mp4BitReaderRead(&br, 5U);
        mp4BitReaderRead(&br, 2U);
        rem -= 15U;
    } else if (predefined == 1U) {
        body->sl_ts_resolution = 1000U;
        body->sl_ts_len = 32U;
        body->sl_use_timestamps = 0U;
    } else if (predefined == 2U) {
        body->sl_use_timestamps = 1U;
    }

    if (body->sl_duration_flag == 1U) {
        if (rem < 8U) {
            return MP4_ERR_CORRUPT;
        }
        body->sl_time_scale = mp4BitReaderRead(&br, 32U);
        body->sl_au_duration = (uint16_t)mp4BitReaderRead(&br, 16U);
        body->sl_cu_duration = (uint16_t)mp4BitReaderRead(&br, 16U);
        rem -= 8U;
    }

    if (body->sl_use_timestamps == 0U) {
        uint8_t ts_len = body->sl_ts_len;
        uint32_t need_bytes = (uint32_t)((int32_t)(2U * ts_len - 1U) / 8 + 1);
        uint8_t lo_bits = ts_len;

        if (rem < need_bytes) {
            return MP4_ERR_CORRUPT;
        }
        if (ts_len >= 33U) {
            if (ts_len >= 65U) {
                return MP4_ERR_CORRUPT;
            }
            body->sl_start_dts_hi = mp4BitReaderRead(&br, (uint8_t)(ts_len - 32U));
            lo_bits = 32U;
        }
        body->sl_start_dts_lo = mp4BitReaderRead(&br, lo_bits);

        ts_len = body->sl_ts_len;
        lo_bits = ts_len;
        if (ts_len >= 33U) {
            if (ts_len >= 65U) {
                return MP4_ERR_CORRUPT;
            }
            body->sl_start_cts_hi = mp4BitReaderRead(&br, (uint8_t)(ts_len - 32U));
            lo_bits = 32U;
        }
        body->sl_start_cts_lo = mp4BitReaderRead(&br, lo_bits);

        rem -= (uint32_t)((int32_t)(2U * body->sl_ts_len - 1U) / 8 + 1);
    }

    return (rem == 0U) ? MP4_OK : MP4_ERR_CORRUPT;
}

/* 0x0228cc */
MP4_NOINLINE int32_t mp4DescParseIodsEsTrack(Mp4IodsBody *iods, uint32_t size, const uint8_t *buf)
{
    if (size != 4U) {
        return MP4_ERR_CORRUPT;
    }

    if (iods->es_track_ids != NULL) {
        uint32_t *new_arr = (uint32_t *)mp4MemCalloc(4U * iods->es_track_count + 4U);
        if (new_arr == NULL) {
            return MP4_ERR_NOMEM;
        }
        mp4SafeMemcpy(new_arr, 4U * iods->es_track_count + 4U, iods->es_track_ids, 4U * iods->es_track_count);
        mp4CbFree(iods->es_track_ids);
        iods->es_track_ids = new_arr;
    } else {
        iods->es_track_ids = (uint32_t *)mp4MemCalloc(4U);
        if (iods->es_track_ids == NULL) {
            return MP4_ERR_NOMEM;
        }
    }

    iods->es_track_ids[iods->es_track_count] =
        ((uint32_t)buf[0] << 24) + ((uint32_t)buf[1] << 16) + ((uint32_t)buf[2] << 8) + buf[3];
    iods->es_track_count++;
    return MP4_OK;
}

/* 0x0229c0 */
MP4_NOINLINE int32_t mp4DescReadTagAndLen(
    const uint8_t *buf,
    uint32_t rem,
    uint8_t *out_tag,
    uint32_t *out_len,
    uint8_t *out_hdr_bytes)
{
    const uint8_t *cursor;
    uint32_t left;

    if (rem < 2U) {
        return MP4_ERR_CORRUPT;
    }

    *out_len = 0U;
    *out_hdr_bytes = 0U;
    *out_tag = buf[0];
    (*out_hdr_bytes)++;
    cursor = buf + 1;
    left = rem - 1U;

    while (left != 0U) {
        uint8_t cur_byte = *cursor++;
        left--;
        *out_len = (*out_len << 7) + (cur_byte & 0x7FU);
        (*out_hdr_bytes)++;
        if ((cur_byte >> 7) == 0U) {
            return MP4_OK;
        }
        if (*out_hdr_bytes == 5U) {
            return MP4_ERR_CORRUPT;
        }
    }

    return MP4_ERR_CORRUPT;
}

/* 0x022a50 */
MP4_NOINLINE int32_t mp4BitReaderInit(Mp4BitReader *br, const uint8_t *buf)
{
    br->ptr = buf;
    br->bit_pos = 0U;
    return MP4_OK;
}

/* 0x022a60 */
MP4_NOINLINE uint32_t mp4BitReaderRead(Mp4BitReader *br, uint8_t nbits)
{
    uint32_t rem = nbits;
    uint32_t acc = 0U;

    while (rem != 0U) {
        uint8_t bit_pos = br->bit_pos;
        uint8_t take = (uint8_t)(8U - bit_pos);
        uint8_t shifted;

        if (rem < take) {
            take = (uint8_t)rem;
        }
        shifted = (uint8_t)(*br->ptr << bit_pos);
        acc = (acc << take) + (uint32_t)(shifted >> (8U - take));
        br->bit_pos = (uint8_t)(bit_pos + take);
        rem -= take;
        if (br->bit_pos == 8U) {
            br->ptr++;
            br->bit_pos = 0U;
        }
    }

    return acc;
}

/* 0x022ae4 */
MP4_NOINLINE int32_t mp4OdsmParseCommand(uint32_t size, const uint8_t *buf, void **out_ctx)
{
    Mp4OdsmCtx *ctx;
    int32_t err;

    ctx = (Mp4OdsmCtx *)mp4MemCalloc(sizeof(Mp4OdsmCtx));
    if (ctx == NULL) {
        return MP4_ERR_NOMEM;
    }

    err = mp4OdsmParseUpdateList(ctx, size, buf);
    if (err != MP4_OK) {
        mp4OdsmFreeCtx(ctx);
        return err;
    }

    *out_ctx = ctx;
    return MP4_OK;
}

/* 0x022b70 */
MP4_NOINLINE int32_t mp4OdsmParseUpdateList(Mp4OdsmCtx *ctx, uint32_t size, const uint8_t *buf)
{
    uint32_t rem = size;
    const uint8_t *cursor = buf;
    uint8_t tag = 0U;
    uint8_t hdr_bytes = 0U;
    uint32_t desc_len = 0U;

    while (rem >= 2U) {
        int32_t err = mp4DescReadTagAndLen(cursor, rem, &tag, &desc_len, &hdr_bytes);
        if (err != MP4_OK) {
            return err;
        }
        rem -= hdr_bytes;
        cursor += hdr_bytes;
        if (rem < desc_len) {
            return MP4_ERR_CORRUPT;
        }
        if (tag == 1U) {
            if (ctx->od_update_seen == 0U) {
                err = mp4DescParseOdUpdate(ctx, desc_len, cursor);
                if (err != MP4_OK) {
                    return err;
                }
                ctx->od_update_seen++;
            }
        } else if (tag == 5U) {
            if (ctx->es_update_seen == 0U) {
                err = mp4DescParseEsUpdate(ctx, desc_len, cursor);
                if (err != MP4_OK) {
                    return err;
                }
                ctx->es_update_seen++;
            }
        }
        rem -= desc_len;
        cursor += desc_len;
    }

    return (rem == 0U) ? MP4_OK : MP4_ERR_CORRUPT;
}

/* 0x022ca0 */
MP4_NOINLINE int32_t mp4ChunkArraySumU32(
    Mp4ChunkArray *head,
    uint32_t start_idx,
    uint32_t end_idx,
    uint32_t col,
    uint32_t *out_sum)
{
    uint32_t rel = start_idx;
    Mp4ChunkArray *chunk;
    uint32_t cur_idx;

    *out_sum = 0U;
    chunk = mp4ChunkArrayLocate(head, &rel);
    if (chunk == NULL) {
        return MP4_ERR_PARSE_FATAL;
    }
    if (end_idx < start_idx) {
        return MP4_OK;
    }

    for (cur_idx = start_idx; cur_idx <= end_idx; cur_idx++) {
        if (chunk->count == rel) {
            chunk = chunk->next;
            rel = 0U;
            if (chunk == NULL) {
                return MP4_ERR_PARSE_FATAL;
            }
        }
        *out_sum += chunk->words[chunk->cols * rel + col];
        rel++;
    }

    return MP4_OK;
}

/* 0x022d78 */
MP4_NOINLINE int32_t mp4ChunkArrayFindGe(
    Mp4ChunkArray *head,
    uint32_t target,
    uint32_t col,
    uint32_t *out_idx)
{
    uint32_t base_row = 0U;
    Mp4ChunkArray *chunk = head;

    if (chunk == NULL || chunk->count == 0U) {
        return MP4_ERR_PARSE_FATAL;
    }

    while (chunk != NULL) {
        uint32_t cnt = chunk->count;
        uint32_t row;

        for (row = 0U; row < cnt; row++) {
            if (target < chunk->words[row * chunk->cols + col]) {
                if ((base_row | row) == 0U) {
                    return MP4_ERR_PARSE_FATAL;
                }
                *out_idx = base_row + row - 1U;
                return MP4_OK;
            }
        }
        base_row += cnt;
        chunk = chunk->next;
    }

    *out_idx = base_row - 1U;
    return MP4_OK;
}

/* 0x022e10 */
MP4_NOINLINE int32_t mp4ChunkArrayAppendU32(
    Mp4ChunkArray **head_ptr,
    uint32_t cols,
    const uint32_t *vals,
    uint32_t chunk_cap)
{
    Mp4ChunkArray *orig_head = *head_ptr;
    Mp4ChunkArray *chunk = orig_head;
    uint32_t col;

    if (chunk != NULL) {
        if (chunk->cols < cols) {
            return MP4_ERR_PARSE_FATAL;
        }
    } else {
        chunk = (Mp4ChunkArray *)mp4MemCalloc(sizeof(Mp4ChunkArray));
        *head_ptr = chunk;
        if (chunk == NULL) {
            return MP4_ERR_NOMEM;
        }
    }

    if (chunk->words == NULL) {
        if (mp4ChunkArrayInitU32(chunk, chunk_cap, cols) != MP4_OK) {
            if (orig_head == NULL) {
                mp4MemFree(chunk);
            }
            *head_ptr = NULL;
            return MP4_ERR_NOMEM;
        }
    }

    while (chunk->next != NULL) {
        chunk = chunk->next;
    }

    if (chunk->capacity == chunk->count) {
        Mp4ChunkArray *next_chunk = (Mp4ChunkArray *)mp4MemCalloc(sizeof(Mp4ChunkArray));
        chunk->next = next_chunk;
        if (next_chunk == NULL) {
            return MP4_ERR_NOMEM;
        }
        if (mp4ChunkArrayInitU32(next_chunk, chunk_cap, chunk->cols) != MP4_OK) {
            mp4MemFree(next_chunk);
            chunk->next = NULL;
            return MP4_ERR_NOMEM;
        }
        chunk = next_chunk;
    }

    for (col = 0U; col < cols; col++) {
        chunk->words[chunk->count * chunk->cols + col] = vals[col];
    }
    for (col = cols; col < chunk->cols; col++) {
        chunk->words[chunk->count * chunk->cols + col] = 0U;
    }

    chunk->count++;
    return MP4_OK;
}

/* 0x023000 */
MP4_NOINLINE int32_t mp4ChunkArraySetU32(
    Mp4ChunkArray **head_ptr,
    uint32_t row,
    uint32_t cols,
    uint32_t col,
    uint32_t val)
{
    Mp4ChunkArray *head = *head_ptr;
    Mp4ChunkArray *chunk;
    uint32_t rel = row;

    if (head == NULL) {
        return MP4_ERR_PARSE_FATAL;
    }
    if ((head->cols < cols) | (col >= cols)) {
        return MP4_ERR_PARSE_FATAL;
    }

    chunk = mp4ChunkArrayLocate(head, &rel);
    if (chunk == NULL) {
        return MP4_ERR_PARSE_FATAL;
    }

    chunk->words[rel * cols + col] = val;
    return MP4_OK;
}

/* 0x0230a0 */
MP4_NOINLINE int32_t mp4ChunkArrayGetU32(
    Mp4ChunkArray *head,
    uint32_t row,
    uint32_t col,
    uint32_t *out_val)
{
    Mp4ChunkArray *chunk;
    uint32_t rel = row;

    if (head != NULL && col >= head->cols) {
        return MP4_ERR_PARSE_FATAL;
    }

    chunk = mp4ChunkArrayLocate(head, &rel);
    if (chunk == NULL) {
        return MP4_ERR_PARSE_FATAL;
    }

    *out_val = chunk->words[rel * chunk->cols + col];
    return MP4_OK;
}

/* 0x023128 */
MP4_NOINLINE Mp4ChunkArray *mp4ChunkArrayLocate(Mp4ChunkArray *head, uint32_t *row_io)
{
    Mp4ChunkArray *chunk = head;

    if (chunk == NULL) {
        return NULL;
    }

    while (chunk->count < *row_io + 1U) {
        Mp4ChunkArray *next = chunk->next;
        if (next == NULL) {
            return NULL;
        }
        *row_io -= chunk->count;
        chunk = next;
    }

    return chunk;
}

/* 0x023184 */
MP4_NOINLINE Mp4ChunkArray *mp4ChunkArrayPruneEmpty(Mp4ChunkArray *head)
{
    Mp4ChunkArray *cur = head;
    Mp4ChunkArray *prev;

    while (cur != NULL && cur->count == 0U) {
        Mp4ChunkArray *next = cur->next;
        if (cur->words != NULL) {
            mp4MemFree(cur->words);
        }
        if (cur->extra_a != NULL) {
            mp4MemFree(cur->extra_a);
        }
        if (cur->wide_words != NULL) {
            mp4MemFree(cur->wide_words);
        }
        mp4MemFree(cur);
        cur = next;
    }

    if (cur == NULL) {
        return NULL;
    }

    prev = cur;
    while (prev->next != NULL) {
        Mp4ChunkArray *node = prev->next;
        if (node->count != 0U) {
            prev = node;
        } else {
            Mp4ChunkArray *next = node->next;
            if (node->words != NULL) {
                mp4MemFree(node->words);
            }
            if (node->extra_a != NULL) {
                mp4MemFree(node->extra_a);
            }
            if (node->wide_words != NULL) {
                mp4MemFree(node->wide_words);
            }
            mp4MemFree(node);
            prev->next = next;
        }
    }

    return cur;
}

/* 0x0232c4 */
MP4_NOINLINE int32_t mp4ChunkArrayAppendU64(
    Mp4ChunkArray **head_ptr,
    uint32_t cols,
    const uint64_t *vals,
    uint32_t chunk_cap)
{
    Mp4ChunkArray *orig_head = *head_ptr;
    Mp4ChunkArray *chunk = orig_head;
    uint32_t col;

    if (chunk != NULL) {
        if (chunk->cols < cols) {
            return MP4_ERR_PARSE_FATAL;
        }
    } else {
        chunk = (Mp4ChunkArray *)mp4MemCalloc(sizeof(Mp4ChunkArray));
        *head_ptr = chunk;
        if (chunk == NULL) {
            return MP4_ERR_NOMEM;
        }
    }

    if (chunk->wide_words == NULL) {
        if (mp4ChunkArrayInitU64(chunk, chunk_cap, cols) != MP4_OK) {
            if (orig_head == NULL) {
                mp4MemFree(chunk);
            }
            *head_ptr = NULL;
            return MP4_ERR_NOMEM;
        }
    }

    while (chunk->next != NULL) {
        chunk = chunk->next;
    }

    if (chunk->capacity == chunk->count) {
        Mp4ChunkArray *next_chunk = (Mp4ChunkArray *)mp4MemCalloc(sizeof(Mp4ChunkArray));
        chunk->next = next_chunk;
        if (next_chunk == NULL) {
            return MP4_ERR_NOMEM;
        }
        if (mp4ChunkArrayInitU64(next_chunk, chunk_cap, chunk->cols) != MP4_OK) {
            mp4MemFree(next_chunk);
            chunk->next = NULL;
            return MP4_ERR_NOMEM;
        }
        chunk = next_chunk;
    }

    for (col = 0U; col < cols; col++) {
        chunk->wide_words[chunk->count * chunk->cols + col] = vals[col];
    }
    for (col = cols; col < chunk->cols; col++) {
        chunk->wide_words[chunk->count * chunk->cols + col] = 0ULL;
    }

    chunk->count++;
    return MP4_OK;
}

/* 0x0234b8 */
MP4_NOINLINE int32_t mp4ChunkArraySetU64(
    Mp4ChunkArray **head_ptr,
    uint32_t row,
    uint32_t cols,
    uint32_t col,
    uint64_t val)
{
    Mp4ChunkArray *head = *head_ptr;
    Mp4ChunkArray *chunk;
    uint32_t rel = row;

    if (head == NULL) {
        return MP4_ERR_PARSE_FATAL;
    }
    if ((head->cols < cols) | (col >= cols)) {
        return MP4_ERR_PARSE_FATAL;
    }

    chunk = mp4ChunkArrayLocate(head, &rel);
    if (chunk == NULL) {
        return MP4_ERR_PARSE_FATAL;
    }

    chunk->wide_words[rel * cols + col] = val;
    return MP4_OK;
}

/* 0x023568 */
MP4_NOINLINE int32_t mp4ChunkArrayGetU64(
    Mp4ChunkArray *head,
    uint32_t row,
    uint32_t col,
    uint64_t *out_val)
{
    Mp4ChunkArray *chunk;
    uint32_t rel = row;

    if (head != NULL && col >= head->cols) {
        return MP4_ERR_PARSE_FATAL;
    }

    chunk = mp4ChunkArrayLocate(head, &rel);
    if (chunk == NULL) {
        return MP4_ERR_PARSE_FATAL;
    }

    *out_val = chunk->wide_words[rel * chunk->cols + col];
    return MP4_OK;
}

/* 0x0235f8 */
MP4_NOINLINE int32_t mp4ChunkArrayInitU32(Mp4ChunkArray *chunk, uint32_t cap, uint32_t cols)
{
    chunk->words = (uint32_t *)mp4MemMalloc(4U * cap * cols);
    if (chunk->words == NULL) {
        return MP4_ERR_NOMEM;
    }
    chunk->cols = cols;
    chunk->capacity = cap;
    chunk->next = NULL;
    chunk->count = 0U;
    return MP4_OK;
}

/* 0x023668 */
MP4_NOINLINE int32_t mp4ChunkArrayInitU64(Mp4ChunkArray *chunk, uint32_t cap, uint32_t cols)
{
    chunk->wide_words = (uint64_t *)mp4MemMalloc(8U * cap * cols);
    if (chunk->wide_words == NULL) {
        return MP4_ERR_NOMEM;
    }
    chunk->cols = cols;
    chunk->capacity = cap;
    chunk->next = NULL;
    chunk->count = 0U;
    return MP4_OK;
}
