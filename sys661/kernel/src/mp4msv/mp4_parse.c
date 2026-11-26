#include "mp4msv_internal.h"

/* 0x00bb84 */
MP4_NOINLINE int32_t mp4DivU64ByU32(uint32_t hi, uint32_t lo, uint32_t divisor,
                                    uint32_t *out_q_hi, uint32_t *out_q_lo, uint32_t *out_rem)
{
    uint32_t rem_hi = hi;
    uint32_t rem_lo = lo;

    if (divisor == 0) {
        *out_rem = 0;
        *out_q_lo = 0;
        *out_q_hi = 0;
        return MP4_ERR_PARSE_FATAL;
    }

    *out_q_hi = 0;
    *out_q_lo = 0;
    *out_rem = 0;

    uint32_t div_msb = 0;
    for (uint32_t bit = 0; bit < 32U; bit++) {
        if ((divisor & (1U << bit)) != 0) {
            div_msb = bit;
        }
    }

    for (;;) {
        uint32_t num_msb = 0;
        for (uint32_t bit = 0; bit < 32U; bit++) {
            if ((rem_lo & (1U << bit)) != 0) {
                num_msb = bit;
            }
        }
        for (uint32_t bit = 0; bit < 32U; bit++) {
            if ((rem_hi & (1U << bit)) != 0) {
                num_msb = bit + 32U;
            }
        }
        if (div_msb >= num_msb) {
            if (num_msb == div_msb && rem_lo >= divisor) {
                if (mp4SubU64(&rem_hi, &rem_lo, 0, divisor) != MP4_OK) {
                    return MP4_ERR_SUB_UNDERFLOW;
                }
                if (mp4AddU64(out_q_hi, out_q_lo, 0, 1) != MP4_OK) {
                    return MP4_ERR_ADD_OVERFLOW;
                }
                continue;
            }
            break;
        }

        uint32_t shift = num_msb - div_msb;
        uint32_t sh_hi = 0;
        uint32_t sh_lo = 0;
        uint32_t bit_hi = 0;
        uint32_t bit_lo = 0;
        mp4ShiftLeftU32ToU64(divisor, shift, &sh_hi, &sh_lo);
        mp4ShiftLeftU32ToU64(1, shift, &bit_hi, &bit_lo);
        if (rem_hi < sh_hi || (rem_hi == sh_hi && rem_lo < sh_lo)) {
            shift--;
            mp4ShiftLeftU32ToU64(divisor, shift, &sh_hi, &sh_lo);
            mp4ShiftLeftU32ToU64(1, shift, &bit_hi, &bit_lo);
        }

        if (mp4SubU64(&rem_hi, &rem_lo, sh_hi, sh_lo) != MP4_OK) {
            return MP4_ERR_SUB_UNDERFLOW;
        }
        if (mp4AddU64(out_q_hi, out_q_lo, bit_hi, bit_lo) != MP4_OK) {
            return MP4_ERR_ADD_OVERFLOW;
        }
    }

    *out_rem = rem_lo;
    return MP4_OK;
}

/* 0x00bdd0 */
MP4_NOINLINE int32_t mp4ShiftLeftU32ToU64(uint32_t val, uint32_t shift,
                                         uint32_t *out_hi, uint32_t *out_lo)
{
    if (shift >= 64U) {
        *out_hi = 0;
        *out_lo = 0;
        return MP4_OK;
    }
    if (shift - 32U < 32U) {
        *out_hi = val << (shift - 32U);
        *out_lo = 0;
        return MP4_OK;
    }
    if (shift - 1U < 31U) {
        *out_hi = val >> (32U - shift);
        *out_lo = val << shift;
        return MP4_OK;
    }
    *out_hi = 0;
    *out_lo = val;
    return MP4_OK;
}

/* 0x00be30 */
MP4_NOINLINE int32_t mp4SubU64(uint32_t *hi_ptr, uint32_t *lo_ptr,
                               uint32_t rhs_hi, uint32_t rhs_lo)
{
    uint32_t cur_lo = *lo_ptr;
    uint32_t cur_hi = *hi_ptr;
    uint32_t borrow = (cur_lo < rhs_lo) ? 1U : 0U;
    if (cur_hi < rhs_hi + borrow) {
        return MP4_ERR_SUB_UNDERFLOW;
    }
    *hi_ptr = cur_hi - rhs_hi - borrow;
    *lo_ptr = cur_lo - rhs_lo;
    return MP4_OK;
}

/* 0x00be6c */
MP4_NOINLINE int32_t mp4AddU64(uint32_t *hi_ptr, uint32_t *lo_ptr,
                               uint32_t add_hi, uint32_t add_lo)
{
    uint32_t cur_lo = *lo_ptr;
    uint32_t cur_hi = *hi_ptr;

    uint32_t lo_low = (cur_lo & 0xFFFFU) + (add_lo & 0xFFFFU);
    uint32_t lo_high = (cur_lo >> 16) + (add_lo >> 16) + (lo_low >> 16);
    uint32_t new_lo = (lo_high << 16) + (lo_low & 0xFFFFU);

    uint32_t carry = lo_high >> 16;
    uint32_t hi_low = (cur_hi & 0xFFFFU) + (add_hi & 0xFFFFU) + carry;
    uint32_t hi_high = (cur_hi >> 16) + (add_hi >> 16) + (hi_low >> 16);
    uint32_t new_hi = (hi_high << 16) + (hi_low & 0xFFFFU);

    if ((hi_high >> 16) != 0) {
        return MP4_ERR_ADD_OVERFLOW;
    }
    *hi_ptr = new_hi;
    *lo_ptr = new_lo;
    return MP4_OK;
}

/* 0x00bf14 */
MP4_NOINLINE int32_t mp4AdjustU64(uint32_t *hi_ptr, uint32_t *lo_ptr, int32_t delta)
{
    int32_t err;

    if (delta < 0) {
        err = mp4SubU64(hi_ptr, lo_ptr, 0, (uint32_t)(-delta));
        if (err != MP4_OK) {
            return MP4_ERR_SUB_UNDERFLOW;
        }
    } else {
        err = mp4AddU64(hi_ptr, lo_ptr, 0, (uint32_t)delta);
        if (err != MP4_OK) {
            return MP4_ERR_ADD_OVERFLOW;
        }
    }
    return MP4_OK;
}

/* 0x00bf64 */
MP4_NOINLINE int32_t mp4AddS32ToU64(uint64_t *val_ptr, int32_t delta)
{
    if (delta < 0) {
        uint32_t mag = (uint32_t)(-delta);
        if (*val_ptr < (uint64_t)mag) {
            return MP4_ERR_SUB_UNDERFLOW;
        }
        *val_ptr -= (uint64_t)mag;
        return MP4_OK;
    }
    uint64_t cur = *val_ptr;
    uint64_t next = cur + (uint64_t)(uint32_t)delta;
    if (next < cur) {
        return MP4_ERR_ADD_OVERFLOW;
    }
    *val_ptr = next;
    return MP4_OK;
}

/* 0x00c03c */
MP4_NOINLINE int32_t mp4TrackResolveDrefs(Mp4Track *track)
{
    Mp4Stbl *stbl = &track->mdia.minf.stbl;
    int32_t failed = 0;

    for (uint32_t idx = 0; idx < stbl->stsd.entry_count; idx++) {
        Mp4SampleEntry *entry = NULL;
        if (mp4StsdGetEntry(&stbl->stsd, idx + 1U, &entry) != MP4_OK) {
            failed = 1;
        } else if (mp4DrefGetEntry(entry->data_ref_index, &track->mdia.minf.dinf.dref,
                                   &entry->data_ref) != MP4_OK) {
            failed = 1;
        }
    }
    return (failed == 1) ? MP4_ERR_PARSE_FATAL : MP4_OK;
}

/* 0x00c0ec */
MP4_NOINLINE int32_t mp4MtdfSetEntry(Mp4MtdfBox *mtdf, uint16_t size, uint32_t tag,
                                     uint16_t lang, uint16_t enc, uint32_t index,
                                     const void *payload)
{
    if (mtdf->entries == NULL) {
        mtdf->entries = (Mp4MtdfEntry *)mp4CbCalloc(sizeof(Mp4MtdfEntry) * 10U);
        if (mtdf->entries == NULL) {
            return MP4_ERR_NOMEM;
        }
        mtdf->capacity = 10;
    }

    if (index >= mtdf->capacity) {
        if (index != mtdf->capacity) {
            return MP4_ERR_INVALID_PARAM;
        }
        uint32_t old_bytes = sizeof(Mp4MtdfEntry) * index;
        uint32_t new_bytes = old_bytes + sizeof(Mp4MtdfEntry) * 10U;
        Mp4MtdfEntry *grown = (Mp4MtdfEntry *)mp4CbCalloc(new_bytes);
        if (grown == NULL) {
            return MP4_ERR_NOMEM;
        }
        uint32_t copy_len = (old_bytes < new_bytes) ? old_bytes : new_bytes;
        mp4SafeMemcpy(grown, new_bytes, mtdf->entries, copy_len);
        mp4CbFree(mtdf->entries);
        mtdf->entries = grown;
        mtdf->capacity += 10U;
    }

    Mp4MtdfEntry *dst = &mtdf->entries[index];
    dst->size = size;
    dst->tag = tag;
    dst->language = lang;
    dst->encoding = enc;
    if (size >= 11U) {
        if (size < 42U) {
            mp4SafeMemcpy(dst->payload, 32, payload, (uint32_t)size - 10U);
        } else {
            mp4SafeMemcpy(dst->payload, 32, payload, 31);
            mtdf->entries[index].payload[31] = 0;
        }
    }
    if (index >= mtdf->entry_count) {
        mtdf->entry_count++;
    }
    return MP4_OK;
}

/* 0x00c298 */
MP4_NOINLINE int32_t mp4MtdtReserveEntry(Mp4MtdtBox *mtdt, uint16_t size, uint32_t tag,
                                         uint16_t lang, uint16_t enc, uint32_t index)
{
    if (mtdt->entries == NULL) {
        mtdt->entries = (Mp4MtdtEntry *)mp4CbCalloc(sizeof(Mp4MtdtEntry) * 10U);
        if (mtdt->entries == NULL) {
            return MP4_ERR_NOMEM;
        }
        mtdt->capacity = 10;
    }

    if (index >= mtdt->capacity) {
        uint32_t old_bytes = sizeof(Mp4MtdtEntry) * mtdt->capacity;
        uint32_t new_bytes = old_bytes + sizeof(Mp4MtdtEntry) * 10U;
        Mp4MtdtEntry *grown = (Mp4MtdtEntry *)mp4CbCalloc(new_bytes);
        if (grown == NULL) {
            return MP4_ERR_NOMEM;
        }
        uint32_t copy_len = (old_bytes < new_bytes) ? old_bytes : new_bytes;
        mp4SafeMemcpy(grown, new_bytes, mtdt->entries, copy_len);
        mp4CbFree(mtdt->entries);
        mtdt->entries = grown;
        mtdt->capacity += 10U;
    }

    Mp4MtdtEntry *dst = &mtdt->entries[index];
    uint8_t *old_payload = dst->ext.raw_payload;
    dst->size = size;
    dst->tag = tag;
    dst->language = lang;
    dst->encoding = enc;
    mp4MemFree(old_payload);
    mtdt->entries[index].ext.raw_payload = NULL;
    if (index >= mtdt->entry_count) {
        mtdt->entry_count++;
    }
    return MP4_OK;
}

/* 0x00c404 */
MP4_NOINLINE int32_t mp4MtdtSetEntry(Mp4MtdtBox *mtdt, uint16_t size, uint32_t tag,
                                     uint16_t lang, uint16_t enc, uint32_t index,
                                     const void *payload)
{
    int32_t status = mp4MtdtReserveEntry(mtdt, size, tag, lang, enc, index);
    if (status != MP4_OK) {
        return status;
    }
    if (size < 11U) {
        return MP4_OK;
    }
    uint32_t body_len = (uint32_t)size - 10U;
    mtdt->entries[index].ext.raw_payload = (uint8_t *)mp4CbCalloc(body_len);
    if (mtdt->entries[index].ext.raw_payload == NULL) {
        return MP4_ERR_NOMEM;
    }
    mp4SafeMemcpy(mtdt->entries[index].ext.raw_payload, body_len, payload, body_len);
    return MP4_OK;
}

/* 0x00c4d0 */
MP4_NOINLINE void *mp4MemCalloc(uint32_t size)
{
    return mp4CbCalloc(size);
}

/* 0x00c4ec */
MP4_NOINLINE void *mp4MemMalloc(uint32_t size)
{
    return mp4CbMalloc(size);
}

/* 0x00c508 */
MP4_NOINLINE int32_t mp4MemRealloc(void **ptr, uint32_t old_size, uint32_t new_size)
{
    void *grown = mp4CbCalloc(new_size);
    if (grown == NULL) {
        return MP4_ERR_NOMEM;
    }
    uint32_t copy_len = (old_size < new_size) ? old_size : new_size;
    mp4SafeMemcpy(grown, new_size, *ptr, copy_len);
    mp4CbFree(*ptr);
    *ptr = grown;
    return MP4_OK;
}

/* 0x00c588 */
MP4_NOINLINE void mp4MtdtExtFree(Mp4MtdtExt *ext)
{
    mp4MemFree(ext->raw_payload);
    ext->raw_payload = NULL;
    mp4MemFree(ext->items);
    ext->items = NULL;
}

/* 0x00c5c0 */
MP4_NOINLINE int32_t mp4ParseFile(Mp4Movie *movie, uint64_t file_size,
                                  Mp4IoStream *stream, int32_t mode)
{
    stream->is_fragmented = 0;
    int32_t status = mp4ParseRootCheck(movie, file_size, stream, mode);
    if (status == MP4_ERR_PARSE_FATAL) {
        return status;
    }
    if (movie->ftyp.size != 0 && movie->ftyp.size < 16U) {
        movie->ftyp.size = 16;
    }
    if (movie->prof.size != 0) {
        mp4CalcProfSize(&movie->prof);
    }
    if (movie->size != 0) {
        mp4CalcMoovSize(movie);
    }
    return status;
}

/* 0x00c654 */
MP4_NOINLINE int32_t mp4ParseFilePartial(Mp4Movie *movie, uint32_t mask, Mp4IoStream *stream)
{
    stream->is_fragmented = 0;
    int32_t status = mp4ParseRootPartial(movie, mask, stream);
    if ((stream->error_flags & MP4_PARSE_FLAG_FATAL_MASK) != 0) {
        status = MP4_ERR_PARSE_FATAL;
    }
    if (status == MP4_ERR_PARSE_FATAL) {
        return MP4_ERR_PARSE_FATAL;
    }
    if (movie->ftyp.size != 0 && movie->ftyp.size < 16U) {
        movie->ftyp.size = 16;
    }
    if (movie->prof.size != 0) {
        mp4CalcProfSize(&movie->prof);
    }
    if (movie->size != 0) {
        mp4CalcMoovSize(movie);
    }
    return status;
}

/* 0x00c704 */
MP4_NOINLINE int32_t mp4TrackGetId(Mp4Track *track, uint32_t *out_id)
{
    if (track == NULL) {
        return MP4_ERR_NO_ENTRY;
    }
    *out_id = track->tkhd.track_id;
    return MP4_OK;
}

/* 0x00c720 */
MP4_NOINLINE int32_t mp4MovieFindTrackById(Mp4Movie *movie, uint32_t track_id, Mp4Track **out_track)
{
    for (uint32_t idx = 0; idx < movie->track_count; idx++) {
        Mp4Track *trk = mp4MovieGetTrackAt(movie, idx);
        if (trk->tkhd.track_id == track_id) {
            *out_track = mp4MovieGetTrackAt(movie, idx);
            return MP4_OK;
        }
    }
    return MP4_ERR_PARSE_FATAL;
}

/* 0x00c7b4 */
MP4_NOINLINE int32_t mp4ElstReadCol(Mp4ElstBox *elst, uint32_t row, uint32_t col, uint32_t *out_val)
{
    if (elst == NULL) {
        return MP4_ERR_PARSE_FATAL;
    }
    if ((row >= elst->entry_count) | (col >= 3U)) {
        return MP4_ERR_INDEX_RANGE;
    }
    if (mp4ChunkArrayGetU32(elst->entries, row, col, out_val) != MP4_OK) {
        return MP4_ERR_PARSE_FATAL;
    }
    return MP4_OK;
}

/* 0x00c804 */
MP4_NOINLINE int32_t mp4TrackGetActiveStbl(Mp4Track *track, Mp4Stbl **out_stbl)
{
    if (track == NULL) {
        return MP4_ERR_NO_ENTRY;
    }
    *out_stbl = track->mdia.minf.active_stbl;
    return (*out_stbl != NULL) ? MP4_OK : MP4_ERR_NO_ENTRY;
}

/* 0x00c824 */
MP4_NOINLINE int32_t mp4GetBit(uint8_t octet_val, uint32_t bit_idx)
{
    return ((int32_t)octet_val & (1 << bit_idx)) >> bit_idx;
}

/* 0x00c83c */
MP4_NOINLINE int32_t mp4CheckedAddU32(uint32_t *acc, uint32_t addend)
{
    uint32_t sum = *acc + addend;
    if (sum < *acc) {
        return MP4_ERR_ADD_OVERFLOW;
    }
    *acc = sum;
    return MP4_OK;
}

/* 0x00c870 */
MP4_NOINLINE int32_t mp4CheckedAddU64(uint64_t *acc, uint32_t addend)
{
    uint64_t sum = *acc + (uint64_t)addend;
    if (sum < *acc || sum < (uint64_t)addend) {
        return MP4_ERR_ADD_OVERFLOW;
    }
    *acc = sum;
    return MP4_OK;
}

/* 0x00c8ec */
MP4_NOINLINE int32_t mp4MtdtSetExtEntry(Mp4MtdtBox *mtdt, uint16_t size, uint32_t tag,
                                        uint16_t lang, uint16_t enc, uint32_t index,
                                        uint32_t magic, Mp4MtdtExt **out_ext)
{
    int32_t status = mp4MtdtReserveEntry(mtdt, size, tag, lang, enc, index);
    if (status != MP4_OK) {
        return status;
    }
    Mp4MtdtEntry *ent = &mtdt->entries[index];
    *out_ext = &ent->ext;
    ent->ext.head_magic = magic;
    ent->ext.owner = ent;
    return MP4_OK;
}

/* 0x00c974 */
MP4_NOINLINE int32_t mp4ParseRootCheck(Mp4Movie *movie, uint64_t file_size,
                                       Mp4IoStream *stream, int32_t mode)
{
    stream->error_flags = 0;
    int32_t status = mp4ParseRootBoxes(movie, file_size, stream, mode);
    if (status == MP4_ERR_PARSE_FATAL || (stream->error_flags & MP4_PARSE_FLAG_FATAL_MASK) != 0) {
        return MP4_ERR_PARSE_FATAL;
    }
    if ((stream->error_flags & 0xFFU) != 0) {
        return MP4_ERR_PARSE_WARN;
    }
    if (mode == 1 && status == MP4_ERR_NO_MOOV) {
        return MP4_ERR_NO_MOOV;
    }
    return MP4_OK;
}

/* 0x00c9f8 */
MP4_NOINLINE int32_t mp4ParseRootBoxes(Mp4Movie *movie, uint64_t file_size,
                                       Mp4IoStream *stream, int32_t mode)
{
    uint16_t moof_seen = 0;
    uint64_t rem = file_size;
    Mp4MoofBox *last_moof = NULL;

    if (file_size >= 1U && file_size < 8U) {
        mp4SetParseFlag(MP4_TAG_SLDM, MP4_PARSE_FLAG_SIZE_MISMATCH, stream);
        return MP4_OK;
    }

    while (rem >= 8U) {
        uint32_t box_size = mp4ReadU32(stream);
        if (box_size == 0) {
            return MP4_OK;
        }
        if (box_size < 8U) {
            mp4SetParseFlag(MP4_TAG_SLDM, MP4_PARSE_FLAG_SIZE_MISMATCH, stream);
            return MP4_OK;
        }
        uint32_t box_tag = mp4ReadU32(stream);
        if (rem < (uint64_t)box_size) {
            mp4SetParseFlag(box_tag, MP4_PARSE_FLAG_SIZE_MISMATCH, stream);
            box_size = (uint32_t)rem;
        }

        uint64_t box_end = (uint64_t)mp4CbTell(stream->file_handle) + (uint64_t)box_size - 8U;
        uint32_t dispatch_tag = box_tag;
        if (box_tag == MP4_TAG_MFRA && (movie->options & 1U) == 0) {
            dispatch_tag = MP4_TAG_SKIP_UUID;
        }

        if (dispatch_tag == MP4_TAG_MDAT || dispatch_tag == MP4_TAG_FREE) {
            mp4SeekAbs(stream->file_handle, box_end);
        } else if (dispatch_tag == MP4_TAG_FTYP) {
            if (box_size < 16U) {
                mp4SetParseFlag(MP4_TAG_FTYP, MP4_PARSE_FLAG_BAD_BOX_SIZE, stream);
            } else if (movie->ftyp.size != 0) {
                mp4SetParseFlag(box_tag, MP4_PARSE_FLAG_DUPLICATE_BOX, stream);
                mp4SeekAbs(stream->file_handle, box_end);
            } else {
                mp4ParseFtyp(&movie->ftyp, &rem, &box_size, stream);
            }
        } else if (dispatch_tag == MP4_TAG_MOOV) {
            if (movie->size != 0) {
                mp4SetParseFlag(box_tag, MP4_PARSE_FLAG_DUPLICATE_BOX, stream);
                mp4SeekAbs(stream->file_handle, box_end);
            } else {
                movie->self = movie;
                movie->tag = box_tag;
                movie->size = box_size;
                if (mp4ParseMoov(movie, box_size - 8U, stream) == MP4_ERR_PARSE_FATAL) {
                    return MP4_ERR_PARSE_FATAL;
                }
                for (uint32_t idx = 0; idx < movie->track_count; idx++) {
                    Mp4Track *trk = mp4MovieGetTrackAt(movie, idx);
                    trk->movie_timescale = movie->mvhd.timescale;
                }
                movie->moov_offset = box_end - (uint64_t)box_size;
                movie->first_moof_offset = file_size;
                if (mode != 0) {
                    Mp4Track *trk = movie->tracks_head;
                    for (uint32_t idx = 0; idx < movie->track_count && trk != NULL; idx++) {
                        trk->mdia.minf.active_stbl = &trk->mdia.minf.stbl;
                        trk = trk->next;
                    }
                    movie->parsed_flag = 1;
                }
            }
        } else if (dispatch_tag == MP4_TAG_MFRA) {
            if (box_size < 8U) {
                mp4SetParseFlag(box_tag, MP4_PARSE_FLAG_BAD_BOX_SIZE, stream);
                mp4SeekAbs(stream->file_handle, box_end);
            } else if (movie->mfra != NULL) {
                mp4SetParseFlag(box_tag, MP4_PARSE_FLAG_DUPLICATE_BOX, stream);
                mp4SeekAbs(stream->file_handle, box_end);
            } else {
                Mp4MfraBox *mfra = NULL;
                if (mp4MfraCreate(&mfra) == MP4_OK) {
                    uint32_t mfra_err = 0;
                    int32_t res = mp4MfraParse(mfra, stream->file_handle, box_size - 8U, &mfra_err);
                    stream->error_flags |= mfra_err;
                    if (res == MP4_OK) {
                        movie->mfra = mfra;
                    } else {
                        mp4MfraDestroy(mfra);
                    }
                }
            }
        } else if (dispatch_tag == MP4_TAG_MOOF) {
            if (moof_seen != 0) {
                if (last_moof != NULL) {
                    last_moof->next_moof_offset = box_end - (uint64_t)box_size;
                }
            } else {
                movie->first_moof_offset = box_end - (uint64_t)box_size;
                stream->is_fragmented = 1;
                if (mp4FsmIsPendingCache(stream->fsm) != 0) {
                    return MP4_OK;
                }
            }

            if (mode == 1) {
                void *moof_slot = NULL;
                if (mp4AllocUniqueBox(&moof_slot, box_tag, sizeof(Mp4MoofBox),
                                      box_end, stream) != MP4_ERR_PARSE_FATAL) {
                    Mp4MoofBox *moof = (Mp4MoofBox *)moof_slot;
                    moof->size = box_size;
                    moof->tag = box_tag;
                    if (mp4MoofParse(moof, box_size - 8U, stream) == MP4_ERR_PARSE_FATAL) {
                        for (uint32_t ti = 0; ti < moof->traf_count; ti++) {
                            mp4TrafFree(moof->traf_list[ti]);
                            moof->traf_list[ti] = NULL;
                        }
                        moof->traf_count = 0;
                    }
                    mp4MovieAppendMoof(movie, moof);
                    moof->file_offset = box_end - (uint64_t)box_size;
                    Mp4TrafBox *prev_traf = NULL;
                    for (uint32_t ti = 0; ti < moof->traf_count; ti++) {
                        mp4TrafResolveDataOffsets(prev_traf, moof, moof->traf_list[ti]);
                        prev_traf = moof->traf_list[ti];
                    }
                    moof_seen++;
                    last_moof = moof;
                    moof->next_moof_offset = file_size;
                }
            } else if (mode == 2) {
                moof_seen++;
                mp4SeekAbs(stream->file_handle, box_end);
            } else if (mode == 3) {
                moof_seen++;
                uint32_t tail_tag = 0;
                uint32_t mfra_sz = 0;
                mp4CbSeek(stream->file_handle, (int64_t)(file_size - 12U), 0);
                mp4CbReadBe(&tail_tag, 4, stream->file_handle);
                if (tail_tag != MP4_TAG_MFRO) {
                    mp4CbSeek(stream->file_handle, 0, 2);
                    rem = (uint64_t)box_size;
                } else {
                    mp4CbSeek(stream->file_handle, (int64_t)(file_size - 4U), 0);
                    mp4CbReadBe(&mfra_sz, 4, stream->file_handle);
                    mp4CbSeek(stream->file_handle, (int64_t)(file_size - (uint64_t)mfra_sz + 4U), 0);
                    mp4CbReadBe(&tail_tag, 4, stream->file_handle);
                    if (tail_tag == MP4_TAG_MFRA) {
                        mp4CbSeek(stream->file_handle, (int64_t)(file_size - (uint64_t)mfra_sz), 0);
                        rem = (uint64_t)box_size + (uint64_t)mfra_sz;
                    }
                }
            } else {
                mp4SetParseFlag(MP4_TAG_SLDM, MP4_PARSE_FLAG_UNREC_BOX, stream);
                mp4SeekAbs(stream->file_handle, box_end);
            }
        } else if (dispatch_tag == MP4_TAG_UUID) {
            if (box_size < 24U) {
                mp4SetParseFlag(box_tag, MP4_PARSE_FLAG_BAD_BOX_SIZE, stream);
                return MP4_OK;
            }
            uint32_t uuid[4];
            for (uint32_t ui = 0; ui < 4U; ui++) {
                uuid[ui] = mp4ReadU32(stream);
            }
            int32_t uuid_kind = 0;
            if (uuid[1] == MP4_SONY_UUID_WORD_A &&
                uuid[2] == MP4_SONY_UUID_WORD_B &&
                uuid[3] == MP4_SONY_UUID_WORD_C) {
                if (uuid[0] == MP4_TAG_LICI || uuid[0] == MP4_TAG_MTSD || uuid[0] == MP4_TAG_PROF) {
                    uuid_kind = 1;
                } else {
                    uuid_kind = -1;
                }
            }

            if (uuid_kind == 0) {
                uint64_t cur_pos = (uint64_t)mp4CbTell(stream->file_handle);
                mp4RecordCustomUuid(movie, cur_pos - 24U, box_size, uuid);
                mp4SeekAbs(stream->file_handle, box_end);
            } else if (uuid_kind != 1) {
                mp4SetParseFlag(box_tag, MP4_PARSE_FLAG_UNREC_BOX, stream);
                mp4SeekAbs(stream->file_handle, box_end);
            } else if (uuid[0] == MP4_TAG_LICI) {
                if (movie->lici.size != 0) {
                    mp4SetParseFlag(MP4_TAG_LICI, MP4_PARSE_FLAG_DUPLICATE_BOX, stream);
                    mp4SeekAbs(stream->file_handle, box_end);
                } else {
                    movie->lici.tag = uuid[0];
                    movie->lici.size = box_size;
                    if (box_size < 32U) {
                        mp4SetParseFlag(MP4_TAG_LICI, MP4_PARSE_FLAG_BAD_BOX_SIZE, stream);
                        mp4SeekAbs(stream->file_handle, box_end);
                    } else {
                        uint32_t part_a = mp4ReadU32(stream);
                        mp4ReadU32(stream);
                        if (part_a < 8U || box_size < part_a + 24U) {
                            mp4SetParseFlag(MP4_TAG_LICI, MP4_PARSE_FLAG_BAD_BOX_SIZE, stream);
                            mp4SeekAbs(stream->file_handle, box_end);
                        } else {
                            movie->lici.first_len = part_a - 8U;
                            movie->lici.first_buf = (uint8_t *)mp4MemCalloc(part_a - 8U);
                            if (movie->lici.first_buf == NULL) {
                                mp4SetParseFlag(MP4_TAG_LICI, MP4_PARSE_FLAG_ALLOC_FAIL, stream);
                                mp4SeekAbs(stream->file_handle, box_end);
                            } else {
                                mp4ReadBytes(movie->lici.first_buf, part_a - 8U, stream);
                                uint32_t part_b = mp4ReadU32(stream);
                                mp4ReadU32(stream);
                                if (part_b < 8U || box_size < part_a + part_b + 24U) {
                                    mp4SetParseFlag(MP4_TAG_LICI, MP4_PARSE_FLAG_BAD_BOX_SIZE, stream);
                                    mp4SeekAbs(stream->file_handle, box_end);
                                } else {
                                    movie->lici.second_len = part_b - 8U;
                                    movie->lici.second_buf = (uint8_t *)mp4MemCalloc(part_b - 8U);
                                    if (movie->lici.second_buf == NULL) {
                                        mp4SetParseFlag(MP4_TAG_LICI, MP4_PARSE_FLAG_ALLOC_FAIL, stream);
                                    } else {
                                        mp4ReadBytes(movie->lici.second_buf, part_b - 8U, stream);
                                    }
                                    mp4SeekAbs(stream->file_handle, box_end);
                                }
                            }
                        }
                    }
                }
            } else if (uuid[0] == MP4_TAG_MTSD) {
                if (movie->mtsd_size != 0) {
                    mp4SetParseFlag(box_tag, MP4_PARSE_FLAG_DUPLICATE_BOX, stream);
                } else {
                    uint64_t cur_pos = (uint64_t)mp4CbTell(stream->file_handle);
                    movie->mtsd_size = (uint64_t)box_size;
                    movie->mtsd_offset = cur_pos - 24U;
                    mp4SeekAbs(stream->file_handle, box_end);
                }
            } else if (uuid[0] == MP4_TAG_PROF) {
                if (box_size < 12U) {
                    mp4SetParseFlag(box_tag, MP4_PARSE_FLAG_BAD_BOX_SIZE, stream);
                } else {
                    uint8_t ver = 0;
                    mp4ReadBytes(&ver, 1, stream);
                    if (ver != 0) {
                        mp4SetParseFlag(box_tag, MP4_PARSE_FLAG_UNREC_BOX, stream);
                        mp4SeekAbs(stream->file_handle, box_end);
                    } else if (box_size < 32U) {
                        mp4SetParseFlag(movie->prof.tag, MP4_PARSE_FLAG_BAD_BOX_SIZE, stream);
                        return MP4_OK;
                    } else if (movie->prof.size != 0) {
                        mp4SetParseFlag(box_tag, MP4_PARSE_FLAG_DUPLICATE_BOX, stream);
                        mp4SeekAbs(stream->file_handle, box_end);
                    } else {
                        movie->prof.size = box_size;
                        movie->prof.tag = box_tag;
                        for (uint32_t ui = 0; ui < 4U; ui++) {
                            movie->prof.uuid[ui] = uuid[ui];
                        }
                        movie->prof.version = ver;
                        mp4ReadBytes(movie->prof.flags, 3, stream);
                        uint32_t prof_cnt = mp4ReadU32(stream);
                        if (mp4ParseProf(&movie->prof, prof_cnt, stream) != MP4_OK) {
                            return MP4_ERR_PARSE_FATAL;
                        }
                    }
                }
            } else {
                mp4SetParseFlag(MP4_TAG_SLDM, MP4_PARSE_FLAG_UNREC_BOX, stream);
                mp4SeekAbs(stream->file_handle, box_end);
            }
        } else {
            mp4SetParseFlag(MP4_TAG_SLDM, MP4_PARSE_FLAG_UNREC_BOX, stream);
            mp4SeekAbs(stream->file_handle, box_end);
        }

        if ((stream->error_flags & MP4_PARSE_FLAG_FATAL_MASK) != 0) {
            return MP4_ERR_PARSE_FATAL;
        }
        uint64_t cur_pos = (uint64_t)mp4CbTell(stream->file_handle);
        if (cur_pos < box_end) {
            mp4SetParseFlag(box_tag, MP4_PARSE_FLAG_SIZE_MISMATCH, stream);
            mp4SeekAbs(stream->file_handle, box_end);
        }
        rem -= (uint64_t)box_size;
    }

    if (rem != 0) {
        mp4SetParseFlag(MP4_TAG_SLDM, MP4_PARSE_FLAG_SIZE_MISMATCH, stream);
    }
    if (mode == 1 && movie->size == 0) {
        return MP4_ERR_NO_MOOV;
    }
    return MP4_OK;
}

/* 0x00d5b0 */
MP4_NOINLINE int32_t mp4ParseMoov(Mp4Movie *movie, uint32_t body_size, Mp4IoStream *stream)
{
    uint32_t rem = body_size;
    while (rem >= 8U) {
        uint32_t box_size = mp4ReadU32(stream);
        if (box_size < 8U) {
            mp4SetParseFlag(MP4_TAG_MOOV, MP4_PARSE_FLAG_SIZE_MISMATCH, stream);
            return MP4_OK;
        }
        uint32_t box_tag = mp4ReadU32(stream);
        if (rem < box_size) {
            mp4SetParseFlag(box_tag, MP4_PARSE_FLAG_SIZE_MISMATCH, stream);
            box_size = rem;
        }
        uint64_t box_end = (uint64_t)mp4CbTell(stream->file_handle) + (uint64_t)box_size - 8U;

        if (box_tag == MP4_TAG_MVHD) {
            if (box_size < 12U) {
                mp4SetParseFlag(MP4_TAG_MVHD, MP4_PARSE_FLAG_BAD_BOX_SIZE, stream);
            } else {
                uint8_t ver = 0;
                mp4ReadBytes(&ver, 1, stream);
                if (ver != 0) {
                    mp4SetParseFlag(MP4_TAG_MVHD, MP4_PARSE_FLAG_UNREC_BOX, stream);
                    mp4SeekAbs(stream->file_handle, box_end);
                } else if (box_size < 108U) {
                    mp4SetParseFlag(MP4_TAG_MVHD, MP4_PARSE_FLAG_BAD_BOX_SIZE, stream);
                } else if (movie->mvhd.size != 0) {
                    mp4SetParseFlag(MP4_TAG_MVHD, MP4_PARSE_FLAG_DUPLICATE_BOX, stream);
                    mp4SeekAbs(stream->file_handle, box_end);
                } else {
                    movie->mvhd.size = box_size;
                    movie->mvhd.tag = MP4_TAG_MVHD;
                    mp4ReadMvhd(&movie->mvhd, stream);
                }
            }
        } else if (box_tag == MP4_TAG_IODS) {
            if (box_size < 12U) {
                mp4SetParseFlag(MP4_TAG_IODS, MP4_PARSE_FLAG_BAD_BOX_SIZE, stream);
            } else {
                uint8_t ver = 0;
                mp4ReadBytes(&ver, 1, stream);
                if (ver != 0) {
                    mp4SetParseFlag(MP4_TAG_IODS, MP4_PARSE_FLAG_UNREC_BOX, stream);
                    mp4SeekAbs(stream->file_handle, box_end);
                } else {
                    void *slot = movie->iods;
                    if (mp4AllocUniqueBox(&slot, MP4_TAG_IODS, sizeof(Mp4IodsBox),
                                          box_end, stream) != MP4_ERR_PARSE_FATAL) {
                        movie->iods = (Mp4IodsBox *)slot;
                        movie->iods->size = box_size;
                        movie->iods->tag = MP4_TAG_IODS;
                        movie->iods->version = ver;
                        mp4ReadBytes(movie->iods->flags, 3, stream);
                        if (box_size != 12U) {
                            movie->iods->payload = (uint8_t *)mp4MemCalloc(box_size - 12U);
                            if (movie->iods->payload == NULL) {
                                mp4SetParseFlag(MP4_TAG_IODS, MP4_PARSE_FLAG_ALLOC_FAIL, stream);
                                return MP4_ERR_PARSE_FATAL;
                            }
                            mp4ReadBytes(movie->iods->payload, box_size - 12U, stream);
                        }
                    }
                }
            }
        } else if (box_tag == MP4_TAG_MVEX) {
            void *slot = movie->mvex;
            if (mp4AllocUniqueBox(&slot, MP4_TAG_MVEX, sizeof(Mp4MvexBox),
                                  box_end, stream) != MP4_ERR_PARSE_FATAL) {
                movie->mvex = (Mp4MvexBox *)slot;
                movie->mvex->size = box_size;
                movie->mvex->tag = MP4_TAG_MVEX;
                if (mp4MvexParse(movie->mvex, box_size - 8U, stream) == MP4_ERR_PARSE_FATAL) {
                    return MP4_ERR_PARSE_FATAL;
                }
            }
        } else if (box_tag == MP4_TAG_TRAK) {
            Mp4Track *trk = (Mp4Track *)mp4MemCalloc(sizeof(Mp4Track));
            if (trk == NULL) {
                mp4SetParseFlag(MP4_TAG_TRAK, MP4_PARSE_FLAG_ALLOC_FAIL, stream);
                return MP4_ERR_PARSE_FATAL;
            }
            trk->size = box_size;
            trk->tag = MP4_TAG_TRAK;
            int32_t trk_res = mp4ParseTrak(trk, box_size - 8U, stream);
            if (mp4TrackResolveDrefs(trk) != MP4_OK) {
                mp4SetParseFlag(MP4_TAG_TRAK, MP4_PARSE_FLAG_DREF_MISMATCH, stream);
            }
            if (mp4FsmIsPendingCache(stream->fsm) != 0 || mp4FsmIsCacheMode(stream->fsm) != 0) {
                if (trk->mdia.minf.stbl.cache != NULL) {
                    mp4CacheSetTrack(trk->mdia.minf.stbl.cache, trk);
                }
            }
            trk->mdia.minf.stbl.track = trk;
            if (mp4MovieAppendTrack(movie, trk) == MP4_ERR_PARSE_FATAL) {
                mp4SetParseFlag(MP4_TAG_TRAK, MP4_PARSE_FLAG_BAD_BOX_SIZE, stream);
            }
            if (trk_res != MP4_OK) {
                return trk_res;
            }
        } else if (box_tag == MP4_TAG_UDTA) {
            void *slot = movie->udta;
            if (mp4AllocUniqueBox(&slot, MP4_TAG_UDTA, sizeof(Mp4UdtaBox),
                                  box_end, stream) == MP4_ERR_PARSE_FATAL) {
                mp4SetParseFlag(MP4_TAG_UDTA, MP4_PARSE_FLAG_ALLOC_FAIL, stream);
                return MP4_ERR_PARSE_FATAL;
            }
            movie->udta = (Mp4UdtaBox *)slot;
            movie->udta->size = 8;
            movie->udta->tag = MP4_TAG_UDTA;
            mp4SeekAbs(stream->file_handle, box_end);
        } else if (box_tag == MP4_TAG_UUID) {
            if (box_size < 24U) {
                mp4SetParseFlag(MP4_TAG_USMT, MP4_PARSE_FLAG_SIZE_MISMATCH, stream);
            } else {
                uint32_t uuid[4];
                for (uint32_t ui = 0; ui < 4U; ui++) {
                    uuid[ui] = mp4ReadU32(stream);
                }
                if (uuid[1] != MP4_SONY_UUID_WORD_A ||
                    uuid[2] != MP4_SONY_UUID_WORD_B ||
                    uuid[3] != MP4_SONY_UUID_WORD_C) {
                    mp4SetParseFlag(MP4_TAG_USMT, MP4_PARSE_FLAG_UNREC_BOX, stream);
                    mp4SeekAbs(stream->file_handle, box_end);
                } else if (uuid[0] != MP4_TAG_USMT) {
                    mp4SetParseFlag(MP4_TAG_SLDM, MP4_PARSE_FLAG_UNREC_BOX, stream);
                    mp4SeekAbs(stream->file_handle, box_end);
                } else if (movie->usmt != NULL) {
                    mp4SetParseFlag(MP4_TAG_UUID, MP4_PARSE_FLAG_DUPLICATE_BOX, stream);
                    mp4SeekAbs(stream->file_handle, box_end);
                } else {
                    void *slot = movie->usmt;
                    if (mp4AllocUniqueBox(&slot, MP4_TAG_USMT, sizeof(Mp4UsmtBox),
                                          box_end, stream) != MP4_ERR_PARSE_FATAL) {
                        movie->usmt = (Mp4UsmtBox *)slot;
                        movie->usmt->size = box_size;
                        movie->usmt->tag = MP4_TAG_UUID;
                        for (uint32_t ui = 0; ui < 4U; ui++) {
                            movie->usmt->uuid[ui] = uuid[ui];
                        }
                        if (mp4ParseUsmt(movie->usmt, box_size - 24U, stream) == MP4_ERR_PARSE_FATAL) {
                            return MP4_ERR_PARSE_FATAL;
                        }
                    }
                }
            }
        } else {
            mp4SetParseFlag(MP4_TAG_MOOV, MP4_PARSE_FLAG_UNREC_BOX, stream);
            mp4SeekAbs(stream->file_handle, box_end);
        }

        if ((stream->error_flags & MP4_PARSE_FLAG_FATAL_MASK) != 0) {
            return MP4_ERR_PARSE_FATAL;
        }
        uint64_t cur_pos = (uint64_t)mp4CbTell(stream->file_handle);
        if (cur_pos < box_end) {
            mp4SetParseFlag(box_tag, MP4_PARSE_FLAG_SIZE_MISMATCH, stream);
            mp4SeekAbs(stream->file_handle, box_end);
        }
        rem -= box_size;
    }

    if (rem != 0) {
        mp4SetParseFlag(MP4_TAG_MOOV, MP4_PARSE_FLAG_SIZE_MISMATCH, stream);
    }
    return MP4_OK;
}

/* 0x00dc04 */
MP4_NOINLINE int32_t mp4ParseMoovPartial(Mp4Movie *movie, uint32_t body_size,
                                         uint32_t mask, Mp4IoStream *stream)
{
    uint32_t rem = body_size;
    while (rem >= 8U) {
        uint32_t box_size = mp4ReadU32(stream);
        if (box_size < 8U) {
            mp4SetParseFlag(MP4_TAG_MOOV, MP4_PARSE_FLAG_SIZE_MISMATCH, stream);
            return MP4_OK;
        }
        uint32_t box_tag = mp4ReadU32(stream);
        if (rem < box_size) {
            mp4SetParseFlag(box_tag, MP4_PARSE_FLAG_SIZE_MISMATCH, stream);
            box_size = rem;
        }
        uint64_t box_end = (uint64_t)mp4CbTell(stream->file_handle) + (uint64_t)box_size - 8U;

        if (box_tag == MP4_TAG_MVHD) {
            if ((mask & 4U) == 0) {
                mp4SeekAbs(stream->file_handle, box_end);
                if (movie->mvhd.size != 0) {
                    mp4SetParseFlag(MP4_TAG_MVHD, MP4_PARSE_FLAG_DUPLICATE_BOX, stream);
                }
            } else if (box_size < 12U) {
                mp4SetParseFlag(MP4_TAG_MVHD, MP4_PARSE_FLAG_BAD_BOX_SIZE, stream);
            } else {
                uint8_t ver = 0;
                mp4ReadBytes(&ver, 1, stream);
                if (ver != 0) {
                    mp4SeekAbs(stream->file_handle, box_end);
                } else if (box_size < 108U) {
                    mp4SetParseFlag(MP4_TAG_MVHD, MP4_PARSE_FLAG_BAD_BOX_SIZE, stream);
                } else {
                    mask -= 4U;
                    movie->mvhd.version = 0;
                    movie->mvhd.size = box_size;
                    movie->mvhd.tag = MP4_TAG_MVHD;
                    mp4ReadMvhd(&movie->mvhd, stream);
                }
            }
        } else if (box_tag == MP4_TAG_UUID) {
            if (box_size < 24U) {
                mp4SetParseFlag(MP4_TAG_USMT, MP4_PARSE_FLAG_SIZE_MISMATCH, stream);
            } else {
                uint32_t uuid[4];
                for (uint32_t ui = 0; ui < 4U; ui++) {
                    uuid[ui] = mp4ReadU32(stream);
                }
                if (uuid[1] != MP4_SONY_UUID_WORD_A ||
                    uuid[2] != MP4_SONY_UUID_WORD_B ||
                    uuid[3] != MP4_SONY_UUID_WORD_C ||
                    uuid[0] != MP4_TAG_USMT) {
                    mp4SeekAbs(stream->file_handle, box_end);
                } else if ((mask & 8U) != 0) {
                    void *slot = movie->usmt;
                    if (mp4AllocUniqueBox(&slot, MP4_TAG_USMT, sizeof(Mp4UsmtBox),
                                          box_end, stream) != MP4_ERR_PARSE_FATAL) {
                        movie->usmt = (Mp4UsmtBox *)slot;
                        movie->usmt->size = box_size;
                        movie->usmt->tag = MP4_TAG_UUID;
                        for (uint32_t ui = 0; ui < 4U; ui++) {
                            movie->usmt->uuid[ui] = uuid[ui];
                        }
                        mask -= 8U;
                        int32_t res = mp4ParseUsmt(movie->usmt, box_size - 24U, stream);
                        stream->error_flags &= 0xE000000BU;
                        if (res != MP4_OK) {
                            return MP4_ERR_PARSE_FATAL;
                        }
                    }
                } else {
                    mp4SeekAbs(stream->file_handle, box_end);
                    if (movie->usmt != NULL) {
                        mp4SetParseFlag(MP4_TAG_UUID, MP4_PARSE_FLAG_DUPLICATE_BOX, stream);
                    }
                }
            }
        } else {
            mp4SeekAbs(stream->file_handle, box_end);
        }

        if ((stream->error_flags & MP4_PARSE_FLAG_FATAL_MASK) != 0) {
            return MP4_ERR_PARSE_FATAL;
        }
        uint64_t cur_pos = (uint64_t)mp4CbTell(stream->file_handle);
        if (cur_pos < box_end) {
            mp4SetParseFlag(box_tag, MP4_PARSE_FLAG_SIZE_MISMATCH, stream);
            mp4SeekAbs(stream->file_handle, box_end);
        }
        rem -= box_size;
        if (mask == 0) {
            return MP4_OK;
        }
    }
    return MP4_OK;
}

/* 0x00dfbc */
MP4_NOINLINE int32_t mp4ParseTrak(Mp4Track *track, uint32_t body_size, Mp4IoStream *stream)
{
    uint32_t rem = body_size;
    while (rem >= 8U) {
        uint32_t box_size = mp4ReadU32(stream);
        if (box_size < 8U) {
            mp4SetParseFlag(MP4_TAG_TRAK, MP4_PARSE_FLAG_SIZE_MISMATCH, stream);
            return MP4_OK;
        }
        uint32_t box_tag = mp4ReadU32(stream);
        if (rem < box_size) {
            mp4SetParseFlag(box_tag, MP4_PARSE_FLAG_SIZE_MISMATCH, stream);
            box_size = rem;
        }
        uint64_t box_end = (uint64_t)mp4CbTell(stream->file_handle) + (uint64_t)box_size - 8U;

        if (box_tag == MP4_TAG_TKHD) {
            if (box_size < 12U) {
                mp4SetParseFlag(MP4_TAG_TKHD, MP4_PARSE_FLAG_BAD_BOX_SIZE, stream);
            } else {
                uint8_t ver = 0;
                mp4ReadBytes(&ver, 1, stream);
                if (ver != 0) {
                    mp4SetParseFlag(MP4_TAG_TKHD, MP4_PARSE_FLAG_UNREC_BOX, stream);
                    mp4SeekAbs(stream->file_handle, box_end);
                } else if (box_size < 92U) {
                    mp4SetParseFlag(MP4_TAG_TKHD, MP4_PARSE_FLAG_BAD_BOX_SIZE, stream);
                } else if (track->tkhd.size != 0) {
                    mp4SetParseFlag(MP4_TAG_TKHD, MP4_PARSE_FLAG_DUPLICATE_BOX, stream);
                    mp4SeekAbs(stream->file_handle, box_end);
                } else {
                    track->tkhd.size = box_size;
                    track->tkhd.tag = MP4_TAG_TKHD;
                    mp4ReadTkhd(&track->tkhd, stream);
                }
            }
        } else if (box_tag == MP4_TAG_EDTS) {
            if (track->edts.size != 0) {
                mp4SetParseFlag(MP4_TAG_EDTS, MP4_PARSE_FLAG_DUPLICATE_BOX, stream);
                mp4SeekAbs(stream->file_handle, box_end);
            } else {
                track->edts.size = box_size;
                track->edts.tag = MP4_TAG_EDTS;
                if (mp4ParseEdts(&track->edts, box_size - 8U, stream) == MP4_ERR_PARSE_FATAL) {
                    return MP4_ERR_PARSE_FATAL;
                }
            }
        } else if (box_tag == MP4_TAG_MDIA) {
            if (track->mdia.size != 0) {
                mp4SetParseFlag(MP4_TAG_MDIA, MP4_PARSE_FLAG_DUPLICATE_BOX, stream);
                mp4SeekAbs(stream->file_handle, box_end);
            } else {
                track->mdia.size = box_size;
                track->mdia.tag = MP4_TAG_MDIA;
                if (mp4ParseMdia(&track->mdia, box_size - 8U, stream) == MP4_ERR_PARSE_FATAL) {
                    return MP4_ERR_PARSE_FATAL;
                }
            }
        } else if (box_tag == MP4_TAG_TREF) {
            void *slot = track->tref;
            if (mp4AllocUniqueBox(&slot, MP4_TAG_TREF, sizeof(Mp4TrefBox),
                                  box_end, stream) != MP4_ERR_PARSE_FATAL) {
                track->tref = (Mp4TrefBox *)slot;
                track->tref->size = box_size;
                track->tref->tag = MP4_TAG_TREF;
                if (mp4ParseTref(track->tref, box_size - 8U, stream) == MP4_ERR_PARSE_FATAL) {
                    return MP4_ERR_PARSE_FATAL;
                }
            }
        } else if (box_tag == MP4_TAG_UDTA) {
            void *slot = track->udta;
            if (mp4AllocUniqueBox(&slot, MP4_TAG_UDTA, sizeof(Mp4UdtaBox),
                                  box_end, stream) == MP4_ERR_PARSE_FATAL) {
                mp4SetParseFlag(MP4_TAG_UDTA, MP4_PARSE_FLAG_ALLOC_FAIL, stream);
                return MP4_ERR_PARSE_FATAL;
            }
            track->udta = (Mp4UdtaBox *)slot;
            track->udta->size = 8;
            track->udta->tag = MP4_TAG_UDTA;
            mp4SeekAbs(stream->file_handle, box_end);
        } else if (box_tag == MP4_TAG_UUID) {
            if (box_size < 24U) {
                mp4SetParseFlag(MP4_TAG_USMT, MP4_PARSE_FLAG_SIZE_MISMATCH, stream);
            } else {
                uint32_t uuid[4];
                for (uint32_t ui = 0; ui < 4U; ui++) {
                    uuid[ui] = mp4ReadU32(stream);
                }
                if (uuid[1] != MP4_SONY_UUID_WORD_A ||
                    uuid[2] != MP4_SONY_UUID_WORD_B ||
                    uuid[3] != MP4_SONY_UUID_WORD_C ||
                    uuid[0] != MP4_TAG_USMT) {
                    mp4SetParseFlag(MP4_TAG_UUID, MP4_PARSE_FLAG_UNREC_BOX, stream);
                    mp4SeekAbs(stream->file_handle, box_end);
                } else {
                    void *slot = track->usmt;
                    if (mp4AllocUniqueBox(&slot, MP4_TAG_USMT, sizeof(Mp4UsmtBox),
                                          box_end, stream) != MP4_ERR_PARSE_FATAL) {
                        track->usmt = (Mp4UsmtBox *)slot;
                        track->usmt->size = box_size;
                        track->usmt->tag = MP4_TAG_UUID;
                        for (uint32_t ui = 0; ui < 4U; ui++) {
                            track->usmt->uuid[ui] = uuid[ui];
                        }
                        if (mp4ParseUsmt(track->usmt, box_size - 24U, stream) == MP4_ERR_PARSE_FATAL) {
                            return MP4_ERR_PARSE_FATAL;
                        }
                    }
                }
            }
        } else {
            mp4SetParseFlag(MP4_TAG_TRAK, MP4_PARSE_FLAG_UNREC_BOX, stream);
            mp4SeekAbs(stream->file_handle, box_end);
        }

        if ((stream->error_flags & MP4_PARSE_FLAG_FATAL_MASK) != 0) {
            return MP4_ERR_PARSE_FATAL;
        }
        uint64_t cur_pos = (uint64_t)mp4CbTell(stream->file_handle);
        if (cur_pos < box_end) {
            mp4SetParseFlag(box_tag, MP4_PARSE_FLAG_SIZE_MISMATCH, stream);
            mp4SeekAbs(stream->file_handle, box_end);
        }
        rem -= box_size;
    }

    if (rem != 0) {
        mp4SetParseFlag(MP4_TAG_TRAK, MP4_PARSE_FLAG_SIZE_MISMATCH, stream);
    }
    return MP4_OK;
}

/* 0x00e490 */
MP4_NOINLINE int32_t mp4ParseTref(Mp4TrefBox *tref, uint32_t body_size, Mp4IoStream *stream)
{
    uint32_t rem = body_size;
    while (rem >= 8U) {
        uint32_t box_size = mp4ReadU32(stream);
        if (box_size < 8U) {
            mp4SetParseFlag(MP4_TAG_TREF, MP4_PARSE_FLAG_SIZE_MISMATCH, stream);
            return MP4_OK;
        }
        uint32_t box_tag = mp4ReadU32(stream);
        if (rem < box_size) {
            mp4SetParseFlag(box_tag, MP4_PARSE_FLAG_SIZE_MISMATCH, stream);
            box_size = rem;
        }
        if ((box_size & 3U) != 0) {
            mp4SetParseFlag(MP4_TAG_TREF, MP4_PARSE_FLAG_COUNT_CLAMPED, stream);
            uint32_t aligned = box_size & ~3U;
            rem -= (box_size - aligned);
            box_size = aligned;
        }
        uint64_t box_end = (uint64_t)mp4CbTell(stream->file_handle) + (uint64_t)box_size - 8U;

        if (box_tag == MP4_TAG_MPOD || box_tag == MP4_TAG_SYNC) {
            Mp4TrefTypeBox **slot_ref = (box_tag == MP4_TAG_MPOD) ? &tref->mpod_ref : &tref->sync_ref;
            void *slot = *slot_ref;
            if (mp4AllocUniqueBox(&slot, box_tag, sizeof(Mp4TrefTypeBox),
                                  box_end, stream) != MP4_ERR_PARSE_FATAL) {
                Mp4TrefTypeBox *sub = (Mp4TrefTypeBox *)slot;
                *slot_ref = sub;
                uint16_t cnt = (uint16_t)((box_size - 8U) >> 2);
                sub->size = box_size;
                sub->tag = box_tag;
                sub->track_ids = (uint32_t *)mp4MemCalloc((uint32_t)cnt * 4U);
                if (sub->track_ids == NULL) {
                    mp4SetParseFlag(box_tag, MP4_PARSE_FLAG_ALLOC_FAIL, stream);
                } else {
                    sub->count = cnt;
                    sub->capacity = cnt;
                    for (uint32_t idx = 0; idx < cnt; idx++) {
                        sub->track_ids[idx] = mp4ReadU32(stream);
                    }
                }
            }
        } else {
            mp4SetParseFlag(MP4_TAG_TREF, MP4_PARSE_FLAG_UNREC_BOX, stream);
            mp4SeekAbs(stream->file_handle, box_end);
        }

        if ((stream->error_flags & MP4_PARSE_FLAG_FATAL_MASK) != 0) {
            return MP4_ERR_PARSE_FATAL;
        }
        uint64_t cur_pos = (uint64_t)mp4CbTell(stream->file_handle);
        if (cur_pos < box_end) {
            mp4SetParseFlag(box_tag, MP4_PARSE_FLAG_SIZE_MISMATCH, stream);
            mp4SeekAbs(stream->file_handle, box_end);
        }
        rem -= box_size;
    }

    if (rem != 0) {
        mp4SetParseFlag(MP4_TAG_TREF, MP4_PARSE_FLAG_SIZE_MISMATCH, stream);
    }
    return MP4_OK;
}

/* 0x00e7c0 */
MP4_NOINLINE int32_t mp4ParseEdts(Mp4EdtsBox *edts, uint32_t body_size, Mp4IoStream *stream)
{
    uint32_t rem = body_size;
    while (rem >= 8U) {
        uint32_t box_size = mp4ReadU32(stream);
        if (box_size < 8U) {
            mp4SetParseFlag(MP4_TAG_EDTS, MP4_PARSE_FLAG_SIZE_MISMATCH, stream);
            return MP4_OK;
        }
        uint32_t box_tag = mp4ReadU32(stream);
        if (rem < box_size) {
            mp4SetParseFlag(box_tag, MP4_PARSE_FLAG_SIZE_MISMATCH, stream);
            box_size = rem;
        }
        uint64_t box_end = (uint64_t)mp4CbTell(stream->file_handle) + (uint64_t)box_size - 8U;

        if (box_tag == MP4_TAG_ELST) {
            if (box_size < 12U) {
                mp4SetParseFlag(MP4_TAG_ELST, MP4_PARSE_FLAG_BAD_BOX_SIZE, stream);
            } else {
                uint8_t ver = 0;
                mp4ReadBytes(&ver, 1, stream);
                if (ver != 0) {
                    mp4SetParseFlag(MP4_TAG_ELST, MP4_PARSE_FLAG_UNREC_BOX, stream);
                    mp4SeekAbs(stream->file_handle, box_end);
                } else if (box_size < 16U) {
                    mp4SetParseFlag(MP4_TAG_ELST, MP4_PARSE_FLAG_BAD_BOX_SIZE, stream);
                } else if (edts->elst.size != 0) {
                    mp4SetParseFlag(MP4_TAG_ELST, MP4_PARSE_FLAG_DUPLICATE_BOX, stream);
                    mp4SeekAbs(stream->file_handle, box_end);
                } else {
                    edts->elst.size = box_size;
                    edts->elst.tag = MP4_TAG_ELST;
                    mp4ReadBytes(edts->elst.flags, 3, stream);
                    uint32_t cnt = mp4ReadU32(stream);
                    uint32_t max_cnt = (box_size - 16U) / 12U;
                    edts->elst.entry_count = cnt;
                    if (max_cnt < cnt) {
                        mp4SetParseFlag(MP4_TAG_EDTS, MP4_PARSE_FLAG_COUNT_CLAMPED, stream);
                        edts->elst.entry_count = max_cnt;
                    }
                    if (mp4ReadChunkArrayU32(&edts->elst.entries, edts->elst.entry_count,
                                             3, stream) == MP4_ERR_PARSE_FATAL) {
                        return MP4_ERR_PARSE_FATAL;
                    }
                }
            }
        } else {
            mp4SetParseFlag(MP4_TAG_EDTS, MP4_PARSE_FLAG_UNREC_BOX, stream);
            mp4SeekAbs(stream->file_handle, box_end);
        }

        if ((stream->error_flags & MP4_PARSE_FLAG_FATAL_MASK) != 0) {
            return MP4_ERR_PARSE_FATAL;
        }
        uint64_t cur_pos = (uint64_t)mp4CbTell(stream->file_handle);
        if (cur_pos < box_end) {
            mp4SetParseFlag(box_tag, MP4_PARSE_FLAG_SIZE_MISMATCH, stream);
            mp4SeekAbs(stream->file_handle, box_end);
        }
        rem -= box_size;
    }

    if (rem != 0) {
        mp4SetParseFlag(MP4_TAG_EDTS, MP4_PARSE_FLAG_SIZE_MISMATCH, stream);
    }
    return MP4_OK;
}

/* 0x00ea70 */
MP4_NOINLINE int32_t mp4ParseMdia(Mp4MdiaBox *mdia, uint32_t body_size, Mp4IoStream *stream)
{
    uint32_t handler_type = 0;
    uint32_t rem = body_size;

    while (rem >= 8U) {
        uint32_t box_size = mp4ReadU32(stream);
        if (box_size < 8U) {
            mp4SetParseFlag(MP4_TAG_MDIA, MP4_PARSE_FLAG_SIZE_MISMATCH, stream);
            return MP4_OK;
        }
        uint32_t box_tag = mp4ReadU32(stream);
        if (rem < box_size) {
            mp4SetParseFlag(box_tag, MP4_PARSE_FLAG_SIZE_MISMATCH, stream);
            box_size = rem;
        }
        uint64_t box_end = (uint64_t)mp4CbTell(stream->file_handle) + (uint64_t)box_size - 8U;

        if (box_tag == MP4_TAG_MDHD) {
            if (box_size < 12U) {
                mp4SetParseFlag(MP4_TAG_MDHD, MP4_PARSE_FLAG_BAD_BOX_SIZE, stream);
            } else {
                uint8_t ver = 0;
                mp4ReadBytes(&ver, 1, stream);
                if (ver != 0) {
                    mp4SetParseFlag(MP4_TAG_MDHD, MP4_PARSE_FLAG_UNREC_BOX, stream);
                    mp4SeekAbs(stream->file_handle, box_end);
                } else if (box_size < 32U) {
                    mp4SetParseFlag(MP4_TAG_MDHD, MP4_PARSE_FLAG_BAD_BOX_SIZE, stream);
                } else if (mdia->mdhd.size != 0) {
                    mp4SetParseFlag(MP4_TAG_MDHD, MP4_PARSE_FLAG_DUPLICATE_BOX, stream);
                    mp4SeekAbs(stream->file_handle, box_end);
                } else {
                    mdia->mdhd.size = box_size;
                    mdia->mdhd.tag = MP4_TAG_MDHD;
                    mp4ReadMdhd(&mdia->mdhd, stream);
                }
            }
        } else if (box_tag == MP4_TAG_HDLR) {
            if (box_size < 12U) {
                mp4SetParseFlag(MP4_TAG_HDLR, MP4_PARSE_FLAG_BAD_BOX_SIZE, stream);
            } else {
                uint8_t ver = 0;
                mp4ReadBytes(&ver, 1, stream);
                if (ver != 0) {
                    mp4SetParseFlag(MP4_TAG_HDLR, MP4_PARSE_FLAG_UNREC_BOX, stream);
                    mp4SeekAbs(stream->file_handle, box_end);
                } else if (box_size < 33U) {
                    mp4SetParseFlag(MP4_TAG_HDLR, MP4_PARSE_FLAG_BAD_BOX_SIZE, stream);
                } else if (mdia->hdlr.size != 0) {
                    mp4SetParseFlag(MP4_TAG_HDLR, MP4_PARSE_FLAG_DUPLICATE_BOX, stream);
                    mp4SeekAbs(stream->file_handle, box_end);
                } else {
                    mdia->hdlr.size = box_size;
                    mdia->hdlr.tag = MP4_TAG_HDLR;
                    mp4ReadBytes(mdia->hdlr.flags, 3, stream);
                    mdia->hdlr.predefined = mp4ReadU32(stream);
                    mdia->hdlr.handler_type = mp4ReadU32(stream);
                    mdia->hdlr.reserved[0] = mp4ReadU32(stream);
                    mdia->hdlr.reserved[1] = mp4ReadU32(stream);
                    mdia->hdlr.reserved[2] = mp4ReadU32(stream);
                    if (box_size - 32U > 64U) {
                        mdia->hdlr.is_c_string = (uint8_t)mp4ReadHandlerName(mdia->hdlr.name, 64, stream);
                        mp4SeekAbs(stream->file_handle, box_end);
                        mdia->hdlr.size = 96;
                    } else {
                        mdia->hdlr.is_c_string =
                            (uint8_t)mp4ReadHandlerName(mdia->hdlr.name, box_size - 32U, stream);
                    }
                    handler_type = mdia->hdlr.handler_type;
                }
            }
        } else if (box_tag == MP4_TAG_MINF) {
            if (mdia->minf.size != 0) {
                mp4SetParseFlag(MP4_TAG_MINF, MP4_PARSE_FLAG_DUPLICATE_BOX, stream);
                mp4SeekAbs(stream->file_handle, box_end);
            } else {
                mdia->minf.size = box_size;
                mdia->minf.tag = MP4_TAG_MINF;
                if (mp4ParseMinf(&mdia->minf, box_size - 8U, stream, handler_type) == MP4_ERR_PARSE_FATAL) {
                    return MP4_ERR_PARSE_FATAL;
                }
            }
        } else {
            mp4SetParseFlag(MP4_TAG_MDIA, MP4_PARSE_FLAG_UNREC_BOX, stream);
            mp4SeekAbs(stream->file_handle, box_end);
        }

        if ((stream->error_flags & MP4_PARSE_FLAG_FATAL_MASK) != 0) {
            return MP4_ERR_PARSE_FATAL;
        }
        uint64_t cur_pos = (uint64_t)mp4CbTell(stream->file_handle);
        if (cur_pos < box_end) {
            mp4SetParseFlag(box_tag, MP4_PARSE_FLAG_SIZE_MISMATCH, stream);
            mp4SeekAbs(stream->file_handle, box_end);
        }
        rem -= box_size;
    }

    if (rem != 0) {
        mp4SetParseFlag(MP4_TAG_MDIA, MP4_PARSE_FLAG_SIZE_MISMATCH, stream);
    }
    return MP4_OK;
}

/* 0x00ee1c */
MP4_NOINLINE int32_t mp4ParseUsmt(Mp4UsmtBox *usmt, uint32_t body_size, Mp4IoStream *stream)
{
    uint32_t rem = body_size;
    while (rem >= 8U) {
        uint32_t box_size = mp4ReadU32(stream);
        if (box_size < 8U) {
            mp4SetParseFlag(MP4_TAG_USMT, MP4_PARSE_FLAG_SIZE_MISMATCH, stream);
            return MP4_OK;
        }
        uint32_t box_tag = mp4ReadU32(stream);
        if (rem < box_size) {
            mp4SetParseFlag(box_tag, MP4_PARSE_FLAG_SIZE_MISMATCH, stream);
            box_size = rem;
        }
        uint64_t box_end = (uint64_t)mp4CbTell(stream->file_handle) + (uint64_t)box_size - 8U;

        if (box_tag == MP4_TAG_MTDF) {
            if (box_size < 10U) {
                mp4SetParseFlag(MP4_TAG_MTDF, MP4_PARSE_FLAG_BAD_BOX_SIZE, stream);
            } else if (usmt->mtdf.size != 0) {
                mp4SetParseFlag(MP4_TAG_MTDF, MP4_PARSE_FLAG_DUPLICATE_BOX, stream);
                mp4SeekAbs(stream->file_handle, box_end);
            } else {
                usmt->mtdf.size = box_size;
                usmt->mtdf.tag = MP4_TAG_MTDF;
                int32_t left = (int32_t)(box_size - 10U);
                uint32_t entry_total = mp4ReadU16(stream);
                for (uint32_t idx = 0; idx < entry_total; idx++) {
                    if (left < 10) {
                        mp4SetParseFlag(MP4_TAG_MTDF, MP4_PARSE_FLAG_BAD_BOX_SIZE, stream);
                        break;
                    }
                    left -= 10;
                    uint16_t esz = mp4ReadU16(stream);
                    if (esz < 10U) {
                        mp4SetParseFlag(MP4_TAG_MTDF, MP4_PARSE_FLAG_SIZE_MISMATCH, stream);
                        break;
                    }
                    if (esz >= 43U) {
                        esz = 42;
                        mp4SetParseFlag(MP4_TAG_MTDF, MP4_PARSE_FLAG_SIZE_MISMATCH, stream);
                    }
                    uint32_t etag = mp4ReadU32(stream);
                    uint16_t elang = mp4ReadU16(stream);
                    uint16_t eenc = mp4ReadU16(stream);
                    uint16_t plen = (uint16_t)(esz - 10U);
                    if (left < (int32_t)plen) {
                        mp4SetParseFlag(MP4_TAG_MTDF, MP4_PARSE_FLAG_BAD_BOX_SIZE, stream);
                        break;
                    }
                    uint8_t payload[32];
                    if (plen != 0) {
                        left -= (int32_t)plen;
                        mp4ReadBytes(payload, plen, stream);
                    }
                    if (mp4MtdfSetEntry(&usmt->mtdf, esz, etag, elang, eenc,
                                        usmt->mtdf.entry_count, payload) == MP4_ERR_NOMEM) {
                        mp4SetParseFlag(MP4_TAG_MTDF, MP4_PARSE_FLAG_ALLOC_FAIL, stream);
                        return MP4_ERR_PARSE_FATAL;
                    }
                }
            }
        } else if (box_tag == MP4_TAG_MTDT) {
            if (box_size < 10U) {
                mp4SetParseFlag(MP4_TAG_MTDT, MP4_PARSE_FLAG_BAD_BOX_SIZE, stream);
            } else if (usmt->mtdt.size != 0) {
                mp4SetParseFlag(MP4_TAG_MTDT, MP4_PARSE_FLAG_DUPLICATE_BOX, stream);
                mp4SeekAbs(stream->file_handle, box_end);
            } else {
                usmt->mtdt.size = box_size;
                usmt->mtdt.tag = MP4_TAG_MTDT;
                int32_t left = (int32_t)(box_size - 10U);
                uint32_t entry_total = mp4ReadU16(stream);
                for (uint32_t idx = 0; idx < entry_total; idx++) {
                    if (left < 10) {
                        mp4SetParseFlag(MP4_TAG_MTDT, MP4_PARSE_FLAG_BAD_BOX_SIZE, stream);
                        break;
                    }
                    uint32_t esz = mp4ReadU16(stream);
                    uint32_t etag = mp4ReadU32(stream);
                    uint16_t elang = mp4ReadU16(stream);
                    uint16_t eenc = mp4ReadU16(stream);
                    left -= 10;
                    if (esz < 10U) {
                        mp4SetParseFlag(MP4_TAG_MTDT, MP4_PARSE_FLAG_BAD_BOX_SIZE, stream);
                        break;
                    }
                    uint16_t plen = (uint16_t)(esz - 10U);
                    if (left < (int32_t)plen) {
                        mp4SetParseFlag(MP4_TAG_MTDT, MP4_PARSE_FLAG_SIZE_MISMATCH, stream);
                        esz = (uint32_t)(left + 10);
                        plen = (uint16_t)left;
                    }
                    left -= (int32_t)plen;
                    if (eenc == 257U) {
                        Mp4MtdtExt *ext = NULL;
                        int32_t res = mp4MtdtSetExtEntry(&usmt->mtdt, (uint16_t)esz, etag, elang,
                                                         257, usmt->mtdt.entry_count, 0, &ext);
                        if (res != MP4_OK) {
                            return res;
                        }
                        res = mp4ParseMtdtExt(ext, plen, stream);
                        if (res != MP4_OK) {
                            return res;
                        }
                        ext->mtsm = &usmt->mtsm;
                    } else {
                        uint8_t *raw = NULL;
                        if (plen != 0) {
                            raw = (uint8_t *)mp4MemCalloc(plen);
                            if (raw == NULL) {
                                mp4SetParseFlag(MP4_TAG_MTDT, MP4_PARSE_FLAG_ALLOC_FAIL, stream);
                                return MP4_ERR_PARSE_FATAL;
                            }
                            mp4ReadBytes(raw, plen, stream);
                        }
                        if (mp4MtdtSetEntry(&usmt->mtdt, (uint16_t)esz, etag, elang, eenc,
                                            usmt->mtdt.entry_count, raw) != MP4_OK) {
                            mp4SetParseFlag(MP4_TAG_MTDF, MP4_PARSE_FLAG_ALLOC_FAIL, stream);
                            mp4MemFree(raw);
                            return MP4_ERR_PARSE_FATAL;
                        }
                        mp4MemFree(raw);
                    }
                }
            }
        } else if (box_tag == MP4_TAG_MTSM) {
            if (box_size < 8U) {
                mp4SetParseFlag(MP4_TAG_MTSM, MP4_PARSE_FLAG_BAD_BOX_SIZE, stream);
            } else if (usmt->mtsm.size != 0) {
                mp4SetParseFlag(MP4_TAG_MTSM, MP4_PARSE_FLAG_DUPLICATE_BOX, stream);
                mp4SeekAbs(stream->file_handle, box_end);
            } else {
                usmt->mtsm.size = box_size;
                usmt->mtsm.tag = MP4_TAG_MTSM;
                if (mp4ParseMtsm(&usmt->mtsm, box_size - 8U, stream) != MP4_OK) {
                    return MP4_ERR_PARSE_FATAL;
                }
            }
        } else {
            mp4SetParseFlag(MP4_TAG_USMT, MP4_PARSE_FLAG_UNREC_BOX, stream);
            mp4SeekAbs(stream->file_handle, box_end);
        }

        if ((stream->error_flags & MP4_PARSE_FLAG_FATAL_MASK) != 0) {
            return MP4_ERR_PARSE_FATAL;
        }
        uint64_t cur_pos = (uint64_t)mp4CbTell(stream->file_handle);
        if (cur_pos < box_end) {
            mp4SetParseFlag(box_tag, MP4_PARSE_FLAG_SIZE_MISMATCH, stream);
            mp4SeekAbs(stream->file_handle, box_end);
        }
        rem -= box_size;
    }

    if (rem != 0) {
        mp4SetParseFlag(MP4_TAG_USMT, MP4_PARSE_FLAG_SIZE_MISMATCH, stream);
    }
    return MP4_OK;
}

/* 0x00f3c8 */
MP4_NOINLINE int32_t mp4ParseMtsm(Mp4MtsmBox *mtsm, uint32_t body_size, Mp4IoStream *stream)
{
    uint32_t rem = body_size;
    while (rem >= 8U) {
        uint32_t box_size = mp4ReadU32(stream);
        uint32_t box_tag = mp4ReadU32(stream);
        if (box_size < 8U) {
            mp4SetParseFlag(MP4_TAG_MTSM, MP4_PARSE_FLAG_SIZE_MISMATCH, stream);
            return MP4_OK;
        }
        if (rem < box_size) {
            mp4SetParseFlag(box_tag, MP4_PARSE_FLAG_SIZE_MISMATCH, stream);
            box_size = rem;
        }
        uint64_t box_end = (uint64_t)mp4CbTell(stream->file_handle) + (uint64_t)box_size - 8U;

        if (box_tag == MP4_TAG_MTHD) {
            if (box_size < 32U) {
                mp4SetParseFlag(MP4_TAG_MTHD, MP4_PARSE_FLAG_BAD_BOX_SIZE, stream);
            } else if (mtsm->mthd.size != 0) {
                mp4SetParseFlag(MP4_TAG_MTHD, MP4_PARSE_FLAG_DUPLICATE_BOX, stream);
                mp4SeekAbs(stream->file_handle, box_end);
            } else {
                mtsm->mthd.size = box_size;
                mtsm->mthd.tag = MP4_TAG_MTHD;
                mp4ReadBytes(&mtsm->mthd.version, 1, stream);
                mp4ReadBytes(mtsm->mthd.flags, 3, stream);
                mtsm->mthd.attr_a = mp4ReadU32(stream);
                mtsm->mthd.attr_b = mp4ReadU32(stream);
                mtsm->mthd.media_id = mp4ReadU32(stream);
                mtsm->mthd.attr_c = mp4ReadU32(stream);
                mtsm->mthd.attr_d = mp4ReadU32(stream);
            }
        } else if (box_tag == MP4_TAG_DINF) {
            if (mtsm->dinf.size != 0) {
                mp4SetParseFlag(MP4_TAG_DINF, MP4_PARSE_FLAG_DUPLICATE_BOX, stream);
                mp4SeekAbs(stream->file_handle, box_end);
            } else {
                mtsm->dinf.size = box_size;
                mtsm->dinf.tag = MP4_TAG_DINF;
                if (mp4ParseDinf(&mtsm->dinf, box_size - 8U, stream) != MP4_OK) {
                    return MP4_ERR_PARSE_FATAL;
                }
            }
        } else if (box_tag == MP4_TAG_STSD) {
            if (box_size < 16U) {
                mp4SetParseFlag(MP4_TAG_STSD, MP4_PARSE_FLAG_BAD_BOX_SIZE, stream);
            } else if (mtsm->stsd.size != 0) {
                mp4SetParseFlag(MP4_TAG_STSD, MP4_PARSE_FLAG_DUPLICATE_BOX, stream);
                mp4SeekAbs(stream->file_handle, box_end);
            } else {
                uint8_t ver = 0;
                mp4ReadBytes(&ver, 1, stream);
                if (ver != 0) {
                    mp4SetParseFlag(MP4_TAG_STSD, MP4_PARSE_FLAG_UNREC_BOX, stream);
                    mp4SeekAbs(stream->file_handle, box_end);
                } else {
                    mtsm->stsd.size = box_size;
                    mtsm->stsd.tag = MP4_TAG_STSD;
                    mp4ReadBytes(mtsm->stsd.flags, 3, stream);
                    uint32_t cnt = mp4ReadU32(stream);
                    uint32_t max_cnt = (mtsm->stsd.size - 16U) >> 4;
                    mtsm->stsd.entry_count = cnt;
                    if (max_cnt < cnt) {
                        mtsm->stsd.entry_count = max_cnt;
                        mp4SetParseFlag(MP4_TAG_STSD, MP4_PARSE_FLAG_COUNT_CLAMPED, stream);
                    }
                    if (mtsm->stsd.entry_count != 0) {
                        if (mp4ParseStsd(&mtsm->stsd, box_size - 16U, stream, 0xFFFFFFFEU) != MP4_OK) {
                            return MP4_ERR_PARSE_FATAL;
                        }
                    }
                }
            }
        } else if (box_tag == MP4_TAG_MCST) {
            if (box_size < 16U) {
                mp4SetParseFlag(MP4_TAG_MCST, MP4_PARSE_FLAG_BAD_BOX_SIZE, stream);
            } else if (mtsm->mcst.size != 0) {
                mp4SetParseFlag(MP4_TAG_MCST, MP4_PARSE_FLAG_DUPLICATE_BOX, stream);
                mp4SeekAbs(stream->file_handle, box_end);
            } else {
                mtsm->mcst.size = box_size;
                mtsm->mcst.tag = MP4_TAG_MCST;
                mp4ReadBytes(&mtsm->mcst.version, 1, stream);
                mp4ReadBytes(mtsm->mcst.flags, 3, stream);
                uint32_t cnt = mp4ReadU32(stream);
                mtsm->mcst.entry_count = cnt;
                if (box_size < cnt * 20U) {
                    mp4SetParseFlag(MP4_TAG_MCST, MP4_PARSE_FLAG_BAD_BOX_SIZE, stream);
                } else {
                    mtsm->mcst.entries = (Mp4McstEntry *)mp4MemCalloc(cnt * sizeof(Mp4McstEntry));
                    if (mtsm->mcst.entries == NULL) {
                        return MP4_ERR_NOMEM;
                    }
                    for (uint32_t idx = 0; idx < mtsm->mcst.entry_count; idx++) {
                        Mp4McstEntry *ent = &mtsm->mcst.entries[idx];
                        ent->sample_offset = mp4ReadU32(stream);
                        ent->sample_size = mp4ReadU32(stream);
                        ent->stsd_index = mp4ReadU32(stream);
                        ent->extra_a = mp4ReadU32(stream);
                        ent->extra_b = mp4ReadU32(stream);
                    }
                }
            }
        } else {
            mp4SetParseFlag(MP4_TAG_USMT, MP4_PARSE_FLAG_UNREC_BOX, stream);
            mp4SeekAbs(stream->file_handle, box_end);
        }

        if ((stream->error_flags & MP4_PARSE_FLAG_FATAL_MASK) != 0) {
            return MP4_ERR_PARSE_FATAL;
        }
        uint64_t cur_pos = (uint64_t)mp4CbTell(stream->file_handle);
        if (cur_pos < box_end) {
            mp4SetParseFlag(box_tag, MP4_PARSE_FLAG_SIZE_MISMATCH, stream);
            mp4SeekAbs(stream->file_handle, box_end);
        }
        rem -= box_size;
    }

    if (rem != 0) {
        mp4SetParseFlag(MP4_TAG_USMT, MP4_PARSE_FLAG_SIZE_MISMATCH, stream);
    }
    return MP4_OK;
}

/* 0x00f894 */
MP4_NOINLINE int32_t mp4ParseMtdtExt(Mp4MtdtExt *ext, uint32_t body_size, Mp4IoStream *stream)
{
    if (body_size < 12U) {
        return MP4_ERR_CORRUPT;
    }
    ext->head_magic = mp4ReadU32(stream);
    ext->head_version = mp4ReadU32(stream);
    ext->item_count = mp4ReadU32(stream);
    if (body_size - 12U < ext->item_count * 12U) {
        return MP4_ERR_CORRUPT;
    }
    ext->items = (Mp4MtdtExtItem *)mp4MemCalloc(ext->item_count * sizeof(Mp4MtdtExtItem));
    if (ext->items == NULL) {
        return MP4_ERR_NOMEM;
    }
    for (uint32_t idx = 0; idx < ext->item_count; idx++) {
        ext->items[idx].media_id = mp4ReadU32(stream);
        ext->items[idx].sample_index = mp4ReadU32(stream);
        ext->items[idx].item_flags = mp4ReadU32(stream);
    }
    return MP4_OK;
}

/* 0x00f984 */
MP4_NOINLINE int32_t mp4ParseFtyp(Mp4FtypBox *ftyp, uint64_t *rem_file,
                                  uint32_t *box_size_ptr, Mp4IoStream *stream)
{
    uint32_t box_size = *box_size_ptr;
    if ((box_size & 3U) != 0) {
        mp4SetParseFlag(MP4_TAG_FTYP, MP4_PARSE_FLAG_SIZE_MISMATCH, stream);
        uint32_t aligned = ((box_size - 16U) & ~3U) + 16U;
        *rem_file -= (uint64_t)(box_size - aligned);
        box_size = aligned;
        *box_size_ptr = aligned;
    }

    ftyp->size = box_size;
    ftyp->tag = MP4_TAG_FTYP;
    ftyp->major_brand = mp4ReadU32(stream);
    ftyp->minor_version = mp4ReadU32(stream);

    uint16_t compat_cnt = (uint16_t)((*box_size_ptr - 16U) >> 2);
    ftyp->compat_count = compat_cnt;
    uint16_t blocks = (uint16_t)(compat_cnt / 5U);
    if ((compat_cnt % 5U) != 0) {
        blocks++;
    }
    ftyp->compat_capacity = (uint16_t)(blocks * 5U);
    if (ftyp->compat_capacity != 0) {
        ftyp->compat_brands = (uint32_t *)mp4CbMalloc((uint32_t)ftyp->compat_capacity * 4U);
        if (ftyp->compat_brands == NULL) {
            mp4SetParseFlag(MP4_TAG_FTYP, MP4_PARSE_FLAG_ALLOC_FAIL, stream);
            return MP4_ERR_PARSE_FATAL;
        }
    }
    for (uint32_t idx = 0; idx < ftyp->compat_count; idx++) {
        ftyp->compat_brands[idx] = mp4ReadU32(stream);
    }
    return MP4_OK;
}

/* 0x00fb1c */
MP4_NOINLINE int32_t mp4ParseProf(Mp4ProfBox *prof, uint32_t entry_count, Mp4IoStream *stream)
{
    uint32_t last_tag = MP4_TAG_PROF;
    uint32_t rem = prof->size - 32U;

    for (uint32_t idx = 0; idx < entry_count; idx++) {
        if (rem < 8U) {
            mp4SetParseFlag(MP4_TAG_PROF, MP4_PARSE_FLAG_SIZE_MISMATCH, stream);
            return MP4_OK;
        }
        uint32_t box_size = mp4ReadU32(stream);
        uint32_t box_tag = mp4ReadU32(stream);
        last_tag = box_tag;
        if (rem < box_size) {
            mp4SetParseFlag(box_tag, MP4_PARSE_FLAG_SIZE_MISMATCH, stream);
            box_size = rem;
        }
        uint64_t box_end = (uint64_t)mp4CbTell(stream->file_handle) + (uint64_t)box_size - 8U;

        if (box_tag == MP4_TAG_FPRF) {
            if (box_size < 12U) {
                mp4SetParseFlag(MP4_TAG_FPRF, MP4_PARSE_FLAG_BAD_BOX_SIZE, stream);
            } else {
                uint8_t ver = 0;
                mp4ReadBytes(&ver, 1, stream);
                if (ver != 0) {
                    mp4SetParseFlag(MP4_TAG_FPRF, MP4_PARSE_FLAG_UNREC_BOX, stream);
                    mp4SeekAbs(stream->file_handle, box_end);
                } else if (box_size < 20U) {
                    mp4SetParseFlag(MP4_TAG_FPRF, MP4_PARSE_FLAG_BAD_BOX_SIZE, stream);
                } else if (prof->fprf_count != 0) {
                    mp4SetParseFlag(MP4_TAG_FPRF, MP4_PARSE_FLAG_COUNT_CLAMPED, stream);
                } else {
                    prof->fprf_list = (Mp4FprfBox *)mp4MemCalloc(sizeof(Mp4FprfBox));
                    if (prof->fprf_list == NULL) {
                        mp4SetParseFlag(MP4_TAG_FPRF, MP4_PARSE_FLAG_ALLOC_FAIL, stream);
                        return MP4_ERR_PARSE_FATAL;
                    }
                    prof->fprf_list[0].size = box_size;
                    prof->fprf_list[0].tag = MP4_TAG_FPRF;
                    prof->fprf_list[0].version = ver;
                    mp4ReadFprfBody(&prof->fprf_list[0], stream);
                    prof->fprf_count++;
                }
            }
        } else if (box_tag == MP4_TAG_APRF) {
            if (box_size < 12U) {
                mp4SetParseFlag(MP4_TAG_APRF, MP4_PARSE_FLAG_BAD_BOX_SIZE, stream);
            } else {
                uint8_t ver = 0;
                mp4ReadBytes(&ver, 1, stream);
                if (ver != 0) {
                    mp4SetParseFlag(MP4_TAG_APRF, MP4_PARSE_FLAG_UNREC_BOX, stream);
                    mp4SeekAbs(stream->file_handle, box_end);
                } else if (box_size < 44U) {
                    mp4SetParseFlag(MP4_TAG_APRF, MP4_PARSE_FLAG_BAD_BOX_SIZE, stream);
                } else {
                    Mp4AprfBox *dst = NULL;
                    if (prof->aprf_count != 0) {
                        uint32_t old_sz = prof->aprf_count * sizeof(Mp4AprfBox);
                        void *ptr = prof->aprf_list;
                        if (mp4MemRealloc(&ptr, old_sz, old_sz + sizeof(Mp4AprfBox)) != MP4_OK) {
                            mp4SetParseFlag(MP4_TAG_APRF, MP4_PARSE_FLAG_ALLOC_FAIL, stream);
                            return MP4_ERR_PARSE_FATAL;
                        }
                        prof->aprf_list = (Mp4AprfBox *)ptr;
                        dst = &prof->aprf_list[prof->aprf_count];
                    } else {
                        prof->aprf_list = (Mp4AprfBox *)mp4MemCalloc(sizeof(Mp4AprfBox));
                        if (prof->aprf_list == NULL) {
                            mp4SetParseFlag(MP4_TAG_APRF, MP4_PARSE_FLAG_ALLOC_FAIL, stream);
                            return MP4_ERR_PARSE_FATAL;
                        }
                        dst = &prof->aprf_list[0];
                    }
                    dst->size = box_size;
                    dst->tag = MP4_TAG_APRF;
                    dst->version = ver;
                    mp4ReadAprfBody(dst, stream);
                    prof->aprf_count++;
                }
            }
        } else if (box_tag == MP4_TAG_VPRF) {
            if (box_size < 12U) {
                mp4SetParseFlag(MP4_TAG_VPRF, MP4_PARSE_FLAG_BAD_BOX_SIZE, stream);
            } else {
                uint8_t ver = 0;
                mp4ReadBytes(&ver, 1, stream);
                if (ver != 0) {
                    mp4SetParseFlag(MP4_TAG_VPRF, MP4_PARSE_FLAG_UNREC_BOX, stream);
                    mp4SeekAbs(stream->file_handle, box_end);
                } else if (box_size < 52U) {
                    mp4SetParseFlag(MP4_TAG_VPRF, MP4_PARSE_FLAG_BAD_BOX_SIZE, stream);
                } else {
                    Mp4VprfBox *dst = NULL;
                    if (prof->vprf_count != 0) {
                        uint32_t old_sz = prof->vprf_count * sizeof(Mp4VprfBox);
                        void *ptr = prof->vprf_list;
                        if (mp4MemRealloc(&ptr, old_sz, old_sz + sizeof(Mp4VprfBox)) != MP4_OK) {
                            mp4SetParseFlag(MP4_TAG_VPRF, MP4_PARSE_FLAG_ALLOC_FAIL, stream);
                            return MP4_ERR_PARSE_FATAL;
                        }
                        prof->vprf_list = (Mp4VprfBox *)ptr;
                        dst = &prof->vprf_list[prof->vprf_count];
                    } else {
                        prof->vprf_list = (Mp4VprfBox *)mp4MemCalloc(sizeof(Mp4VprfBox));
                        if (prof->vprf_list == NULL) {
                            return MP4_ERR_NOMEM;
                        }
                        dst = &prof->vprf_list[0];
                    }
                    dst->size = box_size;
                    dst->tag = MP4_TAG_VPRF;
                    dst->version = ver;
                    mp4ReadVprfBody(dst, stream);
                    prof->vprf_count++;
                }
            }
        } else {
            mp4SetParseFlag(MP4_TAG_SLDM, MP4_PARSE_FLAG_UNREC_BOX, stream);
            mp4SeekAbs(stream->file_handle, box_end);
        }

        if ((stream->error_flags & MP4_PARSE_FLAG_FATAL_MASK) != 0) {
            return MP4_ERR_PARSE_FATAL;
        }
        uint64_t cur_pos = (uint64_t)mp4CbTell(stream->file_handle);
        if (cur_pos < box_end) {
            mp4SetParseFlag(box_tag, MP4_PARSE_FLAG_SIZE_MISMATCH, stream);
            mp4SeekAbs(stream->file_handle, box_end);
        }
        rem -= box_size;
    }

    if (rem != 0) {
        mp4SetParseFlag(last_tag, MP4_PARSE_FLAG_SIZE_MISMATCH, stream);
    }
    return MP4_OK;
}

/* 0x00ff38 */
MP4_NOINLINE uint32_t mp4ReadFprfBody(Mp4FprfBox *fprf, Mp4IoStream *stream)
{
    mp4ReadBytes(fprf->flags, 3, stream);
    fprf->global_attr = mp4ReadU32(stream);
    fprf->reserved_val = mp4ReadU32(stream);
    return fprf->reserved_val;
}

/* 0x00ff8c */
MP4_NOINLINE uint32_t mp4ReadAprfBody(Mp4AprfBox *aprf, Mp4IoStream *stream)
{
    mp4ReadBytes(aprf->flags, 3, stream);
    aprf->track_id = mp4ReadU32(stream);
    aprf->codec_tag = mp4ReadU32(stream);
    aprf->codec_attr = mp4ReadU32(stream);
    aprf->avg_bitrate = mp4ReadU32(stream);
    aprf->max_bitrate = mp4ReadU32(stream);
    aprf->sample_rate = mp4ReadU32(stream);
    aprf->channel_count = mp4ReadU32(stream);
    aprf->extra_attr = mp4ReadU32(stream);
    return aprf->extra_attr;
}

/* 0x010028 */
MP4_NOINLINE uint32_t mp4ReadVprfBody(Mp4VprfBox *vprf, Mp4IoStream *stream)
{
    mp4ReadBytes(vprf->flags, 3, stream);
    vprf->track_id = mp4ReadU32(stream);
    vprf->codec_tag = mp4ReadU32(stream);
    vprf->codec_attr = mp4ReadU32(stream);
    vprf->avg_bitrate = mp4ReadU32(stream);
    vprf->max_bitrate = mp4ReadU32(stream);
    vprf->avg_frame_rate = mp4ReadU32(stream);
    vprf->max_frame_rate = mp4ReadU32(stream);
    vprf->width = mp4ReadU32(stream);
    vprf->height = mp4ReadU32(stream);
    vprf->pixel_aspect = mp4ReadU32(stream);
    return vprf->pixel_aspect;
}

/* 0x0100dc */
MP4_NOINLINE int32_t mp4ReadHandlerName(char *dst, uint32_t len, Mp4IoStream *stream)
{
    if (len == 0) {
        dst[0] = '\0';
        return 1;
    }
    mp4ReadBytes(dst, len, stream);
    if ((uint8_t)dst[len - 1U] != 0U) {
        mp4SafeMemcpy(dst, len, dst + 1, len - 1U);
        dst[len - 1U] = '\0';
        return 0;
    }
    return 1;
}

/* 0x01015c */
MP4_NOINLINE int32_t mp4ParseMinf(Mp4MinfBox *minf, uint32_t body_size,
                                  Mp4IoStream *stream, uint32_t handler_type)
{
    uint32_t rem = body_size;
    while (rem >= 8U) {
        uint32_t box_size = mp4ReadU32(stream);
        if (box_size < 8U) {
            mp4SetParseFlag(MP4_TAG_MINF, MP4_PARSE_FLAG_SIZE_MISMATCH, stream);
            return MP4_OK;
        }
        uint32_t box_tag = mp4ReadU32(stream);
        if (rem < box_size) {
            mp4SetParseFlag(box_tag, MP4_PARSE_FLAG_SIZE_MISMATCH, stream);
            box_size = rem;
        }
        uint64_t box_end = (uint64_t)mp4CbTell(stream->file_handle) + (uint64_t)box_size - 8U;

        if (box_tag == MP4_TAG_VMHD) {
            if (box_size < 12U) {
                mp4SetParseFlag(MP4_TAG_VMHD, MP4_PARSE_FLAG_BAD_BOX_SIZE, stream);
            } else {
                uint8_t ver = 0;
                mp4ReadBytes(&ver, 1, stream);
                if (ver != 0) {
                    mp4SetParseFlag(MP4_TAG_VMHD, MP4_PARSE_FLAG_UNREC_BOX, stream);
                    mp4SeekAbs(stream->file_handle, box_end);
                } else if (box_size < 20U) {
                    mp4SetParseFlag(MP4_TAG_VMHD, MP4_PARSE_FLAG_BAD_BOX_SIZE, stream);
                } else if (minf->vmhd.size != 0) {
                    mp4SetParseFlag(MP4_TAG_VMHD, MP4_PARSE_FLAG_DUPLICATE_BOX, stream);
                    mp4SeekAbs(stream->file_handle, box_end);
                } else {
                    minf->vmhd.size = box_size;
                    minf->vmhd.tag = MP4_TAG_VMHD;
                    mp4ReadVmhd(&minf->vmhd, stream);
                }
            }
        } else if (box_tag == MP4_TAG_SMHD) {
            if (box_size < 12U) {
                mp4SetParseFlag(MP4_TAG_SMHD, MP4_PARSE_FLAG_BAD_BOX_SIZE, stream);
            } else {
                uint8_t ver = 0;
                mp4ReadBytes(&ver, 1, stream);
                if (ver != 0) {
                    mp4SetParseFlag(MP4_TAG_SMHD, MP4_PARSE_FLAG_UNREC_BOX, stream);
                    mp4SeekAbs(stream->file_handle, box_end);
                } else if (box_size < 16U) {
                    mp4SetParseFlag(MP4_TAG_SMHD, MP4_PARSE_FLAG_BAD_BOX_SIZE, stream);
                } else if (minf->smhd.size != 0) {
                    mp4SetParseFlag(MP4_TAG_SMHD, MP4_PARSE_FLAG_DUPLICATE_BOX, stream);
                    mp4SeekAbs(stream->file_handle, box_end);
                } else {
                    minf->smhd.size = box_size;
                    minf->smhd.tag = MP4_TAG_SMHD;
                    mp4ReadSmhd(&minf->smhd, stream);
                }
            }
        } else if (box_tag == MP4_TAG_NMHD) {
            if (box_size < 12U) {
                mp4SetParseFlag(MP4_TAG_NMHD, MP4_PARSE_FLAG_BAD_BOX_SIZE, stream);
            } else {
                uint8_t ver = 0;
                mp4ReadBytes(&ver, 1, stream);
                if (ver != 0) {
                    mp4SetParseFlag(MP4_TAG_NMHD, MP4_PARSE_FLAG_UNREC_BOX, stream);
                    mp4SeekAbs(stream->file_handle, box_end);
                } else if (minf->nmhd.size != 0) {
                    mp4SetParseFlag(MP4_TAG_NMHD, MP4_PARSE_FLAG_DUPLICATE_BOX, stream);
                    mp4SeekAbs(stream->file_handle, box_end);
                } else {
                    minf->nmhd.size = box_size;
                    minf->nmhd.tag = MP4_TAG_NMHD;
                    mp4ReadNmhd(&minf->nmhd, stream);
                }
            }
        } else if (box_tag == MP4_TAG_DINF) {
            if (minf->dinf.size != 0) {
                mp4SetParseFlag(MP4_TAG_DINF, MP4_PARSE_FLAG_DUPLICATE_BOX, stream);
                mp4SeekAbs(stream->file_handle, box_end);
            } else {
                minf->dinf.size = box_size;
                minf->dinf.tag = MP4_TAG_DINF;
                if (mp4ParseDinf(&minf->dinf, box_size - 8U, stream) == MP4_ERR_PARSE_FATAL) {
                    return MP4_ERR_PARSE_FATAL;
                }
            }
        } else if (box_tag == MP4_TAG_STBL) {
            if (mp4FsmIsPendingCache(stream->fsm) != 0 || mp4FsmIsCacheMode(stream->fsm) != 0) {
                if (mp4CacheStblCreate(&minf->stbl.cache, stream) != MP4_OK) {
                    return MP4_ERR_PARSE_FATAL;
                }
            }
            if (minf->stbl.size != 0) {
                mp4SetParseFlag(MP4_TAG_STBL, MP4_PARSE_FLAG_DUPLICATE_BOX, stream);
                mp4SeekAbs(stream->file_handle, box_end);
            } else {
                minf->stbl.size = box_size;
                minf->stbl.tag = MP4_TAG_STBL;
                if (mp4ParseStbl(&minf->stbl, box_size - 8U, stream, handler_type) == MP4_ERR_PARSE_FATAL) {
                    return MP4_ERR_PARSE_FATAL;
                }
            }
        } else {
            mp4SetParseFlag(MP4_TAG_MINF, MP4_PARSE_FLAG_UNREC_BOX, stream);
            mp4SeekAbs(stream->file_handle, box_end);
        }

        if ((stream->error_flags & MP4_PARSE_FLAG_FATAL_MASK) != 0) {
            return MP4_ERR_PARSE_FATAL;
        }
        uint64_t cur_pos = (uint64_t)mp4CbTell(stream->file_handle);
        if (cur_pos < box_end) {
            mp4SetParseFlag(box_tag, MP4_PARSE_FLAG_SIZE_MISMATCH, stream);
            mp4SeekAbs(stream->file_handle, box_end);
        }
        rem -= box_size;
    }

    if (rem != 0) {
        mp4SetParseFlag(MP4_TAG_MINF, MP4_PARSE_FLAG_SIZE_MISMATCH, stream);
    }
    return MP4_OK;
}

/* 0x01055c */
MP4_NOINLINE int32_t mp4ParseDinf(Mp4DinfBox *dinf, uint32_t body_size, Mp4IoStream *stream)
{
    uint32_t rem = body_size;
    while (rem >= 8U) {
        uint32_t box_size = mp4ReadU32(stream);
        if (box_size < 8U) {
            mp4SetParseFlag(MP4_TAG_DINF, MP4_PARSE_FLAG_SIZE_MISMATCH, stream);
            return MP4_OK;
        }
        uint32_t box_tag = mp4ReadU32(stream);
        if (rem < box_size) {
            mp4SetParseFlag(box_tag, MP4_PARSE_FLAG_SIZE_MISMATCH, stream);
            box_size = rem;
        }
        uint64_t box_end = (uint64_t)mp4CbTell(stream->file_handle) + (uint64_t)box_size - 8U;

        if (box_tag == MP4_TAG_DREF) {
            if (box_size < 12U) {
                mp4SetParseFlag(MP4_TAG_DREF, MP4_PARSE_FLAG_BAD_BOX_SIZE, stream);
            } else {
                uint8_t ver = 0;
                mp4ReadBytes(&ver, 1, stream);
                if (ver != 0) {
                    mp4SetParseFlag(MP4_TAG_DREF, MP4_PARSE_FLAG_UNREC_BOX, stream);
                    mp4SeekAbs(stream->file_handle, box_end);
                } else if (box_size < 16U) {
                    mp4SetParseFlag(MP4_TAG_DREF, MP4_PARSE_FLAG_BAD_BOX_SIZE, stream);
                } else if (dinf->dref.size != 0) {
                    mp4SetParseFlag(MP4_TAG_DREF, MP4_PARSE_FLAG_DUPLICATE_BOX, stream);
                    mp4SeekAbs(stream->file_handle, box_end);
                } else {
                    dinf->dref.size = box_size;
                    dinf->dref.tag = MP4_TAG_DREF;
                    mp4ReadBytes(dinf->dref.flags, 3, stream);
                    dinf->dref.entry_count = mp4ReadU32(stream);
                    if (dinf->dref.entry_count != 0) {
                        if (mp4ParseDref(&dinf->dref, box_size - 16U, stream) == MP4_ERR_PARSE_FATAL) {
                            return MP4_ERR_PARSE_FATAL;
                        }
                    }
                }
            }
        } else {
            mp4SetParseFlag(MP4_TAG_DINF, MP4_PARSE_FLAG_UNREC_BOX, stream);
            mp4SeekAbs(stream->file_handle, box_end);
        }

        if ((stream->error_flags & MP4_PARSE_FLAG_FATAL_MASK) != 0) {
            return MP4_ERR_PARSE_FATAL;
        }
        uint64_t cur_pos = (uint64_t)mp4CbTell(stream->file_handle);
        if (cur_pos < box_end) {
            mp4SetParseFlag(box_tag, MP4_PARSE_FLAG_SIZE_MISMATCH, stream);
            mp4SeekAbs(stream->file_handle, box_end);
        }
        rem -= box_size;
    }

    if (rem != 0) {
        mp4SetParseFlag(MP4_TAG_DINF, MP4_PARSE_FLAG_SIZE_MISMATCH, stream);
    }
    return MP4_OK;
}

/* 0x0107c8 */
MP4_NOINLINE int32_t mp4ParseDref(Mp4DrefBox *dref, uint32_t body_size, Mp4IoStream *stream)
{
    uint32_t max_entries = (dref->size - 16U) / 12U;
    if (max_entries < dref->entry_count) {
        dref->entry_count = max_entries;
        mp4SetParseFlag(MP4_TAG_DREF, MP4_PARSE_FLAG_COUNT_CLAMPED, stream);
    }

    uint32_t parsed = 0;
    Mp4DrefEntry **tail_ptr = &dref->head;
    uint32_t rem = body_size;

    while (rem >= 8U) {
        if (parsed == dref->entry_count) {
            mp4SetParseFlag(MP4_TAG_DREF, MP4_PARSE_FLAG_COUNT_CLAMPED, stream);
            break;
        }
        uint32_t box_size = mp4ReadU32(stream);
        if (box_size < 8U) {
            mp4SetParseFlag(MP4_TAG_DREF, MP4_PARSE_FLAG_SIZE_MISMATCH, stream);
            return MP4_OK;
        }
        uint32_t box_tag = mp4ReadU32(stream);
        if (rem < box_size) {
            mp4SetParseFlag(box_tag, MP4_PARSE_FLAG_SIZE_MISMATCH, stream);
            box_size = rem;
        }
        uint64_t box_end = (uint64_t)mp4CbTell(stream->file_handle) + (uint64_t)box_size - 8U;

        if (box_tag == MP4_TAG_URL) {
            if (box_size < 12U) {
                mp4SetParseFlag(MP4_TAG_URL, MP4_PARSE_FLAG_BAD_BOX_SIZE, stream);
            } else {
                uint8_t ver = 0;
                mp4ReadBytes(&ver, 1, stream);
                if (ver != 0) {
                    mp4SetParseFlag(MP4_TAG_URL, MP4_PARSE_FLAG_UNREC_BOX, stream);
                    mp4SeekAbs(stream->file_handle, box_end);
                } else {
                    Mp4DrefEntry *ent = (Mp4DrefEntry *)mp4MemCalloc(sizeof(Mp4DrefEntry));
                    if (ent == NULL) {
                        mp4SetParseFlag(MP4_TAG_URL, MP4_PARSE_FLAG_ALLOC_FAIL, stream);
                        return MP4_ERR_PARSE_FATAL;
                    }
                    ent->size = box_size;
                    ent->tag = MP4_TAG_URL;
                    ent->version = ver;
                    mp4ReadBytes(ent->flags, 3, stream);
                    uint32_t flg24 = ((uint32_t)ent->flags[0] << 16) |
                                     ((uint32_t)ent->flags[1] << 8) |
                                     (uint32_t)ent->flags[2];
                    if (flg24 == 0 && box_size != 12U) {
                        char *loc = (char *)mp4MemCalloc(box_size - 11U);
                        ent->location = loc;
                        if (loc == NULL) {
                            mp4MemFree(ent);
                            mp4SetParseFlag(MP4_TAG_URL, MP4_PARSE_FLAG_ALLOC_FAIL, stream);
                            return MP4_ERR_PARSE_FATAL;
                        }
                        mp4ReadBytes(loc, box_size - 12U, stream);
                        loc[box_size - 12U] = '\0';
                    }
                    *tail_ptr = ent;
                    tail_ptr = &ent->next;
                    parsed++;
                    ent->index = (uint16_t)parsed;
                }
            }
        } else {
            mp4SetParseFlag(MP4_TAG_DREF, MP4_PARSE_FLAG_UNREC_BOX, stream);
            mp4SeekAbs(stream->file_handle, box_end);
        }

        if ((stream->error_flags & MP4_PARSE_FLAG_FATAL_MASK) != 0) {
            return MP4_ERR_PARSE_FATAL;
        }
        uint64_t cur_pos = (uint64_t)mp4CbTell(stream->file_handle);
        if (cur_pos < box_end) {
            mp4SetParseFlag(box_tag, MP4_PARSE_FLAG_SIZE_MISMATCH, stream);
            mp4SeekAbs(stream->file_handle, box_end);
        }
        rem -= box_size;
    }

    if (rem != 0) {
        mp4SetParseFlag(MP4_TAG_DREF, MP4_PARSE_FLAG_SIZE_MISMATCH, stream);
    }
    if (parsed < dref->entry_count) {
        dref->entry_count = parsed;
        mp4SetParseFlag(MP4_TAG_DREF, MP4_PARSE_FLAG_COUNT_CLAMPED, stream);
    }
    return MP4_OK;
}

typedef struct Mp4StblCommonTable {
    uint32_t size;
    uint32_t tag;
    uint8_t version;
    uint8_t flags[3];
    uint32_t const_size;
    uint32_t entry_count;
    Mp4ChunkArray *entries;
} Mp4StblCommonTable;

/* 0x010b48 */
MP4_NOINLINE int32_t mp4ParseStbl(Mp4Stbl *stbl, uint32_t body_size,
                                  Mp4IoStream *stream, uint32_t handler_type)
{
    uint32_t rem = body_size;
    while (rem >= 8U) {
        uint32_t box_size = mp4ReadU32(stream);
        if (box_size < 8U) {
            mp4SetParseFlag(MP4_TAG_STBL, MP4_PARSE_FLAG_SIZE_MISMATCH, stream);
            return MP4_OK;
        }
        uint32_t box_tag = mp4ReadU32(stream);
        if (rem < box_size) {
            mp4SetParseFlag(box_tag, MP4_PARSE_FLAG_SIZE_MISMATCH, stream);
            box_size = rem;
        }
        uint64_t box_end = (uint64_t)mp4CbTell(stream->file_handle) + (uint64_t)box_size - 8U;

        if (box_tag == MP4_TAG_STSD) {
            if (box_size < 12U) {
                mp4SetParseFlag(MP4_TAG_STSD, MP4_PARSE_FLAG_BAD_BOX_SIZE, stream);
            } else {
                uint8_t ver = 0;
                mp4ReadBytes(&ver, 1, stream);
                if (ver != 0) {
                    mp4SetParseFlag(MP4_TAG_STSD, MP4_PARSE_FLAG_UNREC_BOX, stream);
                    mp4SeekAbs(stream->file_handle, box_end);
                } else if (box_size < 16U) {
                    mp4SetParseFlag(MP4_TAG_STSD, MP4_PARSE_FLAG_BAD_BOX_SIZE, stream);
                } else if (stbl->stsd.size != 0) {
                    mp4SetParseFlag(MP4_TAG_STSD, MP4_PARSE_FLAG_DUPLICATE_BOX, stream);
                    mp4SeekAbs(stream->file_handle, box_end);
                } else {
                    stbl->stsd.size = box_size;
                    stbl->stsd.tag = MP4_TAG_STSD;
                    mp4ReadBytes(stbl->stsd.flags, 3, stream);
                    uint32_t cnt = mp4ReadU32(stream);
                    uint32_t max_cnt = (stbl->stsd.size - 16U) >> 4;
                    stbl->stsd.entry_count = cnt;
                    if (max_cnt < cnt) {
                        stbl->stsd.entry_count = max_cnt;
                        mp4SetParseFlag(MP4_TAG_STSD, MP4_PARSE_FLAG_COUNT_CLAMPED, stream);
                    }
                    if (stbl->stsd.entry_count != 0) {
                        if (mp4ParseStsd(&stbl->stsd, box_size - 16U, stream, handler_type) == MP4_ERR_PARSE_FATAL) {
                            return MP4_ERR_PARSE_FATAL;
                        }
                        stbl->first_desc = stbl->stsd.head;
                    }
                    stbl->stsd.stbl = stbl;
                }
            }
        } else if (box_tag == MP4_TAG_STTS || box_tag == MP4_TAG_STSS ||
                   box_tag == MP4_TAG_STSC || box_tag == MP4_TAG_STSZ ||
                   box_tag == MP4_TAG_STCO || box_tag == MP4_TAG_CTTS) {
            if (mp4FsmIsPendingCache(stream->fsm) != 0 || mp4FsmIsCacheMode(stream->fsm) != 0) {
                if (mp4CacheParseBox(stbl->cache, box_size, box_tag, box_end, stream) == MP4_ERR_PARSE_FATAL) {
                    return MP4_ERR_PARSE_FATAL;
                }
            } else if (box_size < 12U) {
                mp4SetParseFlag(box_tag, MP4_PARSE_FLAG_BAD_BOX_SIZE, stream);
            } else {
                uint8_t ver = 0;
                mp4ReadBytes(&ver, 1, stream);
                if (ver != 0) {
                    mp4SetParseFlag(box_tag, MP4_PARSE_FLAG_UNREC_BOX, stream);
                    mp4SeekAbs(stream->file_handle, box_end);
                } else {
                    uint32_t hdr_size = (box_tag == MP4_TAG_STSZ) ? 20U : 16U;
                    if (box_size < hdr_size) {
                        mp4SetParseFlag(box_tag, MP4_PARSE_FLAG_BAD_BOX_SIZE, stream);
                    } else {
                        Mp4StblCommonTable *tbl = NULL;
                        uint32_t entry_bytes = 4U;
                        if (box_tag == MP4_TAG_STSZ) {
                            tbl = (Mp4StblCommonTable *)&stbl->stsz;
                        } else if (box_tag == MP4_TAG_STTS) {
                            tbl = (Mp4StblCommonTable *)&stbl->stts;
                            entry_bytes = 8U;
                        } else if (box_tag == MP4_TAG_STSC) {
                            tbl = (Mp4StblCommonTable *)&stbl->stsc;
                            entry_bytes = 12U;
                        } else if (box_tag == MP4_TAG_STCO) {
                            tbl = (Mp4StblCommonTable *)&stbl->stco;
                        } else if (box_tag == MP4_TAG_CTTS) {
                            tbl = (Mp4StblCommonTable *)&stbl->ctts;
                            entry_bytes = 8U;
                        } else {
                            tbl = (Mp4StblCommonTable *)&stbl->stss;
                        }

                        if (tbl->size != 0) {
                            mp4SetParseFlag(box_tag, MP4_PARSE_FLAG_DUPLICATE_BOX, stream);
                            mp4SeekAbs(stream->file_handle, box_end);
                        } else {
                            tbl->size = box_size;
                            tbl->tag = box_tag;
                            tbl->version = ver;
                            mp4ReadBytes(tbl->flags, 3, stream);
                            if (hdr_size == 20U) {
                                tbl->const_size = mp4ReadU32(stream);
                            }
                            uint32_t cnt = mp4ReadU32(stream);
                            tbl->entry_count = cnt;
                            if (tbl->const_size == 0) {
                                uint32_t max_cnt = (box_size - hdr_size) / entry_bytes;
                                if (max_cnt < cnt) {
                                    mp4SetParseFlag(box_tag, MP4_PARSE_FLAG_COUNT_CLAMPED, stream);
                                    tbl->entry_count = max_cnt;
                                }
                                if (tbl->const_size == 0 && tbl->entry_count != 0) {
                                    uint32_t cols = entry_bytes >> 2;
                                    int32_t rd_res;
                                    if (box_tag == MP4_TAG_STCO) {
                                        rd_res = mp4ReadChunkArrayU64(&tbl->entries, tbl->entry_count, cols, stream);
                                    } else {
                                        rd_res = mp4ReadChunkArrayU32(&tbl->entries, tbl->entry_count, cols, stream);
                                    }
                                    if (rd_res == MP4_ERR_PARSE_FATAL) {
                                        return MP4_ERR_PARSE_FATAL;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        } else {
            mp4SetParseFlag(MP4_TAG_STBL, MP4_PARSE_FLAG_UNREC_BOX, stream);
            mp4SeekAbs(stream->file_handle, box_end);
        }

        if ((stream->error_flags & MP4_PARSE_FLAG_FATAL_MASK) != 0) {
            return MP4_ERR_PARSE_FATAL;
        }
        uint64_t cur_pos = (uint64_t)mp4CbTell(stream->file_handle);
        if (cur_pos < box_end) {
            mp4SetParseFlag(box_tag, MP4_PARSE_FLAG_SIZE_MISMATCH, stream);
            mp4SeekAbs(stream->file_handle, box_end);
        }
        rem -= box_size;
    }

    if (rem != 0) {
        mp4SetParseFlag(MP4_TAG_STBL, MP4_PARSE_FLAG_SIZE_MISMATCH, stream);
    }
    if (mp4FsmIsPendingCache(stream->fsm) != 0 || mp4FsmIsCacheMode(stream->fsm) != 0) {
        if (mp4CacheRecalcBounds(stbl->cache) != MP4_OK) {
            return MP4_ERR_PARSE_FATAL;
        }
    }
    return MP4_OK;
}

/* 0x01112c */
MP4_NOINLINE int32_t mp4ReadChunkArrayU32(Mp4ChunkArray **head_ptr, uint32_t entry_count,
                                          uint32_t cols, Mp4IoStream *stream)
{
    uint32_t rem = entry_count;
    Mp4ChunkArray **slot = head_ptr;

    while (rem != 0) {
        if (*slot != NULL) {
            mp4SetParseFlag(0, MP4_PARSE_FLAG_BAD_BOX_SIZE, stream);
            return MP4_ERR_PARSE_FATAL;
        }
        Mp4ChunkArray *blk = (Mp4ChunkArray *)mp4MemCalloc(sizeof(Mp4ChunkArray));
        *slot = blk;
        if (blk == NULL) {
            mp4SetParseFlag(0, MP4_PARSE_FLAG_ALLOC_FAIL, stream);
            return MP4_ERR_PARSE_FATAL;
        }
        slot = &blk->next;
        blk->cols = cols;
        if (blk->words != NULL) {
            mp4SetParseFlag(0, MP4_PARSE_FLAG_BAD_BOX_SIZE, stream);
            return MP4_ERR_PARSE_FATAL;
        }

        uint32_t take = (rem <= 100U) ? rem : 100U;
        uint32_t *words = (uint32_t *)mp4MemMalloc(400U * cols);
        blk->words = words;
        if (words == NULL) {
            mp4SetParseFlag(0, MP4_PARSE_FLAG_ALLOC_FAIL, stream);
            return MP4_ERR_PARSE_FATAL;
        }
        blk->capacity = 100;
        mp4ReadU32Array(words, take * blk->cols * 4U, stream);
        if ((stream->error_flags & MP4_PARSE_FLAG_FATAL_MASK) != 0) {
            return MP4_ERR_PARSE_FATAL;
        }
        blk->count = take;
        rem -= take;
    }
    return MP4_OK;
}

/* 0x011258 */
MP4_NOINLINE int32_t mp4ReadChunkArrayU64(Mp4ChunkArray **head_ptr, uint32_t entry_count,
                                          uint32_t cols, Mp4IoStream *stream)
{
    uint32_t rem = entry_count;
    Mp4ChunkArray **slot = head_ptr;

    while (rem != 0) {
        if (*slot != NULL) {
            mp4SetParseFlag(0, MP4_PARSE_FLAG_BAD_BOX_SIZE, stream);
            return MP4_ERR_PARSE_FATAL;
        }
        Mp4ChunkArray *blk = (Mp4ChunkArray *)mp4MemCalloc(sizeof(Mp4ChunkArray));
        *slot = blk;
        if (blk == NULL) {
            mp4SetParseFlag(0, MP4_PARSE_FLAG_ALLOC_FAIL, stream);
            return MP4_ERR_PARSE_FATAL;
        }
        slot = &blk->next;
        blk->cols = cols;
        if (blk->wide_words != NULL) {
            mp4SetParseFlag(0, MP4_PARSE_FLAG_BAD_BOX_SIZE, stream);
            return MP4_ERR_PARSE_FATAL;
        }

        uint32_t take = (rem <= 100U) ? rem : 100U;
        uint64_t *wide = (uint64_t *)mp4MemMalloc(800U * cols);
        blk->wide_words = wide;
        if (wide == NULL) {
            mp4SetParseFlag(0, MP4_PARSE_FLAG_ALLOC_FAIL, stream);
            return MP4_ERR_PARSE_FATAL;
        }
        blk->capacity = 100;
        mp4ReadU32ToU64Array((uint8_t *)wide, take * blk->cols * 4U, stream);
        if ((stream->error_flags & MP4_PARSE_FLAG_FATAL_MASK) != 0) {
            return MP4_ERR_PARSE_FATAL;
        }
        blk->count = take;
        rem -= take;
    }
    return MP4_OK;
}

/* 0x011384 */
MP4_NOINLINE int32_t mp4ParseStsd(Mp4StsdBox *stsd, uint32_t body_size,
                                  Mp4IoStream *stream, uint32_t handler_type)
{
    uint32_t effective_handler = handler_type;
    Mp4SampleEntry **tail_ptr = &stsd->head;
    while (*tail_ptr != NULL) {
        tail_ptr = &(*tail_ptr)->next;
    }

    uint32_t expected_count = stsd->entry_count;
    stsd->entry_count = 0;
    uint32_t rem = body_size;

    while (rem >= 8U) {
        if (stsd->entry_count == expected_count) {
            mp4SetParseFlag(MP4_TAG_STSD, MP4_PARSE_FLAG_COUNT_CLAMPED, stream);
            return MP4_OK;
        }
        uint32_t box_size = mp4ReadU32(stream);
        if (box_size < 8U) {
            mp4SetParseFlag(MP4_TAG_STSD, MP4_PARSE_FLAG_SIZE_MISMATCH, stream);
            return MP4_OK;
        }
        uint32_t box_tag = mp4ReadU32(stream);
        if (rem < box_size) {
            mp4SetParseFlag(box_tag, MP4_PARSE_FLAG_SIZE_MISMATCH, stream);
            box_size = rem;
        }
        uint64_t box_end = (uint64_t)mp4CbTell(stream->file_handle) + (uint64_t)box_size - 8U;

        if (box_size < 16U) {
            mp4SetParseFlag(box_tag, MP4_PARSE_FLAG_BAD_BOX_SIZE, stream);
            return MP4_ERR_PARSE_FATAL;
        }
        Mp4SampleEntry *entry = (Mp4SampleEntry *)mp4MemCalloc(sizeof(Mp4SampleEntry));
        *tail_ptr = entry;
        if (entry == NULL) {
            mp4SetParseFlag(box_tag, MP4_PARSE_FLAG_ALLOC_FAIL, stream);
            return MP4_ERR_PARSE_FATAL;
        }
        tail_ptr = &entry->next;

        stsd->entry_count++;
        entry->size = box_size;
        entry->tag = box_tag;
        mp4ReadBytes(entry->reserved_bytes, 6, stream);
        entry->data_ref_index = mp4ReadU16(stream);
        entry->handler_type = handler_type;
        entry->entry_index = stsd->entry_count;
        entry->stsd = stsd;

        if (handler_type == 0xFFFFFFFEU) {
            effective_handler = (entry->tag == MP4_TAG_PNG) ? MP4_TAG_VIDE : 0xFFFFFFFFU;
        }

        uint32_t child_rem = box_size;
        int32_t parse_ext = 0;

        if (effective_handler == MP4_TAG_ODSM) {
            entry->codec_desc = NULL;
            child_rem -= 16U;
            rem -= 16U;
            parse_ext = (child_rem >= 8U);
        } else if (effective_handler == MP4_TAG_GRPH) {
            if (child_rem < 40U) {
                mp4SetParseFlag(box_tag, MP4_PARSE_FLAG_BAD_BOX_SIZE, stream);
            } else {
                Mp4HintDesc *hd = (Mp4HintDesc *)mp4MemCalloc(sizeof(Mp4HintDesc));
                entry->codec_desc = hd;
                if (hd == NULL) {
                    mp4SetParseFlag(box_tag, MP4_PARSE_FLAG_ALLOC_FAIL, stream);
                } else {
                    mp4ReadHintDesc(hd, stream);
                    child_rem -= 40U;
                    rem -= 40U;
                }
            }
        } else if (effective_handler == MP4_TAG_SOUN) {
            if (child_rem < 36U) {
                mp4SetParseFlag(box_tag, MP4_PARSE_FLAG_BAD_BOX_SIZE, stream);
            } else {
                Mp4AudioDesc *ad = (Mp4AudioDesc *)mp4MemCalloc(sizeof(Mp4AudioDesc));
                if (ad == NULL) {
                    mp4SetParseFlag(box_tag, MP4_PARSE_FLAG_ALLOC_FAIL, stream);
                } else {
                    entry->codec_desc = ad;
                    ad->version = mp4ReadU16(stream);
                    if (ad->version != 0) {
                        if (child_rem < 52U) {
                            mp4SetParseFlag(box_tag, MP4_PARSE_FLAG_BAD_BOX_SIZE, stream);
                        } else {
                            mp4ReadAudioDesc(ad, 1, stream);
                            child_rem -= 52U;
                            rem -= 52U;
                            parse_ext = (child_rem >= 8U);
                        }
                    } else {
                        mp4ReadAudioDesc(ad, 0, stream);
                        child_rem -= 36U;
                        rem -= 36U;
                        parse_ext = (child_rem >= 8U);
                    }
                }
            }
        } else if (effective_handler == MP4_TAG_VIDE) {
            if (child_rem < 86U) {
                mp4SetParseFlag(box_tag, MP4_PARSE_FLAG_BAD_BOX_SIZE, stream);
            } else {
                Mp4VisualDesc *vd = (Mp4VisualDesc *)mp4MemCalloc(sizeof(Mp4VisualDesc));
                if (vd == NULL) {
                    mp4SetParseFlag(box_tag, MP4_PARSE_FLAG_ALLOC_FAIL, stream);
                } else {
                    entry->codec_desc = vd;
                    mp4ReadVisualDesc(vd, stream);
                    child_rem -= 86U;
                    rem -= 86U;
                    parse_ext = (child_rem >= 8U);
                }
            }
        } else {
            mp4SetParseFlag(MP4_TAG_STSD, MP4_PARSE_FLAG_UNREC_BOX, stream);
            if (child_rem <= 16U) {
                entry->codec_desc = NULL;
                rem -= child_rem;
                child_rem = 0;
            } else {
                void *raw = mp4MemCalloc(child_rem - 16U);
                entry->codec_desc = raw;
                if (raw != NULL) {
                    mp4ReadBytes(raw, child_rem - 16U, stream);
                    rem -= child_rem;
                    child_rem = 0;
                }
            }
        }

        if (parse_ext != 0) {
            if (mp4ParseSampleExt(entry, child_rem, 0, stream) == MP4_ERR_PARSE_FATAL) {
                return MP4_ERR_PARSE_FATAL;
            }
        }

        if ((stream->error_flags & MP4_PARSE_FLAG_FATAL_MASK) != 0) {
            return MP4_ERR_PARSE_FATAL;
        }
        uint64_t cur_pos = (uint64_t)mp4CbTell(stream->file_handle);
        if (cur_pos < box_end) {
            mp4SetParseFlag(box_tag, MP4_PARSE_FLAG_SIZE_MISMATCH, stream);
            mp4SeekAbs(stream->file_handle, box_end);
        }
        rem -= child_rem;
    }

    if (rem != 0) {
        mp4SetParseFlag(MP4_TAG_STSD, MP4_PARSE_FLAG_SIZE_MISMATCH, stream);
    }
    return MP4_OK;
}

/* 0x0117f0 */
MP4_NOINLINE int32_t mp4ParseSampleExt(Mp4SampleEntry *entry, uint32_t body_size,
                                       int32_t unused_flag, Mp4IoStream *stream)
{
    (void)unused_flag;

    uint32_t rem = body_size;
    while (rem >= 8U) {
        uint32_t box_size = mp4ReadU32(stream);
        if (box_size < 8U) {
            mp4SetParseFlag(entry->tag, MP4_PARSE_FLAG_SIZE_MISMATCH, stream);
            return MP4_OK;
        }
        uint32_t box_tag = mp4ReadU32(stream);
        if (rem < box_size) {
            mp4SetParseFlag(box_tag, MP4_PARSE_FLAG_SIZE_MISMATCH, stream);
            box_size = rem;
        }
        uint64_t box_end = (uint64_t)mp4CbTell(stream->file_handle) + (uint64_t)box_size - 8U;

        if (box_tag == MP4_TAG_ESDS) {
            if (box_size < 12U) {
                mp4SetParseFlag(MP4_TAG_ESDS, MP4_PARSE_FLAG_BAD_BOX_SIZE, stream);
            } else {
                uint8_t ver = 0;
                mp4ReadBytes(&ver, 1, stream);
                if (ver != 0) {
                    mp4SetParseFlag(MP4_TAG_ESDS, MP4_PARSE_FLAG_UNREC_BOX, stream);
                    mp4SeekAbs(stream->file_handle, box_end);
                } else if (entry->handler_type != MP4_TAG_VIDE &&
                           entry->handler_type != MP4_TAG_SOUN &&
                           entry->handler_type != MP4_TAG_ODSM) {
                    if (mp4ParseExtraBox(entry, MP4_TAG_ESDS, box_size, box_end, stream) != MP4_OK) {
                        return MP4_ERR_PARSE_FATAL;
                    }
                } else {
                    void *slot = entry->esds;
                    if (mp4AllocUniqueBox(&slot, MP4_TAG_ESDS, sizeof(Mp4EsdsBox),
                                          box_end, stream) != MP4_ERR_PARSE_FATAL) {
                        entry->esds = (Mp4EsdsBox *)slot;
                        entry->esds->size = box_size;
                        entry->esds->tag = MP4_TAG_ESDS;
                        entry->esds->version = ver;
                        mp4ReadBytes(entry->esds->flags, 3, stream);
                        if (entry->esds->payload != NULL) {
                            mp4MemFree(entry->esds->payload);
                        }
                        if (box_size != 12U) {
                            entry->esds->payload = (uint8_t *)mp4MemCalloc(box_size - 12U);
                            if (entry->esds->payload == NULL) {
                                mp4SetParseFlag(MP4_TAG_ESDS, MP4_PARSE_FLAG_ALLOC_FAIL, stream);
                                return MP4_ERR_PARSE_FATAL;
                            }
                            mp4ReadBytes(entry->esds->payload, box_size - 12U, stream);
                        }
                    }
                }
            }
        } else if (box_tag == MP4_TAG_AVCC) {
            if (entry->handler_type != MP4_TAG_VIDE) {
                if (mp4ParseExtraBox(entry, MP4_TAG_AVCC, box_size, box_end, stream) != MP4_OK) {
                    return MP4_ERR_PARSE_FATAL;
                }
            } else if (box_size < 15U) {
                mp4SetParseFlag(MP4_TAG_AVCC, MP4_PARSE_FLAG_BAD_BOX_SIZE, stream);
                mp4SeekAbs(stream->file_handle, box_end);
            } else {
                uint8_t cfg_ver = 0;
                mp4ReadBytes(&cfg_ver, 1, stream);
                if (cfg_ver != 1U) {
                    mp4SetParseFlag(MP4_TAG_AVCC, MP4_PARSE_FLAG_UNREC_BOX, stream);
                    mp4SeekAbs(stream->file_handle, box_end);
                } else {
                    void *slot = entry->avcc;
                    if (mp4AllocUniqueBox(&slot, MP4_TAG_AVCC, sizeof(Mp4AvccBox),
                                          box_end, stream) != MP4_ERR_PARSE_FATAL) {
                        Mp4AvccBox *avcc = (Mp4AvccBox *)slot;
                        entry->avcc = avcc;
                        if (mp4DynArrayCreate(sizeof(Mp4AvccNal), 5, &avcc->sps_array) != MP4_OK ||
                            mp4DynArrayCreate(sizeof(Mp4AvccNal), 5, &avcc->pps_array) != MP4_OK) {
                            mp4SetParseFlag(MP4_TAG_AVCC, MP4_PARSE_FLAG_ALLOC_FAIL, stream);
                        } else {
                            avcc->size = box_size;
                            avcc->tag = MP4_TAG_AVCC;
                            avcc->config_version = cfg_ver;
                            mp4ReadBytes(&avcc->profile_idc, 1, stream);
                            mp4ReadBytes(&avcc->profile_compat, 1, stream);
                            mp4ReadBytes(&avcc->level_idc, 1, stream);
                            uint8_t packed = 0;
                            mp4ReadBytes(&packed, 1, stream);
                            avcc->reserved_len = (uint8_t)(packed >> 2);
                            avcc->length_size_minus = (uint8_t)(packed & 3U);
                            mp4ReadBytes(&packed, 1, stream);
                            avcc->reserved_sps = (uint8_t)(packed >> 5);
                            avcc->sps_count = (uint8_t)(packed & 0x1FU);

                            uint32_t left = box_size - 14U;
                            int32_t sps_ok = 1;
                            if (avcc->sps_count != 0) {
                                if (mp4ParseAvccNalList(box_end, &avcc->sps_count, &left,
                                                        avcc->sps_array, stream) == MP4_ERR_PARSE_FATAL) {
                                    sps_ok = 0;
                                }
                            }
                            if (sps_ok != 0) {
                                if (left == 0) {
                                    mp4SetParseFlag(MP4_TAG_AVCC, MP4_PARSE_FLAG_BAD_BOX_SIZE, stream);
                                } else {
                                    mp4ReadBytes(&avcc->pps_count, 1, stream);
                                    if (avcc->pps_count != 0) {
                                        mp4ParseAvccNalList(box_end, &avcc->pps_count, &left,
                                                            avcc->pps_array, stream);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        } else if (box_tag == MP4_TAG_SINF) {
            if (entry->handler_type != MP4_TAG_VIDE && entry->handler_type != MP4_TAG_SOUN) {
                if (mp4ParseExtraBox(entry, MP4_TAG_SINF, box_size, box_end, stream) != MP4_OK) {
                    return MP4_ERR_PARSE_FATAL;
                }
            } else {
                void *slot = entry->sinf;
                if (mp4AllocUniqueBox(&slot, MP4_TAG_SINF, sizeof(Mp4SinfBox),
                                      box_end, stream) != MP4_ERR_PARSE_FATAL) {
                    Mp4SinfBox *sinf = (Mp4SinfBox *)slot;
                    entry->sinf = sinf;
                    sinf->size = box_size;
                    sinf->tag = MP4_TAG_SINF;
                    sinf->payload_size = box_size - 8U;
                    sinf->payload = (uint8_t *)mp4MemCalloc(box_size - 8U);
                    if (sinf->payload == NULL) {
                        mp4SetParseFlag(sinf->tag, MP4_PARSE_FLAG_ALLOC_FAIL, stream);
                    } else {
                        mp4ReadBytes(sinf->payload, sinf->payload_size, stream);
                    }
                }
            }
        } else if (box_tag == MP4_TAG_UUID) {
            if (box_size < 24U) {
                mp4SetParseFlag(MP4_TAG_UUID, MP4_PARSE_FLAG_BAD_BOX_SIZE, stream);
                return MP4_OK;
            }
            uint32_t uuid[4];
            for (uint32_t ui = 0; ui < 4U; ui++) {
                uuid[ui] = mp4ReadU32(stream);
            }
            if (uuid[1] != MP4_SONY_UUID_WORD_A ||
                uuid[2] != MP4_SONY_UUID_WORD_B ||
                uuid[3] != MP4_SONY_UUID_WORD_C ||
                uuid[0] != MP4_TAG_STSD_UUID) {
                mp4SetParseFlag(MP4_TAG_UUID, MP4_PARSE_FLAG_UNREC_BOX, stream);
                mp4SeekAbs(stream->file_handle, box_end);
            } else if (entry->uuid_payload != NULL) {
                mp4SetParseFlag(MP4_TAG_UUID, MP4_PARSE_FLAG_DUPLICATE_BOX, stream);
                mp4SeekAbs(stream->file_handle, box_end);
            } else {
                uint32_t plen = box_size - 24U;
                uint8_t *buf = (uint8_t *)mp4MemCalloc(plen);
                if (buf == NULL) {
                    mp4SetParseFlag(MP4_TAG_STSD_UUID, MP4_PARSE_FLAG_ALLOC_FAIL, stream);
                } else if ((uint32_t)mp4ReadBytes(buf, plen, stream) == plen) {
                    entry->uuid_payload = buf;
                    entry->uuid_size = plen;
                } else {
                    mp4MemFree(buf);
                    mp4SetParseFlag(MP4_TAG_STSD_UUID, MP4_PARSE_FLAG_IO_FAIL, stream);
                }
            }
        } else {
            if (mp4ParseExtraBox(entry, box_tag, box_size, box_end, stream) != MP4_OK) {
                return MP4_ERR_PARSE_FATAL;
            }
        }

        if ((stream->error_flags & MP4_PARSE_FLAG_FATAL_MASK) != 0) {
            return MP4_ERR_PARSE_FATAL;
        }
        uint64_t cur_pos = (uint64_t)mp4CbTell(stream->file_handle);
        if (cur_pos < box_end) {
            mp4SetParseFlag(box_tag, MP4_PARSE_FLAG_SIZE_MISMATCH, stream);
            mp4SeekAbs(stream->file_handle, box_end);
        }
        rem -= box_size;
    }

    if (rem != 0) {
        mp4SetParseFlag(entry->tag, MP4_PARSE_FLAG_SIZE_MISMATCH, stream);
    }
    return MP4_OK;
}

/* 0x011ee4 */
MP4_NOINLINE int32_t mp4ParseExtraBox(Mp4SampleEntry *entry, uint32_t tag,
                                      uint32_t box_size, uint64_t box_end,
                                      Mp4IoStream *stream)
{
    uint32_t slot_idx = 0xFFFFU;
    mp4SetParseFlag(entry->tag, MP4_PARSE_FLAG_UNREC_BOX, stream);

    for (uint32_t idx = 0; idx < 3U; idx++) {
        if (entry->extra[idx] == NULL) {
            slot_idx = idx;
            break;
        }
    }
    if (slot_idx == 0xFFFFU) {
        mp4SetParseFlag(tag, MP4_PARSE_FLAG_COUNT_CLAMPED, stream);
        mp4SeekAbs(stream->file_handle, box_end);
    }

    Mp4ExtraBox *box = (Mp4ExtraBox *)mp4MemCalloc(sizeof(Mp4ExtraBox));
    entry->extra[slot_idx] = box;
    if (box == NULL) {
        mp4SetParseFlag(tag, MP4_PARSE_FLAG_ALLOC_FAIL, stream);
        return MP4_ERR_PARSE_FATAL;
    }
    box->size = box_size;
    box->tag = tag;
    if (box_size == 8U) {
        return MP4_OK;
    }
    uint32_t plen = box_size - 8U;
    box->payload = (uint8_t *)mp4MemCalloc(plen);
    if (box->payload == NULL) {
        mp4SetParseFlag(tag, MP4_PARSE_FLAG_ALLOC_FAIL, stream);
        return MP4_ERR_PARSE_FATAL;
    }
    mp4ReadBytes(box->payload, plen, stream);
    return MP4_OK;
}

/* 0x01202c */
MP4_NOINLINE int32_t mp4ParseAvccNalList(uint64_t box_end, uint8_t *count_ptr,
                                         uint32_t *out_rem, Mp4DynArray *arr,
                                         Mp4IoStream *stream)
{
    int32_t status = MP4_OK;
    uint8_t parsed = 0;
    *out_rem = 0;

    int64_t cur_pos = mp4CbTell(stream->file_handle);
    if (cur_pos == -1 || box_end < (uint64_t)cur_pos) {
        mp4SetParseFlag(MP4_TAG_AVCC, MP4_PARSE_FLAG_BAD_BOX_SIZE, stream);
        *count_ptr = 0;
        return MP4_ERR_PARSE_FATAL;
    }

    uint32_t rem = (uint32_t)(box_end - (uint64_t)cur_pos);
    for (uint8_t idx = 0; idx < *count_ptr; idx++) {
        if (rem < 2U) {
            mp4SetParseFlag(MP4_TAG_AVCC, MP4_PARSE_FLAG_BAD_BOX_SIZE, stream);
            status = MP4_ERR_PARSE_FATAL;
            break;
        }
        rem -= 2U;
        uint16_t nal_len = mp4ReadU16(stream);
        Mp4AvccNal nal;
        nal.length = nal_len;
        nal.pad = 0;
        nal.nal_bytes = NULL;

        if (nal_len != 0) {
            if (rem < (uint32_t)nal_len) {
                mp4SetParseFlag(MP4_TAG_AVCC, MP4_PARSE_FLAG_BAD_BOX_SIZE, stream);
                status = MP4_ERR_PARSE_FATAL;
                break;
            }
            nal.nal_bytes = (uint8_t *)mp4MemCalloc(nal_len);
            if (nal.nal_bytes == NULL) {
                mp4SetParseFlag(MP4_TAG_AVCC, MP4_PARSE_FLAG_ALLOC_FAIL, stream);
                status = MP4_ERR_PARSE_FATAL;
                break;
            }
            mp4ReadBytes(nal.nal_bytes, nal_len, stream);
            rem -= (uint32_t)nal_len;
        }

        if (arr == NULL || mp4DynArrayPush(arr, &nal) != MP4_OK) {
            if (nal.nal_bytes != NULL) {
                mp4MemFree(nal.nal_bytes);
            }
            mp4SetParseFlag(MP4_TAG_AVCC, MP4_PARSE_FLAG_ALLOC_FAIL, stream);
            status = MP4_ERR_PARSE_FATAL;
            break;
        }
        parsed++;
        if ((stream->error_flags & MP4_PARSE_FLAG_FATAL_MASK) != 0) {
            status = MP4_ERR_PARSE_FATAL;
            break;
        }
    }

    *out_rem = rem;
    *count_ptr = parsed;
    return status;
}

/* 0x012234 */
MP4_NOINLINE int32_t mp4ParseRootPartial(Mp4Movie *movie, uint32_t mask, Mp4IoStream *stream)
{
    uint64_t rem = (uint64_t)mp4CbGetSize(stream->file_handle);
    if (rem >= 1U && rem < 8U) {
        mp4SetParseFlag(MP4_TAG_SLDM, MP4_PARSE_FLAG_SIZE_MISMATCH, stream);
        return MP4_OK;
    }

    while (rem >= 8U) {
        uint32_t box_size = mp4ReadU32(stream);
        if (box_size == 0) {
            return MP4_OK;
        }
        if (box_size < 8U) {
            mp4SetParseFlag(MP4_TAG_SLDM, MP4_PARSE_FLAG_SIZE_MISMATCH, stream);
            return MP4_OK;
        }
        uint32_t box_tag = mp4ReadU32(stream);
        if (rem < (uint64_t)box_size) {
            mp4SetParseFlag(box_tag, MP4_PARSE_FLAG_SIZE_MISMATCH, stream);
            box_size = (uint32_t)rem;
        }
        uint64_t box_end = (uint64_t)mp4CbTell(stream->file_handle) + (uint64_t)box_size - 8U;

        if (box_tag == MP4_TAG_FTYP) {
            if ((mask & 1U) != 0) {
                if (box_size < 16U) {
                    mp4SetParseFlag(MP4_TAG_FTYP, MP4_PARSE_FLAG_BAD_BOX_SIZE, stream);
                } else if (mp4ParseFtyp(&movie->ftyp, &rem, &box_size, stream) != MP4_ERR_PARSE_FATAL) {
                    mask--;
                }
            } else {
                mp4SeekAbs(stream->file_handle, box_end);
                if (movie->ftyp.size != 0) {
                    mp4SetParseFlag(MP4_TAG_FTYP, MP4_PARSE_FLAG_DUPLICATE_BOX, stream);
                }
            }
        } else if (box_tag == MP4_TAG_MOOV) {
            if ((mask & 0x0CU) == 0) {
                mp4SeekAbs(stream->file_handle, box_end);
                if (movie->size != 0) {
                    mp4SetParseFlag(MP4_TAG_MOOV, MP4_PARSE_FLAG_DUPLICATE_BOX, stream);
                }
            } else {
                movie->self = movie;
                movie->tag = MP4_TAG_MOOV;
                movie->size = box_size;
                if (mp4ParseMoovPartial(movie, box_size - 8U, mask & 0x0CU, stream) == MP4_ERR_PARSE_FATAL) {
                    return MP4_ERR_PARSE_FATAL;
                }
                if (movie->mvhd.size == 0 && movie->usmt == NULL) {
                    mp4FreeMoovChildren(movie);
                    movie->size = 0;
                    movie->tag = 0;
                    movie->self = NULL;
                }
                mp4SeekAbs(stream->file_handle, box_end);
                mask &= ~0x0CU;
            }
        } else if (box_tag == MP4_TAG_UUID) {
            if (box_size < 24U) {
                mp4SetParseFlag(MP4_TAG_UUID, MP4_PARSE_FLAG_BAD_BOX_SIZE, stream);
                return MP4_OK;
            }
            uint32_t uuid[4];
            for (uint32_t ui = 0; ui < 4U; ui++) {
                uuid[ui] = mp4ReadU32(stream);
            }
            if (uuid[1] != MP4_SONY_UUID_WORD_A ||
                uuid[2] != MP4_SONY_UUID_WORD_B ||
                uuid[3] != MP4_SONY_UUID_WORD_C) {
                mp4SeekAbs(stream->file_handle, box_end);
            } else if (uuid[0] == MP4_TAG_LICI) {
                if ((mask & 0x10U) == 0) {
                    mp4SeekAbs(stream->file_handle, box_end);
                } else if (movie->lici.size != 0) {
                    mp4SetParseFlag(MP4_TAG_LICI, MP4_PARSE_FLAG_DUPLICATE_BOX, stream);
                    mp4SeekAbs(stream->file_handle, box_end);
                } else {
                    movie->lici.tag = uuid[0];
                    movie->lici.size = box_size;
                    if (box_size < 32U) {
                        mp4SetParseFlag(MP4_TAG_LICI, MP4_PARSE_FLAG_BAD_BOX_SIZE, stream);
                        mp4SeekAbs(stream->file_handle, box_end);
                    } else {
                        uint32_t part_a = mp4ReadU32(stream);
                        mp4ReadU32(stream);
                        if (part_a < 8U || box_size < part_a + 24U) {
                            mp4SetParseFlag(MP4_TAG_LICI, MP4_PARSE_FLAG_BAD_BOX_SIZE, stream);
                            mp4SeekAbs(stream->file_handle, box_end);
                        } else {
                            movie->lici.first_len = part_a - 8U;
                            movie->lici.first_buf = (uint8_t *)mp4MemCalloc(part_a - 8U);
                            if (movie->lici.first_buf == NULL) {
                                mp4SetParseFlag(MP4_TAG_LICI, MP4_PARSE_FLAG_ALLOC_FAIL, stream);
                                mp4SeekAbs(stream->file_handle, box_end);
                            } else {
                                mp4ReadBytes(movie->lici.first_buf, part_a - 8U, stream);
                                uint32_t part_b = mp4ReadU32(stream);
                                mp4ReadU32(stream);
                                if (part_b < 8U || box_size < part_a + part_b + 24U) {
                                    mp4SetParseFlag(MP4_TAG_LICI, MP4_PARSE_FLAG_BAD_BOX_SIZE, stream);
                                    mp4SeekAbs(stream->file_handle, box_end);
                                } else {
                                    movie->lici.second_len = part_b - 8U;
                                    movie->lici.second_buf = (uint8_t *)mp4MemCalloc(part_b - 8U);
                                    if (movie->lici.second_buf == NULL) {
                                        mp4SetParseFlag(MP4_TAG_LICI, MP4_PARSE_FLAG_ALLOC_FAIL, stream);
                                    } else {
                                        mp4ReadBytes(movie->lici.second_buf, part_b - 8U, stream);
                                    }
                                    mp4SeekAbs(stream->file_handle, box_end);
                                }
                            }
                        }
                    }
                }
            } else if (uuid[0] == MP4_TAG_MTSD) {
                if (movie->mtsd_size != 0) {
                    mp4SetParseFlag(MP4_TAG_UUID, MP4_PARSE_FLAG_DUPLICATE_BOX, stream);
                } else {
                    uint64_t cur_pos = (uint64_t)mp4CbTell(stream->file_handle);
                    movie->mtsd_size = (uint64_t)box_size;
                    movie->mtsd_offset = cur_pos - 24U;
                    mp4SeekAbs(stream->file_handle, box_end);
                }
            } else if (uuid[0] == MP4_TAG_PROF) {
                if ((mask & 2U) == 0) {
                    mp4SeekAbs(stream->file_handle, box_end);
                    if (movie->prof.size != 0) {
                        mp4SetParseFlag(MP4_TAG_UUID, MP4_PARSE_FLAG_DUPLICATE_BOX, stream);
                    }
                } else if (box_size < 12U) {
                    mp4SetParseFlag(MP4_TAG_UUID, MP4_PARSE_FLAG_BAD_BOX_SIZE, stream);
                } else {
                    uint8_t ver = 0;
                    mp4ReadBytes(&ver, 1, stream);
                    if (ver != 0) {
                        mp4SeekAbs(stream->file_handle, box_end);
                    } else if (box_size < 32U) {
                        mp4SetParseFlag(movie->prof.tag, MP4_PARSE_FLAG_BAD_BOX_SIZE, stream);
                        return MP4_OK;
                    } else {
                        movie->prof.size = box_size;
                        movie->prof.tag = MP4_TAG_UUID;
                        for (uint32_t ui = 0; ui < 4U; ui++) {
                            movie->prof.uuid[ui] = uuid[ui];
                        }
                        movie->prof.version = ver;
                        mask -= 2U;
                        mp4ReadBytes(movie->prof.flags, 3, stream);
                        uint32_t cnt = mp4ReadU32(stream);
                        int32_t res = mp4ParseProf(&movie->prof, cnt, stream);
                        stream->error_flags &= 0xE000000BU;
                        if (res != MP4_OK) {
                            return MP4_ERR_PARSE_FATAL;
                        }
                    }
                }
            } else {
                mp4SeekAbs(stream->file_handle, box_end);
            }
        } else {
            mp4SeekAbs(stream->file_handle, box_end);
        }

        if ((stream->error_flags & MP4_PARSE_FLAG_FATAL_MASK) != 0) {
            return MP4_ERR_PARSE_FATAL;
        }
        if (mask == 0) {
            return MP4_OK;
        }
        uint64_t cur_pos = (uint64_t)mp4CbTell(stream->file_handle);
        if (cur_pos < box_end) {
            mp4SetParseFlag(box_tag, MP4_PARSE_FLAG_SIZE_MISMATCH, stream);
            mp4SeekAbs(stream->file_handle, box_end);
        }
        rem -= (uint64_t)box_size;
    }

    return MP4_OK;
}

/* 0x0128e0 */
MP4_NOINLINE uint16_t mp4ReadVisualDesc(Mp4VisualDesc *desc, Mp4IoStream *stream)
{
    desc->version = mp4ReadU16(stream);
    desc->revision = mp4ReadU16(stream);
    desc->vendor = mp4ReadU32(stream);
    desc->temporal_quality = mp4ReadU32(stream);
    desc->spatial_quality = mp4ReadU32(stream);
    desc->width = mp4ReadS16(stream);
    desc->height = mp4ReadS16(stream);
    desc->horiz_res = mp4ReadS32(stream);
    desc->vert_res = mp4ReadS32(stream);
    desc->data_size = mp4ReadU32(stream);
    desc->frame_count = mp4ReadU16(stream);
    mp4ReadBytes(desc->compressor, 32, stream);
    desc->depth = mp4ReadS16(stream);
    uint16_t color_id = mp4ReadU16(stream);
    desc->color_table_id = (int16_t)color_id;
    return color_id;
}

/* 0x0129b8 */
MP4_NOINLINE uint32_t mp4ReadAudioDesc(Mp4AudioDesc *desc, uint16_t ver, Mp4IoStream *stream)
{
    desc->revision = mp4ReadU16(stream);
    desc->vendor = mp4ReadU32(stream);
    desc->channel_count = mp4ReadU16(stream);
    desc->sample_size = mp4ReadU16(stream);
    desc->compression_id = mp4ReadU16(stream);
    desc->packet_size = mp4ReadU16(stream);
    uint32_t last = mp4ReadU32(stream);
    desc->sample_rate = last;
    if (ver == 1U) {
        desc->samples_per_pkt = mp4ReadU32(stream);
        desc->bytes_per_pkt = mp4ReadU32(stream);
        desc->bytes_per_frame = mp4ReadU32(stream);
        last = mp4ReadU32(stream);
        desc->bytes_per_sample = last;
    }
    return last;
}

/* 0x012a84 */
MP4_NOINLINE uint32_t mp4ReadHintDesc(Mp4HintDesc *desc, Mp4IoStream *stream)
{
    desc->version = mp4ReadU16(stream);
    mp4ReadU16(stream);
    desc->width = mp4ReadU16(stream);
    desc->height = mp4ReadU16(stream);
    desc->horiz_res = mp4ReadU32(stream);
    desc->vert_res = mp4ReadU32(stream);
    desc->depth = mp4ReadU16(stream);
    desc->color_id = mp4ReadU16(stream);
    uint32_t tail = mp4ReadU32(stream);
    desc->bit_flags = (uint8_t)(tail >> 24);
    return tail;
}

/* 0x012b1c */
MP4_NOINLINE int16_t mp4ReadVmhd(Mp4VmhdBox *vmhd, Mp4IoStream *stream)
{
    mp4ReadBytes(vmhd->flags, 3, stream);
    vmhd->graphics_mode = mp4ReadS16(stream);
    vmhd->opcolor[0] = mp4ReadS16(stream);
    vmhd->opcolor[1] = mp4ReadS16(stream);
    vmhd->opcolor[2] = mp4ReadS16(stream);
    return vmhd->opcolor[2];
}

/* 0x012b88 */
MP4_NOINLINE uint16_t mp4ReadSmhd(Mp4SmhdBox *smhd, Mp4IoStream *stream)
{
    mp4ReadBytes(smhd->flags, 3, stream);
    smhd->balance = mp4ReadS16(stream);
    smhd->reserved = mp4ReadU16(stream);
    return smhd->reserved;
}

/* 0x012bdc */
MP4_NOINLINE int32_t mp4ReadNmhd(Mp4NmhdBox *nmhd, Mp4IoStream *stream)
{
    return mp4ReadBytes(nmhd->flags, 3, stream);
}

/* 0x012c00 */
MP4_NOINLINE uint16_t mp4ReadMdhd(Mp4MdhdBox *mdhd, Mp4IoStream *stream)
{
    mp4ReadBytes(mdhd->flags, 3, stream);
    mdhd->creation_time = mp4ReadU32(stream);
    mdhd->modification_time = mp4ReadU32(stream);
    mdhd->timescale = mp4ReadU32(stream);
    mdhd->duration = mp4ReadU32(stream);
    mdhd->language = mp4ReadU16(stream);
    mdhd->predefined = mp4ReadU16(stream);
    return mdhd->predefined;
}

/* 0x012c84 */
MP4_NOINLINE uint32_t mp4ReadTkhd(Mp4TkhdBox *tkhd, Mp4IoStream *stream)
{
    mp4ReadBytes(tkhd->flags, 3, stream);
    tkhd->creation_time = mp4ReadU32(stream);
    tkhd->modification_time = mp4ReadU32(stream);
    tkhd->track_id = mp4ReadU32(stream);
    tkhd->reserved_a = mp4ReadU32(stream);
    tkhd->duration = mp4ReadU32(stream);
    tkhd->reserved_b[0] = mp4ReadU32(stream);
    tkhd->reserved_b[1] = mp4ReadU32(stream);
    tkhd->layer = mp4ReadS16(stream);
    tkhd->alternate_group = mp4ReadS16(stream);
    tkhd->volume = mp4ReadS16(stream);
    tkhd->reserved_short = mp4ReadU16(stream);
    for (uint32_t row = 0; row < 3U; row++) {
        for (uint32_t col = 0; col < 3U; col++) {
            tkhd->matrix[row * 3U + col] = mp4ReadS32(stream);
        }
    }
    tkhd->width = mp4ReadU32(stream);
    tkhd->height = mp4ReadU32(stream);
    return tkhd->height;
}

/* 0x012dbc */
MP4_NOINLINE uint32_t mp4ReadMvhd(Mp4MvhdBox *mvhd, Mp4IoStream *stream)
{
    mp4ReadBytes(mvhd->flags, 3, stream);
    mvhd->creation_time = mp4ReadU32(stream);
    mvhd->modification_time = mp4ReadU32(stream);
    mvhd->timescale = mp4ReadU32(stream);
    mvhd->duration = mp4ReadU32(stream);
    mvhd->rate = mp4ReadS32(stream);
    mvhd->volume = mp4ReadS16(stream);
    mvhd->reserved_short = mp4ReadS16(stream);
    mvhd->reserved_pair[0] = mp4ReadU32(stream);
    mvhd->reserved_pair[1] = mp4ReadU32(stream);
    for (uint32_t row = 0; row < 3U; row++) {
        for (uint32_t col = 0; col < 3U; col++) {
            mvhd->matrix[row * 3U + col] = mp4ReadS32(stream);
        }
    }
    mvhd->predefined[0] = mp4ReadU32(stream);
    mvhd->predefined[1] = mp4ReadU32(stream);
    mvhd->predefined[2] = mp4ReadU32(stream);
    mvhd->predefined[3] = mp4ReadU32(stream);
    mvhd->predefined[4] = mp4ReadU32(stream);
    mvhd->predefined[5] = mp4ReadU32(stream);
    mvhd->next_track_id = mp4ReadU32(stream);
    return mvhd->next_track_id;
}

/* 0x012f18 */
MP4_NOINLINE int32_t mp4SetParseFlag(uint32_t tag, uint32_t flag, Mp4IoStream *stream)
{
    (void)tag;
    stream->error_flags |= flag;
    return MP4_OK;
}

/* 0x012f2c */
MP4_NOINLINE int32_t mp4AllocUniqueBox(void **slot, uint32_t tag, uint32_t alloc_size,
                                       uint64_t box_end, Mp4IoStream *stream)
{
    if (*slot != NULL) {
        mp4SetParseFlag(tag, MP4_PARSE_FLAG_DUPLICATE_BOX, stream);
        mp4SeekAbs(stream->file_handle, box_end);
        return MP4_ERR_PARSE_FATAL;
    }
    void *mem = mp4MemCalloc(alloc_size);
    *slot = mem;
    if (mem == NULL) {
        mp4SetParseFlag(tag, MP4_PARSE_FLAG_ALLOC_FAIL, stream);
        return MP4_ERR_PARSE_FATAL;
    }
    return MP4_OK;
}

/* 0x012fe8 */
MP4_NOINLINE int32_t mp4RecordCustomUuid(Mp4Movie *movie, uint64_t offset,
                                         uint32_t box_size, const uint32_t *uuid)
{
    if (movie == NULL || uuid == NULL) {
        return MP4_ERR_PARSE_FATAL;
    }
    Mp4UuidNode **tail_ptr = &movie->uuid_head;
    Mp4UuidNode *cur = movie->uuid_head;
    while (cur != NULL) {
        if (cur->file_offset == offset) {
            return MP4_OK;
        }
        tail_ptr = &cur->next;
        cur = cur->next;
    }

    Mp4UuidNode *node = (Mp4UuidNode *)mp4MemCalloc(sizeof(Mp4UuidNode));
    if (node == NULL) {
        return MP4_ERR_PARSE_FATAL;
    }
    node->file_offset = offset;
    node->box_size = box_size;
    for (uint32_t ui = 0; ui < 4U; ui++) {
        node->uuid[ui] = uuid[ui];
    }
    *tail_ptr = node;
    return MP4_OK;
}

/* 0x0130d8 */
MP4_NOINLINE int32_t mp4CacheStblCreate(Mp4CacheStbl **out_cache, Mp4IoStream *stream)
{
    if (*out_cache != NULL) {
        mp4MemFree(*out_cache);
    }
    Mp4CacheStbl *cache = (Mp4CacheStbl *)mp4MemCalloc(sizeof(Mp4CacheStbl));
    *out_cache = cache;
    if (cache == NULL) {
        mp4SetParseFlag(0, MP4_PARSE_FLAG_ALLOC_FAIL, stream);
        return MP4_ERR_NOMEM;
    }
    cache->initialized = 1;
    cache->ctrl = (Mp4CacheCtrl *)mp4MemCalloc(sizeof(Mp4CacheCtrl));
    cache->ctrl->stream = stream;
    cache->ctrl->total_chunks = 0;
    cache->ctrl->total_samples = 0;
    cache->ctrl->total_duration = 0;
    return MP4_OK;
}
