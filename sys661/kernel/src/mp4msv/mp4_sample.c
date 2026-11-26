#include "mp4msv_internal.h"

static __inline__ __attribute__((always_inline)) int32_t mp4SampleBlockReadWord(
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

static inline uint32_t mp4StssReadEntry(Mp4StssBox *stss, uint32_t index)
{
    uint32_t val = 0U;

    if (stss != NULL && mp4ChunkArrayGetU32(stss->entries, index, 0U, &val) == MP4_OK) {
        return val;
    }
    return 0xFFFFFFFFU;
}

/* 0x019ddc */
MP4_NOINLINE int32_t mp4CacheCalcStscSize(Mp4CacheStsc *stsc, uint32_t *out_size)
{
    uint32_t sz = stsc->size;

    if (sz != 0U) {
        sz = 12U * stsc->entry_count + 16U;
        stsc->size = sz;
    }
    *out_size = sz;
    return MP4_OK;
}

/* 0x019e0c */
MP4_NOINLINE int32_t mp4CacheCalcStszSize(Mp4CacheStsz *stsz, uint32_t *out_size)
{
    uint32_t sz = stsz->size;

    if (sz != 0U) {
        if (stsz->const_sample_size != 0U) {
            sz = 20U;
            stsz->size = 20U;
        } else {
            sz = 4U * stsz->sample_count + 20U;
            stsz->size = sz;
        }
    }
    *out_size = sz;
    return MP4_OK;
}

/* 0x019e50 */
MP4_NOINLINE int32_t mp4CacheCalcStcoSize(Mp4CacheStco *stco, uint32_t *out_size)
{
    uint32_t sz = stco->size;

    if (sz != 0U) {
        sz = 4U * stco->entry_count + 16U;
        stco->size = sz;
    }
    *out_size = sz;
    return MP4_OK;
}

/* 0x019e74 */
MP4_NOINLINE int32_t mp4CacheCalcStssSize(Mp4CacheStss *stss, uint32_t *out_size)
{
    uint32_t sz = stss->size;

    if (sz != 0U) {
        sz = 4U * stss->entry_count + 16U;
        stss->size = sz;
    }
    *out_size = sz;
    return MP4_OK;
}

/* 0x019e98 */
MP4_NOINLINE int32_t mp4CacheCalcCttsSize(Mp4CacheCtts *ctts, uint32_t *out_size)
{
    uint32_t sz = ctts->size;

    if (sz != 0U) {
        sz = 8U * ctts->entry_count + 16U;
        ctts->size = sz;
    }
    *out_size = sz;
    return MP4_OK;
}

/* 0x019ebc */
MP4_NOINLINE int32_t mp4CacheGetSampleOffset(
    Mp4CacheStbl *cache,
    uint32_t sample_idx,
    uint64_t *out_offset,
    uint32_t *out_chunk)
{
    Mp4CacheCtrl *ctrl = cache->ctrl;
    uint32_t chunk_idx = 0U;
    uint32_t chunk_first_sample = 0U;
    uint32_t desc_idx = 0U;
    uint32_t chunk_off = 0U;
    int32_t err;

    *out_offset = 0ULL;
    if (sample_idx < ctrl->win_sample_start ||
        ctrl->win_sample_start + ctrl->win_sample_count - 1U < sample_idx) {
        return MP4_ERR_SAMPLE_RANGE;
    }

    err = mp4CacheStscLookupBySample(&cache->stsc, sample_idx,
                                     &chunk_idx, &chunk_first_sample, &desc_idx);
    if (err != MP4_OK) {
        return err;
    }

    ctrl = cache->ctrl;
    if (chunk_idx < ctrl->win_chunk_start ||
        ctrl->win_chunk_start + ctrl->win_chunk_count - 1U < chunk_idx) {
        err = mp4CacheLoadStcoForSamples(cache, ctrl->win_sample_start, ctrl->win_sample_count,
                                         ctrl->stream);
        if (err != MP4_OK) {
            return err;
        }
    }

    err = mp4SampleBlockReadWord(cache->stco.active, cache->stco.win_chunk_start,
                                 chunk_idx, 0U, &chunk_off);
    if (err != MP4_OK) {
        return err;
    }

    *out_chunk = chunk_idx;
    *out_offset = (uint64_t)chunk_off;

    if (sample_idx != chunk_first_sample) {
        uint32_t stsz_win_start = cache->stsz.win_sample_start;
        uint32_t sum_start = chunk_first_sample;
        uint32_t delta_bytes = 0U;

        if (chunk_first_sample < stsz_win_start) {
            int32_t prefix = (int32_t)cache->ctrl->chunk_prefix_bytes;
            *out_offset = (uint64_t)chunk_off + (uint64_t)(int64_t)prefix;
            sum_start = stsz_win_start;
            if (sample_idx == stsz_win_start) {
                return MP4_OK;
            }
        }

        err = mp4CacheSumStszInWindow(&cache->stsz, sum_start, sample_idx - 1U, &delta_bytes);
        if (err != MP4_OK) {
            return err;
        }
        *out_offset += (uint64_t)delta_bytes;
    }

    return MP4_OK;
}

/* 0x01a104 */
MP4_NOINLINE int32_t mp4CacheSumStszInWindow(
    Mp4CacheStsz *stsz,
    uint32_t start_idx,
    uint32_t end_idx,
    uint32_t *out_sum)
{
    uint32_t win_start = stsz->win_sample_start;
    uint32_t win_end = win_start + stsz->win_sample_count - 1U;

    if (start_idx < win_start || win_end < start_idx || end_idx < win_start || win_end < end_idx) {
        return MP4_ERR_SAMPLE_RANGE;
    }

    if (stsz->const_sample_size != 0U) {
        *out_sum = stsz->const_sample_size * (end_idx - start_idx + 1U);
        return MP4_OK;
    }

    return mp4CacheSumBlockRange(stsz->active, win_start, start_idx, end_idx, 0U, out_sum);
}

/* 0x01a1a8 */
MP4_NOINLINE int32_t mp4CacheSumBlockRange(
    Mp4CacheBlock *head,
    uint32_t win_start,
    uint32_t start_idx,
    uint32_t end_idx,
    uint32_t col,
    uint32_t *out_sum)
{
    uint32_t rel_start = start_idx - win_start;
    uint32_t rel_end = end_idx - win_start;
    uint32_t in_blk = rel_start;
    uint32_t cur_rel;
    Mp4CacheBlock *blk = head;

    *out_sum = 0U;
    if (blk != NULL) {
        while (blk->count < in_blk + 1U) {
            in_blk -= blk->count;
            blk = blk->next;
            if (blk == NULL) {
                break;
            }
        }
    }

    if (blk == NULL) {
        return MP4_ERR_INTERNAL;
    }
    if (rel_end < rel_start) {
        return MP4_OK;
    }

    for (cur_rel = rel_start; cur_rel <= rel_end; cur_rel++) {
        if (blk->count == in_blk) {
            blk = blk->next;
            in_blk = 0U;
            if (blk == NULL) {
                return MP4_ERR_INTERNAL;
            }
        }
        *out_sum += blk->words[blk->cols * in_blk + col];
        in_blk++;
    }

    return MP4_OK;
}

/* 0x01a28c */
MP4_NOINLINE int32_t mp4CacheFindSyncSample(
    Mp4CacheStbl *cache,
    uint32_t sample_idx,
    int32_t direction,
    uint32_t *out_sample,
    uint32_t *out_delta,
    int32_t allow_io)
{
    uint32_t total_samples;
    uint32_t base_time = 0U;
    uint32_t found_time = 0U;
    uint32_t sample_dur = 0U;
    uint32_t is_sync = 0U;
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

    total_samples = cache->ctrl->total_samples;
    if (total_samples == 0U || sample_idx >= total_samples) {
        return MP4_ERR_SAMPLE_RANGE;
    }
    if (sample_idx == 0U && direction == 0) {
        return MP4_ERR_NO_SYNC;
    }

    err = mp4CacheLookupSampleTime(cache, sample_idx, &base_time, &sample_dur, allow_io);
    if (err != MP4_OK) {
        return err;
    }

    if (direction != 0) {
        uint32_t scan = sample_idx + 1U;
        while (scan < cache->ctrl->total_samples) {
            err = mp4CacheEnsureStssSample(cache, scan, allow_io);
            if (err == MP4_OK) {
                err = mp4CacheCheckSyncSample(&cache->stss, scan, &is_sync);
            }
            if (err != MP4_OK) {
                if (err == MP4_ERR_SAMPLE_RANGE) {
                    return MP4_ERR_NO_SYNC;
                }
                return err;
            }
            if (is_sync == 1U) {
                *out_sample = scan;
                err = mp4CacheLookupSampleTime(cache, scan, &found_time, &sample_dur, allow_io);
                if (err == MP4_OK) {
                    *out_delta = found_time - base_time;
                }
                return err;
            }
            scan++;
        }
        return MP4_ERR_NO_SYNC;
    }

    if (sample_idx == 0U) {
        return MP4_ERR_NO_SYNC;
    }

    {
        uint32_t scan = sample_idx - 1U;
        uint32_t steps = 0U;

        for (;;) {
            steps++;
            err = mp4CacheEnsureStssSample(cache, scan, allow_io);
            if (err == MP4_OK) {
                err = mp4CacheCheckSyncSample(&cache->stss, scan, &is_sync);
            }
            if (err != MP4_OK) {
                return err;
            }
            if (is_sync == 1U) {
                *out_sample = scan;
                err = mp4CacheLookupSampleTime(cache, scan, &found_time, &sample_dur, allow_io);
                if (err == MP4_OK) {
                    *out_delta = base_time - found_time;
                }
                return err;
            }
            if (steps >= sample_idx) {
                return MP4_ERR_NO_SYNC;
            }
            scan--;
        }
    }
}

/* 0x01a4c8 */
MP4_NOINLINE int32_t mp4CacheGetSampleTime(
    Mp4CacheStbl *cache,
    uint32_t sample_idx,
    uint32_t *out_time,
    uint32_t *out_dur,
    int32_t allow_io)
{
    Mp4CacheCtrl *ctrl;
    uint32_t total_samples;

    if (cache == NULL) {
        return MP4_ERR_CORRUPT;
    }
    ctrl = cache->ctrl;
    if (ctrl == NULL) {
        return MP4_ERR_NOT_READY;
    }
    if (!mp4CacheHasStts(&cache->stts)) {
        return MP4_ERR_CORRUPT;
    }

    total_samples = ctrl->total_samples;
    if (total_samples == 0U || sample_idx >= total_samples) {
        return MP4_ERR_SAMPLE_RANGE;
    }

    return mp4CacheLookupSampleTime(cache, sample_idx, out_time, out_dur, allow_io);
}

/* 0x01a584 */
MP4_NOINLINE int32_t mp4CacheLookupSampleTime(
    Mp4CacheStbl *cache,
    uint32_t sample_idx,
    uint32_t *out_time,
    uint32_t *out_dur,
    int32_t allow_io)
{
    int32_t err;

    err = mp4CacheEnsureSttsSample(cache, sample_idx, allow_io);
    if (err == MP4_OK) {
        return mp4CacheSttsLookupBySample(&cache->stts, sample_idx, out_time, out_dur);
    }
    return err;
}

/* 0x01a600 */
MP4_NOINLINE int32_t mp4CacheSttsLookupBySample(
    Mp4CacheStts *stts,
    uint32_t sample_idx,
    uint32_t *out_time,
    uint32_t *out_dur)
{
    uint32_t win_sample_start = stts->win_sample_start;
    uint32_t win_sample_count = stts->win_sample_count;
    uint32_t win_entry_start = stts->win_entry_start;
    uint32_t win_entry_count = stts->win_entry_count;
    uint32_t cur_entry = stts->cur_entry;
    uint32_t cur_sample;
    int32_t err;

    if (sample_idx < win_sample_start || win_sample_start + win_sample_count - 1U < sample_idx) {
        return MP4_ERR_SAMPLE_RANGE;
    }

    if (cur_entry < win_entry_start || win_entry_start + win_entry_count - 1U < cur_entry) {
        stts->cur_entry = 0U;
        stts->cur_sample = 0U;
        stts->cur_time = 0U;
        cur_entry = 0U;
    }

    if (cur_entry == 0U) {
        stts->cur_entry = win_entry_start;
        stts->cur_time = stts->win_time_start;
        stts->cur_sample = win_sample_start;
    }

    cur_sample = stts->cur_sample;
    if (sample_idx < cur_sample) {
        if (sample_idx - win_sample_start < cur_sample - sample_idx) {
            cur_sample = win_sample_start;
            cur_entry = win_entry_start;
            stts->cur_time = stts->win_time_start;
            stts->cur_entry = win_entry_start;
            stts->cur_sample = win_sample_start;
        } else {
            cur_entry = stts->cur_entry;
        }
    } else {
        cur_entry = stts->cur_entry;
        if (cur_sample < sample_idx) {
            uint32_t last_entry = win_entry_start + win_entry_count - 1U;
            if (cur_entry != last_entry &&
                win_sample_start + win_sample_count - sample_idx < sample_idx - cur_sample) {
                uint32_t run_samples = 0U;
                uint32_t run_delta = 0U;

                stts->cur_entry = last_entry;
                err = mp4SampleBlockReadWord(stts->active, win_entry_start, last_entry, 0U, &run_samples);
                if (err != MP4_OK) {
                    return err;
                }
                err = mp4SampleBlockReadWord(stts->active, win_entry_start, last_entry, 1U, &run_delta);
                if (err != MP4_OK) {
                    return err;
                }
                cur_entry = stts->cur_entry;
                cur_sample = win_sample_start + win_sample_count - run_samples;
                stts->cur_sample = cur_sample;
                stts->cur_time = stts->win_time_start + stts->win_time_count - run_samples * run_delta;
            }
        }
    }

    if (sample_idx < cur_sample) {
        uint32_t scan_idx = cur_entry - 1U;
        uint32_t acc_sample = cur_sample;
        uint32_t acc_time = 0U;

        if (scan_idx >= stts->entry_count) {
            return MP4_ERR_CORRUPT;
        }

        for (;;) {
            uint32_t run_samples = 0U;
            uint32_t run_delta = 0U;

            err = mp4SampleBlockReadWord(stts->active, win_entry_start, scan_idx, 0U, &run_samples);
            if (err != MP4_OK) {
                return err;
            }
            err = mp4SampleBlockReadWord(stts->active, win_entry_start, scan_idx, 1U, &run_delta);
            if (err != MP4_OK) {
                return err;
            }
            acc_sample -= run_samples;
            acc_time += run_delta * run_samples;
            if (sample_idx >= acc_sample) {
                uint32_t new_time = stts->cur_time - acc_time;
                *out_time = new_time + (sample_idx - acc_sample) * run_delta;
                *out_dur = run_delta;
                stts->cur_entry = scan_idx;
                stts->cur_time = new_time;
                stts->cur_sample = acc_sample;
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
        uint32_t acc_sample = cur_sample;
        uint32_t acc_time = 0U;
        uint32_t scan_idx = cur_entry;
        uint32_t win_entry_end = win_entry_start + win_entry_count;

        while (scan_idx < win_entry_end) {
            uint32_t run_samples = 0U;
            uint32_t run_delta = 0U;

            err = mp4SampleBlockReadWord(stts->active, win_entry_start, scan_idx, 0U, &run_samples);
            if (err != MP4_OK) {
                return err;
            }
            err = mp4SampleBlockReadWord(stts->active, win_entry_start, scan_idx, 1U, &run_delta);
            if (err != MP4_OK) {
                return err;
            }
            if (sample_idx < acc_sample + run_samples) {
                uint32_t new_time = stts->cur_time + acc_time;
                *out_time = new_time + (sample_idx - acc_sample) * run_delta;
                *out_dur = run_delta;
                stts->cur_entry = scan_idx;
                stts->cur_time = new_time;
                stts->cur_sample = acc_sample;
                return MP4_OK;
            }
            acc_sample += run_samples;
            acc_time += run_delta * run_samples;
            scan_idx++;
        }
    }

    return MP4_ERR_CORRUPT;
}

/* 0x01ac80 */
MP4_NOINLINE int32_t mp4CacheEnsureSttsSample(
    Mp4CacheStbl *cache,
    uint32_t sample_idx,
    int32_t allow_io)
{
    Mp4CacheCtrl *ctrl;
    uint32_t win_s_start;
    uint32_t win_s_cnt;
    uint32_t win_t_cnt;
    uint32_t load_start;
    uint32_t load_cnt;
    int32_t err;

    if (cache == NULL || cache->ctrl == NULL) {
        return MP4_ERR_NOT_READY;
    }
    ctrl = cache->ctrl;
    if (!mp4CacheHasStts(&cache->stts)) {
        return MP4_ERR_CORRUPT;
    }
    if (ctrl->total_samples == 0U || sample_idx >= ctrl->total_samples) {
        return MP4_ERR_SAMPLE_RANGE;
    }

    mp4CacheSttsGetWindow(&cache->stts, &win_s_start, &win_s_cnt, NULL, &win_t_cnt);
    if (win_s_cnt != 0U && win_t_cnt != 0U &&
        sample_idx >= win_s_start && win_s_start + win_s_cnt - 1U >= sample_idx) {
        return MP4_OK;
    }
    if (allow_io == 0) {
        return MP4_ERR_IO;
    }

    err = mp4AttachStream(cache->ctrl->stream);
    if (err != MP4_OK) {
        return err;
    }

    err = mp4CacheCalcLoadRange(sample_idx, win_s_start, win_s_cnt, cache->ctrl->total_samples,
                                &load_start, &load_cnt, cache->ctrl);
    if (err != MP4_OK) {
        return err;
    }

    err = mp4CacheLoadStts(&cache->stts, load_start, load_cnt,
                           cache->ctrl->total_samples, cache->ctrl->total_duration, cache->ctrl->stream);
    mp4CacheRecalcBounds(cache);
    return err;
}

/* 0x01adc8 */
MP4_NOINLINE int32_t mp4CacheEnsureStscSample(
    Mp4CacheStbl *cache,
    uint32_t sample_idx,
    int32_t allow_io)
{
    Mp4CacheCtrl *ctrl;
    uint32_t win_s_start;
    uint32_t win_s_cnt;
    uint32_t load_start;
    uint32_t load_cnt;
    int32_t err;

    if (cache == NULL || cache->ctrl == NULL) {
        return MP4_ERR_NOT_READY;
    }
    ctrl = cache->ctrl;
    if (!mp4CacheHasStsc(&cache->stsc)) {
        return MP4_ERR_CORRUPT;
    }
    if (ctrl->total_samples == 0U || sample_idx >= ctrl->total_samples) {
        return MP4_ERR_SAMPLE_RANGE;
    }

    mp4CacheStscGetWindow(&cache->stsc, &win_s_start, &win_s_cnt, NULL, NULL);
    if (win_s_cnt != 0U &&
        sample_idx >= win_s_start && win_s_start + win_s_cnt - 1U >= sample_idx) {
        return MP4_OK;
    }
    if (allow_io == 0) {
        return MP4_ERR_IO;
    }

    err = mp4AttachStream(cache->ctrl->stream);
    if (err != MP4_OK) {
        return err;
    }

    err = mp4CacheCalcLoadRange(sample_idx, win_s_start, win_s_cnt, cache->ctrl->total_samples,
                                &load_start, &load_cnt, cache->ctrl);
    if (err != MP4_OK) {
        return err;
    }

    err = mp4CacheLoadStsc(&cache->stsc, load_start, load_cnt,
                           cache->ctrl->total_samples, cache->ctrl->total_chunks, cache->ctrl->stream);
    mp4CacheRecalcBounds(cache);
    return err;
}

/* 0x01af08 */
MP4_NOINLINE int32_t mp4CacheEnsureStszSample(
    Mp4CacheStbl *cache,
    uint32_t sample_idx,
    int32_t allow_io)
{
    Mp4CacheCtrl *ctrl;
    uint32_t win_s_start;
    uint32_t win_s_cnt;
    uint32_t load_start;
    uint32_t load_cnt;
    uint32_t chunk_idx;
    uint32_t chunk_first_sample;
    uint32_t desc_idx;
    int32_t err;

    if (cache == NULL) {
        return MP4_ERR_NOT_READY;
    }
    ctrl = cache->ctrl;
    if (ctrl == NULL) {
        return MP4_ERR_NOT_READY;
    }
    if (!mp4CacheHasStsz(&cache->stsz)) {
        return MP4_ERR_CORRUPT;
    }
    if (ctrl->total_samples == 0U || sample_idx >= ctrl->total_samples) {
        return MP4_ERR_SAMPLE_RANGE;
    }

    win_s_start = cache->stsz.win_sample_start;
    win_s_cnt = cache->stsz.win_sample_count;
    if (win_s_cnt != 0U &&
        sample_idx >= win_s_start && win_s_start + win_s_cnt - 1U >= sample_idx) {
        return MP4_OK;
    }
    if (allow_io == 0) {
        return MP4_ERR_IO;
    }

    err = mp4AttachStream(ctrl->stream);
    if (err != MP4_OK) {
        return err;
    }

    err = mp4CacheCalcLoadRange(sample_idx, win_s_start, win_s_cnt, cache->ctrl->total_samples,
                                &load_start, &load_cnt, cache->ctrl);
    if (err != MP4_OK) {
        return err;
    }

    err = mp4CacheEnsureStscSample(cache, load_start, allow_io);
    if (err == MP4_OK) {
        err = mp4CacheStscLookupBySample(&cache->stsc, load_start,
                                         &chunk_idx, &chunk_first_sample, &desc_idx);
    }
    if (err != MP4_OK) {
        return err;
    }

    err = mp4CacheLoadStsz(&cache->stsz, load_start, load_cnt,
                           chunk_first_sample, &cache->ctrl->chunk_prefix_bytes, cache->ctrl->stream);
    mp4CacheRecalcBounds(cache);
    return err;
}

/* 0x01b084 */
MP4_NOINLINE int32_t mp4CacheEnsureStssSample(
    Mp4CacheStbl *cache,
    uint32_t sample_idx,
    int32_t allow_io)
{
    uint32_t win_s_start;
    uint32_t win_s_cnt;
    uint32_t load_start;
    uint32_t load_cnt;
    int32_t err;

    if (cache == NULL || cache->ctrl == NULL) {
        return MP4_ERR_NOT_READY;
    }
    if (cache->stss.size == 0U) {
        return MP4_OK;
    }
    if (cache->ctrl->total_samples == 0U || sample_idx >= cache->ctrl->total_samples) {
        return MP4_ERR_SAMPLE_RANGE;
    }
    if (cache->stss.entry_count == 0U) {
        return MP4_OK;
    }

    win_s_start = cache->stss.win_sample_start;
    win_s_cnt = cache->stss.win_sample_count;
    if (win_s_cnt != 0U &&
        sample_idx >= win_s_start && win_s_start + win_s_cnt - 1U >= sample_idx) {
        return MP4_OK;
    }
    if (allow_io == 0) {
        return MP4_ERR_IO;
    }

    err = mp4AttachStream(cache->ctrl->stream);
    if (err != MP4_OK) {
        return err;
    }

    err = mp4CacheCalcLoadRange(sample_idx, win_s_start, win_s_cnt, cache->ctrl->total_samples,
                                &load_start, &load_cnt, cache->ctrl);
    if (err != MP4_OK) {
        return err;
    }

    err = mp4CacheLoadStss(&cache->stss, load_start, load_cnt, cache->ctrl->total_samples, cache->ctrl->stream);
    mp4CacheRecalcBounds(cache);
    return err;
}

/* 0x01b1b4 */
MP4_NOINLINE int32_t mp4CacheGetCttsOffset(
    Mp4CacheCtts *ctts,
    uint32_t sample_idx,
    uint32_t *out_offset)
{
    uint32_t win_sample_start = ctts->win_sample_start;
    uint32_t win_sample_count = ctts->win_sample_count;
    uint32_t win_entry_start;
    uint32_t win_entry_count;
    uint32_t cur_entry;
    uint32_t cur_sample;
    uint32_t found_entry = 0U;
    uint32_t found_sample = 0U;
    uint32_t offset_val = 0U;
    int32_t err;

    if (sample_idx < win_sample_start || win_sample_start + win_sample_count - 1U < sample_idx) {
        return MP4_ERR_SAMPLE_RANGE;
    }
    if (ctts->size == 0U) {
        *out_offset = 0U;
        return MP4_OK;
    }

    win_entry_start = ctts->win_entry_start;
    win_entry_count = ctts->win_entry_count;
    cur_entry = ctts->cur_entry;

    if (cur_entry < win_entry_start || win_entry_start + win_entry_count - 1U < cur_entry) {
        ctts->cur_entry = 0U;
        ctts->cur_sample = 0U;
        cur_entry = 0U;
    }

    if (cur_entry == 0U) {
        ctts->cur_entry = win_entry_start;
        ctts->cur_sample = win_sample_start;
    }

    cur_sample = ctts->cur_sample;
    if (sample_idx < cur_sample) {
        if (sample_idx - win_sample_start < cur_sample - sample_idx) {
            cur_sample = win_sample_start;
            cur_entry = win_entry_start;
            ctts->cur_entry = win_entry_start;
            ctts->cur_sample = win_sample_start;
        } else {
            cur_entry = ctts->cur_entry;
        }
    } else {
        cur_entry = ctts->cur_entry;
        if (cur_sample < sample_idx) {
            uint32_t last_entry = win_entry_start + win_entry_count - 1U;
            if (cur_entry != last_entry &&
                win_sample_start + win_sample_count - sample_idx < sample_idx - cur_sample) {
                uint32_t run_samples = 0U;

                ctts->cur_entry = last_entry;
                err = mp4SampleBlockReadWord(ctts->active, win_entry_start, last_entry, 0U, &run_samples);
                if (err != MP4_OK) {
                    return err;
                }
                cur_entry = ctts->cur_entry;
                cur_sample = win_sample_start + win_sample_count - run_samples;
                ctts->cur_sample = cur_sample;
            }
        }
    }

    if (sample_idx < cur_sample) {
        uint32_t scan_idx = cur_entry - 1U;
        uint32_t acc_sample = cur_sample;

        if (scan_idx >= ctts->entry_count) {
            return MP4_ERR_CORRUPT;
        }

        for (;;) {
            uint32_t run_samples = 0U;

            err = mp4SampleBlockReadWord(ctts->active, win_entry_start, scan_idx, 0U, &run_samples);
            if (err != MP4_OK) {
                return err;
            }
            acc_sample -= run_samples;
            if (sample_idx >= acc_sample) {
                found_entry = scan_idx;
                found_sample = acc_sample;
                break;
            }
            if (scan_idx == 0U) {
                return MP4_ERR_CORRUPT;
            }
            scan_idx--;
        }
    } else {
        uint32_t scan_idx = cur_entry;
        uint32_t acc_sample = cur_sample;
        uint32_t win_entry_end = win_entry_start + win_entry_count;

        if (scan_idx >= ctts->entry_count || scan_idx >= win_entry_end) {
            return MP4_ERR_CORRUPT;
        }

        while (scan_idx < win_entry_end) {
            uint32_t run_samples = 0U;

            err = mp4SampleBlockReadWord(ctts->active, win_entry_start, scan_idx, 0U, &run_samples);
            if (err != MP4_OK) {
                return err;
            }
            if (sample_idx < acc_sample + run_samples) {
                found_entry = scan_idx;
                found_sample = acc_sample;
                break;
            }
            acc_sample += run_samples;
            scan_idx++;
        }

        if (scan_idx >= win_entry_end) {
            return MP4_ERR_CORRUPT;
        }
    }

    err = mp4SampleBlockReadWord(ctts->active, win_entry_start, found_entry, 1U, &offset_val);
    if (err != MP4_OK) {
        return err;
    }
    *out_offset = offset_val;
    ctts->cur_sample = found_sample;
    ctts->cur_entry = found_entry;
    return MP4_OK;
}

/* 0x01b618 */
MP4_NOINLINE int32_t mp4CacheSumSampleSizesSpan(
    Mp4CacheStsz *stsz,
    uint32_t start_idx,
    uint32_t end_idx,
    uint32_t *out_sum,
    Mp4IoStream *stream)
{
    uint32_t win_end_excl = stsz->win_sample_start + stsz->win_sample_count;
    uint32_t tail_sum;
    int32_t err;

    *out_sum = 0U;
    if (end_idx >= win_end_excl) {
        err = mp4CacheSumStszInWindow(stsz, start_idx, win_end_excl - 1U, out_sum);
        if (err != MP4_OK) {
            return err;
        }
        err = mp4CacheReadAndSumStsz(stsz, win_end_excl, end_idx, &tail_sum, stream);
        if (err != MP4_OK) {
            return err;
        }
        *out_sum += tail_sum;
        return MP4_OK;
    }

    err = mp4CacheSumStszInWindow(stsz, start_idx, end_idx, out_sum);
    if (err != MP4_OK) {
        return err;
    }
    return MP4_OK;
}

/* 0x01b6e0 */
MP4_NOINLINE int32_t mp4CacheSumDurationsSpan(
    Mp4CacheStts *stts,
    uint32_t start_idx,
    uint32_t end_idx,
    uint32_t *out_sum,
    Mp4IoStream *stream)
{
    uint32_t win_end_excl = stts->win_sample_start + stts->win_sample_count;
    uint32_t tail_sum;
    int32_t err;

    *out_sum = 0U;
    if (end_idx >= win_end_excl) {
        err = mp4CacheSumDurationsInWindow(stts, start_idx, win_end_excl - 1U, out_sum);
        if (err != MP4_OK) {
            return err;
        }
        err = mp4CacheSumDurationsBeyond(stts, end_idx, &tail_sum, stream);
        if (err != MP4_OK) {
            return err;
        }
        *out_sum += tail_sum;
        return MP4_OK;
    }

    err = mp4CacheSumDurationsInWindow(stts, start_idx, end_idx, out_sum);
    if (err != MP4_OK) {
        return err;
    }
    return MP4_OK;
}

/* 0x01b79c */
MP4_NOINLINE int32_t mp4CacheSumDurationsInWindow(
    Mp4CacheStts *stts,
    uint32_t start_idx,
    uint32_t end_idx,
    uint32_t *out_sum)
{
    uint32_t win_start = stts->win_sample_start;
    uint32_t win_end = win_start + stts->win_sample_count - 1U;
    uint32_t cur_idx;

    *out_sum = 0U;
    if (start_idx < win_start || win_end < start_idx || end_idx < win_start || win_end < end_idx) {
        return MP4_ERR_SAMPLE_RANGE;
    }

    for (cur_idx = start_idx; cur_idx <= end_idx; cur_idx++) {
        uint32_t sample_time = 0U;
        uint32_t sample_dur = 0U;
        int32_t err = mp4CacheSttsLookupBySample(stts, cur_idx, &sample_time, &sample_dur);
        if (err != MP4_OK) {
            return err;
        }
        *out_sum += sample_dur;
    }

    return MP4_OK;
}

/* 0x01b86c */
MP4_NOINLINE int32_t mp4CacheSumDurationsBeyond(
    Mp4CacheStts *stts,
    uint32_t end_idx,
    uint32_t *out_sum,
    Mp4IoStream *stream)
{
    uint32_t next_entry = stts->win_entry_start + stts->win_entry_count;
    uint32_t cur_sample = stts->win_sample_start + stts->win_sample_count;
    uint32_t cur_time = stts->win_time_start + stts->win_time_count;
    uint32_t rem_entries = stts->entry_count - next_entry;
    uint64_t seek_pos = stts->file_offset + (uint64_t)(8U * next_entry);
    Mp4CacheBlock *blk = NULL;
    int32_t err = MP4_OK;
    int32_t done = 0;

    *out_sum = 0U;
    if ((uint64_t)mp4CbTell(stream->file_handle) != seek_pos) {
        if (mp4SeekAbs(stream->file_handle, seek_pos) != MP4_OK) {
            return MP4_ERR_IO;
        }
    }

    while (rem_entries != 0U && !done) {
        uint32_t chunk_req = stream->base_unit;
        uint32_t blk_samples = 0U;
        uint32_t blk_dur = 0U;
        uint32_t blk_entries = 0U;
        uint32_t row;

        if (rem_entries < chunk_req) {
            chunk_req = rem_entries;
        }
        err = mp4CacheReadSttsBlock(&blk, chunk_req, 1, cur_sample, cur_time,
                                    &blk_samples, &blk_dur, &blk_entries,
                                    &stts->free_pool, stream);
        if (err != MP4_OK) {
            break;
        }

        for (row = 0U; row < blk_entries; row++) {
            uint32_t run_samples = 0U;
            uint32_t run_delta = 0U;

            err = mp4SampleBlockReadWord(blk, 0U, row, 0U, &run_samples);
            if (err != MP4_OK) {
                done = 1;
                break;
            }
            err = mp4SampleBlockReadWord(blk, 0U, row, 1U, &run_delta);
            if (err != MP4_OK) {
                done = 1;
                break;
            }
            if (end_idx < cur_sample + run_samples) {
                *out_sum += (end_idx - cur_sample + 1U) * run_delta;
                done = 1;
                break;
            }
            cur_sample += run_samples;
            *out_sum += run_samples * run_delta;
            cur_time += run_samples * run_delta;
        }
    }

    if (blk != NULL) {
        blk->next = stts->free_pool;
        stts->free_pool = blk;
    }
    return err;
}

/* 0x01bb6c */
MP4_NOINLINE int32_t mp4CacheStscLookupByChunk(
    Mp4CacheStsc *stsc,
    uint32_t chunk_idx,
    uint32_t *out_first_sample,
    uint32_t *out_samples_per_chunk,
    uint32_t *out_desc_idx)
{
    uint32_t win_chunk_start = stsc->win_chunk_start;
    uint32_t win_chunk_count = stsc->win_chunk_count;
    uint32_t win_entry_start;
    uint32_t win_entry_count;
    uint32_t cur_entry;
    uint32_t cur_first_chunk = 0U;
    uint32_t cur_chunk_zero;
    int32_t err;

    if (chunk_idx < win_chunk_start || win_chunk_start + win_chunk_count - 1U < chunk_idx) {
        return MP4_ERR_OUT_OF_CHUNKS;
    }
    if (stsc->entry_count == 0U) {
        return MP4_ERR_OUT_OF_CHUNKS;
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
        cur_entry = win_entry_start;
        stsc->cur_entry = win_entry_start;
        stsc->cur_sample = stsc->win_sample_start;
    }

    err = mp4SampleBlockReadWord(stsc->active, win_entry_start, cur_entry, 0U, &cur_first_chunk);
    if (err != MP4_OK) {
        return err;
    }
    cur_chunk_zero = cur_first_chunk - 1U;

    if (chunk_idx < cur_chunk_zero) {
        if (chunk_idx - win_chunk_start < cur_chunk_zero - chunk_idx) {
            cur_entry = win_entry_start;
            stsc->cur_entry = win_entry_start;
            stsc->cur_sample = stsc->win_sample_start;
        }
    } else if (cur_chunk_zero < chunk_idx) {
        uint32_t last_entry = win_entry_start + win_entry_count - 1U;
        uint32_t win_chunk_end_excl = win_chunk_start + win_chunk_count;
        if (cur_entry != last_entry && win_chunk_end_excl - chunk_idx < chunk_idx - cur_chunk_zero) {
            uint32_t last_first_chunk = 0U;
            uint32_t last_spc = 0U;

            stsc->cur_entry = last_entry;
            err = mp4SampleBlockReadWord(stsc->active, win_entry_start, last_entry, 0U, &last_first_chunk);
            if (err != MP4_OK) {
                return err;
            }
            err = mp4SampleBlockReadWord(stsc->active, win_entry_start, last_entry, 1U, &last_spc);
            if (err != MP4_OK) {
                return err;
            }
            cur_entry = stsc->cur_entry;
            stsc->cur_sample = stsc->win_sample_start + stsc->win_sample_count -
                               (win_chunk_end_excl + 1U - last_first_chunk) * last_spc;
        }
    }

    err = mp4SampleBlockReadWord(stsc->active, win_entry_start, cur_entry, 0U, &cur_first_chunk);
    if (err != MP4_OK) {
        return err;
    }

    if (chunk_idx < cur_first_chunk - 1U) {
        uint32_t next_chunk_zero = cur_first_chunk - 1U;
        uint32_t acc_sample = stsc->cur_sample;
        uint32_t scan_idx;

        if (cur_entry == 0U) {
            return MP4_ERR_CORRUPT;
        }
        scan_idx = cur_entry - 1U;
        for (;;) {
            uint32_t first_chunk = 0U;
            uint32_t spc = 0U;
            uint32_t chunk_zero;

            err = mp4SampleBlockReadWord(stsc->active, win_entry_start, scan_idx, 0U, &first_chunk);
            if (err != MP4_OK) {
                return err;
            }
            chunk_zero = first_chunk - 1U;
            err = mp4SampleBlockReadWord(stsc->active, win_entry_start, scan_idx, 1U, &spc);
            if (err != MP4_OK) {
                return err;
            }
            acc_sample -= (next_chunk_zero - chunk_zero) * spc;
            if (chunk_idx >= chunk_zero) {
                uint32_t desc_val = 0U;

                *out_first_sample = acc_sample + (chunk_idx - chunk_zero) * spc;
                *out_samples_per_chunk = spc;
                err = mp4SampleBlockReadWord(stsc->active, stsc->win_entry_start, scan_idx, 2U, &desc_val);
                if (err != MP4_OK) {
                    return err;
                }
                *out_desc_idx = desc_val;
                stsc->cur_sample = acc_sample;
                stsc->cur_entry = scan_idx;
                return MP4_OK;
            }
            if (scan_idx == 0U) {
                return MP4_ERR_CORRUPT;
            }
            next_chunk_zero = chunk_zero;
            scan_idx--;
        }
    }

    {
        uint32_t chunk_zero = cur_first_chunk - 1U;
        uint32_t acc_sample = stsc->cur_sample;
        uint32_t scan_idx = cur_entry;
        uint32_t win_entry_end = win_entry_start + win_entry_count;
        uint32_t win_chunk_end_excl = win_chunk_start + win_chunk_count;

        while (scan_idx < win_entry_end) {
            uint32_t next_chunk_zero;
            uint32_t spc = 0U;

            if (scan_idx == win_entry_end - 1U) {
                next_chunk_zero = win_chunk_end_excl;
            } else {
                uint32_t next_first_chunk = 0U;
                err = mp4SampleBlockReadWord(stsc->active, win_entry_start, scan_idx + 1U, 0U, &next_first_chunk);
                if (err != MP4_OK) {
                    return err;
                }
                next_chunk_zero = next_first_chunk - 1U;
            }

            err = mp4SampleBlockReadWord(stsc->active, win_entry_start, scan_idx, 1U, &spc);
            if (err != MP4_OK) {
                return err;
            }
            if (chunk_idx < next_chunk_zero) {
                uint32_t desc_val = 0U;

                *out_first_sample = acc_sample + (chunk_idx - chunk_zero) * spc;
                *out_samples_per_chunk = spc;
                err = mp4SampleBlockReadWord(stsc->active, stsc->win_entry_start, scan_idx, 2U, &desc_val);
                if (err != MP4_OK) {
                    return err;
                }
                *out_desc_idx = desc_val;
                stsc->cur_sample = acc_sample;
                stsc->cur_entry = scan_idx;
                return MP4_OK;
            }
            acc_sample += (next_chunk_zero - chunk_zero) * spc;
            chunk_zero = next_chunk_zero;
            scan_idx++;
        }
    }

    return MP4_ERR_CORRUPT;
}

/* 0x01c420 */
MP4_NOINLINE int32_t mp4CacheLoadStcoForSamples(
    Mp4CacheStbl *cache,
    uint32_t start_sample,
    uint32_t count,
    Mp4IoStream *stream)
{
    uint32_t first_chunk;
    uint32_t last_chunk;
    uint32_t chunk_first_sample;
    uint32_t desc_idx;
    uint32_t win_s_start;
    uint32_t win_s_cnt;
    uint32_t win_c_start;
    uint32_t win_c_cnt;
    uint32_t base_unit;
    uint32_t total_chunks;
    int32_t err;

    err = mp4CacheStscLookupBySample(&cache->stsc, start_sample,
                                     &first_chunk, &chunk_first_sample, &desc_idx);
    if (err != MP4_OK) {
        return err;
    }

    base_unit = stream->base_unit;
    first_chunk = (first_chunk / base_unit) * base_unit;

    mp4CacheStscGetWindow(&cache->stsc, &win_s_start, &win_s_cnt, &win_c_start, &win_c_cnt);
    if (win_c_cnt == 1U) {
        last_chunk = first_chunk;
    } else if (win_s_start + win_s_cnt >= start_sample + count) {
        err = mp4CacheStscLookupBySample(&cache->stsc, start_sample + count - 1U,
                                         &last_chunk, &chunk_first_sample, &desc_idx);
        if (err != MP4_OK) {
            return err;
        }
    } else {
        last_chunk = win_c_start + win_c_cnt - 1U;
    }

    base_unit = stream->base_unit;
    total_chunks = cache->ctrl->total_chunks;
    last_chunk = (last_chunk / base_unit) * base_unit + base_unit - 1U;
    if (total_chunks - 1U < last_chunk) {
        last_chunk = total_chunks - 1U;
    }

    return mp4CacheLoadStco(&cache->stco, first_chunk, last_chunk - first_chunk + 1U, stream);
}

/* 0x01c5a0 */
MP4_NOINLINE int32_t mp4CacheSetTrack(Mp4CacheStbl *cache, Mp4Track *track)
{
    if (cache == NULL) {
        return MP4_ERR_CORRUPT;
    }
    cache->track = track;
    return MP4_OK;
}

/* 0x01c5b8 */
MP4_NOINLINE int32_t mp4CacheSetTotals(
    Mp4CacheStbl *cache,
    uint32_t total_samples,
    uint32_t total_dur,
    uint32_t total_chunks)
{
    if (cache == NULL) {
        return MP4_ERR_CORRUPT;
    }
    if (cache->ctrl == NULL) {
        return MP4_ERR_NOT_READY;
    }
    cache->ctrl->total_chunks = total_chunks;
    cache->ctrl->total_samples = total_samples;
    cache->ctrl->total_duration = total_dur;
    return MP4_OK;
}

/* 0x01c5e4 */
MP4_NOINLINE int32_t mp4CacheGetChunkCount(Mp4CacheStbl *cache, uint32_t *out_count)
{
    *out_count = 0U;
    if (cache == NULL) {
        return MP4_ERR_CORRUPT;
    }
    if (cache->ctrl == NULL) {
        return MP4_ERR_NOT_READY;
    }
    if (!mp4CacheHasStco(&cache->stco)) {
        return MP4_ERR_CORRUPT;
    }
    *out_count = mp4CacheStcoCount(&cache->stco);
    return MP4_OK;
}

/* 0x01c654 */
MP4_NOINLINE void mp4CacheDestroy(Mp4CacheStbl *cache)
{
    if (cache == NULL) {
        return;
    }

    if (cache->stts.active != NULL) {
        mp4CacheFreeBlocks(&cache->stts.active);
    }
    if (cache->stts.free_pool != NULL) {
        mp4CacheFreeBlocks(&cache->stts.free_pool);
    }
    cache->stts.win_entry_count = 0U;
    cache->stts.win_sample_start = 0U;
    cache->stts.win_sample_count = 0U;
    cache->stts.win_time_start = 0U;
    cache->stts.win_time_count = 0U;
    cache->stts.win_entry_start = 0U;

    if (cache->stsc.active != NULL) {
        mp4CacheFreeBlocks(&cache->stsc.active);
    }
    if (cache->stsc.free_pool != NULL) {
        mp4CacheFreeBlocks(&cache->stsc.free_pool);
    }
    cache->stsc.win_entry_count = 0U;
    cache->stsc.win_sample_start = 0U;
    cache->stsc.win_sample_count = 0U;
    cache->stsc.win_chunk_start = 0U;
    cache->stsc.win_chunk_count = 0U;
    cache->stsc.win_entry_start = 0U;

    if (cache->stsz.active != NULL) {
        mp4CacheFreeBlocks(&cache->stsz.active);
    }
    if (cache->stsz.free_pool != NULL) {
        mp4CacheFreeBlocks(&cache->stsz.free_pool);
    }
    cache->stsz.win_sample_count = 0U;
    cache->stsz.win_sample_start = 0U;

    if (cache->stco.active != NULL) {
        mp4CacheFreeBlocks(&cache->stco.active);
    }
    if (cache->stco.free_pool != NULL) {
        mp4CacheFreeBlocks(&cache->stco.free_pool);
    }
    cache->stco.win_chunk_count = 0U;
    cache->stco.win_chunk_start = 0U;

    if (cache->stss.active != NULL) {
        mp4CacheFreeBlocks(&cache->stss.active);
    }
    if (cache->stss.free_pool != NULL) {
        mp4CacheFreeBlocks(&cache->stss.free_pool);
    }
    cache->stss.win_entry_count = 0U;
    cache->stss.win_sample_start = 0U;
    cache->stss.win_sample_count = 0U;
    cache->stss.win_entry_start = 0U;

    if (cache->ctts.active != NULL) {
        mp4CacheFreeBlocks(&cache->ctts.active);
    }
    if (cache->ctts.free_pool != NULL) {
        mp4CacheFreeBlocks(&cache->ctts.free_pool);
    }
    cache->ctts.win_entry_count = 0U;
    cache->ctts.win_sample_start = 0U;
    cache->ctts.win_sample_count = 0U;
    cache->ctts.win_entry_start = 0U;

    mp4MemFree(cache->ctrl);
    mp4MemFree(cache);
}

/* 0x01c84c */
MP4_NOINLINE int32_t mp4CacheGetTotalDuration(Mp4CacheStbl *cache, uint32_t *out_dur)
{
    if (cache == NULL) {
        return MP4_ERR_CORRUPT;
    }
    if (cache->ctrl == NULL) {
        return MP4_ERR_NOT_READY;
    }
    *out_dur = cache->ctrl->total_duration;
    return MP4_OK;
}

/* 0x01c874 */
MP4_NOINLINE int32_t mp4CacheGetSampleWindow(
    Mp4CacheStbl *cache,
    uint32_t *out_start,
    uint32_t *out_count)
{
    if (cache == NULL) {
        return MP4_ERR_CORRUPT;
    }
    if (cache->ctrl == NULL) {
        return MP4_ERR_NOT_READY;
    }
    *out_start = cache->ctrl->win_sample_start;
    *out_count = cache->ctrl->win_sample_count;
    return MP4_OK;
}

/* 0x01c8a4 */
MP4_NOINLINE int32_t mp4StblInitDynamic(Mp4Stbl *stbl, Mp4IoStream *stream)
{
    if (stbl == NULL) {
        return MP4_ERR_NULL_OBJ;
    }

    stbl->frag_seq_num = 0U;
    stbl->frag_base_time = 0U;
    stbl->frag_moof_offset = 0ULL;

    stbl->stts.size = 8U;
    stbl->stts.tag = MP4_TAG_STTS;
    stbl->stsc.size = 8U;
    stbl->stsc.tag = MP4_TAG_STSC;
    stbl->stsz.size = 8U;
    stbl->stsz.tag = MP4_TAG_STSZ;
    stbl->stco.size = 8U;
    stbl->stco.tag = MP4_TAG_STCO;
    stbl->stss.size = 0U;
    stbl->ctts.size = 0U;

    if (mp4StblAttachStream(stbl, stream) != MP4_OK) {
        return MP4_ERR_NOMEM;
    }

    stbl->frag_ctx->is_complete = 1U;
    stbl->frag_ctx->total_samples = 0U;
    stbl->frag_ctx->total_duration = 0U;
    return MP4_OK;
}

/* 0x01c958 */
MP4_NOINLINE int32_t mp4StblAttachStream(Mp4Stbl *stbl, Mp4IoStream *stream)
{
    Mp4StblFragCtx *frag;

    if ((stbl == NULL) | (stream == NULL)) {
        return MP4_ERR_NULL_OBJ;
    }
    if (stbl->size == 0U || stbl->stsc.size == 0U || stbl->stts.size == 0U ||
        stbl->stsz.size == 0U || stbl->stco.size == 0U) {
        return MP4_ERR_CORRUPT;
    }

    frag = stbl->frag_ctx;
    if (frag == NULL) {
        frag = (Mp4StblFragCtx *)mp4MemCalloc(sizeof(Mp4StblFragCtx));
        stbl->frag_ctx = frag;
        if (frag == NULL) {
            return MP4_ERR_NOMEM;
        }
    }

    stream->max_frag_window = 2U;
    frag->stream = stream;

    if (stream->is_fragmented == 1U) {
        if (mp4StblResetFrags(stbl) != MP4_OK) {
            return MP4_ERR_INTERNAL;
        }
        frag = stbl->frag_ctx;
        frag->is_complete = 0U;
    }

    frag->frag_boundary = 1U;
    stbl->chunk_open = 1U;
    return MP4_OK;
}

/* 0x01ca4c */
MP4_NOINLINE int32_t mp4StblDetachFragCtx(Mp4Stbl *stbl)
{
    if (stbl == NULL) {
        return MP4_ERR_NULL_OBJ;
    }
    if (stbl->frag_ctx != NULL) {
        if (stbl->frag_ctx->frag_marks != NULL) {
            mp4BoxFree((void **)&stbl->frag_ctx->frag_marks, MP4_TAG_VTBL);
            stbl->frag_ctx->frag_marks = NULL;
        }
        mp4MemFree(stbl->frag_ctx);
        stbl->frag_ctx = NULL;
    }
    return MP4_OK;
}

/* 0x01cab8 */
MP4_NOINLINE int32_t mp4StblResetFrags(Mp4Stbl *stbl)
{
    Mp4StblFragCtx *frag = stbl->frag_ctx;

    if (frag == NULL) {
        return MP4_ERR_NOT_READY;
    }
    if (frag->stream->is_fragmented == 0U) {
        return MP4_ERR_NOT_SUPPORTED;
    }

    if (stbl->stts.entries != NULL) {
        mp4BoxFree((void **)&stbl->stts.entries, MP4_TAG_VTBL);
    }
    stbl->stts.entry_count = 0U;

    if (stbl->ctts.entries != NULL) {
        mp4BoxFree((void **)&stbl->ctts.entries, MP4_TAG_VTBL);
    }
    stbl->ctts.entry_count = 0U;

    if (stbl->stsc.entries != NULL) {
        mp4BoxFree((void **)&stbl->stsc.entries, MP4_TAG_VTBL);
    }
    stbl->stsc.entry_count = 0U;

    if (stbl->stsz.entries != NULL) {
        mp4BoxFree((void **)&stbl->stsz.entries, MP4_TAG_VTBL);
    }
    stbl->stsz.sample_count = 0U;
    stbl->stsz.const_sample_size = 0U;

    if (stbl->stco.entries != NULL) {
        mp4BoxFree((void **)&stbl->stco.entries, MP4_TAG_VTBL);
    }
    stbl->stco.entry_count = 0U;

    if (stbl->stss.entries != NULL) {
        mp4BoxFree((void **)&stbl->stss.entries, MP4_TAG_VTBL);
    }
    stbl->stss.entry_count = 0U;

    frag = stbl->frag_ctx;
    if (frag->frag_marks != NULL) {
        mp4BoxFree((void **)&frag->frag_marks, MP4_TAG_VTBL);
        frag = stbl->frag_ctx;
        frag->frag_marks = NULL;
    }

    frag->base_sample = 0U;
    frag->base_chunk = 0U;
    frag->frag_boundary = 1U;
    frag->base_time = stbl->frag_base_time;
    frag->next_moof_offset = stbl->frag_moof_offset;
    frag->loaded_duration = 0U;
    frag->loaded_frags = 0U;
    frag->cur_read_sample = 0U;

    if (frag->is_complete == 0U) {
        frag->total_duration = 0U;
        frag->total_samples = 0U;
    }

    frag->min_chunk = 0U;
    frag->min_sample = 0U;
    frag->min_time = 0U;

    stbl->ctts.reserved_b = 0U;
    stbl->stts.cursor_entry = 0U;
    stbl->stts.cursor_sample = 0U;
    stbl->stts.cursor_time = 0U;
    stbl->stsc.cursor_entry = 0U;
    stbl->stsc.cursor_sample = 0U;
    stbl->stsc.reserved_b = 0U;
    stbl->stss.cursor_entry = 0U;
    stbl->stss.cursor_sample = 0U;
    stbl->stss.reserved_b = 0U;
    stbl->ctts.cursor_entry = 0U;
    stbl->ctts.cursor_sample = 0U;
    return MP4_OK;
}

/* 0x01cc80 */
MP4_NOINLINE int32_t mp4StblReloadFromStart(Mp4Stbl *stbl)
{
    Mp4StblFragCtx *frag = stbl->frag_ctx;
    int32_t err;

    if (frag->stream->is_fragmented == 0U) {
        return MP4_ERR_NOT_SUPPORTED;
    }

    stbl->frag_seq_num = 0U;
    stbl->frag_base_time = 0U;
    stbl->frag_moof_offset = 0ULL;
    frag->total_duration = 0U;
    frag->total_samples = 0U;
    frag->is_complete = 0U;

    if (mp4StblResetFrags(stbl) != MP4_OK) {
        return MP4_ERR_INTERNAL;
    }

    err = mp4StblLoadNextFrag(stbl);
    if (err == MP4_ERR_EOF_FRAG) {
        return MP4_ERR_INTERNAL;
    }
    return err;
}

/* 0x01cd00 */
MP4_NOINLINE int32_t mp4StblJumpToMoof(Mp4Stbl *stbl, Mp4MoofBox *moof, uint32_t base_time)
{
    uint32_t seq_num;
    uint64_t moof_off;
    Mp4StblFragCtx *frag;
    int32_t err;

    if (stbl->frag_ctx->stream->is_fragmented == 0U) {
        return MP4_ERR_NOT_SUPPORTED;
    }

    err = mp4MoofGetSeqNum(moof, &seq_num);
    if (err != MP4_OK) {
        return err;
    }
    err = mp4MoofGetOffset(moof, &moof_off);
    if (err != MP4_OK) {
        return err;
    }

    stbl->frag_base_time = base_time;
    frag = stbl->frag_ctx;
    stbl->frag_seq_num = seq_num;
    frag->total_duration = base_time;
    stbl->frag_moof_offset = moof_off;
    frag->total_samples = 0U;
    frag->is_complete = 0U;

    if (mp4StblResetFrags(stbl) != MP4_OK) {
        return MP4_ERR_INTERNAL;
    }

    err = mp4StblLoadNextFrag(stbl);
    if (err == MP4_ERR_EOF_FRAG) {
        return MP4_ERR_INTERNAL;
    }
    return err;
}

/* 0x01cdd8 */
MP4_NOINLINE int32_t mp4StblAddRunAndCount(
    Mp4Stbl *stbl,
    uint32_t count,
    uint32_t dur,
    uint64_t offset,
    uint32_t sample_size,
    int32_t sync_flag,
    uint32_t ctts_off,
    uint32_t desc_idx)
{
    int32_t err;

    if (stbl->frag_ctx == NULL) {
        return MP4_ERR_NOT_READY;
    }
    if (stbl->frag_ctx->is_complete != 1U) {
        return MP4_ERR_INVALID_HANDLE;
    }

    err = mp4StblAddSampleRun(stbl, count, dur, offset, sample_size, sync_flag, ctts_off, desc_idx);
    if (err == MP4_OK) {
        stbl->frag_ctx->total_samples += count;
        stbl->frag_ctx->total_duration += count * dur;
    }
    return err;
}

/* 0x01ce74 */
MP4_NOINLINE int32_t mp4StblAddSampleRun(
    Mp4Stbl *stbl,
    uint32_t count,
    uint32_t dur,
    uint64_t offset,
    uint32_t sample_size,
    int32_t sync_flag,
    uint32_t ctts_off,
    uint32_t desc_idx)
{
    uint32_t mul_hi = 0U;
    uint32_t mul_lo = 0U;
    uint32_t check_val;
    uint32_t idx;

    if (count == 0U) {
        return MP4_OK;
    }
    if (stbl->frag_ctx == NULL) {
        return MP4_ERR_NOT_READY;
    }

    mp4MulU32ToU64(count, dur, &mul_hi, &mul_lo);
    if (mul_hi != 0U) {
        return MP4_ERR_DUR_OVERFLOW;
    }

    check_val = stbl->frag_ctx->loaded_duration + stbl->frag_ctx->base_time;
    if (mp4CheckedAddU32(&check_val, count * dur) != MP4_OK) {
        return MP4_ERR_DUR_OVERFLOW;
    }

    check_val = stbl->frag_ctx->base_sample + stbl->stsz.sample_count;
    if (mp4CheckedAddU32(&check_val, count) != MP4_OK) {
        return MP4_ERR_COUNT_OVERFLOW;
    }

    /* Update stss sync table */
    if (sync_flag == 2 && count != 1U) {
        if (stbl->stss.size == 0U) {
            uint32_t end_prev = stbl->frag_ctx->base_sample + stbl->stsz.sample_count;
            stbl->stss.size = 16U;
            stbl->stss.tag = MP4_TAG_STSS;
            for (idx = stbl->frag_ctx->base_sample; idx < end_prev; idx++) {
                if (mp4StssSetEntry(&stbl->stss, stbl->stss.entry_count, idx + 1U) != MP4_OK) {
                    return MP4_ERR_NOMEM;
                }
            }
        }
        if (mp4StssSetEntry(&stbl->stss, stbl->stss.entry_count,
                            stbl->frag_ctx->base_sample + stbl->stsz.sample_count + 1U) != MP4_OK) {
            return MP4_ERR_NOMEM;
        }
    } else if ((sync_flag == 2 && count == 1U) || sync_flag == 0) {
        if (stbl->stss.size != 0U) {
            for (idx = 0U; idx < count; idx++) {
                if (mp4StssSetEntry(&stbl->stss, stbl->stss.entry_count,
                                    stbl->stsz.sample_count + stbl->frag_ctx->base_sample + idx + 1U) != MP4_OK) {
                    return MP4_ERR_NOMEM;
                }
            }
        }
    } else if (sync_flag == 1) {
        if (stbl->stss.size == 0U) {
            uint32_t end_prev = stbl->frag_ctx->base_sample + stbl->stsz.sample_count;
            stbl->stss.size = 16U;
            stbl->stss.tag = MP4_TAG_STSS;
            for (idx = stbl->frag_ctx->base_sample; idx < end_prev; idx++) {
                if (mp4StssSetEntry(&stbl->stss, stbl->stss.entry_count, idx + 1U) != MP4_OK) {
                    return MP4_ERR_NOMEM;
                }
            }
        }
    }

    /* Update stco if starting a new chunk */
    if (stbl->chunk_open == 1U) {
        if (mp4StcoSetEntry(&stbl->stco, stbl->stco.entry_count, offset) != MP4_OK) {
            return MP4_ERR_NOMEM;
        }
    }

    /* Append sample sizes to stsz */
    if (stbl->stsz.tag == MP4_TAG_STSZ) {
        for (idx = 0U; idx < count; idx++) {
            if (mp4StszSetEntry(&stbl->stsz, stbl->stsz.sample_count, sample_size) != MP4_OK) {
                return MP4_ERR_NOMEM;
            }
        }
    }

    /* Update stts */
    if (stbl->stts.entry_count != 0U &&
        mp4SttsGetWord(&stbl->stts, stbl->stts.entry_count - 1U, 1U) == dur &&
        !(stbl->frag_ctx->frag_boundary == 1U && stbl->chunk_open == 1U)) {
        uint32_t prev_cnt = mp4SttsGetWord(&stbl->stts, stbl->stts.entry_count - 1U, 0U);
        if (mp4SttsSetEntry(&stbl->stts, stbl->stts.entry_count - 1U, 0U, prev_cnt + count) != MP4_OK) {
            return MP4_ERR_NOMEM;
        }
    } else {
        if (mp4SttsSetEntry(&stbl->stts, stbl->stts.entry_count, 0U, count) != MP4_OK) {
            return MP4_ERR_NOMEM;
        }
        if (mp4SttsSetEntry(&stbl->stts, stbl->stts.entry_count - 1U, 1U, dur) != MP4_OK) {
            return MP4_ERR_NOMEM;
        }
    }

    /* Update stsc */
    if (stbl->chunk_open == 1U) {
        if (mp4StscSetEntry(&stbl->stsc, stbl->stsc.entry_count, 0U,
                            stbl->stco.entry_count + stbl->frag_ctx->base_chunk) != MP4_OK) {
            return MP4_ERR_NOMEM;
        }
        if (mp4StscSetEntry(&stbl->stsc, stbl->stsc.entry_count - 1U, 1U, count) != MP4_OK) {
            return MP4_ERR_NOMEM;
        }
        if (mp4StscSetEntry(&stbl->stsc, stbl->stsc.entry_count - 1U, 2U, desc_idx) != MP4_OK) {
            return MP4_ERR_NOMEM;
        }
    } else {
        uint32_t prev_spc = mp4StscGetWord(&stbl->stsc, stbl->stsc.entry_count - 1U, 1U);
        if (mp4StscSetEntry(&stbl->stsc, stbl->stsc.entry_count - 1U, 1U, prev_spc + count) != MP4_OK) {
            return MP4_ERR_NOMEM;
        }
    }

    /* Update ctts */
    if (stbl->ctts.size == 0U) {
        stbl->ctts.size = 16U;
        stbl->ctts.tag = MP4_TAG_CTTS;
    }
    if (!(stbl->frag_ctx->frag_boundary == 1U && stbl->chunk_open == 1U) &&
        stbl->ctts.entry_count != 0U &&
        mp4CttsGetWord(&stbl->ctts, stbl->ctts.entry_count - 1U, 1U) == ctts_off) {
        uint32_t prev_cnt = mp4CttsGetWord(&stbl->ctts, stbl->ctts.entry_count - 1U, 0U);
        mp4CttsSetEntry(&stbl->ctts, stbl->ctts.entry_count - 1U, 0U, prev_cnt + count);
    } else {
        if (mp4CttsSetEntry(&stbl->ctts, stbl->ctts.entry_count, 0U, count) != MP4_OK) {
            return MP4_ERR_NOMEM;
        }
        mp4CttsSetEntry(&stbl->ctts, stbl->ctts.entry_count - 1U, 1U, ctts_off);
    }

    stbl->chunk_open = 0U;
    stbl->frag_ctx->loaded_duration += count * dur;
    return MP4_OK;
}

/* 0x01d444 */
MP4_NOINLINE int32_t mp4StblEndChunk(Mp4Stbl *stbl, int32_t mark_frag)
{
    Mp4StblFragCtx *frag;
    uint32_t mark_triple[3];
    int32_t err;

    if (stbl->chunk_open == 1U) {
        return MP4_ERR_CHUNK_EMPTY;
    }
    frag = stbl->frag_ctx;
    if (frag == NULL) {
        return MP4_ERR_NOT_READY;
    }
    if (stbl->stsz.sample_count == 0U) {
        return MP4_ERR_CHUNK_EMPTY;
    }

    if (stbl->stsc.entry_count >= 2U) {
        uint32_t prev_spc = mp4StscGetWord(&stbl->stsc, stbl->stsc.entry_count - 2U, 1U);
        uint32_t cur_spc = mp4StscGetWord(&stbl->stsc, stbl->stsc.entry_count - 1U, 1U);

        if (prev_spc == cur_spc) {
            uint32_t prev_desc = mp4StscGetWord(&stbl->stsc, stbl->stsc.entry_count - 2U, 2U);
            uint32_t cur_desc = mp4StscGetWord(&stbl->stsc, stbl->stsc.entry_count - 1U, 2U);

            frag = stbl->frag_ctx;
            if (prev_desc == cur_desc && frag->frag_boundary == 0U) {
                if (stbl->stsc.cursor_entry == stbl->stsc.entry_count - 1U) {
                    uint32_t prev_fc = mp4StscGetWord(&stbl->stsc, stbl->stsc.entry_count - 2U, 0U);
                    uint32_t prev_s = mp4StscGetWord(&stbl->stsc, stbl->stsc.entry_count - 2U, 1U);
                    uint32_t cur_fc = mp4StscGetWord(&stbl->stsc, stbl->stsc.entry_count - 1U, 0U);
                    stbl->stsc.cursor_entry--;
                    stbl->stsc.cursor_sample -= (cur_fc - prev_fc) * prev_s;
                }
                if (mp4ChunkArrayEraseU32(&stbl->stsc.entries,
                                          stbl->stsc.entry_count - 1U,
                                          stbl->stsc.entry_count - 1U, 3U) != MP4_OK) {
                    return MP4_ERR_NOMEM;
                }
                frag = stbl->frag_ctx;
                stbl->stsc.entry_count--;
            }
        }
    }

    stbl->chunk_open = 1U;
    if (mark_frag != 1) {
        stbl->frag_ctx->frag_boundary = 0U;
        return MP4_OK;
    }

    stbl->frag_ctx->frag_boundary = 1U;
    frag = stbl->frag_ctx;
    frag->loaded_frags++;
    mark_triple[0] = 0U;
    mark_triple[1] = 0U;
    mark_triple[2] = stbl->stco.entry_count + frag->base_chunk - 1U;
    err = mp4ChunkArrayAppendU32(&frag->frag_marks, 3U, mark_triple, 2U);
    if (err != MP4_OK) {
        return err;
    }

    if (stbl->frag_ctx->stream->max_frag_window >= stbl->frag_ctx->loaded_frags) {
        return MP4_OK;
    }
    if (mp4StblEvictFrags(stbl, 1U) != MP4_OK) {
        return MP4_ERR_INTERNAL;
    }
    if (mp4StblUpdateMinBounds(stbl) != MP4_OK) {
        return MP4_ERR_INTERNAL;
    }
    return MP4_OK;
}

/* 0x01d68c */
MP4_NOINLINE int32_t mp4StblAdvanceFrags(Mp4Stbl *stbl)
{
    Mp4StblFragCtx *frag;
    uint32_t free_slots;
    uint32_t to_load = 1U;
    uint32_t idx;
    int32_t err;

    if (stbl == NULL || stbl->frag_ctx == NULL) {
        return MP4_ERR_NOT_READY;
    }

    frag = stbl->frag_ctx;
    free_slots = frag->stream->max_frag_window - frag->loaded_frags;
    if (free_slots == 0U) {
        uint32_t evictable = frag->loaded_frags;
        if (frag->cur_read_sample != frag->base_sample + stbl->stsz.sample_count) {
            uint32_t cur_chunk = 0U;
            uint32_t chunk_first = 0U;
            uint32_t frag_idx = 0U;

            mp4StblFindChunkForSample(stbl, frag->cur_read_sample, &cur_chunk, &chunk_first);
            evictable = 0U;
            for (frag_idx = 0U; frag_idx < stbl->frag_ctx->loaded_frags; frag_idx++) {
                uint32_t mark_chunk = 0U;
                mp4ChunkArrayGetU32(stbl->frag_ctx->frag_marks, frag_idx, 2U, &mark_chunk);
                if (mark_chunk >= cur_chunk) {
                    evictable = frag_idx;
                    break;
                }
            }
        }
        to_load = evictable + free_slots;
        if (to_load >= 2U) {
            to_load = 1U;
        }
        if (to_load == 0U) {
            return MP4_ERR_NO_NEXT_FRAG;
        }
    }

    err = MP4_OK;
    for (idx = 0U; idx < to_load; idx++) {
        err = mp4StblLoadNextFrag(stbl);
        if (err != MP4_OK) {
            break;
        }
    }
    return err;
}

/* 0x01d7c0 */
MP4_NOINLINE int32_t mp4StblLoadNextFrag(Mp4Stbl *stbl)
{
    Mp4StblFragCtx *frag;
    uint32_t added_samples = 0U;
    uint32_t seq_num = 0U;
    uint64_t next_off = 0ULL;
    uint32_t mark_triple[3];
    int32_t err;

    if (stbl == NULL || stbl->frag_ctx == NULL) {
        return MP4_ERR_NOT_READY;
    }

    frag = stbl->frag_ctx;
    if (frag->stream->is_fragmented == 0U) {
        return MP4_ERR_EOF_FRAG;
    }

    mp4AttachStream(frag->stream);
    frag = stbl->frag_ctx;

    if (frag->next_moof_offset != 0ULL) {
        Mp4MoofBox *moof = NULL;
        uint32_t track_id = 0U;
        Mp4TrexBox *trex = NULL;
        uint32_t traf_cnt = 0U;
        uint32_t traf_idx;

        err = mp4MovieFindMoofByOffset(stbl->track->movie->self, frag->stream,
                                       frag->next_moof_offset, &moof, &next_off);
        if (err == MP4_ERR_NO_ENTRY) {
            if (mp4FsmIsFragReadOnly(mp4StblGetFsm(stbl))) {
                err = MP4_ERR_EOF_FRAG;
            } else {
                err = mp4MoofReadNext(stbl->track->movie->self, stbl->frag_ctx->stream,
                                      stbl->frag_ctx->next_moof_offset, &moof, &next_off);
            }
            if (err != MP4_OK) {
                if (err == MP4_ERR_EOF_FRAG) {
                    frag = stbl->frag_ctx;
                    frag->is_complete = 1U;
                    frag->total_samples = frag->base_sample + stbl->stsz.sample_count;
                    frag->total_duration = frag->base_time + frag->loaded_duration;
                }
                return err;
            }
        }

        if (stbl->frag_ctx->loaded_frags == stbl->frag_ctx->stream->max_frag_window) {
            if (mp4StblEvictFrags(stbl, 1U) != MP4_OK) {
                return MP4_ERR_INTERNAL;
            }
        }

        mp4TrackGetId(stbl->track, &track_id);
        mp4TrackGetTrex(stbl->track, &trex);
        if (mp4MoofCountTrafForTrack(moof, track_id, &traf_cnt) != MP4_OK) {
            return MP4_ERR_INTERNAL;
        }

        if (traf_cnt == 0U) {
            stbl->frag_ctx->next_moof_offset = next_off;
            return MP4_OK;
        }

        for (traf_idx = 0U; traf_idx < traf_cnt; traf_idx++) {
            Mp4TrafBox *traf = NULL;
            uint32_t trun_idx = 0U;
            uint32_t traf_sample_idx = 0U;

            if (mp4MoofGetTrafForTrack(moof, track_id, traf_idx, &traf) != MP4_OK) {
                return MP4_ERR_INTERNAL;
            }

            for (;;) {
                uint32_t trun_samples = 0U;
                uint32_t smp_idx;

                if (mp4TrafGetTrunSampleCount(traf, trun_idx, &trun_samples) != MP4_OK) {
                    break;
                }
                for (smp_idx = 0U; smp_idx < trun_samples; smp_idx++) {
                    Mp4SampleInfo sinfo;
                    uint32_t desc_idx = 0U;
                    int32_t detail_err;

                    detail_err = mp4TrafGetSampleDetail(traf, trex, traf_sample_idx++, &sinfo, &desc_idx);
                    if (detail_err != MP4_OK) {
                        if (mp4StblResetFrags(stbl) != MP4_OK) {
                            return MP4_ERR_INTERNAL;
                        }
                        if (detail_err == MP4_ERR_OVERFLOW) {
                            return MP4_ERR_FRAG_OVERFLOW;
                        }
                        if (detail_err == MP4_ERR_CHUNK_EMPTY) {
                            return MP4_ERR_CHUNK_EMPTY;
                        }
                        return MP4_ERR_CORRUPT;
                    }

                    err = mp4StblAddSampleRun(stbl, 1U, sinfo.duration, sinfo.offset,
                                              sinfo.size, (sinfo.is_sync != 1U) ? 1 : 0,
                                              sinfo.ctts_offset, desc_idx);
                    if (err != MP4_OK) {
                        if (mp4StblResetFrags(stbl) != MP4_OK) {
                            return MP4_ERR_INTERNAL;
                        }
                        return err;
                    }
                }
                if (trun_samples != 0U) {
                    mp4StblEndChunk(stbl, 0);
                    added_samples += trun_samples;
                }
                trun_idx++;
            }
        }

        if (mp4MoofGetSeqNum(moof, &seq_num) != MP4_OK) {
            return MP4_ERR_INTERNAL;
        }
    } else {
        Mp4Stbl *src_stbl = NULL;
        uint32_t src_samples = 0U;
        uint32_t smp_idx = 0U;

        mp4TrackGetActiveStbl(stbl->track, &src_stbl);
        if (src_stbl == NULL || src_stbl->stts.size == 0U || src_stbl->stsc.size == 0U ||
            src_stbl->stsz.size == 0U || src_stbl->stco.size == 0U) {
            return MP4_ERR_CORRUPT;
        }
        if (mp4StblGetSampleCount(src_stbl, &src_samples, 0) != MP4_OK) {
            return MP4_ERR_CORRUPT;
        }

        while (smp_idx < src_samples) {
            Mp4SampleInfo sinfo;
            uint32_t chunk_rem;
            uint32_t chunk_step;

            if (mp4StblGetSampleInfo(src_stbl, smp_idx, 1, &sinfo, 0) != MP4_OK) {
                return MP4_ERR_CORRUPT;
            }
            chunk_rem = sinfo.chunk_rem_samples;
            for (chunk_step = 0U; chunk_step < chunk_rem; chunk_step++) {
                if (chunk_step != 0U) {
                    if (mp4StblGetSampleInfo(src_stbl, smp_idx, 0, &sinfo, 0) != MP4_OK) {
                        return MP4_ERR_CORRUPT;
                    }
                }
                smp_idx++;
                err = mp4StblAddSampleRun(stbl, 1U, sinfo.duration, sinfo.offset,
                                          sinfo.size, (sinfo.is_sync != 1U) ? 1 : 0,
                                          sinfo.ctts_offset, sinfo.sample_desc);
                if (err != MP4_OK) {
                    return err;
                }
            }
            mp4StblEndChunk(stbl, 0);
        }

        added_samples = src_samples;
        seq_num = 0U;
        next_off = stbl->track->movie->first_moof_offset;
    }

    frag = stbl->frag_ctx;
    frag->next_moof_offset = next_off;
    frag->frag_boundary = 1U;
    stbl->chunk_open = 1U;
    if (added_samples != 0U) {
        frag->loaded_frags++;
        mark_triple[0] = 1U;
        mark_triple[1] = seq_num;
        mark_triple[2] = stbl->stco.entry_count + frag->base_chunk - 1U;
        err = mp4ChunkArrayAppendU32(&frag->frag_marks, 3U, mark_triple, 2U);
        if (err != MP4_OK) {
            return err;
        }
    }
    return MP4_OK;
}

/* 0x01dcdc */
MP4_NOINLINE int32_t mp4StblEvictFrags(Mp4Stbl *stbl, uint32_t evict_count)
{
    Mp4StblFragCtx *frag = stbl->frag_ctx;
    uint32_t evict_last_chunk = 0U;
    uint32_t chunk_first_sample = 0U;
    uint32_t chunk_spc = 0U;
    uint32_t desc_idx = 0U;
    uint32_t evict_last_sample;
    uint32_t evict_sample_cnt;
    uint32_t stts_last_row = 0U;
    uint32_t ctts_last_row = 0U;
    uint32_t stsc_last_row = 0U;
    uint32_t stss_last_row = 0U;
    uint32_t evict_dur = 0U;
    uint32_t rem_samples;
    uint32_t row;

    if (frag == NULL) {
        return MP4_ERR_NOT_READY;
    }
    if (frag->loaded_frags < evict_count) {
        evict_count = frag->loaded_frags;
    }
    if (evict_count == 0U) {
        return MP4_ERR_CHUNK_EMPTY;
    }

    if (mp4ChunkArrayGetU32(frag->frag_marks, evict_count - 1U, 2U, &evict_last_chunk) != MP4_OK) {
        return MP4_ERR_EVICT_FAIL;
    }
    if (mp4StblGetChunkInfo(stbl, evict_last_chunk, &chunk_first_sample, &chunk_spc, &desc_idx) != MP4_OK) {
        return MP4_ERR_EVICT_FAIL;
    }

    evict_sample_cnt = chunk_first_sample + chunk_spc - stbl->frag_ctx->base_sample;
    evict_last_sample = chunk_first_sample + chunk_spc - 1U;

    if (stbl->stts.size == 0U || stbl->stsc.size == 0U ||
        stbl->stsz.size == 0U || stbl->stco.size == 0U) {
        return MP4_ERR_EVICT_FAIL;
    }

    rem_samples = evict_sample_cnt;
    if (mp4ChunkArrayAccumFind(stbl->stts.entries, &rem_samples, 0U, &stts_last_row) != MP4_OK) {
        return MP4_ERR_EVICT_FAIL;
    }

    for (row = 0U; row <= stts_last_row; row++) {
        uint32_t run_samples = 0U;
        uint32_t run_delta = 0U;
        mp4ChunkArrayGetU32(stbl->stts.entries, row, 0U, &run_samples);
        mp4ChunkArrayGetU32(stbl->stts.entries, row, 1U, &run_delta);
        evict_dur += run_samples * run_delta;
    }

    mp4ChunkArrayEraseU32(&stbl->stts.entries, 0U, stts_last_row, 2U);
    stbl->stts.entry_count = stbl->stts.entry_count - stts_last_row - 1U;

    if (stbl->ctts.size != 0U) {
        rem_samples = evict_sample_cnt;
        if (mp4ChunkArrayAccumFind(stbl->ctts.entries, &rem_samples, 0U, &ctts_last_row) != MP4_OK) {
            return MP4_ERR_EVICT_FAIL;
        }
        mp4ChunkArrayEraseU32(&stbl->ctts.entries, 0U, ctts_last_row, 2U);
        stbl->ctts.entry_count = stbl->ctts.entry_count - ctts_last_row - 1U;
    }

    if (mp4ChunkArrayFindGe(stbl->stsc.entries, evict_last_chunk + 1U, 0U, &stsc_last_row) != MP4_OK) {
        return MP4_ERR_EVICT_FAIL;
    }
    mp4ChunkArrayEraseU32(&stbl->stsc.entries, 0U, stsc_last_row, 3U);
    stbl->stsc.entry_count = stbl->stsc.entry_count - stsc_last_row - 1U;

    if (stbl->stsz.const_sample_size == 0U) {
        mp4ChunkArrayEraseU32(&stbl->stsz.entries, 0U, evict_sample_cnt - 1U, 1U);
    }
    stbl->stsz.sample_count -= evict_sample_cnt;

    mp4ChunkArrayEraseU64(&stbl->stco.entries, 0U, evict_last_chunk - stbl->frag_ctx->base_chunk, 1U);
    frag = stbl->frag_ctx;
    stbl->stco.entry_count = stbl->stco.entry_count + frag->base_chunk - evict_last_chunk - 1U;

    if (stbl->stss.size != 0U && stbl->stss.entries != NULL) {
        if (mp4ChunkArrayFindGe(stbl->stss.entries, evict_last_sample + 1U, 0U, &stss_last_row) == MP4_OK) {
            mp4ChunkArrayEraseU32(&stbl->stss.entries, 0U, stss_last_row, 1U);
            stbl->stss.entry_count = stbl->stss.entry_count - stss_last_row - 1U;
        }
    }

    frag = stbl->frag_ctx;
    frag->base_sample = evict_last_sample + 1U;
    frag->base_time += evict_dur;
    frag->base_chunk = evict_last_chunk + 1U;
    frag->loaded_duration -= evict_dur;
    frag->loaded_frags -= evict_count;
    mp4ChunkArrayEraseU32(&frag->frag_marks, 0U, evict_count - 1U, 3U);

    frag = stbl->frag_ctx;
    if (frag->cur_read_sample < frag->base_sample ||
        frag->cur_read_sample > frag->base_sample + stbl->stsz.sample_count) {
        frag->cur_read_sample = frag->base_sample;
    }

    if (stts_last_row < stbl->stts.cursor_entry) {
        stbl->stts.cursor_entry = stbl->stts.cursor_entry - stts_last_row - 1U;
    } else {
        stbl->stts.cursor_entry = 0U;
    }
    if (stsc_last_row < stbl->stsc.cursor_entry) {
        stbl->stsc.cursor_entry = stbl->stsc.cursor_entry - stsc_last_row - 1U;
    } else {
        stbl->stsc.cursor_entry = 0U;
    }
    if (stss_last_row < stbl->stss.cursor_entry) {
        stbl->stss.cursor_entry = stbl->stss.cursor_entry - stss_last_row - 1U;
    } else {
        stbl->stss.cursor_entry = 0U;
    }
    if (ctts_last_row < stbl->ctts.cursor_entry) {
        stbl->ctts.cursor_entry = stbl->ctts.cursor_entry - ctts_last_row - 1U;
    } else {
        stbl->ctts.cursor_entry = 0U;
    }
    return MP4_OK;
}

/* 0x01e0f4 */
MP4_NOINLINE int32_t mp4StblGetChunkInfo(
    Mp4Stbl *stbl,
    uint32_t chunk_idx,
    uint32_t *out_first_sample,
    uint32_t *out_samples_per_chunk,
    uint32_t *out_desc_idx)
{
    Mp4StblFragCtx *frag = stbl->frag_ctx;
    uint32_t base_chunk = frag->base_chunk;
    uint32_t cur_entry;
    uint32_t cur_chunk_zero;

    if (chunk_idx >= base_chunk + stbl->stco.entry_count || chunk_idx < base_chunk) {
        return MP4_ERR_OUT_OF_CHUNKS;
    }
    if (stbl->stsc.entry_count == 0U) {
        return MP4_ERR_OUT_OF_CHUNKS;
    }

    if (stbl->stsc.cursor_entry == 0U) {
        stbl->stsc.cursor_sample = frag->base_sample;
    }

    cur_chunk_zero = mp4StscGetWord(&stbl->stsc, stbl->stsc.cursor_entry, 0U) - 1U;
    if (chunk_idx < cur_chunk_zero) {
        if (chunk_idx - stbl->frag_ctx->base_chunk < cur_chunk_zero - chunk_idx) {
            stbl->stsc.cursor_entry = 0U;
            stbl->stsc.cursor_sample = stbl->frag_ctx->base_sample;
        }
    } else if (cur_chunk_zero < chunk_idx) {
        uint32_t last_entry = stbl->stsc.entry_count - 1U;
        if (stbl->stsc.cursor_entry != last_entry &&
            stbl->frag_ctx->base_chunk + stbl->stco.entry_count - chunk_idx < chunk_idx - cur_chunk_zero) {
            uint32_t total_chunks_plus = stbl->frag_ctx->base_chunk + stbl->stco.entry_count + 1U;
            uint32_t last_fc = mp4StscGetWord(&stbl->stsc, last_entry, 0U);
            uint32_t last_spc = mp4StscGetWord(&stbl->stsc, last_entry, 1U);
            stbl->stsc.cursor_entry = last_entry;
            stbl->stsc.cursor_sample = stbl->frag_ctx->base_sample + stbl->stsz.sample_count -
                                       (total_chunks_plus - last_fc) * last_spc;
        }
    }

    cur_entry = stbl->stsc.cursor_entry;
    cur_chunk_zero = mp4StscGetWord(&stbl->stsc, cur_entry, 0U) - 1U;

    if (chunk_idx >= cur_chunk_zero) {
        uint32_t scan = cur_entry;
        uint32_t chunk_zero = cur_chunk_zero;
        uint32_t acc_sample = stbl->stsc.cursor_sample;

        while (scan < stbl->stsc.entry_count) {
            uint32_t next_chunk_zero;
            uint32_t spc;

            if (scan == stbl->stsc.entry_count - 1U) {
                next_chunk_zero = stbl->frag_ctx->base_chunk + stbl->stco.entry_count;
            } else {
                next_chunk_zero = mp4StscGetWord(&stbl->stsc, scan + 1U, 0U) - 1U;
            }
            spc = mp4StscGetWord(&stbl->stsc, scan, 1U);
            if (chunk_idx < next_chunk_zero) {
                *out_first_sample = acc_sample + (chunk_idx - chunk_zero) * spc;
                *out_samples_per_chunk = spc;
                *out_desc_idx = mp4StscGetWord(&stbl->stsc, scan, 2U);
                stbl->stsc.cursor_sample = acc_sample;
                stbl->stsc.cursor_entry = scan;
                return MP4_OK;
            }
            acc_sample += (next_chunk_zero - chunk_zero) * spc;
            chunk_zero = next_chunk_zero;
            scan++;
        }
        return MP4_ERR_CORRUPT;
    }

    if (cur_entry == 0U) {
        return MP4_ERR_CORRUPT;
    }

    {
        uint32_t next_chunk_zero = cur_chunk_zero;
        uint32_t acc_sample = stbl->stsc.cursor_sample;
        uint32_t scan = cur_entry - 1U;

        for (;;) {
            uint32_t chunk_zero = mp4StscGetWord(&stbl->stsc, scan, 0U) - 1U;
            uint32_t spc = mp4StscGetWord(&stbl->stsc, scan, 1U);
            acc_sample -= (next_chunk_zero - chunk_zero) * spc;
            if (chunk_idx >= chunk_zero) {
                *out_first_sample = acc_sample + (chunk_idx - chunk_zero) * spc;
                *out_samples_per_chunk = spc;
                *out_desc_idx = mp4StscGetWord(&stbl->stsc, scan, 2U);
                stbl->stsc.cursor_sample = acc_sample;
                stbl->stsc.cursor_entry = scan;
                return MP4_OK;
            }
            if (scan == 0U) {
                return MP4_ERR_CORRUPT;
            }
            next_chunk_zero = chunk_zero;
            scan--;
        }
    }
}

/* 0x01e43c */
MP4_NOINLINE int32_t mp4StblFindChunkForSample(
    Mp4Stbl *stbl,
    uint32_t sample_idx,
    uint32_t *out_chunk,
    uint32_t *out_chunk_first_sample)
{
    Mp4StblFragCtx *frag = stbl->frag_ctx;
    uint32_t base_sample = frag->base_sample;
    uint32_t cur_entry;
    uint32_t cur_sample;
    uint32_t found_entry = 0U;
    uint32_t found_sample = 0U;
    uint32_t found_chunk_base = 0U;
    uint32_t found_spc = 0U;

    if (sample_idx >= base_sample + stbl->stsz.sample_count || sample_idx < base_sample) {
        return MP4_ERR_SAMPLE_RANGE;
    }
    if (stbl->stsc.size == 0U || stbl->stco.size == 0U) {
        return MP4_ERR_CORRUPT;
    }

    cur_entry = stbl->stsc.cursor_entry;
    if (cur_entry == 0U) {
        stbl->stsc.cursor_sample = base_sample;
    }
    cur_sample = stbl->stsc.cursor_sample;

    if (sample_idx < cur_sample) {
        if (sample_idx - base_sample < cur_sample - sample_idx) {
            cur_entry = 0U;
            cur_sample = base_sample;
            stbl->stsc.cursor_entry = 0U;
            stbl->stsc.cursor_sample = base_sample;
        }
    } else if (base_sample + stbl->stsz.sample_count - sample_idx < sample_idx - cur_sample) {
        uint32_t last_entry = stbl->stsc.entry_count - 1U;
        if (cur_entry != last_entry) {
            uint32_t total_chunks_plus = frag->base_chunk + stbl->stco.entry_count + 1U;
            uint32_t last_fc = mp4StscGetWord(&stbl->stsc, last_entry, 0U);
            uint32_t last_spc = mp4StscGetWord(&stbl->stsc, last_entry, 1U);
            stbl->stsc.cursor_entry = last_entry;
            cur_entry = last_entry;
            cur_sample = stbl->frag_ctx->base_sample + stbl->stsz.sample_count -
                         (total_chunks_plus - last_fc) * last_spc;
            stbl->stsc.cursor_sample = cur_sample;
        }
    }

    if (sample_idx < cur_sample) {
        uint32_t next_fc = mp4StscGetWord(&stbl->stsc, cur_entry, 0U);
        uint32_t acc_sample = cur_sample;
        uint32_t scan = cur_entry - 1U;

        for (;;) {
            uint32_t first_chunk = mp4StscGetWord(&stbl->stsc, scan, 0U);
            uint32_t spc = mp4StscGetWord(&stbl->stsc, scan, 1U);
            acc_sample -= (next_fc - first_chunk) * spc;
            if (sample_idx >= acc_sample) {
                found_entry = scan;
                found_sample = acc_sample;
                found_chunk_base = first_chunk - 1U;
                found_spc = spc;
                break;
            }
            if (scan == 0U) {
                return MP4_ERR_CORRUPT;
            }
            next_fc = first_chunk;
            scan--;
        }
    } else {
        uint32_t cur_fc = mp4StscGetWord(&stbl->stsc, cur_entry, 0U);
        uint32_t acc_sample = cur_sample;
        uint32_t scan = cur_entry;

        if (scan >= stbl->stsc.entry_count) {
            return MP4_ERR_CORRUPT;
        }

        while (scan < stbl->stsc.entry_count) {
            uint32_t next_fc = 0U;
            uint32_t spc;
            uint32_t span_samples;

            if (scan == stbl->stsc.entry_count - 1U) {
                mp4StblGetChunkCount(stbl, &next_fc);
                next_fc++;
            } else {
                next_fc = mp4StscGetWord(&stbl->stsc, scan + 1U, 0U);
            }
            spc = mp4StscGetWord(&stbl->stsc, scan, 1U);
            span_samples = (next_fc - cur_fc) * spc;
            if (sample_idx < acc_sample + span_samples) {
                found_entry = scan;
                found_sample = acc_sample;
                found_chunk_base = cur_fc - 1U;
                found_spc = spc;
                break;
            }
            acc_sample += span_samples;
            cur_fc = next_fc;
            scan++;
        }

        if (scan >= stbl->stsc.entry_count) {
            return MP4_ERR_CORRUPT;
        }
    }

    {
        uint32_t chunk_off = (sample_idx - found_sample) / found_spc;
        *out_chunk = found_chunk_base + chunk_off;
        *out_chunk_first_sample = found_sample + chunk_off * found_spc;
        stbl->stsc.cursor_sample = found_sample;
        stbl->stsc.cursor_entry = found_entry;
    }
    return MP4_OK;
}

/* 0x01e7b4 */
MP4_NOINLINE int32_t mp4StblGetSampleSize(
    Mp4Stbl *stbl,
    uint32_t sample_idx,
    uint32_t *out_size,
    int32_t allow_io)
{
    int32_t err;

    if (mp4FsmIsCacheMode(mp4StblGetFsm(stbl)) == 1) {
        return mp4CacheGetSampleSize(stbl->cache, sample_idx, out_size, allow_io);
    }
    if (stbl->stsz.size == 0U) {
        return MP4_ERR_CORRUPT;
    }
    if (stbl->frag_ctx == NULL) {
        return MP4_ERR_NOT_READY;
    }
    if (sample_idx < stbl->frag_ctx->min_sample) {
        return MP4_ERR_SAMPLE_RANGE;
    }

    err = mp4StblEnsureSample(stbl, sample_idx, allow_io);
    if (err == MP4_OK) {
        return mp4StblLookupSampleSize(stbl, sample_idx, out_size);
    }
    return err;
}

/* 0x01e88c */
MP4_NOINLINE uint32_t mp4StscGetWord(Mp4StscBox *stsc, uint32_t row, uint32_t col)
{
    uint32_t val;

    if (stsc == NULL) {
        return 0xFFFFFFFFU;
    }
    return (mp4ChunkArrayGetU32(stsc->entries, row, col, &val) == MP4_OK) ? val : 0xFFFFFFFFU;
}

/* 0x01e8c4 */
MP4_NOINLINE uint32_t mp4SttsGetWord(Mp4SttsBox *stts, uint32_t row, uint32_t col)
{
    uint32_t val;

    if (stts == NULL) {
        return 0xFFFFFFFFU;
    }
    return (mp4ChunkArrayGetU32(stts->entries, row, col, &val) == MP4_OK) ? val : 0xFFFFFFFFU;
}

/* 0x01e8fc */
MP4_NOINLINE uint32_t mp4CttsGetWord(Mp4CttsBox *ctts, uint32_t row, uint32_t col)
{
    uint32_t val;

    if (ctts == NULL) {
        return 0xFFFFFFFFU;
    }
    return (mp4ChunkArrayGetU32(ctts->entries, row, col, &val) == MP4_OK) ? val : 0xFFFFFFFFU;
}

/* 0x01e934 */
MP4_NOINLINE int32_t mp4StszSetEntry(Mp4StszBox *stsz, uint32_t index, uint32_t size_val)
{
    if (stsz == NULL) {
        return MP4_ERR_NULL_OBJ;
    }
    if (index >= stsz->sample_count) {
        return mp4StszAppend(stsz, size_val);
    }
    if (stsz->const_sample_size == 0U) {
        return mp4ChunkArraySetU32(&stsz->entries, index, 1U, 0U, size_val);
    }
    if (stsz->const_sample_size != size_val) {
        if (stsz->sample_count < 2U) {
            stsz->const_sample_size = size_val;
            return MP4_OK;
        }
        if (mp4StszExpandConst(stsz) != MP4_OK) {
            return MP4_ERR_NOMEM;
        }
        return mp4ChunkArraySetU32(&stsz->entries, index, 1U, 0U, size_val);
    }
    return MP4_OK;
}

/* 0x01e9ec */
MP4_NOINLINE int32_t mp4StcoSetEntry(Mp4StcoBox *stco, uint32_t index, uint64_t offset_val)
{
    if (stco == NULL) {
        return MP4_ERR_NULL_OBJ;
    }
    if (index < stco->entry_count) {
        return mp4ChunkArraySetU64(&stco->entries, index, 1U, 0U, offset_val);
    }
    return mp4StcoAppend(stco, offset_val);
}

/* 0x01ea40 */
MP4_NOINLINE int32_t mp4StscSetEntry(Mp4StscBox *stsc, uint32_t index, uint32_t col, uint32_t val)
{
    uint32_t triple[3];

    if (stsc == NULL) {
        return MP4_ERR_NULL_OBJ;
    }
    if (index < stsc->entry_count) {
        return mp4ChunkArraySetU32(&stsc->entries, index, 3U, col, val);
    }
    triple[0] = 0U;
    triple[1] = 0U;
    triple[2] = 0U;
    triple[col] = val;
    return mp4StscAppend(stsc, triple);
}

/* 0x01eaac */
MP4_NOINLINE int32_t mp4SttsSetEntry(Mp4SttsBox *stts, uint32_t index, uint32_t col, uint32_t val)
{
    uint32_t pair[2];

    if (stts == NULL) {
        return MP4_ERR_NULL_OBJ;
    }
    if (index < stts->entry_count) {
        return mp4ChunkArraySetU32(&stts->entries, index, 2U, col, val);
    }
    pair[0] = 0U;
    pair[1] = 0U;
    pair[col] = val;
    return mp4SttsAppend(stts, pair);
}

/* 0x01eb14 */
MP4_NOINLINE int32_t mp4StssSetEntry(Mp4StssBox *stss, uint32_t index, uint32_t sample_num)
{
    if (stss == NULL) {
        return MP4_ERR_NULL_OBJ;
    }
    if (index < stss->entry_count) {
        return mp4ChunkArraySetU32(&stss->entries, index, 1U, 0U, sample_num);
    }
    return mp4StssAppend(stss, sample_num);
}

/* 0x01eb68 */
MP4_NOINLINE int32_t mp4CttsSetEntry(Mp4CttsBox *ctts, uint32_t index, uint32_t col, uint32_t val)
{
    uint32_t pair[2];

    if (ctts == NULL) {
        return MP4_ERR_NULL_OBJ;
    }
    if (index < ctts->entry_count) {
        return mp4ChunkArraySetU32(&ctts->entries, index, 2U, col, val);
    }
    pair[0] = 0U;
    pair[1] = 0U;
    pair[col] = val;
    return mp4CttsAppend(ctts, pair);
}

/* 0x01ebd4 */
MP4_NOINLINE int32_t mp4StblGetSampleDur(
    Mp4Stbl *stbl,
    uint32_t sample_idx,
    uint32_t *out_dur,
    int32_t allow_io)
{
    int32_t err;

    if (stbl->frag_ctx == NULL) {
        return MP4_ERR_NOT_READY;
    }
    if (sample_idx < stbl->frag_ctx->min_sample) {
        return MP4_ERR_SAMPLE_RANGE;
    }

    err = mp4StblEnsureSample(stbl, sample_idx, allow_io);
    if (err == MP4_OK) {
        return mp4StblLookupSampleDur(stbl, sample_idx, out_dur);
    }
    return err;
}

/* 0x01ec58 */
MP4_NOINLINE int32_t mp4StblLookupSampleDur(
    Mp4Stbl *stbl,
    uint32_t sample_idx,
    uint32_t *out_dur)
{
    uint32_t sample_time = 0U;

    return mp4StblLookupSampleTime(stbl, sample_idx, &sample_time, out_dur);
}

/* 0x01ec78 */
MP4_NOINLINE int32_t mp4StblEnsureSample(
    Mp4Stbl *stbl,
    uint32_t sample_idx,
    int32_t allow_io)
{
    Mp4StblFragCtx *frag;

    if (stbl == NULL || stbl->frag_ctx == NULL) {
        return MP4_ERR_NOT_READY;
    }

    frag = stbl->frag_ctx;
    if (frag->is_complete == 1U && sample_idx >= frag->total_samples) {
        return MP4_ERR_SAMPLE_RANGE;
    }

    if (sample_idx >= frag->base_sample && sample_idx < frag->base_sample + stbl->stsz.sample_count) {
        return MP4_OK;
    }

    if (allow_io == 0) {
        return MP4_ERR_IO;
    }

    if (sample_idx < frag->base_sample) {
        if (mp4StblResetFrags(stbl) != MP4_OK) {
            return MP4_ERR_INTERNAL;
        }
    }

    for (;;) {
        int32_t err;
        frag = stbl->frag_ctx;
        if (sample_idx >= frag->base_sample && sample_idx < frag->base_sample + stbl->stsz.sample_count) {
            return MP4_OK;
        }
        err = mp4StblLoadNextFrag(stbl);
        if (err != MP4_OK) {
            if (err == MP4_ERR_EOF_FRAG) {
                return MP4_ERR_SAMPLE_RANGE;
            }
            return err;
        }
    }
}

/* 0x01ed74 */
MP4_NOINLINE int32_t mp4StblSumSampleSizes(
    Mp4Stbl *stbl,
    uint32_t start_idx,
    uint32_t end_idx,
    uint32_t *out_sum)
{
    uint32_t base_sample = stbl->frag_ctx->base_sample;
    uint32_t last_sample = base_sample + stbl->stsz.sample_count - 1U;

    if (((start_idx < base_sample) | (last_sample < start_idx)) ||
        ((end_idx < base_sample) | (last_sample < end_idx))) {
        return MP4_ERR_SAMPLE_RANGE;
    }

    if (stbl->stsz.const_sample_size != 0U) {
        *out_sum = stbl->stsz.const_sample_size * (end_idx - start_idx + 1U);
        return MP4_OK;
    }

    return mp4ChunkArraySumU32(stbl->stsz.entries, start_idx - base_sample, end_idx - base_sample,
                               0U, out_sum);
}

/* 0x01ee14 */
MP4_NOINLINE int32_t mp4StblGetSampleTime(
    Mp4Stbl *stbl,
    uint32_t sample_idx,
    uint32_t *out_time,
    uint32_t *out_dur,
    int32_t allow_io)
{
    int32_t err;

    if (mp4FsmIsCacheMode(mp4StblGetFsm(stbl)) == 1) {
        return mp4CacheGetSampleTime(stbl->cache, sample_idx, out_time, out_dur, allow_io);
    }
    if (stbl->frag_ctx == NULL) {
        return MP4_ERR_NOT_READY;
    }
    if (sample_idx < stbl->frag_ctx->min_sample) {
        return MP4_ERR_SAMPLE_RANGE;
    }

    err = mp4StblEnsureSample(stbl, sample_idx, allow_io);
    if (err == MP4_OK) {
        return mp4StblLookupSampleTime(stbl, sample_idx, out_time, out_dur);
    }
    return err;
}

/* 0x01eef4 */
MP4_NOINLINE int32_t mp4StblLookupSampleTime(
    Mp4Stbl *stbl,
    uint32_t sample_idx,
    uint32_t *out_time,
    uint32_t *out_dur)
{
    Mp4StblFragCtx *frag = stbl->frag_ctx;
    uint32_t base_sample = frag->base_sample;
    uint32_t cur_entry;
    uint32_t cur_sample;

    if (sample_idx >= base_sample + stbl->stsz.sample_count || sample_idx < base_sample) {
        return MP4_ERR_SAMPLE_RANGE;
    }

    cur_entry = stbl->stts.cursor_entry;
    if (cur_entry == 0U) {
        stbl->stts.cursor_sample = base_sample;
        stbl->stts.cursor_time = frag->base_time;
    }
    cur_sample = stbl->stts.cursor_sample;

    if (sample_idx < cur_sample) {
        if (sample_idx - base_sample < cur_sample - sample_idx) {
            cur_entry = 0U;
            cur_sample = base_sample;
            stbl->stts.cursor_entry = 0U;
            stbl->stts.cursor_sample = base_sample;
            stbl->stts.cursor_time = frag->base_time;
        }
    } else if (cur_sample < sample_idx) {
        uint32_t last_entry = stbl->stts.entry_count - 1U;
        if (cur_entry != last_entry &&
            base_sample + stbl->stsz.sample_count - sample_idx < sample_idx - cur_sample) {
            uint32_t last_cnt = mp4SttsGetWord(&stbl->stts, last_entry, 0U);
            uint32_t last_delta = mp4SttsGetWord(&stbl->stts, last_entry, 1U);
            stbl->stts.cursor_entry = last_entry;
            cur_entry = last_entry;
            cur_sample = base_sample + stbl->stsz.sample_count - last_cnt;
            stbl->stts.cursor_sample = cur_sample;
            stbl->stts.cursor_time = frag->base_time + frag->loaded_duration - last_cnt * last_delta;
        }
    }

    if (sample_idx < cur_sample) {
        uint32_t scan = cur_entry - 1U;
        uint32_t acc_sample = cur_sample;
        uint32_t acc_time = 0U;

        if (scan >= stbl->stts.entry_count) {
            return MP4_ERR_CORRUPT;
        }

        for (;;) {
            uint32_t run_samples = mp4SttsGetWord(&stbl->stts, scan, 0U);
            uint32_t run_delta = mp4SttsGetWord(&stbl->stts, scan, 1U);
            acc_sample -= run_samples;
            acc_time += run_delta * run_samples;
            if (sample_idx >= acc_sample) {
                uint32_t new_time = stbl->stts.cursor_time - acc_time;
                *out_time = new_time + (sample_idx - acc_sample) * run_delta;
                *out_dur = run_delta;
                stbl->stts.cursor_entry = scan;
                stbl->stts.cursor_time = new_time;
                stbl->stts.cursor_sample = acc_sample;
                return MP4_OK;
            }
            if (scan == 0U) {
                return MP4_ERR_CORRUPT;
            }
            scan--;
        }
    }

    if (cur_entry >= stbl->stts.entry_count) {
        return MP4_ERR_CORRUPT;
    }

    {
        uint32_t scan = cur_entry;
        uint32_t acc_sample = cur_sample;
        uint32_t acc_time = 0U;

        while (scan < stbl->stts.entry_count) {
            uint32_t run_samples = mp4SttsGetWord(&stbl->stts, scan, 0U);
            uint32_t run_delta = mp4SttsGetWord(&stbl->stts, scan, 1U);
            if (sample_idx < acc_sample + run_samples) {
                uint32_t new_time = stbl->stts.cursor_time + acc_time;
                *out_time = new_time + (sample_idx - acc_sample) * run_delta;
                *out_dur = run_delta;
                stbl->stts.cursor_entry = scan;
                stbl->stts.cursor_time = new_time;
                stbl->stts.cursor_sample = acc_sample;
                return MP4_OK;
            }
            acc_sample += run_samples;
            acc_time += run_delta * run_samples;
            scan++;
        }
    }

    return MP4_ERR_CORRUPT;
}

/* 0x01f220 */
MP4_NOINLINE int32_t mp4StblFindSampleByTime(
    Mp4Stbl *stbl,
    uint32_t time_val,
    uint32_t *out_sample,
    uint32_t *out_is_sync,
    uint32_t *out_rem,
    int32_t allow_io)
{
    int32_t err;

    if (mp4FsmIsCacheMode(mp4StblGetFsm(stbl)) == 1) {
        return mp4CacheFindSampleByTime(stbl->cache, time_val, out_sample, out_is_sync, out_rem, allow_io);
    }
    if (stbl->frag_ctx == NULL) {
        return MP4_ERR_NOT_READY;
    }
    if (time_val < stbl->frag_ctx->min_time) {
        return MP4_ERR_TIME_RANGE;
    }

    err = mp4StblEnsureTime(stbl, time_val, allow_io);
    if (err != MP4_OK) {
        return err;
    }
    err = mp4StblLookupTime(stbl, time_val, out_sample, out_rem);
    if (err != MP4_OK) {
        return err;
    }

    err = mp4StblLookupSyncSample(stbl, *out_sample, out_is_sync);
    if (err == MP4_ERR_CHUNK_EMPTY) {
        return MP4_ERR_CORRUPT;
    }
    return err;
}

/* 0x01f330 */
MP4_NOINLINE int32_t mp4StblGetSampleInfo(
    Mp4Stbl *stbl,
    uint32_t sample_idx,
    int32_t want_chunk_rem,
    Mp4SampleInfo *out_info,
    int32_t allow_io)
{
    uint32_t chunk_idx = 0U;
    uint32_t sample_time = 0U;
    uint32_t chunk_first_sample = 0U;
    uint32_t chunk_spc = 0U;
    uint32_t size_val;
    int32_t err;

    if (mp4FsmIsCacheMode(mp4StblGetFsm(stbl)) || mp4FsmIsPendingCache(mp4StblGetFsm(stbl))) {
        return mp4CacheGetSampleInfo(stbl->cache, sample_idx, want_chunk_rem, out_info, allow_io);
    }
    if (stbl->frag_ctx == NULL) {
        return MP4_ERR_NOT_READY;
    }
    if (sample_idx < stbl->frag_ctx->min_sample) {
        return MP4_ERR_SAMPLE_RANGE;
    }

    err = mp4StblEnsureSample(stbl, sample_idx, allow_io);
    if (err != MP4_OK) {
        return err;
    }
    if (mp4StblGetSampleOffset(stbl, sample_idx, &out_info->offset, &chunk_idx) != MP4_OK) {
        return MP4_ERR_CORRUPT;
    }

    size_val = stbl->stsz.const_sample_size;
    if (size_val == 0U) {
        uint32_t read_sz = 0U;
        if (mp4ChunkArrayGetU32(stbl->stsz.entries, sample_idx - stbl->frag_ctx->base_sample,
                                0U, &read_sz) == MP4_OK) {
            size_val = read_sz;
        } else {
            size_val = 0xFFFFFFFFU;
        }
    }
    out_info->size = size_val;

    if (mp4StblLookupSampleTime(stbl, sample_idx, &sample_time, &out_info->duration) != MP4_OK) {
        return MP4_ERR_CORRUPT;
    }

    if (stbl->stss.size != 0U) {
        if (mp4StblLookupSyncSample(stbl, sample_idx, &out_info->is_sync) != MP4_OK) {
            return MP4_ERR_CORRUPT;
        }
    } else {
        out_info->is_sync = 1U;
    }

    if (stbl->ctts.size != 0U) {
        if (mp4StblLookupCtts(stbl, sample_idx, &out_info->ctts_offset) != MP4_OK) {
            return MP4_ERR_CORRUPT;
        }
    } else {
        out_info->ctts_offset = 0U;
    }

    if (mp4StblGetChunkInfo(stbl, chunk_idx, &chunk_first_sample, &chunk_spc, &out_info->sample_desc) != MP4_OK) {
        return MP4_ERR_CORRUPT;
    }

    if (want_chunk_rem == 1) {
        uint32_t chunk_last_sample;
        uint32_t scan;

        if (chunk_spc == 0U) {
            return MP4_ERR_CORRUPT;
        }
        out_info->chunk_rem_bytes = 0U;
        chunk_last_sample = chunk_first_sample + chunk_spc - 1U;
        if (mp4StblSumSampleSizes(stbl, sample_idx, chunk_last_sample, &out_info->chunk_rem_bytes) != MP4_OK) {
            return MP4_ERR_CORRUPT;
        }
        out_info->chunk_rem_dur = 0U;
        for (scan = sample_idx; scan <= chunk_last_sample; scan++) {
            uint32_t dur_val = 0U;
            if (mp4StblLookupSampleDur(stbl, scan, &dur_val) != MP4_OK) {
                return MP4_ERR_CORRUPT;
            }
            out_info->chunk_rem_dur += dur_val;
        }
        out_info->chunk_rem_samples = chunk_last_sample - sample_idx + 1U;
    }

    stbl->frag_ctx->cur_read_sample = sample_idx + 1U;
    return MP4_OK;
}

/* 0x01f5c4 */
MP4_NOINLINE int32_t mp4StblCheckSyncSample(
    Mp4Stbl *stbl,
    uint32_t sample_idx,
    uint32_t *out_is_sync,
    int32_t allow_io)
{
    int32_t err;

    if (stbl->frag_ctx == NULL) {
        return MP4_ERR_NOT_READY;
    }
    if (sample_idx < stbl->frag_ctx->min_sample) {
        return MP4_ERR_SAMPLE_RANGE;
    }

    err = mp4StblEnsureSample(stbl, sample_idx, allow_io);
    if (err == MP4_OK) {
        return mp4StblLookupSyncSample(stbl, sample_idx, out_is_sync);
    }
    return err;
}

/* 0x01f648 */
MP4_NOINLINE int32_t mp4StblLookupSyncSample(
    Mp4Stbl *stbl,
    uint32_t sample_idx,
    uint32_t *out_is_sync)
{
    Mp4StblFragCtx *frag = stbl->frag_ctx;
    Mp4StssBox *stss = &stbl->stss;
    uint32_t cur_sample;

    if (sample_idx >= frag->base_sample + stbl->stsz.sample_count || sample_idx < frag->base_sample) {
        return MP4_ERR_SAMPLE_RANGE;
    }
    if (stss->size == 0U) {
        *out_is_sync = 1U;
        return MP4_OK;
    }
    if (stss->entry_count == 0U) {
        *out_is_sync = 0U;
        return MP4_OK;
    }

    if (stss->cursor_entry != 0U) {
        cur_sample = stss->cursor_sample;
    } else {
        cur_sample = mp4StssReadEntry(stss, 0U) - 1U;
        stss->cursor_sample = cur_sample;
    }

    if (sample_idx < cur_sample) {
        if (stss->cursor_entry != 0U &&
            sample_idx - stbl->frag_ctx->base_sample < cur_sample - sample_idx) {
            stss->cursor_entry = 0U;
            cur_sample = mp4StssReadEntry(stss, 0U) - 1U;
            stss->cursor_sample = cur_sample;
        }
    } else if (cur_sample < sample_idx) {
        uint32_t last_entry = stss->entry_count - 1U;
        if (stss->cursor_entry != last_entry &&
            stbl->frag_ctx->base_sample + stbl->stsz.sample_count - sample_idx < sample_idx - cur_sample) {
            stss->cursor_entry = last_entry;
            cur_sample = mp4StssReadEntry(stss, last_entry) - 1U;
            stss->cursor_sample = cur_sample;
        }
    }

    if (sample_idx < cur_sample) {
        uint32_t scan;

        if (stss->cursor_entry == 0U) {
            *out_is_sync = 0U;
            return MP4_OK;
        }
        scan = stss->cursor_entry - 1U;
        for (;;) {
            uint32_t sync_sample = mp4StssReadEntry(stss, scan) - 1U;
            if (sync_sample == sample_idx) {
                *out_is_sync = 1U;
                stss->cursor_sample = sync_sample;
                stss->cursor_entry = scan;
                return MP4_OK;
            }
            if (sync_sample < sample_idx) {
                *out_is_sync = 0U;
                return MP4_OK;
            }
            stss->cursor_entry = scan;
            stss->cursor_sample = sync_sample;
            if (scan == 0U) {
                *out_is_sync = 0U;
                return MP4_OK;
            }
            scan--;
        }
    }

    if (cur_sample == sample_idx) {
        *out_is_sync = 1U;
        return MP4_OK;
    }

    {
        uint32_t scan = stss->cursor_entry + 1U;
        while (scan < stss->entry_count) {
            uint32_t sync_sample = mp4StssReadEntry(stss, scan) - 1U;
            if (sync_sample == sample_idx) {
                *out_is_sync = 1U;
                stss->cursor_sample = sync_sample;
                stss->cursor_entry = scan;
                return MP4_OK;
            }
            if (sample_idx < sync_sample) {
                *out_is_sync = 0U;
                return MP4_OK;
            }
            stss->cursor_entry = scan;
            stss->cursor_sample = sync_sample;
            scan++;
        }
    }

    *out_is_sync = 0U;
    return MP4_OK;
}

/* 0x01f938 */
MP4_NOINLINE int32_t mp4StblFindSyncSample(
    Mp4Stbl *stbl,
    uint32_t sample_idx,
    int32_t direction,
    uint32_t *out_sample,
    uint32_t *out_delta,
    int32_t allow_io)
{
    Mp4StblFragCtx *frag;
    uint32_t base_time = 0U;
    uint32_t found_time = 0U;
    uint32_t sample_dur = 0U;
    uint32_t is_sync = 0U;
    int32_t err;

    if (mp4FsmIsCacheMode(mp4StblGetFsm(stbl))) {
        return mp4CacheFindSyncSample(stbl->cache, sample_idx, direction, out_sample, out_delta, allow_io);
    }
    frag = stbl->frag_ctx;
    if (frag == NULL) {
        return MP4_ERR_NOT_READY;
    }
    if (sample_idx < frag->min_sample) {
        return MP4_ERR_SAMPLE_RANGE;
    }
    if (sample_idx == 0U && direction == 0) {
        return MP4_ERR_NO_SYNC;
    }
    if (frag->is_complete == 1U && frag->total_samples < sample_idx + 1U) {
        return MP4_ERR_SAMPLE_RANGE;
    }

    err = mp4StblGetSampleTime(stbl, sample_idx, &base_time, &sample_dur, 1);
    if (err != MP4_OK) {
        return err;
    }

    if (direction != 0) {
        uint32_t scan = sample_idx + 1U;
        for (;;) {
            err = mp4StblCheckSyncSample(stbl, scan, &is_sync, allow_io);
            if (err != MP4_OK) {
                if (err == MP4_ERR_SAMPLE_RANGE) {
                    return MP4_ERR_NO_SYNC;
                }
                return err;
            }
            if (is_sync == 1U) {
                *out_sample = scan;
                err = mp4StblLookupSampleTime(stbl, scan, &found_time, &sample_dur);
                if (err == MP4_OK) {
                    *out_delta = found_time - base_time;
                }
                return err;
            }
            scan++;
        }
    }

    if (sample_idx == 0U) {
        return MP4_ERR_NO_SYNC;
    }

    {
        uint32_t scan = sample_idx - 1U;
        uint32_t steps = 0U;

        for (;;) {
            steps++;
            err = mp4StblCheckSyncSample(stbl, scan, &is_sync, allow_io);
            if (err != MP4_OK) {
                return err;
            }
            if (is_sync == 1U) {
                *out_sample = scan;
                err = mp4StblLookupSampleTime(stbl, scan, &found_time, &sample_dur);
                if (err == MP4_OK) {
                    *out_delta = base_time - found_time;
                }
                return err;
            }
            if (steps >= sample_idx) {
                return MP4_ERR_NO_SYNC;
            }
            scan--;
        }
    }
}

/* 0x01fb44 */
MP4_NOINLINE int32_t mp4StblGetSampleCount(Mp4Stbl *stbl, uint32_t *out_count, int32_t allow_io)
{
    Mp4StblFragCtx *frag;

    if (mp4FsmIsCacheMode(mp4StblGetFsm(stbl))) {
        return mp4CacheGetSampleCount(stbl->cache, out_count);
    }
    frag = stbl->frag_ctx;
    if (frag == NULL) {
        return MP4_ERR_NOT_READY;
    }
    if (stbl->stsz.size == 0U) {
        return MP4_ERR_CORRUPT;
    }
    if (frag->is_complete == 1U) {
        *out_count = frag->total_samples;
        return MP4_OK;
    }
    if (allow_io == 0) {
        return MP4_ERR_IO;
    }

    for (;;) {
        int32_t err = mp4StblLoadNextFrag(stbl);
        if (err == MP4_ERR_EOF_FRAG) {
            break;
        }
        if (err != MP4_OK) {
            return err;
        }
    }

    *out_count = stbl->frag_ctx->total_samples;
    return MP4_OK;
}

/* 0x01fc10 */
MP4_NOINLINE int32_t mp4StblGetTotalDuration(Mp4Stbl *stbl, uint32_t *out_dur, int32_t allow_io)
{
    Mp4StblFragCtx *frag;

    if (mp4FsmIsCacheMode(mp4StblGetFsm(stbl))) {
        return mp4CacheGetTotalDuration(stbl->cache, out_dur);
    }
    frag = stbl->frag_ctx;
    if (frag == NULL) {
        return MP4_ERR_NOT_READY;
    }
    if (frag->is_complete == 1U) {
        *out_dur = frag->total_duration;
        return MP4_OK;
    }
    if (allow_io == 0) {
        return MP4_ERR_IO;
    }

    for (;;) {
        int32_t err = mp4StblLoadNextFrag(stbl);
        if (err == MP4_ERR_EOF_FRAG) {
            break;
        }
        if (err != MP4_OK) {
            return err;
        }
    }

    *out_dur = stbl->frag_ctx->total_duration;
    return MP4_OK;
}

/* 0x01fcd0 */
MP4_NOINLINE int32_t mp4StblGetChunkCount(Mp4Stbl *stbl, uint32_t *out_count)
{
    if (mp4FsmIsCacheMode(mp4StblGetFsm(stbl)) == 1) {
        return mp4CacheGetChunkCount(stbl->cache, out_count);
    }
    *out_count = stbl->frag_ctx->base_chunk + stbl->stco.entry_count;
    return MP4_OK;
}

/* 0x01fd40 */
MP4_NOINLINE int32_t mp4StblLookupSampleSize(
    Mp4Stbl *stbl,
    uint32_t sample_idx,
    uint32_t *out_size)
{
    Mp4StblFragCtx *frag = stbl->frag_ctx;
    uint32_t size_val;

    if (frag == NULL) {
        return MP4_ERR_NOT_READY;
    }
    if (sample_idx < frag->base_sample || sample_idx >= frag->base_sample + stbl->stsz.sample_count) {
        return MP4_ERR_NO_ENTRY;
    }

    size_val = stbl->stsz.const_sample_size;
    if (size_val == 0U) {
        uint32_t read_sz;
        size_val = (mp4ChunkArrayGetU32(stbl->stsz.entries, sample_idx - frag->base_sample,
                                        0U, &read_sz) == MP4_OK) ? read_sz : 0xFFFFFFFFU;
    }
    *out_size = size_val;
    return MP4_OK;
}

/* 0x01fdd8 */
MP4_NOINLINE int32_t mp4StblInitStatic(Mp4Stbl *stbl, Mp4IoStream *stream)
{
    Mp4StblFragCtx *frag;
    uint32_t row;

    if (mp4FsmIsCacheMode(stream->fsm) || mp4FsmIsPendingCache(stream->fsm)) {
        return MP4_OK;
    }
    if (stbl == NULL) {
        return MP4_ERR_NULL_OBJ;
    }
    if (stbl->size == 0U || stbl->stsc.size == 0U || stbl->stts.size == 0U ||
        stbl->stsz.size == 0U || stbl->stco.size == 0U) {
        return MP4_ERR_CORRUPT;
    }

    if (stbl->frag_ctx != NULL) {
        mp4StblDetachFragCtx(stbl);
    }

    frag = (Mp4StblFragCtx *)mp4MemCalloc(sizeof(Mp4StblFragCtx));
    stbl->frag_ctx = frag;
    if (frag == NULL) {
        return MP4_ERR_NOMEM;
    }

    stream->max_frag_window = 2U;
    frag->loaded_duration = 0U;
    frag->frag_boundary = 0U;
    frag->is_complete = 0U;
    frag->stream = stream;

    for (row = 0U; row < stbl->stts.entry_count; row++) {
        uint32_t run_samples = mp4SttsGetWord(&stbl->stts, row, 0U);
        uint32_t run_delta = mp4SttsGetWord(&stbl->stts, row, 1U);
        stbl->frag_ctx->loaded_duration += run_samples * run_delta;
    }

    frag = stbl->frag_ctx;
    frag->frag_boundary = 1U;
    stbl->chunk_open = 1U;
    frag->next_moof_offset = stbl->track->movie->first_moof_offset;

    if (stbl->stco.entry_count != 0U) {
        uint32_t mark_triple[3];
        int32_t err;

        frag->loaded_frags = 1U;
        mark_triple[0] = 1U;
        mark_triple[1] = 0U;
        mark_triple[2] = stbl->stco.entry_count + frag->base_chunk - 1U;
        err = mp4ChunkArrayAppendU32(&frag->frag_marks, 3U, mark_triple, 2U);
        if (err != MP4_OK) {
            return err;
        }
        frag = stbl->frag_ctx;
    }

    frag->is_complete = 1U;
    frag->total_duration = frag->loaded_duration;
    frag->total_samples = stbl->stsz.sample_count;
    return MP4_OK;
}

/* 0x01ffc0 */
MP4_NOINLINE int32_t mp4StblRecalcSizes(Mp4Stbl *stbl)
{
    if ((mp4FsmIsCacheMode(mp4StblGetFsm(stbl)) || mp4FsmIsPendingCache(mp4StblGetFsm(stbl))) &&
        stbl->cache != NULL) {
        uint32_t cache_total = 0U;
        int32_t err = mp4CacheCalcBoxSizes(stbl->cache, &cache_total);
        if (err == MP4_OK) {
            stbl->size = cache_total;
        }
        return err;
    }

    stbl->size = 8U;
    if (stbl->ctts.size != 0U) {
        stbl->ctts.size = 8U * stbl->ctts.entry_count + 16U;
        if (mp4CttsHasNonZero(&stbl->ctts) != 0) {
            stbl->size += stbl->ctts.size;
        }
    }
    if (stbl->stts.size != 0U) {
        stbl->stts.size = 8U * stbl->stts.entry_count + 16U;
        stbl->size += stbl->stts.size;
    }
    if (stbl->stsc.size != 0U) {
        stbl->stsc.size = 12U * stbl->stsc.entry_count + 16U;
        stbl->size += stbl->stsc.size;
    }
    if (stbl->stss.size != 0U) {
        stbl->stss.size = 4U * stbl->stss.entry_count + 16U;
        stbl->size += stbl->stss.size;
    }
    if (stbl->stsz.size != 0U) {
        if (stbl->stsz.const_sample_size != 0U) {
            stbl->stsz.size = 20U;
            stbl->size += 20U;
        } else {
            stbl->stsz.size = 4U * stbl->stsz.sample_count + 20U;
            stbl->size += stbl->stsz.size;
        }
    }
    if (stbl->stco.size != 0U) {
        stbl->stco.size = 4U * stbl->stco.entry_count + 16U;
        stbl->size += stbl->stco.size;
    }

    return MP4_OK;
}

/* 0x020178 */
MP4_NOINLINE int32_t mp4StblLookupCtts(
    Mp4Stbl *stbl,
    uint32_t sample_idx,
    uint32_t *out_offset)
{
    Mp4StblFragCtx *frag = stbl->frag_ctx;
    Mp4CttsBox *ctts = &stbl->ctts;
    uint32_t found_entry;
    uint32_t found_sample;

    if (sample_idx >= frag->base_sample + stbl->stsz.sample_count || sample_idx < frag->base_sample) {
        return MP4_ERR_SAMPLE_RANGE;
    }
    if (ctts->size == 0U) {
        *out_offset = 0U;
        return MP4_OK;
    }

    if (ctts->cursor_entry == 0U) {
        ctts->cursor_sample = frag->base_sample;
    }

    if (sample_idx < ctts->cursor_sample) {
        uint32_t scan = ctts->cursor_entry - 1U;
        uint32_t acc_sample = ctts->cursor_sample;

        if (scan >= ctts->entry_count) {
            return MP4_ERR_CORRUPT;
        }
        for (;;) {
            acc_sample -= mp4CttsGetWord(ctts, scan, 0U);
            if (sample_idx >= acc_sample) {
                found_entry = scan;
                found_sample = acc_sample;
                break;
            }
            if (scan == 0U) {
                return MP4_ERR_CORRUPT;
            }
            scan--;
        }
    } else {
        uint32_t scan = ctts->cursor_entry;
        uint32_t acc_sample = ctts->cursor_sample;

        if (scan >= ctts->entry_count) {
            return MP4_ERR_CORRUPT;
        }
        for (;;) {
            uint32_t run_samples = mp4CttsGetWord(ctts, scan, 0U);
            if (sample_idx < acc_sample + run_samples) {
                found_entry = scan;
                found_sample = acc_sample;
                break;
            }
            acc_sample += run_samples;
            scan++;
            if (scan >= ctts->entry_count) {
                return MP4_ERR_CORRUPT;
            }
        }
    }

    *out_offset = mp4CttsGetWord(ctts, found_entry, 1U);
    ctts->cursor_sample = found_sample;
    ctts->cursor_entry = found_entry;
    return MP4_OK;
}

/* 0x020320 */
MP4_NOINLINE int32_t mp4StblUpdateMinBounds(Mp4Stbl *stbl)
{
    Mp4StblFragCtx *frag = stbl->frag_ctx;

    if (frag == NULL) {
        return MP4_ERR_NOT_READY;
    }
    frag->min_sample = frag->base_sample;
    frag->min_chunk = frag->base_chunk;
    frag->min_time = frag->base_time;
    return MP4_OK;
}

/* 0x020350 */
MP4_NOINLINE int32_t mp4CttsHasNonZero(Mp4CttsBox *ctts)
{
    uint32_t row;

    if (ctts == NULL || ctts->entries == NULL || ctts->entry_count == 0U) {
        return 0;
    }

    for (row = 0U; row < ctts->entry_count; row++) {
        if (mp4CttsGetWord(ctts, row, 1U) != 0U) {
            return 1;
        }
    }
    return 0;
}

/* 0x0203e8 */
MP4_NOINLINE int32_t mp4StblIsAtInitialFrag(Mp4Stbl *stbl, uint32_t *out_is_init)
{
    *out_is_init = (stbl->frag_seq_num == 0U) ? 1U : 0U;
    return MP4_OK;
}

/* 0x0203fc */
MP4_NOINLINE int32_t mp4StblGetFragBaseTime(Mp4Stbl *stbl, uint32_t *out_time)
{
    *out_time = stbl->frag_base_time;
    return MP4_OK;
}
