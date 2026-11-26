#include "mp4msv_internal.h"

/* 0x002aa4 */
MP4_NOINLINE int32_t mp4FindFragSampleByTime(Mp4Track *track, uint32_t time_val,
                                             uint32_t *out_sample, uint32_t *out_sync,
                                             uint32_t *out_rem)
{
    Mp4Stbl *stbl = &track->mdia.minf.stbl;
    int32_t status = mp4SeekTrackFragByTime(track, time_val);
    if (status != MP4_OK) {
        return status;
    }

    uint32_t sample_idx = 0;
    uint32_t is_sync = 0;
    uint32_t rem = 0;
    status = mp4StblFindSampleByTime(stbl, time_val, &sample_idx, &is_sync, &rem, 0);
    if (status != MP4_OK) {
        return status;
    }

    for (;;) {
        int32_t chk = mp4StblCheckSyncSample(stbl, sample_idx, &is_sync, 0);
        if (chk == MP4_ERR_PARSE_FATAL) {
            return MP4_ERR_CORRUPT;
        }
        if (chk != MP4_OK) {
            return MP4_ERR_INTERNAL;
        }
        if (is_sync != 0) {
            status = mp4StblFindSampleByTime(stbl, time_val, out_sample, out_sync, out_rem, 1);
            if (status == MP4_OK) {
                (*out_sample)++;
            }
            return status;
        }

        uint32_t sync_sample = 0;
        uint32_t sync_delta = 0;
        int32_t find_res = mp4StblFindSyncSample(stbl, sample_idx, 0, &sync_sample, &sync_delta, 0);
        if (find_res == MP4_OK) {
            status = mp4StblFindSampleByTime(stbl, time_val, out_sample, out_sync, out_rem, 1);
            if (status == MP4_OK) {
                (*out_sample)++;
            }
            return status;
        }
        if (find_res != MP4_ERR_NO_SYNC) {
            return find_res;
        }

        uint32_t is_init = 0;
        if (mp4StblIsAtInitialFrag(stbl, &is_init) != MP4_OK) {
            return MP4_ERR_INTERNAL;
        }
        if (is_init != 0) {
            return MP4_ERR_NO_PREV_FRAG;
        }

        status = mp4LoadPrevTrackFrag(track);
        if (status != MP4_OK) {
            return status;
        }

        uint32_t loaded_start = 0;
        uint32_t loaded_count = 0;
        if (mp4StblGetLoadedRange(stbl, &loaded_start, &loaded_count) != MP4_OK) {
            return MP4_ERR_INTERNAL;
        }
        sample_idx = loaded_count - 1U;
    }
}

/* 0x002c40 */
MP4_NOINLINE int32_t mp4PrefetchCacheWindow(Mp4Track *track, uint32_t start_sample,
                                            uint32_t count, uint32_t *out_start,
                                            uint32_t *out_count)
{
    if (start_sample == 0) {
        return MP4_ERR_SAMPLE_RANGE;
    }

    Mp4IoStream *stream = track->movie->self->handle->stream;
    int32_t status = mp4CacheUpdateWindow(track->mdia.minf.stbl.cache,
                                          start_sample - 1U, count, stream);
    int32_t ret = (status == MP4_ERR_NOT_READY) ? MP4_ERR_CORRUPT : status;
    if (status == MP4_OK) {
        int32_t rng = mp4StblGetLoadedRange(&track->mdia.minf.stbl, out_start, out_count);
        if (rng == MP4_OK && *out_count != 0) {
            (*out_start)++;
        }
        ret = (rng == MP4_ERR_NOT_READY) ? MP4_ERR_CORRUPT : rng;
    }
    return ret;
}

/* 0x002cf8 */
MP4_NOINLINE int32_t mp4CheckMoovValid(Mp4Handle *handle)
{
    Mp4Movie *movie = handle->movie;
    int32_t ret = MP4_ERR_CORRUPT;
    if (movie->size != 0) {
        ret = (movie->mvhd.size != 0) ? MP4_OK : MP4_ERR_CORRUPT;
    }
    return ret;
}

/* 0x002d18 */
MP4_NOINLINE int32_t mp4TrackCheckValid(Mp4Track *track, uint32_t movie_time,
                                        int32_t mode, uint32_t *out_media_time)
{
    return mp4TrackMapMovieTime(track, movie_time, mode, out_media_time);
}

/* 0x002d34 */
MP4_NOINLINE int32_t mp4GetTrackFragBaseTime(Mp4Track *track, uint32_t *out_time)
{
    return mp4StblGetFragBaseTime(&track->mdia.minf.stbl, out_time);
}

/* 0x002d50 */
MP4_NOINLINE int32_t mp4GetTrackCacheRange(Mp4Track *track, uint32_t *out_start,
                                           uint32_t *out_count)
{
    int32_t status = mp4StblGetLoadedRange(&track->mdia.minf.stbl, out_start, out_count);
    int32_t ret = (status == MP4_ERR_NOT_READY) ? MP4_ERR_CORRUPT : status;
    if (status == MP4_OK && *out_count != 0) {
        (*out_start)++;
    }
    return ret;
}

/* 0x002db0 */
MP4_NOINLINE int32_t mp4LoadNextTrackFrag(Mp4Track *track)
{
    int32_t status = mp4StblAdvanceFrags(&track->mdia.minf.stbl);
    if (status == MP4_ERR_NOT_READY) {
        return MP4_ERR_CORRUPT;
    }
    if ((status == MP4_ERR_COUNT_OVERFLOW) | ((uint32_t)(status - MP4_ERR_FRAG_OVERFLOW) < 2U)) {
        status = MP4_ERR_OVERFLOW;
    }
    return status;
}

/* 0x002dfc */
MP4_NOINLINE int32_t mp4SeekTrackFragByTime(Mp4Track *track, uint32_t target_time)
{
    Mp4Stbl *stbl = &track->mdia.minf.stbl;
    Mp4Handle *handle = track->movie->self->handle;

    if (target_time < track->mdia.mdhd.duration) {
        return mp4StblReloadFromStart(stbl);
    }

    uint32_t track_id = 0;
    if (mp4TrackGetId(track, &track_id) != MP4_OK) {
        return MP4_ERR_INTERNAL;
    }

    Mp4TfraEntry tfra_entry;
    uint32_t found = 0;
    if (handle->movie->mfra != NULL) {
        if (mp4MfraLookupByTime(handle->movie->mfra, track_id, target_time,
                                &found, &tfra_entry) != MP4_OK) {
            return MP4_ERR_INTERNAL;
        }
    }

    int32_t need_linear = 1;
    uint32_t base_time = 0;
    Mp4MoofBox *moof = NULL;
    uint64_t next_offset = 0;
    uint32_t frag_dur = 0;

    if (found != 0 &&
        tfra_entry.traf_number != 0 &&
        tfra_entry.trun_number != 0 &&
        tfra_entry.sample_number != 0) {
        int32_t moof_res = mp4MoofFindOrRead(handle->movie, handle->stream,
                                             tfra_entry.moof_offset, &moof, &next_offset);
        need_linear = 0;
        if (moof_res == MP4_ERR_EOF_FRAG) {
            need_linear = 1;
        } else if (moof_res != MP4_OK) {
            return moof_res;
        } else {
            uint32_t pre_dur = 0;
            int32_t time_res = mp4MoofGetTimeBeforeSample(
                moof, track_id, handle->movie->mvex,
                tfra_entry.traf_number - 1U,
                tfra_entry.trun_number - 1U,
                tfra_entry.sample_number - 1U,
                &pre_dur);
            if ((uint32_t)(time_res - MP4_ERR_TRAF_RANGE) < 2U || time_res == MP4_ERR_SAMPLE_RANGE) {
                need_linear = 1;
            } else if (time_res != MP4_OK) {
                return MP4_ERR_INTERNAL;
            } else {
                base_time = tfra_entry.time_lo - pre_dur;
                int32_t dur_res = mp4MoofGetTrackDuration(moof, track_id,
                                                          handle->movie->mvex, &frag_dur);
                if (dur_res != MP4_OK) {
                    return dur_res;
                }
            }
        }
    }

    if (found == 0 || need_linear != 0) {
        int32_t first_res = mp4MoofFindOrRead(handle->movie, handle->stream,
                                              handle->movie->first_moof_offset,
                                              &moof, &next_offset);
        if (first_res == MP4_ERR_EOF_FRAG) {
            return MP4_ERR_TIME_RANGE;
        }
        if (first_res != MP4_OK) {
            return first_res;
        }
        base_time = track->mdia.mdhd.duration;
        int32_t dur_res = mp4MoofGetTrackDuration(moof, track_id,
                                                  handle->movie->mvex, &frag_dur);
        if (dur_res != MP4_OK) {
            return dur_res;
        }
    }

    while (target_time >= base_time + frag_dur) {
        int32_t next_res = mp4MoofFindOrRead(handle->movie, handle->stream,
                                             next_offset, &moof, &next_offset);
        if (next_res == MP4_ERR_EOF_FRAG) {
            return MP4_ERR_TIME_RANGE;
        }
        if (next_res != MP4_OK) {
            return next_res;
        }
        base_time += frag_dur;
        int32_t dur_res = mp4MoofGetTrackDuration(moof, track_id,
                                                  handle->movie->mvex, &frag_dur);
        if (dur_res != MP4_OK) {
            return dur_res;
        }
    }

    return mp4StblJumpToMoof(stbl, moof, base_time);
}

/* 0x0030a8 */
MP4_NOINLINE int32_t mp4LoadPrevTrackFrag(Mp4Track *track)
{
    Mp4Stbl *stbl = &track->mdia.minf.stbl;
    Mp4Handle *handle = track->movie->self->handle;

    uint32_t is_init = 0;
    if (mp4StblIsAtInitialFrag(stbl, &is_init) != MP4_OK) {
        return MP4_ERR_INTERNAL;
    }
    if (is_init != 0) {
        return MP4_OK;
    }

    uint64_t cur_offset = 0;
    if (mp4StblGetFragMoofOffset(stbl, &cur_offset) != MP4_OK) {
        return MP4_ERR_INTERNAL;
    }

    uint32_t cur_time = 0;
    if (mp4StblGetFragBaseTime(stbl, &cur_time) != MP4_OK) {
        return MP4_ERR_INTERNAL;
    }

    if (handle->movie->first_moof_offset == cur_offset) {
        return mp4StblReloadFromStart(stbl);
    }

    uint32_t track_id = 0;
    if (mp4TrackGetId(track, &track_id) != MP4_OK) {
        return MP4_ERR_INTERNAL;
    }

    uint32_t found = 0;
    uint64_t prev_offset = 0;
    if (handle->movie->mfra != NULL) {
        if (mp4MfraFindPrevMoof(handle->movie->mfra, track_id, cur_time, cur_offset,
                                &found, &prev_offset) != MP4_OK) {
            return MP4_ERR_INTERNAL;
        }
    }

    int32_t fallback = 0;
    Mp4MoofBox *moof = NULL;
    uint64_t next_offset = 0;
    if (found != 0) {
        int32_t res = mp4MoofFindOrRead(handle->movie, handle->stream,
                                        prev_offset, &moof, &next_offset);
        if (res == MP4_ERR_EOF_FRAG) {
            fallback = 1;
        } else if (res != MP4_OK) {
            return res;
        }
    }

    if (found == 0 || fallback != 0) {
        int32_t res = mp4MoofFindOrRead(handle->movie, handle->stream,
                                        handle->movie->first_moof_offset,
                                        &moof, &next_offset);
        if (res == MP4_ERR_EOF_FRAG) {
            return MP4_ERR_INTERNAL;
        }
        if (res != MP4_OK) {
            return res;
        }
    }

    for (;;) {
        if (next_offset >= cur_offset) {
            uint32_t frag_dur = 0;
            int32_t dur_res = mp4MoofGetTrackDuration(moof, track_id,
                                                      handle->movie->mvex, &frag_dur);
            if (dur_res != MP4_OK) {
                return dur_res;
            }
            return mp4StblJumpToMoof(stbl, moof, cur_time - frag_dur);
        }
        int32_t next_res = mp4MoofFindOrRead(handle->movie, handle->stream,
                                             next_offset, &moof, &next_offset);
        if (next_res != MP4_OK) {
            return next_res;
        }
    }
}

/* 0x0032ec */
MP4_NOINLINE int32_t mp4OpenMovieFull(Mp4Handle *handle, uint32_t *out_flags, int32_t mode)
{
    if (mp4AttachStream(handle->stream) != MP4_OK) {
        return MP4_ERR_IO;
    }

    Mp4IoStream *stream = handle->stream;
    stream->io_mode = 3;
    void *fh = stream->file_handle;

    if (handle->movie != NULL) {
        mp4CloseMovie(handle);
    }

    Mp4Movie *movie = (Mp4Movie *)mp4MemCalloc(sizeof(Mp4Movie));
    handle->movie = movie;
    if (movie == NULL) {
        return MP4_ERR_NOMEM;
    }

    movie->handle = handle;
    movie->options = handle->options;
    mp4SeekAbs(fh, 0);
    int64_t file_size = mp4CbGetSize(fh);
    int32_t parse_res = mp4ParseFile(handle->movie, (uint64_t)file_size, stream, mode);

    if (parse_res == MP4_OK || stream->error_flags == MP4_PARSE_FLAG_UNREC_BOX) {
        if (stream->is_fragmented != 0) {
            if (mp4FsmIsPendingCache(stream->fsm) != 0) {
                mp4CloseMovie(handle);
                movie = (Mp4Movie *)mp4MemCalloc(sizeof(Mp4Movie));
                handle->movie = movie;
                if (movie == NULL) {
                    return MP4_ERR_NOMEM;
                }
                movie->handle = handle;
                movie->options = handle->options;
                mp4SeekAbs(fh, 0);
                mp4FsmSwitchToFrag(stream->fsm);
                file_size = mp4CbGetSize(fh);
                parse_res = mp4ParseFile(handle->movie, (uint64_t)file_size, stream, mode);
            } else {
                mp4FsmSwitchToFrag(stream->fsm);
            }
        }
    }

    uint32_t flags = stream->error_flags;
    *out_flags = flags;
    if ((flags & MP4_PARSE_FLAG_FATAL_MASK) != 0) {
        mp4CloseMovie(handle);
        if ((int32_t)*out_flags < 0) {
            return MP4_ERR_IO;
        }
        if ((*out_flags & MP4_PARSE_FLAG_ALLOC_FAIL) != 0) {
            return MP4_ERR_NOMEM;
        }
        if ((*out_flags & MP4_PARSE_FLAG_BAD_BOX_SIZE) != 0) {
            return MP4_ERR_BAD_FORMAT;
        }
    }

    if (parse_res == MP4_OK || *out_flags == MP4_PARSE_FLAG_UNREC_BOX) {
        Mp4Movie *cur_movie = handle->movie;
        uint32_t track_idx = 0;
        while (track_idx < cur_movie->track_count) {
            Mp4Track *trk = mp4MovieGetTrackAt(cur_movie, track_idx);
            if (trk != NULL) {
                if (mp4FsmIsPendingCache(stream->fsm) != 0) {
                    if (trk->mdia.minf.stbl.cache != NULL) {
                        uint32_t sample_cnt = 0;
                        uint32_t chunk_cnt = 0;
                        mp4CacheGetSampleCount(trk->mdia.minf.stbl.cache, &sample_cnt);
                        mp4CacheGetChunkCount(trk->mdia.minf.stbl.cache, &chunk_cnt);
                        mp4CacheSetTotals(trk->mdia.minf.stbl.cache, sample_cnt,
                                          trk->mdia.mdhd.duration, chunk_cnt);
                    }
                } else {
                    if (mp4StblInitStatic(&trk->mdia.minf.stbl, stream) == MP4_ERR_NOMEM) {
                        mp4CloseMovie(handle);
                        return MP4_ERR_NOMEM;
                    }
                }
            }
            track_idx++;
        }

        if (stream->is_fragmented == 1) {
            if (cur_movie->track_count >= 128U) {
                uint32_t trim_idx = 126;
                while (trim_idx < cur_movie->track_count) {
                    Mp4Track *extra_trk = mp4MovieGetTrackAt(cur_movie, trim_idx);
                    mp4MovieRemoveTrackById(cur_movie, extra_trk->tkhd.track_id);
                    trim_idx++;
                }
                *out_flags |= MP4_PARSE_FLAG_TRACK_LIMIT;
            }
            uint32_t *counts = (uint32_t *)mp4MemCalloc(508);
            if (counts != NULL) {
                uint32_t ci = 0;
                while (ci < cur_movie->track_count) {
                    Mp4Track *trk = mp4MovieGetTrackAt(cur_movie, ci);
                    if (trk != NULL) {
                        mp4StblGetSampleCount(&trk->mdia.minf.stbl, &counts[ci], 0);
                    }
                    ci++;
                }
                mp4InitFragmentedTracks(cur_movie, counts, stream);
                mp4MemFree(counts);
            }
        }

        if (mp4FsmIsPendingCache(stream->fsm) == 0) {
            uint32_t si = 0;
            while (si < cur_movie->track_count) {
                Mp4Track *trk = mp4MovieGetTrackAt(cur_movie, si);
                mp4StblAttachStream(&trk->mdia.minf.stbl, stream);
                si++;
            }
        }
    }

    uint32_t cur_flags = *out_flags;
    if ((uint32_t)(mode - 1) < 3U) {
        if (handle->movie->mvex != NULL && handle->movie->mvex->mehd.size != 0) {
            cur_flags |= MP4_PARSE_FLAG_HAS_MVEX;
            *out_flags = cur_flags;
        }
        if (stream->is_fragmented == 1) {
            cur_flags |= MP4_PARSE_FLAG_HAS_MOOF;
            *out_flags = cur_flags;
        }
    }

    if ((cur_flags & 0xFFF0FFFBU) != 0) {
        mp4CloseMovie(handle);
        return MP4_ERR_CORRUPT;
    }

    Mp4Movie *final_movie = handle->movie;
    Mp4IodsBox *iods = final_movie->iods;
    if (iods != NULL) {
        Mp4DescNode *desc = NULL;
        if (mp4DescParseIods(iods->size - 12U, iods->payload, &desc) == MP4_OK) {
            Mp4IodsBody *body = (Mp4IodsBody *)desc->body;
            if (body->es_track_count != 0) {
                uint32_t od_track_id = body->es_track_ids[0];
                mp4DescFree(desc);
                Mp4Track *od_track = NULL;
                if (mp4MovieFindTrackById(final_movie, od_track_id, &od_track) == MP4_OK) {
                    od_track->odsm_ctx = NULL;
                    final_movie->iods->od_track = od_track;
                    if (final_movie->mvhd.timescale == od_track->mdia.mdhd.timescale &&
                        final_movie->mvhd.duration == od_track->tkhd.duration) {
                        od_track->odsm_mode = 2;
                    } else {
                        od_track->odsm_mode = 1;
                    }
                    if (stream->is_fragmented == 1) {
                        od_track->odsm_mode = 1;
                    }
                    Mp4SampleInfo smp;
                    if (mp4GetSampleInfo(od_track, 1, 0, &smp) == MP4_OK) {
                        uint8_t *buf = (uint8_t *)mp4MemCalloc(smp.size);
                        if (buf != NULL) {
                            mp4SeekAbs(fh, smp.offset);
                            int32_t rd = mp4CbRead(buf, smp.size, fh);
                            if ((uint32_t)rd == smp.size) {
                                mp4OdsmParseCommand((uint32_t)rd, buf, &od_track->odsm_ctx);
                            }
                            mp4MemFree(buf);
                        }
                    }
                }
            } else {
                mp4DescFree(desc);
            }
        }
    }

    return MP4_OK;
}

/* 0x0038dc */
MP4_NOINLINE int32_t mp4OpenMovieBasic(Mp4Handle *handle, uint32_t *out_flags)
{
    if (mp4AttachStream(handle->stream) != MP4_OK) {
        return MP4_ERR_IO;
    }

    handle->stream->io_mode = 3;
    if (handle->movie != NULL) {
        mp4CloseMovie(handle);
    }

    Mp4Movie *movie = (Mp4Movie *)mp4MemCalloc(sizeof(Mp4Movie));
    handle->movie = movie;
    if (movie == NULL) {
        return MP4_ERR_NOMEM;
    }

    movie->handle = handle;
    mp4SeekAbs(handle->stream->file_handle, 0);
    mp4ParseFilePartial(handle->movie, 3, handle->stream);

    uint32_t flags = handle->stream->error_flags;
    *out_flags = flags;
    if ((flags & MP4_PARSE_FLAG_FATAL_MASK) != 0) {
        mp4CloseMovie(handle);
        if ((int32_t)*out_flags < 0) {
            return MP4_ERR_IO;
        }
        if ((*out_flags & MP4_PARSE_FLAG_ALLOC_FAIL) != 0) {
            return MP4_ERR_NOMEM;
        }
        if ((*out_flags & MP4_PARSE_FLAG_BAD_BOX_SIZE) != 0) {
            return MP4_ERR_BAD_FORMAT;
        }
        flags = *out_flags;
        handle = NULL;
    }

    if ((flags & 0x0BU) != 0) {
        mp4CloseMovie(handle);
        return MP4_ERR_CORRUPT;
    }
    return MP4_OK;
}

/* 0x0039f4 */
MP4_NOINLINE int32_t mp4OpenMoviePartial(Mp4Handle *handle, uint32_t mask, uint32_t *out_flags)
{
    if (mp4AttachStream(handle->stream) != MP4_OK) {
        return MP4_ERR_IO;
    }

    handle->stream->io_mode = 3;
    if (handle->movie != NULL) {
        mp4CloseMovie(handle);
    }

    Mp4Movie *movie = (Mp4Movie *)mp4MemCalloc(sizeof(Mp4Movie));
    handle->movie = movie;
    if (movie == NULL) {
        return MP4_ERR_NOMEM;
    }

    movie->handle = handle;
    mp4SeekAbs(handle->stream->file_handle, 0);
    mp4ParseFilePartial(handle->movie, mask, handle->stream);

    uint32_t flags = handle->stream->error_flags;
    *out_flags = flags;
    if ((flags & MP4_PARSE_FLAG_FATAL_MASK) != 0) {
        mp4CloseMovie(handle);
        if ((int32_t)*out_flags < 0) {
            return MP4_ERR_IO;
        }
        if ((*out_flags & MP4_PARSE_FLAG_ALLOC_FAIL) != 0) {
            return MP4_ERR_NOMEM;
        }
        if ((*out_flags & MP4_PARSE_FLAG_BAD_BOX_SIZE) != 0) {
            return MP4_ERR_BAD_FORMAT;
        }
        flags = *out_flags;
        handle = NULL;
    }

    if ((flags & 0x0BU) != 0) {
        mp4CloseMovie(handle);
        return MP4_ERR_CORRUPT;
    }
    return MP4_OK;
}

/* 0x003b18 */
MP4_NOINLINE int32_t mp4GetTrackByIndex(Mp4Handle *handle, uint16_t index, Mp4Track **out_track)
{
    if (index == 0) {
        return MP4_ERR_TRACK_RANGE;
    }
    Mp4Movie *movie = handle->movie;
    if (movie->size == 0) {
        return MP4_ERR_CORRUPT;
    }
    if (movie->track_count < (uint32_t)index) {
        return MP4_ERR_TRACK_RANGE;
    }
    Mp4Track *trk = mp4MovieGetTrackAt(movie, (uint32_t)index - 1U);
    if (trk == NULL) {
        return MP4_ERR_CORRUPT;
    }
    *out_track = trk;
    return MP4_OK;
}

/* 0x003b84 */
MP4_NOINLINE int32_t mp4GetTrackHeaderInfo(Mp4Track *track, Mp4TrackInfo *out_info)
{
    if (track->tkhd.size == 0 || track->mdia.mdhd.size == 0) {
        return MP4_ERR_CORRUPT;
    }
    out_info->track_id = track->tkhd.track_id;
    out_info->duration = track->tkhd.duration;
    if (track->tkhd.flags[2] & 1U) {
        out_info->enabled = 1U;
    } else {
        out_info->enabled = 0U;
    }
    out_info->width = (uint16_t)(track->tkhd.width >> 16);
    out_info->height = (uint16_t)(track->tkhd.height >> 16);
    out_info->layer = track->tkhd.layer;
    out_info->language = track->mdia.mdhd.language;
    out_info->alternate_group = track->tkhd.alternate_group;
    return MP4_OK;
}

/* 0x003bf8 */
MP4_NOINLINE int32_t mp4GetMatrix(Mp4Handle *handle, Mp4Track *track, int32_t *out_matrix)
{
    if (track != NULL) {
        for (uint32_t row = 0; row < 3; row++) {
            for (uint32_t col = 0; col < 3; col++) {
                out_matrix[row * 3U + col] = track->tkhd.matrix[row * 3U + col];
            }
        }
    } else if (handle != NULL) {
        Mp4Movie *movie = handle->movie;
        for (uint32_t row = 0; row < 3; row++) {
            for (uint32_t col = 0; col < 3; col++) {
                out_matrix[row * 3U + col] = movie->mvhd.matrix[row * 3U + col];
            }
        }
    }
    return MP4_OK;
}

/* 0x003cb8 */
MP4_NOINLINE int32_t mp4GetTrefCount(Mp4Track *track, uint32_t ref_type, uint32_t *out_count)
{
    Mp4TrefBox *tref = track->tref;
    if (tref == NULL) {
        return MP4_ERR_NO_ENTRY;
    }
    Mp4TrefTypeBox *sub = NULL;
    if (ref_type == MP4_TAG_MPOD) {
        sub = tref->mpod_ref;
    } else if (ref_type == MP4_TAG_SYNC) {
        sub = tref->sync_ref;
    } else {
        return MP4_ERR_NO_ENTRY;
    }
    if (sub == NULL) {
        return MP4_ERR_NO_ENTRY;
    }
    *out_count = (sub->size - 8U) >> 2;
    return MP4_OK;
}

/* 0x003d10 */
MP4_NOINLINE int32_t mp4GetTrefTrack(Mp4Track *track, uint32_t ref_type,
                                     uint32_t index, Mp4Track **out_track)
{
    if (track->movie == NULL) {
        return MP4_ERR_INVALID_HANDLE;
    }
    Mp4TrefBox *tref = track->tref;
    if (tref == NULL) {
        return MP4_ERR_NO_ENTRY;
    }
    if (index == 0) {
        return MP4_ERR_INVALID_PARAM;
    }

    uint32_t target_id = 0;
    if (ref_type == MP4_TAG_MPOD) {
        Mp4TrefTypeBox *sub = tref->mpod_ref;
        if (sub == NULL || sub->count < index) {
            return MP4_ERR_NO_ENTRY;
        }
        target_id = sub->track_ids[index - 1U];
    }
    if (ref_type == MP4_TAG_SYNC) {
        Mp4TrefTypeBox *sub = tref->sync_ref;
        if (sub == NULL || sub->count < index) {
            return MP4_ERR_NO_ENTRY;
        }
        target_id = sub->track_ids[index - 1U];
    }
    if (target_id == 0) {
        return MP4_ERR_NO_ENTRY;
    }
    if (mp4MovieFindTrackById(track->movie, target_id, out_track) != MP4_OK) {
        return MP4_ERR_CORRUPT;
    }
    return MP4_OK;
}

/* 0x003ddc */
MP4_NOINLINE int32_t mp4GetElstEntry(Mp4Track *track, uint32_t index,
                                     uint32_t *out_dur, uint32_t *out_time,
                                     uint32_t *out_rate)
{
    if (index == 0) {
        return MP4_ERR_INVALID_PARAM;
    }
    if (track->edts.size == 0 || track->edts.elst.size == 0) {
        return MP4_ERR_NO_ENTRY;
    }
    if (track->edts.elst.entry_count < index) {
        return MP4_ERR_NO_ENTRY;
    }
    uint32_t row = index - 1U;
    if (mp4ElstReadCol(&track->edts.elst, row, 0, out_dur) != MP4_OK) {
        return MP4_ERR_CORRUPT;
    }
    if (mp4ElstReadCol(&track->edts.elst, row, 1, out_time) != MP4_OK) {
        return MP4_ERR_CORRUPT;
    }
    if (mp4ElstReadCol(&track->edts.elst, row, 2, out_rate) != MP4_OK) {
        return MP4_ERR_CORRUPT;
    }
    return MP4_OK;
}

/* 0x003eb0 */
MP4_NOINLINE int32_t mp4GetTrackDuration(Mp4Track *track, uint32_t *out_dur)
{
    int32_t status = mp4TrackCalcDuration(track, out_dur, 0);
    if (status == MP4_ERR_IO) {
        mp4MovieCalcDuration(track->movie, out_dur, 1);
        status = mp4StblGetTotalDuration(&track->mdia.minf.stbl, out_dur, 1);
    }
    if (status == MP4_ERR_NOT_READY) {
        return MP4_ERR_CORRUPT;
    }
    if ((status == MP4_ERR_COUNT_OVERFLOW) | ((uint32_t)(status - MP4_ERR_FRAG_OVERFLOW) < 2U)) {
        status = MP4_ERR_OVERFLOW;
    }
    return status;
}

/* 0x003f44 */
MP4_NOINLINE int32_t mp4GetSampleSize(Mp4Track *track, uint32_t sample_num, uint32_t *out_size)
{
    if (sample_num == 0) {
        return MP4_ERR_INVALID_PARAM;
    }
    int32_t status = mp4StblGetSampleSize(&track->mdia.minf.stbl, sample_num - 1U, out_size, 1);
    switch (status) {
        case MP4_ERR_FRAG_OVERFLOW:
        case MP4_ERR_DUR_OVERFLOW:
        case MP4_ERR_COUNT_OVERFLOW:
            return MP4_ERR_OVERFLOW;
        case MP4_ERR_SAMPLE_RANGE:
            return MP4_ERR_NO_ENTRY;
        case MP4_ERR_NOT_READY:
            return MP4_ERR_CORRUPT;
        default:
            return status;
    }
}

/* 0x003fb8 */
MP4_NOINLINE int32_t mp4GetSampleDescByIndex(Mp4Track *track, uint32_t index,
                                             Mp4SampleEntry **out_entry)
{
    if (track->mdia.minf.stbl.stsd.size == 0) {
        return MP4_ERR_CORRUPT;
    }
    if (index == 0) {
        return MP4_ERR_INVALID_PARAM;
    }
    if (track->mdia.minf.stbl.stsd.entry_count < index) {
        return MP4_ERR_NO_ENTRY;
    }
    if (mp4StsdGetEntry(&track->mdia.minf.stbl.stsd, index, out_entry) != MP4_OK) {
        return MP4_ERR_NO_ENTRY;
    }
    return MP4_OK;
}

/* 0x004008 */
MP4_NOINLINE int32_t mp4GetAudioSampleDesc(Mp4SampleEntry *entry, uint16_t *out_ver,
                                           Mp4AudioSampleDescOut *out_desc)
{
    if (entry->handler_type != MP4_TAG_SOUN) {
        return MP4_ERR_INVALID_HANDLE;
    }
    Mp4AudioDesc *ad = (Mp4AudioDesc *)entry->codec_desc;
    out_desc->codec_tag = entry->tag;
    out_desc->reserved = 0;
    out_desc->data_ref = entry->data_ref;
    out_desc->vendor = ad->vendor;
    out_desc->channel_count = ad->channel_count;
    out_desc->sample_size = ad->sample_size;
    out_desc->sample_rate = ad->sample_rate;
    out_desc->samples_per_pkt = ad->samples_per_pkt;
    out_desc->bytes_per_sample = ad->bytes_per_sample;
    if (out_ver != NULL) {
        *out_ver = ad->version;
    }
    return MP4_OK;
}

/* 0x00407c */
MP4_NOINLINE int32_t mp4GetAvccSpsLen(Mp4SampleEntry *entry, uint8_t index, uint16_t *out_len)
{
    if (mp4MatchCodecTag(MP4_TAG_AVC1, entry) == 0) {
        return MP4_ERR_CODEC_MISMATCH;
    }
    Mp4AvccBox *avcc = entry->avcc;
    if (avcc == NULL) {
        return MP4_ERR_CORRUPT;
    }
    if (index == 0) {
        return MP4_ERR_INVALID_PARAM;
    }
    if (avcc->sps_count < index) {
        return MP4_ERR_NO_ENTRY;
    }
    if (avcc->sps_array == NULL) {
        return MP4_ERR_INTERNAL;
    }
    Mp4AvccNal nal;
    if (mp4DynArrayGet(avcc->sps_array, (uint32_t)index - 1U, &nal) != MP4_OK) {
        return MP4_ERR_INTERNAL;
    }
    *out_len = nal.length;
    return MP4_OK;
}

/* 0x00412c */
MP4_NOINLINE int32_t mp4GetAvccPpsLen(Mp4SampleEntry *entry, uint8_t index, uint16_t *out_len)
{
    if (mp4MatchCodecTag(MP4_TAG_AVC1, entry) == 0) {
        return MP4_ERR_CODEC_MISMATCH;
    }
    Mp4AvccBox *avcc = entry->avcc;
    if (avcc == NULL) {
        return MP4_ERR_CORRUPT;
    }
    if (index == 0) {
        return MP4_ERR_INVALID_PARAM;
    }
    if (avcc->pps_count < index) {
        return MP4_ERR_NO_ENTRY;
    }
    if (avcc->pps_array == NULL) {
        return MP4_ERR_INTERNAL;
    }
    Mp4AvccNal nal;
    if (mp4DynArrayGet(avcc->pps_array, (uint32_t)index - 1U, &nal) != MP4_OK) {
        return MP4_ERR_INTERNAL;
    }
    *out_len = nal.length;
    return MP4_OK;
}

/* 0x0041dc */
MP4_NOINLINE int32_t mp4GetAvccSpsData(Mp4SampleEntry *entry, uint8_t index,
                                       uint32_t buf_size, void *out_buf)
{
    if (mp4MatchCodecTag(MP4_TAG_AVC1, entry) == 0) {
        return MP4_ERR_CODEC_MISMATCH;
    }
    Mp4AvccBox *avcc = entry->avcc;
    if (avcc == NULL) {
        return MP4_ERR_CORRUPT;
    }
    if (index == 0) {
        return MP4_ERR_INVALID_PARAM;
    }
    if (avcc->sps_count < index) {
        return MP4_ERR_NO_ENTRY;
    }
    if (avcc->sps_array == NULL) {
        return MP4_ERR_INTERNAL;
    }
    Mp4AvccNal nal;
    if (mp4DynArrayGet(avcc->sps_array, (uint32_t)index - 1U, &nal) != MP4_OK) {
        return MP4_ERR_INTERNAL;
    }
    return mp4SafeMemcpy(out_buf, buf_size, nal.nal_bytes, nal.length);
}

/* 0x0042a4 */
MP4_NOINLINE int32_t mp4GetAvccPpsData(Mp4SampleEntry *entry, uint8_t index,
                                       uint32_t buf_size, void *out_buf)
{
    if (mp4MatchCodecTag(MP4_TAG_AVC1, entry) == 0) {
        return MP4_ERR_CODEC_MISMATCH;
    }
    Mp4AvccBox *avcc = entry->avcc;
    if (avcc == NULL) {
        return MP4_ERR_CORRUPT;
    }
    if (index == 0) {
        return MP4_ERR_INVALID_PARAM;
    }
    if (avcc->pps_count < index) {
        return MP4_ERR_NO_ENTRY;
    }
    if (avcc->pps_array == NULL) {
        return MP4_ERR_INTERNAL;
    }
    Mp4AvccNal nal;
    if (mp4DynArrayGet(avcc->pps_array, (uint32_t)index - 1U, &nal) != MP4_OK) {
        return MP4_ERR_INTERNAL;
    }
    return mp4SafeMemcpy(out_buf, buf_size, nal.nal_bytes, nal.length);
}

/* 0x00436c */
MP4_NOINLINE int32_t mp4GetTrackUsmtCustom(Mp4Track *track, uint32_t *out_hi,
                                           uint32_t *out_lo, int32_t mode)
{
    if (mode != 5) {
        return MP4_OK;
    }
    Mp4UsmtBox *usmt = track->usmt;
    if (usmt == NULL || usmt->mtdt.size == 0) {
        return MP4_ERR_CORRUPT;
    }
    uint32_t count = usmt->mtdt.entry_count;
    for (uint32_t idx = 0; idx < count; idx++) {
        Mp4MtdtEntry *ent = &usmt->mtdt.entries[idx];
        uint32_t lang_enc = (uint32_t)ent->language | ((uint32_t)ent->encoding << 16);
        if (ent->tag == 10U && ent->size == 18U && lang_enc == 21956U &&
            ent->ext.raw_payload != NULL) {
            uint8_t raw[8];
            mp4SafeMemcpy(raw, 8, ent->ext.raw_payload, 8);
            *out_hi = ((uint32_t)raw[0] << 24) + ((uint32_t)raw[1] << 16) +
                      ((uint32_t)raw[2] << 8) + (uint32_t)raw[3];
            *out_lo = ((uint32_t)raw[4] << 24) + ((uint32_t)raw[5] << 16) +
                      ((uint32_t)raw[6] << 8) + (uint32_t)raw[7];
            return MP4_OK;
        }
    }
    return MP4_ERR_CORRUPT;
}

/* 0x004498 */
MP4_NOINLINE int32_t mp4FindMtdtEntry(Mp4Handle *handle, Mp4Track *track, uint32_t tag,
                                      uint16_t lang, uint16_t enc, uint16_t *out_lang,
                                      Mp4MtdtEntry **out_entry, uint16_t *out_len)
{
    Mp4MtdtBox *mtdt = NULL;
    if (track != NULL) {
        if (track->usmt == NULL || track->usmt->mtdt.size == 0) {
            return MP4_ERR_NO_ENTRY;
        }
        mtdt = &track->usmt->mtdt;
    } else {
        Mp4Movie *movie = handle->movie;
        if (movie->size == 0) {
            return MP4_ERR_CORRUPT;
        }
        if (movie->usmt == NULL || movie->usmt->mtdt.size == 0) {
            return MP4_ERR_NO_ENTRY;
        }
        mtdt = &movie->usmt->mtdt;
    }

    uint16_t lang_mask = (uint16_t)(lang & 0x7FFFU);
    for (uint32_t idx = 0; idx < mtdt->entry_count; idx++) {
        Mp4MtdtEntry *ent = &mtdt->entries[idx];
        if (ent->tag == tag && (ent->language & 0x7FFFU) == lang_mask && ent->encoding == enc) {
            if (ent->size < 10U) {
                return MP4_ERR_CORRUPT;
            }
            *out_lang = ent->language;
            *out_entry = ent;
            *out_len = (uint16_t)(ent->size - 10U);
            return MP4_OK;
        }
    }
    return MP4_ERR_NO_ENTRY;
}

/* 0x004570 */
MP4_NOINLINE int32_t mp4SerializeMtdtExt(Mp4MtdtEntry *entry, uint8_t *out_buf)
{
    uint32_t magic = entry->ext.head_magic;
    uint32_t ver = entry->ext.head_version;
    uint32_t cnt = entry->ext.item_count;

    out_buf[0] = (uint8_t)(magic >> 24);
    out_buf[1] = (uint8_t)(magic >> 16);
    out_buf[2] = (uint8_t)(magic >> 8);
    out_buf[3] = (uint8_t)magic;

    out_buf[4] = (uint8_t)(ver >> 24);
    out_buf[5] = (uint8_t)(ver >> 16);
    out_buf[6] = (uint8_t)(ver >> 8);
    out_buf[7] = (uint8_t)ver;

    out_buf[8] = (uint8_t)(cnt >> 24);
    out_buf[9] = (uint8_t)(cnt >> 16);
    out_buf[10] = (uint8_t)(cnt >> 8);
    out_buf[11] = (uint8_t)cnt;

    uint32_t pos = 12;
    for (uint32_t idx = 0; idx < entry->ext.item_count; idx++) {
        Mp4MtdtExtItem *item = &entry->ext.items[idx];
        uint32_t mid = item->media_id;
        uint32_t sidx = item->sample_index;
        uint32_t flg = item->item_flags;

        out_buf[pos + 0] = (uint8_t)(mid >> 24);
        out_buf[pos + 1] = (uint8_t)(mid >> 16);
        out_buf[pos + 2] = (uint8_t)(mid >> 8);
        out_buf[pos + 3] = (uint8_t)mid;

        out_buf[pos + 4] = (uint8_t)(sidx >> 24);
        out_buf[pos + 5] = (uint8_t)(sidx >> 16);
        out_buf[pos + 6] = (uint8_t)(sidx >> 8);
        out_buf[pos + 7] = (uint8_t)sidx;

        out_buf[pos + 8] = (uint8_t)(flg >> 24);
        out_buf[pos + 9] = (uint8_t)(flg >> 16);
        out_buf[pos + 10] = (uint8_t)(flg >> 8);
        out_buf[pos + 11] = (uint8_t)flg;
        pos += 12;
    }
    return MP4_OK;
}

/* 0x0046a0 */
MP4_NOINLINE int32_t mp4CopyMtdtPayload(Mp4MtdtEntry *entry, uint32_t buf_size, uint8_t *out_buf)
{
    uint32_t entry_sz = entry->size;
    if (entry_sz < 10U) {
        return MP4_ERR_CORRUPT;
    }
    if (entry_sz != 10U) {
        uint32_t len = entry_sz - 10U;
        if (entry->encoding != 257U) {
            return mp4SafeMemcpy(out_buf, buf_size, entry->ext.raw_payload, len);
        }
        int32_t ret = mp4SerializeMtdtExt(entry, out_buf);
        if (ret != MP4_OK) {
            return ret;
        }
    }
    return MP4_OK;
}

/* 0x00470c */
MP4_NOINLINE int32_t mp4GetMtdtCount(Mp4Handle *handle, Mp4Track *track, uint32_t *out_count)
{
    Mp4UsmtBox *usmt = NULL;
    if (track != NULL) {
        usmt = track->usmt;
    } else {
        Mp4Movie *movie = handle->movie;
        if (movie->size == 0) {
            return MP4_ERR_CORRUPT;
        }
        usmt = movie->usmt;
    }

    if (usmt != NULL && usmt->mtdt.size != 0) {
        *out_count = usmt->mtdt.entry_count;
    } else {
        *out_count = 0;
    }
    return MP4_OK;
}

/* 0x004770 */
MP4_NOINLINE int32_t mp4GetMtdtByIndex(Mp4Handle *handle, Mp4Track *track, uint32_t index,
                                       Mp4MtdtEntry **out_entry, uint32_t *out_tag,
                                       uint16_t *out_lang, uint16_t *out_enc, uint16_t *out_len)
{
    Mp4MtdtBox *mtdt = NULL;
    if (track != NULL) {
        if (track->usmt == NULL || track->usmt->mtdt.size == 0) {
            return MP4_ERR_NO_ENTRY;
        }
        mtdt = &track->usmt->mtdt;
    } else {
        Mp4Movie *movie = handle->movie;
        if (movie->size == 0) {
            return MP4_ERR_CORRUPT;
        }
        if (movie->usmt == NULL || movie->usmt->mtdt.size == 0) {
            return MP4_ERR_NO_ENTRY;
        }
        mtdt = &movie->usmt->mtdt;
    }

    if (index == 0) {
        return MP4_ERR_INVALID_PARAM;
    }
    if (mtdt->entry_count < index) {
        return MP4_ERR_NO_ENTRY;
    }
    Mp4MtdtEntry *ent = &mtdt->entries[index - 1U];
    if (ent->size < 10U) {
        return MP4_ERR_CORRUPT;
    }
    *out_entry = ent;
    *out_lang = ent->language;
    *out_enc = ent->encoding;
    *out_tag = ent->tag;
    *out_len = (uint16_t)(ent->size - 10U);
    return MP4_OK;
}

/* 0x004830 */
MP4_NOINLINE int32_t mp4FindMtdfEntry(Mp4Handle *handle, Mp4Track *track, uint32_t tag,
                                      uint16_t *out_lang, uint16_t *out_enc, void *out_buf)
{
    Mp4MtdfBox *mtdf = NULL;
    if (track != NULL) {
        if (track->usmt == NULL || track->usmt->mtdf.size == 0) {
            return MP4_ERR_NO_ENTRY;
        }
        mtdf = &track->usmt->mtdf;
    } else {
        Mp4Movie *movie = handle->movie;
        if (movie->size == 0) {
            return MP4_ERR_CORRUPT;
        }
        if (movie->usmt == NULL || movie->usmt->mtdf.size == 0) {
            return MP4_ERR_NO_ENTRY;
        }
        mtdf = &movie->usmt->mtdf;
    }

    Mp4MtdfEntry *found = NULL;
    for (uint32_t idx = 0; idx < mtdf->entry_count; idx++) {
        if (mtdf->entries[idx].tag == tag) {
            found = &mtdf->entries[idx];
            break;
        }
    }
    if (found == NULL) {
        return MP4_ERR_NO_ENTRY;
    }

    *out_lang = found->language;
    *out_enc = found->encoding;
    if (found->size < 10U) {
        return MP4_ERR_CORRUPT;
    }
    uint32_t len = (uint32_t)found->size - 10U;
    if (len == 0) {
        return MP4_OK;
    }
    if (found->encoding == 1U && len > 32U) {
        uint8_t *dst = (uint8_t *)out_buf;
        mp4SafeMemcpy(dst, 32, found->payload, 30);
        dst[31] = 0;
        dst[30] = 0;
        return MP4_OK;
    }
    if (len > 32U) {
        len = 32U;
    }
    mp4SafeMemcpy(out_buf, 32, found->payload, len);
    return MP4_OK;
}

/* 0x00495c */
MP4_NOINLINE int32_t mp4GetMtdfByIndex(Mp4Handle *handle, Mp4Track *track, uint32_t index,
                                       uint32_t *out_tag, uint16_t *out_lang,
                                       uint16_t *out_enc, void *out_buf)
{
    Mp4MtdfBox *mtdf = NULL;
    if (track != NULL) {
        if (track->usmt == NULL || track->usmt->mtdf.size == 0) {
            return MP4_ERR_NO_ENTRY;
        }
        mtdf = &track->usmt->mtdf;
    } else {
        Mp4Movie *movie = handle->movie;
        if (movie->size == 0) {
            return MP4_ERR_CORRUPT;
        }
        if (movie->usmt == NULL || movie->usmt->mtdf.size == 0) {
            return MP4_ERR_NO_ENTRY;
        }
        mtdf = &movie->usmt->mtdf;
    }

    if (index == 0) {
        return MP4_ERR_INVALID_PARAM;
    }
    if (mtdf->entry_count < index) {
        return MP4_ERR_NO_ENTRY;
    }
    Mp4MtdfEntry *ent = &mtdf->entries[index - 1U];
    *out_lang = ent->language;
    *out_tag = ent->tag;
    *out_enc = ent->encoding;
    if (ent->size < 10U) {
        return MP4_ERR_CORRUPT;
    }
    uint32_t len = (uint32_t)ent->size - 10U;
    if (ent->encoding == 1U) {
        if (len == 0) {
            return MP4_OK;
        }
        if (len > 32U) {
            uint8_t *dst = (uint8_t *)out_buf;
            mp4SafeMemcpy(dst, 32, ent->payload, 30);
            dst[31] = 0;
            dst[30] = 0;
            return MP4_OK;
        }
        mp4SafeMemcpy(out_buf, 32, ent->payload, len);
        return MP4_OK;
    }
    if (len != 0) {
        if (len > 32U) {
            len = 32U;
        }
        mp4SafeMemcpy(out_buf, 32, ent->payload, len);
    }
    return MP4_OK;
}

/* 0x004a8c */
MP4_NOINLINE int32_t mp4GetAprfEntry(Mp4Handle *handle, uint32_t index, uint32_t *out_words)
{
    Mp4Movie *movie = handle->movie;
    if (movie->prof.size == 0) {
        return MP4_ERR_CORRUPT;
    }
    if (index == 0) {
        return MP4_ERR_INVALID_PARAM;
    }
    if (movie->prof.aprf_count == 0 || movie->prof.aprf_count < index) {
        return MP4_ERR_NO_ENTRY;
    }
    Mp4AprfBox *aprf = &movie->prof.aprf_list[index - 1U];
    out_words[0] = aprf->track_id;
    out_words[1] = aprf->codec_tag;
    out_words[2] = aprf->codec_attr;
    out_words[3] = aprf->avg_bitrate;
    out_words[4] = aprf->max_bitrate;
    out_words[5] = aprf->sample_rate;
    out_words[6] = aprf->channel_count;
    out_words[7] = aprf->extra_attr;
    return MP4_OK;
}

/* 0x004b20 */
MP4_NOINLINE int32_t mp4GetVprfEntry(Mp4Handle *handle, uint32_t index, uint32_t *out_words)
{
    Mp4Movie *movie = handle->movie;
    if (movie->prof.size == 0) {
        return MP4_ERR_CORRUPT;
    }
    if (index == 0) {
        return MP4_ERR_INVALID_PARAM;
    }
    if (movie->prof.vprf_count == 0 || movie->prof.vprf_count < index) {
        return MP4_ERR_NO_ENTRY;
    }
    Mp4VprfBox *vprf = &movie->prof.vprf_list[index - 1U];
    out_words[0] = vprf->track_id;
    out_words[1] = vprf->codec_tag;
    out_words[2] = vprf->codec_attr;
    out_words[3] = vprf->avg_bitrate;
    out_words[4] = vprf->max_bitrate;
    out_words[5] = vprf->avg_frame_rate;
    out_words[6] = vprf->max_frame_rate;
    out_words[7] = vprf->width;
    out_words[8] = vprf->height;
    out_words[9] = vprf->pixel_aspect;
    return MP4_OK;
}

/* 0x004bc4 */
MP4_NOINLINE int32_t mp4GetOdsmCount(Mp4Handle *handle, uint16_t *out_count)
{
    Mp4IodsBox *iods = handle->movie->iods;
    if (iods == NULL || iods->od_track == NULL || iods->od_track->odsm_ctx == NULL) {
        return MP4_ERR_NO_ENTRY;
    }
    mp4OdsmGetObjectCount(iods->od_track->odsm_ctx, out_count);
    return MP4_OK;
}

/* 0x004c10 */
MP4_NOINLINE int32_t mp4GetOdsmObject(Mp4Handle *handle, uint16_t index, Mp4OdsmObject **out_obj)
{
    Mp4IodsBox *iods = handle->movie->iods;
    if (iods == NULL || iods->od_track == NULL || iods->od_track->odsm_ctx == NULL) {
        return MP4_ERR_NO_ENTRY;
    }
    Mp4Track *od_track = iods->od_track;
    int32_t status = mp4OdsmGetObjectAt(od_track->odsm_ctx, index, out_obj);
    if (status == MP4_OK) {
        (*out_obj)->od_track = od_track;
    }
    return status;
}

/* 0x004c84 */
MP4_NOINLINE int32_t mp4GetOdsmTrackInfo(Mp4OdsmObject *obj, Mp4OdsmTrackInfo *out_info)
{
    if (obj->od_track == NULL) {
        return MP4_ERR_INVALID_HANDLE;
    }
    if (obj->es_id_count == 0) {
        return MP4_ERR_CORRUPT;
    }
    Mp4Track *ref_track;
    int32_t status = mp4GetTrefTrack(obj->od_track, MP4_TAG_MPOD, obj->es_ids[0], &ref_track);
    if (status == MP4_OK) {
        out_info->track = ref_track;
        out_info->es_index_count = obj->es_index_count;
    }
    return status;
}

/* 0x004d00 */
MP4_NOINLINE int32_t mp4GetOdsmEsHeader(Mp4OdsmObject *obj, uint8_t index,
                                        uint16_t *out_type, uint32_t *out_size)
{
    if (obj->od_track == NULL) {
        return MP4_ERR_INVALID_HANDLE;
    }
    void *ctx = obj->od_track->odsm_ctx;
    if (ctx == NULL) {
        return MP4_ERR_CORRUPT;
    }
    if (index == 0) {
        return MP4_ERR_INVALID_PARAM;
    }
    if (obj->es_index_count < index) {
        return MP4_ERR_NO_ENTRY;
    }
    Mp4OdsmEsItem *item;
    if (mp4OdsmFindEsItem(ctx, obj->es_indices[index - 1U], &item) != MP4_OK) {
        return MP4_ERR_CORRUPT;
    }
    *out_size = item->payload_size;
    *out_type = item->stream_type;
    return MP4_OK;
}

/* 0x004d9c */
MP4_NOINLINE int32_t mp4GetOdsmEsPayload(Mp4OdsmObject *obj, uint8_t index,
                                         uint32_t buf_size, void *out_buf)
{
    if (obj->od_track == NULL) {
        return MP4_ERR_INVALID_HANDLE;
    }
    void *ctx = obj->od_track->odsm_ctx;
    if (ctx == NULL) {
        return MP4_ERR_CORRUPT;
    }
    if (index == 0) {
        return MP4_ERR_INVALID_PARAM;
    }
    if (obj->es_index_count < index) {
        return MP4_ERR_NO_ENTRY;
    }
    Mp4OdsmEsItem *item;
    if (mp4OdsmFindEsItem(ctx, obj->es_indices[index - 1U], &item) != MP4_OK) {
        return MP4_ERR_CORRUPT;
    }
    return mp4SafeMemcpy(out_buf, buf_size, item->payload, item->payload_size);
}

/* 0x004e3c */
MP4_NOINLINE int32_t mp4GetEsConfig(Mp4SampleEntry *entry, Mp4EsConfigOut *out_cfg)
{
    Mp4EsdsBox *esds = entry->esds;
    if (esds == NULL) {
        return MP4_ERR_CORRUPT;
    }
    Mp4DescNode *node;
    if (mp4DescParseEsds(esds->size - 12U, esds->payload, &node) != MP4_OK) {
        return MP4_ERR_CORRUPT;
    }
    Mp4EsDescBody *body = (Mp4EsDescBody *)node->body;
    if (body->has_dec_cfg == 0) {
        return MP4_ERR_CORRUPT;
    }
    out_cfg->object_type_ind = body->object_type_ind;
    out_cfg->stream_type_byte = body->stream_type_byte;
    out_cfg->buffer_size_db = body->buffer_size_db;
    out_cfg->max_bitrate = body->max_bitrate;
    out_cfg->avg_bitrate = body->avg_bitrate;
    out_cfg->dec_specific_size = body->dec_specific_size;
    mp4DescFree(node);
    return MP4_OK;
}

/* 0x004ed4 */
MP4_NOINLINE int32_t mp4GetEsDecSpecific(Mp4SampleEntry *entry, int32_t check_size,
                                         uint32_t buf_size, void *out_buf)
{
    Mp4EsdsBox *esds = entry->esds;
    if (esds == NULL) {
        return MP4_ERR_CORRUPT;
    }
    Mp4DescNode *node;
    if (mp4DescParseEsds(esds->size - 12U, esds->payload, &node) != MP4_OK) {
        return MP4_ERR_CORRUPT;
    }
    Mp4EsDescBody *body = (Mp4EsDescBody *)node->body;
    if (body->has_dec_cfg == 0) {
        mp4DescFree(node);
        return MP4_ERR_CORRUPT;
    }
    uint32_t dst_cap = (check_size == 0) ? body->dec_specific_size : buf_size;
    int32_t status = mp4SafeMemcpy(out_buf, dst_cap, body->dec_specific_info,
                                   body->dec_specific_size);
    mp4DescFree(node);
    return status;
}

/* 0x004f8c */
MP4_NOINLINE int32_t mp4GetHintSampleDesc(Mp4SampleEntry *entry, Mp4HintSampleDescOut *out_desc)
{
    Mp4HintDesc *hd = (Mp4HintDesc *)entry->codec_desc;
    if (hd == NULL) {
        return MP4_ERR_INVALID_HANDLE;
    }
    if (out_desc == NULL) {
        return MP4_ERR_INVALID_PARAM;
    }
    out_desc->codec_tag = entry->tag;
    out_desc->data_ref = entry->data_ref;
    out_desc->version = hd->version;
    out_desc->width = hd->width;
    out_desc->height = hd->height;
    out_desc->horiz_res = hd->horiz_res;
    out_desc->vert_res = hd->vert_res;
    out_desc->depth = hd->depth;
    out_desc->color_id = hd->color_id;
    out_desc->bit_flags = hd->bit_flags;
    return MP4_OK;
}

/* 0x004ffc */
MP4_NOINLINE int32_t mp4GetAvcUuidSize(Mp4SampleEntry *entry, uint32_t *out_size)
{
    if (entry->tag != MP4_TAG_AVC1) {
        return MP4_ERR_CODEC_MISMATCH;
    }
    if (entry->avcc == NULL) {
        return MP4_ERR_CORRUPT;
    }
    if (entry->uuid_payload == NULL) {
        return MP4_ERR_NO_ENTRY;
    }
    *out_size = entry->uuid_size;
    return MP4_OK;
}

/* 0x00503c */
MP4_NOINLINE int32_t mp4GetFtypBrand(Mp4Handle *handle, uint32_t *out_major, uint32_t *out_minor)
{
    Mp4Movie *movie = handle->movie;
    if (movie->ftyp.size == 0) {
        return MP4_ERR_CORRUPT;
    }
    *out_major = movie->ftyp.major_brand;
    *out_minor = movie->ftyp.minor_version;
    return MP4_OK;
}

/* 0x005068 */
MP4_NOINLINE int32_t mp4GetMovieTimes(Mp4Handle *handle, Mp4DateTime *out_ctime,
                                      Mp4DateTime *out_mtime)
{
    Mp4Movie *movie = handle->movie;
    if (movie->mvhd.size == 0) {
        return MP4_ERR_CORRUPT;
    }
    mp4SecToDateTime(movie->mvhd.creation_time, out_ctime);
    mp4SecToDateTime(handle->movie->mvhd.modification_time, out_mtime);
    return MP4_OK;
}

/* 0x0050c4 */
MP4_NOINLINE int32_t mp4GetMovieTimescale(Mp4Handle *handle, uint32_t *out_scale)
{
    Mp4Movie *movie = handle->movie;
    if (movie->mvhd.size == 0) {
        return MP4_ERR_CORRUPT;
    }
    *out_scale = movie->mvhd.timescale;
    return MP4_OK;
}

/* 0x0050e8 */
MP4_NOINLINE int32_t mp4GetMovieDuration(Mp4Handle *handle, uint32_t *out_dur)
{
    int32_t status = mp4MovieCalcDuration(handle->movie, out_dur, 1);
    if (status == MP4_ERR_NOT_READY) {
        return MP4_ERR_CORRUPT;
    }
    if ((status == MP4_ERR_COUNT_OVERFLOW) | ((uint32_t)(status - MP4_ERR_FRAG_OVERFLOW) < 2U)) {
        status = MP4_ERR_OVERFLOW;
    }
    return status;
}

/* 0x005138 */
MP4_NOINLINE int32_t mp4GetTrackCount(Mp4Handle *handle, uint16_t *out_count)
{
    Mp4Movie *movie = handle->movie;
    if (movie->size == 0) {
        return MP4_ERR_CORRUPT;
    }
    *out_count = (uint16_t)movie->track_count;
    return MP4_OK;
}

/* 0x00515c */
MP4_NOINLINE int32_t mp4GetTrackById(Mp4Handle *handle, uint32_t track_id, Mp4Track **out_track)
{
    Mp4Movie *movie = handle->movie;
    if (movie->size == 0) {
        return MP4_ERR_CORRUPT;
    }
    if (track_id == 0) {
        return MP4_ERR_INVALID_PARAM;
    }
    Mp4Track *trk;
    if (mp4MovieFindTrackById(movie, track_id, &trk) != MP4_OK) {
        return MP4_ERR_NO_ENTRY;
    }
    *out_track = trk;
    return MP4_OK;
}

/* 0x0051b8 */
MP4_NOINLINE int32_t mp4GetElstCount(Mp4Track *track, uint32_t *out_count)
{
    if (track->edts.size == 0 || track->edts.elst.size == 0) {
        return MP4_ERR_NO_ENTRY;
    }
    *out_count = track->edts.elst.entry_count;
    return MP4_OK;
}

/* 0x0051e4 */
MP4_NOINLINE int32_t mp4GetMediaTimescale(Mp4Track *track, uint32_t *out_scale)
{
    if (track->mdia.mdhd.size == 0) {
        return MP4_ERR_CORRUPT;
    }
    *out_scale = track->mdia.mdhd.timescale;
    return MP4_OK;
}

/* 0x005204 */
MP4_NOINLINE int32_t mp4GetHandlerType(Mp4Track *track, uint32_t *out_type)
{
    if (track->mdia.hdlr.size == 0) {
        return MP4_ERR_CORRUPT;
    }
    *out_type = track->mdia.hdlr.handler_type;
    return MP4_OK;
}

/* 0x005224 */
MP4_NOINLINE int32_t mp4GetTrackSampleCount(Mp4Track *track, uint32_t *out_count)
{
    int32_t status = mp4StblGetSampleCount(&track->mdia.minf.stbl, out_count, 1);
    if (status == MP4_ERR_NOT_READY) {
        return MP4_ERR_CORRUPT;
    }
    if ((status == MP4_ERR_COUNT_OVERFLOW) | ((uint32_t)(status - MP4_ERR_FRAG_OVERFLOW) < 2U)) {
        status = MP4_ERR_OVERFLOW;
    }
    return status;
}

/* 0x005274 */
MP4_NOINLINE int32_t mp4GetSampleDescCount(Mp4Track *track, uint32_t *out_count)
{
    if (track->mdia.minf.stbl.stsd.size == 0) {
        return MP4_ERR_CORRUPT;
    }
    *out_count = track->mdia.minf.stbl.stsd.entry_count;
    return MP4_OK;
}

/* 0x005294 */
MP4_NOINLINE int32_t mp4GetVisualSampleDesc(Mp4SampleEntry *entry, Mp4VisualSampleDescOut *out_desc)
{
    if ((entry->handler_type != MP4_TAG_VIDE) & (entry->handler_type != 0xFFFFFFFEU)) {
        return MP4_ERR_INVALID_HANDLE;
    }
    Mp4VisualDesc *vd = (Mp4VisualDesc *)entry->codec_desc;
    out_desc->codec_tag = entry->tag;
    out_desc->reserved = 0;
    out_desc->data_ref = entry->data_ref;
    out_desc->vendor = vd->vendor;
    out_desc->horiz_res = vd->horiz_res;
    out_desc->vert_res = vd->vert_res;
    out_desc->width = vd->width;
    out_desc->height = vd->height;
    mp4SafeMemcpy(out_desc->compressor, 32, vd->compressor, 32);
    out_desc->depth = vd->depth;
    return MP4_OK;
}

/* 0x00534c */
MP4_NOINLINE int32_t mp4GetVisualBrief(Mp4SampleEntry *entry, Mp4VisualBriefOut *out_desc)
{
    Mp4VisualSampleDescOut full;

    if ((entry->handler_type != MP4_TAG_VIDE) & (entry->handler_type != 0xFFFFFFFEU)) {
        return MP4_ERR_INVALID_HANDLE;
    }
    Mp4VisualDesc *vd = (Mp4VisualDesc *)entry->codec_desc;
    full.codec_tag = entry->tag;
    full.data_ref = entry->data_ref;
    full.width = vd->width;
    full.horiz_res = vd->horiz_res;
    full.height = vd->height;
    full.vert_res = vd->vert_res;
    mp4SafeMemcpy(full.compressor, 32, vd->compressor, 32);
    full.depth = vd->depth;
    out_desc->vert_res = full.vert_res;
    out_desc->codec_tag = full.codec_tag;
    out_desc->data_ref = full.data_ref;
    out_desc->width = full.width;
    out_desc->height = full.height;
    out_desc->horiz_res = full.horiz_res;
    return MP4_OK;
}

/* 0x00543c */
MP4_NOINLINE int32_t mp4GetAudioBrief(Mp4SampleEntry *entry, Mp4AudioBriefOut *out_desc)
{
    Mp4AudioSampleDescOut full;
    int32_t status = mp4GetAudioSampleDesc(entry, NULL, &full);
    if (status == MP4_OK) {
        out_desc->codec_tag = full.codec_tag;
        out_desc->data_ref = full.data_ref;
        out_desc->channel_count = full.channel_count;
        out_desc->sample_rate = full.sample_rate;
    }
    return status;
}

/* 0x00548c */
MP4_NOINLINE int32_t mp4GetEsdsLen(Mp4SampleEntry *entry, uint32_t *out_len)
{
    Mp4EsdsBox *esds = entry->esds;
    if (esds != NULL && esds->size >= 12U) {
        *out_len = esds->size - 12U;
    } else {
        *out_len = 0;
    }
    return MP4_OK;
}

/* 0x0054bc */
MP4_NOINLINE int32_t mp4GetEsdsData(Mp4SampleEntry *entry, uint32_t buf_size, void *out_buf)
{
    Mp4EsdsBox *esds = entry->esds;
    if (esds == NULL || esds->size < 12U || esds->payload == NULL) {
        return MP4_ERR_CORRUPT;
    }
    return mp4SafeMemcpy(out_buf, buf_size, esds->payload, esds->size - 12U);
}

/* 0x005514 */
MP4_NOINLINE int32_t mp4GetAvccHeader(Mp4SampleEntry *entry, uint8_t *out_hdr,
                                      uint8_t *out_sps_cnt, uint8_t *out_pps_cnt)
{
    if (mp4MatchCodecTag(MP4_TAG_AVC1, entry) == 0) {
        return MP4_ERR_CODEC_MISMATCH;
    }
    Mp4AvccBox *avcc = entry->avcc;
    if (avcc == NULL) {
        return MP4_ERR_CORRUPT;
    }
    out_hdr[0] = avcc->profile_idc;
    out_hdr[1] = avcc->profile_compat;
    out_hdr[2] = avcc->level_idc;
    out_hdr[3] = avcc->length_size_minus;
    *out_sps_cnt = avcc->sps_count;
    *out_pps_cnt = avcc->pps_count;
    return MP4_OK;
}

/* 0x0055c8 */
MP4_NOINLINE int32_t mp4GetDrefLocationLen(Mp4DrefEntry *entry, uint32_t *out_len)
{
    if (entry->location != NULL) {
        *out_len = mp4CbStrlen(entry->location) + 1U;
    } else {
        *out_len = 0;
    }
    return MP4_OK;
}

/* 0x005610 */
MP4_NOINLINE int32_t mp4GetDrefLocation(Mp4DrefEntry *entry, uint32_t buf_size, char *out_buf)
{
    if (entry->location == NULL) {
        return MP4_ERR_NOT_FOUND;
    }
    uint32_t needed = mp4CbStrlen(entry->location) + 1U;
    if (buf_size < needed) {
        return MP4_ERR_INVALID_PARAM;
    }
    mp4CbStrncpy(out_buf, entry->location, buf_size);
    return MP4_OK;
}

/* 0x005688 */
MP4_NOINLINE int32_t mp4GetMoovOffset(Mp4Handle *handle, uint64_t *out_offset)
{
    Mp4Movie *movie = handle->movie;
    if (movie == NULL) {
        return MP4_ERR_INVALID_HANDLE;
    }
    if (movie->size == 0 || movie->tag != MP4_TAG_MOOV) {
        return MP4_ERR_CORRUPT;
    }
    *out_offset = movie->moov_offset;
    return MP4_OK;
}

/* 0x0056d4 */
MP4_NOINLINE int32_t mp4GetFprfAttr(Mp4Handle *handle, uint32_t *out_attr)
{
    Mp4Movie *movie = handle->movie;
    if (movie->prof.size == 0) {
        return MP4_ERR_CORRUPT;
    }
    if (movie->prof.fprf_count == 0) {
        return MP4_ERR_NO_ENTRY;
    }
    *out_attr = movie->prof.fprf_list[0].global_attr;
    return MP4_OK;
}

/* 0x005708 */
MP4_NOINLINE int32_t mp4GetIodsProfiles(Mp4Handle *handle, uint8_t *out_profiles)
{
    Mp4IodsBox *iods = handle->movie->iods;
    if (iods == NULL) {
        return MP4_ERR_NO_ENTRY;
    }
    Mp4DescNode *node;
    if (mp4DescParseIods(iods->size - 12U, iods->payload, &node) != MP4_OK) {
        return MP4_ERR_CORRUPT;
    }
    Mp4IodsBody *body = (Mp4IodsBody *)node->body;
    out_profiles[0] = body->audio_profile;
    out_profiles[1] = body->visual_profile;
    out_profiles[2] = body->od_profile;
    mp4DescFree(node);
    return MP4_OK;
}

/* 0x005784 */
MP4_NOINLINE int32_t mp4GetAvcUuidData(Mp4SampleEntry *entry, uint32_t buf_size, void *out_buf)
{
    uint32_t uuid_len = 0;
    int32_t status = mp4GetAvcUuidSize(entry, &uuid_len);
    if (status != MP4_OK) {
        return status;
    }
    if (buf_size < uuid_len) {
        return MP4_ERR_INVALID_PARAM;
    }
    mp4SafeMemcpy(out_buf, buf_size, entry->uuid_payload, entry->uuid_size);
    return MP4_OK;
}

/* 0x0057fc */
MP4_NOINLINE int32_t mp4MatchCodecTag(uint32_t target_tag, Mp4SampleEntry *entry)
{
    if (entry->tag == target_tag) {
        return 1;
    }
    if ((entry->tag == MP4_TAG_ENCV) | (entry->tag == MP4_TAG_ENCA)) {
        Mp4SinfBox *sinf = entry->sinf;
        if (sinf != NULL) {
            const uint8_t *p = sinf->payload;
            uint32_t orig_fmt = ((uint32_t)p[8] << 24) | ((uint32_t)p[9] << 16) |
                                ((uint32_t)p[10] << 8) | (uint32_t)p[11];
            return orig_fmt == target_tag ? 1 : 0;
        }
    }
    return 0;
}

/* 0x00588c */
MP4_NOINLINE uint32_t mp4ReadU32(Mp4IoStream *stream)
{
    uint32_t val = 0;
    if (mp4CbReadBe(&val, 4, stream->file_handle) != 4) {
        val = 0;
        mp4SetParseFlag(0, MP4_PARSE_FLAG_IO_FAIL, stream);
    }
    return val;
}

/* 0x0058e0 */
MP4_NOINLINE int32_t mp4ReadS32(Mp4IoStream *stream)
{
    int32_t val = 0;
    if (mp4CbReadBe(&val, 4, stream->file_handle) != 4) {
        val = 0;
        mp4SetParseFlag(0, MP4_PARSE_FLAG_IO_FAIL, stream);
    }
    return val;
}

/* 0x005934 */
MP4_NOINLINE uint16_t mp4ReadU16(Mp4IoStream *stream)
{
    uint16_t val = 0;
    if (mp4CbReadBe(&val, 2, stream->file_handle) != 2) {
        val = 0;
        mp4SetParseFlag(0, MP4_PARSE_FLAG_IO_FAIL, stream);
    }
    return val;
}

/* 0x005988 */
MP4_NOINLINE int16_t mp4ReadS16(Mp4IoStream *stream)
{
    int16_t val = 0;
    if (mp4CbReadBe(&val, 2, stream->file_handle) != 2) {
        val = 0;
        mp4SetParseFlag(0, MP4_PARSE_FLAG_IO_FAIL, stream);
    }
    return val;
}

/* 0x0059dc */
MP4_NOINLINE int32_t mp4ReadBytes(void *dst, uint32_t size, Mp4IoStream *stream)
{
    int32_t rd = mp4CbRead(dst, size, stream->file_handle);
    if ((uint32_t)rd != size) {
        mp4SetParseFlag(0, MP4_PARSE_FLAG_IO_FAIL, stream);
    }
    return rd;
}

/* 0x005a38 */
MP4_NOINLINE int32_t mp4ReadU32Array(void *dst, uint32_t num_bytes, Mp4IoStream *stream)
{
    uint8_t *cur = (uint8_t *)dst;
    int32_t rd = mp4CbRead(dst, num_bytes, stream->file_handle);
    if ((uint32_t)rd != num_bytes) {
        mp4SetParseFlag(0, MP4_PARSE_FLAG_IO_FAIL, stream);
    }
    uint32_t count = num_bytes >> 2;
    uint32_t *words = (uint32_t *)dst;
    for (uint32_t idx = 0; idx < count; idx++) {
        words[idx] = ((uint32_t)cur[0] << 24) + ((uint32_t)cur[1] << 16) +
                     ((uint32_t)cur[2] << 8) + (uint32_t)cur[3];
        cur += 4;
    }
    return rd;
}

/* 0x005ae4 */
MP4_NOINLINE uint32_t mp4ReadU32ToU64Array(uint8_t *dst, uint32_t num_bytes, Mp4IoStream *stream)
{
    uint32_t count = num_bytes >> 2;
    uint64_t *wide = (uint64_t *)dst;
    uint8_t *cur = dst;
    for (uint32_t idx = 0; idx < count; idx++) {
        if (mp4CbRead(cur, 4, stream->file_handle) != 4) {
            mp4SetParseFlag(0, MP4_PARSE_FLAG_IO_FAIL, stream);
        }
        uint32_t val = ((uint32_t)cur[0] << 24) + ((uint32_t)cur[1] << 16) +
                       ((uint32_t)cur[2] << 8) + (uint32_t)cur[3];
        wide[idx] = (uint64_t)val;
        cur += 8;
    }
    return num_bytes;
}

/* 0x005bb4 */
MP4_NOINLINE int32_t mp4AttachStream(Mp4IoStream *stream)
{
    if (stream->is_attached == 0) {
        return MP4_ERR_IO;
    }
    stream->file_handle = stream->user_handle;
    return MP4_OK;
}

/* 0x005bd4 */
MP4_NOINLINE int32_t mp4MfraCreate(Mp4MfraBox **out_mfra)
{
    Mp4MfraBox *mfra = (Mp4MfraBox *)mp4CbMalloc(sizeof(Mp4MfraBox));
    if (mfra == NULL) {
        return MP4_ERR_NOMEM;
    }
    mfra->size = 24;
    mfra->tag = MP4_TAG_MFRA;

    Mp4MfroBox *mfro;
    int32_t status = mp4MfroCreate(&mfro);
    if (status == MP4_OK) {
        mfra->mfro = mfro;
        Mp4DynArray *tfra_list;
        status = mp4DynArrayCreate(sizeof(Mp4TfraBox *), 10, &tfra_list);
        if (status == MP4_OK) {
            mfra->tfra_list = tfra_list;
            Mp4DynArray *other_list;
            status = mp4DynArrayCreate(sizeof(Mp4MfraOther *), 10, &other_list);
            if (status == MP4_OK) {
                *out_mfra = mfra;
                mfra->reserved_a = 0;
                mfra->incomplete_flag = 0;
                mfra->other_list = other_list;
                return MP4_OK;
            }
            mp4DynArrayDestroy(mfra->tfra_list);
        }
        mp4MfroDestroy(mfra->mfro);
    }
    mp4CbFree(mfra);
    return status;
}

/* 0x005cc0 */
MP4_NOINLINE int32_t mp4MfraDestroy(Mp4MfraBox *mfra)
{
    if (mfra->mfro != NULL) {
        mp4MfroDestroy(mfra->mfro);
    }

    uint32_t tfra_count = 0;
    int32_t status = mp4DynArrayCount(mfra->tfra_list, &tfra_count);
    if (status != MP4_OK) {
        return status;
    }
    for (uint32_t idx = 0; idx < tfra_count; idx++) {
        Mp4TfraBox *tfra = NULL;
        status = mp4DynArrayGet(mfra->tfra_list, idx, &tfra);
        if (status != MP4_OK) {
            return status;
        }
        mp4TfraDestroy(tfra);
    }
    if (mfra->tfra_list != NULL) {
        mp4DynArrayDestroy(mfra->tfra_list);
    }

    uint32_t other_count = 0;
    status = mp4DynArrayCount(mfra->other_list, &other_count);
    if (status != MP4_OK) {
        return status;
    }
    for (uint32_t idx = 0; idx < other_count; idx++) {
        Mp4MfraOther *other = NULL;
        status = mp4DynArrayGet(mfra->other_list, idx, &other);
        if (status != MP4_OK) {
            return status;
        }
        mp4MfraOtherDestroy(other);
    }
    if (mfra->other_list != NULL) {
        mp4DynArrayDestroy(mfra->other_list);
    }
    if (mfra != NULL) {
        mp4CbFree(mfra);
    }
    return MP4_OK;
}

/* 0x005e0c */
MP4_NOINLINE int32_t mp4MfraLookupByTime(Mp4MfraBox *mfra, uint32_t track_id,
                                         uint32_t target_time, uint32_t *out_found,
                                         Mp4TfraEntry *out_entry)
{
    uint32_t tfra_count = 0;
    int32_t status = mp4DynArrayCount(mfra->tfra_list, &tfra_count);
    if (status != MP4_OK) {
        return status;
    }

    for (uint32_t idx = 0; idx < tfra_count; idx++) {
        Mp4TfraBox *tfra = NULL;
        status = mp4DynArrayGet(mfra->tfra_list, idx, &tfra);
        if (status != MP4_OK) {
            return status;
        }
        uint32_t tid = 0;
        status = mp4TfraGetTrackId(tfra, &tid);
        if (status != MP4_OK) {
            return status;
        }
        if (track_id < tid) {
            *out_found = 0;
            return MP4_OK;
        }
        if (track_id == tid) {
            uint32_t entry_count = 0;
            status = mp4TfraGetEntryCount(tfra, &entry_count);
            if (status != MP4_OK) {
                return status;
            }
            if (entry_count == 0) {
                *out_found = 0;
                return MP4_OK;
            }
            Mp4TfraEntry cur;
            status = mp4TfraGetEntry(tfra, 1, &cur);
            if (status != MP4_OK) {
                return status;
            }
            if (cur.time_lo == target_time && cur.time_hi == 0) {
                *out_found = 1;
                *out_entry = cur;
                return MP4_OK;
            }
            if (target_time < cur.time_lo) {
                *out_found = 0;
                return MP4_OK;
            }
            for (uint32_t eidx = 2; eidx <= entry_count; eidx++) {
                Mp4TfraEntry prev = cur;
                status = mp4TfraGetEntry(tfra, eidx, &cur);
                if (status != MP4_OK) {
                    return status;
                }
                if (cur.time_lo == target_time && cur.time_hi == 0) {
                    *out_found = 1;
                    *out_entry = cur;
                    return MP4_OK;
                }
                if (target_time < cur.time_lo) {
                    *out_found = 1;
                    *out_entry = prev;
                    return MP4_OK;
                }
            }
            *out_found = 1;
            *out_entry = cur;
            return MP4_OK;
        }
    }

    *out_found = 0;
    return MP4_OK;
}
