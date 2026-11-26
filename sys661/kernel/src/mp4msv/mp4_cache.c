#include "mp4msv_internal.h"

/* Common prefix shared by all six Mp4Cache* sample table headers */
typedef struct Mp4CacheTableHdr {
    uint32_t size;
    uint32_t tag;
    uint8_t version;
    uint8_t flags[3];
    uint32_t const_val;
    uint32_t entry_count;
    uint32_t reserved_align;
    uint64_t file_offset;
} Mp4CacheTableHdr;

#define MP4_PLAN_NONE        0x00000000
#define MP4_PLAN_FWD_EXTEND  0x10000001
#define MP4_PLAN_FWD_RELOAD  0x10000002
#define MP4_PLAN_BWD_EXTEND  0x20000001
#define MP4_PLAN_BWD_RELOAD  0x20000002
#define MP4_PLAN_SUPERSET    0x20000003
#define MP4_PLAN_FWD_MASK    0x10000000

static __inline__ __attribute__((always_inline)) int32_t mp4CacheBlockReadWord(
    Mp4CacheBlock *head,
    uint32_t base_idx,
    uint32_t target_idx,
    uint32_t col,
    uint32_t *out_val)
{
    Mp4CacheBlock *blk;
    uint32_t rel;

    if (head == NULL || head->cols <= col || target_idx < base_idx) {
        return MP4_ERR_INTERNAL;
    }

    rel = target_idx - base_idx;
    blk = head;
    while (blk->count < rel + 1U) {
        rel -= blk->count;
        blk = blk->next;
        if (blk == NULL) {
            return MP4_ERR_INTERNAL;
        }
    }

    *out_val = blk->words[rel * blk->cols + col];
    return MP4_OK;
}

static __inline__ __attribute__((always_inline)) Mp4CacheBlock *mp4CachePopPoolBlock(
    Mp4CacheBlock **slot,
    Mp4CacheBlock **pool)
{
    Mp4CacheBlock *blk = *slot;

    if (blk == NULL) {
        if (*pool == NULL) {
            return NULL;
        }
        *slot = *pool;
        *pool = (*pool)->next;
        (*slot)->next = NULL;
        blk = *slot;
        blk->first_sample = 0U;
        blk->first_time = 0U;
        blk->first_chunk = 0U;
        blk->count = 0U;
    }

    return blk;
}

static __inline__ __attribute__((always_inline)) void mp4CacheReturnSlotBlock(
    Mp4CacheBlock **slot,
    Mp4CacheBlock **pool)
{
    if (*slot != NULL) {
        (*slot)->next = *pool;
        *pool = *slot;
        *slot = NULL;
    }
}

/* 0x013188 */
MP4_NOINLINE int32_t mp4CacheParseBox(
    Mp4CacheStbl *cache,
    uint32_t box_size,
    uint32_t tag,
    uint64_t box_end,
    Mp4IoStream *stream)
{
    uint32_t min_read;
    uint32_t should_load_stco = 0U;
    int32_t err;

    if (cache == NULL) {
        mp4SetParseFlag(tag, MP4_PARSE_FLAG_BAD_BOX_SIZE, stream);
        return MP4_ERR_PARSE_FATAL;
    }

    min_read = stream->min_read;

    if (tag == MP4_TAG_STSC) {
        err = mp4CacheInitTableHeader(&cache->stsc, box_size, MP4_TAG_STSC, 16U, 12U, box_end, stream);
        if (err == MP4_OK) {
            if (cache->stsc.entry_count == 0U) {
                return MP4_OK;
            }
            if (mp4CacheAllocPool(&cache->stsc.free_pool, cache->stsc.entry_count, 3U, stream) == MP4_OK) {
                if (mp4CacheLoadStsc(&cache->stsc, 0U, min_read,
                                     cache->ctrl->total_samples, cache->ctrl->total_chunks,
                                     stream) == MP4_OK) {
                    should_load_stco = (uint32_t)mp4CacheNeedsInitialStco(&cache->stco);
                    if (should_load_stco != 0U) {
                        mp4CacheLoadStcoForSamples(cache, 0U, min_read, stream);
                    }
                }
            }
        }
    } else if (tag == MP4_TAG_STSZ) {
        err = mp4CacheInitTableHeader(&cache->stsz, box_size, MP4_TAG_STSZ, 20U, 4U, box_end, stream);
        if (err == MP4_OK) {
            if (cache->stsz.sample_count == 0U) {
                return MP4_OK;
            }
            cache->ctrl->total_samples = mp4CacheStszCount(&cache->stsz);
            if (!mp4CacheIsStssTailResolved(&cache->stss)) {
                if (mp4CacheSetStssSampleTotal(&cache->stss, cache->ctrl->total_samples) != MP4_OK) {
                    if ((stream->error_flags & MP4_PARSE_FLAG_FATAL_MASK) != 0U) {
                        return MP4_ERR_PARSE_FATAL;
                    }
                    if ((uint64_t)mp4CbTell(stream->file_handle) < box_end) {
                        mp4SeekAbs(stream->file_handle, box_end);
                    }
                    return MP4_OK;
                }
            }
            if (mp4CacheAllocPool(&cache->stsz.free_pool, cache->stsz.sample_count, 1U, stream) == MP4_OK) {
                mp4CacheLoadStsz(&cache->stsz, 0U, min_read, 0U, &cache->ctrl->chunk_prefix_bytes, stream);
            }
        }
    } else if (tag == MP4_TAG_STTS) {
        err = mp4CacheInitTableHeader(&cache->stts, box_size, MP4_TAG_STTS, 16U, 8U, box_end, stream);
        if (err == MP4_OK) {
            if (cache->stts.entry_count == 0U) {
                return MP4_OK;
            }
            if (mp4CacheAllocPool(&cache->stts.free_pool, cache->stts.entry_count, 2U, stream) == MP4_OK) {
                mp4CacheLoadStts(&cache->stts, 0U, min_read,
                                 cache->ctrl->total_samples, cache->ctrl->total_duration, stream);
            }
        }
    } else if (tag == MP4_TAG_STSS) {
        err = mp4CacheInitTableHeader(&cache->stss, box_size, MP4_TAG_STSS, 16U, 4U, box_end, stream);
        if (err == MP4_OK) {
            if (cache->stss.entry_count == 0U) {
                return MP4_OK;
            }
            if (mp4CacheAllocPool(&cache->stss.free_pool, cache->stss.entry_count, 1U, stream) == MP4_OK) {
                mp4CacheLoadStss(&cache->stss, 0U, min_read, cache->ctrl->total_samples, stream);
            }
        }
    } else if (tag == MP4_TAG_STCO) {
        err = mp4CacheInitTableHeader(&cache->stco, box_size, MP4_TAG_STCO, 16U, 4U, box_end, stream);
        if (err == MP4_OK) {
            uint32_t stsc_chunk_cnt = 0U;

            if (cache->stco.entry_count == 0U) {
                return MP4_OK;
            }
            cache->ctrl->total_chunks = mp4CacheStcoCount(&cache->stco);
            if (mp4CacheAllocPool(&cache->stco.free_pool, cache->stco.entry_count, 1U, stream) == MP4_OK) {
                int32_t ok = 1;
                if (!mp4CacheIsStscTailResolved(&cache->stsc)) {
                    if (mp4CacheFixupStscTail(&cache->stsc, cache->ctrl->total_chunks - 1U) != MP4_OK) {
                        ok = 0;
                    }
                }
                if (ok) {
                    mp4CacheStscGetWindow(&cache->stsc, NULL, NULL, NULL, &stsc_chunk_cnt);
                    if (stsc_chunk_cnt != 0U) {
                        mp4CacheLoadStcoForSamples(cache, 0U, min_read, stream);
                    }
                }
            }
        }
    } else if (tag == MP4_TAG_CTTS) {
        err = mp4CacheInitTableHeader(&cache->ctts, box_size, MP4_TAG_CTTS, 16U, 8U, box_end, stream);
        if (err == MP4_OK) {
            if (cache->ctts.entry_count == 0U) {
                return MP4_OK;
            }
            if (mp4CacheAllocPool(&cache->ctts.free_pool, cache->ctts.entry_count, 2U, stream) == MP4_OK) {
                mp4CacheLoadCtts(&cache->ctts, 0U, min_read, cache->ctrl->total_samples, stream);
            }
        }
    }

    if ((stream->error_flags & MP4_PARSE_FLAG_FATAL_MASK) != 0U) {
        return MP4_ERR_PARSE_FATAL;
    }
    if ((uint64_t)mp4CbTell(stream->file_handle) < box_end) {
        mp4SeekAbs(stream->file_handle, box_end);
    }
    return MP4_OK;
}

/* 0x01362c */
MP4_NOINLINE int32_t mp4CacheAllocPool(
    Mp4CacheBlock **pool_ptr,
    uint32_t entry_count,
    uint32_t cols,
    Mp4IoStream *stream)
{
    uint32_t max_read;
    uint32_t base_unit;
    uint32_t num_blocks;

    if (entry_count == 0U) {
        return MP4_OK;
    }

    max_read = stream->max_read;
    if (max_read == 0U) {
        return MP4_ERR_INVALID_PARAM;
    }

    base_unit = stream->base_unit;
    if (max_read >= entry_count) {
        num_blocks = (entry_count - 1U) / base_unit + 1U;
    } else {
        num_blocks = max_read / base_unit + 2U;
    }

    while (num_blocks != 0U) {
        Mp4CacheBlock *blk;
        uint32_t *words;

        num_blocks--;
        blk = (Mp4CacheBlock *)mp4MemCalloc(sizeof(Mp4CacheBlock));
        *pool_ptr = blk;
        if (blk == NULL) {
            mp4SetParseFlag(0U, MP4_PARSE_FLAG_ALLOC_FAIL, stream);
            return MP4_ERR_NOMEM;
        }
        words = (uint32_t *)mp4MemMalloc(4U * cols * stream->base_unit);
        blk->words = words;
        if (words == NULL) {
            mp4SetParseFlag(0U, MP4_PARSE_FLAG_ALLOC_FAIL, stream);
            mp4MemFree(blk);
            *pool_ptr = NULL;
            return MP4_ERR_NOMEM;
        }
        blk->cols = cols;
        blk->capacity = stream->base_unit;
        pool_ptr = &blk->next;
    }

    return MP4_OK;
}

/* 0x01375c */
MP4_NOINLINE int32_t mp4CacheFreeBlocks(Mp4CacheBlock **head_ptr)
{
    Mp4CacheBlock *cur = *head_ptr;

    if (cur != NULL) {
        do {
            Mp4CacheBlock *next = cur->next;
            if (cur->words != NULL) {
                mp4MemFree(cur->words);
            }
            mp4MemFree(*head_ptr);
            *head_ptr = next;
            cur = next;
        } while (cur != NULL);
        *head_ptr = NULL;
    }

    return MP4_OK;
}

/* 0x0137c8 */
MP4_NOINLINE int32_t mp4CacheInitTableHeader(
    void *table,
    uint32_t box_size,
    uint32_t tag,
    uint32_t hdr_size,
    uint32_t entry_bytes,
    uint64_t box_end,
    Mp4IoStream *stream)
{
    Mp4CacheTableHdr *hdr = (Mp4CacheTableHdr *)table;
    uint8_t ver = 0U;
    uint32_t count;
    int64_t cur_pos;
    uint64_t expected_end;

    if (box_size < hdr_size) {
        mp4SetParseFlag(tag, MP4_PARSE_FLAG_BAD_BOX_SIZE, stream);
        return MP4_ERR_PARSE_FATAL;
    }

    mp4ReadBytes(&ver, 1U, stream);
    if (ver != 0U) {
        mp4SetParseFlag(tag, MP4_PARSE_FLAG_UNREC_BOX, stream);
        return MP4_ERR_PARSE_FATAL;
    }

    if (hdr->size != 0U) {
        mp4SetParseFlag(tag, MP4_PARSE_FLAG_DUPLICATE_BOX, stream);
        return MP4_ERR_PARSE_FATAL;
    }

    hdr->version = 0U;
    hdr->size = box_size;
    hdr->tag = tag;
    mp4ReadBytes(hdr->flags, 3U, stream);

    if (tag == MP4_TAG_STSZ) {
        hdr->const_val = mp4ReadU32(stream);
    }

    count = mp4ReadU32(stream);
    hdr->entry_count = count;
    if (hdr->const_val == 0U) {
        uint32_t max_entries = (box_size - hdr_size) / entry_bytes;
        if (max_entries < count) {
            mp4SetParseFlag(tag, MP4_PARSE_FLAG_COUNT_CLAMPED, stream);
            hdr->entry_count = max_entries;
        }
    }

    cur_pos = mp4CbTell(stream->file_handle);
    hdr->file_offset = (uint64_t)cur_pos;
    if (cur_pos == -1LL) {
        mp4SetParseFlag(tag, MP4_PARSE_FLAG_BAD_BOX_SIZE, stream);
        return MP4_ERR_PARSE_FATAL;
    }

    if (tag == MP4_TAG_STSZ && hdr->const_val != 0U) {
        if (hdr->file_offset != box_end) {
            mp4SetParseFlag(MP4_TAG_STSZ, MP4_PARSE_FLAG_SIZE_MISMATCH, stream);
        }
        return MP4_OK;
    }

    expected_end = hdr->file_offset + (uint64_t)(hdr->entry_count * entry_bytes);
    if (expected_end != box_end) {
        mp4SetParseFlag(tag, MP4_PARSE_FLAG_SIZE_MISMATCH, stream);
    }
    return MP4_OK;
}

/* 0x0139d8 */
MP4_NOINLINE int32_t mp4CachePlanWindow(
    uint32_t req_start,
    uint32_t req_count,
    uint32_t cur_start,
    uint32_t cur_count,
    uint32_t max_idx,
    int32_t *out_plan,
    int32_t *out_evict)
{
    uint32_t req_end = req_start + req_count - 1U;
    uint32_t cur_end = cur_start + cur_count - 1U;
    int32_t starts_after = (req_start >= cur_start);
    int32_t ends_before = (cur_end >= req_end);

    *out_plan = MP4_PLAN_FWD_RELOAD;
    *out_evict = 2;

    if (starts_after && ends_before) {
        *out_evict = 0;
        *out_plan = MP4_PLAN_NONE;
        return MP4_OK;
    }

    if (req_start < cur_start && cur_end < req_end) {
        *out_plan = MP4_PLAN_SUPERSET;
        *out_evict = 0;
        return MP4_OK;
    }

    if (starts_after && cur_end >= req_start) {
        *out_evict = 1;
        *out_plan = MP4_PLAN_FWD_EXTEND;
        return MP4_OK;
    }

    if (req_end >= cur_start && ends_before) {
        *out_evict = 1;
        *out_plan = MP4_PLAN_BWD_EXTEND;
        return MP4_OK;
    }

    if (cur_start >= req_start) {
        if (cur_start - req_end >= req_start) {
            *out_evict = 2;
            *out_plan = MP4_PLAN_FWD_RELOAD;
        } else {
            *out_evict = 2;
            *out_plan = MP4_PLAN_BWD_EXTEND;
        }
    } else {
        if (max_idx - req_end < req_start - cur_end) {
            *out_evict = 2;
            *out_plan = MP4_PLAN_BWD_RELOAD;
        } else {
            *out_evict = 2;
            *out_plan = MP4_PLAN_FWD_EXTEND;
        }
    }
    return MP4_OK;
}

/* 0x013ad4 */
MP4_NOINLINE int32_t mp4CacheLoadStts(
    Mp4CacheStts *stts,
    uint32_t req_start,
    uint32_t req_count,
    uint32_t total_samples,
    uint32_t total_dur,
    Mp4IoStream *stream)
{
    uint32_t start_sample = 0U;
    uint32_t start_time = 0U;
    uint32_t start_entry = 0U;
    int32_t is_fwd = 1;
    int32_t has_existing = 0;
    int32_t plan = MP4_PLAN_FWD_RELOAD;
    int32_t evict;
    int32_t err;

    if (req_count == 0U) {
        return MP4_ERR_INVALID_PARAM;
    }
    if (stts->size == 0U) {
        return MP4_ERR_CORRUPT;
    }
    if (stts->entry_count == 0U) {
        return MP4_OK;
    }

    if (stts->win_sample_count != 0U) {
        uint32_t last_sample = total_samples - 1U;

        if (mp4CachePlanWindow(req_start, req_count, stts->win_sample_start, stts->win_sample_count,
                               last_sample, &plan, &evict) != MP4_OK) {
            return MP4_ERR_INTERNAL;
        }

        switch (plan) {
            case MP4_PLAN_NONE:
                return MP4_OK;
            case MP4_PLAN_FWD_EXTEND:
                start_sample = stts->win_sample_start + stts->win_sample_count;
                start_time = stts->win_time_start + stts->win_time_count;
                start_entry = stts->win_entry_start + stts->win_entry_count;
                break;
            case MP4_PLAN_FWD_RELOAD:
                break;
            case MP4_PLAN_BWD_EXTEND:
            case MP4_PLAN_SUPERSET:
                start_time = stts->win_time_start - 1U;
                start_sample = stts->win_sample_start - 1U;
                start_entry = stts->win_entry_start - 1U;
                break;
            case MP4_PLAN_BWD_RELOAD:
                start_sample = last_sample;
                start_time = total_dur - 1U;
                start_entry = stts->entry_count - 1U;
                break;
            default:
                plan = MP4_PLAN_FWD_RELOAD;
                evict = 2;
                break;
        }

        is_fwd = ((plan & MP4_PLAN_FWD_MASK) != 0);
        if (evict == 1) {
            uint32_t trimmed_samples = 0U;
            uint32_t trimmed_dur = 0U;
            uint32_t trimmed_entries = 0U;

            err = mp4CacheTrimBlocks(&stts->active, req_start, req_count,
                                     stts->win_sample_start + stts->win_sample_count - 1U,
                                     stts->win_time_start + stts->win_time_count - 1U,
                                     0U, is_fwd,
                                     &trimmed_samples, &trimmed_dur, NULL, &trimmed_entries,
                                     &stts->free_pool);
            if (err != MP4_OK) {
                return err;
            }
            if (stts->active != NULL) {
                if (is_fwd) {
                    stts->win_sample_count -= trimmed_samples;
                    stts->win_time_count -= trimmed_dur;
                    stts->win_entry_count -= trimmed_entries;
                    stts->win_sample_start += trimmed_samples;
                    stts->win_time_start += trimmed_dur;
                    stts->win_entry_start += trimmed_entries;
                    has_existing = 1;
                } else {
                    stts->win_sample_count -= trimmed_samples;
                    stts->win_time_count -= trimmed_dur;
                    stts->win_entry_count -= trimmed_entries;
                }
            } else {
                is_fwd = 1;
                start_entry = 0U;
                start_sample = 0U;
                start_time = 0U;
                plan = MP4_PLAN_FWD_RELOAD;
                evict = 2;
            }
        } else if (evict == 2) {
            mp4CacheResetStts(stts);
        } else if (evict != 0) {
            mp4CacheResetStts(stts);
            evict = 2;
            is_fwd = 1;
            plan = MP4_PLAN_FWD_RELOAD;
            start_entry = 0U;
            start_sample = 0U;
            start_time = 0U;
        }
    }

    if (is_fwd) {
        err = mp4CacheLoadSttsFwd(stts, req_start, req_count,
                                  start_entry, start_sample, start_time, has_existing, stream);
        if (err != MP4_OK) {
            return err;
        }
        return MP4_OK;
    }

    err = mp4CacheLoadSttsBwd(stts, req_start, req_count,
                              start_entry, start_sample, start_time, has_existing, stream);
    if (err != MP4_OK) {
        return err;
    }
    if (plan != MP4_PLAN_SUPERSET) {
        return MP4_OK;
    }

    return mp4CacheLoadSttsFwd(stts, req_start, req_count,
                               stts->win_entry_start + stts->win_entry_count,
                               stts->win_sample_start + stts->win_sample_count,
                               stts->win_time_start + stts->win_time_count,
                               1, stream);
}

/* 0x013ec4 */
MP4_NOINLINE int32_t mp4CacheLoadSttsFwd(
    Mp4CacheStts *stts,
    uint32_t req_start,
    uint32_t req_count,
    uint32_t entry_idx,
    uint32_t sample_idx,
    uint32_t time_val,
    int32_t has_existing,
    Mp4IoStream *stream)
{
    Mp4CacheBlock **tail_slot;
    uint64_t seek_pos;
    uint32_t rem_entries;
    uint32_t req_end;

    tail_slot = mp4CacheFindTailSlot(&stts->active);
    seek_pos = stts->file_offset + (uint64_t)(8U * entry_idx);
    rem_entries = stts->entry_count - entry_idx;

    if ((uint64_t)mp4CbTell(stream->file_handle) != seek_pos) {
        if (mp4SeekAbs(stream->file_handle, seek_pos) != MP4_OK) {
            return MP4_ERR_IO;
        }
    }

    if (rem_entries == 0U) {
        return MP4_OK;
    }

    req_end = req_start + req_count - 1U;
    while (rem_entries != 0U) {
        uint32_t chunk_req = stream->base_unit;
        uint32_t blk_samples = 0U;
        uint32_t blk_dur = 0U;
        uint32_t blk_entries = 0U;
        int32_t err;

        if (rem_entries < chunk_req) {
            chunk_req = rem_entries;
        }
        err = mp4CacheReadSttsBlock(tail_slot, chunk_req, 1, sample_idx, time_val,
                                    &blk_samples, &blk_dur, &blk_entries,
                                    &stts->free_pool, stream);
        if (err != MP4_OK) {
            return err;
        }
        if (sample_idx + blk_samples - 1U >= req_start) {
            if (!has_existing) {
                has_existing = 1;
                stts->win_entry_start = entry_idx;
                stts->win_sample_start = sample_idx;
                stts->win_time_start = time_val;
            }
            stts->win_entry_count += blk_entries;
            stts->win_sample_count += blk_samples;
            stts->win_time_count += blk_dur;
            tail_slot = mp4CacheNextSlot(tail_slot);
            if (sample_idx + blk_samples - 1U >= req_end) {
                return MP4_OK;
            }
        }
        sample_idx += blk_samples;
        rem_entries -= blk_entries;
        entry_idx += blk_entries;
        time_val += blk_dur;
    }

    return MP4_OK;
}

/* 0x0140a8 */
MP4_NOINLINE int32_t mp4CacheLoadSttsBwd(
    Mp4CacheStts *stts,
    uint32_t req_start,
    uint32_t req_count,
    uint32_t entry_idx,
    uint32_t sample_idx,
    uint32_t time_val,
    int32_t has_existing,
    Mp4IoStream *stream)
{
    uint32_t base_unit = stream->base_unit;
    uint32_t rem_entries = stts->entry_count;
    uint32_t blk_start = (entry_idx / base_unit) * base_unit;
    uint32_t blk_req = rem_entries - blk_start;
    uint32_t req_end = req_start + req_count - 1U;
    Mp4CacheBlock *blk = NULL;

    if (rem_entries - base_unit >= blk_start) {
        rem_entries = entry_idx + 1U;
        blk_req = base_unit;
    }

    while (rem_entries != 0U) {
        uint64_t seek_pos = stts->file_offset + (uint64_t)(8U * blk_start);
        uint32_t blk_samples = 0U;
        uint32_t blk_dur = 0U;
        uint32_t blk_entries = 0U;
        int32_t err;

        if ((uint64_t)mp4CbTell(stream->file_handle) != seek_pos) {
            if (mp4SeekAbs(stream->file_handle, seek_pos) != MP4_OK) {
                return MP4_ERR_IO;
            }
        }

        err = mp4CacheReadSttsBlock(&blk, blk_req, 0, sample_idx, time_val,
                                    &blk_samples, &blk_dur, &blk_entries,
                                    &stts->free_pool, stream);
        if (err != MP4_OK) {
            return err;
        }
        if (req_end >= sample_idx - blk_samples + 1U) {
            stts->win_sample_start = sample_idx - blk_samples + 1U;
            stts->win_entry_start = blk_start;
            stts->win_entry_count += blk_entries;
            stts->win_time_start = time_val - blk_dur + 1U;
            stts->win_sample_count += blk_samples;
            stts->win_time_count += blk_dur;
            mp4CacheSetNext(blk, stts->active);
            stts->active = blk;
            blk = NULL;
            if (req_start >= sample_idx - blk_samples + 1U) {
                return MP4_OK;
            }
        }
        rem_entries -= blk_entries;
        sample_idx -= blk_samples;
        blk_req = stream->base_unit;
        blk_start -= blk_req;
        time_val -= blk_dur;
    }

    return has_existing ? MP4_OK : MP4_ERR_CORRUPT;
}

/* 0x0142d4 */
MP4_NOINLINE int32_t mp4CacheLoadStsc(
    Mp4CacheStsc *stsc,
    uint32_t req_start,
    uint32_t req_count,
    uint32_t total_samples,
    uint32_t total_chunks,
    Mp4IoStream *stream)
{
    uint32_t start_sample = 0U;
    uint32_t start_chunk = 0U;
    uint32_t start_entry = 0U;
    int32_t is_fwd = 1;
    int32_t has_existing = 0;
    int32_t plan = MP4_PLAN_FWD_RELOAD;
    int32_t evict;
    int32_t err;

    if (req_count == 0U) {
        return MP4_ERR_INVALID_PARAM;
    }
    if (stsc->size == 0U) {
        return MP4_ERR_CORRUPT;
    }
    if (stsc->entry_count == 0U) {
        return MP4_OK;
    }

    if (stsc->win_sample_count != 0U) {
        uint32_t last_sample = total_samples - 1U;

        if (mp4CachePlanWindow(req_start, req_count, stsc->win_sample_start, stsc->win_sample_count,
                               last_sample, &plan, &evict) != MP4_OK) {
            return MP4_ERR_INTERNAL;
        }
        if (plan == MP4_PLAN_NONE) {
            return MP4_OK;
        }

        if (plan == MP4_PLAN_FWD_EXTEND) {
            start_sample = stsc->win_sample_start + stsc->win_sample_count;
            start_chunk = stsc->win_chunk_start + stsc->win_chunk_count;
            start_entry = stsc->win_entry_start + stsc->win_entry_count;
        } else if (plan != MP4_PLAN_FWD_RELOAD) {
            if (plan == MP4_PLAN_BWD_EXTEND || plan == MP4_PLAN_SUPERSET) {
                start_sample = stsc->win_sample_start - 1U;
                start_chunk = stsc->win_chunk_start - 1U;
                start_entry = stsc->win_entry_start - 1U;
            } else if (plan == MP4_PLAN_BWD_RELOAD) {
                start_sample = last_sample;
                start_chunk = total_chunks - 1U;
                start_entry = stsc->entry_count - 1U;
            } else {
                plan = MP4_PLAN_FWD_RELOAD;
                evict = 2;
            }
        }

        is_fwd = ((plan & MP4_PLAN_FWD_MASK) != 0);
        if (evict == 1) {
            uint32_t trimmed_samples = 0U;
            uint32_t trimmed_chunks = 0U;
            uint32_t trimmed_entries = 0U;

            err = mp4CacheTrimBlocks(&stsc->active, req_start, req_count,
                                     stsc->win_sample_start + stsc->win_sample_count - 1U,
                                     0U,
                                     stsc->win_chunk_start + stsc->win_chunk_count - 1U,
                                     is_fwd,
                                     &trimmed_samples, NULL, &trimmed_chunks, &trimmed_entries,
                                     &stsc->free_pool);
            if (err != MP4_OK) {
                return err;
            }
            if (stsc->active != NULL) {
                if (is_fwd) {
                    stsc->win_sample_count -= trimmed_samples;
                    stsc->win_chunk_count -= trimmed_chunks;
                    stsc->win_entry_count -= trimmed_entries;
                    stsc->win_sample_start += trimmed_samples;
                    stsc->win_chunk_start += trimmed_chunks;
                    stsc->win_entry_start += trimmed_entries;
                    has_existing = 1;
                } else {
                    stsc->win_sample_count -= trimmed_samples;
                    stsc->win_chunk_count -= trimmed_chunks;
                    stsc->win_entry_count -= trimmed_entries;
                }
            } else {
                is_fwd = 1;
                start_entry = 0U;
                start_sample = 0U;
                start_chunk = 0U;
                plan = MP4_PLAN_FWD_RELOAD;
                evict = 2;
            }
        } else if (evict == 2) {
            mp4CacheResetStsc(stsc);
        } else if (evict != 0) {
            mp4CacheResetStsc(stsc);
            start_entry = 0U;
            plan = MP4_PLAN_FWD_RELOAD;
            start_sample = 0U;
            is_fwd = 1;
            evict = 2;
            start_chunk = 0U;
        }
    }

    if (is_fwd) {
        err = mp4CacheLoadStscFwd(stsc, req_start, req_count,
                                  start_entry, start_sample, start_chunk,
                                  has_existing, total_chunks, stream);
        if (err != MP4_OK) {
            return err;
        }
        return MP4_OK;
    }

    err = mp4CacheLoadStscBwd(stsc, req_start, req_count,
                              start_entry, start_sample, start_chunk,
                              has_existing, total_chunks, stream);
    if (err != MP4_OK) {
        return err;
    }
    if (plan != MP4_PLAN_SUPERSET) {
        return MP4_OK;
    }

    return mp4CacheLoadStscFwd(stsc, req_start, req_count,
                               stsc->win_entry_start + stsc->win_entry_count,
                               stsc->win_sample_start + stsc->win_sample_count,
                               stsc->win_chunk_start + stsc->win_chunk_count,
                               1, total_chunks, stream);
}

/* 0x0146d8 */
MP4_NOINLINE int32_t mp4CacheLoadStscFwd(
    Mp4CacheStsc *stsc,
    uint32_t req_start,
    uint32_t req_count,
    uint32_t entry_idx,
    uint32_t sample_idx,
    uint32_t chunk_idx,
    int32_t has_existing,
    uint32_t total_chunks,
    Mp4IoStream *stream)
{
    Mp4CacheBlock **tail_slot;
    uint32_t rem_entries;
    uint32_t req_end;

    tail_slot = mp4CacheFindTailSlot(&stsc->active);
    rem_entries = stsc->entry_count - entry_idx;
    if (rem_entries == 0U) {
        return MP4_OK;
    }

    req_end = req_start + req_count - 1U;
    while (rem_entries != 0U) {
        uint64_t seek_pos = stsc->file_offset + (uint64_t)(12U * entry_idx);
        uint32_t chunk_req;
        uint32_t blk_samples = 0U;
        uint32_t blk_chunks = 0U;
        uint32_t blk_entries = 0U;
        int32_t err;

        if ((uint64_t)mp4CbTell(stream->file_handle) != seek_pos) {
            if (mp4SeekAbs(stream->file_handle, seek_pos) != MP4_OK) {
                return MP4_ERR_IO;
            }
        }
        chunk_req = stream->base_unit;
        if (rem_entries < chunk_req) {
            chunk_req = rem_entries;
        }
        err = mp4CacheReadStscBlock(tail_slot, chunk_req, 1, sample_idx, chunk_idx,
                                    (rem_entries == chunk_req) ? 1 : 0, total_chunks,
                                    &stsc->tail_resolved, &blk_samples, &blk_chunks, &blk_entries,
                                    &stsc->free_pool, stream);
        if (err != MP4_OK) {
            return err;
        }
        if (sample_idx + blk_samples - 1U >= req_start) {
            if (!has_existing) {
                has_existing = 1;
                stsc->win_entry_start = entry_idx;
                stsc->win_sample_start = sample_idx;
                stsc->win_chunk_start = chunk_idx;
            }
            stsc->win_entry_count += blk_entries;
            stsc->win_sample_count += blk_samples;
            stsc->win_chunk_count += blk_chunks;
            tail_slot = mp4CacheNextSlot(tail_slot);
            if (stsc->tail_resolved == 0U) {
                return MP4_OK;
            }
            if (sample_idx + blk_samples - 1U >= req_end) {
                return MP4_OK;
            }
        }
        sample_idx += blk_samples;
        rem_entries -= blk_entries;
        entry_idx += blk_entries;
        chunk_idx += blk_chunks;
    }

    return MP4_OK;
}

/* 0x0148f8 */
MP4_NOINLINE int32_t mp4CacheLoadStscBwd(
    Mp4CacheStsc *stsc,
    uint32_t req_start,
    uint32_t req_count,
    uint32_t entry_idx,
    uint32_t sample_idx,
    uint32_t chunk_idx,
    int32_t has_existing,
    uint32_t total_chunks,
    Mp4IoStream *stream)
{
    uint32_t base_unit = stream->base_unit;
    uint32_t rem_entries = stsc->entry_count;
    uint32_t blk_start = (entry_idx / base_unit) * base_unit;
    uint32_t blk_req = rem_entries - blk_start;
    uint32_t req_end = req_start + req_count - 1U;
    Mp4CacheBlock *blk = NULL;

    if (rem_entries - base_unit >= blk_start) {
        rem_entries = entry_idx + 1U;
        blk_req = base_unit;
    }

    while (rem_entries != 0U) {
        uint64_t seek_pos = stsc->file_offset + (uint64_t)(12U * blk_start);
        uint32_t blk_samples = 0U;
        uint32_t blk_chunks = 0U;
        uint32_t blk_entries = 0U;
        int32_t err;

        if ((uint64_t)mp4CbTell(stream->file_handle) != seek_pos) {
            if (mp4SeekAbs(stream->file_handle, seek_pos) != MP4_OK) {
                return MP4_ERR_IO;
            }
        }

        err = mp4CacheReadStscBlock(&blk, blk_req, 0, sample_idx, chunk_idx,
                                    (rem_entries == stsc->entry_count) ? 1 : 0, total_chunks,
                                    NULL, &blk_samples, &blk_chunks, &blk_entries,
                                    &stsc->free_pool, stream);
        if (err != MP4_OK) {
            return err;
        }
        if (req_end >= sample_idx - blk_samples + 1U) {
            stsc->win_sample_start = sample_idx - blk_samples + 1U;
            stsc->win_entry_start = blk_start;
            stsc->win_entry_count += blk_entries;
            stsc->win_chunk_start = chunk_idx - blk_chunks + 1U;
            stsc->win_sample_count += blk_samples;
            stsc->win_chunk_count += blk_chunks;
            mp4CacheSetNext(blk, stsc->active);
            stsc->active = blk;
            blk = NULL;
            if (req_start >= sample_idx - blk_samples + 1U) {
                stsc->tail_resolved = 1U;
                return MP4_OK;
            }
        }
        rem_entries -= blk_entries;
        sample_idx -= blk_samples;
        blk_req = stream->base_unit;
        blk_start -= blk_req;
        chunk_idx -= blk_chunks;
    }

    if (has_existing) {
        stsc->tail_resolved = 1U;
        return MP4_OK;
    }
    return MP4_ERR_CORRUPT;
}

/* 0x014b58 */
MP4_NOINLINE int32_t mp4CacheLoadStsz(
    Mp4CacheStsz *stsz,
    uint32_t req_start,
    uint32_t req_count,
    uint32_t chunk_first_sample,
    uint32_t *prefix_bytes,
    Mp4IoStream *stream)
{
    uint32_t clamped_count;
    uint32_t saved_prefix;
    uint32_t load_start;
    uint32_t load_count;
    uint32_t req_end;
    int32_t plan = MP4_PLAN_FWD_RELOAD;
    int32_t evict;
    int32_t is_fwd = 1;
    int32_t has_existing = 0;
    int32_t err;

    if (req_count == 0U) {
        return MP4_ERR_INVALID_PARAM;
    }
    if (stsz->size == 0U) {
        return MP4_ERR_CORRUPT;
    }
    if (stsz->sample_count == 0U) {
        return MP4_OK;
    }

    saved_prefix = *prefix_bytes;
    *prefix_bytes = 0U;

    clamped_count = req_count;
    if (stsz->sample_count < req_count) {
        clamped_count = stsz->sample_count;
    }

    load_start = req_start;
    load_count = clamped_count;
    req_end = req_start + clamped_count - 1U;

    if (stsz->const_sample_size != 0U) {
        stsz->win_sample_count = clamped_count;
        stsz->win_sample_start = req_start;
        *prefix_bytes = (chunk_first_sample - req_start) * stsz->const_sample_size;
        return MP4_OK;
    }

    if (stsz->win_sample_count != 0U) {
        if (mp4CachePlanWindow(req_start, clamped_count, stsz->win_sample_start, stsz->win_sample_count,
                               stsz->sample_count - 1U, &plan, &evict) != MP4_OK) {
            return MP4_ERR_INTERNAL;
        }

        if (plan == MP4_PLAN_NONE) {
            *prefix_bytes = saved_prefix;
            return MP4_OK;
        }
        if ((plan == MP4_PLAN_BWD_EXTEND && evict == 1) || plan == MP4_PLAN_SUPERSET) {
            load_count = stsz->win_sample_start - req_start;
            load_start = stsz->win_sample_start - 1U;
            is_fwd = 0;
        }

        if (evict == 1) {
            uint32_t trimmed_samples = 0U;

            if (plan == MP4_PLAN_FWD_EXTEND) {
                err = mp4CacheCalcPrefixBytes(stsz, chunk_first_sample, req_start, saved_prefix, prefix_bytes);
                if (err != MP4_OK) {
                    return err;
                }
            }
            err = mp4CacheTrimBlocks(&stsz->active, req_start, clamped_count,
                                     stsz->win_sample_start + stsz->win_sample_count - 1U,
                                     0U, 0U, is_fwd,
                                     &trimmed_samples, NULL, NULL, NULL,
                                     &stsz->free_pool);
            if (err != MP4_OK) {
                return err;
            }
            if (stsz->active != NULL) {
                if (is_fwd == 1) {
                    load_start = stsz->win_sample_start + stsz->win_sample_count;
                    load_count = req_end - load_start + 1U;
                    has_existing = 1;
                    stsz->win_sample_count -= trimmed_samples;
                    stsz->win_sample_start += trimmed_samples;
                } else {
                    load_count = stsz->win_sample_start - req_start;
                    load_start = stsz->win_sample_start - 1U;
                    stsz->win_sample_count -= trimmed_samples;
                }
            } else {
                is_fwd = 1;
                plan = MP4_PLAN_FWD_RELOAD;
                evict = 2;
            }
        } else if (evict != 0) {
            if (evict != 2) {
                plan = MP4_PLAN_FWD_RELOAD;
                is_fwd = 1;
                evict = 2;
            }
            mp4CacheResetStsz(stsz);
        }
    }

    if (*prefix_bytes == 0U && chunk_first_sample != req_start) {
        err = mp4CacheReadAndSumStsz(stsz, chunk_first_sample, req_start - 1U, prefix_bytes, stream);
        if (err != MP4_OK) {
            return err;
        }
    }

    if (is_fwd == 1) {
        return mp4CacheLoadStszFwd(stsz, load_start, load_count, has_existing, stream);
    }

    err = mp4CacheLoadStszBwd(stsz, load_start, load_count, stream);
    if (err != MP4_OK) {
        return err;
    }
    if (plan != MP4_PLAN_SUPERSET) {
        return MP4_OK;
    }

    load_start = stsz->win_sample_start + stsz->win_sample_count;
    return mp4CacheLoadStszFwd(stsz, load_start, req_end - load_start + 1U, 1, stream);
}

/* 0x014f0c */
MP4_NOINLINE int32_t mp4CacheLoadStszFwd(
    Mp4CacheStsz *stsz,
    uint32_t start_idx,
    uint32_t count,
    int32_t has_existing,
    Mp4IoStream *stream)
{
    Mp4CacheBlock **tail_slot;
    uint64_t seek_pos;
    uint32_t rem = count;

    tail_slot = mp4CacheFindTailSlot(&stsz->active);
    seek_pos = stsz->file_offset + (uint64_t)(4U * start_idx);

    if ((uint64_t)mp4CbTell(stream->file_handle) != seek_pos) {
        if (mp4SeekAbs(stream->file_handle, seek_pos) != MP4_OK) {
            return MP4_ERR_IO;
        }
    }

    while (rem != 0U) {
        uint32_t chunk_req = stream->base_unit;
        uint32_t got = 0U;
        int32_t err;

        if (rem < chunk_req) {
            chunk_req = rem;
        }
        err = mp4CacheReadStszBlock(tail_slot, start_idx, chunk_req, &got, &stsz->free_pool, stream);
        if (err != MP4_OK) {
            return err;
        }
        if (!has_existing) {
            stsz->win_sample_start = start_idx;
            has_existing = 1;
        }
        rem -= got;
        start_idx += got;
        stsz->win_sample_count += got;
        tail_slot = mp4CacheNextSlot(tail_slot);
    }

    return MP4_OK;
}

/* 0x015058 */
MP4_NOINLINE int32_t mp4CacheLoadStszBwd(
    Mp4CacheStsz *stsz,
    uint32_t end_idx,
    uint32_t count,
    Mp4IoStream *stream)
{
    uint32_t rem = count;
    uint32_t base_unit = stream->base_unit;
    uint32_t blk_start = end_idx - base_unit + 1U;
    Mp4CacheBlock *blk = NULL;

    while (rem != 0U) {
        uint64_t seek_pos = stsz->file_offset + (uint64_t)(4U * blk_start);
        uint32_t got = 0U;
        int32_t err;

        if ((uint64_t)mp4CbTell(stream->file_handle) != seek_pos) {
            if (mp4SeekAbs(stream->file_handle, seek_pos) != MP4_OK) {
                return MP4_ERR_IO;
            }
        }
        err = mp4CacheReadStszBlock(&blk, blk_start, base_unit, &got, &stsz->free_pool, stream);
        if (err != MP4_OK) {
            return err;
        }
        stsz->win_sample_start = blk_start;
        rem -= got;
        stsz->win_sample_count += got;
        blk_start -= got;
        mp4CacheSetNext(blk, stsz->active);
        stsz->active = blk;
        blk = NULL;
    }

    return MP4_OK;
}

/* 0x015184 */
MP4_NOINLINE int32_t mp4CacheLoadStco(
    Mp4CacheStco *stco,
    uint32_t req_start,
    uint32_t req_count,
    Mp4IoStream *stream)
{
    uint32_t req_end;
    uint32_t load_start;
    uint32_t load_count;
    int32_t plan = MP4_PLAN_FWD_RELOAD;
    int32_t evict;
    int32_t is_fwd = 1;
    int32_t has_existing = 0;
    int32_t err;

    if (req_count == 0U) {
        return MP4_ERR_INVALID_PARAM;
    }
    if (stco->size == 0U) {
        return MP4_ERR_CORRUPT;
    }
    if (stco->entry_count == 0U) {
        return MP4_OK;
    }

    req_end = req_start + req_count - 1U;
    load_start = req_start;
    load_count = req_count;

    if (stco->win_chunk_count != 0U) {
        if (mp4CachePlanWindow(req_start, req_count, stco->win_chunk_start, stco->win_chunk_count,
                               stco->entry_count - 1U, &plan, &evict) != MP4_OK) {
            return MP4_ERR_INTERNAL;
        }

        if (plan == MP4_PLAN_NONE) {
            return MP4_OK;
        }
        if ((plan == MP4_PLAN_BWD_EXTEND && evict == 1) || plan == MP4_PLAN_SUPERSET) {
            is_fwd = 0;
            load_count = stco->win_chunk_start - req_start;
            load_start = stco->win_chunk_start - 1U;
        }

        if (evict == 1) {
            uint32_t trimmed_chunks = 0U;

            err = mp4CacheTrimStcoBlocks(&stco->active, req_start, req_count, is_fwd,
                                         &trimmed_chunks, &stco->free_pool);
            if (err != MP4_OK) {
                return err;
            }
            if (stco->active != NULL) {
                if (is_fwd == 1) {
                    load_start = stco->win_chunk_start + stco->win_chunk_count;
                    load_count = req_end - load_start + 1U;
                    has_existing = 1;
                    stco->win_chunk_count -= trimmed_chunks;
                    stco->win_chunk_start += trimmed_chunks;
                } else {
                    load_count = stco->win_chunk_start - req_start;
                    load_start = stco->win_chunk_start - 1U;
                    stco->win_chunk_count -= trimmed_chunks;
                }
            } else {
                is_fwd = 1;
                plan = MP4_PLAN_FWD_RELOAD;
                evict = 2;
            }
        } else if (evict != 0) {
            if (evict != 2) {
                plan = MP4_PLAN_FWD_RELOAD;
                is_fwd = 1;
                evict = 2;
            }
            mp4CacheResetStco(stco);
        }
    }

    if (is_fwd == 1) {
        return mp4CacheLoadStcoFwd(stco, load_start, load_count, has_existing, stream);
    }

    err = mp4CacheLoadStcoBwd(stco, load_start, load_count, stream);
    if (err != MP4_OK) {
        return err;
    }
    if (plan != MP4_PLAN_SUPERSET) {
        return MP4_OK;
    }

    load_start = stco->win_chunk_start + stco->win_chunk_count;
    return mp4CacheLoadStcoFwd(stco, load_start, req_end - load_start + 1U, 1, stream);
}

/* 0x01542c */
MP4_NOINLINE int32_t mp4CacheLoadStcoFwd(
    Mp4CacheStco *stco,
    uint32_t start_idx,
    uint32_t count,
    int32_t has_existing,
    Mp4IoStream *stream)
{
    Mp4CacheBlock **tail_slot;
    uint64_t seek_pos;
    uint32_t rem = count;

    tail_slot = mp4CacheFindTailSlot(&stco->active);
    seek_pos = stco->file_offset + (uint64_t)(4U * start_idx);

    if ((uint64_t)mp4CbTell(stream->file_handle) != seek_pos) {
        if (mp4SeekAbs(stream->file_handle, seek_pos) != MP4_OK) {
            return MP4_ERR_IO;
        }
    }

    while (rem != 0U) {
        uint32_t chunk_req = rem;
        uint32_t got = 0U;
        int32_t err;

        if (chunk_req >= stream->base_unit) {
            chunk_req = stream->base_unit;
        }
        err = mp4CacheReadStcoBlock(tail_slot, start_idx, chunk_req, &got, &stco->free_pool, stream);
        if (err != MP4_OK) {
            return err;
        }
        if (!has_existing) {
            stco->win_chunk_start = start_idx;
            has_existing = 1;
        }
        rem -= got;
        start_idx += got;
        stco->win_chunk_count += chunk_req;
        tail_slot = mp4CacheNextSlot(tail_slot);
    }

    return MP4_OK;
}

/* 0x015580 */
MP4_NOINLINE int32_t mp4CacheLoadStcoBwd(
    Mp4CacheStco *stco,
    uint32_t end_idx,
    uint32_t count,
    Mp4IoStream *stream)
{
    uint32_t rem = count;
    uint32_t base_unit = stream->base_unit;
    uint32_t blk_start = end_idx - base_unit + 1U;
    Mp4CacheBlock *blk = NULL;

    while (rem != 0U) {
        uint64_t seek_pos = stco->file_offset + (uint64_t)(4U * blk_start);
        uint32_t got = 0U;
        int32_t err;

        if ((uint64_t)mp4CbTell(stream->file_handle) != seek_pos) {
            if (mp4SeekAbs(stream->file_handle, seek_pos) != MP4_OK) {
                return MP4_ERR_IO;
            }
        }
        err = mp4CacheReadStcoBlock(&blk, blk_start, base_unit, &got, &stco->free_pool, stream);
        if (err != MP4_OK) {
            return err;
        }
        stco->win_chunk_start = blk_start;
        rem -= got;
        stco->win_chunk_count += got;
        blk_start -= stream->base_unit;
        mp4CacheSetNext(blk, stco->active);
        stco->active = blk;
        blk = NULL;
    }

    return MP4_OK;
}

/* 0x0156b0 */
MP4_NOINLINE int32_t mp4CacheLoadStss(
    Mp4CacheStss *stss,
    uint32_t req_start,
    uint32_t req_count,
    uint32_t total_samples,
    Mp4IoStream *stream)
{
    uint32_t start_sample = 0U;
    uint32_t start_entry = 0U;
    int32_t is_fwd = 1;
    int32_t has_existing = 0;
    int32_t plan = MP4_PLAN_FWD_MASK;
    int32_t evict;
    int32_t err;

    if (req_count == 0U) {
        return MP4_ERR_INVALID_PARAM;
    }
    if (stss->size == 0U || stss->entry_count == 0U) {
        return MP4_OK;
    }

    if (stss->win_sample_count != 0U) {
        uint32_t last_sample = total_samples - 1U;

        if (mp4CachePlanWindow(req_start, req_count, stss->win_sample_start, stss->win_sample_count,
                               last_sample, &plan, &evict) != MP4_OK) {
            return MP4_ERR_INTERNAL;
        }
        if (plan == MP4_PLAN_NONE) {
            return MP4_OK;
        }

        is_fwd = ((plan & MP4_PLAN_FWD_MASK) != 0);
        switch (plan) {
            case MP4_PLAN_FWD_EXTEND:
                start_sample = stss->win_sample_start + stss->win_sample_count;
                start_entry = stss->win_entry_start + stss->win_entry_count;
                break;
            case MP4_PLAN_BWD_EXTEND:
            case MP4_PLAN_SUPERSET:
                start_sample = stss->win_sample_start - 1U;
                start_entry = stss->win_entry_start - 1U;
                break;
            case MP4_PLAN_BWD_RELOAD:
                start_sample = last_sample;
                start_entry = stss->entry_count - 1U;
                break;
            case MP4_PLAN_FWD_RELOAD:
                break;
            default:
                plan = MP4_PLAN_FWD_RELOAD;
                is_fwd = 1;
                evict = 2;
                break;
        }

        if (evict == 1) {
            uint32_t trimmed_samples = 0U;
            uint32_t trimmed_entries = 0U;

            err = mp4CacheTrimBlocks(&stss->active, req_start, req_count,
                                     stss->win_sample_start + stss->win_sample_count - 1U,
                                     0U, 0U, is_fwd,
                                     &trimmed_samples, NULL, NULL, &trimmed_entries,
                                     &stss->free_pool);
            if (err != MP4_OK) {
                return err;
            }
            if (stss->active != NULL) {
                if (is_fwd) {
                    stss->win_sample_count -= trimmed_samples;
                    stss->win_entry_count -= trimmed_entries;
                    stss->win_sample_start += trimmed_samples;
                    stss->win_entry_start += trimmed_entries;
                    has_existing = 1;
                } else {
                    stss->win_sample_count -= trimmed_samples;
                    stss->win_entry_count -= trimmed_entries;
                }
            } else {
                is_fwd = 1;
                start_entry = 0U;
                start_sample = 0U;
                plan = MP4_PLAN_FWD_RELOAD;
                evict = 2;
            }
        } else if (evict != 0) {
            if (evict != 2) {
                evict = 2;
                plan = MP4_PLAN_FWD_RELOAD;
                is_fwd = 1;
                start_entry = 0U;
                start_sample = 0U;
            }
            mp4CacheResetStss(stss);
        }
    }

    if (is_fwd) {
        return mp4CacheLoadStssFwd(stss, req_start, req_count,
                                   start_entry, start_sample, has_existing, total_samples, stream);
    }

    err = mp4CacheLoadStssBwd(stss, req_start, req_count,
                              start_entry, start_sample, has_existing, total_samples, stream);
    if (err != MP4_OK) {
        return err;
    }
    if (plan != MP4_PLAN_SUPERSET) {
        return MP4_OK;
    }

    return mp4CacheLoadStssFwd(stss, req_start, req_count,
                               stss->win_entry_start + stss->win_entry_count,
                               stss->win_sample_start + stss->win_sample_count,
                               1, total_samples, stream);
}

/* 0x015a24 */
MP4_NOINLINE int32_t mp4CacheLoadStssFwd(
    Mp4CacheStss *stss,
    uint32_t req_start,
    uint32_t req_count,
    uint32_t entry_idx,
    uint32_t sample_idx,
    int32_t has_existing,
    uint32_t total_samples,
    Mp4IoStream *stream)
{
    Mp4CacheBlock **tail_slot;
    uint32_t rem_entries;
    int32_t is_first;
    uint32_t req_end;

    tail_slot = mp4CacheFindTailSlot(&stss->active);
    rem_entries = stss->entry_count - entry_idx;
    is_first = (rem_entries == stss->entry_count);
    if (rem_entries == 0U) {
        return MP4_OK;
    }

    req_end = req_start + req_count - 1U;
    while (rem_entries != 0U) {
        uint64_t seek_pos = stss->file_offset + (uint64_t)(4U * entry_idx);
        uint32_t chunk_req;
        uint32_t blk_samples = 0U;
        uint32_t blk_entries = 0U;
        int32_t cur_first;
        int32_t err;

        if ((uint64_t)mp4CbTell(stream->file_handle) != seek_pos) {
            if (mp4SeekAbs(stream->file_handle, seek_pos) != MP4_OK) {
                return MP4_ERR_IO;
            }
        }
        chunk_req = stream->base_unit;
        if (rem_entries < chunk_req) {
            chunk_req = rem_entries;
        }
        cur_first = is_first;
        is_first = 0;
        err = mp4CacheReadStssBlock(tail_slot, chunk_req, cur_first,
                                    (rem_entries == chunk_req) ? 1 : 0, total_samples,
                                    &stss->tail_resolved, &blk_samples, &blk_entries,
                                    &stss->free_pool, stream);
        if (err != MP4_OK) {
            return err;
        }
        if (sample_idx + blk_samples - 1U >= req_start) {
            if (!has_existing) {
                has_existing = 1;
                stss->win_entry_start = entry_idx;
                stss->win_sample_start = sample_idx;
            }
            stss->win_entry_count += blk_entries;
            stss->win_sample_count += blk_samples;
            tail_slot = mp4CacheNextSlot(tail_slot);
            if (stss->tail_resolved == 0U) {
                return MP4_OK;
            }
            if (sample_idx + blk_samples - 1U >= req_end) {
                return MP4_OK;
            }
        }
        sample_idx += blk_samples;
        rem_entries -= blk_entries;
        entry_idx += blk_entries;
    }

    return MP4_OK;
}

/* 0x015c14 */
MP4_NOINLINE int32_t mp4CacheLoadStssBwd(
    Mp4CacheStss *stss,
    uint32_t req_start,
    uint32_t req_count,
    uint32_t entry_idx,
    uint32_t sample_idx,
    int32_t has_existing,
    uint32_t total_samples,
    Mp4IoStream *stream)
{
    uint32_t base_unit = stream->base_unit;
    uint32_t rem_entries = stss->entry_count;
    uint32_t blk_start = (entry_idx / base_unit) * base_unit;
    uint32_t blk_req = rem_entries - blk_start;
    uint32_t req_end = req_start + req_count - 1U;
    int32_t is_last;
    Mp4CacheBlock *blk = NULL;

    if (rem_entries - base_unit >= blk_start) {
        rem_entries = entry_idx + 1U;
        blk_req = base_unit;
    }
    is_last = (rem_entries == stss->entry_count);

    while (rem_entries != 0U) {
        uint64_t seek_pos = stss->file_offset + (uint64_t)(4U * blk_start);
        uint32_t blk_samples = 0U;
        uint32_t blk_entries = 0U;
        int32_t cur_last;
        int32_t err;

        if ((uint64_t)mp4CbTell(stream->file_handle) != seek_pos) {
            if (mp4SeekAbs(stream->file_handle, seek_pos) != MP4_OK) {
                return MP4_ERR_IO;
            }
        }
        cur_last = is_last;
        is_last = 0;
        err = mp4CacheReadStssBlock(&blk, blk_req, (rem_entries == blk_req) ? 1 : 0, cur_last,
                                    total_samples, NULL, &blk_samples, &blk_entries,
                                    &stss->free_pool, stream);
        if (err != MP4_OK) {
            return err;
        }
        if (req_end >= sample_idx - blk_samples + 1U) {
            stss->win_sample_start = sample_idx - blk_samples + 1U;
            stss->win_sample_count += blk_samples;
            stss->win_entry_start = blk_start;
            stss->win_entry_count += blk_entries;
            mp4CacheSetNext(blk, stss->active);
            stss->active = blk;
            blk = NULL;
            if (req_start >= sample_idx - blk_samples + 1U) {
                stss->tail_resolved = 1U;
                return MP4_OK;
            }
        }
        sample_idx -= blk_samples;
        rem_entries -= blk_entries;
        blk_req = stream->base_unit;
        blk_start -= blk_req;
    }

    if (has_existing) {
        stss->tail_resolved = 1U;
        return MP4_OK;
    }
    return MP4_ERR_CORRUPT;
}

/* 0x015e34 */
MP4_NOINLINE int32_t mp4CacheLoadCtts(
    Mp4CacheCtts *ctts,
    uint32_t req_start,
    uint32_t req_count,
    uint32_t total_samples,
    Mp4IoStream *stream)
{
    uint32_t start_sample = 0U;
    uint32_t start_entry = 0U;
    int32_t is_fwd = 1;
    int32_t has_existing = 0;
    int32_t plan = MP4_PLAN_FWD_MASK;
    int32_t evict;
    int32_t err;

    if (req_count == 0U) {
        return MP4_ERR_INVALID_PARAM;
    }
    if (ctts->size == 0U || ctts->entry_count == 0U) {
        return MP4_OK;
    }

    if (ctts->win_sample_count != 0U) {
        uint32_t last_sample = total_samples - 1U;

        if (mp4CachePlanWindow(req_start, req_count, ctts->win_sample_start, ctts->win_sample_count,
                               last_sample, &plan, &evict) != MP4_OK) {
            return MP4_ERR_INTERNAL;
        }
        if (plan == MP4_PLAN_NONE) {
            return MP4_OK;
        }

        is_fwd = ((plan & MP4_PLAN_FWD_MASK) != 0);
        switch (plan) {
            case MP4_PLAN_FWD_EXTEND:
                start_sample = ctts->win_sample_start + ctts->win_sample_count;
                start_entry = ctts->win_entry_start + ctts->win_entry_count;
                break;
            case MP4_PLAN_FWD_RELOAD:
                break;
            case MP4_PLAN_BWD_EXTEND:
            case MP4_PLAN_SUPERSET:
                start_sample = ctts->win_sample_start - 1U;
                start_entry = ctts->win_entry_start - 1U;
                break;
            case MP4_PLAN_BWD_RELOAD:
                start_sample = last_sample;
                start_entry = ctts->entry_count - 1U;
                break;
            default:
                plan = MP4_PLAN_FWD_RELOAD;
                is_fwd = 1;
                evict = 2;
                break;
        }

        if (evict == 1) {
            uint32_t trimmed_samples = 0U;
            uint32_t trimmed_entries = 0U;

            err = mp4CacheTrimBlocks(&ctts->active, req_start, req_count,
                                     ctts->win_sample_start + ctts->win_sample_count - 1U,
                                     0U, 0U, is_fwd,
                                     &trimmed_samples, NULL, NULL, &trimmed_entries,
                                     &ctts->free_pool);
            if (err != MP4_OK) {
                return err;
            }
            if (ctts->active != NULL) {
                if (is_fwd) {
                    ctts->win_sample_count -= trimmed_samples;
                    ctts->win_entry_count -= trimmed_entries;
                    ctts->win_sample_start += trimmed_samples;
                    ctts->win_entry_start += trimmed_entries;
                    has_existing = 1;
                } else {
                    ctts->win_sample_count -= trimmed_samples;
                    ctts->win_entry_count -= trimmed_entries;
                }
            } else {
                is_fwd = 1;
                start_entry = 0U;
                start_sample = 0U;
                plan = MP4_PLAN_FWD_RELOAD;
                evict = 2;
            }
        } else if (evict != 0) {
            if (evict != 2) {
                evict = 2;
                plan = MP4_PLAN_FWD_RELOAD;
                is_fwd = 1;
                start_entry = 0U;
                start_sample = 0U;
            }
            mp4CacheResetCtts(ctts);
        }
    }

    if (is_fwd) {
        return mp4CacheLoadCttsFwd(ctts, req_start, req_count,
                                   start_entry, start_sample, has_existing, stream);
    }

    err = mp4CacheLoadCttsBwd(ctts, req_start, req_count,
                              start_entry, start_sample, has_existing, stream);
    if (err != MP4_OK) {
        return err;
    }
    if (plan != MP4_PLAN_SUPERSET) {
        return MP4_OK;
    }

    return mp4CacheLoadCttsFwd(ctts, req_start, req_count,
                               ctts->win_entry_start + ctts->win_entry_count,
                               ctts->win_sample_start + ctts->win_sample_count,
                               1, stream);
}

/* 0x016194 */
MP4_NOINLINE int32_t mp4CacheLoadCttsFwd(
    Mp4CacheCtts *ctts,
    uint32_t req_start,
    uint32_t req_count,
    uint32_t entry_idx,
    uint32_t sample_idx,
    int32_t has_existing,
    Mp4IoStream *stream)
{
    Mp4CacheBlock **tail_slot;
    uint64_t seek_pos;
    uint32_t rem_entries;
    uint32_t req_end;

    tail_slot = mp4CacheFindTailSlot(&ctts->active);
    seek_pos = ctts->file_offset + (uint64_t)(8U * entry_idx);
    rem_entries = ctts->entry_count - entry_idx;

    if ((uint64_t)mp4CbTell(stream->file_handle) != seek_pos) {
        if (mp4SeekAbs(stream->file_handle, seek_pos) != MP4_OK) {
            return MP4_ERR_IO;
        }
    }

    if (rem_entries == 0U) {
        return MP4_OK;
    }

    req_end = req_start + req_count - 1U;
    while (rem_entries != 0U) {
        uint32_t chunk_req = stream->base_unit;
        uint32_t blk_samples = 0U;
        uint32_t blk_entries = 0U;
        int32_t err;

        if (rem_entries < chunk_req) {
            chunk_req = rem_entries;
        }
        err = mp4CacheReadCttsBlock(tail_slot, chunk_req, 1, sample_idx,
                                    &blk_samples, &blk_entries, &ctts->free_pool, stream);
        if (err != MP4_OK) {
            return err;
        }
        if (sample_idx + blk_samples - 1U >= req_start) {
            if (!has_existing) {
                has_existing = 1;
                ctts->win_entry_start = entry_idx;
                ctts->win_sample_start = sample_idx;
            }
            ctts->win_sample_count += blk_samples;
            ctts->win_entry_count += blk_entries;
            tail_slot = mp4CacheNextSlot(tail_slot);
            if (sample_idx + blk_samples - 1U >= req_end) {
                return MP4_OK;
            }
        }
        sample_idx += blk_samples;
        rem_entries -= blk_entries;
        entry_idx += blk_entries;
    }

    return MP4_OK;
}

/* 0x01634c */
MP4_NOINLINE int32_t mp4CacheLoadCttsBwd(
    Mp4CacheCtts *ctts,
    uint32_t req_start,
    uint32_t req_count,
    uint32_t entry_idx,
    uint32_t sample_idx,
    int32_t has_existing,
    Mp4IoStream *stream)
{
    uint32_t base_unit = stream->base_unit;
    uint32_t rem_entries = ctts->entry_count;
    uint32_t blk_start = (entry_idx / base_unit) * base_unit;
    uint32_t blk_req = rem_entries - blk_start;
    uint32_t req_end = req_start + req_count - 1U;
    Mp4CacheBlock *blk = NULL;

    if (rem_entries - base_unit >= blk_start) {
        rem_entries = entry_idx + 1U;
        blk_req = base_unit;
    }

    while (rem_entries != 0U) {
        uint64_t seek_pos = ctts->file_offset + (uint64_t)(8U * blk_start);
        uint32_t blk_samples = 0U;
        uint32_t blk_entries = 0U;
        int32_t err;

        if ((uint64_t)mp4CbTell(stream->file_handle) != seek_pos) {
            if (mp4SeekAbs(stream->file_handle, seek_pos) != MP4_OK) {
                return MP4_ERR_IO;
            }
        }
        err = mp4CacheReadCttsBlock(&blk, blk_req, 0, sample_idx,
                                    &blk_samples, &blk_entries, &ctts->free_pool, stream);
        if (err != MP4_OK) {
            return err;
        }
        if (req_end >= sample_idx - blk_samples + 1U) {
            ctts->win_sample_start = sample_idx - blk_samples + 1U;
            ctts->win_sample_count += blk_samples;
            ctts->win_entry_start = blk_start;
            ctts->win_entry_count += blk_entries;
            mp4CacheSetNext(blk, ctts->active);
            ctts->active = blk;
            blk = NULL;
            if (req_start >= sample_idx - blk_samples + 1U) {
                return MP4_OK;
            }
        }
        sample_idx -= blk_samples;
        rem_entries -= blk_entries;
        blk_req = stream->base_unit;
        blk_start -= blk_req;
    }

    return has_existing ? MP4_OK : MP4_ERR_CORRUPT;
}

/* 0x016538 */
MP4_NOINLINE int32_t mp4CacheReadSttsBlock(
    Mp4CacheBlock **slot,
    uint32_t req_entries,
    int32_t is_fwd,
    uint32_t sample_ref,
    uint32_t time_ref,
    uint32_t *out_samples,
    uint32_t *out_dur,
    uint32_t *out_entries,
    Mp4CacheBlock **pool,
    Mp4IoStream *stream)
{
    Mp4CacheBlock *blk;
    uint32_t read_cnt;
    uint32_t idx;
    uint32_t *cursor;

    *out_samples = 0U;
    *out_dur = 0U;
    *out_entries = 0U;

    blk = mp4CachePopPoolBlock(slot, pool);
    if (blk == NULL) {
        return MP4_ERR_INTERNAL;
    }

    read_cnt = req_entries;
    if (stream->base_unit < read_cnt) {
        read_cnt = stream->base_unit;
    }

    mp4ReadU32Array(blk->words, 8U * read_cnt, stream);
    if ((stream->error_flags & MP4_PARSE_FLAG_FATAL_MASK) != 0U) {
        mp4CacheReturnSlotBlock(slot, pool);
        return MP4_ERR_IO;
    }

    blk->count = read_cnt;
    cursor = blk->words;
    for (idx = 0U; idx < blk->count; idx++, cursor += 2) {
        uint32_t run_samples = cursor[0];
        *out_samples += run_samples;
        *out_dur += cursor[1] * run_samples;
    }
    *out_entries = blk->count;

    if (is_fwd == 1) {
        blk->first_time = time_ref;
        blk->first_sample = sample_ref;
    } else {
        blk->first_sample = sample_ref - *out_samples + 1U;
        blk->first_time = time_ref - *out_dur + 1U;
    }
    return MP4_OK;
}

/* 0x0166fc */
MP4_NOINLINE int32_t mp4CacheReadStscBlock(
    Mp4CacheBlock **slot,
    uint32_t req_entries,
    int32_t is_fwd,
    uint32_t sample_ref,
    uint32_t chunk_ref,
    int32_t is_last,
    uint32_t total_chunks,
    uint32_t *out_resolved,
    uint32_t *out_samples,
    uint32_t *out_chunks,
    uint32_t *out_entries,
    Mp4CacheBlock **pool,
    Mp4IoStream *stream)
{
    Mp4CacheBlock *blk;
    uint32_t read_cnt;
    uint32_t *words;
    uint32_t *next_word;
    uint32_t cur_chunk;
    uint32_t cur_spc;
    uint32_t next_chunk = 0U;
    uint32_t next_spc = 0U;
    uint32_t idx = 0U;
    uint32_t resolved = 0U;

    *out_samples = 0U;
    *out_chunks = 0U;
    *out_entries = 0U;

    blk = mp4CachePopPoolBlock(slot, pool);
    if (blk == NULL) {
        return MP4_ERR_INTERNAL;
    }

    read_cnt = req_entries;
    if (stream->base_unit < read_cnt) {
        read_cnt = stream->base_unit;
    }

    mp4ReadU32Array(blk->words, 12U * read_cnt, stream);
    if ((stream->error_flags & MP4_PARSE_FLAG_FATAL_MASK) != 0U) {
        mp4CacheReturnSlotBlock(slot, pool);
        return MP4_ERR_IO;
    }

    blk->count = read_cnt;
    words = blk->words;
    next_word = words + 3;
    cur_chunk = words[0];
    cur_spc = words[1];

    while (idx < blk->count) {
        uint32_t run_chunks;

        if (idx + 1U == blk->count) {
            if (is_fwd != 1) {
                run_chunks = chunk_ref - cur_chunk + 2U;
                resolved = 1U;
            } else if (is_last == 0) {
                next_chunk = mp4ReadU32(stream);
                if ((int32_t)stream->error_flags < 0) {
                    mp4CacheReturnSlotBlock(slot, pool);
                    return MP4_ERR_IO;
                }
                run_chunks = next_chunk - cur_chunk;
                resolved = 1U;
            } else if (total_chunks != 0U) {
                run_chunks = total_chunks - cur_chunk + 1U;
                resolved = 1U;
            } else {
                break;
            }
        } else {
            next_chunk = next_word[0];
            next_spc = next_word[1];
            next_word += 3;
            run_chunks = next_chunk - cur_chunk;
        }

        *out_samples += run_chunks * cur_spc;
        *out_chunks += run_chunks;
        cur_spc = next_spc;
        cur_chunk = next_chunk;
        idx++;
    }

    if (resolved == 0U) {
        *out_entries = blk->count - 1U;
    } else {
        *out_entries = blk->count;
    }

    if (is_fwd == 1) {
        blk->first_chunk = chunk_ref;
        blk->first_sample = sample_ref;
    } else {
        blk->first_sample = sample_ref - *out_samples + 1U;
        blk->first_chunk = chunk_ref - *out_chunks + 1U;
    }

    if (out_resolved != NULL) {
        *out_resolved = resolved;
    }
    return MP4_OK;
}

/* 0x0169cc */
MP4_NOINLINE int32_t mp4CacheReadStszBlock(
    Mp4CacheBlock **slot,
    uint32_t first_sample,
    uint32_t req_entries,
    uint32_t *out_entries,
    Mp4CacheBlock **pool,
    Mp4IoStream *stream)
{
    Mp4CacheBlock *blk;
    uint32_t read_cnt = req_entries;

    blk = mp4CachePopPoolBlock(slot, pool);
    if (blk == NULL) {
        return MP4_ERR_INTERNAL;
    }

    if (stream->base_unit < read_cnt) {
        read_cnt = stream->base_unit;
    }

    mp4ReadU32Array(blk->words, 4U * read_cnt, stream);
    if ((stream->error_flags & MP4_PARSE_FLAG_FATAL_MASK) != 0U) {
        mp4CacheReturnSlotBlock(slot, pool);
        return MP4_ERR_IO;
    }

    blk->count = read_cnt;
    *out_entries = (*slot)->count;
    (*slot)->first_sample = first_sample;
    return MP4_OK;
}

/* 0x016adc */
MP4_NOINLINE int32_t mp4CacheReadStcoBlock(
    Mp4CacheBlock **slot,
    uint32_t first_chunk,
    uint32_t req_entries,
    uint32_t *out_entries,
    Mp4CacheBlock **pool,
    Mp4IoStream *stream)
{
    Mp4CacheBlock *blk;
    uint32_t read_cnt = req_entries;

    blk = mp4CachePopPoolBlock(slot, pool);
    if (blk == NULL) {
        return MP4_ERR_INTERNAL;
    }

    if (stream->base_unit < read_cnt) {
        read_cnt = stream->base_unit;
    }

    mp4ReadU32Array(blk->words, 4U * read_cnt, stream);
    if ((stream->error_flags & MP4_PARSE_FLAG_FATAL_MASK) != 0U) {
        mp4CacheReturnSlotBlock(slot, pool);
        return MP4_ERR_IO;
    }

    blk->count = read_cnt;
    *out_entries = (*slot)->count;
    (*slot)->first_chunk = first_chunk;
    return MP4_OK;
}

/* 0x016bec */
MP4_NOINLINE int32_t mp4CacheReadStssBlock(
    Mp4CacheBlock **slot,
    uint32_t req_entries,
    int32_t is_first,
    int32_t is_last,
    uint32_t total_samples,
    uint32_t *out_resolved,
    uint32_t *out_samples,
    uint32_t *out_entries,
    Mp4CacheBlock **pool,
    Mp4IoStream *stream)
{
    Mp4CacheBlock *blk;
    uint32_t read_cnt;
    uint32_t *words;
    uint32_t resolved = 1U;

    *out_samples = 0U;
    *out_entries = 0U;

    blk = mp4CachePopPoolBlock(slot, pool);
    if (blk == NULL) {
        return MP4_ERR_INTERNAL;
    }

    read_cnt = req_entries;
    if (stream->base_unit < read_cnt) {
        read_cnt = stream->base_unit;
    }

    mp4ReadU32Array(blk->words, 4U * read_cnt, stream);
    if ((stream->error_flags & MP4_PARSE_FLAG_FATAL_MASK) != 0U) {
        mp4CacheReturnSlotBlock(slot, pool);
        return MP4_ERR_IO;
    }

    blk->count = read_cnt;
    blk = *slot;
    words = blk->words;
    if (is_first == 1) {
        blk->first_sample = 0U;
    } else {
        blk->first_sample = words[0] - 1U;
    }

    if (is_last == 1) {
        if (total_samples == 0U) {
            resolved = 0U;
            *out_samples = words[blk->count - 1U] - blk->first_sample;
        } else {
            *out_samples = total_samples - blk->first_sample;
        }
    } else {
        uint32_t next_sync = mp4ReadU32(stream);
        if ((int32_t)stream->error_flags < 0) {
            mp4CacheReturnSlotBlock(slot, pool);
            return MP4_ERR_IO;
        }
        blk = *slot;
        *out_samples = next_sync - blk->first_sample - 1U;
    }

    *out_entries = blk->count;
    if (out_resolved != NULL) {
        *out_resolved = resolved;
    }
    return MP4_OK;
}

/* 0x016dc0 */
MP4_NOINLINE int32_t mp4CacheReadCttsBlock(
    Mp4CacheBlock **slot,
    uint32_t req_entries,
    int32_t is_fwd,
    uint32_t sample_ref,
    uint32_t *out_samples,
    uint32_t *out_entries,
    Mp4CacheBlock **pool,
    Mp4IoStream *stream)
{
    Mp4CacheBlock *blk;
    uint32_t read_cnt;
    uint32_t idx;
    uint32_t *cursor;

    *out_samples = 0U;
    *out_entries = 0U;

    blk = mp4CachePopPoolBlock(slot, pool);
    if (blk == NULL) {
        return MP4_ERR_INTERNAL;
    }

    read_cnt = req_entries;
    if (stream->base_unit < read_cnt) {
        read_cnt = stream->base_unit;
    }

    mp4ReadU32Array(blk->words, 8U * read_cnt, stream);
    if ((stream->error_flags & MP4_PARSE_FLAG_FATAL_MASK) != 0U) {
        mp4CacheReturnSlotBlock(slot, pool);
        return MP4_ERR_IO;
    }

    blk->count = read_cnt;
    cursor = blk->words;
    for (idx = 0U; idx < blk->count; idx++, cursor += 2) {
        *out_samples += cursor[0];
    }
    *out_entries = blk->count;

    if (is_fwd == 1) {
        blk->first_sample = sample_ref;
    } else {
        blk->first_sample = sample_ref - *out_samples + 1U;
    }
    return MP4_OK;
}

/* 0x016f38 */
MP4_NOINLINE int32_t mp4CacheTrimBlocks(
    Mp4CacheBlock **active_ptr,
    uint32_t req_start,
    uint32_t req_count,
    uint32_t end_sample,
    uint32_t end_time,
    uint32_t end_chunk,
    int32_t is_fwd,
    uint32_t *out_samples,
    uint32_t *out_dur,
    uint32_t *out_chunks,
    uint32_t *out_entries,
    Mp4CacheBlock **pool)
{
    uint32_t trimmed_samples = 0U;
    uint32_t trimmed_dur = 0U;
    uint32_t trimmed_chunks = 0U;
    uint32_t trimmed_entries = 0U;
    Mp4CacheBlock *cur;
    Mp4CacheBlock *kept_tail;

    if (*active_ptr == NULL) {
        return MP4_ERR_NULL_OBJ;
    }

    cur = *active_ptr;
    kept_tail = cur;

    if (is_fwd == 1) {
        while (cur->next != NULL) {
            Mp4CacheBlock *evict_blk = cur;
            uint32_t next_first_sample = cur->next->first_sample;

            if (next_first_sample - 1U >= req_start) {
                break;
            }
            cur = cur->next;
            evict_blk->next = *pool;
            *pool = evict_blk;
            trimmed_samples += next_first_sample - evict_blk->first_sample;
            trimmed_entries += evict_blk->count;
            trimmed_dur += cur->first_time - evict_blk->first_time;
            trimmed_chunks += cur->first_chunk - evict_blk->first_chunk;
        }
        *active_ptr = cur;
    } else {
        Mp4CacheBlock *scan = cur->next;
        uint32_t req_end = req_start + req_count - 1U;

        while (scan != NULL) {
            Mp4CacheBlock *evict_blk = scan;
            uint32_t lim_sample = end_sample;
            uint32_t lim_time = end_time;
            uint32_t lim_chunk = end_chunk;

            if (req_end < scan->first_sample) {
                scan = scan->next;
                if (scan != NULL) {
                    lim_sample = scan->first_sample - 1U;
                    lim_time = scan->first_time - 1U;
                    lim_chunk = scan->first_chunk - 1U;
                }
                evict_blk->next = *pool;
                *pool = evict_blk;
                trimmed_samples += lim_sample - evict_blk->first_sample + 1U;
                trimmed_dur += lim_time - evict_blk->first_time + 1U;
                trimmed_chunks += lim_chunk - evict_blk->first_chunk + 1U;
                trimmed_entries += evict_blk->count;
            } else {
                kept_tail = scan;
                scan = scan->next;
            }
        }
        kept_tail->next = NULL;
    }

    if (out_samples != NULL) {
        *out_samples = trimmed_samples;
    }
    if (out_dur != NULL) {
        *out_dur = trimmed_dur;
    }
    if (out_chunks != NULL) {
        *out_chunks = trimmed_chunks;
    }
    if (out_entries != NULL) {
        *out_entries = trimmed_entries;
    }
    return MP4_OK;
}

/* 0x01711c */
MP4_NOINLINE int32_t mp4CacheTrimStcoBlocks(
    Mp4CacheBlock **active_ptr,
    uint32_t req_start,
    uint32_t req_count,
    int32_t is_fwd,
    uint32_t *out_chunks,
    Mp4CacheBlock **pool)
{
    Mp4CacheBlock *cur = *active_ptr;
    Mp4CacheBlock *kept_tail;

    *out_chunks = 0U;
    if (cur == NULL) {
        return MP4_ERR_NULL_OBJ;
    }

    kept_tail = cur;
    if (is_fwd == 1) {
        while (cur->next != NULL) {
            Mp4CacheBlock *next_blk = cur->next;
            Mp4CacheBlock *evict_blk = cur;

            if (next_blk->first_chunk - 1U >= req_start) {
                break;
            }
            cur = next_blk;
            *out_chunks += next_blk->count;
            evict_blk->next = *pool;
            *pool = evict_blk;
        }
        *active_ptr = cur;
    } else {
        Mp4CacheBlock *scan = cur->next;
        uint32_t req_end = req_start + req_count - 1U;

        while (scan != NULL) {
            Mp4CacheBlock *evict_blk = scan;

            if (req_end < scan->first_chunk) {
                uint32_t cnt = scan->count;
                scan = scan->next;
                evict_blk->next = *pool;
                *out_chunks += cnt;
                *pool = evict_blk;
            } else {
                kept_tail = scan;
                scan = scan->next;
            }
        }
        kept_tail->next = NULL;
    }

    return MP4_OK;
}

/* 0x0171fc */
MP4_NOINLINE Mp4CacheBlock **mp4CacheFindTailSlot(Mp4CacheBlock **head_ptr)
{
    Mp4CacheBlock *cur;

    for (cur = *head_ptr; cur != NULL; cur = cur->next) {
        head_ptr = &cur->next;
    }
    return head_ptr;
}

/* 0x017220 */
MP4_NOINLINE Mp4CacheBlock **mp4CacheNextSlot(Mp4CacheBlock **slot)
{
    return &((*slot)->next);
}

/* 0x01722c */
MP4_NOINLINE int32_t mp4CacheSetNext(Mp4CacheBlock *blk, Mp4CacheBlock *next_blk)
{
    blk->next = next_blk;
    return MP4_OK;
}

/* 0x017238 */
MP4_NOINLINE int32_t mp4CacheGetSampleInfo(
    Mp4CacheStbl *cache,
    uint32_t sample_idx,
    int32_t want_chunk_rem,
    Mp4SampleInfo *out_info,
    int32_t allow_io)
{
    uint32_t total_samples;
    uint32_t chunk_idx;
    uint32_t sample_time;
    uint32_t chunk_first_sample;
    uint32_t chunk_sample_cnt;
    int32_t err;

    if (cache == NULL) {
        return MP4_ERR_CORRUPT;
    }
    if (cache->ctrl == NULL) {
        return MP4_ERR_NOT_READY;
    }
    if (!mp4CacheHasStts(&cache->stts) || !mp4CacheHasStsc(&cache->stsc) ||
        !mp4CacheHasStsz(&cache->stsz) || !mp4CacheHasStco(&cache->stco)) {
        return MP4_ERR_CORRUPT;
    }

    total_samples = cache->ctrl->total_samples;
    if (total_samples == 0U || sample_idx >= total_samples) {
        return MP4_ERR_SAMPLE_RANGE;
    }

    err = mp4CacheEnsureSampleWindow(cache, sample_idx, allow_io);
    if (err != MP4_OK) {
        return err;
    }

    err = mp4CacheGetSampleOffset(cache, sample_idx, &out_info->offset, &chunk_idx);
    if (err != MP4_OK) {
        return err;
    }

    err = mp4CacheStszLookup(&cache->stsz, sample_idx, &out_info->size);
    if (err != MP4_OK) {
        return err;
    }

    err = mp4CacheSttsLookupBySample(&cache->stts, sample_idx, &sample_time, &out_info->duration);
    if (err != MP4_OK) {
        return err;
    }

    err = mp4CacheCheckSyncSample(&cache->stss, sample_idx, &out_info->is_sync);
    if (err != MP4_OK) {
        return err;
    }

    err = mp4CacheGetCttsOffset(&cache->ctts, sample_idx, &out_info->ctts_offset);
    if (err != MP4_OK) {
        return err;
    }

    err = mp4CacheStscLookupByChunk(&cache->stsc, chunk_idx,
                                    &chunk_first_sample, &chunk_sample_cnt, &out_info->sample_desc);
    if (err != MP4_OK) {
        return err;
    }

    if (want_chunk_rem != 1) {
        return MP4_OK;
    }
    if (chunk_sample_cnt == 0U) {
        return MP4_ERR_CORRUPT;
    }

    {
        uint32_t chunk_last_sample = chunk_first_sample + chunk_sample_cnt - 1U;

        out_info->chunk_rem_bytes = 0U;
        err = mp4CacheSumSampleSizesSpan(&cache->stsz, sample_idx, chunk_last_sample,
                                         &out_info->chunk_rem_bytes, cache->ctrl->stream);
        if (err != MP4_OK) {
            return err;
        }

        out_info->chunk_rem_dur = 0U;
        err = mp4CacheSumDurationsSpan(&cache->stts, sample_idx, chunk_last_sample,
                                       &out_info->chunk_rem_dur, cache->ctrl->stream);
        if (err != MP4_OK) {
            return err;
        }

        out_info->chunk_rem_samples = chunk_last_sample - sample_idx + 1U;
    }
    return MP4_OK;
}

/* 0x01745c */
MP4_NOINLINE int32_t mp4CacheEnsureSampleWindow(
    Mp4CacheStbl *cache,
    uint32_t sample_idx,
    int32_t allow_io)
{
    uint32_t total_samples;
    uint32_t start_sample;
    uint32_t count;
    int32_t err;

    if (cache == NULL || cache->ctrl == NULL) {
        return MP4_ERR_NOT_READY;
    }

    total_samples = cache->ctrl->total_samples;
    if (total_samples == 0U || sample_idx >= total_samples) {
        return MP4_ERR_SAMPLE_RANGE;
    }

    if (cache->ctrl->win_sample_count != 0U && cache->ctrl->win_chunk_count != 0U &&
        cache->ctrl->win_time_count != 0U &&
        sample_idx >= cache->ctrl->win_sample_start &&
        cache->ctrl->win_sample_start + cache->ctrl->win_sample_count - 1U >= sample_idx) {
        return MP4_OK;
    }

    if (allow_io == 0) {
        return MP4_ERR_IO;
    }

    err = mp4AttachStream(cache->ctrl->stream);
    if (err != MP4_OK) {
        return err;
    }

    err = mp4CacheCalcLoadRange(sample_idx, cache->ctrl->win_sample_start,
                                cache->ctrl->win_sample_count,
                                cache->ctrl->total_samples, &start_sample, &count, cache->ctrl);
    if (err != MP4_OK) {
        return err;
    }

    return mp4CacheUpdateWindow(cache, start_sample, count, cache->ctrl->stream);
}

/* 0x01755c */
MP4_NOINLINE int32_t mp4CacheCalcLoadRange(
    uint32_t target,
    uint32_t cur_start,
    uint32_t cur_count,
    uint32_t total,
    uint32_t *out_start,
    uint32_t *out_count,
    Mp4CacheCtrl *ctrl)
{
    Mp4IoStream *stream = ctrl->stream;
    uint32_t cache_size = stream->cache_size;
    uint32_t start_pos;
    uint32_t span;

    if (cache_size == 0U) {
        return MP4_ERR_INVALID_PARAM;
    }

    if (cur_count != 0U && target == cur_start + cur_count) {
        uint32_t max_read = stream->max_read;
        if (max_read - cache_size >= cur_count) {
            *out_start = cur_start;
            if (total - cache_size >= target) {
                *out_count = cur_count + cache_size;
            } else {
                *out_count = total - cur_start;
            }
            return MP4_OK;
        }
        start_pos = target + cache_size - max_read;
        *out_start = start_pos;
        span = max_read;
        if (span >= total) {
            *out_count = total - start_pos;
            return MP4_OK;
        }
    } else {
        uint32_t base_unit = stream->base_unit;
        start_pos = (target / base_unit) * base_unit;
        *out_start = start_pos;
        span = cache_size;
        if (span >= total) {
            *out_start = 0U;
            *out_count = total;
            return MP4_OK;
        }
    }

    if (total - span >= start_pos) {
        *out_count = span;
    } else {
        *out_count = total - start_pos;
    }
    return MP4_OK;
}

/* 0x01762c */
MP4_NOINLINE int32_t mp4CacheCheckSyncSample(
    Mp4CacheStss *stss,
    uint32_t sample_idx,
    uint32_t *out_is_sync)
{
    uint32_t cur_sample;
    uint32_t win_entry_start;
    uint32_t win_entry_end;
    uint32_t raw_sync = 0U;
    int32_t err;

    if (stss->size == 0U) {
        *out_is_sync = 1U;
        return MP4_OK;
    }
    if (stss->entry_count == 0U) {
        *out_is_sync = 0U;
        return MP4_OK;
    }
    if (sample_idx < stss->win_sample_start ||
        sample_idx >= stss->win_sample_start + stss->win_sample_count) {
        return MP4_ERR_SAMPLE_RANGE;
    }

    win_entry_start = stss->win_entry_start;
    win_entry_end = win_entry_start + stss->win_entry_count;

    if (stss->cur_entry < win_entry_start || win_entry_end - 1U < stss->cur_entry) {
        stss->cur_entry = 0U;
        stss->cur_sample = 0U;
    }

    if (stss->cur_entry != 0U) {
        cur_sample = stss->cur_sample;
    } else {
        stss->cur_entry = win_entry_start;
        err = mp4CacheBlockReadWord(stss->active, win_entry_start, win_entry_start, 0U, &raw_sync);
        if (err != MP4_OK) {
            return err;
        }
        cur_sample = raw_sync - 1U;
        stss->cur_sample = cur_sample;
    }

    if (sample_idx < cur_sample) {
        if (stss->cur_entry != 0U &&
            sample_idx - stss->win_sample_start < cur_sample - sample_idx) {
            stss->cur_entry = win_entry_start;
            err = mp4CacheBlockReadWord(stss->active, win_entry_start, win_entry_start, 0U, &raw_sync);
            if (err != MP4_OK) {
                return err;
            }
            cur_sample = raw_sync - 1U;
            stss->cur_sample = cur_sample;
        }
    } else if (cur_sample < sample_idx) {
        uint32_t last_entry = win_entry_end - 1U;
        if (stss->cur_entry != last_entry &&
            stss->win_sample_start + stss->win_sample_count - sample_idx < sample_idx - cur_sample) {
            stss->cur_entry = last_entry;
            err = mp4CacheBlockReadWord(stss->active, win_entry_start, last_entry, 0U, &raw_sync);
            if (err != MP4_OK) {
                return err;
            }
            cur_sample = raw_sync - 1U;
            stss->cur_sample = cur_sample;
        }
    }

    if (sample_idx < cur_sample) {
        uint32_t scan_idx;

        if (stss->cur_entry == 0U) {
            *out_is_sync = 0U;
            return MP4_OK;
        }
        scan_idx = stss->cur_entry - 1U;
        for (;;) {
            uint32_t sync_sample;

            err = mp4CacheBlockReadWord(stss->active, win_entry_start, scan_idx, 0U, &raw_sync);
            if (err != MP4_OK) {
                return err;
            }
            sync_sample = raw_sync - 1U;
            if (sync_sample == sample_idx) {
                *out_is_sync = 1U;
                stss->cur_sample = sample_idx;
                stss->cur_entry = scan_idx;
                return MP4_OK;
            }
            if (sync_sample < sample_idx) {
                *out_is_sync = 0U;
                return MP4_OK;
            }
            stss->cur_entry = scan_idx;
            stss->cur_sample = sync_sample;
            if (scan_idx == 0U) {
                *out_is_sync = 0U;
                return MP4_OK;
            }
            scan_idx--;
        }
    }

    if (cur_sample == sample_idx) {
        *out_is_sync = 1U;
        return MP4_OK;
    }

    {
        uint32_t scan_idx = stss->cur_entry + 1U;

        while (scan_idx < win_entry_end) {
            uint32_t sync_sample;

            err = mp4CacheBlockReadWord(stss->active, win_entry_start, scan_idx, 0U, &raw_sync);
            if (err != MP4_OK) {
                return err;
            }
            sync_sample = raw_sync - 1U;
            if (sync_sample == sample_idx) {
                *out_is_sync = 1U;
                stss->cur_sample = sync_sample;
                stss->cur_entry = scan_idx;
                return MP4_OK;
            }
            if (sample_idx < sync_sample) {
                *out_is_sync = 0U;
                return MP4_OK;
            }
            stss->cur_entry = scan_idx;
            stss->cur_sample = sync_sample;
            scan_idx++;
        }
    }

    *out_is_sync = 0U;
    return MP4_OK;
}

/* 0x017b64 */
MP4_NOINLINE int32_t mp4CacheFindSampleByTime(
    Mp4CacheStbl *cache,
    uint32_t time_val,
    uint32_t *out_sample,
    uint32_t *out_is_sync,
    uint32_t *out_rem,
    int32_t allow_io)
{
    int32_t err;

    if (cache == NULL) {
        return MP4_ERR_CORRUPT;
    }
    if (cache->ctrl == NULL) {
        return MP4_ERR_NOT_READY;
    }
    if (!mp4CacheHasStts(&cache->stts)) {
        return MP4_ERR_CORRUPT;
    }
    if (time_val >= cache->ctrl->total_duration) {
        return MP4_ERR_TIME_RANGE;
    }

    err = mp4CacheLookupTime(cache, time_val, out_sample, out_rem, allow_io);
    if (err != MP4_OK) {
        return err;
    }

    {
        uint32_t found_sample = *out_sample;
        err = mp4CacheEnsureStssSample(cache, found_sample, allow_io);
        if (err != MP4_OK) {
            return err;
        }

        return mp4CacheCheckSyncSample(&cache->stss, found_sample, out_is_sync);
    }
}

/* 0x017c5c */
MP4_NOINLINE int32_t mp4CacheLookupTime(
    Mp4CacheStbl *cache,
    uint32_t time_val,
    uint32_t *out_sample,
    uint32_t *out_rem,
    int32_t allow_io)
{
    int32_t err;

    err = mp4CacheEnsureTimeWindow(cache, time_val, allow_io);
    if (err == MP4_OK) {
        return mp4CacheSttsLookupByTime(&cache->stts, time_val, out_sample, out_rem);
    }
    return err;
}

/* 0x017cd8 */
MP4_NOINLINE int32_t mp4CacheEnsureTimeWindow(
    Mp4CacheStbl *cache,
    uint32_t time_val,
    int32_t allow_io)
{
    Mp4CacheStts *stts = &cache->stts;
    Mp4CacheCtrl *ctrl = cache->ctrl;
    uint32_t win_s_start = 0U;
    uint32_t win_s_cnt = 0U;
    uint32_t win_t_start = 0U;
    uint32_t win_t_cnt = 0U;
    int32_t plan = 0;
    int32_t evict = 0;
    uint32_t probe_sample = 0U;
    int32_t err;

    if (!mp4CacheHasStts(stts)) {
        return MP4_ERR_CORRUPT;
    }
    if (time_val >= ctrl->total_duration) {
        return MP4_ERR_TIME_RANGE;
    }

    mp4CacheSttsGetWindow(stts, &win_s_start, &win_s_cnt, &win_t_start, &win_t_cnt);
    if (win_s_cnt != 0U && win_t_cnt != 0U &&
        time_val >= win_t_start && win_t_start + win_t_cnt - 1U >= time_val) {
        return MP4_OK;
    }

    if (allow_io == 0) {
        return MP4_ERR_IO;
    }

    err = mp4AttachStream(ctrl->stream);
    if (err != MP4_OK) {
        return err;
    }

    if (win_t_cnt != 0U) {
        if (mp4CachePlanWindow(time_val, 1U, win_t_start, win_t_cnt, ctrl->total_duration,
                               &plan, &evict) != MP4_OK) {
            return MP4_ERR_INTERNAL;
        }
    } else {
        if (ctrl->total_duration - time_val < time_val) {
            plan = MP4_PLAN_BWD_RELOAD;
        } else {
            plan = MP4_PLAN_FWD_RELOAD;
        }
        evict = 0;
    }

    if (plan == MP4_PLAN_NONE) {
        return MP4_OK;
    }

    if (plan == MP4_PLAN_FWD_EXTEND) {
        probe_sample = win_s_start + win_s_cnt;
    } else if (plan == MP4_PLAN_FWD_RELOAD) {
        probe_sample = 0U;
    } else if (plan == MP4_PLAN_BWD_EXTEND || plan == MP4_PLAN_SUPERSET) {
        uint32_t cache_sz = ctrl->stream->cache_size;
        if (cache_sz >= win_s_start) {
            probe_sample = 0U;
            plan = MP4_PLAN_FWD_RELOAD;
        } else {
            probe_sample = win_s_start - cache_sz;
        }
    } else if (plan == MP4_PLAN_BWD_RELOAD) {
        if (ctrl->stream->cache_size < ctrl->total_samples) {
            probe_sample = ctrl->total_samples - 1U;
        } else {
            probe_sample = 0U;
            plan = MP4_PLAN_FWD_RELOAD;
        }
    } else {
        probe_sample = 0U;
        plan = MP4_PLAN_FWD_RELOAD;
    }

    for (;;) {
        uint32_t prev_s_start = win_s_start;
        uint32_t prev_s_cnt = win_s_cnt;

        err = mp4CacheEnsureSttsSample(cache, probe_sample, allow_io);
        if (err != MP4_OK) {
            break;
        }
        mp4CacheSttsGetWindow(stts, &win_s_start, &win_s_cnt, &win_t_start, &win_t_cnt);
        if (time_val >= win_t_start && win_t_start + win_t_cnt - 1U >= time_val) {
            break;
        }
        if ((plan & MP4_PLAN_FWD_MASK) != 0) {
            probe_sample = win_s_start + win_s_cnt;
            if (probe_sample >= ctrl->total_samples ||
                (prev_s_start == win_s_start && prev_s_cnt == win_s_cnt)) {
                err = MP4_ERR_TIME_RANGE;
                break;
            }
        } else {
            if (win_s_start == 0U) {
                err = MP4_ERR_TIME_RANGE;
                break;
            }
            probe_sample = win_s_start - 1U;
        }
    }

    mp4CacheRecalcBounds(cache);
    return err;
}

/* 0x017fc4 */
MP4_NOINLINE int32_t mp4CacheSttsLookupByTime(
    Mp4CacheStts *stts,
    uint32_t time_val,
    uint32_t *out_sample,
    uint32_t *out_rem)
{
    uint32_t win_time_start;
    uint32_t win_time_count;
    uint32_t win_entry_start;
    uint32_t win_entry_count;
    uint32_t cur_entry;
    uint32_t cur_time;
    int32_t err;

    if (stts->size == 0U) {
        return MP4_ERR_CORRUPT;
    }
    win_time_start = stts->win_time_start;
    win_time_count = stts->win_time_count;
    if (time_val < win_time_start || win_time_start + win_time_count - 1U < time_val) {
        return MP4_ERR_TIME_RANGE;
    }

    win_entry_start = stts->win_entry_start;
    win_entry_count = stts->win_entry_count;
    cur_entry = stts->cur_entry;

    if (cur_entry < win_entry_start || win_entry_start + win_entry_count - 1U < cur_entry) {
        stts->cur_sample = 0U;
        stts->cur_time = 0U;
        stts->cur_entry = 0U;
        cur_entry = 0U;
    }

    if (cur_entry == 0U) {
        stts->cur_time = win_time_start;
        stts->cur_sample = stts->win_sample_start;
        stts->cur_entry = win_entry_start;
    }

    cur_time = stts->cur_time;
    if (time_val < cur_time) {
        if (time_val - win_time_start < cur_time - time_val) {
            cur_time = win_time_start;
            cur_entry = win_entry_start;
            stts->cur_sample = stts->win_sample_start;
            stts->cur_time = win_time_start;
            stts->cur_entry = win_entry_start;
        } else {
            cur_entry = stts->cur_entry;
        }
    } else {
        cur_entry = stts->cur_entry;
        if (cur_time < time_val) {
            uint32_t last_entry = win_entry_start + win_entry_count - 1U;
            if (cur_entry != last_entry &&
                win_time_start + win_time_count - time_val < time_val - cur_time) {
                uint32_t run_samples = 0U;
                uint32_t run_delta = 0U;

                stts->cur_entry = last_entry;
                err = mp4CacheBlockReadWord(stts->active, win_entry_start, last_entry, 0U, &run_samples);
                if (err != MP4_OK) {
                    return err;
                }
                err = mp4CacheBlockReadWord(stts->active, win_entry_start, last_entry, 1U, &run_delta);
                if (err != MP4_OK) {
                    return err;
                }
                cur_entry = stts->cur_entry;
                stts->cur_sample = stts->win_sample_start + stts->win_sample_count - run_samples;
                cur_time = win_time_start + win_time_count - run_samples * run_delta;
                stts->cur_time = cur_time;
            }
        }
    }

    if (time_val < cur_time) {
        uint32_t scan_idx = cur_entry - 1U;
        uint32_t acc_samples = 0U;
        uint32_t acc_time = cur_time;

        if (scan_idx >= stts->entry_count) {
            return MP4_ERR_CORRUPT;
        }

        for (;;) {
            uint32_t run_samples = 0U;
            uint32_t run_delta = 0U;

            err = mp4CacheBlockReadWord(stts->active, win_entry_start, scan_idx, 0U, &run_samples);
            if (err != MP4_OK) {
                return err;
            }
            err = mp4CacheBlockReadWord(stts->active, win_entry_start, scan_idx, 1U, &run_delta);
            if (err != MP4_OK) {
                return err;
            }
            acc_samples += run_samples;
            acc_time -= run_samples * run_delta;
            if (time_val >= acc_time) {
                uint32_t step = (time_val - acc_time) / run_delta;
                *out_sample = stts->cur_sample - acc_samples + step;
                *out_rem = time_val - (acc_time + step * run_delta);
                stts->cur_time = acc_time;
                stts->cur_entry = scan_idx;
                stts->cur_sample -= acc_samples;
                return MP4_OK;
            }
            if (scan_idx == 0U) {
                return MP4_ERR_CORRUPT;
            }
            scan_idx--;
        }
    }

    if (cur_entry >= stts->entry_count) {
        return MP4_ERR_CORRUPT;
    }

    {
        uint32_t acc_samples = 0U;
        uint32_t acc_time = cur_time;
        uint32_t scan_idx = cur_entry;
        uint32_t win_entry_end = win_entry_start + win_entry_count;

        while (scan_idx < win_entry_end) {
            uint32_t run_samples = 0U;
            uint32_t run_delta = 0U;

            err = mp4CacheBlockReadWord(stts->active, win_entry_start, scan_idx, 0U, &run_samples);
            if (err != MP4_OK) {
                return err;
            }
            err = mp4CacheBlockReadWord(stts->active, win_entry_start, scan_idx, 1U, &run_delta);
            if (err != MP4_OK) {
                return err;
            }
            if (time_val < acc_time + run_samples * run_delta) {
                uint32_t step = (time_val - acc_time) / run_delta;
                *out_sample = stts->cur_sample + acc_samples + step;
                *out_rem = time_val - (acc_time + step * run_delta);
                stts->cur_time = acc_time;
                stts->cur_entry = scan_idx;
                stts->cur_sample += acc_samples;
                return MP4_OK;
            }
            acc_time += run_samples * run_delta;
            acc_samples += run_samples;
            scan_idx++;
        }
    }

    return MP4_ERR_CORRUPT;
}

/* 0x018688 */
MP4_NOINLINE int32_t mp4CacheStscLookupBySample(
    Mp4CacheStsc *stsc,
    uint32_t sample_idx,
    uint32_t *out_chunk,
    uint32_t *out_chunk_first_sample,
    uint32_t *out_desc_idx)
{
    uint32_t win_sample_start;
    uint32_t win_sample_count;
    uint32_t win_entry_start;
    uint32_t win_entry_count;
    uint32_t cur_entry;
    uint32_t cur_sample;
    uint32_t found_entry = 0U;
    uint32_t found_sample = 0U;
    uint32_t found_chunk_base = 0U;
    uint32_t found_spc = 0U;
    uint32_t desc_val = 0U;
    int32_t err;

    win_sample_start = stsc->win_sample_start;
    win_sample_count = stsc->win_sample_count;
    if (sample_idx < win_sample_start || win_sample_start + win_sample_count - 1U < sample_idx) {
        return MP4_ERR_SAMPLE_RANGE;
    }
    if (stsc->size == 0U) {
        return MP4_ERR_CORRUPT;
    }

    win_entry_start = stsc->win_entry_start;
    win_entry_count = stsc->win_entry_count;
    cur_entry = stsc->cur_entry;

    if (cur_entry < win_entry_start || win_entry_start + win_entry_count - 1U < cur_entry) {
        stsc->cur_entry = 0U;
        stsc->cur_sample = 0U;
        cur_entry = 0U;
    }

    if (cur_entry == 0U) {
        stsc->cur_entry = win_entry_start;
        stsc->cur_sample = win_sample_start;
    }

    cur_sample = stsc->cur_sample;
    if (sample_idx < cur_sample) {
        if (sample_idx - win_sample_start < cur_sample - sample_idx) {
            cur_sample = win_sample_start;
            cur_entry = win_entry_start;
            stsc->cur_sample = win_sample_start;
            stsc->cur_entry = win_entry_start;
        } else {
            cur_entry = stsc->cur_entry;
        }
    } else {
        cur_entry = stsc->cur_entry;
        if (win_sample_start + win_sample_count - sample_idx < sample_idx - cur_sample) {
            uint32_t last_entry = win_entry_start + win_entry_count - 1U;
            if (cur_entry != last_entry) {
                uint32_t first_chunk = 0U;
                uint32_t spc = 0U;

                stsc->cur_entry = last_entry;
                err = mp4CacheBlockReadWord(stsc->active, win_entry_start, last_entry, 0U, &first_chunk);
                if (err != MP4_OK) {
                    return err;
                }
                err = mp4CacheBlockReadWord(stsc->active, win_entry_start, last_entry, 1U, &spc);
                if (err != MP4_OK) {
                    return err;
                }
                cur_entry = stsc->cur_entry;
                cur_sample = win_sample_start + win_sample_count -
                             (stsc->win_chunk_start + stsc->win_chunk_count + 1U - first_chunk) * spc;
                stsc->cur_sample = cur_sample;
            }
        }
    }

    if (sample_idx < cur_sample) {
        uint32_t next_first_chunk = 0U;
        uint32_t scan_idx = cur_entry - 1U;
        uint32_t acc_sample = cur_sample;

        err = mp4CacheBlockReadWord(stsc->active, win_entry_start, cur_entry, 0U, &next_first_chunk);
        if (err != MP4_OK) {
            return err;
        }

        for (;;) {
            uint32_t first_chunk = 0U;
            uint32_t spc = 0U;

            err = mp4CacheBlockReadWord(stsc->active, win_entry_start, scan_idx, 0U, &first_chunk);
            if (err != MP4_OK) {
                return err;
            }
            err = mp4CacheBlockReadWord(stsc->active, win_entry_start, scan_idx, 1U, &spc);
            if (err != MP4_OK) {
                return err;
            }
            acc_sample -= (next_first_chunk - first_chunk) * spc;
            if (sample_idx >= acc_sample) {
                found_entry = scan_idx;
                found_sample = acc_sample;
                found_chunk_base = first_chunk - 1U;
                found_spc = spc;
                break;
            }
            if (scan_idx == 0U) {
                return MP4_ERR_CORRUPT;
            }
            next_first_chunk = first_chunk;
            scan_idx--;
        }
    } else {
        uint32_t cur_first_chunk = 0U;
        uint32_t scan_idx = cur_entry;
        uint32_t acc_sample = cur_sample;
        uint32_t win_entry_end = win_entry_start + win_entry_count;

        err = mp4CacheBlockReadWord(stsc->active, win_entry_start, scan_idx, 0U, &cur_first_chunk);
        if (err != MP4_OK) {
            return err;
        }

        while (scan_idx < win_entry_end) {
            uint32_t next_first_chunk = 0U;
            uint32_t spc = 0U;
            uint32_t span_samples;

            if (scan_idx == win_entry_end - 1U) {
                next_first_chunk = stsc->win_chunk_start + stsc->win_chunk_count + 1U;
            } else {
                err = mp4CacheBlockReadWord(stsc->active, win_entry_start, scan_idx + 1U, 0U, &next_first_chunk);
                if (err != MP4_OK) {
                    return err;
                }
            }
            err = mp4CacheBlockReadWord(stsc->active, win_entry_start, scan_idx, 1U, &spc);
            if (err != MP4_OK) {
                return err;
            }
            span_samples = (next_first_chunk - cur_first_chunk) * spc;
            if (sample_idx < acc_sample + span_samples) {
                found_entry = scan_idx;
                found_sample = acc_sample;
                found_chunk_base = cur_first_chunk - 1U;
                found_spc = spc;
                break;
            }
            cur_first_chunk = next_first_chunk;
            acc_sample += span_samples;
            scan_idx++;
        }

        if (scan_idx >= win_entry_end) {
            return MP4_ERR_CORRUPT;
        }
    }

    {
        uint32_t chunk_off = (sample_idx - found_sample) / found_spc;
        *out_chunk = found_chunk_base + chunk_off;
        *out_chunk_first_sample = found_sample + chunk_off * found_spc;
    }

    err = mp4CacheBlockReadWord(stsc->active, win_entry_start, found_entry, 2U, &desc_val);
    if (err != MP4_OK) {
        return err;
    }
    *out_desc_idx = desc_val;
    stsc->cur_sample = found_sample;
    stsc->cur_entry = found_entry;
    return MP4_OK;
}

/* 0x018ec0 */
MP4_NOINLINE int32_t mp4CacheUpdateWindow(
    Mp4CacheStbl *cache,
    uint32_t start_sample,
    uint32_t count,
    Mp4IoStream *stream)
{
    Mp4CacheCtrl *ctrl;
    uint32_t total_samples;
    uint32_t base_unit;
    uint32_t aligned_start;
    uint32_t aligned_end_excl;
    uint32_t span_count;
    uint32_t span_end;
    uint32_t max_read;
    uint32_t chunk_idx = 0U;
    uint32_t chunk_first_sample = 0U;
    uint32_t desc_idx = 0U;
    int32_t err;

    if (cache == NULL) {
        return MP4_ERR_CORRUPT;
    }
    if (cache->ctrl == NULL) {
        return MP4_ERR_NOT_READY;
    }
    if (count == 0U) {
        return MP4_ERR_INVALID_PARAM;
    }
    if (!mp4CacheHasStts(&cache->stts) || !mp4CacheHasStsc(&cache->stsc) ||
        !mp4CacheHasStsz(&cache->stsz) || !mp4CacheHasStco(&cache->stco)) {
        return MP4_ERR_CORRUPT;
    }

    ctrl = cache->ctrl;
    total_samples = ctrl->total_samples;
    if (total_samples == 0U || start_sample >= total_samples) {
        return MP4_ERR_SAMPLE_RANGE;
    }

    base_unit = ctrl->stream->base_unit;
    aligned_end_excl = ((start_sample + count - 1U) / base_unit) * base_unit + base_unit;
    aligned_start = (start_sample / base_unit) * base_unit;
    span_count = aligned_end_excl - aligned_start;
    span_end = aligned_end_excl - 1U;

    if (total_samples - aligned_start < span_count) {
        span_count = total_samples - aligned_start;
        span_end = total_samples - 1U;
    }

    max_read = ctrl->stream->max_read;
    if (max_read < span_count) {
        span_count = max_read;
        span_end = aligned_start + max_read - 1U;
    }

    if (ctrl->win_sample_count != 0U && ctrl->win_chunk_count != 0U &&
        aligned_start >= ctrl->win_sample_start) {
        uint32_t cur_end = ctrl->win_sample_start + ctrl->win_sample_count - 1U;
        if (cur_end >= aligned_start && span_end >= ctrl->win_sample_start && cur_end >= span_end) {
            return MP4_OK;
        }
    }

    err = mp4CacheLoadStts(&cache->stts, aligned_start, span_count,
                           ctrl->total_samples, ctrl->total_duration, stream);
    if (err == MP4_OK) {
        err = mp4CacheLoadStsc(&cache->stsc, aligned_start, span_count,
                               ctrl->total_samples, ctrl->total_chunks, stream);
        if (err == MP4_OK) {
            err = mp4CacheStscLookupBySample(&cache->stsc, aligned_start,
                                             &chunk_idx, &chunk_first_sample, &desc_idx);
            if (err == MP4_OK) {
                err = mp4CacheLoadStsz(&cache->stsz, aligned_start, span_count,
                                       chunk_first_sample, &ctrl->chunk_prefix_bytes, stream);
                if (err == MP4_OK) {
                    err = mp4CacheLoadStcoForSamples(cache, aligned_start, span_count, stream);
                    if (err == MP4_OK) {
                        err = mp4CacheLoadStss(&cache->stss, aligned_start, span_count,
                                               ctrl->total_samples, stream);
                        if (err == MP4_OK) {
                            err = mp4CacheLoadCtts(&cache->ctts, aligned_start, span_count,
                                                   ctrl->total_samples, stream);
                        }
                    }
                }
            }
        }
    }

    mp4CacheRecalcBounds(cache);
    return err;
}

/* 0x019168 */
MP4_NOINLINE int32_t mp4CacheHasStts(const Mp4CacheStts *stts)
{
    return stts->size != 0U;
}

/* 0x019174 */
MP4_NOINLINE int32_t mp4CacheHasStsc(const Mp4CacheStsc *stsc)
{
    return stsc->size != 0U;
}

/* 0x019180 */
MP4_NOINLINE int32_t mp4CacheHasStsz(const Mp4CacheStsz *stsz)
{
    return stsz->size != 0U;
}

/* 0x01918c */
MP4_NOINLINE int32_t mp4CacheHasStco(const Mp4CacheStco *stco)
{
    return stco->size != 0U;
}

/* 0x019198 */
MP4_NOINLINE int32_t mp4CacheGetSampleCount(Mp4CacheStbl *cache, uint32_t *out_count)
{
    *out_count = 0U;
    if (cache == NULL) {
        return MP4_ERR_CORRUPT;
    }
    if (cache->ctrl == NULL) {
        return MP4_ERR_NOT_READY;
    }
    if (!mp4CacheHasStsz(&cache->stsz)) {
        return MP4_ERR_CORRUPT;
    }
    *out_count = mp4CacheStszCount(&cache->stsz);
    return MP4_OK;
}

/* 0x019208 */
MP4_NOINLINE uint32_t mp4CacheStszCount(const Mp4CacheStsz *stsz)
{
    return stsz->sample_count;
}

/* 0x019210 */
MP4_NOINLINE uint32_t mp4CacheStcoCount(const Mp4CacheStco *stco)
{
    return stco->entry_count;
}

/* 0x019218 */
MP4_NOINLINE void mp4CacheSttsGetWindow(
    const Mp4CacheStts *stts,
    uint32_t *out_s_start,
    uint32_t *out_s_cnt,
    uint32_t *out_t_start,
    uint32_t *out_t_cnt)
{
    if (out_s_start != NULL) {
        *out_s_start = stts->win_sample_start;
    }
    if (out_s_cnt != NULL) {
        *out_s_cnt = stts->win_sample_count;
    }
    if (out_t_start != NULL) {
        *out_t_start = stts->win_time_start;
    }
    if (out_t_cnt != NULL) {
        *out_t_cnt = stts->win_time_count;
    }
}

/* 0x019260 */
MP4_NOINLINE void mp4CacheStscGetWindow(
    const Mp4CacheStsc *stsc,
    uint32_t *out_s_start,
    uint32_t *out_s_cnt,
    uint32_t *out_c_start,
    uint32_t *out_c_cnt)
{
    if (out_s_start != NULL) {
        *out_s_start = stsc->win_sample_start;
    }
    if (out_s_cnt != NULL) {
        *out_s_cnt = stsc->win_sample_count;
    }
    if (out_c_start != NULL) {
        *out_c_start = stsc->win_chunk_start;
    }
    if (out_c_cnt != NULL) {
        *out_c_cnt = stsc->win_chunk_count;
    }
}

/* 0x0192a8 */
MP4_NOINLINE int32_t mp4CacheRecalcBounds(Mp4CacheStbl *cache)
{
    uint32_t stsc_s_start;
    uint32_t stsc_s_cnt;

    if (cache == NULL) {
        return MP4_ERR_CORRUPT;
    }
    if (cache->ctrl == NULL) {
        return MP4_ERR_NOT_READY;
    }

    mp4CacheSttsGetWindow(&cache->stts,
                          &cache->ctrl->win_sample_start, &cache->ctrl->win_sample_count,
                          &cache->ctrl->win_time_start, &cache->ctrl->win_time_count);
    mp4CacheStscGetWindow(&cache->stsc,
                          &stsc_s_start, &stsc_s_cnt,
                          &cache->ctrl->win_chunk_start, &cache->ctrl->win_chunk_count);

    mp4CacheIntersectRange(&cache->ctrl->win_sample_start, &cache->ctrl->win_sample_count,
                           stsc_s_start, stsc_s_cnt);
    mp4CacheIntersectRange(&cache->ctrl->win_sample_start, &cache->ctrl->win_sample_count,
                           cache->stsz.win_sample_start, cache->stsz.win_sample_count);
    mp4CacheIntersectRange(&cache->ctrl->win_chunk_start, &cache->ctrl->win_chunk_count,
                           cache->stco.win_chunk_start, cache->stco.win_chunk_count);

    if (cache->stss.size != 0U && cache->stss.entry_count != 0U) {
        mp4CacheIntersectRange(&cache->ctrl->win_sample_start, &cache->ctrl->win_sample_count,
                               cache->stss.win_sample_start, cache->stss.win_sample_count);
    }
    if (cache->ctts.size != 0U) {
        mp4CacheIntersectRange(&cache->ctrl->win_sample_start, &cache->ctrl->win_sample_count,
                               cache->ctts.win_sample_start, cache->ctts.win_sample_count);
    }

    return MP4_OK;
}

/* 0x0193ec */
MP4_NOINLINE int32_t mp4CacheIntersectRange(
    uint32_t *start_ptr,
    uint32_t *count_ptr,
    uint32_t other_start,
    uint32_t other_count)
{
    uint32_t cur_start = *start_ptr;
    uint32_t cur_count = *count_ptr;
    uint32_t cur_end;
    uint32_t other_end;

    if (cur_start == 0U && cur_count == 0U) {
        return MP4_OK;
    }
    if ((other_start | other_count) == 0U) {
        *start_ptr = 0U;
        *count_ptr = 0U;
        return MP4_OK;
    }
    if (cur_start == other_start && cur_count == other_count) {
        return MP4_OK;
    }

    cur_end = cur_start + cur_count - 1U;
    other_end = other_start + other_count - 1U;
    if (cur_end < other_start || other_end < cur_start) {
        *start_ptr = 0U;
        *count_ptr = 0U;
        return MP4_OK;
    }

    if (cur_start < other_start) {
        *start_ptr = other_start;
        cur_start = other_start;
    }
    if (other_end < cur_end) {
        cur_end = other_end;
    }
    *count_ptr = cur_end - cur_start + 1U;
    return MP4_OK;
}

/* 0x019480 */
MP4_NOINLINE void mp4CacheResetStts(Mp4CacheStts *stts)
{
    if (stts->active != NULL) {
        mp4CacheRecycleList(&stts->free_pool, &stts->active);
    }
    stts->win_entry_count = 0U;
    stts->win_sample_start = 0U;
    stts->win_sample_count = 0U;
    stts->win_time_start = 0U;
    stts->win_time_count = 0U;
    stts->win_entry_start = 0U;
}

/* 0x0194d8 */
MP4_NOINLINE void mp4CacheResetStsc(Mp4CacheStsc *stsc)
{
    if (stsc->active != NULL) {
        mp4CacheRecycleList(&stsc->free_pool, &stsc->active);
    }
    stsc->win_entry_count = 0U;
    stsc->win_sample_start = 0U;
    stsc->win_sample_count = 0U;
    stsc->win_chunk_start = 0U;
    stsc->win_chunk_count = 0U;
    stsc->win_entry_start = 0U;
}

/* 0x019530 */
MP4_NOINLINE void mp4CacheResetStsz(Mp4CacheStsz *stsz)
{
    if (stsz->active != NULL) {
        mp4CacheRecycleList(&stsz->free_pool, &stsz->active);
    }
    stsz->win_sample_count = 0U;
    stsz->win_sample_start = 0U;
}

/* 0x019578 */
MP4_NOINLINE void mp4CacheResetStco(Mp4CacheStco *stco)
{
    if (stco->active != NULL) {
        mp4CacheRecycleList(&stco->free_pool, &stco->active);
    }
    stco->win_chunk_count = 0U;
    stco->win_chunk_start = 0U;
}

/* 0x0195c0 */
MP4_NOINLINE void mp4CacheResetStss(Mp4CacheStss *stss)
{
    if (stss->active != NULL) {
        mp4CacheRecycleList(&stss->free_pool, &stss->active);
    }
    stss->win_entry_count = 0U;
    stss->win_sample_start = 0U;
    stss->win_sample_count = 0U;
    stss->win_entry_start = 0U;
}

/* 0x019610 */
MP4_NOINLINE void mp4CacheResetCtts(Mp4CacheCtts *ctts)
{
    if (ctts->active != NULL) {
        mp4CacheRecycleList(&ctts->free_pool, &ctts->active);
    }
    ctts->win_entry_count = 0U;
    ctts->win_sample_start = 0U;
    ctts->win_sample_count = 0U;
    ctts->win_entry_start = 0U;
}

/* 0x019660 */
MP4_NOINLINE void mp4CacheRecycleList(Mp4CacheBlock **pool_ptr, Mp4CacheBlock **active_ptr)
{
    Mp4CacheBlock *cur = *active_ptr;

    while (cur != NULL) {
        Mp4CacheBlock *next = cur->next;
        cur->next = *pool_ptr;
        *pool_ptr = *active_ptr;
        *active_ptr = next;
        cur = next;
    }
    *active_ptr = NULL;
}

/* 0x0196a0 */
MP4_NOINLINE int32_t mp4CacheFixupStscTail(Mp4CacheStsc *stsc, uint32_t last_chunk_idx)
{
    Mp4CacheBlock *blk = stsc->active;
    uint32_t add_chunks = 0U;
    uint32_t add_samples = 0U;
    int32_t err;

    if (blk == NULL) {
        return MP4_ERR_INTERNAL;
    }

    while (blk->next != NULL) {
        blk = blk->next;
    }

    err = mp4CacheCalcStscTail(blk, last_chunk_idx, &add_chunks, &add_samples);
    if (err != MP4_OK) {
        return err;
    }

    stsc->win_chunk_count += add_chunks;
    stsc->win_sample_count += add_samples;
    stsc->win_entry_count += 1U;
    stsc->tail_resolved = 1U;
    return MP4_OK;
}

/* 0x019738 */
MP4_NOINLINE int32_t mp4CacheSetStssSampleTotal(Mp4CacheStss *stss, uint32_t total_samples)
{
    stss->win_sample_count = total_samples;
    return MP4_OK;
}

/* 0x019744 */
MP4_NOINLINE int32_t mp4CacheCalcStscTail(
    Mp4CacheBlock *blk,
    uint32_t last_chunk_idx,
    uint32_t *out_chunks,
    uint32_t *out_samples)
{
    uint32_t cols;
    uint32_t last_row;
    uint32_t first_chunk;
    uint32_t spc;
    uint32_t chunk_span;

    if (blk == NULL) {
        return MP4_ERR_INTERNAL;
    }

    cols = blk->cols;
    last_row = blk->count - 1U;
    if (cols != 0U) {
        first_chunk = blk->words[last_row * cols];
    }
    if (cols >= 2U) {
        spc = blk->words[last_row * cols + 1U];
    }

    chunk_span = last_chunk_idx - first_chunk + 2U;
    *out_chunks = chunk_span;
    *out_samples = chunk_span * spc;
    return MP4_OK;
}

/* 0x0197e4 */
MP4_NOINLINE int32_t mp4CacheCalcPrefixBytes(
    Mp4CacheStsz *stsz,
    uint32_t chunk_first,
    uint32_t win_start,
    uint32_t prev_prefix,
    uint32_t *out_prefix)
{
    uint32_t sum_bytes;
    int32_t err;

    *out_prefix = 0U;
    if (chunk_first == win_start) {
        return MP4_OK;
    }

    if (win_start == stsz->win_sample_start) {
        *out_prefix = prev_prefix;
        return MP4_OK;
    }

    if (chunk_first < stsz->win_sample_start) {
        *out_prefix = prev_prefix;
        chunk_first = stsz->win_sample_start;
    }

    err = mp4CacheSumStszInWindow(stsz, chunk_first, win_start - 1U, &sum_bytes);
    if (err == MP4_OK) {
        *out_prefix += sum_bytes;
    }
    return err;
}

/* 0x01986c */
MP4_NOINLINE int32_t mp4CacheReadAndSumStsz(
    Mp4CacheStsz *stsz,
    uint32_t start_idx,
    uint32_t end_idx,
    uint32_t *out_sum,
    Mp4IoStream *stream)
{
    uint32_t rem;
    uint32_t cur_idx;
    uint64_t seek_pos;
    Mp4CacheBlock *blk = NULL;
    int32_t err = MP4_OK;

    *out_sum = 0U;
    if (stsz->sample_count - 1U < start_idx || stsz->sample_count - 1U < end_idx) {
        return MP4_ERR_CORRUPT;
    }

    if (stsz->const_sample_size != 0U) {
        *out_sum = stsz->const_sample_size * (end_idx - start_idx + 1U);
        return MP4_OK;
    }

    seek_pos = stsz->file_offset + (uint64_t)(4U * start_idx);
    if ((uint64_t)mp4CbTell(stream->file_handle) != seek_pos) {
        if (mp4SeekAbs(stream->file_handle, seek_pos) != MP4_OK) {
            return MP4_ERR_IO;
        }
    }

    rem = end_idx - start_idx + 1U;
    cur_idx = start_idx;
    while (rem != 0U) {
        uint32_t chunk_req = stream->base_unit;
        uint32_t got = 0U;
        uint32_t blk_sum = 0U;

        if (rem < chunk_req) {
            chunk_req = rem;
        }
        err = mp4CacheReadStszBlock(&blk, cur_idx, chunk_req, &got, &stsz->free_pool, stream);
        if (err != MP4_OK) {
            break;
        }
        if (mp4CacheSumBlockWords(blk, &blk_sum) != MP4_OK) {
            err = MP4_ERR_INTERNAL;
            break;
        }
        rem -= got;
        cur_idx += got;
        *out_sum += blk_sum;
    }

    if (blk != NULL) {
        blk->next = stsz->free_pool;
        stsz->free_pool = blk;
    }
    return err;
}

/* 0x019a1c */
MP4_NOINLINE int32_t mp4CacheSumBlockWords(Mp4CacheBlock *blk, uint32_t *out_sum)
{
    uint32_t idx;
    uint32_t *words = blk->words;

    *out_sum = 0U;
    for (idx = 0U; idx < blk->count; idx++) {
        *out_sum += words[idx];
    }
    return MP4_OK;
}

/* 0x019a5c */
MP4_NOINLINE int32_t mp4CacheIsStscTailResolved(const Mp4CacheStsc *stsc)
{
    if (stsc->size != 0U && stsc->active != NULL) {
        return (int32_t)stsc->tail_resolved;
    }
    return 1;
}

/* 0x019a7c */
MP4_NOINLINE int32_t mp4CacheIsStssTailResolved(const Mp4CacheStss *stss)
{
    if (stss->size != 0U && stss->active != NULL) {
        return (int32_t)stss->tail_resolved;
    }
    return 1;
}

/* 0x019a9c */
MP4_NOINLINE int32_t mp4CacheNeedsInitialStco(const Mp4CacheStco *stco)
{
    if (stco->file_offset != 0ULL) {
        return stco->win_chunk_count == 0U;
    }
    return 0;
}

/* 0x019ac0 */
MP4_NOINLINE int32_t mp4CacheGetSampleSize(
    Mp4CacheStbl *cache,
    uint32_t sample_idx,
    uint32_t *out_size,
    int32_t allow_io)
{
    int32_t err;

    if (cache == NULL) {
        return MP4_ERR_CORRUPT;
    }
    if (cache->ctrl == NULL) {
        return MP4_ERR_NOT_READY;
    }
    if (!mp4CacheHasStsz(&cache->stsz)) {
        return MP4_ERR_CORRUPT;
    }
    if (cache->ctrl->total_samples == 0U || sample_idx >= cache->ctrl->total_samples) {
        return MP4_ERR_SAMPLE_RANGE;
    }

    err = mp4CacheEnsureStszSample(cache, sample_idx, allow_io);
    if (err != MP4_OK) {
        return err;
    }
    return mp4CacheStszLookup(&cache->stsz, sample_idx, out_size);
}

/* 0x019b90 */
MP4_NOINLINE int32_t mp4CacheStszLookup(
    const Mp4CacheStsz *stsz,
    uint32_t sample_idx,
    uint32_t *out_size)
{
    if (stsz->const_sample_size != 0U) {
        *out_size = stsz->const_sample_size;
        return MP4_OK;
    }

    return mp4CacheBlockReadWord(stsz->active, stsz->win_sample_start, sample_idx, 0U, out_size);
}

/* 0x019c5c */
MP4_NOINLINE int32_t mp4CacheCalcBoxSizes(Mp4CacheStbl *cache, uint32_t *out_total)
{
    uint32_t child_size;
    int32_t err;

    if (cache == NULL) {
        return MP4_ERR_CORRUPT;
    }
    if (cache->ctrl == NULL) {
        return MP4_ERR_NOT_READY;
    }

    cache->size = 8U;

    err = mp4CacheCalcSttsSize(&cache->stts, &child_size);
    if (err != MP4_OK) {
        return err;
    }
    cache->size += child_size;

    err = mp4CacheCalcStscSize(&cache->stsc, &child_size);
    if (err != MP4_OK) {
        return err;
    }
    cache->size += child_size;

    err = mp4CacheCalcStszSize(&cache->stsz, &child_size);
    if (err != MP4_OK) {
        return err;
    }
    cache->size += child_size;

    err = mp4CacheCalcStcoSize(&cache->stco, &child_size);
    if (err != MP4_OK) {
        return err;
    }
    cache->size += child_size;

    err = mp4CacheCalcStssSize(&cache->stss, &child_size);
    if (err != MP4_OK) {
        return err;
    }
    cache->size += child_size;

    err = mp4CacheCalcCttsSize(&cache->ctts, &child_size);
    if (err != MP4_OK) {
        return err;
    }
    cache->size += child_size;
    *out_total = cache->size;
    return MP4_OK;
}

/* 0x019db8 */
MP4_NOINLINE int32_t mp4CacheCalcSttsSize(Mp4CacheStts *stts, uint32_t *out_size)
{
    uint32_t sz = stts->size;

    if (sz != 0U) {
        sz = 8U * stts->entry_count + 16U;
        stts->size = sz;
    }
    *out_size = sz;
    return MP4_OK;
}
